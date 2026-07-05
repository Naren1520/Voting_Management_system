#include "../../include/controllers/PublicMultiVoteController.h"
#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Logger.h"
#include <algorithm>
#include <set>

// ─────────────────────────────────────────────────────────────────────────────
// getBallot
// ─────────────────────────────────────────────────────────────────────────────

json PublicMultiVoteController::getBallot(const std::string& electionId) {
    auto elec = supabaseRequest("GET",
        "elections?select=id,title,is_active,election_type&id=eq."+
        electionId+"&limit=1");
    try {
        auto arr = json::parse(elec.body);
        if (!arr.is_array() || arr.empty()) {
            json r; r["success"]=false; r["message"]="Election not found"; return r;
        }
        if (!arr[0]["is_active"].get<bool>()) {
            json r; r["success"]=false; r["message"]="This election is closed"; return r;
        }
    } catch (...) {
        json r; r["success"]=false; r["message"]="Election not found"; return r;
    }

    // Fetch all positions with their nested candidates in ONE round-trip.
    // Supabase PostgREST supports resource embedding via foreign key relations.
    // Requires "candidates" table to have a foreign key to "positions".
    auto posRes = supabaseRequest("GET",
        "positions?select=id,title,order_index,candidates(name)"
        "&election_id=eq."+electionId+"&order=order_index.asc");

    json positions = json::array();
    try {
        positions = json::parse(posRes.body);
    } catch (...) {
        // Fallback: nested select unavailable, fetch per-position
        auto fallback = supabaseRequest("GET",
            "positions?select=id,title,order_index&election_id=eq."+
            electionId+"&order=order_index.asc");
        try { positions = json::parse(fallback.body); } catch (...) {}
        for (auto& pos : positions) {
            std::string posId = pos["id"].get<std::string>();
            auto candRes = supabaseRequest("GET",
                "candidates?select=name&election_id=eq."+electionId+
                "&position_id=eq."+posId+"&order=name.asc");
            try { pos["candidates"] = json::parse(candRes.body); }
            catch (...) { pos["candidates"] = json::array(); }
        }
    }

    json res; res["success"]=true; res["positions"]=positions;
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// checkVoter
// ─────────────────────────────────────────────────────────────────────────────

json PublicMultiVoteController::checkVoter(const std::string& electionId,
                                           const std::string& voterId) {
    json res;
    if (voterId.empty()) {
        res["success"]=false; res["message"]="Voter ID required"; return res;
    }
    auto reg = supabaseRequest("GET",
        "registered_voters?select=voter_id,name&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId)+"&limit=1");
    try {
        auto arr = json::parse(reg.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"]=false; res["registered"]=false;
            res["message"]="Voter ID not registered for this election";
            return res;
        }
    } catch (...) {
        res["success"]=false; res["message"]="Database error"; return res;
    }

    auto posRes = supabaseRequest("GET",
        "positions?select=id&election_id=eq."+electionId);
    int totalPositions = 0;
    try {
        auto arr = json::parse(posRes.body);
        if (arr.is_array()) totalPositions = static_cast<int>(arr.size());
    } catch (...) {}

    auto votedRes = supabaseRequest("GET",
        "votes_cast?select=position_id&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId));
    int  votedCount    = 0;
    json votedPositions = json::array();
    try {
        auto arr = json::parse(votedRes.body);
        if (arr.is_array()) {
            votedCount = static_cast<int>(arr.size());
            for (auto& v : arr) votedPositions.push_back(v["position_id"]);
        }
    } catch (...) {}

    res["success"]         = true;
    res["registered"]      = true;
    res["total_positions"] = totalPositions;
    res["voted_count"]     = votedCount;
    res["voted_positions"] = votedPositions;
    res["fully_voted"]     = (votedCount >= totalPositions && totalPositions > 0);
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// castVotes — atomic via Supabase RPC to eliminate the TOCTOU race.
//
// Fix #14 (multi-vote): Same race as single-vote — two concurrent requests
//   for the same voter_id can both pass checkVoter() before either writes to
//   votes_cast, resulting in double votes across one or more positions.
//
//   The fix calls cast_vote_multi, a Postgres function that processes all
//   positions in a single transaction using INSERT ON CONFLICT DO NOTHING per
//   position.  Duplicate position votes are silently ignored at the DB level.
//
//   SQL to create in Supabase SQL editor:
//
//   CREATE OR REPLACE FUNCTION cast_vote_multi(
//       p_election_id uuid,
//       p_voter_id    text,
//       p_votes       jsonb   -- array of {position_id, candidate_name}
//   ) RETURNS json
//   LANGUAGE plpgsql
//   SECURITY DEFINER
//   AS $$
//   DECLARE
//       v        jsonb;
//       pos_id   uuid;
//       cand     text;
//       inserted int;
//       total    int := 0;
//   BEGIN
//       FOR v IN SELECT * FROM jsonb_array_elements(p_votes)
//       LOOP
//           pos_id := (v->>'position_id')::uuid;
//           cand   :=  v->>'candidate_name';
//
//           IF pos_id IS NULL OR cand IS NULL THEN
//               CONTINUE;
//           END IF;
//
//           -- One vote per voter per position (unique constraint)
//           INSERT INTO votes_cast
//               (election_id, voter_id, position_id, candidate_name)
//           VALUES
//               (p_election_id, p_voter_id, pos_id, cand)
//           ON CONFLICT (election_id, voter_id, position_id) DO NOTHING;
//
//           GET DIAGNOSTICS inserted = ROW_COUNT;
//           IF inserted > 0 THEN
//               UPDATE candidates
//               SET votes = votes + 1
//               WHERE election_id = p_election_id
//                 AND position_id = pos_id
//                 AND name        = cand;
//               total := total + 1;
//           END IF;
//       END LOOP;
//
//       RETURN json_build_object('success', true, 'votes_recorded', total);
//   END;
//   $$;
//
//   Unique constraint for multi-vote (one vote per voter per position):
//   ALTER TABLE votes_cast
//       ADD CONSTRAINT votes_cast_unique_voter_position
//       UNIQUE (election_id, voter_id, position_id);
// ─────────────────────────────────────────────────────────────────────────────

json PublicMultiVoteController::castVotes(const std::string& electionId,
                                          const std::string& voterId,
                                          const json& votes) {
    json res;

    if (voterId.empty()) {
        res["success"]=false; res["message"]="Voter ID required"; return res;
    }
    if (!votes.is_array() || votes.empty()) {
        res["success"]=false; res["message"]="No votes provided"; return res;
    }

    // Verify election is active
    auto elec = supabaseRequest("GET",
        "elections?select=is_active&id=eq."+electionId+"&limit=1");
    try {
        auto arr = json::parse(elec.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"]=false; res["message"]="Election not found"; return res;
        }
        if (!arr[0]["is_active"].get<bool>()) {
            res["success"]=false; res["message"]="This election is closed"; return res;
        }
    } catch (...) {
        res["success"]=false; res["message"]="Election not found"; return res;
    }

    // Verify voter is registered
    auto reg = supabaseRequest("GET",
        "registered_voters?select=voter_id&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId)+"&limit=1");
    try {
        auto arr = json::parse(reg.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"]=false; res["registered"]=false;
            res["message"]="Voter ID not registered for this election";
            return res;
        }
    } catch (...) {
        res["success"]=false; res["message"]="Database error"; return res;
    }

    // ── Atomic multi-position vote via RPC ────────────────────────────────
    json rpcBody;
    rpcBody["p_election_id"] = electionId;
    rpcBody["p_voter_id"]    = voterId;
    rpcBody["p_votes"]       = votes;

    auto rpcResult = SupabaseClient::instance().request(
        "POST", "rpc/cast_vote_multi", rpcBody.dump());

    try {
        auto rpcJson = json::parse(rpcResult.body);
        bool success = rpcJson.value("success", false);
        if (!success) {
            std::string message = rpcJson.value("message", "Vote failed");
            res["success"]=false; res["message"]=message; return res;
        }
    } catch (...) {
        // RPC not yet created — fall back to legacy per-position writes
        // with a duplicate check right before each write to narrow the window.
        LOG_WARN("[castVotes] RPC cast_vote_multi not available — "
                 "falling back to legacy per-position writes. "
                 "Create the SQL function to fix the TOCTOU race!");

        auto chk = checkVoter(electionId, voterId);
        if (!chk["success"].get<bool>()) return chk;
        if (chk["fully_voted"].get<bool>()) {
            res["success"]=false;
            res["message"]="You have already voted in all positions";
            return res;
        }

        json votedPositions = chk["voted_positions"];
        std::set<std::string> alreadyVoted;
        for (auto& p : votedPositions) alreadyVoted.insert(p.get<std::string>());

        for (const auto& v : votes) {
            std::string posId    = v.value("position_id","");
            std::string candName = v.value("candidate_name","");
            if (posId.empty() || candName.empty()) continue;
            if (alreadyVoted.count(posId)) continue;

            auto cand = supabaseRequest("GET",
                "candidates?select=name,votes&election_id=eq."+electionId+
                "&position_id=eq."+posId+
                "&name=eq."+SupabaseClient::urlEncode(candName)+"&limit=1");
            try {
                auto arr = json::parse(cand.body);
                if (!arr.is_array() || arr.empty()) continue;
                int newVotes = arr[0]["votes"].get<int>() + 1;
                json upd; upd["votes"] = newVotes;
                supabaseRequest("PATCH",
                    "candidates?election_id=eq."+electionId+
                    "&position_id=eq."+posId+
                    "&name=eq."+SupabaseClient::urlEncode(candName), upd.dump());
            } catch (...) { continue; }

            json voteBody;
            voteBody["election_id"]    = electionId;
            voteBody["voter_id"]       = voterId;
            voteBody["position_id"]    = posId;
            voteBody["candidate_name"] = candName;
            supabaseRequest("POST","votes_cast",voteBody.dump());
        }
    }

    res["success"] = true;
    res["message"] = "Votes cast successfully";
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// getResults
// ─────────────────────────────────────────────────────────────────────────────

json PublicMultiVoteController::getResults(const std::string& electionId) {
    // One round-trip: positions with nested candidates via resource embedding.
    auto posRes = supabaseRequest("GET",
        "positions?select=id,title,order_index,candidates(name,votes)"
        "&election_id=eq."+electionId+"&order=order_index.asc");

    json positions = json::array();
    try {
        positions = json::parse(posRes.body);
    } catch (...) {
        // Fallback: N+1 per-position fetch if nested select fails
        auto fallback = supabaseRequest("GET",
            "positions?select=id,title,order_index&election_id=eq."+
            electionId+"&order=order_index.asc");
        try { positions = json::parse(fallback.body); } catch (...) {}
        for (auto& pos : positions) {
            std::string posId = pos["id"].get<std::string>();
            auto candRes = supabaseRequest("GET",
                "candidates?select=name,votes&election_id=eq."+electionId+
                "&position_id=eq."+posId+"&order=votes.desc");
            try { pos["candidates"] = json::parse(candRes.body); }
            catch (...) { pos["candidates"] = json::array(); }
        }
    }

    // Sum total_votes per position from the already-fetched candidates
    for (auto& pos : positions) {
        int total = 0;
        if (pos.contains("candidates") && pos["candidates"].is_array()) {
            // Sort candidates by votes desc
            auto& cands = pos["candidates"];
            std::sort(cands.begin(), cands.end(), [](const json& a, const json& b){
                return a.value("votes", 0) > b.value("votes", 0);
            });
            for (auto& c : cands) total += c.value("votes", 0);
        }
        pos["total_votes"] = total;
    }

    json res; res["success"]=true; res["positions"]=positions;
    return res;
}

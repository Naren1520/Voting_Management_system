#include "../../include/controllers/PublicVoteController.h"
#include "../../include/cache/CandidateCache.h"
#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Logger.h"

// getCandidates - with candidate cache

json PublicVoteController::getCandidates(const std::string& electionId) {
    auto elec = supabaseRequest("GET",
        "elections?select=id,title,is_active&id=eq."+electionId+"&limit=1");
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

    // Cache hit
    auto& cache = CandidateCache::instance();
    std::string cached = cache.get(electionId);
    if (!cached.empty()) {
        try {
            json r; r["success"]=true; r["candidates"]=json::parse(cached); return r;
        } catch (...) { /* corrupt cache entry - fall through */ }
    }

    auto res = supabaseRequest("GET",
        "candidates?select=name&election_id=eq."+electionId+"&order=name.asc");
    try {
        auto candidatesJson = json::parse(res.body);
        cache.set(electionId, candidatesJson.dump());
        json r; r["success"]=true; r["candidates"]=candidatesJson; return r;
    } catch (...) {
        json r; r["success"]=false; r["message"]="Failed to load candidates"; return r;
    }
}

// checkVoter - read-only eligibility check (does NOT cast a vote)

json PublicVoteController::checkVoter(const std::string& electionId,
                                      const std::string& voterId) {
    json res;
    if (voterId.empty()) {
        res["success"]=false; res["message"]="Voter ID required"; return res;
    }
    auto reg = supabaseRequest("GET",
        "voters?select=voter_id,name&election_id=eq."+electionId+
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

    auto voted = supabaseRequest("GET",
        "votes_cast?select=voter_id&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId)+"&limit=1");
    try {
        auto arr = json::parse(voted.body);
        if (arr.is_array() && !arr.empty()) {
            res["success"]=false; res["already_voted"]=true;
            res["message"]="You have already voted in this election";
            return res;
        }
    } catch (...) {}

    res["success"]=true; res["registered"]=true; res["already_voted"]=false;
    res["message"]="Voter verified";
    return res;
}

// castVote - atomic via Supabase RPC to eliminate the TOCTOU race.
//
// Fix #14: The old flow was three separate HTTP calls:
//   1. GET votes_cast  (check duplicate)
//   2. PATCH candidates (increment count)
//   3. POST votes_cast (record vote)
//
//   Two concurrent requests with the same voter_id could both pass step 1
//   before either completed step 3 → double vote.
//
//   The fix calls a single Postgres function (cast_vote_single) that performs
//   all three steps inside one transaction with a row-level lock on votes_cast
//   using INSERT ... ON CONFLICT DO NOTHING.  Either the vote is recorded or
//   it is not - there is no in-between.
//
//   SQL to create this function in Supabase (run once in the SQL editor):
//
//   CREATE OR REPLACE FUNCTION cast_vote_single(
//       p_election_id  uuid,
//       p_voter_id     text,
//       p_candidate    text
//   ) RETURNS json
//   LANGUAGE plpgsql
//   SECURITY DEFINER
//   AS $$
//   DECLARE
//       inserted int;
//   BEGIN
//       -- Attempt to record the vote; unique constraint prevents duplicates.
//       INSERT INTO votes_cast (election_id, voter_id, candidate_name)
//       VALUES (p_election_id, p_voter_id, p_candidate)
//       ON CONFLICT (election_id, voter_id) DO NOTHING;
//
//       GET DIAGNOSTICS inserted = ROW_COUNT;
//
//       IF inserted = 0 THEN
//           RETURN json_build_object(
//               'success', false,
//               'message', 'You have already voted in this election'
//           );
//       END IF;
//
//       -- Increment candidate vote count atomically
//       UPDATE candidates
//       SET votes = votes + 1
//       WHERE election_id = p_election_id
//         AND name        = p_candidate;
//
//       RETURN json_build_object('success', true,
//                                'message', 'Vote cast successfully');
//   END;
//   $$;
//
//   Also add the unique constraint if not already present:
//   ALTER TABLE votes_cast
//       ADD CONSTRAINT votes_cast_unique_voter
//       UNIQUE (election_id, voter_id);

json PublicVoteController::castVote(const std::string& electionId,
                                    const std::string& voterId,
                                    const std::string& candidateName) {
    json res;

    // Basic input validation
    if (voterId.empty() || candidateName.empty()) {
        res["success"]=false; res["message"]="voter_id and candidate_name required";
        return res;
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
        "voters?select=voter_id&election_id=eq."+electionId+
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

    // Verify candidate exists
    auto cand = supabaseRequest("GET",
        "candidates?select=name&election_id=eq."+electionId+
        "&name=eq."+SupabaseClient::urlEncode(candidateName)+"&limit=1");
    try {
        auto arr = json::parse(cand.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"]=false; res["message"]="Candidate not found"; return res;
        }
    } catch (...) {
        res["success"]=false; res["message"]="Database error"; return res;
    }

    // ── Atomic single-transaction vote via RPC ────────────────────────────
    // Calls the cast_vote_single Postgres function (see SQL comment above).
    // INSERT ON CONFLICT + UPDATE in one transaction = no TOCTOU race.
    json rpcBody;
    rpcBody["p_election_id"] = electionId;
    rpcBody["p_voter_id"]    = voterId;
    rpcBody["p_candidate"]   = candidateName;

    auto rpcResult = SupabaseClient::instance().request(
        "POST", "rpc/cast_vote_single", rpcBody.dump());

    // Parse RPC response
    try {
        // Supabase RPC returns the function's return value directly
        auto rpcJson = json::parse(rpcResult.body);

        // RPC returns a JSON object from json_build_object()
        bool success = rpcJson.value("success", false);
        std::string message = rpcJson.value("message", "Unknown error");

        if (!success) {
            res["success"]=false; res["message"]=message; return res;
        }
    } catch (...) {
        // If RPC doesn't exist yet, fall back to the legacy two-step flow
        // but log a warning so the developer knows to create the function.
        LOG_WARN("[castVote] RPC cast_vote_single not available - "
                 "falling back to two-step write. "
                 "Create the SQL function to fix the TOCTOU race!");

        // Legacy fallback - direct insert into votes_cast
        // Check again immediately before writing
        auto voted = supabaseRequest("GET",
            "votes_cast?select=voter_id&election_id=eq."+electionId+
            "&voter_id=eq."+SupabaseClient::urlEncode(voterId)+"&limit=1");
        try {
            auto arr = json::parse(voted.body);
            if (arr.is_array() && !arr.empty()) {
                res["success"]=false;
                res["message"]="You have already voted in this election";
                return res;
            }
        } catch (...) {}

        json voteBody;
        voteBody["election_id"]    = electionId;
        voteBody["voter_id"]       = voterId;
        voteBody["candidate_name"] = candidateName;
        supabaseRequest("POST", "votes_cast", voteBody.dump());
    }

    // Invalidate candidate cache - vote counts changed
    CandidateCache::instance().invalidate(electionId);

    res["success"] = true;
    res["message"] = "Vote cast successfully for " + candidateName;
    // NOTE: do not re-fetch candidates here - the cache was just invalidated.
    // The frontend should call GET /api/vote/:id/results for updated counts.
    return res;
}

json PublicVoteController::getResults(const std::string& electionId) {
    // Get candidate names
    auto cands = supabaseRequest("GET",
        "candidates?select=name&election_id=eq."+electionId+"&order=name.asc");

    // Get all votes for this election
    auto votes = supabaseRequest("GET",
        "votes_cast?select=candidate_name&election_id=eq."+electionId);

    try {
        auto candArr  = json::parse(cands.body);
        auto votesArr = json::parse(votes.body);

        // Count votes per candidate
        std::map<std::string, int> counts;
        if (candArr.is_array()) {
            for (auto& c : candArr) counts[c["name"].get<std::string>()] = 0;
        }
        int totalVotes = 0;
        if (votesArr.is_array()) {
            for (auto& v : votesArr) {
                std::string cn = v.value("candidate_name","");
                if (!cn.empty()) { counts[cn]++; totalVotes++; }
            }
        }

        // Build result array sorted by votes descending
        json resultArr = json::array();
        for (auto& [name, cnt] : counts) {
            json c; c["name"]=name; c["votes"]=cnt;
            resultArr.push_back(c);
        }
        std::sort(resultArr.begin(), resultArr.end(),
            [](const json& a, const json& b){
                return a["votes"].get<int>() > b["votes"].get<int>();
            });

        json res;
        res["success"]     = true;
        res["candidates"]  = resultArr;
        res["total_votes"] = totalVotes;
        return res;
    } catch (...) {
        json r; r["success"]=false; r["message"]="Failed to load results"; return r;
    }
}

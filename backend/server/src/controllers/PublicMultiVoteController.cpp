#include "../../include/controllers/PublicMultiVoteController.h"
#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Logger.h"
#include <algorithm>
#include <set>
#include <map>

// getBallot

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

    // Fetch positions
    auto posRes = supabaseRequest("GET",
        "positions?select=id,title&election_id=eq."+electionId+"&order=created_at.asc");

    json positions = json::array();
    try { positions = json::parse(posRes.body); } catch (...) {}

    // Fetch candidates for each position
    for (auto& pos : positions) {
        std::string posId = pos["id"].get<std::string>();
        auto candRes = supabaseRequest("GET",
            "position_candidates?select=name&position_id=eq."+posId+"&order=name.asc");
        try { pos["candidates"] = json::parse(candRes.body); }
        catch (...) { pos["candidates"] = json::array(); }
    }

    json res; res["success"]=true; res["positions"]=positions;
    return res;
}

// checkVoter

json PublicMultiVoteController::checkVoter(const std::string& electionId,
                                           const std::string& voterId) {
    json res;
    if (voterId.empty()) {
        res["success"]=false; res["message"]="Voter ID required"; return res;
    }
    // Check voter is registered
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

    // Count positions
    auto posRes = supabaseRequest("GET",
        "positions?select=id&election_id=eq."+electionId);
    int totalPositions = 0;
    try {
        auto arr = json::parse(posRes.body);
        if (arr.is_array()) totalPositions = static_cast<int>(arr.size());
    } catch (...) {}

    // Count voted positions - use vote_ledger (no candidate info leaked)
    auto votedRes = supabaseRequest("GET",
        "multi_vote_ledger?select=position_id&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId));
    int  votedCount     = 0;
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

// castVotes - secret ballot via Supabase RPC.
//
// The old flow inserted (voter_id, candidate_name) into multi_votes_cast —
// a single table where every admin could see exactly who chose whom.
//
// The new flow calls cast_multi_vote_secret(), a SECURITY DEFINER Postgres
// function that for each position inserts into TWO tables in one transaction:
//
//   multi_vote_ledger  (election_id, voter_id, position_id, ballot_id)
//   multi_vote_ballots (ballot_id, election_id, position_id, candidate_name)
//
// ballot_id is a random UUID generated INSIDE the function per position vote.
// The two tables share ballot_id but never have voter_id and candidate_name
// in the same row.

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

    // ── Atomic secret-ballot multi-vote via RPC ───────────────────────────
    json rpcBody;
    rpcBody["p_election_id"] = electionId;
    rpcBody["p_voter_id"]    = voterId;
    rpcBody["p_votes"]       = votes;   // JSON array forwarded verbatim

    auto rpcResult = SupabaseClient::instance().request(
        "POST", "rpc/cast_multi_vote_secret", rpcBody.dump());

    try {
        auto rpcJson = json::parse(rpcResult.body);
        bool success = rpcJson.value("success", false);
        std::string message = rpcJson.value("message", "Unknown error");
        if (!success) {
            res["success"]=false; res["message"]=message; return res;
        }
    } catch (...) {
        LOG_ERROR("[castVotes] RPC cast_multi_vote_secret failed or not found. "
                  "Run the SQL migration in supabase_schema.sql.");
        res["success"]=false;
        res["message"]="Vote service unavailable - please contact the administrator";
        return res;
    }

    res["success"] = true;
    res["message"] = "Votes cast successfully";
    return res;
}

// getResults

json PublicMultiVoteController::getResults(const std::string& electionId) {
    // Fetch positions
    auto posRes = supabaseRequest("GET",
        "positions?select=id,title&election_id=eq."+electionId+"&order=created_at.asc");

    json positions = json::array();
    try { positions = json::parse(posRes.body); } catch (...) {}

    // Fetch all anonymous ballots for this election (no voter_id column here)
    auto votesRes = supabaseRequest("GET",
        "multi_vote_ballots?select=position_id,candidate_name&election_id=eq."+electionId);

    // Count votes per position+candidate
    std::map<std::string, std::map<std::string,int>> counts;
    try {
        auto votesArr = json::parse(votesRes.body);
        if (votesArr.is_array()) {
            for (auto& v : votesArr) {
                std::string posId = v.value("position_id","");
                std::string cand  = v.value("candidate_name","");
                if (!posId.empty() && !cand.empty()) counts[posId][cand]++;
            }
        }
    } catch (...) {}

    // Build result for each position
    for (auto& pos : positions) {
        std::string posId = pos["id"].get<std::string>();
        auto& posVotes = counts[posId];

        // Get candidates for this position
        auto candRes = supabaseRequest("GET",
            "position_candidates?select=name&position_id=eq."+posId+"&order=name.asc");

        json candArr = json::array();
        try { candArr = json::parse(candRes.body); } catch (...) {}

        int totalVotes = 0;
        json resultCands = json::array();
        for (auto& c : candArr) {
            std::string name = c.value("name","");
            int voteCount = posVotes.count(name) ? posVotes[name] : 0;
            json cr; cr["name"]=name; cr["votes"]=voteCount;
            resultCands.push_back(cr);
            totalVotes += voteCount;
        }
        // Sort by votes desc
        std::sort(resultCands.begin(), resultCands.end(),
            [](const json& a, const json& b){
                return a["votes"].get<int>() > b["votes"].get<int>();
            });

        pos["candidates"]  = resultCands;
        pos["total_votes"] = totalVotes;
    }

    json res; res["success"]=true; res["positions"]=positions;
    return res;
}

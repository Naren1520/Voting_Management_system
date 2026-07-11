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
        "vote_ledger?select=voter_id&election_id=eq."+electionId+
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

// castVote - secret ballot via Supabase RPC.
//
// The old flow stored (voter_id, candidate_name) in the same row of
// votes_cast, making it trivial for a DB admin to see how everyone voted.
//
// The new flow calls cast_vote_secret(), a SECURITY DEFINER Postgres
// function that inserts into TWO separate tables inside one transaction:
//
//   vote_ledger  (election_id, voter_id, ballot_id)  ← who voted
//   vote_ballots (ballot_id, election_id, candidate_name) ← what was chosen
//
// The ballot_id is a random UUID generated INSIDE the DB function.
// Neither table on its own reveals the voter→choice link.  Joining them on
// ballot_id requires access to both tables simultaneously and leaves an
// audit trail — meaningfully higher bar than a plain SELECT on votes_cast.

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

    // ── Atomic secret-ballot vote via RPC ─────────────────────────────────
    // cast_vote_secret inserts vote_ledger + vote_ballots in one transaction.
    // voter_id and candidate_name are never in the same DB row.
    json rpcBody;
    rpcBody["p_election_id"] = electionId;
    rpcBody["p_voter_id"]    = voterId;
    rpcBody["p_candidate"]   = candidateName;

    auto rpcResult = SupabaseClient::instance().request(
        "POST", "rpc/cast_vote_secret", rpcBody.dump());

    try {
        auto rpcJson = json::parse(rpcResult.body);
        bool success = rpcJson.value("success", false);
        std::string message = rpcJson.value("message", "Unknown error");
        if (!success) {
            res["success"]=false; res["message"]=message; return res;
        }
    } catch (...) {
        LOG_ERROR("[castVote] RPC cast_vote_secret failed or not found. "
                  "Run the SQL migration in supabase_schema.sql.");
        res["success"]=false;
        res["message"]="Vote service unavailable - please contact the administrator";
        return res;
    }

    // Invalidate candidate cache - vote counts changed
    CandidateCache::instance().invalidate(electionId);

    res["success"] = true;
    res["message"] = "Vote cast successfully";
    return res;
}

json PublicVoteController::getResults(const std::string& electionId) {
    // Get candidate names
    auto cands = supabaseRequest("GET",
        "candidates?select=name&election_id=eq."+electionId+"&order=name.asc");

    // Get all anonymous ballots for this election (no voter_id column exists here)
    auto votes = supabaseRequest("GET",
        "vote_ballots?select=candidate_name&election_id=eq."+electionId);

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

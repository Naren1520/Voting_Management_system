#include "../../include/controllers/CandidateController.h"
#include "../../include/cache/CandidateCache.h"
#include "../../include/db/SupabaseClient.h"

bool CandidateController::ownsElection(const std::string& userId,
                                       const std::string& electionId) {
    auto r = supabaseRequest("GET",
        "elections?select=id&id=eq."+electionId+"&user_id=eq."+userId+"&limit=1");
    try {
        auto arr = json::parse(r.body);
        return arr.is_array() && !arr.empty();
    } catch (...) { return false; }
}

json CandidateController::getCandidates(const std::string& userId,
                                        const std::string& electionId) {
    if (!ownsElection(userId, electionId)) {
        json r; r["success"]=false; r["message"]="Unauthorized"; return r;
    }
    auto res = supabaseRequest("GET",
        "candidates?select=id,name&election_id=eq."+electionId+
        "&order=name.asc");
    try {
        json r; r["success"]=true; r["candidates"]=json::parse(res.body); return r;
    } catch (...) {
        json r; r["success"]=false; r["message"]="Failed to load candidates"; return r;
    }
}

json CandidateController::addCandidate(const std::string& userId,
                                       const std::string& electionId,
                                       const std::string& name) {
    json res;
    if (!ownsElection(userId, electionId)) {
        res["success"]=false; res["message"]="Unauthorized"; return res;
    }
    if (name.empty()) {
        res["success"]=false; res["message"]="Candidate name required"; return res;
    }
    auto check = supabaseRequest("GET",
        "candidates?select=id&election_id=eq."+electionId+
        "&name=eq."+SupabaseClient::urlEncode(name)+"&limit=1");
    try {
        auto arr = json::parse(check.body);
        if (arr.is_array() && !arr.empty()) {
            res["success"]=false; res["message"]="Candidate already exists"; return res;
        }
    } catch (...) {}

    json body; body["election_id"]=electionId; body["name"]=name;
    auto r = supabaseRequest("POST","candidates",body.dump());
    try {
        auto arr = json::parse(r.body);
        if ((r.statusCode==200||r.statusCode==201) && arr.is_array() && !arr.empty()) {
            // Invalidate candidate cache - list has changed
            CandidateCache::instance().invalidate(electionId);
            res["success"]=true; res["message"]="Candidate added";
            res["candidates"] = getCandidates(userId,electionId)["candidates"];
        } else {
            res["success"]=false; res["message"]="Failed to add candidate";
        }
    } catch (...) { res["success"]=false; res["message"]="Server error"; }
    return res;
}

json CandidateController::deleteCandidate(const std::string& userId,
                                          const std::string& electionId,
                                          const std::string& name) {
    if (!ownsElection(userId, electionId)) {
        json r; r["success"]=false; r["message"]="Unauthorized"; return r;
    }
    supabaseRequest("DELETE",
        "candidates?election_id=eq."+electionId+
        "&name=eq."+SupabaseClient::urlEncode(name));
    // Invalidate candidate cache - list has changed
    CandidateCache::instance().invalidate(electionId);
    json res; res["success"]=true; res["message"]="Candidate deleted";
    res["candidates"] = getCandidates(userId,electionId)["candidates"];
    return res;
}

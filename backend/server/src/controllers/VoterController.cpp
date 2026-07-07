#include "../../include/controllers/VoterController.h"
#include "../../include/db/SupabaseClient.h"

bool VoterController::ownsElection(const std::string& userId,
                                   const std::string& electionId) {
    auto r = supabaseRequest("GET",
        "elections?select=id&id=eq."+electionId+"&user_id=eq."+userId+"&limit=1");
    try {
        auto arr = json::parse(r.body);
        return arr.is_array() && !arr.empty();
    } catch (...) { return false; }
}

json VoterController::getVoters(const std::string& userId,
                                const std::string& electionId) {
    if (!ownsElection(userId, electionId)) {
        json r; r["success"]=false; r["message"]="Unauthorized"; return r;
    }
    auto res = supabaseRequest("GET",
        "voters?select=id,voter_id,name,email,phone"
        "&election_id=eq."+electionId+"&order=name.asc");
    try {
        json r; r["success"]=true; r["voters"]=json::parse(res.body); return r;
    } catch (...) {
        json r; r["success"]=false; r["message"]="Failed to load voters"; return r;
    }
}

json VoterController::addVoter(const std::string& userId,
                               const std::string& electionId,
                               const std::string& voterId,
                               const std::string& name,
                               const std::string& email,
                               const std::string& phone) {
    json res;
    if (!ownsElection(userId, electionId)) {
        res["success"]=false; res["message"]="Unauthorized"; return res;
    }
    if (voterId.empty() || name.empty()) {
        res["success"]=false; res["message"]="Voter ID and name are required"; return res;
    }
    auto check = supabaseRequest("GET",
        "voters?select=id&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId)+"&limit=1");
    try {
        auto arr = json::parse(check.body);
        if (arr.is_array() && !arr.empty()) {
            res["success"]=false; res["message"]="Voter ID already exists"; return res;
        }
    } catch (...) {}

    json body;
    body["election_id"] = electionId;
    body["voter_id"]    = voterId;
    body["name"]        = name;
    body["email"]       = email;
    body["phone"]       = phone;
    supabaseRequest("POST","voters",body.dump());
    res["success"]=true; res["message"]="Voter added";
    res["voters"] = getVoters(userId,electionId)["voters"];
    return res;
}

json VoterController::syncVoters(const std::string& userId,
                                  const std::string& electionId,
                                  const json& voterList) {
    json res;
    if (!ownsElection(userId, electionId)) {
        res["success"]=false; res["message"]="Unauthorized"; return res;
    }
    if (!voterList.is_array()) {
        res["success"]=false; res["message"]="voters must be an array"; return res;
    }

    // Delete existing voters first
    supabaseRequest("DELETE","voters?election_id=eq."+electionId);

    // Batch-insert all voters in a single POST
    if (!voterList.empty()) {
        json batch = json::array();
        for (const auto& v : voterList) {
            json row;
            row["election_id"] = electionId;
            row["voter_id"]    = v.value("voter_id","");
            row["name"]        = v.value("name","");
            row["email"]       = v.value("email","");
            row["phone"]       = v.value("phone","");
            if (!row["voter_id"].get<std::string>().empty())
                batch.push_back(row);
        }
        if (!batch.empty())
            supabaseRequest("POST","voters",batch.dump());
    }

    res["success"]=true; res["message"]="Voters synced";
    res["voters"] = getVoters(userId,electionId)["voters"];
    return res;
}

json VoterController::deleteVoter(const std::string& userId,
                                   const std::string& electionId,
                                   const std::string& voterId) {
    if (!ownsElection(userId, electionId)) {
        json r; r["success"]=false; r["message"]="Unauthorized"; return r;
    }
    supabaseRequest("DELETE",
        "voters?election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId));
    json res; res["success"]=true; res["message"]="Voter removed";
    return res;
}

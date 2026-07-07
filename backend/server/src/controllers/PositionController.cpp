#include "../../include/controllers/PositionController.h"
#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Logger.h"

bool PositionController::ownsElection(const std::string& userId,
                                      const std::string& electionId) {
    auto r = supabaseRequest("GET",
        "elections?select=id&id=eq."+electionId+"&user_id=eq."+userId+"&limit=1");
    try { auto a = json::parse(r.body); return a.is_array() && !a.empty(); }
    catch (...) { return false; }
}

json PositionController::getPositions(const std::string& userId,
                                      const std::string& electionId) {
    if (!ownsElection(userId, electionId)) {
        json r; r["success"]=false; r["message"]="Unauthorized"; return r;
    }
    auto r = supabaseRequest("GET",
        "positions?select=id,title&election_id=eq."+electionId+
        "&order=created_at.asc");
    try {
        json res; res["success"]=true; res["positions"]=json::parse(r.body); return res;
    } catch (...) {
        json res; res["success"]=false; res["message"]="Failed to load positions"; return res;
    }
}

json PositionController::addPosition(const std::string& userId,
                                     const std::string& electionId,
                                     const std::string& title) {
    json res;
    if (!ownsElection(userId, electionId)) {
        res["success"]=false; res["message"]="Unauthorized"; return res;
    }
    if (title.empty()) {
        res["success"]=false; res["message"]="Position title required"; return res;
    }

    json body;
    body["election_id"] = electionId;
    body["title"]       = title;
    auto r = supabaseRequest("POST","positions",body.dump());
    try {
        auto arr = json::parse(r.body);
        if ((r.statusCode==200||r.statusCode==201) && arr.is_array() && !arr.empty()) {
            res["success"]=true; res["message"]="Position added";
            res["positions"] = getPositions(userId,electionId)["positions"];
        } else {
            LOG_ERROR("[addPosition] Supabase error. Status: " +
                      std::to_string(r.statusCode) + " Body: " + r.body);
            res["success"]=false; res["message"]="Failed to add position";
        }
    } catch (...) { res["success"]=false; res["message"]="Server error"; }
    return res;
}

json PositionController::deletePosition(const std::string& userId,
                                        const std::string& electionId,
                                        const std::string& positionId) {
    if (!ownsElection(userId, electionId)) {
        json r; r["success"]=false; r["message"]="Unauthorized"; return r;
    }
    supabaseRequest("DELETE","positions?id=eq."+positionId+
        "&election_id=eq."+electionId);
    json res; res["success"]=true; res["message"]="Position deleted";
    res["positions"] = getPositions(userId,electionId)["positions"];
    return res;
}

json PositionController::getCandidates(const std::string& userId,
                                       const std::string& electionId,
                                       const std::string& positionId) {
    if (!ownsElection(userId, electionId)) {
        json r; r["success"]=false; r["message"]="Unauthorized"; return r;
    }
    auto r = supabaseRequest("GET",
        "position_candidates?select=id,name&position_id=eq."+positionId+
        "&order=name.asc");
    try {
        json res; res["success"]=true; res["candidates"]=json::parse(r.body); return res;
    } catch (...) {
        json res; res["success"]=false; res["message"]="Failed to load candidates"; return res;
    }
}

json PositionController::addCandidate(const std::string& userId,
                                      const std::string& electionId,
                                      const std::string& positionId,
                                      const std::string& name) {
    json res;
    if (!ownsElection(userId, electionId)) {
        res["success"]=false; res["message"]="Unauthorized"; return res;
    }
    if (name.empty()) {
        res["success"]=false; res["message"]="Candidate name required"; return res;
    }
    auto check = supabaseRequest("GET",
        "position_candidates?select=id&position_id=eq."+positionId+
        "&name=eq."+SupabaseClient::urlEncode(name)+"&limit=1");
    try {
        auto a = json::parse(check.body);
        if (a.is_array() && !a.empty()) {
            res["success"]=false;
            res["message"]="Candidate already exists in this position";
            return res;
        }
    } catch (...) {}

    json body;
    body["election_id"] = electionId;
    body["position_id"] = positionId;
    body["name"]        = name;
    auto r = supabaseRequest("POST","position_candidates",body.dump());
    try {
        auto arr = json::parse(r.body);
        if ((r.statusCode==200||r.statusCode==201) && arr.is_array() && !arr.empty()) {
            res["success"]=true; res["message"]="Candidate added";
            res["candidates"] = getCandidates(userId,electionId,positionId)["candidates"];
        } else {
            res["success"]=false; res["message"]="Failed to add candidate";
        }
    } catch (...) { res["success"]=false; res["message"]="Server error"; }
    return res;
}

json PositionController::deleteCandidate(const std::string& userId,
                                         const std::string& electionId,
                                         const std::string& positionId,
                                         const std::string& name) {
    if (!ownsElection(userId, electionId)) {
        json r; r["success"]=false; r["message"]="Unauthorized"; return r;
    }
    supabaseRequest("DELETE",
        "position_candidates?position_id=eq."+positionId+
        "&name=eq."+SupabaseClient::urlEncode(name));
    json res; res["success"]=true; res["message"]="Candidate removed";
    res["candidates"] = getCandidates(userId,electionId,positionId)["candidates"];
    return res;
}

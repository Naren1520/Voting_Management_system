#include "../../include/controllers/ElectionController.h"
#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Logger.h"

json ElectionController::getElections(const std::string& userId) {
    auto r = supabaseRequest("GET",
        "elections?select=id,title,is_active,election_type,schedule_type,"
        "starts_at,ends_at,schedule_json,timezone,created_at"
        "&user_id=eq." + userId + "&order=created_at.desc");
    try {
        auto arr = json::parse(r.body);
        json res; res["success"] = true;
        res["elections"] = arr.is_array() ? arr : json::array();
        return res;
    } catch (...) {
        json res; res["success"]=false; res["message"]="Failed to load elections";
        return res;
    }
}

json ElectionController::createElection(const std::string& userId,
                                        const std::string& title,
                                        const std::string& electionType,
                                        const std::string& scheduleType,
                                        const std::string& startsAt,
                                        const std::string& endsAt,
                                        const std::string& scheduleJson,
                                        const std::string& timezone) {
    json res;
    if (title.empty()) {
        res["success"] = false; res["message"] = "Title is required"; return res;
    }
    json body;
    body["user_id"]       = userId;
    body["title"]         = title;
    body["is_active"]     = true;
    body["election_type"] = electionType;
    body["schedule_type"] = scheduleType;
    body["timezone"]      = timezone.empty() ? "UTC" : timezone;
    if (!startsAt.empty())     body["starts_at"]     = startsAt;
    if (!endsAt.empty())       body["ends_at"]       = endsAt;
    if (!scheduleJson.empty()) {
        try { body["schedule_json"] = json::parse(scheduleJson); } catch (...) {}
    }

    auto r = supabaseRequest("POST", "elections", body.dump());
    try {
        auto arr = json::parse(r.body);
        if ((r.statusCode==200||r.statusCode==201) && arr.is_array() && !arr.empty()) {
            res["success"] = true; res["message"] = "Election created";
            res["election"] = arr[0];
        } else {
            res["success"] = false; res["message"] = "Failed to create election";
        }
    } catch (...) {
        res["success"] = false; res["message"] = "Server error";
    }
    return res;
}

json ElectionController::getElection(const std::string& userId,
                                     const std::string& electionId) {
    auto r = supabaseRequest("GET",
        "elections?select=id,title,is_active,election_type,schedule_type,"
        "starts_at,ends_at,schedule_json,timezone,created_at"
        "&id=eq." + electionId + "&user_id=eq." + userId + "&limit=1");
    try {
        auto arr = json::parse(r.body);
        if (arr.is_array() && !arr.empty()) {
            json res; res["success"]=true; res["election"]=arr[0]; return res;
        }
    } catch (...) {}
    json res; res["success"]=false; res["message"]="Election not found";
    return res;
}

json ElectionController::deleteElection(const std::string& userId,
                                        const std::string& electionId) {
    auto check = supabaseRequest("GET",
        "elections?select=id&id=eq."+electionId+"&user_id=eq."+userId+"&limit=1");
    try {
        auto arr = json::parse(check.body);
        if (!arr.is_array() || arr.empty()) {
            json res; res["success"]=false; res["message"]="Election not found";
            return res;
        }
    } catch (...) {}
    supabaseRequest("DELETE", "elections?id=eq."+electionId);
    json res; res["success"]=true; res["message"]="Election deleted";
    return res;
}

json ElectionController::updateSchedule(const std::string& userId,
                                        const std::string& electionId,
                                        const json& body) {
    auto check = supabaseRequest("GET",
        "elections?select=id&id=eq."+electionId+"&user_id=eq."+userId+"&limit=1");
    try {
        auto arr = json::parse(check.body);
        if (!arr.is_array() || arr.empty()) {
            json res; res["success"]=false; res["message"]="Election not found";
            return res;
        }
    } catch (...) {
        json res; res["success"]=false; res["message"]="Server error"; return res;
    }

    json upd;
    std::string schedType = body.value("schedule_type","always_on");
    upd["schedule_type"] = schedType;
    upd["timezone"]      = body.value("timezone","UTC");
    if (schedType == "date_range") {
        upd["starts_at"]     = body.value("starts_at","");
        upd["ends_at"]       = body.value("ends_at","");
        upd["schedule_json"] = nullptr;
    } else if (schedType == "recurring") {
        upd["starts_at"]     = nullptr;
        upd["ends_at"]       = nullptr;
        upd["schedule_json"] = body.value("schedule_json", json::object());
    } else {
        upd["starts_at"]     = nullptr;
        upd["ends_at"]       = nullptr;
        upd["schedule_json"] = nullptr;
    }

    auto r = supabaseRequest("PATCH", "elections?id=eq."+electionId, upd.dump());
    json res;
    res["success"] = (r.statusCode==200||r.statusCode==204);
    res["message"] = res["success"].get<bool>() ? "Schedule updated" : "Failed to update schedule";
    return res;
}

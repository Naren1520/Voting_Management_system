/**
 * voting_server_linux.cpp
 * Full-stack Voting System Backend - C++ with Supabase PostgreSQL
 * Compile: g++ -std=c++17 -O2 -o voting_server voting_server_linux.cpp -pthread
 *
 * API Endpoints:
 *   POST /api/auth/signup
 *   POST /api/auth/login
 *   POST /api/auth/logout
 *   GET  /api/elections
 *   POST /api/elections
 *   GET  /api/elections/:id
 *   DELETE /api/elections/:id
 *   GET  /api/elections/:id/candidates
 *   POST /api/elections/:id/candidates
 *   DELETE /api/elections/:id/candidates
 *   GET  /api/elections/:id/voters
 *   POST /api/elections/:id/voters
 *   POST /api/elections/:id/voters/sync
 *   DELETE /api/elections/:id/voters
 *   GET  /api/vote/:id/candidates
 *   POST /api/vote/:id/check
 *   POST /api/vote/:id/cast
 *   GET  /api/vote/:id/results
 */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <array>
#include <map>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include "json.hpp"

using json = nlohmann::json;

// ============================================================================
// CONFIG
// ============================================================================

const std::string SUPABASE_URL = "https://drwkzpoxyhluwuxzcjxx.supabase.co";
const std::string SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImRyd2t6cG94eWhsdXd1eHpjanh4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODI3ODk2NDEsImV4cCI6MjA5ODM2NTY0MX0.ZC6uOOGc8frORkSolT47YwRIQ6QxnBbnkHxpyYQ61Pw";

// ============================================================================
// SIMPLE PASSWORD HASHING (SHA-256 via openssl CLI)
// For production consider linking libcrypto directly
// ============================================================================

std::string hashPassword(const std::string& password) {
    std::string cmd = "echo -n '" + password + "' | openssl dgst -sha256 -hex 2>/dev/null | awk '{print $2}'";
    std::array<char, 128> buf;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe)) result += buf.data();
    pclose(pipe);
    // trim newline
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// ============================================================================
// TOKEN GENERATION (random hex string)
// ============================================================================

std::string generateToken() {
    std::array<char, 128> buf;
    std::string result;
    FILE* pipe = popen("openssl rand -hex 32 2>/dev/null", "r");
    if (!pipe) return "token_fallback_" + std::to_string(time(nullptr));
    while (fgets(buf.data(), buf.size(), pipe)) result += buf.data();
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// ============================================================================
// SUPABASE HTTP CLIENT
// ============================================================================

struct HttpResult {
    int statusCode;
    std::string body;
};

HttpResult supabaseRequest(const std::string& method,
                           const std::string& endpoint,
                           const std::string& body = "",
                           const std::string& extraHeader = "") {
    std::string url = SUPABASE_URL + "/rest/v1/" + endpoint;

    // Write body to temp file to avoid shell quoting issues
    std::string tmpFile = "";
    if (!body.empty()) {
        tmpFile = "/tmp/sb_" + std::to_string(getpid()) + "_" +
                  std::to_string(time(nullptr)) + ".json";
        std::ofstream f(tmpFile);
        f << body;
        f.close();
    }

    std::string cmd = "curl -s -w \"\\n%{http_code}\" -X " + method;
    cmd += " -H \"apikey: " + SUPABASE_KEY + "\"";
    cmd += " -H \"Authorization: Bearer " + SUPABASE_KEY + "\"";
    cmd += " -H \"Content-Type: application/json\"";
    cmd += " -H \"Prefer: return=representation\"";
    if (!extraHeader.empty()) cmd += " -H \"" + extraHeader + "\"";
    if (!tmpFile.empty()) cmd += " -d @" + tmpFile;
    cmd += " \"" + url + "\" 2>/dev/null";

    std::string result;
    std::array<char, 65536> buf;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        if (!tmpFile.empty()) remove(tmpFile.c_str());
        return {500, "[]"};
    }
    while (fgets(buf.data(), buf.size(), pipe)) result += buf.data();
    pclose(pipe);
    if (!tmpFile.empty()) remove(tmpFile.c_str());

    size_t last = result.rfind('\n');
    HttpResult hr;
    if (last != std::string::npos) {
        std::string code = result.substr(last + 1);
        code.erase(code.find_last_not_of(" \t\r\n") + 1);
        hr.statusCode = code.empty() ? 200 : std::stoi(code);
        hr.body = result.substr(0, last);
    } else {
        hr.statusCode = 200;
        hr.body = result;
    }

    std::cerr << "[SB] " << method << " " << endpoint.substr(0,60)
              << " -> " << hr.statusCode << std::endl;
    return hr;
}

// ============================================================================
// URL ENCODE HELPER
// ============================================================================

std::string urlEncode(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r += c;
        else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); r += b; }
    }
    return r;
}

// ============================================================================
// AUTH CONTROLLER
// ============================================================================

class AuthController {
public:

    // POST /api/auth/signup  { name, email, password }
    json signup(const std::string& name, const std::string& email,
                const std::string& password) {
        json res;
        if (name.empty() || email.empty() || password.empty()) {
            res["success"] = false;
            res["message"] = "Name, email and password are required";
            return res;
        }
        if (password.size() < 6) {
            res["success"] = false;
            res["message"] = "Password must be at least 6 characters";
            return res;
        }

        // Check email exists
        auto check = supabaseRequest("GET",
            "users?select=id&email=eq." + urlEncode(email) + "&limit=1");
        try {
            auto arr = json::parse(check.body);
            if (arr.is_array() && !arr.empty()) {
                res["success"] = false;
                res["message"] = "Email already registered";
                return res;
            }
        } catch (...) {}

        std::string hash = hashPassword(password);
        if (hash.empty()) {
            res["success"] = false;
            res["message"] = "Server error during signup";
            return res;
        }

        json body;
        body["name"] = name;
        body["email"] = email;
        body["password_hash"] = hash;

        auto r = supabaseRequest("POST", "users", body.dump());
        try {
            auto arr = json::parse(r.body);
            if ((r.statusCode == 200 || r.statusCode == 201) &&
                arr.is_array() && !arr.empty()) {
                std::string userId = arr[0]["id"].get<std::string>();
                std::string token = createSession(userId);
                res["success"] = true;
                res["message"] = "Account created successfully";
                res["token"] = token;
                res["user"]["id"] = userId;
                res["user"]["name"] = name;
                res["user"]["email"] = email;
            } else {
                res["success"] = false;
                res["message"] = "Failed to create account";
            }
        } catch (...) {
            res["success"] = false;
            res["message"] = "Server error";
        }
        return res;
    }

    // POST /api/auth/login  { email, password }
    json login(const std::string& email, const std::string& password) {
        json res;
        if (email.empty() || password.empty()) {
            res["success"] = false;
            res["message"] = "Email and password are required";
            return res;
        }

        auto r = supabaseRequest("GET",
            "users?select=id,name,email,password_hash&email=eq." +
            urlEncode(email) + "&limit=1");
        try {
            auto arr = json::parse(r.body);
            if (!arr.is_array() || arr.empty()) {
                res["success"] = false;
                res["message"] = "Invalid email or password";
                return res;
            }
            std::string storedHash = arr[0]["password_hash"].get<std::string>();
            std::string inputHash  = hashPassword(password);
            if (storedHash != inputHash) {
                res["success"] = false;
                res["message"] = "Invalid email or password";
                return res;
            }
            std::string userId = arr[0]["id"].get<std::string>();
            std::string token  = createSession(userId);
            res["success"] = true;
            res["message"] = "Login successful";
            res["token"] = token;
            res["user"]["id"]    = userId;
            res["user"]["name"]  = arr[0]["name"].get<std::string>();
            res["user"]["email"] = email;
        } catch (...) {
            res["success"] = false;
            res["message"] = "Server error during login";
        }
        return res;
    }

    // Validate token → return user_id or empty string
    std::string validateToken(const std::string& token) {
        if (token.empty()) return "";
        // Delete expired sessions first
        supabaseRequest("DELETE",
            "sessions?expires_at=lt." + currentTimestamp());

        auto r = supabaseRequest("GET",
            "sessions?select=user_id&token=eq." + urlEncode(token) +
            "&limit=1");
        try {
            auto arr = json::parse(r.body);
            if (arr.is_array() && !arr.empty()) {
                return arr[0]["user_id"].get<std::string>();
            }
        } catch (...) {}
        return "";
    }

    // POST /api/auth/logout
    json logout(const std::string& token) {
        supabaseRequest("DELETE", "sessions?token=eq." + urlEncode(token));
        json res;
        res["success"] = true;
        res["message"] = "Logged out successfully";
        return res;
    }

private:
    std::string createSession(const std::string& userId) {
        std::string token = generateToken();
        // Expires in 24 hours
        std::string expires = futureTimestamp(86400);
        json body;
        body["token"]      = token;
        body["user_id"]    = userId;
        body["expires_at"] = expires;
        supabaseRequest("POST", "sessions", body.dump());
        return token;
    }

    std::string currentTimestamp() {
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
        return std::string(buf);
    }

    std::string futureTimestamp(int secondsFromNow) {
        time_t t = time(nullptr) + secondsFromNow;
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
        return std::string(buf);
    }
};

// ============================================================================
// ELECTION CONTROLLER
// ============================================================================

class ElectionController {
public:

    // GET /api/elections  (owned by user)
    json getElections(const std::string& userId) {
        auto r = supabaseRequest("GET",
            "elections?select=id,title,is_active,created_at&user_id=eq." +
            userId + "&order=created_at.desc");
        try {
            auto arr = json::parse(r.body);
            json res;
            res["success"] = true;
            res["elections"] = arr.is_array() ? arr : json::array();
            return res;
        } catch (...) {
            json res; res["success"]=false; res["message"]="Failed to load elections";
            return res;
        }
    }

    // POST /api/elections  { title }
    json createElection(const std::string& userId, const std::string& title) {
        json res;
        if (title.empty()) {
            res["success"] = false; res["message"] = "Title is required";
            return res;
        }
        json body;
        body["user_id"] = userId;
        body["title"]   = title;
        body["is_active"] = true;
        auto r = supabaseRequest("POST", "elections", body.dump());
        try {
            auto arr = json::parse(r.body);
            if ((r.statusCode==200||r.statusCode==201) && arr.is_array() && !arr.empty()) {
                res["success"] = true;
                res["message"] = "Election created";
                res["election"] = arr[0];
            } else {
                res["success"] = false; res["message"] = "Failed to create election";
            }
        } catch (...) {
            res["success"] = false; res["message"] = "Server error";
        }
        return res;
    }

    // GET single election (ownership verified)
    json getElection(const std::string& userId, const std::string& electionId) {
        auto r = supabaseRequest("GET",
            "elections?select=id,title,is_active,created_at&id=eq." +
            electionId + "&user_id=eq." + userId + "&limit=1");
        try {
            auto arr = json::parse(r.body);
            if (arr.is_array() && !arr.empty()) {
                json res; res["success"]=true; res["election"]=arr[0];
                return res;
            }
        } catch (...) {}
        json res; res["success"]=false; res["message"]="Election not found";
        return res;
    }

    // DELETE election
    json deleteElection(const std::string& userId, const std::string& electionId) {
        // Verify ownership first
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
};

// ============================================================================
// CANDIDATE CONTROLLER
// ============================================================================

class CandidateController {
public:

    // Verify election belongs to user
    bool ownsElection(const std::string& userId, const std::string& electionId) {
        auto r = supabaseRequest("GET",
            "elections?select=id&id=eq."+electionId+"&user_id=eq."+userId+"&limit=1");
        try {
            auto arr = json::parse(r.body);
            return arr.is_array() && !arr.empty();
        } catch (...) { return false; }
    }

    // GET /api/elections/:id/candidates
    json getCandidates(const std::string& userId, const std::string& electionId) {
        if (!ownsElection(userId, electionId)) {
            json r; r["success"]=false; r["message"]="Unauthorized"; return r;
        }
        auto res = supabaseRequest("GET",
            "candidates?select=id,name,votes&election_id=eq."+electionId+
            "&order=votes.desc");
        try {
            json r; r["success"]=true;
            r["candidates"] = json::parse(res.body);
            return r;
        } catch (...) {
            json r; r["success"]=false; r["message"]="Failed to load candidates";
            return r;
        }
    }

    // POST /api/elections/:id/candidates  { name }
    json addCandidate(const std::string& userId, const std::string& electionId,
                      const std::string& name) {
        json res;
        if (!ownsElection(userId, electionId)) {
            res["success"]=false; res["message"]="Unauthorized"; return res;
        }
        if (name.empty()) {
            res["success"]=false; res["message"]="Candidate name required"; return res;
        }
        // Check duplicate
        auto check = supabaseRequest("GET",
            "candidates?select=id&election_id=eq."+electionId+
            "&name=eq."+urlEncode(name)+"&limit=1");
        try {
            auto arr = json::parse(check.body);
            if (arr.is_array() && !arr.empty()) {
                res["success"]=false; res["message"]="Candidate already exists"; return res;
            }
        } catch (...) {}

        json body; body["election_id"]=electionId; body["name"]=name; body["votes"]=0;
        auto r = supabaseRequest("POST","candidates",body.dump());
        try {
            auto arr = json::parse(r.body);
            if ((r.statusCode==200||r.statusCode==201)&&arr.is_array()&&!arr.empty()) {
                res["success"]=true; res["message"]="Candidate added";
                res["candidates"] = getCandidates(userId,electionId)["candidates"];
            } else {
                res["success"]=false; res["message"]="Failed to add candidate";
            }
        } catch (...) { res["success"]=false; res["message"]="Server error"; }
        return res;
    }

    // DELETE /api/elections/:id/candidates  { name }
    json deleteCandidate(const std::string& userId, const std::string& electionId,
                         const std::string& name) {
        if (!ownsElection(userId, electionId)) {
            json r; r["success"]=false; r["message"]="Unauthorized"; return r;
        }
        supabaseRequest("DELETE",
            "candidates?election_id=eq."+electionId+"&name=eq."+urlEncode(name));
        json res; res["success"]=true; res["message"]="Candidate deleted";
        res["candidates"] = getCandidates(userId,electionId)["candidates"];
        return res;
    }
};

// ============================================================================
// VOTER CONTROLLER
// ============================================================================

class VoterController {
public:

    bool ownsElection(const std::string& userId, const std::string& electionId) {
        auto r = supabaseRequest("GET",
            "elections?select=id&id=eq."+electionId+"&user_id=eq."+userId+"&limit=1");
        try {
            auto arr = json::parse(r.body);
            return arr.is_array() && !arr.empty();
        } catch (...) { return false; }
    }

    // GET /api/elections/:id/voters
    json getVoters(const std::string& userId, const std::string& electionId) {
        if (!ownsElection(userId, electionId)) {
            json r; r["success"]=false; r["message"]="Unauthorized"; return r;
        }
        auto res = supabaseRequest("GET",
            "registered_voters?select=id,voter_id,name,email,phone&election_id=eq."+
            electionId+"&order=name.asc");
        try {
            json r; r["success"]=true; r["voters"]=json::parse(res.body); return r;
        } catch (...) {
            json r; r["success"]=false; r["message"]="Failed to load voters"; return r;
        }
    }

    // POST /api/elections/:id/voters  { voter_id, name, email, phone }
    json addVoter(const std::string& userId, const std::string& electionId,
                  const std::string& voterId, const std::string& name,
                  const std::string& email, const std::string& phone) {
        json res;
        if (!ownsElection(userId, electionId)) {
            res["success"]=false; res["message"]="Unauthorized"; return res;
        }
        if (voterId.empty() || name.empty()) {
            res["success"]=false; res["message"]="Voter ID and name are required"; return res;
        }
        // Check duplicate voter_id in this election
        auto check = supabaseRequest("GET",
            "registered_voters?select=id&election_id=eq."+electionId+
            "&voter_id=eq."+urlEncode(voterId)+"&limit=1");
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
        supabaseRequest("POST","registered_voters",body.dump());
        res["success"]=true; res["message"]="Voter added";
        res["voters"] = getVoters(userId,electionId)["voters"];
        return res;
    }

    // POST /api/elections/:id/voters/sync  { voters: [...] }
    json syncVoters(const std::string& userId, const std::string& electionId,
                    const json& voterList) {
        json res;
        if (!ownsElection(userId, electionId)) {
            res["success"]=false; res["message"]="Unauthorized"; return res;
        }
        // Delete all existing registered voters for this election
        supabaseRequest("DELETE","registered_voters?election_id=eq."+electionId);
        // Re-insert all
        for (const auto& v : voterList) {
            json body;
            body["election_id"] = electionId;
            body["voter_id"]    = v.value("voter_id","");
            body["name"]        = v.value("name","");
            body["email"]       = v.value("email","");
            body["phone"]       = v.value("phone","");
            supabaseRequest("POST","registered_voters",body.dump());
        }
        res["success"]=true; res["message"]="Voters synced";
        res["voters"] = getVoters(userId,electionId)["voters"];
        return res;
    }

    // DELETE /api/elections/:id/voters  { voter_id }
    json deleteVoter(const std::string& userId, const std::string& electionId,
                     const std::string& voterId) {
        if (!ownsElection(userId, electionId)) {
            json r; r["success"]=false; r["message"]="Unauthorized"; return r;
        }
        supabaseRequest("DELETE",
            "registered_voters?election_id=eq."+electionId+
            "&voter_id=eq."+urlEncode(voterId));
        json res; res["success"]=true; res["message"]="Voter removed";
        return res;
    }
};

// ============================================================================
// PUBLIC VOTING CONTROLLER (no auth - election_id scoped)
// ============================================================================

class PublicVoteController {
public:

    // GET /api/vote/:election_id/candidates
    json getCandidates(const std::string& electionId) {
        // Verify election exists and is active
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

        auto res = supabaseRequest("GET",
            "candidates?select=name,votes&election_id=eq."+electionId+
            "&order=name.asc");
        try {
            json r; r["success"]=true; r["candidates"]=json::parse(res.body); return r;
        } catch (...) {
            json r; r["success"]=false; r["message"]="Failed to load candidates"; return r;
        }
    }

    // POST /api/vote/:election_id/check  { voter_id }
    json checkVoter(const std::string& electionId, const std::string& voterId) {
        json res;
        if (voterId.empty()) {
            res["success"]=false; res["message"]="Voter ID required"; return res;
        }
        // Check registered
        auto reg = supabaseRequest("GET",
            "registered_voters?select=voter_id,name&election_id=eq."+electionId+
            "&voter_id=eq."+urlEncode(voterId)+"&limit=1");
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
        // Check already voted
        auto voted = supabaseRequest("GET",
            "votes_cast?select=voter_id&election_id=eq."+electionId+
            "&voter_id=eq."+urlEncode(voterId)+"&limit=1");
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

    // POST /api/vote/:election_id/cast  { voter_id, candidate_name }
    json castVote(const std::string& electionId, const std::string& voterId,
                  const std::string& candidateName) {
        json res;
        // Re-verify voter
        auto check = checkVoter(electionId, voterId);
        if (!check["success"].get<bool>()) return check;

        // Verify candidate exists
        auto cand = supabaseRequest("GET",
            "candidates?select=name,votes&election_id=eq."+electionId+
            "&name=eq."+urlEncode(candidateName)+"&limit=1");
        json candArr;
        try {
            candArr = json::parse(cand.body);
            if (!candArr.is_array() || candArr.empty()) {
                res["success"]=false; res["message"]="Candidate not found"; return res;
            }
        } catch (...) {
            res["success"]=false; res["message"]="Database error"; return res;
        }

        // Increment vote
        int newVotes = candArr[0]["votes"].get<int>() + 1;
        json upd; upd["votes"] = newVotes;
        supabaseRequest("PATCH",
            "candidates?election_id=eq."+electionId+
            "&name=eq."+urlEncode(candidateName), upd.dump());

        // Record vote
        json voteBody;
        voteBody["election_id"]     = electionId;
        voteBody["voter_id"]        = voterId;
        voteBody["candidate_name"]  = candidateName;
        supabaseRequest("POST","votes_cast",voteBody.dump());

        res["success"]=true;
        res["message"]="Vote cast successfully for "+candidateName;
        res["candidates"] = getCandidates(electionId)["candidates"];
        return res;
    }

    // GET /api/vote/:election_id/results
    json getResults(const std::string& electionId) {
        auto cands = supabaseRequest("GET",
            "candidates?select=name,votes&election_id=eq."+electionId+
            "&order=votes.desc");
        auto totalRes = supabaseRequest("GET",
            "votes_cast?select=voter_id&election_id=eq."+electionId);
        try {
            auto candArr = json::parse(cands.body);
            auto voteArr = json::parse(totalRes.body);
            json res;
            res["success"]=true;
            res["candidates"]=candArr;
            res["total_votes"]= voteArr.is_array() ? (int)voteArr.size() : 0;
            return res;
        } catch (...) {
            json r; r["success"]=false; r["message"]="Failed to load results";
            return r;
        }
    }
};

// ============================================================================
// HTTP SERVER
// ============================================================================

class HttpServer {
private:
    AuthController     authCtrl;
    ElectionController electionCtrl;
    CandidateController candidateCtrl;
    VoterController    voterCtrl;
    PublicVoteController voteCtrl;
    int listenFd, port;

    // ---- Request parsing helpers ----
    std::string getMethod(const std::string& req) {
        size_t e = req.find(' ');
        return e != std::string::npos ? req.substr(0,e) : "";
    }
    std::string getPath(const std::string& req) {
        size_t s = req.find(' ');
        size_t e = req.find(' ', s+1);
        if (s==std::string::npos||e==std::string::npos) return "/";
        std::string p = req.substr(s+1, e-s-1);
        size_t q = p.find('?');
        return q != std::string::npos ? p.substr(0,q) : p;
    }
    std::string getBody(const std::string& req) {
        size_t p = req.find("\r\n\r\n");
        return p != std::string::npos ? req.substr(p+4) : "";
    }
    std::string getHeader(const std::string& req, const std::string& name) {
        std::string search = name + ": ";
        size_t p = req.find(search);
        if (p == std::string::npos) return "";
        size_t e = req.find("\r\n", p);
        return req.substr(p+search.size(), e-p-search.size());
    }
    std::string getToken(const std::string& req) {
        std::string auth = getHeader(req, "Authorization");
        if (auth.substr(0,7) == "Bearer ") return auth.substr(7);
        return "";
    }

    // ---- Response builder ----
    std::string respond(int code, const std::string& body) {
        std::string s;
        switch(code){case 200:s="OK";break;case 201:s="Created";break;
                     case 400:s="Bad Request";break;case 401:s="Unauthorized";break;
                     case 403:s="Forbidden";break;case 404:s="Not Found";break;
                     default:s="OK";}
        std::string r = "HTTP/1.1 "+std::to_string(code)+" "+s+"\r\n";
        r += "Content-Type: application/json\r\n";
        r += "Access-Control-Allow-Origin: *\r\n";
        r += "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n";
        r += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
        r += "Content-Length: "+std::to_string(body.size())+"\r\n";
        r += "Connection: close\r\n\r\n";
        r += body;
        return r;
    }

    json err(const std::string& msg) {
        json j; j["success"]=false; j["message"]=msg; return j;
    }

    // ---- Path segment extractor ----
    // e.g. /api/elections/UUID/candidates -> segments[2]=UUID, segments[3]=candidates
    std::vector<std::string> splitPath(const std::string& path) {
        std::vector<std::string> segs;
        std::stringstream ss(path);
        std::string seg;
        while (std::getline(ss, seg, '/'))
            if (!seg.empty()) segs.push_back(seg);
        return segs;
    }

    // ---- Main request router ----
    void handleClient(int fd) {
        std::string request;
        char buf[8192];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
            buf[n] = '\0';
            request += buf;
            size_t he = request.find("\r\n\r\n");
            if (he != std::string::npos) {
                size_t clp = request.find("Content-Length: ");
                if (clp != std::string::npos) {
                    size_t cle = request.find("\r\n", clp);
                    int cl = std::stoi(request.substr(clp+16, cle-clp-16));
                    if ((int)request.size()-(int)(he+4) >= cl) break;
                } else break;
            }
        }
        if (request.empty()) { close(fd); return; }

        std::string method = getMethod(request);
        std::string path   = getPath(request);
        std::string body   = getBody(request);
        std::string token  = getToken(request);
        auto segs = splitPath(path);
        std::string response;

        try {
            // CORS preflight
            if (method == "OPTIONS") {
                response = respond(200, "");

            // ── AUTH ─────────────────────────────────────────────────────
            } else if (path == "/api/auth/signup" && method == "POST") {
                auto rb = json::parse(body);
                auto r = authCtrl.signup(rb.value("name",""), rb.value("email",""),
                                         rb.value("password",""));
                response = respond(r["success"].get<bool>() ? 201:400, r.dump());

            } else if (path == "/api/auth/login" && method == "POST") {
                auto rb = json::parse(body);
                auto r = authCtrl.login(rb.value("email",""), rb.value("password",""));
                response = respond(r["success"].get<bool>() ? 200:401, r.dump());

            } else if (path == "/api/auth/logout" && method == "POST") {
                auto r = authCtrl.logout(token);
                response = respond(200, r.dump());

            // ── ELECTIONS ────────────────────────────────────────────────
            } else if (path == "/api/elections" && method == "GET") {
                std::string uid = authCtrl.validateToken(token);
                if (uid.empty()) { response = respond(401, err("Unauthorized").dump()); }
                else { auto r = electionCtrl.getElections(uid); response = respond(200, r.dump()); }

            } else if (path == "/api/elections" && method == "POST") {
                std::string uid = authCtrl.validateToken(token);
                if (uid.empty()) { response = respond(401, err("Unauthorized").dump()); }
                else {
                    auto rb = json::parse(body);
                    auto r = electionCtrl.createElection(uid, rb.value("title",""));
                    response = respond(r["success"].get<bool>() ? 201:400, r.dump());
                }

            // /api/elections/:id
            } else if (segs.size()==3 && segs[0]=="api" && segs[1]=="elections") {
                std::string elecId = segs[2];
                std::string uid = authCtrl.validateToken(token);
                if (uid.empty()) { response = respond(401, err("Unauthorized").dump()); }
                else if (method=="GET") {
                    auto r = electionCtrl.getElection(uid, elecId);
                    response = respond(r["success"].get<bool>() ? 200:404, r.dump());
                } else if (method=="DELETE") {
                    auto r = electionCtrl.deleteElection(uid, elecId);
                    response = respond(r["success"].get<bool>() ? 200:404, r.dump());
                }

            // /api/elections/:id/candidates
            } else if (segs.size()==4 && segs[1]=="elections" && segs[3]=="candidates") {
                std::string elecId = segs[2];
                std::string uid = authCtrl.validateToken(token);
                if (uid.empty()) { response = respond(401, err("Unauthorized").dump()); }
                else if (method=="GET") {
                    auto r = candidateCtrl.getCandidates(uid, elecId);
                    response = respond(r["success"].get<bool>() ? 200:403, r.dump());
                } else if (method=="POST") {
                    auto rb = json::parse(body);
                    auto r = candidateCtrl.addCandidate(uid, elecId, rb.value("name",""));
                    response = respond(r["success"].get<bool>() ? 201:400, r.dump());
                } else if (method=="DELETE") {
                    auto rb = json::parse(body);
                    auto r = candidateCtrl.deleteCandidate(uid, elecId, rb.value("name",""));
                    response = respond(r["success"].get<bool>() ? 200:400, r.dump());
                }

            // /api/elections/:id/voters
            } else if (segs.size()==4 && segs[1]=="elections" && segs[3]=="voters") {
                std::string elecId = segs[2];
                std::string uid = authCtrl.validateToken(token);
                if (uid.empty()) { response = respond(401, err("Unauthorized").dump()); }
                else if (method=="GET") {
                    auto r = voterCtrl.getVoters(uid, elecId);
                    response = respond(r["success"].get<bool>() ? 200:403, r.dump());
                } else if (method=="POST") {
                    auto rb = json::parse(body);
                    auto r = voterCtrl.addVoter(uid, elecId,
                        rb.value("voter_id",""), rb.value("name",""),
                        rb.value("email",""), rb.value("phone",""));
                    response = respond(r["success"].get<bool>() ? 201:400, r.dump());
                } else if (method=="DELETE") {
                    auto rb = json::parse(body);
                    auto r = voterCtrl.deleteVoter(uid, elecId, rb.value("voter_id",""));
                    response = respond(r["success"].get<bool>() ? 200:400, r.dump());
                }

            // /api/elections/:id/voters/sync
            } else if (segs.size()==5 && segs[1]=="elections" &&
                       segs[3]=="voters" && segs[4]=="sync") {
                std::string elecId = segs[2];
                std::string uid = authCtrl.validateToken(token);
                if (uid.empty()) { response = respond(401, err("Unauthorized").dump()); }
                else if (method=="POST") {
                    auto rb = json::parse(body);
                    auto r = voterCtrl.syncVoters(uid, elecId,
                        rb.value("voters", json::array()));
                    response = respond(r["success"].get<bool>() ? 200:400, r.dump());
                }

            // ── PUBLIC VOTE ──────────────────────────────────────────────
            // /api/vote/:election_id/candidates
            } else if (segs.size()==4 && segs[1]=="vote" && segs[3]=="candidates") {
                auto r = voteCtrl.getCandidates(segs[2]);
                response = respond(r["success"].get<bool>() ? 200:404, r.dump());

            // /api/vote/:election_id/check
            } else if (segs.size()==4 && segs[1]=="vote" && segs[3]=="check" && method=="POST") {
                auto rb = json::parse(body);
                auto r = voteCtrl.checkVoter(segs[2], rb.value("voter_id",""));
                response = respond(r["success"].get<bool>() ? 200:400, r.dump());

            // /api/vote/:election_id/cast
            } else if (segs.size()==4 && segs[1]=="vote" && segs[3]=="cast" && method=="POST") {
                auto rb = json::parse(body);
                auto r = voteCtrl.castVote(segs[2],
                    rb.value("voter_id",""), rb.value("candidate_name",""));
                response = respond(r["success"].get<bool>() ? 200:400, r.dump());

    // GET /api/vote/:election_id/results
            } else if (segs.size()==4 && segs[1]=="vote" && segs[3]=="results") {
                auto r = voteCtrl.getResults(segs[2]);
                response = respond(r["success"].get<bool>() ? 200:404, r.dump());

            // GET /api/vote/:election_id/info  (public - election title)
            } else if (segs.size()==4 && segs[1]=="vote" && segs[3]=="info") {
                auto r = supabaseRequest("GET",
                    "elections?select=title,is_active&id=eq."+segs[2]+"&limit=1");
                try {
                    auto arr = json::parse(r.body);
                    if (arr.is_array() && !arr.empty()) {
                        json res; res["success"]=true; res["title"]=arr[0]["title"];
                        res["is_active"]=arr[0]["is_active"];
                        response = respond(200, res.dump());
                    } else {
                        response = respond(404, err("Election not found").dump());
                    }
                } catch(...) {
                    response = respond(404, err("Election not found").dump());
                }

            } else {
                response = respond(404, err("Not found").dump());
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERR] " << e.what() << std::endl;
            response = respond(400, err("Invalid request").dump());
        }

        write(fd, response.c_str(), response.size());
        close(fd);
    }

public:
    HttpServer(int p=8080) : listenFd(-1), port(p) {}

    bool start() {
        signal(SIGPIPE, SIG_IGN);
        listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) { std::cerr<<"socket failed\n"; return false; }
        int opt=1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);
        if (bind(listenFd,(sockaddr*)&addr,sizeof(addr))<0) {
            std::cerr<<"bind failed\n"; close(listenFd); return false;
        }
        if (listen(listenFd,SOMAXCONN)<0) {
            std::cerr<<"listen failed\n"; close(listenFd); return false;
        }
        std::cout<<"====================================\n";
        std::cout<<"VoteStack API Server\n";
        std::cout<<"Port: "<<port<<"\n";
        std::cout<<"Database: Supabase PostgreSQL\n";
        std::cout<<"====================================\n";
        while (true) {
            int cfd = accept(listenFd,nullptr,nullptr);
            if (cfd < 0) continue;
            handleClient(cfd);
        }
        close(listenFd);
        return true;
    }
};

// ============================================================================
// MAIN
// ============================================================================

int main() {
    int port = 8080;
    const char* ep = std::getenv("PORT");
    if (ep) port = std::atoi(ep);
    HttpServer server(port);
    server.start();
    return 0;
}

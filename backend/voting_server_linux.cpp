/**
 * voting_server_linux.cpp
 * Linux C++ HTTP server with Supabase PostgreSQL backend
 * Uses Supabase REST API over HTTPS via /usr/bin/curl (subprocess)
 * Compile: g++ -std=c++17 -O2 -o voting_server voting_server_linux.cpp -pthread
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

// POSIX socket headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#include "json.hpp"

using json = nlohmann::json;

// ============================================================================
// SUPABASE CONFIG
// ============================================================================

const std::string SUPABASE_URL  = "https://drwkzpoxyhluWuxzcjxx.supabase.co";
const std::string SUPABASE_KEY  = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImRyd2t6cG94eWhsdXd1eHpjanh4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODI3ODk2NDEsImV4cCI6MjA5ODM2NTY0MX0.ZC6uOOGc8frORkSolT47YwRIQ6QxnBbnkHxpyYQ61Pw";
const std::string ADMIN_PASSWORD = "admin123";

// ============================================================================
// SUPABASE HTTP CLIENT (via curl subprocess)
// ============================================================================

struct HttpResult {
    int statusCode;
    std::string body;
};

// Run curl and capture output
HttpResult supabaseRequest(const std::string& method,
                            const std::string& endpoint,
                            const std::string& body = "",
                            const std::string& extraHeaders = "") {
    std::string url = SUPABASE_URL + "/rest/v1/" + endpoint;

    std::string cmd = "curl -s -w '\\n%{http_code}' -X " + method;
    cmd += " -H 'apikey: " + SUPABASE_KEY + "'";
    cmd += " -H 'Authorization: Bearer " + SUPABASE_KEY + "'";
    cmd += " -H 'Content-Type: application/json'";
    cmd += " -H 'Prefer: return=representation'";
    if (!extraHeaders.empty()) cmd += " " + extraHeaders;
    if (!body.empty()) cmd += " -d '" + body + "'";
    cmd += " '" + url + "'";

    std::array<char, 4096> buf;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {500, "{}"};

    while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
        result += buf.data();
    }
    pclose(pipe);

    // Last line is the HTTP status code
    size_t lastNewline = result.rfind('\n');
    HttpResult hr;
    if (lastNewline != std::string::npos) {
        hr.statusCode = std::stoi(result.substr(lastNewline + 1));
        hr.body = result.substr(0, lastNewline);
    } else {
        hr.statusCode = 200;
        hr.body = result;
    }
    return hr;
}

// ============================================================================
// VOTING CONTROLLER (Supabase-backed)
// ============================================================================

class VotingController {
public:

    // ---------- GET ALL CANDIDATES ----------
    json getAllCandidates() {
        auto res = supabaseRequest("GET", "candidates?select=name,votes&order=votes.desc");
        try {
            return json::parse(res.body);
        } catch (...) {
            return json::array();
        }
    }

    // ---------- GET ALL VOTES ----------
    json getAllVotes() {
        auto res = supabaseRequest("GET", "voters?select=voter_id,candidate_name");
        try {
            return json::parse(res.body);
        } catch (...) {
            return json::array();
        }
    }

    // ---------- CHECK VOTER REGISTERED ----------
    json checkVoterRegistration(const std::string& voterId) {
        json response;
        if (voterId.empty()) {
            response["success"] = false;
            response["message"] = "Voter ID cannot be empty";
            return response;
        }

        auto res = supabaseRequest("GET",
            "registered_voters?select=id&id=eq." + voterId + "&limit=1");
        try {
            auto arr = json::parse(res.body);
            if (arr.is_array() && !arr.empty()) {
                response["success"] = true;
                response["registered"] = true;
                response["message"] = "Voter is registered and can vote";
            } else {
                response["success"] = false;
                response["registered"] = false;
                response["message"] = "Voter ID not found in registered voters list";
            }
        } catch (...) {
            response["success"] = false;
            response["message"] = "Database error";
        }
        return response;
    }

    // ---------- CAST VOTE ----------
    json castVote(const std::string& voterId, const std::string& candidateName) {
        json response;

        if (voterId.empty()) {
            response["success"] = false;
            response["message"] = "Voter ID cannot be empty";
            return response;
        }
        if (candidateName.empty()) {
            response["success"] = false;
            response["message"] = "Candidate name cannot be empty";
            return response;
        }

        // 1. Check voter is registered
        auto regRes = supabaseRequest("GET",
            "registered_voters?select=id&id=eq." + voterId + "&limit=1");
        try {
            auto arr = json::parse(regRes.body);
            if (!arr.is_array() || arr.empty()) {
                response["success"] = false;
                response["registered"] = false;
                response["message"] = "Voter ID not registered. Please register before voting.";
                return response;
            }
        } catch (...) {
            response["success"] = false;
            response["message"] = "Database error checking registration";
            return response;
        }

        // 2. Check not already voted
        auto votedRes = supabaseRequest("GET",
            "voters?select=voter_id&voter_id=eq." + voterId + "&limit=1");
        try {
            auto arr = json::parse(votedRes.body);
            if (arr.is_array() && !arr.empty()) {
                response["success"] = false;
                response["message"] = "This voter ID has already voted";
                return response;
            }
        } catch (...) {}

        // 3. Check candidate exists
        auto candRes = supabaseRequest("GET",
            "candidates?select=name,votes&name=eq." + urlEncode(candidateName) + "&limit=1");
        json candArr;
        try {
            candArr = json::parse(candRes.body);
            if (!candArr.is_array() || candArr.empty()) {
                response["success"] = false;
                response["message"] = "Candidate not found: " + candidateName;
                return response;
            }
        } catch (...) {
            response["success"] = false;
            response["message"] = "Database error checking candidate";
            return response;
        }

        // 4. Increment vote count using RPC
        int currentVotes = candArr[0]["votes"].get<int>();
        int newVotes = currentVotes + 1;

        json updateBody;
        updateBody["votes"] = newVotes;
        auto updateRes = supabaseRequest("PATCH",
            "candidates?name=eq." + urlEncode(candidateName),
            updateBody.dump());

        // 5. Record the voter
        json voterBody;
        voterBody["voter_id"] = voterId;
        voterBody["candidate_name"] = candidateName;
        supabaseRequest("POST", "voters", voterBody.dump());

        response["success"] = true;
        response["message"] = "Vote recorded successfully for " + candidateName;
        response["candidates"] = getAllCandidates();
        return response;
    }

    // ---------- ADD CANDIDATE ----------
    json addCandidate(const std::string& candidateName, const std::string& password) {
        json response;
        if (password != ADMIN_PASSWORD) {
            response["success"] = false;
            response["message"] = "Invalid admin password";
            return response;
        }
        if (candidateName.empty()) {
            response["success"] = false;
            response["message"] = "Candidate name cannot be empty";
            return response;
        }

        // Check duplicate
        auto check = supabaseRequest("GET",
            "candidates?select=name&name=eq." + urlEncode(candidateName) + "&limit=1");
        try {
            auto arr = json::parse(check.body);
            if (arr.is_array() && !arr.empty()) {
                response["success"] = false;
                response["message"] = "Candidate already exists: " + candidateName;
                return response;
            }
        } catch (...) {}

        json body;
        body["name"] = candidateName;
        body["votes"] = 0;
        auto res = supabaseRequest("POST", "candidates", body.dump());

        if (res.statusCode == 200 || res.statusCode == 201) {
            response["success"] = true;
            response["message"] = "Candidate added successfully: " + candidateName;
            response["candidates"] = getAllCandidates();
        } else {
            response["success"] = false;
            response["message"] = "Failed to add candidate";
        }
        return response;
    }

    // ---------- DELETE CANDIDATE ----------
    json deleteCandidate(const std::string& candidateName, const std::string& password) {
        json response;
        if (password != ADMIN_PASSWORD) {
            response["success"] = false;
            response["message"] = "Invalid admin password";
            return response;
        }
        if (candidateName.empty()) {
            response["success"] = false;
            response["message"] = "Candidate name cannot be empty";
            return response;
        }

        auto res = supabaseRequest("DELETE",
            "candidates?name=eq." + urlEncode(candidateName));

        response["success"] = true;
        response["message"] = "Candidate deleted successfully: " + candidateName;
        response["candidates"] = getAllCandidates();
        return response;
    }

    // ---------- DELETE VOTER ----------
    json deleteVoter(const std::string& voterId, const std::string& password) {
        json response;
        if (password != ADMIN_PASSWORD) {
            response["success"] = false;
            response["message"] = "Invalid admin password";
            return response;
        }
        if (voterId.empty()) {
            response["success"] = false;
            response["message"] = "Voter ID cannot be empty";
            return response;
        }

        supabaseRequest("DELETE", "voters?voter_id=eq." + voterId);

        response["success"] = true;
        response["message"] = "Voter deleted successfully: " + voterId;
        return response;
    }

    // ---------- SYNC CANDIDATES ----------
    json syncCandidates(const json& candidateList, const std::string& password) {
        json response;
        if (password != ADMIN_PASSWORD) {
            response["success"] = false;
            response["message"] = "Invalid admin password";
            return response;
        }

        // Delete all and re-insert
        supabaseRequest("DELETE", "candidates?id=gte.0");
        for (const auto& item : candidateList) {
            json body;
            body["name"] = item["name"];
            body["votes"] = item.value("votes", 0);
            supabaseRequest("POST", "candidates", body.dump());
        }

        response["success"] = true;
        response["message"] = "Candidates synced successfully";
        response["candidates"] = getAllCandidates();
        return response;
    }

    // ---------- SYNC VOTERS (registered) ----------
    json syncVoters(const json& voterList, const std::string& password) {
        json response;
        if (password != ADMIN_PASSWORD) {
            response["success"] = false;
            response["message"] = "Invalid admin password";
            return response;
        }

        // Upsert registered voters
        for (const auto& item : voterList) {
            json body;
            body["id"]    = item["id"];
            body["name"]  = item["name"];
            body["email"] = item["email"];
            body["phone"] = item.value("phone", "");
            supabaseRequest("POST",
                "registered_voters",
                body.dump(),
                "-H 'Prefer: resolution=merge-duplicates'");
        }

        response["success"] = true;
        response["message"] = "Voters synced successfully";
        return response;
    }

private:
    // Simple URL encoder for candidate names with spaces/special chars
    std::string urlEncode(const std::string& s) {
        std::string result;
        for (unsigned char c : s) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                result += c;
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", c);
                result += buf;
            }
        }
        return result;
    }
};

// ============================================================================
// HTTP SERVER (POSIX - identical to before)
// ============================================================================

class HttpServer {
private:
    VotingController controller;
    int listenFd;
    int port;

    std::string parseBodyFromRequest(const std::string& request) {
        size_t pos = request.find("\r\n\r\n");
        if (pos != std::string::npos) return request.substr(pos + 4);
        return "";
    }

    std::string getPath(const std::string& request) {
        size_t start = request.find(' ');
        size_t end = request.find(' ', start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            std::string p = request.substr(start + 1, end - start - 1);
            size_t q = p.find('?');
            if (q != std::string::npos) p = p.substr(0, q);
            return p;
        }
        return "/";
    }

    std::string getMethod(const std::string& request) {
        size_t end = request.find(' ');
        if (end != std::string::npos) return request.substr(0, end);
        return "";
    }

    std::string buildHttpResponse(int statusCode, const std::string& body) {
        std::string statusMsg;
        switch (statusCode) {
            case 200: statusMsg = "OK"; break;
            case 400: statusMsg = "Bad Request"; break;
            case 404: statusMsg = "Not Found"; break;
            default:  statusMsg = "OK"; break;
        }
        std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + statusMsg + "\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        response += "Access-Control-Allow-Headers: Content-Type\r\n";
        response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += body;
        return response;
    }

    void handleClient(int clientFd) {
        std::string request;
        char buffer[4096];
        ssize_t bytesRead;

        while ((bytesRead = read(clientFd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            request += buffer;
            size_t headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                size_t clPos = request.find("Content-Length: ");
                if (clPos != std::string::npos) {
                    size_t clEnd = request.find("\r\n", clPos);
                    int contentLength = std::stoi(request.substr(clPos + 16, clEnd - clPos - 16));
                    int bodyReceived = (int)request.size() - (int)(headerEnd + 4);
                    if (bodyReceived >= contentLength) break;
                } else {
                    break;
                }
            }
        }

        if (request.empty()) { close(clientFd); return; }

        std::string method = getMethod(request);
        std::string path   = getPath(request);
        std::string body   = parseBodyFromRequest(request);
        std::string response;

        try {
            if (method == "OPTIONS") {
                response = buildHttpResponse(200, "");
            }
            else if (method == "GET" && path == "/candidates") {
                auto cands = controller.getAllCandidates();
                response = buildHttpResponse(200, cands.dump());
            }
            else if (method == "GET" && path == "/getVotes") {
                auto votes = controller.getAllVotes();
                json result;
                result["success"] = true;
                result["votes"] = votes;
                response = buildHttpResponse(200, result.dump());
            }
            else if (method == "POST" && path == "/checkVoter") {
                if (body.empty()) {
                    json e; e["success"]=false; e["message"]="Request body cannot be empty";
                    response = buildHttpResponse(400, e.dump());
                } else {
                    auto rb = json::parse(body);
                    auto r = controller.checkVoterRegistration(rb.value("voter_id",""));
                    response = buildHttpResponse(r["success"].get<bool>() ? 200:400, r.dump());
                }
            }
            else if (method == "POST" && path == "/vote") {
                if (body.empty()) {
                    json e; e["success"]=false; e["message"]="Request body cannot be empty";
                    response = buildHttpResponse(400, e.dump());
                } else {
                    auto rb = json::parse(body);
                    auto r = controller.castVote(rb.value("voter_id",""), rb.value("candidate_name",""));
                    response = buildHttpResponse(r["success"].get<bool>() ? 200:400, r.dump());
                }
            }
            else if (method == "POST" && path == "/addCandidate") {
                if (body.empty()) {
                    json e; e["success"]=false; e["message"]="Request body cannot be empty";
                    response = buildHttpResponse(400, e.dump());
                } else {
                    auto rb = json::parse(body);
                    auto r = controller.addCandidate(rb.value("candidate_name",""), rb.value("admin_password",""));
                    response = buildHttpResponse(r["success"].get<bool>() ? 200:400, r.dump());
                }
            }
            else if (method == "POST" && path == "/syncCandidates") {
                if (body.empty()) {
                    json e; e["success"]=false; e["message"]="Request body cannot be empty";
                    response = buildHttpResponse(400, e.dump());
                } else {
                    auto rb = json::parse(body);
                    auto r = controller.syncCandidates(rb.value("candidates", json::array()), rb.value("admin_password",""));
                    response = buildHttpResponse(r["success"].get<bool>() ? 200:400, r.dump());
                }
            }
            else if (method == "POST" && path == "/syncVoters") {
                if (body.empty()) {
                    json e; e["success"]=false; e["message"]="Request body cannot be empty";
                    response = buildHttpResponse(400, e.dump());
                } else {
                    auto rb = json::parse(body);
                    auto r = controller.syncVoters(rb.value("voters", json::array()), rb.value("admin_password",""));
                    response = buildHttpResponse(r["success"].get<bool>() ? 200:400, r.dump());
                }
            }
            else if (method == "POST" && path == "/deleteCandidate") {
                if (body.empty()) {
                    json e; e["success"]=false; e["message"]="Request body cannot be empty";
                    response = buildHttpResponse(400, e.dump());
                } else {
                    auto rb = json::parse(body);
                    auto r = controller.deleteCandidate(rb.value("candidate_name",""), rb.value("admin_password",""));
                    response = buildHttpResponse(r["success"].get<bool>() ? 200:400, r.dump());
                }
            }
            else if (method == "POST" && path == "/deleteVoter") {
                if (body.empty()) {
                    json e; e["success"]=false; e["message"]="Request body cannot be empty";
                    response = buildHttpResponse(400, e.dump());
                } else {
                    auto rb = json::parse(body);
                    auto r = controller.deleteVoter(rb.value("voter_id",""), rb.value("admin_password",""));
                    response = buildHttpResponse(r["success"].get<bool>() ? 200:400, r.dump());
                }
            }
            else {
                json e; e["error"]="Not found";
                response = buildHttpResponse(404, e.dump());
            }
        } catch (const std::exception& e) {
            json err; err["success"]=false; err["message"]="Invalid request";
            response = buildHttpResponse(400, err.dump());
        }

        write(clientFd, response.c_str(), response.length());
        close(clientFd);
    }

public:
    HttpServer(int p = 8080) : listenFd(-1), port(p) {}

    bool start() {
        signal(SIGPIPE, SIG_IGN);

        listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) { std::cerr << "Failed to create socket" << std::endl; return false; }

        int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in serverAddr{};
        serverAddr.sin_family      = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port        = htons(port);

        if (bind(listenFd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Bind failed on port " << port << std::endl;
            close(listenFd); return false;
        }
        if (listen(listenFd, SOMAXCONN) < 0) {
            std::cerr << "Listen failed" << std::endl;
            close(listenFd); return false;
        }

        std::cout << "=====================================" << std::endl;
        std::cout << "Voting System - Supabase Edition" << std::endl;
        std::cout << "Server running on port " << port << std::endl;
        std::cout << "Database: Supabase PostgreSQL" << std::endl;
        std::cout << "=====================================" << std::endl;

        while (true) {
            int clientFd = accept(listenFd, nullptr, nullptr);
            if (clientFd < 0) { continue; }
            handleClient(clientFd);
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
    const char* envPort = std::getenv("PORT");
    if (envPort != nullptr) port = std::atoi(envPort);

    HttpServer server(port);
    server.start();
    return 0;
}

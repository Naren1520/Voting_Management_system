/**
 * voting_server_linux.cpp
 * Linux-compatible C++ HTTP server (POSIX sockets, no Winsock)
 * Compile: g++ -std=c++17 -o voting_server voting_server_linux.cpp -pthread
 */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>

// POSIX socket headers (Linux/Unix)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#include "json.hpp"

using json = nlohmann::json;

// ============================================================================
// MODELS
// ============================================================================

class Candidate {
public:
    std::string name;
    int votes;

    Candidate(const std::string& n = "", int v = 0) : name(n), votes(v) {}

    json toJson() const {
        json j;
        j["name"] = name;
        j["votes"] = votes;
        return j;
    }
};

class Voter {
public:
    std::string voterId;
    std::string candidateName;

    Voter(const std::string& id = "", const std::string& cand = "")
        : voterId(id), candidateName(cand) {}

    json toJson() const {
        json j;
        j["voter_id"] = voterId;
        j["candidate_name"] = candidateName;
        return j;
    }
};

class RegisteredVoter {
public:
    std::string id;
    std::string name;
    std::string email;
    std::string phone;

    RegisteredVoter(const std::string& vid = "", const std::string& vname = "",
                    const std::string& vemail = "", const std::string& vphone = "")
        : id(vid), name(vname), email(vemail), phone(vphone) {}

    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["email"] = email;
        j["phone"] = phone;
        return j;
    }
};

// ============================================================================
// VOTING CONTROLLER
// ============================================================================

class VotingController {
private:
    std::vector<Candidate> candidates;
    std::vector<Voter> voters;
    std::vector<RegisteredVoter> registeredVoters;

    // Linux uses forward slashes
    const std::string CANDIDATES_FILE = "data/candidates.json";
    const std::string VOTERS_FILE = "data/voters.json";
    const std::string REGISTERED_VOTERS_FILE = "data/registered_voters.json";
    const std::string ADMIN_PASSWORD = "admin123";

public:
    VotingController() {
        loadCandidates();
        loadVoters();
        loadRegisteredVoters();
    }

    void loadCandidates() {
        std::ifstream file(CANDIDATES_FILE);
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                for (const auto& item : j) {
                    candidates.push_back(Candidate(item["name"], item["votes"]));
                }
            } catch (...) {}
            file.close();
        }
    }

    void loadVoters() {
        std::ifstream file(VOTERS_FILE);
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                for (const auto& item : j) {
                    voters.push_back(Voter(item["voter_id"], item["candidate_name"]));
                }
            } catch (...) {}
            file.close();
        }
    }

    void loadRegisteredVoters() {
        std::ifstream file(REGISTERED_VOTERS_FILE);
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                for (const auto& item : j) {
                    registeredVoters.push_back(RegisteredVoter(
                        item["id"],
                        item["name"],
                        item["email"],
                        item.value("phone", "")
                    ));
                }
            } catch (...) {}
            file.close();
        }
    }

    void saveCandidates() {
        std::ofstream file(CANDIDATES_FILE);
        json j = json::array();
        for (const auto& c : candidates) j.push_back(c.toJson());
        file << j.dump(4);
        file.close();
    }

    void saveVoters() {
        std::ofstream file(VOTERS_FILE);
        json j = json::array();
        for (const auto& v : voters) j.push_back(v.toJson());
        file << j.dump(4);
        file.close();
    }

    void saveRegisteredVoters() {
        std::ofstream file(REGISTERED_VOTERS_FILE);
        json j = json::array();
        for (const auto& v : registeredVoters) {
            json voter;
            voter["id"] = v.id;
            voter["name"] = v.name;
            voter["email"] = v.email;
            voter["phone"] = v.phone;
            j.push_back(voter);
        }
        file << j.dump(4);
        file.close();
    }

    json getAllCandidates() {
        json j = json::array();
        for (const auto& c : candidates) j.push_back(c.toJson());
        return j;
    }

    json getAllVotes() {
        json j = json::array();
        for (const auto& v : voters) j.push_back(v.toJson());
        return j;
    }

    bool isVoterRegistered(const std::string& voterId) {
        return std::any_of(registeredVoters.begin(), registeredVoters.end(),
            [&voterId](const RegisteredVoter& v) { return v.id == voterId; });
    }

    json checkVoterRegistration(const std::string& voterId) {
        json response;
        if (voterId.empty()) {
            response["success"] = false;
            response["message"] = "Voter ID cannot be empty";
            return response;
        }
        if (isVoterRegistered(voterId)) {
            response["success"] = true;
            response["message"] = "Voter is registered and can vote";
            response["registered"] = true;
        } else {
            response["success"] = false;
            response["message"] = "Voter ID not found in registered voters list";
            response["registered"] = false;
        }
        return response;
    }

    json castVote(const std::string& voterId, const std::string& candidateName) {
        json response;

        if (voterId.empty()) {
            response["success"] = false;
            response["message"] = "Voter ID cannot be empty";
            return response;
        }
        if (!isVoterRegistered(voterId)) {
            response["success"] = false;
            response["message"] = "Voter ID not registered. Please register before voting.";
            response["registered"] = false;
            return response;
        }
        if (candidateName.empty()) {
            response["success"] = false;
            response["message"] = "Candidate name cannot be empty";
            return response;
        }
        if (std::any_of(voters.begin(), voters.end(),
            [&voterId](const Voter& v) { return v.voterId == voterId; })) {
            response["success"] = false;
            response["message"] = "This voter ID has already voted";
            return response;
        }

        auto it = std::find_if(candidates.begin(), candidates.end(),
            [&candidateName](const Candidate& c) { return c.name == candidateName; });

        if (it == candidates.end()) {
            response["success"] = false;
            response["message"] = "Candidate not found: " + candidateName;
            return response;
        }

        it->votes++;
        voters.push_back(Voter(voterId, candidateName));
        saveCandidates();
        saveVoters();

        response["success"] = true;
        response["message"] = "Vote recorded successfully for " + candidateName;
        response["candidates"] = getAllCandidates();
        return response;
    }

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
        if (std::any_of(candidates.begin(), candidates.end(),
            [&candidateName](const Candidate& c) { return c.name == candidateName; })) {
            response["success"] = false;
            response["message"] = "Candidate already exists: " + candidateName;
            return response;
        }
        candidates.push_back(Candidate(candidateName, 0));
        saveCandidates();
        response["success"] = true;
        response["message"] = "Candidate added successfully: " + candidateName;
        response["candidates"] = getAllCandidates();
        return response;
    }

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
        auto it = std::find_if(candidates.begin(), candidates.end(),
            [&candidateName](const Candidate& c) { return c.name == candidateName; });
        if (it == candidates.end()) {
            response["success"] = false;
            response["message"] = "Candidate not found: " + candidateName;
            return response;
        }
        candidates.erase(it);
        saveCandidates();
        response["success"] = true;
        response["message"] = "Candidate deleted successfully: " + candidateName;
        response["candidates"] = getAllCandidates();
        return response;
    }

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
        auto it = std::find_if(voters.begin(), voters.end(),
            [&voterId](const Voter& v) { return v.voterId == voterId; });
        if (it == voters.end()) {
            response["success"] = false;
            response["message"] = "Voter not found: " + voterId;
            return response;
        }
        voters.erase(it);
        saveVoters();
        response["success"] = true;
        response["message"] = "Voter deleted successfully: " + voterId;
        return response;
    }

    json syncCandidates(const json& candidateList, const std::string& password) {
        json response;
        if (password != ADMIN_PASSWORD) {
            response["success"] = false;
            response["message"] = "Invalid admin password";
            return response;
        }
        candidates.clear();
        for (const auto& item : candidateList) {
            candidates.push_back(Candidate(item["name"], item.value("votes", 0)));
        }
        saveCandidates();
        response["success"] = true;
        response["message"] = "Candidates synced successfully";
        response["candidates"] = getAllCandidates();
        return response;
    }

    json syncVoters(const json& voterList, const std::string& password) {
        json response;
        if (password != ADMIN_PASSWORD) {
            response["success"] = false;
            response["message"] = "Invalid admin password";
            return response;
        }
        registeredVoters.clear();
        for (const auto& item : voterList) {
            registeredVoters.push_back(RegisteredVoter(
                item["id"],
                item["name"],
                item["email"],
                item.value("phone", "")
            ));
        }
        saveRegisteredVoters();
        response["success"] = true;
        response["message"] = "Voters synced successfully";
        return response;
    }
};

// ============================================================================
// HTTP SERVER (POSIX)
// ============================================================================

class HttpServer {
private:
    VotingController controller;
    int listenFd;
    int port;

    std::string parseBodyFromRequest(const std::string& request) {
        size_t pos = request.find("\r\n\r\n");
        if (pos != std::string::npos) {
            return request.substr(pos + 4);
        }
        return "";
    }

    std::string getPath(const std::string& request) {
        size_t start = request.find(' ');
        size_t end = request.find(' ', start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            std::string fullPath = request.substr(start + 1, end - start - 1);
            // Strip query string if any
            size_t qPos = fullPath.find('?');
            if (qPos != std::string::npos) fullPath = fullPath.substr(0, qPos);
            return fullPath;
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
        response += "Connection: close\r\n";
        response += "\r\n";
        response += body;
        return response;
    }

    void handleClient(int clientFd) {
        // Read the full request (handle chunked reads)
        std::string request;
        char buffer[4096];
        ssize_t bytesRead;

        while ((bytesRead = read(clientFd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            request += buffer;
            // Stop reading once we have the full headers + body
            size_t headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                // Check Content-Length to know if we have the full body
                size_t clPos = request.find("Content-Length: ");
                if (clPos != std::string::npos) {
                    size_t clEnd = request.find("\r\n", clPos);
                    int contentLength = std::stoi(request.substr(clPos + 16, clEnd - clPos - 16));
                    int bodyReceived = (int)request.size() - (int)(headerEnd + 4);
                    if (bodyReceived >= contentLength) break;
                } else {
                    break; // No body expected (GET / OPTIONS)
                }
            }
        }

        if (request.empty()) {
            close(clientFd);
            return;
        }

        std::string method = getMethod(request);
        std::string path = getPath(request);
        std::string body = parseBodyFromRequest(request);

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
                    json error; error["success"] = false; error["message"] = "Request body cannot be empty";
                    response = buildHttpResponse(400, error.dump());
                } else {
                    auto reqBody = json::parse(body);
                    auto result = controller.checkVoterRegistration(reqBody.value("voter_id", ""));
                    response = buildHttpResponse(result["success"].get<bool>() ? 200 : 400, result.dump());
                }
            }
            else if (method == "POST" && path == "/vote") {
                if (body.empty()) {
                    json error; error["success"] = false; error["message"] = "Request body cannot be empty";
                    response = buildHttpResponse(400, error.dump());
                } else {
                    auto reqBody = json::parse(body);
                    auto result = controller.castVote(
                        reqBody.value("voter_id", ""),
                        reqBody.value("candidate_name", "")
                    );
                    response = buildHttpResponse(result["success"].get<bool>() ? 200 : 400, result.dump());
                }
            }
            else if (method == "POST" && path == "/addCandidate") {
                if (body.empty()) {
                    json error; error["success"] = false; error["message"] = "Request body cannot be empty";
                    response = buildHttpResponse(400, error.dump());
                } else {
                    auto reqBody = json::parse(body);
                    auto result = controller.addCandidate(
                        reqBody.value("candidate_name", ""),
                        reqBody.value("admin_password", "")
                    );
                    response = buildHttpResponse(result["success"].get<bool>() ? 200 : 400, result.dump());
                }
            }
            else if (method == "POST" && path == "/syncCandidates") {
                if (body.empty()) {
                    json error; error["success"] = false; error["message"] = "Request body cannot be empty";
                    response = buildHttpResponse(400, error.dump());
                } else {
                    auto reqBody = json::parse(body);
                    auto result = controller.syncCandidates(
                        reqBody.value("candidates", json::array()),
                        reqBody.value("admin_password", "")
                    );
                    response = buildHttpResponse(result["success"].get<bool>() ? 200 : 400, result.dump());
                }
            }
            else if (method == "POST" && path == "/syncVoters") {
                if (body.empty()) {
                    json error; error["success"] = false; error["message"] = "Request body cannot be empty";
                    response = buildHttpResponse(400, error.dump());
                } else {
                    auto reqBody = json::parse(body);
                    auto result = controller.syncVoters(
                        reqBody.value("voters", json::array()),
                        reqBody.value("admin_password", "")
                    );
                    response = buildHttpResponse(result["success"].get<bool>() ? 200 : 400, result.dump());
                }
            }
            else if (method == "POST" && path == "/deleteCandidate") {
                if (body.empty()) {
                    json error; error["success"] = false; error["message"] = "Request body cannot be empty";
                    response = buildHttpResponse(400, error.dump());
                } else {
                    auto reqBody = json::parse(body);
                    auto result = controller.deleteCandidate(
                        reqBody.value("candidate_name", ""),
                        reqBody.value("admin_password", "")
                    );
                    response = buildHttpResponse(result["success"].get<bool>() ? 200 : 400, result.dump());
                }
            }
            else if (method == "POST" && path == "/deleteVoter") {
                if (body.empty()) {
                    json error; error["success"] = false; error["message"] = "Request body cannot be empty";
                    response = buildHttpResponse(400, error.dump());
                } else {
                    auto reqBody = json::parse(body);
                    auto result = controller.deleteVoter(
                        reqBody.value("voter_id", ""),
                        reqBody.value("admin_password", "")
                    );
                    response = buildHttpResponse(result["success"].get<bool>() ? 200 : 400, result.dump());
                }
            }
            else {
                json error; error["error"] = "Not found";
                response = buildHttpResponse(404, error.dump());
            }
        } catch (const std::exception& e) {
            json error; error["success"] = false; error["message"] = "Invalid request";
            response = buildHttpResponse(400, error.dump());
        }

        write(clientFd, response.c_str(), response.length());
        close(clientFd);
    }

public:
    HttpServer(int p = 8080) : listenFd(-1), port(p) {}

    bool start() {
        // Ignore SIGPIPE (client disconnects)
        signal(SIGPIPE, SIG_IGN);

        listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // Allow port reuse (prevents "Address already in use" on restart)
        int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
        serverAddr.sin_port = htons(port);

        if (bind(listenFd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Bind failed on port " << port << std::endl;
            close(listenFd);
            return false;
        }

        if (listen(listenFd, SOMAXCONN) < 0) {
            std::cerr << "Listen failed" << std::endl;
            close(listenFd);
            return false;
        }

        std::cout << "=====================================" << std::endl;
        std::cout << "Voting System REST API Server" << std::endl;
        std::cout << "=====================================" << std::endl;
        std::cout << "Server running on port " << port << std::endl;
        std::cout << "Endpoints:" << std::endl;
        std::cout << "  GET  /candidates" << std::endl;
        std::cout << "  POST /checkVoter" << std::endl;
        std::cout << "  POST /vote" << std::endl;
        std::cout << "  POST /addCandidate" << std::endl;
        std::cout << "  POST /syncCandidates" << std::endl;
        std::cout << "  POST /syncVoters" << std::endl;
        std::cout << "  POST /deleteCandidate" << std::endl;
        std::cout << "  POST /deleteVoter" << std::endl;
        std::cout << "=====================================" << std::endl;

        while (true) {
            int clientFd = accept(listenFd, nullptr, nullptr);
            if (clientFd < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
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
    // Railway injects PORT env variable — use it if available, else default 8080
    int port = 8080;
    const char* envPort = std::getenv("PORT");
    if (envPort != nullptr) {
        port = std::atoi(envPort);
    }

    HttpServer server(port);
    server.start();
    return 0;
}

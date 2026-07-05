#include "../../include/net/EpollServer.h"
#include "../../include/net/HttpRequest.h"
#include "../../include/net/HttpResponse.h"
#include "../../include/core/ThreadPool.h"
#include "../../include/core/Logger.h"
#include "../../include/core/Config.h"
#include "../../include/core/Metrics.h"      // Step 11: Prometheus metrics
#include "../../include/db/SupabaseClient.h"
#include "../../include/cache/RedisClient.h"
#include "../../include/controllers/AuthController.h"
#include "../../include/controllers/ElectionController.h"
#include "../../include/controllers/CandidateController.h"
#include "../../include/controllers/VoterController.h"
#include "../../include/controllers/PositionController.h"
#include "../../include/controllers/PublicVoteController.h"
#include "../../include/controllers/PublicMultiVoteController.h"
#include "../../third_party/json.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Controller instances (stateless — safe to share across threads)
// ─────────────────────────────────────────────────────────────────────────────
static AuthController            g_auth;
static ElectionController        g_election;
static CandidateController       g_candidate;
static VoterController           g_voter;
static PositionController        g_position;
static PublicVoteController      g_vote;
static PublicMultiVoteController g_multiVote;

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

EpollServer::EpollServer(int port) : port_(port) {}

EpollServer::~EpollServer() {
    // Stop background cleanup thread
    cleanupStop_.store(true);
    if (cleanupThread_.joinable()) cleanupThread_.join();
    if (listenFd_ >= 0) ::close(listenFd_);
    if (epollFd_  >= 0) ::close(epollFd_);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

int EpollServer::makeNonBlocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool EpollServer::setSocketOptions(int fd) {
    int opt = 1;
    // SO_REUSEADDR: allow immediate rebind after crash/restart
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        return false;
    // SO_REUSEPORT: allows multiple server instances to bind the same port
    // (needed for Phase 4 horizontal scaling). Fix #8.
#ifdef SO_REUSEPORT
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
    return true;
}

std::vector<std::string> EpollServer::splitPath(const std::string& path) {
    std::vector<std::string> segs;
    std::stringstream ss(path);
    std::string seg;
    while (std::getline(ss, seg, '/'))
        if (!seg.empty()) segs.push_back(seg);
    return segs;
}

// ─────────────────────────────────────────────────────────────────────────────
// cleanupLoop / startCleanupTimer
// Fix #1: runs every 10 minutes to DELETE expired sessions from Supabase.
// Keeps the sessions table small without polluting the auth hot path.
// ─────────────────────────────────────────────────────────────────────────────

void EpollServer::cleanupLoop() {
    constexpr int INTERVAL_SEC = 600;  // 10 minutes
    int elapsed = 0;
    while (!cleanupStop_.load()) {
        ::sleep(1);
        if (cleanupStop_.load()) break;
        ++elapsed;
        if (elapsed >= INTERVAL_SEC) {
            elapsed = 0;
            try {
                auto r = supabaseRequest("DELETE",
                    "sessions?expires_at=lt." +
                    SupabaseClient::currentTimestamp());
                LOG_INFO("[Cleanup] Expired session sweep done (HTTP " +
                         std::to_string(r.statusCode) + ")");
            } catch (...) {
                LOG_WARN("[Cleanup] Expired session sweep failed");
            }
        }
    }
}

void EpollServer::startCleanupTimer() {
    cleanupThread_ = std::thread([this] { cleanupLoop(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// start — epoll loop
// ─────────────────────────────────────────────────────────────────────────────

bool EpollServer::start() {
    ::signal(SIGPIPE, SIG_IGN);

    std::cout << "[EpollServer] start() called, port=" << port_ << "\n"; std::cout.flush();

    // Listen socket
    listenFd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listenFd_ < 0) { LOG_ERROR("socket() failed: " + std::string(strerror(errno))); return false; }
    std::cout << "[EpollServer] socket() ok\n"; std::cout.flush();
    setSocketOptions(listenFd_);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port_);

    if (::bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("bind() failed: " + std::string(strerror(errno)));
        return false;
    }
    std::cout << "[EpollServer] bind() ok\n"; std::cout.flush();

    if (::listen(listenFd_, SOMAXCONN) < 0) {
        LOG_ERROR("listen() failed: " + std::string(strerror(errno)));
        return false;
    }
    std::cout << "[EpollServer] listen() ok\n"; std::cout.flush();

    // epoll
    epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) { LOG_ERROR("epoll_create1() failed"); return false; }
    std::cout << "[EpollServer] epoll_create1() ok\n"; std::cout.flush();

    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLET;
    ev.data.fd = listenFd_;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenFd_, &ev) < 0) {
        LOG_ERROR("epoll_ctl ADD listen failed");
        return false;
    }

    // Thread pool — handler set in constructor (Fix #3).
    // Pool size also passed to SupabaseClient so curl handles match workers.
    size_t numThreads = std::thread::hardware_concurrency() * 2;
    if (numThreads < 4) numThreads = 4;
    SupabaseClient::instance().init(static_cast<int>(numThreads));
    pool_ = std::make_unique<ThreadPool>(
        numThreads,
        [this](int fd) { handleClient(fd); }
        /* maxQueue defaults to numThreads*8 */
    );

    LOG_INFO("====================================");
    LOG_INFO("VoteStack API Server (Phase 2)");
    LOG_INFO("Port: "    + std::to_string(port_));
    LOG_INFO("Workers: " + std::to_string(numThreads));
    LOG_INFO("I/O: epoll edge-triggered");
    LOG_INFO("Cache: Redis session + candidate");
    LOG_INFO("Rate-limit: Redis INCR/EXPIRE");
    LOG_INFO("Cleanup: expired sessions every 10 min");
    LOG_INFO("====================================");

    // Fix #1: start background session cleanup timer
    startCleanupTimer();

    constexpr int MAX_EVENTS = 1024;
    epoll_event events[MAX_EVENTS];

    while (true) {
        int nfds = ::epoll_wait(epollFd_, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("epoll_wait error: " + std::string(strerror(errno)));
            break;
        }
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == listenFd_) {
                // Accept all pending connections (edge-triggered)
                while (true) {
                    int clientFd = ::accept4(listenFd_, nullptr, nullptr, SOCK_NONBLOCK);
                    if (clientFd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        LOG_WARN("accept4 error: " + std::string(strerror(errno)));
                        break;
                    }
                    // Fix #2: if queue is full, reject immediately with 503
                    // rather than letting the fd queue unboundedly in memory.
                    if (!pool_->enqueue(clientFd)) {
                        const char* busy =
                            "HTTP/1.1 503 Service Unavailable\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: 41\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "{\"success\":false,\"message\":\"Server busy\"}";
                        ::send(clientFd, busy, strlen(busy), MSG_NOSIGNAL);
                        ::close(clientFd);
                        LOG_WARN("Queue full — rejected connection with 503");
                    }
                }
            }
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// handleClient — called by thread-pool workers
// ─────────────────────────────────────────────────────────────────────────────

void EpollServer::handleClient(int fd) {
    LatencyTimer timer;   // Step 11: starts clock + increments active_connections

    HttpRequest req;
    if (!req.parse(fd)) {
        timer.finish(400);
        ::close(fd);
        return;
    }

    std::string response = route(req);

    // Extract HTTP status code from the first line of the response so we can
    // record it in metrics (e.g. "HTTP/1.1 200 OK" → 200).
    int statusCode = 200;
    if (response.size() > 12) {
        const char* sp = response.c_str();
        // "HTTP/1.1 XXX"
        if (response[8] == ' ') {
            statusCode = std::atoi(sp + 9);
        }
    }
    timer.finish(statusCode);

    if (!response.empty()) {
        // Write in a loop to handle partial sends
        const char* ptr = response.c_str();
        ssize_t remaining = (ssize_t)response.size();
        while (remaining > 0) {
            ssize_t n = ::send(fd, ptr, (size_t)remaining, MSG_NOSIGNAL);
            if (n <= 0) break;
            ptr       += n;
            remaining -= n;
        }
    }
    ::close(fd);
}

// ─────────────────────────────────────────────────────────────────────────────
// route — full request router (all original routes preserved)
// ─────────────────────────────────────────────────────────────────────────────

std::string EpollServer::route(const HttpRequest& req) {
    const std::string& method = req.method;
    const std::string& path   = req.path;
    const std::string& body   = req.body;
    std::string token         = req.getToken();
    // Origin extracted once — passed to all HttpResponse builders for validated CORS
    const std::string  origin = req.getHeader("origin");
    auto segs = splitPath(path);

    try {
        // ── Health check (always pass — before rate limiting) ─────────────────
        if (path == "/health" && method == "GET") {
            json h; h["status"] = "ok";
            return HttpResponse::build(200, h.dump());
        }

        // ── Prometheus metrics endpoint (Step 11) ─────────────────────────────
        // Scraped by Prometheus every 10 s (see prometheus.yml).
        // nginx blocks this path from the public internet (not in location /api/).
        if (path == "/metrics" && method == "GET") {
            std::string body = Metrics::instance().renderPrometheus();
            return std::string("HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                               "Content-Length: ") + std::to_string(body.size()) +
                               "\r\nConnection: close\r\n\r\n" + body;
        }

        // ── CORS preflight ────────────────────────────────────────────────────
        if (method == "OPTIONS") {
            return HttpResponse::buildOptions(origin);
        }

        // ── Rate limiting (skip OPTIONS and /health) ──────────────────────────
        {
            std::string ip = req.getClientIP();
            auto& redis = RedisClient::instance();
            const Config& cfg = Config::instance();
            if (redis.isAvailable() && !ip.empty()) {
                std::string rlKey = "rl:" + ip;
                if (!redis.checkRateLimit(rlKey, cfg.rateLimitRequests(),
                                          cfg.rateLimitWindowSec())) {
                    json r;
                    r["success"] = false;
                    r["message"] = "Too many requests. Please slow down.";
                    return HttpResponse::build(429, r.dump(), origin);
                }
            }
        }

        // ── AUTH ─────────────────────────────────────────────────────────

        if (path == "/api/auth/signup" && method == "POST") {
            auto rb = json::parse(body);
            auto r = g_auth.signup(rb.value("name",""), rb.value("email",""),
                                   rb.value("password",""),
                                   req.getUserAgent(), req.getClientIP());
            return HttpResponse::build(r["success"].get<bool>() ? 201 : 400, r.dump());
        }

        if (path == "/api/auth/login" && method == "POST") {
            auto rb = json::parse(body);
            auto r = g_auth.login(rb.value("email",""), rb.value("password",""),
                                  req.getUserAgent(), req.getClientIP());
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 401, r.dump());
        }

        if (path == "/api/auth/logout" && method == "POST") {
            auto r = g_auth.logout(token);
            return HttpResponse::build(200, r.dump());
        }

        if (path == "/api/auth/ping" && method == "GET") {
            auto r = g_auth.ping(token);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 401, r.dump());
        }

        if (path == "/api/auth/change-password" && method == "POST") {
            auto rb = json::parse(body);
            auto r = g_auth.changePassword(token,
                rb.value("current_password",""), rb.value("new_password",""));
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 401, r.dump());
        }

        if (path == "/api/auth/sessions" && method == "GET") {
            auto r = g_auth.getSessions(token);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 401, r.dump());
        }

        if (path == "/api/auth/sessions" && method == "DELETE") {
            auto r = g_auth.revokeAllOtherSessions(token);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 401, r.dump());
        }

        // DELETE /api/auth/sessions/:session_id
        if (segs.size() == 4 && segs[0]=="api" && segs[1]=="auth" &&
            segs[2]=="sessions" && method == "DELETE") {
            auto r = g_auth.revokeSession(token, segs[3]);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 401, r.dump());
        }

        // ── ELECTIONS ────────────────────────────────────────────────────

        if (path == "/api/elections" && method == "GET") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            auto r = g_election.getElections(uid);
            return HttpResponse::build(200, r.dump());
        }

        if (path == "/api/elections" && method == "POST") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            auto rb = json::parse(body);
            auto r = g_election.createElection(uid,
                rb.value("title",""),
                rb.value("election_type","standard"),
                rb.value("schedule_type","always_on"),
                rb.value("starts_at",""),
                rb.value("ends_at",""),
                rb.contains("schedule_json") ? rb["schedule_json"].dump() : "",
                rb.value("timezone","UTC"));
            return HttpResponse::build(r["success"].get<bool>() ? 201 : 400, r.dump());
        }

        // PATCH /api/elections/:id/schedule
        if (segs.size()==4 && segs[1]=="elections" && segs[3]=="schedule" && method=="PATCH") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            auto rb = json::parse(body);
            auto r = g_election.updateSchedule(uid, segs[2], rb);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
        }

        // GET/DELETE /api/elections/:id
        if (segs.size()==3 && segs[0]=="api" && segs[1]=="elections") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            if (method=="GET") {
                auto r = g_election.getElection(uid, segs[2]);
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 404, r.dump());
            }
            if (method=="DELETE") {
                auto r = g_election.deleteElection(uid, segs[2]);
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 404, r.dump());
            }
        }

        // /api/elections/:id/candidates
        if (segs.size()==4 && segs[1]=="elections" && segs[3]=="candidates") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            if (method=="GET") {
                auto r = g_candidate.getCandidates(uid, segs[2]);
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 403, r.dump());
            }
            if (method=="POST") {
                auto rb = json::parse(body);
                auto r = g_candidate.addCandidate(uid, segs[2], rb.value("name",""));
                return HttpResponse::build(r["success"].get<bool>() ? 201 : 400, r.dump());
            }
            if (method=="DELETE") {
                auto rb = json::parse(body);
                auto r = g_candidate.deleteCandidate(uid, segs[2], rb.value("name",""));
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
            }
        }

        // /api/elections/:id/voters/sync
        if (segs.size()==5 && segs[1]=="elections" &&
            segs[3]=="voters" && segs[4]=="sync" && method=="POST") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            auto rb = json::parse(body);
            auto r = g_voter.syncVoters(uid, segs[2], rb.value("voters", json::array()));
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
        }

        // /api/elections/:id/voters
        if (segs.size()==4 && segs[1]=="elections" && segs[3]=="voters") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            if (method=="GET") {
                auto r = g_voter.getVoters(uid, segs[2]);
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 403, r.dump());
            }
            if (method=="POST") {
                auto rb = json::parse(body);
                auto r = g_voter.addVoter(uid, segs[2],
                    rb.value("voter_id",""), rb.value("name",""),
                    rb.value("email",""), rb.value("phone",""));
                return HttpResponse::build(r["success"].get<bool>() ? 201 : 400, r.dump());
            }
            if (method=="DELETE") {
                auto rb = json::parse(body);
                auto r = g_voter.deleteVoter(uid, segs[2], rb.value("voter_id",""));
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
            }
        }

        // GET /api/elections/:id/voted
        if (segs.size()==4 && segs[1]=="elections" && segs[3]=="voted" && method=="GET") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            auto own = supabaseRequest("GET",
                "elections?select=id&id=eq."+segs[2]+"&user_id=eq."+uid+"&limit=1");
            bool owns = false;
            try { auto a = json::parse(own.body); owns = a.is_array() && !a.empty(); } catch (...) {}
            if (!owns) return HttpResponse::buildError(403, "Unauthorized");
            auto r = supabaseRequest("GET",
                "votes_cast?select=voter_id&election_id=eq."+segs[2]);
            try {
                auto arr = json::parse(r.body);
                json res; res["success"] = true; res["voted_ids"] = json::array();
                if (arr.is_array()) {
                    for (auto& v : arr) res["voted_ids"].push_back(v["voter_id"]);
                }
                return HttpResponse::build(200, res.dump());
            } catch (...) {
                return HttpResponse::buildError(500, "Failed to load vote data");
            }
        }

        // ── POSITIONS ────────────────────────────────────────────────────

        // GET/POST /api/elections/:id/positions
        if (segs.size()==4 && segs[1]=="elections" && segs[3]=="positions") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            if (method=="GET") {
                auto r = g_position.getPositions(uid, segs[2]);
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 403, r.dump());
            }
            if (method=="POST") {
                auto rb = json::parse(body);
                auto r = g_position.addPosition(uid, segs[2], rb.value("title",""));
                return HttpResponse::build(r["success"].get<bool>() ? 201 : 400, r.dump());
            }
        }

        // DELETE /api/elections/:id/positions/:posId
        if (segs.size()==5 && segs[1]=="elections" && segs[3]=="positions" && method=="DELETE") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            auto r = g_position.deletePosition(uid, segs[2], segs[4]);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 403, r.dump());
        }

        // GET/POST/DELETE /api/elections/:id/positions/:posId/candidates
        if (segs.size()==6 && segs[1]=="elections" && segs[3]=="positions" && segs[5]=="candidates") {
            std::string uid = g_auth.validateToken(token);
            if (uid.empty()) return HttpResponse::buildError(401, "Unauthorized");
            if (method=="GET") {
                auto r = g_position.getCandidates(uid, segs[2], segs[4]);
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 403, r.dump());
            }
            if (method=="POST") {
                auto rb = json::parse(body);
                auto r = g_position.addCandidate(uid, segs[2], segs[4], rb.value("name",""));
                return HttpResponse::build(r["success"].get<bool>() ? 201 : 400, r.dump());
            }
            if (method=="DELETE") {
                auto rb = json::parse(body);
                auto r = g_position.deleteCandidate(uid, segs[2], segs[4], rb.value("name",""));
                return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
            }
        }

        // ── PUBLIC MULTI-VOTE ─────────────────────────────────────────────

        // GET /api/multi-vote/:id/positions
        if (segs.size()==4 && segs[1]=="multi-vote" && segs[3]=="positions" && method=="GET") {
            auto r = g_multiVote.getBallot(segs[2]);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 404, r.dump());
        }

        // POST /api/multi-vote/:id/check
        if (segs.size()==4 && segs[1]=="multi-vote" && segs[3]=="check" && method=="POST") {
            auto rb = json::parse(body);
            auto r = g_multiVote.checkVoter(segs[2], rb.value("voter_id",""));
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
        }

        // POST /api/multi-vote/:id/cast
        if (segs.size()==4 && segs[1]=="multi-vote" && segs[3]=="cast" && method=="POST") {
            auto rb = json::parse(body);
            auto r = g_multiVote.castVotes(segs[2],
                rb.value("voter_id",""), rb.value("votes", json::array()));
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
        }

        // GET /api/multi-vote/:id/results
        if (segs.size()==4 && segs[1]=="multi-vote" && segs[3]=="results" && method=="GET") {
            auto r = g_multiVote.getResults(segs[2]);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 404, r.dump());
        }

        // GET /api/multi-vote/:id/info
        if (segs.size()==4 && segs[1]=="multi-vote" && segs[3]=="info" && method=="GET") {
            auto r = supabaseRequest("GET",
                "elections?select=title,is_active,schedule_type,starts_at,ends_at,"
                "schedule_json,timezone&id=eq."+segs[2]+"&limit=1");
            try {
                auto arr = json::parse(r.body);
                if (arr.is_array() && !arr.empty()) {
                    json res; res["success"] = true;
                    res["title"]         = arr[0]["title"];
                    res["is_active"]     = arr[0]["is_active"];
                    res["schedule_type"] = arr[0].value("schedule_type","always_on");
                    res["timezone"]      = arr[0].value("timezone","UTC");
                    if (arr[0].contains("starts_at") && !arr[0]["starts_at"].is_null())
                        res["starts_at"] = arr[0]["starts_at"];
                    if (arr[0].contains("ends_at") && !arr[0]["ends_at"].is_null())
                        res["ends_at"] = arr[0]["ends_at"];
                    if (arr[0].contains("schedule_json") && !arr[0]["schedule_json"].is_null())
                        res["schedule_json"] = arr[0]["schedule_json"];
                    return HttpResponse::build(200, res.dump());
                }
            } catch (...) {}
            return HttpResponse::buildError(404, "Election not found");
        }

        // ── PUBLIC VOTE ───────────────────────────────────────────────────

        // GET /api/vote/:id/candidates
        if (segs.size()==4 && segs[1]=="vote" && segs[3]=="candidates") {
            auto r = g_vote.getCandidates(segs[2]);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 404, r.dump());
        }

        // POST /api/vote/:id/check
        if (segs.size()==4 && segs[1]=="vote" && segs[3]=="check" && method=="POST") {
            auto rb = json::parse(body);
            auto r = g_vote.checkVoter(segs[2], rb.value("voter_id",""));
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
        }

        // POST /api/vote/:id/cast
        if (segs.size()==4 && segs[1]=="vote" && segs[3]=="cast" && method=="POST") {
            auto rb = json::parse(body);
            auto r = g_vote.castVote(segs[2],
                rb.value("voter_id",""), rb.value("candidate_name",""));
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 400, r.dump());
        }

        // GET /api/vote/:id/results
        if (segs.size()==4 && segs[1]=="vote" && segs[3]=="results") {
            auto r = g_vote.getResults(segs[2]);
            return HttpResponse::build(r["success"].get<bool>() ? 200 : 404, r.dump());
        }

        // GET /api/vote/:id/info
        if (segs.size()==4 && segs[1]=="vote" && segs[3]=="info") {
            auto r = supabaseRequest("GET",
                "elections?select=title,is_active,schedule_type,starts_at,ends_at,"
                "schedule_json,timezone&id=eq."+segs[2]+"&limit=1");
            try {
                auto arr = json::parse(r.body);
                if (arr.is_array() && !arr.empty()) {
                    json res; res["success"] = true;
                    res["title"]         = arr[0]["title"];
                    res["is_active"]     = arr[0]["is_active"];
                    res["schedule_type"] = arr[0].value("schedule_type","always_on");
                    res["timezone"]      = arr[0].value("timezone","UTC");
                    if (arr[0].contains("starts_at") && !arr[0]["starts_at"].is_null())
                        res["starts_at"] = arr[0]["starts_at"];
                    if (arr[0].contains("ends_at") && !arr[0]["ends_at"].is_null())
                        res["ends_at"] = arr[0]["ends_at"];
                    if (arr[0].contains("schedule_json") && !arr[0]["schedule_json"].is_null())
                        res["schedule_json"] = arr[0]["schedule_json"];
                    return HttpResponse::build(200, res.dump());
                }
            } catch (...) {}
            return HttpResponse::buildError(404, "Election not found");
        }

        // ── 404 ───────────────────────────────────────────────────────────
        return HttpResponse::buildError(404, "Not found", origin);

    } catch (const std::exception& e) {
        LOG_ERROR("Route exception: " + std::string(e.what()));
        return HttpResponse::buildError(400, "Invalid request", origin);
    } catch (...) {
        return HttpResponse::buildError(500, "Internal server error", origin);
    }
}

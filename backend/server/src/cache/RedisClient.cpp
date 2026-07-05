#include "../../include/cache/RedisClient.h"
#include "../../include/core/Logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

RedisClient& RedisClient::instance() {
    static RedisClient inst;
    return inst;
}

// ─────────────────────────────────────────────────────────────────────────────
// init — parse REDIS_URL, open POOL_SIZE connections (called single-threaded)
// ─────────────────────────────────────────────────────────────────────────────

void RedisClient::init() {
    const char* envUrl = std::getenv("REDIS_URL");
    std::string url = envUrl ? std::string(envUrl) : "redis://127.0.0.1:6379";

    if (url.rfind("redis://", 0) == 0) url = url.substr(8);

    auto atPos = url.find('@');
    if (atPos != std::string::npos) {
        std::string auth = url.substr(0, atPos);
        url = url.substr(atPos + 1);
        auto colonPos = auth.find(':');
        password_ = (colonPos != std::string::npos)
                    ? auth.substr(colonPos + 1) : auth;
    }

    auto colonPos = url.rfind(':');
    if (colonPos != std::string::npos) {
        host_ = url.substr(0, colonPos);
        try { port_ = std::stoi(url.substr(colonPos + 1)); }
        catch (...) { port_ = 6379; }
    } else {
        host_ = url;
        port_ = 6379;
    }
    if (host_.empty()) host_ = "127.0.0.1";

    // Open all pool slots — single-threaded here so no mutex needed
    int opened = 0;
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (connectSlot(i)) ++opened;   // OK: single-threaded init
    }

    if (opened > 0) {
        available_.store(true);
        LOG_INFO("[Redis] Pool ready: " + std::to_string(opened) + "/" +
                 std::to_string(POOL_SIZE) + " connections to " +
                 host_ + ":" + std::to_string(port_));
    } else {
        available_.store(false);
        LOG_WARN("[Redis] Could not connect to " + host_ + ":" +
                 std::to_string(port_) + " — running without cache/rate-limiting");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// isAvailable — Fix #6: lock-free atomic read
// ─────────────────────────────────────────────────────────────────────────────

bool RedisClient::isAvailable() const {
    return available_.load();
}

// ─────────────────────────────────────────────────────────────────────────────
// connectSlot — Fix #5: MUST be called while poolMutex_ is held (or during
// single-threaded init).  Opens a new TCP connection for slot idx.
// ─────────────────────────────────────────────────────────────────────────────

bool RedisClient::connectSlot(int idx) {
    // Close any existing socket first (caller holds lock or is in init)
    if (conns_[idx].socket >= 0) {
        ::close(conns_[idx].socket);
        conns_[idx].socket = -1;
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    std::string portStr = std::to_string(port_);

    if (::getaddrinfo(host_.c_str(), portStr.c_str(), &hints, &res) != 0 || !res)
        return false;

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { ::freeaddrinfo(res); return false; }

    struct timeval tv; tv.tv_sec = 2; tv.tv_usec = 0;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        ::close(fd); ::freeaddrinfo(res); return false;
    }
    ::freeaddrinfo(res);
    conns_[idx].socket = fd;

    // Authenticate if password set (sendCommand safe here: slot not yet shared)
    if (!password_.empty()) {
        std::string authResp = sendCommand(idx, {"AUTH", password_});
        if (authResp.empty() || authResp[0] == '-') {
            LOG_WARN("[Redis] AUTH failed on slot " + std::to_string(idx));
            ::close(fd);
            conns_[idx].socket = -1;
            return false;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// disconnectSlot — Fix #5: caller must hold poolMutex_
// ─────────────────────────────────────────────────────────────────────────────

void RedisClient::disconnectSlot(int idx) {
    if (conns_[idx].socket >= 0) {
        ::close(conns_[idx].socket);
        conns_[idx].socket = -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// reconnectSlot — Fix #5: acquires poolMutex_ internally so two threads can
// never race to reconnect the same slot simultaneously.
// Fix #6: sets available_ atomically on recovery.
// ─────────────────────────────────────────────────────────────────────────────

bool RedisClient::reconnectSlot(int idx) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    // Double-check: another thread may have already reconnected this slot
    if (conns_[idx].socket >= 0) return true;

    if (connectSlot(idx)) {
        available_.store(true);  // Fix #6: atomic write
        LOG_INFO("[Redis] Reconnected slot " + std::to_string(idx));
        return true;
    }
    LOG_WARN("[Redis] Reconnect failed for slot " + std::to_string(idx));
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// acquireConn / releaseConn — condition variable, no busy-wait
// ─────────────────────────────────────────────────────────────────────────────

int RedisClient::acquireConn() {
    std::unique_lock<std::mutex> lock(poolMutex_);
    poolCv_.wait(lock, [this] {
        for (int i = 0; i < POOL_SIZE; ++i)
            if (!conns_[i].inUse) return true;
        return false;
    });
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (!conns_[i].inUse) {
            conns_[i].inUse = true;
            return i;
        }
    }
    return -1;
}

void RedisClient::releaseConn(int idx) {
    {
        std::lock_guard<std::mutex> lock(poolMutex_);
        conns_[idx].inUse = false;
    }
    poolCv_.notify_one();
}

// ─────────────────────────────────────────────────────────────────────────────
// RESP builder
// ─────────────────────────────────────────────────────────────────────────────

std::string RedisClient::buildCommand(const std::vector<std::string>& args) {
    std::ostringstream oss;
    oss << '*' << args.size() << "\r\n";
    for (const auto& a : args)
        oss << '$' << a.size() << "\r\n" << a << "\r\n";
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// sendAll
// ─────────────────────────────────────────────────────────────────────────────

bool RedisClient::sendAll(int idx, const std::string& data) {
    const char* ptr = data.c_str();
    ssize_t remaining = static_cast<ssize_t>(data.size());
    while (remaining > 0) {
        ssize_t n = ::send(conns_[idx].socket, ptr,
                           static_cast<size_t>(remaining), MSG_NOSIGNAL);
        if (n <= 0) {
            // Mark socket dead; reconnectSlot will re-open under its own lock
            conns_[idx].socket = -1;
            return false;
        }
        ptr       += n;
        remaining -= n;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// readLine — reads until \r\n into `line`, stripping the CRLF.
// Uses a per-slot 4KB read buffer to avoid one-byte-at-a-time recv() calls.
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int RBUF_SIZE = 4096;

// Per-slot read buffer state (kept inside Conn would be cleaner but requires
// header change; a thread_local avoids lock contention at the cost of one
// buffer per thread rather than per connection — acceptable since each
// connection is used by one thread at a time anyway).
struct SlotBuf {
    char  data[RBUF_SIZE];
    int   pos{0};
    int   len{0};
};

static thread_local SlotBuf t_buf;  // one buffer per worker thread

// Peek or refill the per-thread buffer.
static bool refillBuf(int sockfd) {
    ssize_t n = ::recv(sockfd, t_buf.data, RBUF_SIZE, 0);
    if (n <= 0) return false;
    t_buf.len = static_cast<int>(n);
    t_buf.pos = 0;
    return true;
}

static int readByte(int sockfd, char& out) {
    if (t_buf.pos >= t_buf.len) {
        if (!refillBuf(sockfd)) return -1;
    }
    out = t_buf.data[t_buf.pos++];
    return 0;
}

std::string RedisClient::readResponse(int idx) {
    int sockfd = conns_[idx].socket;
    // Reset buffer position for this response (new slot, new context)
    // Note: we don't zero t_buf here — leftover bytes from a previous
    // pipelined response are valid and should be consumed.

    // Read first line (type byte + length/value + CRLF)
    std::string firstLine;
    firstLine.reserve(32);
    while (true) {
        char c;
        if (readByte(sockfd, c) < 0) { conns_[idx].socket = -1; return ""; }
        firstLine += c;
        int sz = static_cast<int>(firstLine.size());
        if (sz >= 2 &&
            firstLine[sz-2] == '\r' && firstLine[sz-1] == '\n') break;
    }

    if (firstLine.empty()) return "";
    char type = firstLine[0];

    // Simple string (+), error (-), integer (:)
    if (type == '+' || type == '-' || type == ':') return firstLine;

    // Bulk string ($)
    if (type == '$') {
        int len = 0;
        try { len = std::stoi(firstLine.substr(1)); } catch (...) { return ""; }
        if (len < 0) return "$-1\r\n";

        // Read exactly len+2 bytes (data + CRLF)
        std::string data(static_cast<size_t>(len + 2), '\0');
        int totalRead = 0;
        while (totalRead < len + 2) {
            // Drain from buffer first
            int avail = t_buf.len - t_buf.pos;
            int need  = len + 2 - totalRead;
            int take  = std::min(avail, need);
            if (take > 0) {
                std::memcpy(&data[totalRead], t_buf.data + t_buf.pos, static_cast<size_t>(take));
                t_buf.pos += take;
                totalRead += take;
            }
            if (totalRead < len + 2) {
                if (!refillBuf(sockfd)) { conns_[idx].socket = -1; return ""; }
            }
        }
        return firstLine + data;
    }

    // Array (*)
    if (type == '*') {
        int count = 0;
        try { count = std::stoi(firstLine.substr(1)); } catch (...) { return ""; }
        if (count < 0) return "*-1\r\n";
        std::string result = firstLine;
        for (int i = 0; i < count; ++i) {
            std::string elem = readResponse(idx);
            if (elem.empty()) return "";
            result += elem;
        }
        return result;
    }
    return firstLine;
}

// ─────────────────────────────────────────────────────────────────────────────
// sendCommand — slot must be acquired (inUse=true) by the caller.
// If the socket is dead, calls reconnectSlot() which acquires the mutex.
// ─────────────────────────────────────────────────────────────────────────────

std::string RedisClient::sendCommand(int idx, const std::vector<std::string>& args) {
    if (conns_[idx].socket < 0) {
        if (!reconnectSlot(idx)) return "";
    }
    std::string cmd = buildCommand(args);
    if (!sendAll(idx, cmd)) {
        if (!reconnectSlot(idx)) return "";
        if (!sendAll(idx, cmd)) return "";
    }
    return readResponse(idx);
}

// ─────────────────────────────────────────────────────────────────────────────
// RESP parsers
// ─────────────────────────────────────────────────────────────────────────────

std::string RedisClient::parseSimpleString(const std::string& resp) {
    if (resp.size() >= 3 && resp[0] == '+')
        return resp.substr(1, resp.size() - 3);
    return "";
}

std::string RedisClient::parseBulkString(const std::string& resp) {
    if (resp.size() >= 4 && resp[0] == '$' && resp[1] == '-') return "";
    auto crlfPos = resp.find("\r\n");
    if (crlfPos == std::string::npos) return "";
    int len = std::stoi(resp.substr(1, crlfPos - 1));
    if (len <= 0) return "";
    if (static_cast<int>(resp.size()) < static_cast<int>(crlfPos + 2 + len)) return "";
    return resp.substr(crlfPos + 2, static_cast<size_t>(len));
}

long long RedisClient::parseInteger(const std::string& resp) {
    if (resp.empty() || resp[0] != ':') return -1;
    try { return std::stoll(resp.substr(1)); }
    catch (...) { return -1; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool RedisClient::set(const std::string& key, const std::string& value, int ttlSeconds) {
    if (!available_.load()) return false;
    int idx = acquireConn();
    if (idx < 0) return false;
    std::vector<std::string> args = {"SET", key, value};
    if (ttlSeconds > 0) { args.push_back("EX"); args.push_back(std::to_string(ttlSeconds)); }
    std::string resp = sendCommand(idx, args);
    releaseConn(idx);
    return (!resp.empty() && resp[0] == '+');
}

std::string RedisClient::get(const std::string& key) {
    if (!available_.load()) return "";
    int idx = acquireConn();
    if (idx < 0) return "";
    std::string resp = sendCommand(idx, {"GET", key});
    releaseConn(idx);
    if (resp.empty() || resp[0] == '-') return "";
    if (resp[0] == '$') return parseBulkString(resp);
    return "";
}

bool RedisClient::del(const std::string& key) {
    if (!available_.load()) return false;
    int idx = acquireConn();
    if (idx < 0) return false;
    std::string resp = sendCommand(idx, {"DEL", key});
    releaseConn(idx);
    return (!resp.empty() && resp[0] == ':' && parseInteger(resp) >= 0);
}

bool RedisClient::exists(const std::string& key) {
    if (!available_.load()) return false;
    int idx = acquireConn();
    if (idx < 0) return false;
    std::string resp = sendCommand(idx, {"EXISTS", key});
    releaseConn(idx);
    return (!resp.empty() && resp[0] == ':' && parseInteger(resp) > 0);
}

long long RedisClient::incr(const std::string& key) {
    if (!available_.load()) return -1;
    int idx = acquireConn();
    if (idx < 0) return -1;
    std::string resp = sendCommand(idx, {"INCR", key});
    releaseConn(idx);
    if (resp.empty() || resp[0] != ':') return -1;
    return parseInteger(resp);
}

bool RedisClient::expire(const std::string& key, int ttlSeconds) {
    if (!available_.load()) return false;
    int idx = acquireConn();
    if (idx < 0) return false;
    std::string resp = sendCommand(idx, {"EXPIRE", key, std::to_string(ttlSeconds)});
    releaseConn(idx);
    return (!resp.empty() && resp[0] == ':' && parseInteger(resp) == 1);
}

bool RedisClient::checkRateLimit(const std::string& key,
                                 int maxRequests, int windowSeconds) {
    if (!available_.load()) return true;
    int idx = acquireConn();
    if (idx < 0) return true;

    std::string incrResp = sendCommand(idx, {"INCR", key});
    if (incrResp.empty() || incrResp[0] != ':') { releaseConn(idx); return true; }

    long long count = parseInteger(incrResp);
    if (count < 0) { releaseConn(idx); return true; }
    if (count == 1) sendCommand(idx, {"EXPIRE", key, std::to_string(windowSeconds)});

    releaseConn(idx);
    return count <= static_cast<long long>(maxRequests);
}

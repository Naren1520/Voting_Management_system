#include "../../include/net/HttpRequest.h"
#include "../../include/core/Logger.h"

#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <algorithm>
#include <cctype>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────
// parse — reads the full HTTP request from a client fd.
//
// Fix #1: The client fd is accepted with SOCK_NONBLOCK from accept4(), but we
//   set SO_RCVTIMEO (5 s) so recv() blocks until data arrives or times out.
//   This correctly handles split TCP packets (headers and body in separate
//   segments) without busy-spinning on EAGAIN.
//
// Fix #7: The 5-second SO_RCVTIMEO also prevents worker threads from hanging
//   forever on slow/idle connections. A timeout returns EAGAIN/EWOULDBLOCK
//   which we treat as an error → connection dropped, thread freed.
// ─────────────────────────────────────────────────────────────────────────────

bool HttpRequest::parse(int fd) {
    // Switch fd to blocking mode — it was accepted as SOCK_NONBLOCK for the
    // epoll accept loop, but worker threads need blocking recv() with a timeout.
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    // Set a 5-second receive timeout on the client fd.
    struct timeval tv;
    tv.tv_sec  = 5;
    tv.tv_usec = 0;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Fix #4: hard cap on request body — reject anything over 1 MB.
    // Prevents a malicious client from sending Content-Length: 100MB and
    // exhausting server RAM.
    static constexpr int MAX_BODY_BYTES = 1 * 1024 * 1024; // 1 MB

    std::string raw;
    raw.reserve(8192);

    char buf[4096];
    bool  headersComplete = false;
    std::size_t headerEnd = 0;
    int   contentLength   = 0;

    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
            return false;
        }
        if (n == 0) break;

        raw.append(buf, static_cast<std::size_t>(n));

        // Fix #4: enforce size cap as we read
        if (static_cast<int>(raw.size()) > MAX_BODY_BYTES + 8192) {
            // 8192 headroom for headers; body alone cannot exceed MAX_BODY_BYTES
            return false;  // 413 would be better but parse() just returns bool
        }

        if (!headersComplete) {
            headerEnd = raw.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                headersComplete = true;

                std::string hdrs      = raw.substr(0, headerEnd);
                std::string lowerHdrs = hdrs;
                std::transform(lowerHdrs.begin(), lowerHdrs.end(),
                               lowerHdrs.begin(), ::tolower);

                auto clPos = lowerHdrs.find("content-length:");
                if (clPos != std::string::npos) {
                    auto nl = lowerHdrs.find("\r\n", clPos);
                    std::size_t valStart = clPos + 15;
                    std::size_t valLen   = (nl != std::string::npos)
                                          ? nl - valStart
                                          : hdrs.size() - valStart;
                    std::string clVal = hdrs.substr(valStart, valLen);
                    clVal.erase(0, clVal.find_first_not_of(" \t"));
                    clVal.erase(clVal.find_last_not_of(" \t\r\n") + 1);
                    try { contentLength = std::stoi(clVal); } catch (...) {}

                    // Fix #4: reject oversized declared body immediately
                    if (contentLength > MAX_BODY_BYTES) return false;
                }
            }
        }

        if (headersComplete) {
            int bodyReceived = static_cast<int>(raw.size()) -
                               static_cast<int>(headerEnd + 4);
            if (bodyReceived >= contentLength) break;
        }
    }

    if (raw.empty()) return false;
    if (!headersComplete && raw.find("\r\n\r\n") == std::string::npos) return false;

    // ── Parse request line ────────────────────────────────────────────────
    std::size_t lineEnd = raw.find("\r\n");
    if (lineEnd == std::string::npos) return false;

    std::string requestLine = raw.substr(0, lineEnd);
    std::istringstream rls(requestLine);
    std::string proto;
    std::string rawPath;
    rls >> method >> rawPath >> proto;

    if (method.empty() || rawPath.empty()) return false;

    // Validate method to block obviously malformed requests
    if (method != "GET"  && method != "POST"   && method != "PUT" &&
        method != "PATCH"&& method != "DELETE" && method != "OPTIONS" &&
        method != "HEAD") {
        return false;
    }

    // Separate path from query string (store both)
    auto qpos = rawPath.find('?');
    if (qpos != std::string::npos) {
        path        = rawPath.substr(0, qpos);
        queryString = rawPath.substr(qpos + 1);
    } else {
        path        = rawPath;
        queryString = "";
    }

    // ── Parse headers ─────────────────────────────────────────────────────
    std::size_t pos = lineEnd + 2;
    while (pos < raw.size()) {
        std::size_t end = raw.find("\r\n", pos);
        if (end == std::string::npos || end == pos) break;  // blank line

        std::string headerLine = raw.substr(pos, end - pos);
        auto colon = headerLine.find(':');
        if (colon != std::string::npos) {
            std::string name  = headerLine.substr(0, colon);
            std::string value = headerLine.substr(colon + 1);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            headers[name] = value;
        }
        pos = end + 2;
    }

    // ── Extract body ─────────────────────────────────────────────────────
    std::size_t bodyStart = raw.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        body = raw.substr(bodyStart + 4);
        if (contentLength > 0 && static_cast<int>(body.size()) > contentLength) {
            body.resize(static_cast<std::size_t>(contentLength));
        }
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// getHeader — case-insensitive (keys stored lowercase)
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpRequest::getHeader(const std::string& name) const {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = headers.find(lower);
    return (it != headers.end()) ? it->second : "";
}

// ─────────────────────────────────────────────────────────────────────────────
// getToken — Bearer token from Authorization header
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpRequest::getToken() const {
    std::string auth = getHeader("authorization");
    if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ") {
        return auth.substr(7);
    }
    return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// getClientIP — X-Forwarded-For or X-Real-IP
// Fix #16: normalize the IP; fall back to empty string (never "unknown")
//          so rate limiting keys are always distinct per-IP.
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpRequest::getClientIP() const {
    std::string ip = getHeader("x-forwarded-for");
    if (ip.empty()) ip = getHeader("x-real-ip");

    // X-Forwarded-For may be "clientIP, proxy1, proxy2" — take the first
    auto comma = ip.find(',');
    if (comma != std::string::npos) ip = ip.substr(0, comma);

    // Trim whitespace
    ip.erase(0, ip.find_first_not_of(" \t"));
    if (!ip.empty())
        ip.erase(ip.find_last_not_of(" \t\r\n") + 1);

    // Normalize IPv6 loopback / mapped addresses to plain IPv4
    // Fix #9: guard substr calls with size check
    if (ip == "::1") return "127.0.0.1";
    if (ip.size() >= 7 && ip.substr(0, 7) == "::ffff:") return ip.substr(7);

    return ip;
}

// ─────────────────────────────────────────────────────────────────────────────
// getUserAgent
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpRequest::getUserAgent() const {
    return getHeader("user-agent");
}

// ─────────────────────────────────────────────────────────────────────────────
// getQueryParam — parse a single key from the query string
// Fix #9: query params are now stored and accessible.
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpRequest::getQueryParam(const std::string& key) const {
    if (queryString.empty()) return "";

    std::string search = key + "=";
    std::size_t pos = 0;
    while (pos < queryString.size()) {
        std::size_t amp = queryString.find('&', pos);
        std::string pair = queryString.substr(pos,
            amp == std::string::npos ? std::string::npos : amp - pos);

        if (pair.substr(0, search.size()) == search) {
            return pair.substr(search.size());
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return "";
}

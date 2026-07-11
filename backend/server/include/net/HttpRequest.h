#pragma once
#include <string>
#include <map>

// ==================
// HttpRequest - parses a raw HTTP/1.1 request from a client fd.
//
// Fix #1 / #7: parse() sets SO_RCVTIMEO (5 s) on the fd so it handles
//   split TCP packets correctly without busy-spinning on EAGAIN, and
//   drops idle connections after 5 seconds.
//
// Fix #9: queryString is stored; getQueryParam() retrieves values.
// ==================

class HttpRequest {
public:
    std::string method;
    std::string path;         // path only, without query string
    std::string queryString;  // raw query string (without leading '?')
    std::string body;
    std::map<std::string, std::string> headers;  // lowercase keys

    // Reads from fd until the full request (headers + body) is received.
    // Returns true on success, false on error / timeout / connection closed.
    bool parse(int fd);

    // Case-insensitive header lookup
    std::string getHeader(const std::string& name) const;

    // Extract Bearer token from Authorization header
    std::string getToken() const;

    // Client IP from X-Forwarded-For or X-Real-IP (normalized)
    std::string getClientIP() const;

    // User-Agent header value
    std::string getUserAgent() const;

    // Retrieve a query-string parameter by key ("" if absent)
    std::string getQueryParam(const std::string& key) const;
};

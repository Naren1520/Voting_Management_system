#pragma once
#include <string>

// ==================
// HttpResponse - builds HTTP/1.1 responses with origin-validated CORS headers.
//
// CORS origins are controlled by the ALLOWED_ORIGINS env var (comma-separated).
// Default allows the known Netlify frontend + localhost dev origins.
// Set ALLOWED_ORIGINS=* only for open public APIs.
// ==================

class HttpResponse {
public:
    // Build a complete HTTP response string.
    // requestOrigin - value of the request's Origin header (used for CORS).
    static std::string build(int statusCode, const std::string& jsonBody,
                             const std::string& requestOrigin = "");

    // Build a response that also sets an HttpOnly session cookie.
    // token    - the session token value (written to vs_session cookie).
    // maxAge   - Max-Age in seconds; pass 0 to expire (clear) the cookie.
    static std::string buildWithCookie(int statusCode, const std::string& jsonBody,
                                       const std::string& requestOrigin,
                                       const std::string& token,
                                       int maxAge = 86400);

    // CORS preflight response
    static std::string buildOptions(const std::string& requestOrigin = "");

    // Quick error helper
    static std::string buildError(int statusCode, const std::string& message,
                                  const std::string& requestOrigin = "");

private:
    static const char* statusText(int code);
    // Shared CORS header block (without trailing \r\n)
    static std::string corsHeaders(const std::string& origin);
};

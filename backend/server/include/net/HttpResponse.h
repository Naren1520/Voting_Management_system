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

    // CORS preflight response
    static std::string buildOptions(const std::string& requestOrigin = "");

    // Quick error helper
    static std::string buildError(int statusCode, const std::string& message,
                                  const std::string& requestOrigin = "");

private:
    static const char* statusText(int code);
};

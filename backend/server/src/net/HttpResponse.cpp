#include "../../include/net/HttpResponse.h"
#include "../../include/core/Config.h"
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

// allowedOrigin - returns the CORS origin header value for the request origin.
// Restricts to ALLOWED_ORIGINS env var (comma-separated) or falls back to
// a wildcard only if the env var is explicitly set to "*".
// Default (no env var) allows the known production origins only.

static std::string resolveOrigin(const std::string& requestOrigin) {
    // Read once and cache
    static const std::string allowed = []() -> std::string {
        const char* env = std::getenv("ALLOWED_ORIGINS");
        if (env && *env) return std::string(env);
        // Default: allow all origins (ALLOWED_ORIGINS not set)
        return "*";
    }();

    if (allowed == "*") return "*";

    // Check if requestOrigin is in the comma-separated list
    std::string::size_type pos = 0;
    while (pos < allowed.size()) {
        auto comma = allowed.find(',', pos);
        std::string entry = (comma == std::string::npos)
                            ? allowed.substr(pos)
                            : allowed.substr(pos, comma - pos);
        // Trim whitespace
        while (!entry.empty() && (entry.front() == ' ')) entry.erase(entry.begin());
        while (!entry.empty() && (entry.back()  == ' ')) entry.pop_back();
        if (entry == requestOrigin) return requestOrigin;
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    // Origin not in allowlist - return first allowed origin as fallback
    // (browser will block the response, which is the correct behaviour)
    auto first = allowed.find(',');
    return first == std::string::npos ? allowed : allowed.substr(0, first);
}

// statusText

const char* HttpResponse::statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default:  return "OK";
    }
}

// corsHeaders - shared CORS header block used by all response builders.
// Returns the complete set of CORS headers as a header-folded string
// (each line ends with \r\n).

std::string HttpResponse::corsHeaders(const std::string& origin) {
    std::string h;
    h += "Access-Control-Allow-Origin: ";
    h += origin;
    h += "\r\n";
    h += "Access-Control-Allow-Methods: GET, POST, DELETE, PATCH, OPTIONS\r\n";
    // Include Cookie in exposed headers so JS can read it if needed,
    // and allow Content-Type + Authorization for legacy API clients.
    h += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    // Required for the browser to send cookies on cross-origin requests.
    h += "Access-Control-Allow-Credentials: true\r\n";
    h += "Vary: Origin\r\n";
    return h;
}

// build

std::string HttpResponse::build(int statusCode, const std::string& jsonBody,
                                const std::string& requestOrigin) {
    std::string origin = resolveOrigin(requestOrigin);

    std::string response;
    response.reserve(300 + jsonBody.size());

    response  = "HTTP/1.1 ";
    response += std::to_string(statusCode);
    response += " ";
    response += statusText(statusCode);
    response += "\r\n";
    response += "Content-Type: application/json\r\n";
    response += corsHeaders(origin);
    response += "Content-Length: ";
    response += std::to_string(jsonBody.size());
    response += "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += jsonBody;

    return response;
}

// buildWithCookie - same as build() but also emits a Set-Cookie header
// for the vs_session token.
//
// Cookie attributes:
//   HttpOnly  - JavaScript cannot read it, blocking XSS token theft.
//   Secure    - Only sent over HTTPS (omitted in non-HTTPS dev environments
//               when the SESSION_COOKIE_SECURE env var is "0").
//   SameSite=Lax - Sent on top-level navigations and same-site requests;
//                  blocks CSRF from third-party sites while allowing normal
//                  browser-initiated requests.
//   Path=/    - Cookie valid for all API paths.
//   Max-Age   - Matches the server-side session TTL (86400 s = 24 h).
//               Pass 0 to immediately expire/clear the cookie.

std::string HttpResponse::buildWithCookie(int statusCode, const std::string& jsonBody,
                                          const std::string& requestOrigin,
                                          const std::string& token,
                                          int maxAge) {
    std::string origin = resolveOrigin(requestOrigin);

    // Determine whether to add the Secure attribute.
    // Default ON; set SESSION_COOKIE_SECURE=0 only for local HTTP dev.
    bool secureCookie = true;
    const char* secEnv = std::getenv("SESSION_COOKIE_SECURE");
    if (secEnv && std::string(secEnv) == "0") secureCookie = false;

    std::string cookieLine = "Set-Cookie: vs_session=";
    cookieLine += token;
    cookieLine += "; Path=/; HttpOnly; SameSite=Lax";
    if (secureCookie) cookieLine += "; Secure";
    cookieLine += "; Max-Age=";
    cookieLine += std::to_string(maxAge);
    cookieLine += "\r\n";

    std::string response;
    response.reserve(400 + jsonBody.size());

    response  = "HTTP/1.1 ";
    response += std::to_string(statusCode);
    response += " ";
    response += statusText(statusCode);
    response += "\r\n";
    response += "Content-Type: application/json\r\n";
    response += corsHeaders(origin);
    response += cookieLine;
    response += "Content-Length: ";
    response += std::to_string(jsonBody.size());
    response += "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += jsonBody;

    return response;
}

std::string HttpResponse::buildOptions(const std::string& requestOrigin) {
    std::string origin = resolveOrigin(requestOrigin);
    std::string response;
    response  = "HTTP/1.1 200 OK\r\n";
    response += corsHeaders(origin);
    response += "Access-Control-Max-Age: 86400\r\n";
    response += "Content-Length: 0\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    return response;
}

std::string HttpResponse::buildError(int statusCode, const std::string& message,
                                     const std::string& requestOrigin) {
    json j;
    j["success"] = false;
    j["message"] = message;
    return build(statusCode, j.dump(), requestOrigin);
}

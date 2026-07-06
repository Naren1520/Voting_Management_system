#include "../../include/net/HttpResponse.h"
#include "../../include/core/Config.h"
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

// allowedOrigin — returns the CORS origin header value for the request origin.
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
    // Origin not in allowlist — return first allowed origin as fallback
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
    response += "Access-Control-Allow-Origin: ";
    response += origin;
    response += "\r\n";
    response += "Access-Control-Allow-Methods: GET, POST, DELETE, PATCH, OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response += "Vary: Origin\r\n";
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
    response += "Access-Control-Allow-Origin: ";
    response += origin;
    response += "\r\n";
    response += "Access-Control-Allow-Methods: GET, POST, DELETE, PATCH, OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response += "Access-Control-Max-Age: 86400\r\n";
    response += "Vary: Origin\r\n";
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

#pragma once
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

class AuthController {
public:
    json signup(const std::string& name, const std::string& email,
                const std::string& password,
                const std::string& userAgent = "",
                const std::string& ipAddress = "");

    json login(const std::string& email, const std::string& password,
               const std::string& userAgent = "",
               const std::string& ipAddress = "");

    // Returns user_id or empty string.
    // Checks Redis session cache first; falls back to Supabase on miss.
    // Caches result for 5 minutes on success.
    std::string validateToken(const std::string& token);

    json logout(const std::string& token);

    json changePassword(const std::string& token,
                        const std::string& currentPassword,
                        const std::string& newPassword);

    json ping(const std::string& token);

    json getSessions(const std::string& token);

    json revokeSession(const std::string& token, const std::string& sessionId);

    json revokeAllOtherSessions(const std::string& token);

    // Cache the token → userId mapping in Redis (called after login/signup).
    void cacheSession(const std::string& token, const std::string& userId,
                      int ttlSeconds = 3600);

    // Invalidate the Redis cache entry for this token (called on logout/revoke).
    void invalidateSession(const std::string& token);

private:
    std::string createSession(const std::string& userId,
                              const std::string& userAgent = "",
                              const std::string& ipAddress = "");

    static std::string parseDevice(const std::string& ua);
};

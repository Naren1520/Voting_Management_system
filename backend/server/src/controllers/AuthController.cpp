#include "../../include/controllers/AuthController.h"
#include "../../include/cache/RedisClient.h"
#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Logger.h"

// signup

json AuthController::signup(const std::string& name, const std::string& email,
                            const std::string& password,
                            const std::string& userAgent,
                            const std::string& ipAddress) {
    json res;
    if (name.empty() || email.empty() || password.empty()) {
        res["success"] = false;
        res["message"] = "Name, email and password are required";
        return res;
    }
    if (password.size() < 6) {
        res["success"] = false;
        res["message"] = "Password must be at least 6 characters";
        return res;
    }

    auto check = supabaseRequest("GET",
        "users?select=id&email=eq." + SupabaseClient::urlEncode(email) + "&limit=1");
    try {
        auto arr = json::parse(check.body);
        if (arr.is_array() && !arr.empty()) {
            res["success"] = false;
            res["message"] = "Email already registered";
            return res;
        }
    } catch (...) {}

    std::string hash = SupabaseClient::hashPassword(password);
    if (hash.empty()) {
        res["success"] = false;
        res["message"] = "Server error during signup";
        return res;
    }
    LOG_INFO("[SIGNUP] hash prefix=" + hash.substr(0, 20) +
             " len=" + std::to_string(hash.size()));

    json body;
    body["name"]          = name;
    body["email"]         = email;
    body["password_hash"] = hash;

    auto r = supabaseRequest("POST", "users", body.dump());
    try {
        auto arr = json::parse(r.body);
        if ((r.statusCode == 200 || r.statusCode == 201) &&
            arr.is_array() && !arr.empty()) {
            std::string userId = arr[0]["id"].get<std::string>();
            std::string token  = createSession(userId, userAgent, ipAddress);
            // Pre-warm Redis session cache (Fix #15: consistent 24h TTL = DB session lifetime)
            cacheSession(token, userId, 86400);
            res["success"]      = true;
            res["message"]      = "Account created successfully";
            res["token"]        = token;
            res["user"]["id"]   = userId;
            res["user"]["name"] = name;
            res["user"]["email"]= email;
        } else {
            LOG_ERROR("[SIGNUP] Supabase insert failed. Status: " +
                      std::to_string(r.statusCode) + " Body: " + r.body);
            res["success"] = false;
            res["message"] = "Failed to create account";
        }
    } catch (...) {
        res["success"] = false;
        res["message"] = "Server error";
    }
    return res;
}

// login

json AuthController::login(const std::string& email, const std::string& password,
                           const std::string& userAgent,
                           const std::string& ipAddress) {
    json res;
    if (email.empty() || password.empty()) {
        res["success"] = false;
        res["message"] = "Email and password are required";
        return res;
    }

    auto r = supabaseRequest("GET",
        "users?select=id,name,email,password_hash&email=eq." +
        SupabaseClient::urlEncode(email) + "&limit=1");
    try {
        auto arr = json::parse(r.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"] = false;
            res["message"] = "Invalid email or password";
            return res;
        }
        std::string storedHash = arr[0]["password_hash"].get<std::string>();
        LOG_INFO("[LOGIN] hash prefix=" + storedHash.substr(0, 20) +
                 " len=" + std::to_string(storedHash.size()));
        // Fix #3: use verifyPassword() which handles both PBKDF2 (new) and
        // legacy SHA-256 hashes. If a legacy hash matches, re-hash with PBKDF2
        // and update the DB transparently (automatic migration on login).
        if (!SupabaseClient::verifyPassword(password, storedHash)) {
            LOG_INFO("[LOGIN] verifyPassword returned false for " + email);
            res["success"] = false;
            res["message"] = "Invalid email or password";
            return res;
        }
        // Migrate legacy SHA-256 hash to PBKDF2 on successful login
        if (storedHash.rfind("pbkdf2$", 0) != 0) {
            std::string newHash = SupabaseClient::hashPassword(password);
            if (!newHash.empty()) {
                json upd; upd["password_hash"] = newHash;
                supabaseRequest("PATCH",
                    "users?id=eq." + arr[0]["id"].get<std::string>(), upd.dump());
                LOG_INFO("[AUTH] Migrated password hash for user to PBKDF2");
            }
        }
        std::string userId = arr[0]["id"].get<std::string>();
        std::string token  = createSession(userId, userAgent, ipAddress);
        // Pre-warm Redis session cache (Fix #15: 24h = DB session lifetime)
        cacheSession(token, userId, 86400);
        res["success"]        = true;
        res["message"]        = "Login successful";
        res["token"]          = token;
        res["user"]["id"]     = userId;
        res["user"]["name"]   = arr[0]["name"].get<std::string>();
        res["user"]["email"]  = email;
    } catch (...) {
        res["success"] = false;
        res["message"] = "Server error during login";
    }
    return res;
}

// validateToken - returns user_id or empty string
// Checks Redis first; falls back to Supabase on cache miss.

std::string AuthController::validateToken(const std::string& token) {
    if (token.empty()) return "";

    // 1. Check Redis cache first
    auto& redis = RedisClient::instance();
    if (redis.isAvailable()) {
        std::string cached = redis.get("session:" + token);
        if (!cached.empty()) return cached;  // cache hit - no DB round-trip
    }

    // 2. Cache miss - query Supabase for the session
    // Fix #1: Do NOT run DELETE on every cache miss. Expired-session cleanup
    // is now handled by a background timer in the server (runs every 10 min),
    // not on the hot path of every authenticated request.
    auto r = supabaseRequest("GET",
        "sessions?select=user_id,expires_at&token=eq." +
        SupabaseClient::urlEncode(token) + "&limit=1");
    try {
        auto arr = json::parse(r.body);
        if (arr.is_array() && !arr.empty()) {
            // Check expiry client-side to avoid a wasted cache write
            std::string expiresAt = arr[0].value("expires_at", "");
            if (!expiresAt.empty() && expiresAt < SupabaseClient::currentTimestamp()) {
                // Expired - delete just this one session and return empty
                supabaseRequest("DELETE",
                    "sessions?token=eq." + SupabaseClient::urlEncode(token));
                return "";
            }
            std::string userId = arr[0]["user_id"].get<std::string>();
            // 3. Store in Redis for 24 hours
            if (redis.isAvailable()) redis.set("session:" + token, userId, 86400);
            return userId;
        }
    } catch (...) {}
    return "";
}

// logout

json AuthController::logout(const std::string& token) {
    // Invalidate Redis session cache entry first
    invalidateSession(token);
    supabaseRequest("DELETE", "sessions?token=eq." + SupabaseClient::urlEncode(token));
    json res;
    res["success"] = true;
    res["message"] = "Logged out successfully";
    return res;
}

// changePassword

json AuthController::changePassword(const std::string& token,
                                    const std::string& currentPassword,
                                    const std::string& newPassword) {
    json res;
    std::string userId = validateToken(token);
    if (userId.empty()) {
        res["success"] = false; res["message"] = "Unauthorized"; return res;
    }
    if (currentPassword.empty() || newPassword.empty()) {
        res["success"] = false;
        res["message"] = "Current and new password are required";
        return res;
    }
    if (newPassword.size() < 6) {
        res["success"] = false;
        res["message"] = "New password must be at least 6 characters";
        return res;
    }

    auto r = supabaseRequest("GET",
        "users?select=password_hash&id=eq." + userId + "&limit=1");
    std::string storedHash;
    try {
        auto arr = json::parse(r.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"] = false; res["message"] = "User not found"; return res;
        }
        storedHash = arr[0]["password_hash"].get<std::string>();
    } catch (...) {
        res["success"] = false; res["message"] = "Server error"; return res;
    }

    if (!SupabaseClient::verifyPassword(currentPassword, storedHash)) {
        res["success"] = false;
        res["message"] = "Wrong password. Please try again.";
        return res;
    }

    std::string newHash = SupabaseClient::hashPassword(newPassword);
    if (newHash.empty()) {
        res["success"] = false;
        res["message"] = "Server error while hashing password";
        return res;
    }

    json upd; upd["password_hash"] = newHash;
    auto upRes = supabaseRequest("PATCH", "users?id=eq." + userId, upd.dump());
    if (upRes.statusCode == 200 || upRes.statusCode == 204) {
        res["success"] = true; res["message"] = "Password updated successfully";
    } else {
        res["success"] = false; res["message"] = "Failed to update password";
    }
    return res;
}

// ping

json AuthController::ping(const std::string& token) {
    json res;
    std::string userId = validateToken(token);
    if (userId.empty()) {
        res["success"] = false; res["message"] = "Session expired"; return res;
    }
    res["success"] = true;
    return res;
}

// getSessions

json AuthController::getSessions(const std::string& token) {
    json res;
    std::string userId = validateToken(token);
    if (userId.empty()) {
        res["success"] = false; res["message"] = "Unauthorized"; return res;
    }
    // Fix #1: don't DELETE expired sessions on every view - background cleanup handles this

    auto r = supabaseRequest("GET",
        "sessions?select=token,user_agent,ip_address,location,created_at,expires_at"
        "&user_id=eq." + userId + "&order=created_at.desc");
    try {
        auto arr = json::parse(r.body);
        json sessions = json::array();
        for (auto& s : arr) {
            s["is_current"] = (s["token"].get<std::string>() == token);
            s["session_id"] = s["token"].get<std::string>().substr(0, 16);
            s.erase("token");
            sessions.push_back(s);
        }
        res["success"]  = true;
        res["sessions"] = sessions;
    } catch (...) {
        res["success"] = false; res["message"] = "Failed to load sessions";
    }
    return res;
}

// revokeSession

json AuthController::revokeSession(const std::string& token, const std::string& sessionId) {
    json res;
    std::string userId = validateToken(token);
    if (userId.empty()) {
        res["success"] = false; res["message"] = "Unauthorized"; return res;
    }
    auto r = supabaseRequest("GET",
        "sessions?select=token&user_id=eq." + userId +
        "&token=like." + SupabaseClient::urlEncode(sessionId + "%") + "&limit=1");
    try {
        auto arr = json::parse(r.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"] = false; res["message"] = "Session not found"; return res;
        }
        std::string fullToken = arr[0]["token"].get<std::string>();
        if (fullToken == token) {
            res["success"] = false;
            res["message"] = "Use logout to end your current session";
            return res;
        }
        // Invalidate Redis cache for the revoked session
        invalidateSession(fullToken);
        supabaseRequest("DELETE",
            "sessions?token=eq." + SupabaseClient::urlEncode(fullToken));
        res["success"] = true; res["message"] = "Session revoked";
    } catch (...) {
        res["success"] = false; res["message"] = "Server error";
    }
    return res;
}

// revokeAllOtherSessions

json AuthController::revokeAllOtherSessions(const std::string& token) {
    json res;
    std::string userId = validateToken(token);
    if (userId.empty()) {
        res["success"] = false; res["message"] = "Unauthorized"; return res;
    }
    // Fetch all other session tokens for this user before deleting
    auto& redis = RedisClient::instance();
    if (redis.isAvailable()) {
        auto sessR = supabaseRequest("GET",
            "sessions?select=token&user_id=eq." + userId +
            "&token=neq." + SupabaseClient::urlEncode(token));
        try {
            auto arr = json::parse(sessR.body);
            if (arr.is_array()) {
                for (auto& s : arr) {
                    std::string t = s["token"].get<std::string>();
                    invalidateSession(t);
                }
            }
        } catch (...) {}
    }
    supabaseRequest("DELETE",
        "sessions?user_id=eq." + userId +
        "&token=neq." + SupabaseClient::urlEncode(token));
    res["success"] = true; res["message"] = "All other sessions revoked";
    return res;
}

// Private helpers

std::string AuthController::parseDevice(const std::string& ua) {
    if (ua.empty()) return "Unknown Device";
    std::string browser = "Unknown Browser";
    std::string os      = "Unknown OS";
    if      (ua.find("Edg/")    != std::string::npos) browser = "Edge";
    else if (ua.find("OPR/")    != std::string::npos ||
             ua.find("Opera")   != std::string::npos) browser = "Opera";
    else if (ua.find("Chrome/") != std::string::npos) browser = "Chrome";
    else if (ua.find("Firefox/")!= std::string::npos) browser = "Firefox";
    else if (ua.find("Safari/") != std::string::npos) browser = "Safari";

    if      (ua.find("Windows NT") != std::string::npos) os = "Windows";
    else if (ua.find("Android")    != std::string::npos) os = "Android";
    else if (ua.find("iPhone")     != std::string::npos ||
             ua.find("iPad")       != std::string::npos) os = "iOS";
    else if (ua.find("Mac OS X")   != std::string::npos) os = "macOS";
    else if (ua.find("Linux")      != std::string::npos) os = "Linux";

    return browser + " on " + os;
}

std::string AuthController::createSession(const std::string& userId,
                                          const std::string& userAgent,
                                          const std::string& ipAddress) {
    std::string token    = SupabaseClient::generateToken();
    std::string expires  = SupabaseClient::futureTimestamp(86400);
    std::string device   = parseDevice(userAgent);

    // NOTE: getLocation() makes an outbound HTTP call (ip-api.com, 3s timeout).
    // We skip it here to avoid blocking the handle pool during concurrent
    // logins. Location is stored as "Unknown" and can be enriched later via
    // a background job if needed.
    std::string location = "Unknown";

    json body;
    body["token"]      = token;
    body["user_id"]    = userId;
    body["expires_at"] = expires;
    body["user_agent"] = userAgent;
    body["ip_address"] = ipAddress;
    body["location"]   = location;
    supabaseRequest("POST", "sessions", body.dump());
    return token;
}

// cacheSession - store token → userId in Redis

void AuthController::cacheSession(const std::string& token,
                                  const std::string& userId,
                                  int ttlSeconds) {
    auto& redis = RedisClient::instance();
    if (!redis.isAvailable()) return;
    redis.set("session:" + token, userId, ttlSeconds);
}

// invalidateSession - remove token from Redis cache

void AuthController::invalidateSession(const std::string& token) {
    auto& redis = RedisClient::instance();
    if (!redis.isAvailable()) return;
    redis.del("session:" + token);
}

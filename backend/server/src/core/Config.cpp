#include "../../include/core/Config.h"
#include <cstdlib>
#include <iostream>

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

void Config::load() {
    const char* url = std::getenv("SUPABASE_URL");
    const char* key = std::getenv("SUPABASE_KEY");

    if (!url || std::string(url).empty()) {
        std::cout << "[FATAL] SUPABASE_URL environment variable is not set\n";
        std::cout.flush();
        std::exit(1);
    }
    if (!key || std::string(key).empty()) {
        std::cout << "[FATAL] SUPABASE_KEY environment variable is not set\n";
        std::cout.flush();
        std::exit(1);
    }

    supabaseUrl_ = url;
    supabaseKey_ = key;

    std::cout << "[Config] SUPABASE_URL loaded: " << supabaseUrl_.substr(0, 30) << "...\n";
    std::cout.flush();

    const char* portEnv = std::getenv("PORT");
    if (portEnv && *portEnv) {
        int p = std::atoi(portEnv);
        if (p > 0) port_ = p;
    }

    // Redis URL — optional, gracefully absent
    const char* redisEnv = std::getenv("REDIS_URL");
    redisUrl_ = (redisEnv && *redisEnv) ? std::string(redisEnv) : "redis://127.0.0.1:6379";

    // Rate limiting
    const char* rlReq = std::getenv("RATE_LIMIT_REQUESTS");
    if (rlReq && *rlReq) {
        int v = std::atoi(rlReq);
        if (v > 0) rateLimitRequests_ = v;
    }

    const char* rlWin = std::getenv("RATE_LIMIT_WINDOW_SEC");
    if (rlWin && *rlWin) {
        int v = std::atoi(rlWin);
        if (v > 0) rateLimitWindowSec_ = v;
    }
}

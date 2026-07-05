#pragma once
#include <string>

// ==================
// Config — singleton that loads env vars at startup
// ==================

class Config {
public:
    static Config& instance();

    void load();                        // call once from main()

    const std::string& supabaseUrl() const { return supabaseUrl_; }
    const std::string& supabaseKey() const { return supabaseKey_; }
    int port() const                       { return port_; }

    // Redis / caching
    const std::string& redisUrl() const    { return redisUrl_; }

    // Rate limiting
    int rateLimitRequests() const  { return rateLimitRequests_; }   // default 100
    int rateLimitWindowSec() const { return rateLimitWindowSec_; }  // default 60

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    std::string supabaseUrl_;
    std::string supabaseKey_;
    int port_ = 8080;

    std::string redisUrl_;
    int rateLimitRequests_  = 100;
    int rateLimitWindowSec_ = 60;
};

#include "../include/core/Config.h"
#include "../include/core/Logger.h"
#include "../include/db/SupabaseClient.h"
#include "../include/cache/RedisClient.h"
#include "../include/net/EpollServer.h"
#include <cstdlib>

int main() {
    // Load env config (exits on missing SUPABASE_URL / SUPABASE_KEY)
    Config::instance().load();

    // Open log file. LOG_FILE env var overrides the path.
    // On Render/cloud: set LOG_FILE to empty string to skip file logging
    // (stdout is captured by the platform). Default to server.log locally.
    const char* logFile = std::getenv("LOG_FILE");
    // Only open a file if LOG_FILE is explicitly set to a non-empty path
    if (logFile && std::string(logFile).size() > 0) {
        Logger::instance().openFile(logFile);
    }
    // Otherwise log to stdout only — safe on read-only filesystems

    // Init Redis connection (optional — graceful degradation if unavailable)
    RedisClient::instance().init();

    LOG_INFO("Starting VoteStack API Server — Phase 2 (Redis cache + rate limiting)");

    // NOTE: SupabaseClient::init() is called inside EpollServer::start()
    // after the thread count is determined, so the curl pool size matches
    // the worker count exactly.
    EpollServer server(Config::instance().port());
    server.start();

    return 0;
}

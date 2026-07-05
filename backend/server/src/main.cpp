#include "../include/core/Config.h"
#include "../include/core/Logger.h"
#include "../include/db/SupabaseClient.h"
#include "../include/cache/RedisClient.h"
#include "../include/net/EpollServer.h"
#include <cstdlib>

int main() {
    // Load env config (exits on missing SUPABASE_URL / SUPABASE_KEY)
    Config::instance().load();

    // Fix #7: open log file for persistent logging.
    // Set LOG_FILE env var to change path (default: server.log).
    const char* logFile = std::getenv("LOG_FILE");
    Logger::instance().openFile(logFile ? logFile : "server.log");

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

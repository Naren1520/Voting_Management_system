#include "../include/core/Config.h"
#include "../include/core/Logger.h"
#include "../include/db/SupabaseClient.h"
#include "../include/cache/RedisClient.h"
#include "../include/net/EpollServer.h"
#include "../include/controllers/FaceController.h"
#include <iostream>
#include <cstdlib>
#include <stdexcept>

int main() {
  try {
    // Load env config (exits on missing SUPABASE_URL / SUPABASE_KEY)
    Config::instance().load();

    const char* logFile = std::getenv("LOG_FILE");
    if (logFile && std::string(logFile).size() > 0) {
        Logger::instance().openFile(logFile);
    }

    std::cout << "[main] Config loaded, port=" << Config::instance().port() << "\n";
    std::cout.flush();

    // Load AES-256-GCM key for biometric embedding encryption.
    // Aborts if EMBEDDING_ENCRYPTION_KEY is missing or malformed.
    FaceController::loadEncryptionKey();

    std::cout << "[main] Initialising Redis...\n"; std::cout.flush();
    RedisClient::instance().init();
    std::cout << "[main] Redis init done\n"; std::cout.flush();

    std::cout << "[main] Creating EpollServer on port "
              << Config::instance().port() << "\n"; std::cout.flush();

    EpollServer server(Config::instance().port());

    std::cout << "[main] Calling server.start()\n"; std::cout.flush();
    server.start();

  } catch (const std::exception& e) {
    std::cerr << "[FATAL] Uncaught exception: " << e.what() << "\n";
    std::cerr.flush();
    return 1;
  } catch (...) {
    std::cerr << "[FATAL] Unknown uncaught exception\n";
    std::cerr.flush();
    return 1;
  }

  return 0;
}

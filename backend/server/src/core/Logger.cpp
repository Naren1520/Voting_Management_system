#include "../../include/core/Logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (fileStream_.is_open()) fileStream_.close();
}

// ─────────────────────────────────────────────────────────────────────────────
// openFile — Fix #7: open log file for persistent logging.
// Called once from main() using LOG_FILE env var (default: server.log).
// ─────────────────────────────────────────────────────────────────────────────

void Logger::openFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path.empty()) return;
    fileStream_.open(path, std::ios::app);
    if (!fileStream_.is_open()) {
        std::cerr << "[Logger] WARNING: could not open log file: " << path
                  << " — logging to stdout only\n";
    } else {
        std::cout << "[Logger] Logging to file: " << path << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// levelStr / timestamp helpers
// ─────────────────────────────────────────────────────────────────────────────

const char* Logger::levelStr(LogLevel l) {
    switch (l) {
        case LogLevel::INFO: return "INFO ";
        case LogLevel::WARN: return "WARN ";
        case LogLevel::ERR:  return "ERROR";
        default:             return "INFO ";
    }
}

std::string Logger::timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#ifdef _WIN32
    gmtime_s(&tmBuf, &now);
#else
    gmtime_r(&now, &tmBuf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return std::string(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// log — write to stdout AND file (if open).
// Flush is done every ~100ms in the background (not on every line) to avoid
// serializing all worker threads on I/O during heavy load.
// ─────────────────────────────────────────────────────────────────────────────

void Logger::log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string line = "[" + timestamp() + "] [" +
                       levelStr(level) + "] " + msg + "\n";

    // Write without flushing — OS buffers handle the rest.
    // flush() only when it's an error so error lines appear immediately.
    std::cout << line;
    if (level == LogLevel::ERR) std::cout.flush();

    if (fileStream_.is_open()) {
        fileStream_ << line;
        if (level == LogLevel::ERR) fileStream_.flush();
    }
}

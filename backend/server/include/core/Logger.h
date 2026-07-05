#pragma once
#include <string>
#include <mutex>
#include <fstream>

// ==================
// Logger — thread-safe logger writing to stdout AND an optional log file.
//
// Fix #7: Logs are written to a file (LOG_FILE env var, default server.log)
//   so they survive a crash. Without a file, stdout logs are lost on the
//   free-tier VPS when the terminal closes or the process dies.
// ==================

enum class LogLevel { INFO, WARN, ERR };

class Logger {
public:
    static Logger& instance();

    // Open the log file. Call once from main() after Config::load().
    // path — file path; if empty, logs to stdout only.
    void openFile(const std::string& path);

    void log(LogLevel level, const std::string& msg);

    void info(const std::string& msg)  { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg)  { log(LogLevel::WARN, msg); }
    void error(const std::string& msg) { log(LogLevel::ERR,  msg); }

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex    mutex_;
    std::ofstream fileStream_;  // closed if not opened

    static const char* levelStr(LogLevel l);
    static std::string timestamp();
};

// Convenience macros
#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)

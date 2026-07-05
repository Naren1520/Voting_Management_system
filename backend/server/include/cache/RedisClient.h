#pragma once
#include <string>
#include <array>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>

// ============================================================================
// RedisClient — singleton that speaks RESP over raw TCP sockets.
// No external Redis library — uses POSIX sockets directly.
// Gracefully degrades: if Redis is unavailable, all ops are no-ops.
//
// Fix #5:  connectSlot() in the reconnect path is now called while holding
//          poolMutex_ so two threads cannot race to reconnect the same slot.
//
// Fix #6:  available_ is now std::atomic<bool> — safe to read from any thread
//          without holding the pool mutex.
// ============================================================================

class RedisClient {
public:
    static RedisClient& instance();

    // Connect to Redis using REDIS_URL env var.
    // Format: redis://host:port  or  redis://:password@host:port
    // Call once from main() after Config::load().
    void init();

    // Lock-free read (Fix #6: atomic).
    bool isAvailable() const;

    // ── Core ops ──────────────────────────────────────────────────────────
    bool        set(const std::string& key, const std::string& value,
                    int ttlSeconds = 0);
    std::string get(const std::string& key);
    bool        del(const std::string& key);
    bool        exists(const std::string& key);
    long long   incr(const std::string& key);
    bool        expire(const std::string& key, int ttlSeconds);

    // ── Rate limiting ─────────────────────────────────────────────────────
    bool checkRateLimit(const std::string& key, int maxRequests, int windowSeconds);

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

private:
    RedisClient() = default;

    struct Conn {
        int  socket{-1};
        bool inUse{false};
    };

    // Pool size must cover the thread-pool worker count (hardware_concurrency × 2,
    // min 4). 32 covers up to a 16-core box with headroom.
    static constexpr int POOL_SIZE = 32;

    std::array<Conn, POOL_SIZE> conns_{};  // fixed-size array matches constant

    int  acquireConn();
    void releaseConn(int idx);

    // Fix #5: connectSlot / disconnectSlot must be called while holding
    // poolMutex_. reconnectSlot() acquires the mutex internally.
    bool connectSlot(int idx);       // caller holds poolMutex_
    void disconnectSlot(int idx);    // caller holds poolMutex_
    bool reconnectSlot(int idx);     // acquires poolMutex_ internally

    static std::string buildCommand(const std::vector<std::string>& args);
    bool        sendAll(int idx, const std::string& data);
    std::string readResponse(int idx);
    std::string parseSimpleString(const std::string& resp);
    std::string parseBulkString(const std::string& resp);
    long long   parseInteger(const std::string& resp);
    std::string sendCommand(int idx, const std::vector<std::string>& args);

    mutable std::mutex        poolMutex_;
    std::condition_variable   poolCv_;
    // conns_ declared above with std::array<Conn, POOL_SIZE>
    std::atomic<bool>         available_{false};   // Fix #6
    std::string               host_{"127.0.0.1"};
    int                       port_{6379};
    std::string               password_;
};

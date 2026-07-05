#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <curl/curl.h>

// ==================
// SupabaseClient — libcurl-based Supabase REST client with handle pool.
//
// Fix #5:  acquireHandle() uses a condition variable instead of busy-waiting.
// Fix #6:  POOL_SIZE is set at init() time to match the thread-pool size so
//          workers never spin waiting for a handle.
// Fix #10: getLocation() uses a handle from the shared pool instead of
//          creating/destroying a separate curl handle per call.
// ==================

struct HttpResult {
    int         statusCode{0};
    std::string body;
};

class SupabaseClient {
public:
    static SupabaseClient& instance();

    // Initialise curl global state + handle pool; call once from main().
    // poolSize should equal the thread-pool worker count (default: cores*2).
    void init(int poolSize = 0);   // 0 → hardware_concurrency()*2, min 8

    // Main request entry point
    HttpResult request(const std::string& method,
                       const std::string& endpoint,
                       const std::string& body        = "",
                       const std::string& extraHeader = "");

    // Crypto helpers (OpenSSL, no popen)
    // Fix #3: hashPassword uses PBKDF2-SHA256 (100k iterations) — not plain SHA-256.
    // verifyPassword handles both new PBKDF2 hashes and legacy SHA-256 hashes.
    static std::string hashPassword(const std::string& password);
    static bool        verifyPassword(const std::string& password,
                                      const std::string& stored);
    static std::string generateToken();
    static std::string urlEncode(const std::string& s);

    // Geolocation via libcurl — uses the shared handle pool (Fix #10)
    static std::string getLocation(const std::string& ip);

    // Timestamp helpers
    static std::string currentTimestamp();
    static std::string futureTimestamp(int secondsFromNow);

    // Non-copyable
    SupabaseClient(const SupabaseClient&) = delete;
    SupabaseClient& operator=(const SupabaseClient&) = delete;

private:
    SupabaseClient() = default;

    std::vector<CURL*>      handlePool_;
    std::mutex              poolMutex_;
    std::condition_variable poolCv_;    // Fix #5: condvar replaces busy-wait

    CURL* acquireHandle();
    void  releaseHandle(CURL* handle);

    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
};

// Convenience free function (delegates to singleton)
inline HttpResult supabaseRequest(const std::string& method,
                                  const std::string& endpoint,
                                  const std::string& body        = "",
                                  const std::string& extraHeader = "") {
    return SupabaseClient::instance().request(method, endpoint, body, extraHeader);
}

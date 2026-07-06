#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Config.h"
#include "../../include/core/Logger.h"
#include "../../third_party/json.hpp"

#include <iostream>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/crypto.h>

#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <algorithm>

using json = nlohmann::json;

// Singleton

SupabaseClient& SupabaseClient::instance() {
    static SupabaseClient client;
    return client;
}

// init — build handle pool sized to match the thread pool.
// Fix #6: pool size matches thread count so workers never spin-wait.

void SupabaseClient::init(int poolSize) {
    curl_global_init(CURL_GLOBAL_ALL);

    if (poolSize <= 0) {
        int cores = static_cast<int>(std::thread::hardware_concurrency());
        if (cores < 1) cores = 2;
        poolSize = std::max(8, cores * 2);
    }

    handlePool_.reserve(static_cast<size_t>(poolSize));
    for (int i = 0; i < poolSize; ++i) {
        CURL* h = curl_easy_init();
        if (!h) {
            std::cerr << "[FATAL] curl_easy_init() returned null — libcurl init failed\n";
            std::cerr.flush();
            throw std::runtime_error("curl_easy_init failed");
        }
        curl_easy_setopt(h, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(h, CURLOPT_TCP_KEEPIDLE,  60L);
        curl_easy_setopt(h, CURLOPT_TCP_KEEPINTVL, 30L);
        curl_easy_setopt(h, CURLOPT_FORBID_REUSE,  0L);
        curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
        handlePool_.push_back(h);
    }
    LOG_INFO("[Supabase] curl handle pool: " + std::to_string(poolSize) + " handles");
}

// acquireHandle / releaseHandle
// Fix #5: condition variable replaces the old busy-wait spin loop.

CURL* SupabaseClient::acquireHandle() {
    std::unique_lock<std::mutex> lock(poolMutex_);
    poolCv_.wait(lock, [this] { return !handlePool_.empty(); });
    CURL* h = handlePool_.back();
    handlePool_.pop_back();
    return h;
}

void SupabaseClient::releaseHandle(CURL* handle) {
    if (!handle) return;
    curl_easy_reset(handle);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE,  60L);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 30L);
    curl_easy_setopt(handle, CURLOPT_FORBID_REUSE,  0L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 30L);
    {
        std::lock_guard<std::mutex> lock(poolMutex_);
        handlePool_.push_back(handle);
    }
    poolCv_.notify_one();   // Fix #5: wake a waiting acquireHandle()
}

// writeCallback

size_t SupabaseClient::writeCallback(char* ptr, size_t size,
                                     size_t nmemb, void* userdata) {
    auto* buf = reinterpret_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// request — main entry point

HttpResult SupabaseClient::request(const std::string& method,
                                   const std::string& endpoint,
                                   const std::string& body,
                                   const std::string& extraHeader) {
    const Config& cfg = Config::instance();
    std::string url = cfg.supabaseUrl() + "/rest/v1/" + endpoint;

    CURL* curl = acquireHandle();
    std::string responseBody;
    long httpCode = 0;

    struct curl_slist* headers = nullptr;
    std::string apiKeyHeader = "apikey: "              + cfg.supabaseKey();
    std::string authHeader   = "Authorization: Bearer " + cfg.supabaseKey();
    headers = curl_slist_append(headers, apiKeyHeader.c_str());
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Prefer: return=representation");
    if (!extraHeader.empty())
        headers = curl_slist_append(headers, extraHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &responseBody);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG_ERROR("[SB] curl_easy_perform failed: " +
                  std::string(curl_easy_strerror(res)));
        curl_slist_free_all(headers);
        releaseHandle(curl);
        return {500, "[]"};
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    releaseHandle(curl);

    std::string shortEndpoint = endpoint.size() > 60
                                ? endpoint.substr(0, 60) : endpoint;
    LOG_INFO("[SB] " + method + " " + shortEndpoint +
             " -> " + std::to_string(httpCode));

    return {static_cast<int>(httpCode), responseBody};
}

// hashPassword — PBKDF2-SHA256 via OpenSSL.
//
// Fix #3: SHA-256 is a fast hash — trivially brute-forced with a GPU.
//   PBKDF2 with 100,000 iterations is ~10,000× slower per attempt, making
//   offline dictionary attacks impractical.
//
//   Format stored in DB:  pbkdf2$<hex-salt>$<hex-derived-key>
//   The salt is random per password so identical passwords produce different
//   hashes. verifyPassword() handles the split + re-derive + compare.
//
//   NOTE: existing SHA-256 hashes in the DB will stop matching.
//   Migration path: on next successful login, re-hash and update the DB.
//   See the login() migration block in AuthController.cpp.

static constexpr int    PBKDF2_ITERATIONS = 100000;
static constexpr int    PBKDF2_KEYLEN     = 32;   // 256-bit derived key
static constexpr int    PBKDF2_SALTLEN    = 16;   // 128-bit random salt

std::string SupabaseClient::hashPassword(const std::string& password) {
    // Generate a random salt
    unsigned char salt[PBKDF2_SALTLEN];
    if (RAND_bytes(salt, PBKDF2_SALTLEN) != 1) return "";

    // Derive key
    unsigned char key[PBKDF2_KEYLEN];
    if (PKCS5_PBKDF2_HMAC(password.c_str(),
                           static_cast<int>(password.size()),
                           salt, PBKDF2_SALTLEN,
                           PBKDF2_ITERATIONS,
                           EVP_sha256(),
                           PBKDF2_KEYLEN, key) != 1) {
        return "";
    }

    // Encode as hex: "pbkdf2$<salt_hex>$<key_hex>"
    auto toHex = [](const unsigned char* data, int len) {
        std::ostringstream oss;
        for (int i = 0; i < len; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(data[i]);
        return oss.str();
    };

    return "pbkdf2$" + toHex(salt, PBKDF2_SALTLEN) +
           "$"       + toHex(key,  PBKDF2_KEYLEN);
}

// verifyPassword — re-derive the key from the stored salt and compare.
// Returns true if the password matches the stored hash.
// Also handles legacy SHA-256 hashes (plain 64-char hex) for migration.

bool SupabaseClient::verifyPassword(const std::string& password,
                                    const std::string& stored) {
    // ─ Legacy SHA-256 hash (plain 64-char hex, no prefix) 
    if (stored.rfind("pbkdf2$", 0) != 0) {
        // Old format — just compare SHA-256 directly for migration
        unsigned char digest[SHA256_DIGEST_LENGTH];
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return false;
        bool ok = (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
                   EVP_DigestUpdate(ctx, password.c_str(), password.size()) == 1 &&
                   EVP_DigestFinal_ex(ctx, digest, nullptr) == 1);
        EVP_MD_CTX_free(ctx);
        if (!ok) return false;
        std::ostringstream oss;
        for (unsigned char b : digest)
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(b);
        return oss.str() == stored;
    }

    //  PBKDF2 hash: "pbkdf2$<salt_hex>$<key_hex>" 
    // Parse salt and expected key from stored string
    // "pbkdf2" = 6 chars, '$' is at index 6, salt starts at index 7
    auto first  = stored.find('$');          // index 6
    auto second = stored.find('$', first + 1); // index after salt
    if (first == std::string::npos || second == std::string::npos) return false;

    std::string saltHex = stored.substr(first + 1, second - first - 1);
    std::string keyHex  = stored.substr(second + 1);

    // Decode hex → bytes
    auto fromHex = [](const std::string& hex, unsigned char* out, int maxLen) -> int {
        int len = static_cast<int>(hex.size()) / 2;
        if (len > maxLen) return -1;
        for (int i = 0; i < len; ++i) {
            out[i] = static_cast<unsigned char>(
                std::stoi(hex.substr(static_cast<size_t>(i * 2), 2), nullptr, 16));
        }
        return len;
    };

    unsigned char salt[PBKDF2_SALTLEN];
    unsigned char expectedKey[PBKDF2_KEYLEN];
    if (fromHex(saltHex, salt, PBKDF2_SALTLEN) != PBKDF2_SALTLEN) return false;
    if (fromHex(keyHex,  expectedKey, PBKDF2_KEYLEN) != PBKDF2_KEYLEN) return false;

    // Re-derive
    unsigned char derivedKey[PBKDF2_KEYLEN];
    if (PKCS5_PBKDF2_HMAC(password.c_str(),
                           static_cast<int>(password.size()),
                           salt, PBKDF2_SALTLEN,
                           PBKDF2_ITERATIONS,
                           EVP_sha256(),
                           PBKDF2_KEYLEN, derivedKey) != 1) {
        return false;
    }

    // Constant-time compare to prevent timing attacks
    return CRYPTO_memcmp(derivedKey, expectedKey, PBKDF2_KEYLEN) == 0;
}

// generateToken — 32 random bytes as hex via RAND_bytes

std::string SupabaseClient::generateToken() {
    unsigned char buf[32];
    if (RAND_bytes(buf, sizeof(buf)) != 1)
        return "fallback_" + std::to_string(std::time(nullptr));

    std::ostringstream oss;
    for (unsigned char b : buf)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

// urlEncode

std::string SupabaseClient::urlEncode(const std::string& s) {
    std::string r;
    r.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            r += static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            r += buf;
        }
    }
    return r;
}

// getLocation — uses a handle from the shared pool.
// Fix #10: no longer creates/destroys a fresh handle on every call.

std::string SupabaseClient::getLocation(const std::string& ip) {
    // Fix #9: guard all substr calls with size checks
    if (ip.empty() || ip == "127.0.0.1") return "Local / Unknown";
    if (ip.size() >= 3  && ip.substr(0, 3)  == "10.")      return "Local / Unknown";
    if (ip.size() >= 8  && ip.substr(0, 8)  == "192.168.") return "Local / Unknown";
    if (ip.size() >= 7  && ip.substr(0, 7)  == "172.16.")  return "Local / Unknown";

    std::string url = "http://ip-api.com/json/" + ip + "?fields=city,country";
    std::string responseBody;

    CURL* curl = instance().acquireHandle();
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        3L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &responseBody);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    instance().releaseHandle(curl);

    if (res != CURLE_OK) return "Unknown";

    try {
        auto j = nlohmann::json::parse(responseBody);
        std::string city    = j.value("city",    "");
        std::string country = j.value("country", "");
        if (!city.empty() && !country.empty()) return city + ", " + country;
        if (!country.empty()) return country;
    } catch (...) {}
    return "Unknown";
}

// Timestamp helpers

std::string SupabaseClient::currentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#ifdef _WIN32
    gmtime_s(&tmBuf, &now);
#else
    gmtime_r(&now, &tmBuf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return std::string(buf);
}

std::string SupabaseClient::futureTimestamp(int secondsFromNow) {
    std::time_t t = std::time(nullptr) + secondsFromNow;
    std::tm tmBuf{};
#ifdef _WIN32
    gmtime_s(&tmBuf, &t);
#else
    gmtime_r(&t, &tmBuf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return std::string(buf);
}

#include "../../include/cache/CandidateCache.h"
#include "../../include/cache/RedisClient.h"

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

CandidateCache& CandidateCache::instance() {
    static CandidateCache inst;
    return inst;
}

// ─────────────────────────────────────────────────────────────────────────────
// Key helper
// ─────────────────────────────────────────────────────────────────────────────

std::string CandidateCache::makeKey(const std::string& electionId) {
    return "candidates:" + electionId;
}

// ─────────────────────────────────────────────────────────────────────────────
// get — returns "" if not cached or Redis unavailable
// ─────────────────────────────────────────────────────────────────────────────

std::string CandidateCache::get(const std::string& electionId) {
    auto& redis = RedisClient::instance();
    if (!redis.isAvailable()) return "";
    return redis.get(makeKey(electionId));
}

// ─────────────────────────────────────────────────────────────────────────────
// set — stores candidatesJson with TTL_SECONDS expiry
// ─────────────────────────────────────────────────────────────────────────────

void CandidateCache::set(const std::string& electionId, const std::string& candidatesJson) {
    auto& redis = RedisClient::instance();
    if (!redis.isAvailable()) return;
    redis.set(makeKey(electionId), candidatesJson, TTL_SECONDS);
}

// ─────────────────────────────────────────────────────────────────────────────
// invalidate — deletes the cache entry
// ─────────────────────────────────────────────────────────────────────────────

void CandidateCache::invalidate(const std::string& electionId) {
    auto& redis = RedisClient::instance();
    if (!redis.isAvailable()) return;
    redis.del(makeKey(electionId));
}

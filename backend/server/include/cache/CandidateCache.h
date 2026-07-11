#pragma once
#include <string>

// ==================
// CandidateCache - thin wrapper around RedisClient for candidate list caching.
// Key pattern : candidates:{electionId}
// TTL         : 30 seconds
// Gracefully does nothing when Redis is unavailable.
// ==================

class CandidateCache {
public:
    static CandidateCache& instance();

    // Return cached candidates JSON string for an election ("" if not cached).
    std::string get(const std::string& electionId);

    // Cache candidates JSON for an election (TTL 30 s).
    void set(const std::string& electionId, const std::string& candidatesJson);

    // Invalidate cache entry when candidates change.
    void invalidate(const std::string& electionId);

    // Non-copyable
    CandidateCache(const CandidateCache&) = delete;
    CandidateCache& operator=(const CandidateCache&) = delete;

private:
    CandidateCache() = default;

    static constexpr int TTL_SECONDS = 30;
    static std::string makeKey(const std::string& electionId);
};

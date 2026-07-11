#pragma once
// ============================================================
// Metrics.h - Step 11: Prometheus metrics collector
//
// Tracks (all thread-safe):
//   http_requests_total{status}      - request count by HTTP status class
//   http_request_duration_seconds    - latency histogram (bucket + sum + count)
//   active_connections               - currently open client connections
//   redis_cache_hits_total           - Redis GET hits
//   redis_cache_misses_total         - Redis GET misses
//
// Expose with:  Metrics::instance().renderPrometheus()
// Returns Prometheus text exposition format, served from GET /metrics.
// ============================================================

#include <atomic>
#include <array>
#include <mutex>
#include <string>
#include <chrono>
#include <sstream>

class Metrics {
public:
    static Metrics& instance() {
        static Metrics m;
        return m;
    }

    // ── Call sites ──────────────────────────────────────────────────────────

    // Increment request counter by HTTP status category (2xx,3xx,4xx,5xx)
    void recordRequest(int statusCode) {
        int bucket = statusCode / 100;  // 2,3,4,5
        if (bucket >= 2 && bucket <= 5)
            requestsByStatus_[bucket - 2].fetch_add(1, std::memory_order_relaxed);
        requestsTotal_.fetch_add(1, std::memory_order_relaxed);
    }

    // Record latency in seconds - updates histogram buckets, sum, count
    void recordLatency(double seconds) {
        // Buckets: 1ms,5ms,10ms,25ms,50ms,100ms,250ms,500ms,1s,2.5s,+Inf
        static constexpr double BOUNDS[] = {
            0.001, 0.005, 0.010, 0.025, 0.050,
            0.100, 0.250, 0.500, 1.0,   2.5
        };
        for (int i = 0; i < BUCKET_COUNT - 1; ++i) {
            if (seconds <= BOUNDS[i]) {
                latencyBuckets_[i].fetch_add(1, std::memory_order_relaxed);
            }
        }
        latencyBuckets_[BUCKET_COUNT - 1].fetch_add(1, std::memory_order_relaxed); // +Inf

        // Accumulate sum with a simple mutex - avoids CAS spin on double
        {
            std::lock_guard<std::mutex> lk(sumMutex_);
            latencySumRaw_ += seconds;
        }

        latencyCount_.fetch_add(1, std::memory_order_relaxed);
    }

    void incActiveConnections()  { activeConnections_.fetch_add(1, std::memory_order_relaxed); }
    void decActiveConnections()  { activeConnections_.fetch_sub(1, std::memory_order_relaxed); }

    void recordCacheHit()   { cacheHits_.fetch_add(1,   std::memory_order_relaxed); }
    void recordCacheMiss()  { cacheMisses_.fetch_add(1, std::memory_order_relaxed); }

    // ── Render ──────────────────────────────────────────────────────────────

    // Returns Prometheus exposition format text (no JSON needed)
    std::string renderPrometheus() const {
        std::ostringstream out;

        // ── http_requests_total ──────────────────────────────────────────
        out << "# HELP http_requests_total Total HTTP requests by status class\n";
        out << "# TYPE http_requests_total counter\n";
        static const char* statuses[] = {"2xx","3xx","4xx","5xx"};
        for (int i = 0; i < 4; ++i) {
            out << "http_requests_total{status=\"" << statuses[i] << "\"} "
                << requestsByStatus_[i].load(std::memory_order_relaxed) << "\n";
        }

        // ── http_request_duration_seconds (histogram) ────────────────────
        out << "# HELP http_request_duration_seconds HTTP request latency\n";
        out << "# TYPE http_request_duration_seconds histogram\n";
        static constexpr double BOUNDS[] = {
            0.001, 0.005, 0.010, 0.025, 0.050,
            0.100, 0.250, 0.500, 1.0,   2.5
        };
        for (int i = 0; i < BUCKET_COUNT - 1; ++i) {
            out << "http_request_duration_seconds_bucket{le=\"" << BOUNDS[i] << "\"} "
                << latencyBuckets_[i].load(std::memory_order_relaxed) << "\n";
        }
        out << "http_request_duration_seconds_bucket{le=\"+Inf\"} "
            << latencyBuckets_[BUCKET_COUNT - 1].load(std::memory_order_relaxed) << "\n";
        out << "http_request_duration_seconds_sum ";
        {
            std::lock_guard<std::mutex> lk(sumMutex_);
            out << latencySumRaw_;
        }
        out << "\n";
        out << "http_request_duration_seconds_count "
            << latencyCount_.load(std::memory_order_relaxed) << "\n";

        // ── active_connections ───────────────────────────────────────────
        out << "# HELP active_connections Number of currently open client connections\n";
        out << "# TYPE active_connections gauge\n";
        out << "active_connections "
            << activeConnections_.load(std::memory_order_relaxed) << "\n";

        // ── redis_cache_hits_total ───────────────────────────────────────
        out << "# HELP redis_cache_hits_total Redis cache hits\n";
        out << "# TYPE redis_cache_hits_total counter\n";
        out << "redis_cache_hits_total "
            << cacheHits_.load(std::memory_order_relaxed) << "\n";

        out << "# HELP redis_cache_misses_total Redis cache misses\n";
        out << "# TYPE redis_cache_misses_total counter\n";
        out << "redis_cache_misses_total "
            << cacheMisses_.load(std::memory_order_relaxed) << "\n";

        return out.str();
    }

private:
    Metrics() = default;
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    static constexpr int BUCKET_COUNT = 11;  // 10 finite bounds + +Inf

    // 4 status classes: 2xx[0], 3xx[1], 4xx[2], 5xx[3]
    std::array<std::atomic<uint64_t>, 4> requestsByStatus_{};
    std::atomic<uint64_t>                requestsTotal_{0};

    std::array<std::atomic<uint64_t>, BUCKET_COUNT> latencyBuckets_{};
    // latencySum_ protected by sumMutex_ - avoids CAS spin on double under load
    mutable std::mutex    sumMutex_;
    double                latencySumRaw_{0.0};
    std::atomic<uint64_t> latencyCount_{0};

    std::atomic<int64_t>  activeConnections_{0};
    std::atomic<uint64_t> cacheHits_{0};
    std::atomic<uint64_t> cacheMisses_{0};
};

// ── RAII latency timer ────────────────────────────────────────────────────────
// Usage:
//   {
//     LatencyTimer t(statusCode);   // starts on construction
//     ...handle request...
//   }                               // records on destruction
struct LatencyTimer {
    explicit LatencyTimer() : start_(std::chrono::steady_clock::now()) {
        Metrics::instance().incActiveConnections();
    }
    void finish(int statusCode) {
        if (done_) return;
        done_ = true;
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start_).count();
        Metrics::instance().recordLatency(elapsed);
        Metrics::instance().recordRequest(statusCode);
        Metrics::instance().decActiveConnections();
    }
    ~LatencyTimer() { finish(0); }  // 0 → no status bucket if not finished cleanly

private:
    std::chrono::steady_clock::time_point start_;
    bool done_{false};
};

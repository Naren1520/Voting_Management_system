#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// ==================
// ThreadPool - fixed-size pool that processes integer file descriptors.
//
// Fix #2: Bounded queue with backpressure.
//   enqueue() returns false when the queue is full so the caller can
//   immediately close the fd with a 503 instead of letting it queue forever.
//
// Fix #3: Handler set in constructor - no race between setHandler() and
//   the first worker waking up.
// ==================

class ThreadPool {
public:
    // handler   - called once per fd by a worker thread
    // numThreads - number of worker threads
    // maxQueue  - max pending fds; enqueue() returns false when exceeded
    explicit ThreadPool(size_t numThreads,
                        std::function<void(int)> handler,
                        size_t maxQueue = 0);  // 0 → numThreads * 8
    ~ThreadPool();

    // Push a client fd onto the work queue.
    // Returns false (and does NOT take ownership of fd) if queue is full.
    bool enqueue(int fd);

    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    void workerLoop();

    std::vector<std::thread>   workers_;
    std::queue<int>            queue_;
    std::mutex                 mutex_;
    std::condition_variable    cv_;
    std::atomic<bool>          stop_{false};
    std::function<void(int)>   handler_;
    size_t                     maxQueue_;
};

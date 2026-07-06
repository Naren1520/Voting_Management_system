#include "../../include/core/ThreadPool.h"
#include "../../include/core/Logger.h"
#include <unistd.h>

// Constructor — starts worker threads immediately with the handler already set.
// Fix #3: handler is set before any thread can wake and dequeue.

ThreadPool::ThreadPool(size_t numThreads,
                       std::function<void(int)> handler,
                       size_t maxQueue)
    : handler_(std::move(handler))
    , maxQueue_(maxQueue > 0 ? maxQueue : numThreads * 8)
{
    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
    LOG_INFO("ThreadPool started: " + std::to_string(numThreads) +
             " workers, max queue " + std::to_string(maxQueue_));
}

// Destructor — drain stop signal, join all threads

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_.store(true);
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

// enqueue — returns false if queue is full (caller must close the fd).
// Fix #2: bounded queue prevents OOM under spike load.

bool ThreadPool::enqueue(int fd) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.size() >= maxQueue_) {
        return false;   // caller sends 503 and closes fd
    }
    queue_.push(fd);
    lock.unlock();
    cv_.notify_one();
    return true;
}

// workerLoop

void ThreadPool::workerLoop() {
    while (true) {
        int fd = -1;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stop_.load() || !queue_.empty();
            });
            if (stop_.load() && queue_.empty()) return;
            fd = queue_.front();
            queue_.pop();
        }
        if (fd >= 0 && handler_) {
            try {
                handler_(fd);
            } catch (const std::exception& e) {
                LOG_ERROR("Worker exception on fd " + std::to_string(fd) +
                          ": " + e.what());
                ::close(fd);
            } catch (...) {
                LOG_ERROR("Unknown worker exception on fd " + std::to_string(fd));
                ::close(fd);
            }
        }
    }
}

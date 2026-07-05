#pragma once
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

class ThreadPool;

// ============================================================================
// EpollServer — epoll-based, edge-triggered, non-blocking HTTP server.
//
// Fix #1 (background cleanup): a dedicated thread runs every 10 minutes to
//   DELETE expired sessions from Supabase. This removes the need to run the
//   DELETE on every cache-miss in validateToken(), which was flooding the DB
//   with writes on the hot authentication path.
// ============================================================================

class EpollServer {
public:
    explicit EpollServer(int port = 8080);
    ~EpollServer();   // joins cleanup thread

    bool start();

    EpollServer(const EpollServer&) = delete;
    EpollServer& operator=(const EpollServer&) = delete;

private:
    void handleClient(int fd);
    std::string route(const class HttpRequest& req);

    static std::vector<std::string> splitPath(const std::string& path);
    static int  makeNonBlocking(int fd);
    static bool setSocketOptions(int fd);

    // Fix #1: background expired-session cleanup
    void startCleanupTimer();
    void cleanupLoop();

    int port_;
    int listenFd_{-1};
    int epollFd_{-1};
    std::unique_ptr<ThreadPool> pool_;

    // Cleanup timer
    std::thread          cleanupThread_;
    std::atomic<bool>    cleanupStop_{false};
};

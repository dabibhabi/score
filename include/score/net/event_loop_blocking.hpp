#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <score/net/event_loop.hpp>
#include <thread>

namespace score::net {

/// v1 concurrency backend: one worker thread per registered fd. Each worker
/// poll()s its fd (with a short timeout so it can notice cancellation) and
/// invokes the callback when the fd is readable.
class BlockingThreadEventLoop : public EventLoop {
public:
    BlockingThreadEventLoop() = default;
    ~BlockingThreadEventLoop() override;

    BlockingThreadEventLoop(const BlockingThreadEventLoop&)            = delete;
    BlockingThreadEventLoop& operator=(const BlockingThreadEventLoop&) = delete;

    void register_io(SocketHandle fd, IoEvent event, IoCallback cb) override;
    void unregister(SocketHandle fd) override;
    void run() override;
    void stop() override;

private:
    struct Watch {
        std::thread                        worker;
        std::shared_ptr<std::atomic<bool>> cancel;
    };

    std::mutex                    mu_;
    std::condition_variable       cv_;
    std::map<SocketHandle, Watch> watches_;
    bool                          stopping_ = false;
};

}  // namespace score::net

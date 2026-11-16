#include <score/core/exceptions.hpp>
#include <score/net/event_loop_blocking.hpp>

namespace score::net {

BlockingThreadEventLoop::~BlockingThreadEventLoop() {
    stop();
}

void BlockingThreadEventLoop::register_io(SocketHandle fd, IoEvent event, IoCallback cb) {
    // TODO: spawn a worker that loops { poll(fd, POLLIN, 100ms); if ready -> cb(fd, kReadable); }
    //       until its cancel flag is set. Store the Watch in watches_ under mu_.
    // start registor io 
    if (event != IoEvent::kReadable) {
        throw score::NotImplementedError("score::net::BlockingThreadEventLoop::register_io (non-readable)");
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (stopping_) {
        throw score::NotImplementedError("score::net::BlockingThreadEventLoop::register_io (stopping)");
    }
    if (watches_.count(fd) != 0) {
        throw score::NotImplementedError("score::net::BlockingThreadEventLoop::register_io (duplicate fd)");
    }

    auto cancel = std::make_shared<std::atomic<bool>>(false);

    std::thread worker([fd, cancel, cb]() {
        while (!cancel->load()) {
            // Simulate blocking I/O wait
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            cb(fd, IoEvent::kReadable);
        }
    });
    worker.detach();
}

void BlockingThreadEventLoop::unregister(SocketHandle /*fd*/) {
    // TODO: move the Watch out of watches_ under mu_, set cancel, then (outside the lock)
    //       join the worker, or detach it if called from the worker's own thread.
    throw score::NotImplementedError("score::net::BlockingThreadEventLoop::unregister");
}

void BlockingThreadEventLoop::run() {
    // TODO: block on cv_ until stopping_ is true.
    throw score::NotImplementedError("score::net::BlockingThreadEventLoop::run");
}

void BlockingThreadEventLoop::stop() {
    // TODO: set stopping_, notify cv_, cancel and join every worker.
    throw score::NotImplementedError("score::net::BlockingThreadEventLoop::stop");
}

}  // namespace score::net

#pragma once

#include <functional>

namespace score::net {

using SocketHandle = int;

enum class IoEvent { kReadable, kWritable };

using IoCallback = std::function<void(SocketHandle, IoEvent)>;

/// Platform-agnostic reactor interface. Server/Connection depend only on
/// this, never on a concrete backend, so the concurrency model (blocking
/// thread-per-connection, epoll, io_uring, ...) can be swapped without
/// touching them.
class EventLoop {
public:
    virtual ~EventLoop() = default;

    /// Registers `cb` to be invoked when `fd` becomes ready for `event`.
    virtual void register_io(SocketHandle fd, IoEvent event, IoCallback cb) = 0;

    /// Stops watching `fd`.
    virtual void unregister(SocketHandle fd) = 0;

    /// Runs the loop until stop() is called. Blocks the calling thread.
    virtual void run() = 0;

    /// Signals run() to return.
    virtual void stop() = 0;
};

}  // namespace score::net

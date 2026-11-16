#pragma once

#include <memory>
#include <score/net/codec.hpp>
#include <score/net/event_loop.hpp>

namespace score::net {

/// One accepted client connection: owns the socket and a codec, and
/// registers itself with an EventLoop for readability/writability.
class Connection {
public:
    Connection(SocketHandle fd, EventLoop& loop, std::unique_ptr<FrameCodec> codec);
    ~Connection();

    Connection(const Connection&)            = delete;
    Connection& operator=(const Connection&) = delete;

    /// Queues `frame` for writing to the socket.
    void send(const Frame& frame);

    /// Closes the underlying socket and unregisters from the EventLoop.
    void close();

private:
    SocketHandle                fd_;
    EventLoop&                  loop_;
    std::unique_ptr<FrameCodec> codec_;
};

}  // namespace score::net

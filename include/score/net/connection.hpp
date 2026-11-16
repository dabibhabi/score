#pragma once

#include <functional>
#include <memory>
#include <score/net/codec.hpp>
#include <score/net/event_loop.hpp>

namespace score::net {

class Connection;

using FrameHandler = std::function<void(Connection&, const Frame&)>;

/// One accepted client connection: owns the socket and a codec, and
/// registers itself with an EventLoop for readability.
class Connection {
public:
    Connection(SocketHandle fd, EventLoop& loop, std::unique_ptr<FrameCodec> codec);
    ~Connection();

    Connection(const Connection&)            = delete;
    Connection& operator=(const Connection&) = delete;

    /// Sets the callback invoked once per decoded inbound Frame.
    void set_frame_handler(FrameHandler handler);

    /// Encodes `frame` and writes it to the socket, blocking until all bytes are sent.
    /// Throws ScoreException if the connection is closed or the write fails.
    void send(const Frame& frame);

    /// Unregisters from the EventLoop and closes the socket. Idempotent.
    void close();

    [[nodiscard]] bool is_open() const noexcept { return fd_ >= 0; }

private:
    /// Reads available bytes, feeds the codec, and dispatches decoded frames.
    /// Closes the connection on EOF, I/O error, or a malformed frame.
    void on_readable();

    SocketHandle                fd_;
    EventLoop&                  loop_;
    std::unique_ptr<FrameCodec> codec_;
    FrameHandler                handler_;
};

}  // namespace score::net

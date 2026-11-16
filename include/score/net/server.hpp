#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <score/net/codec.hpp>
#include <score/net/connection.hpp>
#include <score/net/event_loop.hpp>
#include <vector>

namespace score::net {

using CodecFactory = std::function<std::unique_ptr<FrameCodec>()>;

/// Listens on a TCP port and accepts connections, handing each one an
/// EventLoop reference and a freshly-constructed codec from `CodecFactory`.
/// Depends only on the EventLoop/FrameCodec interfaces, so the concurrency
/// backend (blocking thread-per-connection, epoll, io_uring) and the wire
/// format can each be swapped independently of this class.
class Server {
public:
    Server(EventLoop& loop, CodecFactory codec_factory);
    ~Server();

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    /// Sets the handler installed on every accepted Connection. Call before listen().
    void set_frame_handler(FrameHandler handler);

    /// Binds and listens on `port` (0 picks an ephemeral port) and registers
    /// the listening socket with the EventLoop to accept connections.
    void listen(std::uint16_t port);

    /// Port the server is bound to; 0 if not listening.
    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

    /// Stops accepting new connections and closes existing ones.
    void stop();

private:
    /// Accepts one pending client and wraps it in a Connection.
    void on_acceptable();

    EventLoop&                                loop_;
    CodecFactory                              codec_factory_;
    FrameHandler                              handler_;
    SocketHandle                              listen_fd_ = -1;
    std::uint16_t                             port_      = 0;
    std::vector<std::unique_ptr<Connection>> connections_;
};

}  // namespace score::net

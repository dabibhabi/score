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

    /// Binds and listens on `port`.
    void listen(std::uint16_t port);

    /// Stops accepting new connections and closes existing ones.
    void stop();

private:
    EventLoop&                                loop_;
    CodecFactory                              codec_factory_;
    std::vector<std::unique_ptr<Connection>> connections_;
};

}  // namespace score::net

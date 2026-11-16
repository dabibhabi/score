#include <score/core/exceptions.hpp>
#include <score/net/connection.hpp>
#include <utility>

namespace score::net {

Connection::Connection(SocketHandle fd, EventLoop& loop, std::unique_ptr<FrameCodec> codec)
    : fd_(fd), loop_(loop), codec_(std::move(codec)) {}

Connection::~Connection() = default;

void Connection::send(const Frame& /*frame*/) {
    // TODO: implement
    throw score::NotImplementedError("score::net::Connection::send");
}

void Connection::close() {
    // TODO: implement
    throw score::NotImplementedError("score::net::Connection::close");
}

}  // namespace score::net

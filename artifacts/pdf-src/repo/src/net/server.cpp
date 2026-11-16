#include <score/core/exceptions.hpp>
#include <score/net/server.hpp>
#include <utility>

namespace score::net {

Server::Server(EventLoop& loop, CodecFactory codec_factory) : loop_(loop), codec_factory_(std::move(codec_factory)) {}

Server::~Server() = default;

void Server::listen(std::uint16_t /*port*/) {
    // TODO: implement
    throw score::NotImplementedError("score::net::Server::listen");
}

void Server::stop() {
    // TODO: implement
    throw score::NotImplementedError("score::net::Server::stop");
}

}  // namespace score::net

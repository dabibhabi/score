#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <score/core/exceptions.hpp>
#include <score/net/server.hpp>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace score::net {

Server::Server(EventLoop& loop, CodecFactory codec_factory) : loop_(loop), codec_factory_(std::move(codec_factory)) {}

Server::~Server() {
    stop();
}

void Server::set_frame_handler(FrameHandler handler) {
    handler_ = std::move(handler);
}

void Server::listen(std::uint16_t port) {
    if (listen_fd_ >= 0) {
        throw ScoreException("score::net::Server::listen: already listening");
    }

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw ScoreException("score::net::Server::listen: socket() failed");
    }

    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    socklen_t addr_len = sizeof(addr);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || ::listen(fd, SOMAXCONN) < 0 ||
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
        ::close(fd);
        throw ScoreException("score::net::Server::listen: bind/listen failed");
    }

    listen_fd_ = fd;
    port_      = ntohs(addr.sin_port);
    loop_.register_io(listen_fd_, IoEvent::kReadable, [this](SocketHandle, IoEvent) { on_acceptable(); });
}

void Server::stop() {
    connections_.clear();  // each Connection unregisters and closes its socket
    if (listen_fd_ >= 0) {
        loop_.unregister(listen_fd_);
        ::close(listen_fd_);
        listen_fd_ = -1;
        port_      = 0;
    }
}

void Server::on_acceptable() {
    const int client = ::accept(listen_fd_, nullptr, nullptr);
    if (client < 0) {
        return;  // EINTR / transient failure; the loop will fire again if still pending
    }

    // Drop connections that have since closed so the vector doesn't grow unbounded.
    std::erase_if(connections_, [](const std::unique_ptr<Connection>& c) { return !c->is_open(); });

    auto conn = std::make_unique<Connection>(client, loop_, codec_factory_());
    if (handler_) {
        conn->set_frame_handler(handler_);
    }
    connections_.push_back(std::move(conn));
}

}  // namespace score::net

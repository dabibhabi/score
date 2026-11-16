#include <cerrno>
#include <score/core/exceptions.hpp>
#include <score/net/connection.hpp>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

#ifdef __APPLE__
#    define SCORE_SEND_FLAGS 0  // SIGPIPE is suppressed via SO_NOSIGPIPE below
#else
#    define SCORE_SEND_FLAGS MSG_NOSIGNAL
#endif

namespace score::net {

Connection::Connection(SocketHandle fd, EventLoop& loop, std::unique_ptr<FrameCodec> codec)
    : fd_(fd), loop_(loop), codec_(std::move(codec)) {
#ifdef __APPLE__
    int one = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    loop_.register_io(fd_, IoEvent::kReadable, [this](SocketHandle, IoEvent) { on_readable(); });
}

Connection::~Connection() {
    close();
}

void Connection::set_frame_handler(FrameHandler handler) {
    handler_ = std::move(handler);
}

void Connection::send(const Frame& frame) {
    if (!is_open()) {
        throw ScoreException("score::net::Connection::send: connection is closed");
    }

    const std::vector<std::uint8_t> bytes = codec_->encode(frame);
    std::size_t                     sent  = 0;
    while (sent < bytes.size()) {
        const ssize_t n = ::send(fd_, bytes.data() + sent, bytes.size() - sent, SCORE_SEND_FLAGS);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw ScoreException("score::net::Connection::send: socket write failed");
        }
        sent += static_cast<std::size_t>(n);
    }
}

void Connection::close() {
    if (fd_ < 0) {
        return;
    }
    loop_.unregister(fd_);
    ::close(fd_);
    fd_ = -1;
}

void Connection::on_readable() {
    if (!is_open()) {
        return;
    }

    std::uint8_t  buf[4096];
    const ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
    if (n == 0) {
        close();
        return;
    }
    if (n < 0) {
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            close();
        }
        return;
    }

    try {
        codec_->feed(buf, static_cast<std::size_t>(n));
        while (auto frame = codec_->try_decode()) {
            if (handler_) {
                handler_(*this, *frame);
            }
            if (!is_open()) {
                return;  // handler closed us
            }
        }
    } catch (const ScoreException&) {
        close();  // protocol violation: drop the peer
    }
}

}  // namespace score::net

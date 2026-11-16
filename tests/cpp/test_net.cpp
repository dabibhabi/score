#include <arpa/inet.h>
#include <catch2/catch_test_macros.hpp>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <score/core/exceptions.hpp>
#include <score/net/json_codec.hpp>
#include <score/net/server.hpp>
#include <sys/socket.h>
#include <unistd.h>

namespace score::net {

namespace {

/// Records registrations so tests can drive readiness callbacks by hand.
class FakeEventLoop : public EventLoop {
public:
    void register_io(SocketHandle fd, IoEvent, IoCallback cb) override { callbacks_[fd] = std::move(cb); }
    void unregister(SocketHandle fd) override { callbacks_.erase(fd); }
    void run() override {}
    void stop() override {}

    void fire(SocketHandle fd) { callbacks_.at(fd)(fd, IoEvent::kReadable); }
    [[nodiscard]] bool watching(SocketHandle fd) const { return callbacks_.count(fd) != 0; }
    [[nodiscard]] SocketHandle only_fd_except(SocketHandle skip) const {
        for (const auto& [fd, cb] : callbacks_) {
            if (fd != skip) {
                return fd;
            }
        }
        return -1;
    }

private:
    std::map<SocketHandle, IoCallback> callbacks_;
};

int connect_client(std::uint16_t port) {
    const int       fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in     addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    return fd;
}

void write_all(int fd, const std::vector<std::uint8_t>& bytes) {
    REQUIRE(::send(fd, bytes.data(), bytes.size(), 0) == static_cast<ssize_t>(bytes.size()));
}

std::vector<std::uint8_t> read_n(int fd, std::size_t n) {
    std::vector<std::uint8_t> out(n);
    std::size_t               got = 0;
    while (got < n) {
        const ssize_t r = ::recv(fd, out.data() + got, n - got, 0);
        REQUIRE(r > 0);
        got += static_cast<std::size_t>(r);
    }
    return out;
}

}  // namespace

TEST_CASE("JsonFrameCodec round-trips a frame", "[net][codec]") {
    JsonFrameCodec codec;
    const Frame    in{{{"symbol", "AAPL"}, {"price", 189.5}}};

    const auto bytes = codec.encode(in);
    REQUIRE(bytes.size() > 4);
    codec.feed(bytes.data(), bytes.size());

    const auto out = codec.try_decode();
    REQUIRE(out.has_value());
    REQUIRE(out->payload == in.payload);
    REQUIRE_FALSE(codec.try_decode().has_value());
}

TEST_CASE("JsonFrameCodec reassembles fragmented and coalesced frames", "[net][codec]") {
    JsonFrameCodec codec;
    auto           a = codec.encode(Frame{{{"n", 1}}});
    auto           b = codec.encode(Frame{{{"n", 2}}});

    // Byte-by-byte delivery of the first frame: nothing decodes until the last byte.
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE_FALSE(codec.try_decode().has_value());
        codec.feed(&a[i], 1);
    }
    REQUIRE(codec.try_decode()->payload["n"] == 1);

    // Two frames in one feed decode in order.
    a.insert(a.end(), b.begin(), b.end());
    codec.feed(a.data(), a.size());
    REQUIRE(codec.try_decode()->payload["n"] == 1);
    REQUIRE(codec.try_decode()->payload["n"] == 2);
    REQUIRE_FALSE(codec.try_decode().has_value());
}

TEST_CASE("JsonFrameCodec rejects oversized and malformed frames", "[net][codec]") {
    JsonFrameCodec codec;

    const std::uint8_t huge[] = {0xFF, 0xFF, 0xFF, 0xFF};
    codec.feed(huge, sizeof(huge));
    REQUIRE_THROWS_AS(codec.try_decode(), score::DomainError);

    JsonFrameCodec    bad;
    const std::string junk = "{not json";
    std::vector<std::uint8_t> bytes = {0, 0, 0, static_cast<std::uint8_t>(junk.size())};
    bytes.insert(bytes.end(), junk.begin(), junk.end());
    bad.feed(bytes.data(), bytes.size());
    REQUIRE_THROWS_AS(bad.try_decode(), score::DomainError);
    REQUIRE_FALSE(bad.try_decode().has_value());  // buffer was consumed, not wedged
}

TEST_CASE("Server accepts a loopback client and echoes frames", "[net][server]") {
    FakeEventLoop loop;
    Server        server(loop, [] { return std::make_unique<JsonFrameCodec>(); });
    server.set_frame_handler([](Connection& conn, const Frame& f) { conn.send(f); });
    server.listen(0);
    REQUIRE(server.port() != 0);

    const int client = connect_client(server.port());
    const int listen_fd = loop.only_fd_except(-1);
    loop.fire(listen_fd);  // accept
    const int conn_fd = loop.only_fd_except(listen_fd);
    REQUIRE(conn_fd >= 0);

    JsonFrameCodec client_codec;
    const Frame    msg{{{"ping", 42}}};
    write_all(client, client_codec.encode(msg));
    loop.fire(conn_fd);  // read + echo

    const auto expected = client_codec.encode(msg);
    const auto echoed   = read_n(client, expected.size());
    REQUIRE(echoed == expected);

    ::close(client);
    loop.fire(conn_fd);  // EOF -> connection closes itself
    REQUIRE_FALSE(loop.watching(conn_fd));

    server.stop();
    REQUIRE(server.port() == 0);
    REQUIRE_FALSE(loop.watching(listen_fd));
}

TEST_CASE("Connection drops a peer that sends a malformed frame", "[net][server]") {
    FakeEventLoop loop;
    Server        server(loop, [] { return std::make_unique<JsonFrameCodec>(); });
    server.listen(0);

    const int client    = connect_client(server.port());
    const int listen_fd = loop.only_fd_except(-1);
    loop.fire(listen_fd);
    const int conn_fd = loop.only_fd_except(listen_fd);

    write_all(client, {0xFF, 0xFF, 0xFF, 0xFF});
    loop.fire(conn_fd);
    REQUIRE_FALSE(loop.watching(conn_fd));
    ::close(client);
}

TEST_CASE("Server::listen rejects a second call", "[net][server]") {
    FakeEventLoop loop;
    Server        server(loop, [] { return std::make_unique<JsonFrameCodec>(); });
    server.listen(0);
    REQUIRE_THROWS_AS(server.listen(0), score::ScoreException);
}

}  // namespace score::net

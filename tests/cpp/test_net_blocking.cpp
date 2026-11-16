#include <catch2/catch_test_macros.hpp>
#include <score/net/event_loop_blocking.hpp>
#include <score/net/json_codec.hpp>
#include <score/net/server.hpp>

namespace score::net {

// TODO: loopback echo round-trip with a real BlockingThreadEventLoop:
//   run loop.run() on a std::thread, Server::listen(0), connect a client, send a framed
//   message, read the echo (use SO_RCVTIMEO so a bug fails instead of hanging), then
//   server.stop(); loop.stop(); join. Only assert on the main test thread.
TEST_CASE("BlockingThreadEventLoop echoes a frame over loopback", "[net][blocking][.pending]") {
    SUCCEED("not implemented yet");
}

}  // namespace score::net

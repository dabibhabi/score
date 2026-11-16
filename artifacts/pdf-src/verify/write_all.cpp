#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
bool write_all(int fd, const std::uint8_t* p, std::size_t n) {
    while (n > 0) {
        ssize_t k = ::send(fd, p, n, 0);
        if (k < 0) {
            if (errno == EINTR) continue;   // interrupted by a signal: retry
            return false;                   // real error (EPIPE, ECONNRESET, ...)
        }
        p += k;                              // k may be < n: advance and loop
        n -= static_cast<std::size_t>(k);
    }
    return true;
}
int main(){ int sv[2]; socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  std::vector<std::uint8_t> big(4<<20, 0xAB); std::size_t got=0;
  std::thread r([&]{ std::uint8_t b[65536]; ssize_t k; while((k=read(sv[1],b,sizeof b))>0) got+=k; });
  bool ok=write_all(sv[0], big.data(), big.size()); shutdown(sv[0], SHUT_WR); r.join();
  std::printf("write_all ok=%d sent=%zu received=%zu\n", ok, big.size(), got); }

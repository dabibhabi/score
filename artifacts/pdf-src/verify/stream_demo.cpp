// Illustration (not repository code): TCP read boundaries are not write boundaries.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);                       // let the kernel pick a port
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(lfd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    listen(lfd, 16);
    socklen_t len = sizeof addr;
    getsockname(lfd, reinterpret_cast<sockaddr*>(&addr), &len);

    for (int pause_ms : {50, 0}) {
        std::thread client([&] {
            int c = socket(AF_INET, SOCK_STREAM, 0);
            setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
            connect(c, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
            unsigned char frame[44] = {0x00, 0x00, 0x00, 0x28};  // + 40 payload bytes
            const int cuts[] = {0, 3, 23, 44};
            for (int i = 0; i < 3; ++i) {
                send(c, frame + cuts[i], cuts[i + 1] - cuts[i], 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(pause_ms));
            }
            close(c);
        });
        int fd = accept(lfd, nullptr, nullptr);
        if (pause_ms == 0) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        unsigned char buf[4096];
        std::printf("writes 3+20+21 with %d ms pauses -> recv() returned:", pause_ms);
        for (ssize_t n; (n = recv(fd, buf, sizeof buf, 0)) > 0;) std::printf(" %zd", n);
        std::printf(" 0 (EOF)\n");
        close(fd);
        client.join();
    }
    close(lfd);
}

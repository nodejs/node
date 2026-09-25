// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

// Helper RAII over unix udp client socket.
// Will throw on construction if the socket creation failed.

#ifdef _WIN32
    #error "include udp_client-windows.h instead"
#endif

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <spdlog/common.h>
#include <spdlog/details/os.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

namespace spdlog {
namespace details {

class udp_client {
    static constexpr int TX_BUFFER_SIZE = 1024 * 10;
    int socket_ = -1;
    struct sockaddr_in sockAddr_;

    void cleanup_() {
        if (socket_ != -1) {
            ::close(socket_);
            socket_ = -1;
        }
    }

public:
    udp_client(const std::string &host, uint16_t port) {
        socket_ = ::socket(PF_INET, SOCK_DGRAM, 0);
        if (socket_ < 0) {
            throw_spdlog_ex("error: Create Socket Failed!");
        }

        int option_value = TX_BUFFER_SIZE;
        if (::setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                         reinterpret_cast<const char *>(&option_value), sizeof(option_value)) < 0) {
            cleanup_();
            throw_spdlog_ex("error: setsockopt(SO_SNDBUF) Failed!");
        }

        sockAddr_.sin_family = AF_INET;
        sockAddr_.sin_port = htons(port);

        if (::inet_aton(host.c_str(), &sockAddr_.sin_addr) == 0) {
            cleanup_();
            throw_spdlog_ex("error: Invalid address!");
        }

        ::memset(sockAddr_.sin_zero, 0x00, sizeof(sockAddr_.sin_zero));
    }

    ~udp_client() { cleanup_(); }

    int fd() const { return socket_; }

    // Send exactly n_bytes of the given data.
    // On error close the connection and throw.
    void send(const char *data, size_t n_bytes) {
        ssize_t toslen = 0;
        socklen_t tolen = sizeof(struct sockaddr);
        if ((toslen = ::sendto(socket_, data, n_bytes, 0, (struct sockaddr *)&sockAddr_, tolen)) ==
            -1) {
            throw_spdlog_ex("sendto(2) failed", errno);
        }
    }
};
}  // namespace details
}  // namespace spdlog

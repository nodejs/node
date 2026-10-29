// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

// Helper RAII over winsock udp client socket.
// Will throw on construction if socket creation failed.

#include <spdlog/common.h>
#include <spdlog/details/os.h>
#include <spdlog/details/windows_include.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#if defined(_MSC_VER)
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")
#endif

namespace spdlog {
namespace details {
class udp_client {
    static constexpr int TX_BUFFER_SIZE = 1024 * 10;
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in addr_ = {};

    static void init_winsock_() {
        WSADATA wsaData;
        auto rv = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (rv != 0) {
            throw_winsock_error_("WSAStartup failed", ::WSAGetLastError());
        }
    }

    static void throw_winsock_error_(const std::string &msg, int last_error) {
        char buf[512];
        ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                         last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                         (sizeof(buf) / sizeof(char)), NULL);

        throw_spdlog_ex(fmt_lib::format("udp_sink - {}: {}", msg, buf));
    }

    void cleanup_() {
        if (socket_ != INVALID_SOCKET) {
            ::closesocket(socket_);
        }
        socket_ = INVALID_SOCKET;
        ::WSACleanup();
    }

public:
    udp_client(const std::string &host, uint16_t port) {
        init_winsock_();

        addr_.sin_family = PF_INET;
        addr_.sin_port = htons(port);
        addr_.sin_addr.s_addr = INADDR_ANY;
        if (InetPtonA(PF_INET, host.c_str(), &addr_.sin_addr.s_addr) != 1) {
            int last_error = ::WSAGetLastError();
            ::WSACleanup();
            throw_winsock_error_("error: Invalid address!", last_error);
        }

        socket_ = ::socket(PF_INET, SOCK_DGRAM, 0);
        if (socket_ == INVALID_SOCKET) {
            int last_error = ::WSAGetLastError();
            ::WSACleanup();
            throw_winsock_error_("error: Create Socket failed", last_error);
        }

        int option_value = TX_BUFFER_SIZE;
        if (::setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                         reinterpret_cast<const char *>(&option_value), sizeof(option_value)) < 0) {
            int last_error = ::WSAGetLastError();
            cleanup_();
            throw_winsock_error_("error: setsockopt(SO_SNDBUF) Failed!", last_error);
        }
    }

    ~udp_client() { cleanup_(); }

    SOCKET fd() const { return socket_; }

    void send(const char *data, size_t n_bytes) {
        socklen_t tolen = sizeof(struct sockaddr);
        if (::sendto(socket_, data, static_cast<int>(n_bytes), 0, (struct sockaddr *)&addr_,
                     tolen) == -1) {
            throw_spdlog_ex("sendto(2) failed", errno);
        }
    }
};
}  // namespace details
}  // namespace spdlog

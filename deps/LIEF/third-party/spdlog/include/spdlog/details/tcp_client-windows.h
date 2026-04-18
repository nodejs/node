// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#define WIN32_LEAN_AND_MEAN
// tcp client helper
#include <spdlog/common.h>
#include <spdlog/details/os.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

namespace spdlog {
namespace details {
class tcp_client {
    SOCKET socket_ = INVALID_SOCKET;

    static void init_winsock_() {
        WSADATA wsaData;
        auto rv = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (rv != 0) {
            throw_winsock_error_("WSAStartup failed", ::WSAGetLastError());
        }
    }

    static void throw_winsock_error_(const std::string &msg, int last_error) {
        char buf[512];
        ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                         last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                         (sizeof(buf) / sizeof(char)), NULL);

        throw_spdlog_ex(fmt_lib::format("tcp_sink - {}: {}", msg, buf));
    }

public:
    tcp_client() { init_winsock_(); }

    ~tcp_client() {
        close();
        ::WSACleanup();
    }

    bool is_connected() const { return socket_ != INVALID_SOCKET; }

    void close() {
        ::closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }

    SOCKET fd() const { return socket_; }

    // try to connect or throw on failure
    void connect(const std::string &host, int port) {
        if (is_connected()) {
            close();
        }
        struct addrinfo hints {};
        ZeroMemory(&hints, sizeof(hints));

        hints.ai_family = AF_UNSPEC;      // To work with IPv4, IPv6, and so on
        hints.ai_socktype = SOCK_STREAM;  // TCP
        hints.ai_flags = AI_NUMERICSERV;  // port passed as as numeric value
        hints.ai_protocol = 0;

        auto port_str = std::to_string(port);
        struct addrinfo *addrinfo_result;
        auto rv = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &addrinfo_result);
        int last_error = 0;
        if (rv != 0) {
            last_error = ::WSAGetLastError();
            WSACleanup();
            throw_winsock_error_("getaddrinfo failed", last_error);
        }

        // Try each address until we successfully connect(2).

        for (auto *rp = addrinfo_result; rp != nullptr; rp = rp->ai_next) {
            socket_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socket_ == INVALID_SOCKET) {
                last_error = ::WSAGetLastError();
                WSACleanup();
                continue;
            }
            if (::connect(socket_, rp->ai_addr, (int)rp->ai_addrlen) == 0) {
                break;
            } else {
                last_error = ::WSAGetLastError();
                close();
            }
        }
        ::freeaddrinfo(addrinfo_result);
        if (socket_ == INVALID_SOCKET) {
            WSACleanup();
            throw_winsock_error_("connect failed", last_error);
        }

        // set TCP_NODELAY
        int enable_flag = 1;
        ::setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&enable_flag),
                     sizeof(enable_flag));
    }

    // Send exactly n_bytes of the given data.
    // On error close the connection and throw.
    void send(const char *data, size_t n_bytes) {
        size_t bytes_sent = 0;
        while (bytes_sent < n_bytes) {
            const int send_flags = 0;
            auto write_result =
                ::send(socket_, data + bytes_sent, (int)(n_bytes - bytes_sent), send_flags);
            if (write_result == SOCKET_ERROR) {
                int last_error = ::WSAGetLastError();
                close();
                throw_winsock_error_("send failed", last_error);
            }

            if (write_result == 0)  // (probably should not happen but in any case..)
            {
                break;
            }
            bytes_sent += static_cast<size_t>(write_result);
        }
    }
};
}  // namespace details
}  // namespace spdlog

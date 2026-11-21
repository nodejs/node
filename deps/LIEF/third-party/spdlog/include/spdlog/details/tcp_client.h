// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifdef _WIN32
#error include tcp_client-windows.h instead
#endif

// tcp client helper
#include <spdlog/common.h>
#include <spdlog/details/os.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <string>

namespace spdlog {
namespace details {
class tcp_client {
    int socket_ = -1;

public:
    bool is_connected() const { return socket_ != -1; }

    void close() {
        if (is_connected()) {
            ::close(socket_);
            socket_ = -1;
        }
    }

    int fd() const { return socket_; }

    ~tcp_client() { close(); }

    int connect_socket_with_timeout(int sockfd,
                                    const struct sockaddr *addr,
                                    socklen_t addrlen,
                                    const timeval &tv) {
        // Blocking connect if timeout is zero
        if (tv.tv_sec == 0 && tv.tv_usec == 0) {
            int rv = ::connect(sockfd, addr, addrlen);
            if (rv < 0 && errno == EISCONN) {
                // already connected, treat as success
                return 0;
            }
            return rv;
        }

        // Non-blocking path
        int orig_flags = ::fcntl(sockfd, F_GETFL, 0);
        if (orig_flags < 0) {
            return -1;
        }
        if (::fcntl(sockfd, F_SETFL, orig_flags | O_NONBLOCK) < 0) {
            return -1;
        }

        int rv = ::connect(sockfd, addr, addrlen);
        if (rv == 0 || (rv < 0 && errno == EISCONN)) {
            // immediate connect or already connected
            ::fcntl(sockfd, F_SETFL, orig_flags);
            return 0;
        }
        if (errno != EINPROGRESS) {
            ::fcntl(sockfd, F_SETFL, orig_flags);
            return -1;
        }

        // wait for writability
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sockfd, &wfds);

        struct timeval tv_copy = tv;
        rv = ::select(sockfd + 1, nullptr, &wfds, nullptr, &tv_copy);
        if (rv <= 0) {
            // timeout or error
            ::fcntl(sockfd, F_SETFL, orig_flags);
            if (rv == 0) errno = ETIMEDOUT;
            return -1;
        }

        // check socket error
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
            ::fcntl(sockfd, F_SETFL, orig_flags);
            return -1;
        }
        ::fcntl(sockfd, F_SETFL, orig_flags);
        if (so_error != 0 && so_error != EISCONN) {
            errno = so_error;
            return -1;
        }

        return 0;
    }

    // try to connect or throw on failure
    void connect(const std::string &host, int port, int timeout_ms = 0) {
        close();
        struct addrinfo hints {};
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;      // To work with IPv4, IPv6, and so on
        hints.ai_socktype = SOCK_STREAM;  // TCP
        hints.ai_flags = AI_NUMERICSERV;  // port passed as as numeric value
        hints.ai_protocol = 0;

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        auto port_str = std::to_string(port);
        struct addrinfo *addrinfo_result;
        auto rv = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &addrinfo_result);
        if (rv != 0) {
            throw_spdlog_ex(fmt_lib::format("::getaddrinfo failed: {}", gai_strerror(rv)));
        }

        // Try each address until we successfully connect(2).
        int last_errno = 0;
        for (auto *rp = addrinfo_result; rp != nullptr; rp = rp->ai_next) {
#if defined(SOCK_CLOEXEC)
            const int flags = SOCK_CLOEXEC;
#else
            const int flags = 0;
#endif
            socket_ = ::socket(rp->ai_family, rp->ai_socktype | flags, rp->ai_protocol);
            if (socket_ == -1) {
                last_errno = errno;
                continue;
            }
            ::fcntl(socket_, F_SETFD, FD_CLOEXEC);
            if (connect_socket_with_timeout(socket_, rp->ai_addr, rp->ai_addrlen, tv) == 0) {
                last_errno = 0;
                break;
            }
            last_errno = errno;
            ::close(socket_);
            socket_ = -1;
        }
        ::freeaddrinfo(addrinfo_result);
        if (socket_ == -1) {
            throw_spdlog_ex("::connect failed", last_errno);
        }

        if (timeout_ms > 0) {
            // Set timeouts for send and recv
            ::setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
            ::setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
        }

        // set TCP_NODELAY
        int enable_flag = 1;
        ::setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&enable_flag),
                     sizeof(enable_flag));

        // prevent sigpipe on systems where MSG_NOSIGNAL is not available
#if defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
        ::setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<char *>(&enable_flag),
                     sizeof(enable_flag));
#endif

#if !defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
#error "tcp_sink would raise SIGPIPE since neither SO_NOSIGPIPE nor MSG_NOSIGNAL are available"
#endif
    }

    // Send exactly n_bytes of the given data.
    // On error close the connection and throw.
    void send(const char *data, size_t n_bytes) {
        size_t bytes_sent = 0;
        while (bytes_sent < n_bytes) {
#if defined(MSG_NOSIGNAL)
            const int send_flags = MSG_NOSIGNAL;
#else
            const int send_flags = 0;
#endif
            auto write_result =
                ::send(socket_, data + bytes_sent, n_bytes - bytes_sent, send_flags);
            if (write_result < 0) {
                close();
                throw_spdlog_ex("write(2) failed", errno);
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

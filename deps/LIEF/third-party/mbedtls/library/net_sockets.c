/*
 *  TCP/IP or UDP/IP networking functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/* Enable definition of getaddrinfo() even when compiling with -std=c99. Must
 * be set before mbedtls_config.h, which pulls in glibc's features.h indirectly.
 * Harmless on other platforms. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600 /* sockaddr_storage */
#endif

#include "common.h"

#if defined(MBEDTLS_NET_C)

#if !defined(unix) && !defined(__unix__) && !defined(__unix) && \
    !defined(__APPLE__) && !defined(_WIN32) && !defined(__QNXNTO__) && \
    !defined(__HAIKU__) && !defined(__midipix__)
#error "This module only works on Unix and Windows, see MBEDTLS_NET_C in mbedtls_config.h"
#endif

#include "mbedtls/platform.h"

#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"

#include <string.h>

#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
    !defined(EFI32)

#define IS_EINTR(ret) ((ret) == WSAEINTR)

#include <ws2tcpip.h>

#include <winsock2.h>
#include <windows.h>
#if (_WIN32_WINNT < 0x0501)
#include <wspiapi.h>
#endif

#if defined(_MSC_VER)
#if defined(_WIN32_WCE)
#pragma comment( lib, "ws2.lib" )
#else
#pragma comment( lib, "ws2_32.lib" )
#endif
#endif /* _MSC_VER */

#define read(fd, buf, len)        recv(fd, (char *) (buf), (int) (len), 0)
#define write(fd, buf, len)       send(fd, (char *) (buf), (int) (len), 0)
#define close(fd)               closesocket(fd)

static int wsa_init_done = 0;

#else /* ( _WIN32 || _WIN32_WCE ) && !EFIX64 && !EFI32 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

#define IS_EINTR(ret) ((ret) == EINTR)
#define SOCKET int

#endif /* ( _WIN32 || _WIN32_WCE ) && !EFIX64 && !EFI32 */

/* Some MS functions want int and MSVC warns if we pass size_t,
 * but the standard functions use socklen_t, so cast only for MSVC */
#if defined(_MSC_VER)
#define MSVC_INT_CAST   (int)
#else
#define MSVC_INT_CAST
#endif

#include <stdio.h>

#if defined(MBEDTLS_HAVE_TIME)
#include <time.h>
#endif

#include <stdint.h>

/*
 * Prepare for using the sockets interface
 */
static int net_prepare(void)
{
#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
    !defined(EFI32)
    WSADATA wsaData;

    if (wsa_init_done == 0) {
        if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
            return MBEDTLS_ERR_NET_SOCKET_FAILED;
        }

        wsa_init_done = 1;
    }
#else
#if !defined(EFIX64) && !defined(EFI32)
    signal(SIGPIPE, SIG_IGN);
#endif
#endif
    return 0;
}

/*
 * Return 0 if the file descriptor is valid, an error otherwise.
 * If for_select != 0, check whether the file descriptor is within the range
 * allowed for fd_set used for the FD_xxx macros and the select() function.
 */
static int check_fd(int fd, int for_select)
{
    if (fd < 0) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
    !defined(EFI32)
    (void) for_select;
#else
    /* A limitation of select() is that it only works with file descriptors
     * that are strictly less than FD_SETSIZE. This is a limitation of the
     * fd_set type. Error out early, because attempting to call FD_SET on a
     * large file descriptor is a buffer overflow on typical platforms. */
    if (for_select && fd >= FD_SETSIZE) {
        return MBEDTLS_ERR_NET_POLL_FAILED;
    }
#endif

    return 0;
}

/*
 * Initialize a context
 */
void mbedtls_net_init(mbedtls_net_context *ctx)
{
    ctx->fd = -1;
}

/*
 * Initiate a TCP connection with host:port and the given protocol
 */
int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host,
                        const char *port, int proto)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    struct addrinfo hints, *addr_list, *cur;

    if ((ret = net_prepare()) != 0) {
        return ret;
    }

    /* Do name resolution with both IPv6 and IPv4 */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &addr_list) != 0) {
        return MBEDTLS_ERR_NET_UNKNOWN_HOST;
    }

    /* Try the sockaddrs until a connection succeeds */
    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        ctx->fd = (int) socket(cur->ai_family, cur->ai_socktype,
                               cur->ai_protocol);
        if (ctx->fd < 0) {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if (connect(ctx->fd, cur->ai_addr, MSVC_INT_CAST cur->ai_addrlen) == 0) {
            ret = 0;
            break;
        }

        mbedtls_net_close(ctx);
        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
    }

    freeaddrinfo(addr_list);

    return ret;
}

/*
 * Create a listening socket on bind_ip:port
 */
int mbedtls_net_bind(mbedtls_net_context *ctx, const char *bind_ip, const char *port, int proto)
{
    int n, ret;
    struct addrinfo hints, *addr_list, *cur;

    if ((ret = net_prepare()) != 0) {
        return ret;
    }

    /* Bind to IPv6 and/or IPv4, but only in the desired protocol */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;
    if (bind_ip == NULL) {
        hints.ai_flags = AI_PASSIVE;
    }

    if (getaddrinfo(bind_ip, port, &hints, &addr_list) != 0) {
        return MBEDTLS_ERR_NET_UNKNOWN_HOST;
    }

    /* Try the sockaddrs until a binding succeeds */
    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        ctx->fd = (int) socket(cur->ai_family, cur->ai_socktype,
                               cur->ai_protocol);
        if (ctx->fd < 0) {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        n = 1;
        if (setsockopt(ctx->fd, SOL_SOCKET, SO_REUSEADDR,
                       (const char *) &n, sizeof(n)) != 0) {
            mbedtls_net_close(ctx);
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if (bind(ctx->fd, cur->ai_addr, MSVC_INT_CAST cur->ai_addrlen) != 0) {
            mbedtls_net_close(ctx);
            ret = MBEDTLS_ERR_NET_BIND_FAILED;
            continue;
        }

        /* Listen only makes sense for TCP */
        if (proto == MBEDTLS_NET_PROTO_TCP) {
            if (listen(ctx->fd, MBEDTLS_NET_LISTEN_BACKLOG) != 0) {
                mbedtls_net_close(ctx);
                ret = MBEDTLS_ERR_NET_LISTEN_FAILED;
                continue;
            }
        }

        /* Bind was successful */
        ret = 0;
        break;
    }

    freeaddrinfo(addr_list);

    return ret;

}

#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
    !defined(EFI32)
/*
 * Check if the requested operation would be blocking on a non-blocking socket
 * and thus 'failed' with a negative return value.
 */
static int net_would_block(const mbedtls_net_context *ctx)
{
    ((void) ctx);
    return WSAGetLastError() == WSAEWOULDBLOCK;
}
#else
/*
 * Check if the requested operation would be blocking on a non-blocking socket
 * and thus 'failed' with a negative return value.
 *
 * Note: on a blocking socket this function always returns 0!
 */
static int net_would_block(const mbedtls_net_context *ctx)
{
    int err = errno;

    /*
     * Never return 'WOULD BLOCK' on a blocking socket
     */
    if ((fcntl(ctx->fd, F_GETFL) & O_NONBLOCK) != O_NONBLOCK) {
        errno = err;
        return 0;
    }

    switch (errno = err) {
#if defined EAGAIN
        case EAGAIN:
#endif
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
    return 1;
    }
    return 0;
}
#endif /* ( _WIN32 || _WIN32_WCE ) && !EFIX64 && !EFI32 */

/*
 * Accept a connection from a remote client
 */
int mbedtls_net_accept(mbedtls_net_context *bind_ctx,
                       mbedtls_net_context *client_ctx,
                       void *client_ip, size_t buf_size, size_t *cip_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int type;

    struct sockaddr_storage client_addr;

#if defined(__socklen_t_defined) || defined(_SOCKLEN_T) ||  \
    defined(_SOCKLEN_T_DECLARED) || defined(__DEFINED_socklen_t) || \
    defined(socklen_t) || (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L)
    socklen_t n = (socklen_t) sizeof(client_addr);
    socklen_t type_len = (socklen_t) sizeof(type);
#else
    int n = (int) sizeof(client_addr);
    int type_len = (int) sizeof(type);
#endif

    /* Is this a TCP or UDP socket? */
    if (getsockopt(bind_ctx->fd, SOL_SOCKET, SO_TYPE,
                   (void *) &type, &type_len) != 0 ||
        (type != SOCK_STREAM && type != SOCK_DGRAM)) {
        return MBEDTLS_ERR_NET_ACCEPT_FAILED;
    }

    if (type == SOCK_STREAM) {
        /* TCP: actual accept() */
        ret = client_ctx->fd = (int) accept(bind_ctx->fd,
                                            (struct sockaddr *) &client_addr, &n);
    } else {
        /* UDP: wait for a message, but keep it in the queue */
        char buf[1] = { 0 };

        ret = (int) recvfrom(bind_ctx->fd, buf, sizeof(buf), MSG_PEEK,
                             (struct sockaddr *) &client_addr, &n);

#if defined(_WIN32)
        if (ret == SOCKET_ERROR &&
            WSAGetLastError() == WSAEMSGSIZE) {
            /* We know buf is too small, thanks, just peeking here */
            ret = 0;
        }
#endif
    }

    if (ret < 0) {
        if (net_would_block(bind_ctx) != 0) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }

        return MBEDTLS_ERR_NET_ACCEPT_FAILED;
    }

    /* UDP: hijack the listening socket to communicate with the client,
     * then bind a new socket to accept new connections */
    if (type != SOCK_STREAM) {
        struct sockaddr_storage local_addr;
        int one = 1;

        if (connect(bind_ctx->fd, (struct sockaddr *) &client_addr, n) != 0) {
            return MBEDTLS_ERR_NET_ACCEPT_FAILED;
        }

        client_ctx->fd = bind_ctx->fd;
        bind_ctx->fd   = -1; /* In case we exit early */

        n = sizeof(struct sockaddr_storage);
        if (getsockname(client_ctx->fd,
                        (struct sockaddr *) &local_addr, &n) != 0 ||
            (bind_ctx->fd = (int) socket(local_addr.ss_family,
                                         SOCK_DGRAM, IPPROTO_UDP)) < 0 ||
            setsockopt(bind_ctx->fd, SOL_SOCKET, SO_REUSEADDR,
                       (const char *) &one, sizeof(one)) != 0) {
            return MBEDTLS_ERR_NET_SOCKET_FAILED;
        }

        if (bind(bind_ctx->fd, (struct sockaddr *) &local_addr, n) != 0) {
            return MBEDTLS_ERR_NET_BIND_FAILED;
        }
    }

    if (client_ip != NULL) {
        if (client_addr.ss_family == AF_INET) {
            struct sockaddr_in *addr4 = (struct sockaddr_in *) &client_addr;
            *cip_len = sizeof(addr4->sin_addr.s_addr);

            if (buf_size < *cip_len) {
                return MBEDTLS_ERR_NET_BUFFER_TOO_SMALL;
            }

            memcpy(client_ip, &addr4->sin_addr.s_addr, *cip_len);
        } else {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) &client_addr;
            *cip_len = sizeof(addr6->sin6_addr.s6_addr);

            if (buf_size < *cip_len) {
                return MBEDTLS_ERR_NET_BUFFER_TOO_SMALL;
            }

            memcpy(client_ip, &addr6->sin6_addr.s6_addr, *cip_len);
        }
    }

    return 0;
}

/*
 * Set the socket blocking or non-blocking
 */
int mbedtls_net_set_block(mbedtls_net_context *ctx)
{
#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
    !defined(EFI32)
    u_long n = 0;
    return ioctlsocket(ctx->fd, FIONBIO, &n);
#else
    return fcntl(ctx->fd, F_SETFL, fcntl(ctx->fd, F_GETFL) & ~O_NONBLOCK);
#endif
}

int mbedtls_net_set_nonblock(mbedtls_net_context *ctx)
{
#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
    !defined(EFI32)
    u_long n = 1;
    return ioctlsocket(ctx->fd, FIONBIO, &n);
#else
    return fcntl(ctx->fd, F_SETFL, fcntl(ctx->fd, F_GETFL) | O_NONBLOCK);
#endif
}

/*
 * Check if data is available on the socket
 */

int mbedtls_net_poll(mbedtls_net_context *ctx, uint32_t rw, uint32_t timeout)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    struct timeval tv;

    fd_set read_fds;
    fd_set write_fds;

    int fd = ctx->fd;

    ret = check_fd(fd, 1);
    if (ret != 0) {
        return ret;
    }

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
    /* Ensure that memory sanitizers consider read_fds and write_fds as
     * initialized even on platforms such as Glibc/x86_64 where FD_ZERO
     * is implemented in assembly. */
    memset(&read_fds, 0, sizeof(read_fds));
    memset(&write_fds, 0, sizeof(write_fds));
#endif
#endif

    FD_ZERO(&read_fds);
    if (rw & MBEDTLS_NET_POLL_READ) {
        rw &= ~MBEDTLS_NET_POLL_READ;
        FD_SET((SOCKET) fd, &read_fds);
    }

    FD_ZERO(&write_fds);
    if (rw & MBEDTLS_NET_POLL_WRITE) {
        rw &= ~MBEDTLS_NET_POLL_WRITE;
        FD_SET((SOCKET) fd, &write_fds);
    }

    if (rw != 0) {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    do {
        ret = select(fd + 1, &read_fds, &write_fds, NULL,
                     timeout == (uint32_t) -1 ? NULL : &tv);
    } while (IS_EINTR(ret));

    if (ret < 0) {
        return MBEDTLS_ERR_NET_POLL_FAILED;
    }

    ret = 0;
    if (FD_ISSET(fd, &read_fds)) {
        ret |= MBEDTLS_NET_POLL_READ;
    }
    if (FD_ISSET(fd, &write_fds)) {
        ret |= MBEDTLS_NET_POLL_WRITE;
    }

    return ret;
}

/*
 * Portable usleep helper
 */
void mbedtls_net_usleep(unsigned long usec)
{
#if defined(_WIN32)
    Sleep((usec + 999) / 1000);
#else
    struct timeval tv;
    tv.tv_sec  = usec / 1000000;
#if (defined(__unix__) || defined(__unix) || \
    (defined(__APPLE__) && defined(__MACH__))) && !defined(__DJGPP__)
    tv.tv_usec = (suseconds_t) usec % 1000000;
#else
    tv.tv_usec = usec % 1000000;
#endif
    select(0, NULL, NULL, NULL, &tv);
#endif
}

/*
 * Read at most 'len' characters
 */
int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    ret = check_fd(fd, 0);
    if (ret != 0) {
        return ret;
    }

    ret = (int) read(fd, buf, len);

    if (ret < 0) {
        if (net_would_block(ctx) != 0) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }

#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
        !defined(EFI32)
        if (WSAGetLastError() == WSAECONNRESET) {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }
#else
        if (errno == EPIPE || errno == ECONNRESET) {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }

        if (errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
#endif

        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    return ret;
}

/*
 * Read at most 'len' characters, blocking for at most 'timeout' ms
 */
int mbedtls_net_recv_timeout(void *ctx, unsigned char *buf,
                             size_t len, uint32_t timeout)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    struct timeval tv;
    fd_set read_fds;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    ret = check_fd(fd, 1);
    if (ret != 0) {
        return ret;
    }

    FD_ZERO(&read_fds);
    FD_SET((SOCKET) fd, &read_fds);

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    ret = select(fd + 1, &read_fds, NULL, NULL, timeout == 0 ? NULL : &tv);

    /* Zero fds ready means we timed out */
    if (ret == 0) {
        return MBEDTLS_ERR_SSL_TIMEOUT;
    }

    if (ret < 0) {
#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
        !defined(EFI32)
        if (WSAGetLastError() == WSAEINTR) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
#else
        if (errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
#endif

        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    /* This call will not block */
    return mbedtls_net_recv(ctx, buf, len);
}

/*
 * Write at most 'len' characters
 */
int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    ret = check_fd(fd, 0);
    if (ret != 0) {
        return ret;
    }

    ret = (int) write(fd, buf, len);

    if (ret < 0) {
        if (net_would_block(ctx) != 0) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }

#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(EFIX64) && \
        !defined(EFI32)
        if (WSAGetLastError() == WSAECONNRESET) {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }
#else
        if (errno == EPIPE || errno == ECONNRESET) {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }

        if (errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }
#endif

        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    return ret;
}

/*
 * Close the connection
 */
void mbedtls_net_close(mbedtls_net_context *ctx)
{
    if (ctx->fd == -1) {
        return;
    }

    close(ctx->fd);

    ctx->fd = -1;
}

/*
 * Gracefully close the connection
 */
void mbedtls_net_free(mbedtls_net_context *ctx)
{
    if (ctx == NULL || ctx->fd == -1) {
        return;
    }

    shutdown(ctx->fd, 2);
    close(ctx->fd);

    ctx->fd = -1;
}

#endif /* MBEDTLS_NET_C */

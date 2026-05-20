/**
 * \file net_sockets.h
 *
 * \brief   Network sockets abstraction layer to integrate Mbed TLS into a
 *          BSD-style sockets API.
 *
 *          The network sockets module provides an example integration of the
 *          Mbed TLS library into a BSD sockets implementation. The module is
 *          intended to be an example of how Mbed TLS can be integrated into a
 *          networking stack, as well as to be Mbed TLS's network integration
 *          for its supported platforms.
 *
 *          The module is intended only to be used with the Mbed TLS library and
 *          is not intended to be used by third party application software
 *          directly.
 *
 *          The supported platforms are as follows:
 *              * Microsoft Windows and Windows CE
 *              * POSIX/Unix platforms including Linux, OS X
 *
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_NET_SOCKETS_H
#define MBEDTLS_NET_SOCKETS_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/ssl.h"

#include <stddef.h>
#include <stdint.h>

/** Failed to open a socket. */
#define MBEDTLS_ERR_NET_SOCKET_FAILED                     -0x0042
/** The connection to the given server / port failed. */
#define MBEDTLS_ERR_NET_CONNECT_FAILED                    -0x0044
/** Binding of the socket failed. */
#define MBEDTLS_ERR_NET_BIND_FAILED                       -0x0046
/** Could not listen on the socket. */
#define MBEDTLS_ERR_NET_LISTEN_FAILED                     -0x0048
/** Could not accept the incoming connection. */
#define MBEDTLS_ERR_NET_ACCEPT_FAILED                     -0x004A
/** Reading information from the socket failed. */
#define MBEDTLS_ERR_NET_RECV_FAILED                       -0x004C
/** Sending information through the socket failed. */
#define MBEDTLS_ERR_NET_SEND_FAILED                       -0x004E
/** Connection was reset by peer. */
#define MBEDTLS_ERR_NET_CONN_RESET                        -0x0050
/** Failed to get an IP address for the given hostname. */
#define MBEDTLS_ERR_NET_UNKNOWN_HOST                      -0x0052
/** Buffer is too small to hold the data. */
#define MBEDTLS_ERR_NET_BUFFER_TOO_SMALL                  -0x0043
/** The context is invalid, eg because it was free()ed. */
#define MBEDTLS_ERR_NET_INVALID_CONTEXT                   -0x0045
/** Polling the net context failed. */
#define MBEDTLS_ERR_NET_POLL_FAILED                       -0x0047
/** Input invalid. */
#define MBEDTLS_ERR_NET_BAD_INPUT_DATA                    -0x0049

#define MBEDTLS_NET_LISTEN_BACKLOG         10 /**< The backlog that listen() should use. */

#define MBEDTLS_NET_PROTO_TCP 0 /**< The TCP transport protocol */
#define MBEDTLS_NET_PROTO_UDP 1 /**< The UDP transport protocol */

#define MBEDTLS_NET_POLL_READ  1 /**< Used in \c mbedtls_net_poll to check for pending data  */
#define MBEDTLS_NET_POLL_WRITE 2 /**< Used in \c mbedtls_net_poll to check if write possible */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wrapper type for sockets.
 *
 * Currently backed by just a file descriptor, but might be more in the future
 * (eg two file descriptors for combined IPv4 + IPv6 support, or additional
 * structures for hand-made UDP demultiplexing).
 */
typedef struct mbedtls_net_context {
    /** The underlying file descriptor.
     *
     * This field is only guaranteed to be present on POSIX/Unix-like platforms.
     * On other platforms, it may have a different type, have a different
     * meaning, or be absent altogether.
     */
    int fd;
}
mbedtls_net_context;

/**
 * \brief          Initialize a context
 *                 Just makes the context ready to be used or freed safely.
 *
 * \param ctx      Context to initialize
 */
void mbedtls_net_init(mbedtls_net_context *ctx);

/**
 * \brief          Initiate a connection with host:port in the given protocol
 *
 * \param ctx      Socket to use
 * \param host     Host to connect to
 * \param port     Port to connect to
 * \param proto    Protocol: MBEDTLS_NET_PROTO_TCP or MBEDTLS_NET_PROTO_UDP
 *
 * \return         0 if successful, or one of:
 *                      MBEDTLS_ERR_NET_SOCKET_FAILED,
 *                      MBEDTLS_ERR_NET_UNKNOWN_HOST,
 *                      MBEDTLS_ERR_NET_CONNECT_FAILED
 *
 * \note           Sets the socket in connected mode even with UDP.
 */
int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host, const char *port, int proto);

/**
 * \brief          Create a receiving socket on bind_ip:port in the chosen
 *                 protocol. If bind_ip == NULL, all interfaces are bound.
 *
 * \param ctx      Socket to use
 * \param bind_ip  IP to bind to, can be NULL
 * \param port     Port number to use
 * \param proto    Protocol: MBEDTLS_NET_PROTO_TCP or MBEDTLS_NET_PROTO_UDP
 *
 * \return         0 if successful, or one of:
 *                      MBEDTLS_ERR_NET_SOCKET_FAILED,
 *                      MBEDTLS_ERR_NET_UNKNOWN_HOST,
 *                      MBEDTLS_ERR_NET_BIND_FAILED,
 *                      MBEDTLS_ERR_NET_LISTEN_FAILED
 *
 * \note           Regardless of the protocol, opens the sockets and binds it.
 *                 In addition, make the socket listening if protocol is TCP.
 */
int mbedtls_net_bind(mbedtls_net_context *ctx, const char *bind_ip, const char *port, int proto);

/**
 * \brief           Accept a connection from a remote client
 *
 * \param bind_ctx  Relevant socket
 * \param client_ctx Will contain the connected client socket
 * \param client_ip Will contain the client IP address, can be NULL
 * \param buf_size  Size of the client_ip buffer
 * \param cip_len   Will receive the size of the client IP written,
 *                  can be NULL if client_ip is null
 *
 * \return          0 if successful, or
 *                  MBEDTLS_ERR_NET_SOCKET_FAILED,
 *                  MBEDTLS_ERR_NET_BIND_FAILED,
 *                  MBEDTLS_ERR_NET_ACCEPT_FAILED, or
 *                  MBEDTLS_ERR_NET_BUFFER_TOO_SMALL if buf_size is too small,
 *                  MBEDTLS_ERR_SSL_WANT_READ if bind_fd was set to
 *                  non-blocking and accept() would block.
 */
int mbedtls_net_accept(mbedtls_net_context *bind_ctx,
                       mbedtls_net_context *client_ctx,
                       void *client_ip, size_t buf_size, size_t *cip_len);

/**
 * \brief          Check and wait for the context to be ready for read/write
 *
 * \note           The current implementation of this function uses
 *                 select() and returns an error if the file descriptor
 *                 is \c FD_SETSIZE or greater.
 *
 * \param ctx      Socket to check
 * \param rw       Bitflag composed of MBEDTLS_NET_POLL_READ and
 *                 MBEDTLS_NET_POLL_WRITE specifying the events
 *                 to wait for:
 *                 - If MBEDTLS_NET_POLL_READ is set, the function
 *                   will return as soon as the net context is available
 *                   for reading.
 *                 - If MBEDTLS_NET_POLL_WRITE is set, the function
 *                   will return as soon as the net context is available
 *                   for writing.
 * \param timeout  Maximal amount of time to wait before returning,
 *                 in milliseconds. If \c timeout is zero, the
 *                 function returns immediately. If \c timeout is
 *                 -1u, the function blocks potentially indefinitely.
 *
 * \return         Bitmask composed of MBEDTLS_NET_POLL_READ/WRITE
 *                 on success or timeout, or a negative return code otherwise.
 */
int mbedtls_net_poll(mbedtls_net_context *ctx, uint32_t rw, uint32_t timeout);

/**
 * \brief          Set the socket blocking
 *
 * \param ctx      Socket to set
 *
 * \return         0 if successful, or a non-zero error code
 */
int mbedtls_net_set_block(mbedtls_net_context *ctx);

/**
 * \brief          Set the socket non-blocking
 *
 * \param ctx      Socket to set
 *
 * \return         0 if successful, or a non-zero error code
 */
int mbedtls_net_set_nonblock(mbedtls_net_context *ctx);

/**
 * \brief          Portable usleep helper
 *
 * \param usec     Amount of microseconds to sleep
 *
 * \note           Real amount of time slept will not be less than
 *                 select()'s timeout granularity (typically, 10ms).
 */
void mbedtls_net_usleep(unsigned long usec);

/**
 * \brief          Read at most 'len' characters. If no error occurs,
 *                 the actual amount read is returned.
 *
 * \param ctx      Socket
 * \param buf      The buffer to write to
 * \param len      Maximum length of the buffer
 *
 * \return         the number of bytes received,
 *                 or a non-zero error code; with a non-blocking socket,
 *                 MBEDTLS_ERR_SSL_WANT_READ indicates read() would block.
 */
int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len);

/**
 * \brief          Write at most 'len' characters. If no error occurs,
 *                 the actual amount written is returned.
 *
 * \param ctx      Socket
 * \param buf      The buffer to read from
 * \param len      The length of the buffer
 *
 * \return         the number of bytes sent,
 *                 or a non-zero error code; with a non-blocking socket,
 *                 MBEDTLS_ERR_SSL_WANT_WRITE indicates write() would block.
 */
int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len);

/**
 * \brief          Read at most 'len' characters, blocking for at most
 *                 'timeout' seconds. If no error occurs, the actual amount
 *                 read is returned.
 *
 * \note           The current implementation of this function uses
 *                 select() and returns an error if the file descriptor
 *                 is \c FD_SETSIZE or greater.
 *
 * \param ctx      Socket
 * \param buf      The buffer to write to
 * \param len      Maximum length of the buffer
 * \param timeout  Maximum number of milliseconds to wait for data
 *                 0 means no timeout (wait forever)
 *
 * \return         The number of bytes received if successful.
 *                 MBEDTLS_ERR_SSL_TIMEOUT if the operation timed out.
 *                 MBEDTLS_ERR_SSL_WANT_READ if interrupted by a signal.
 *                 Another negative error code (MBEDTLS_ERR_NET_xxx)
 *                 for other failures.
 *
 * \note           This function will block (until data becomes available or
 *                 timeout is reached) even if the socket is set to
 *                 non-blocking. Handling timeouts with non-blocking reads
 *                 requires a different strategy.
 */
int mbedtls_net_recv_timeout(void *ctx, unsigned char *buf, size_t len,
                             uint32_t timeout);

/**
 * \brief          Closes down the connection and free associated data
 *
 * \param ctx      The context to close
 *
 * \note           This function frees and clears data associated with the
 *                 context but does not free the memory pointed to by \p ctx.
 *                 This memory is the responsibility of the caller.
 */
void mbedtls_net_close(mbedtls_net_context *ctx);

/**
 * \brief          Gracefully shutdown the connection and free associated data
 *
 * \param ctx      The context to free
 *
 * \note           This function frees and clears data associated with the
 *                 context but does not free the memory pointed to by \p ctx.
 *                 This memory is the responsibility of the caller.
 */
void mbedtls_net_free(mbedtls_net_context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* net_sockets.h */

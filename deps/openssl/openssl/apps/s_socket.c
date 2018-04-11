/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* socket-related functions used by s_client and s_server */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <openssl/opensslconf.h>

/*
 * With IPv6, it looks like Digital has mixed up the proper order of
 * recursive header file inclusion, resulting in the compiler complaining
 * that u_int isn't defined, but only if _POSIX_C_SOURCE is defined, which is
 * needed to have fileno() declared correctly...  So let's define u_int
 */
#if defined(OPENSSL_SYS_VMS_DECC) && !defined(__U_INT)
# define __U_INT
typedef unsigned int u_int;
#endif

#ifndef OPENSSL_NO_SOCK

# define USE_SOCKETS
# include "apps.h"
# undef USE_SOCKETS
# include "s_apps.h"

# include <openssl/bio.h>
# include <openssl/err.h>

/*
 * init_client - helper routine to set up socket communication
 * @sock: pointer to storage of resulting socket.
 * @host: the host name or path (for AF_UNIX) to connect to.
 * @port: the port to connect to (ignored for AF_UNIX).
 * @family: desired socket family, may be AF_INET, AF_INET6, AF_UNIX or
 *  AF_UNSPEC
 * @type: socket type, must be SOCK_STREAM or SOCK_DGRAM
 *
 * This will create a socket and use it to connect to a host:port, or if
 * family == AF_UNIX, to the path found in host.
 *
 * If the host has more than one address, it will try them one by one until
 * a successful connection is established.  The resulting socket will be
 * found in *sock on success, it will be given INVALID_SOCKET otherwise.
 *
 * Returns 1 on success, 0 on failure.
 */
int init_client(int *sock, const char *host, const char *port,
                int family, int type)
{
    BIO_ADDRINFO *res = NULL;
    const BIO_ADDRINFO *ai = NULL;
    int ret;

    if (!BIO_sock_init())
        return 0;

    ret = BIO_lookup(host, port, BIO_LOOKUP_CLIENT, family, type, &res);
    if (ret == 0) {
        ERR_print_errors(bio_err);
        return 0;
    }

    ret = 0;
    for (ai = res; ai != NULL; ai = BIO_ADDRINFO_next(ai)) {
        /* Admittedly, these checks are quite paranoid, we should not get
         * anything in the BIO_ADDRINFO chain that we haven't
         * asked for. */
        OPENSSL_assert((family == AF_UNSPEC
                        || family == BIO_ADDRINFO_family(ai))
                       && (type == 0 || type == BIO_ADDRINFO_socktype(ai)));

        *sock = BIO_socket(BIO_ADDRINFO_family(ai), BIO_ADDRINFO_socktype(ai),
                           BIO_ADDRINFO_protocol(ai), 0);
        if (*sock == INVALID_SOCKET) {
            /* Maybe the kernel doesn't support the socket family, even if
             * BIO_lookup() added it in the returned result...
             */
            continue;
        }
        if (!BIO_connect(*sock, BIO_ADDRINFO_address(ai), 0)) {
            BIO_closesocket(*sock);
            *sock = INVALID_SOCKET;
            continue;
        }

        /* Success, don't try any more addresses */
        break;
    }

    if (*sock == INVALID_SOCKET) {
        ERR_print_errors(bio_err);
    } else {
        /* Remove any stale errors from previous connection attempts */
        ERR_clear_error();
        ret = 1;
    }
    BIO_ADDRINFO_free(res);
    return ret;
}

/*
 * do_server - helper routine to perform a server operation
 * @accept_sock: pointer to storage of resulting socket.
 * @host: the host name or path (for AF_UNIX) to connect to.
 * @port: the port to connect to (ignored for AF_UNIX).
 * @family: desired socket family, may be AF_INET, AF_INET6, AF_UNIX or
 *  AF_UNSPEC
 * @type: socket type, must be SOCK_STREAM or SOCK_DGRAM
 * @cb: pointer to a function that receives the accepted socket and
 *  should perform the communication with the connecting client.
 * @context: pointer to memory that's passed verbatim to the cb function.
 * @naccept: number of times an incoming connect should be accepted.  If -1,
 *  unlimited number.
 *
 * This will create a socket and use it to listen to a host:port, or if
 * family == AF_UNIX, to the path found in host, then start accepting
 * incoming connections and run cb on the resulting socket.
 *
 * 0 on failure, something other on success.
 */
int do_server(int *accept_sock, const char *host, const char *port,
              int family, int type, do_server_cb cb,
              unsigned char *context, int naccept)
{
    int asock = 0;
    int sock;
    int i;
    BIO_ADDRINFO *res = NULL;
    const BIO_ADDRINFO *next;
    int sock_family, sock_type, sock_protocol;
    const BIO_ADDR *sock_address;
    int sock_options = BIO_SOCK_REUSEADDR;
    int ret = 0;

    if (!BIO_sock_init())
        return 0;

    if (!BIO_lookup(host, port, BIO_LOOKUP_SERVER, family, type, &res)) {
        ERR_print_errors(bio_err);
        return 0;
    }

    /* Admittedly, these checks are quite paranoid, we should not get
     * anything in the BIO_ADDRINFO chain that we haven't asked for */
    OPENSSL_assert((family == AF_UNSPEC || family == BIO_ADDRINFO_family(res))
                   && (type == 0 || type == BIO_ADDRINFO_socktype(res)));

    sock_family = BIO_ADDRINFO_family(res);
    sock_type = BIO_ADDRINFO_socktype(res);
    sock_protocol = BIO_ADDRINFO_protocol(res);
    sock_address = BIO_ADDRINFO_address(res);
    next = BIO_ADDRINFO_next(res);
    if (sock_family == AF_INET6)
        sock_options |= BIO_SOCK_V6_ONLY;
    if (next != NULL
            && BIO_ADDRINFO_socktype(next) == sock_type
            && BIO_ADDRINFO_protocol(next) == sock_protocol) {
        if (sock_family == AF_INET
                && BIO_ADDRINFO_family(next) == AF_INET6) {
            sock_family = AF_INET6;
            sock_address = BIO_ADDRINFO_address(next);
        } else if (sock_family == AF_INET6
                   && BIO_ADDRINFO_family(next) == AF_INET) {
            sock_options &= ~BIO_SOCK_V6_ONLY;
        }
    }

    asock = BIO_socket(sock_family, sock_type, sock_protocol, 0);
    if (asock == INVALID_SOCKET
        || !BIO_listen(asock, sock_address, sock_options)) {
        BIO_ADDRINFO_free(res);
        ERR_print_errors(bio_err);
        if (asock != INVALID_SOCKET)
            BIO_closesocket(asock);
        goto end;
    }

    BIO_ADDRINFO_free(res);
    res = NULL;

    if (accept_sock != NULL)
        *accept_sock = asock;
    for (;;) {
        if (type == SOCK_STREAM) {
            do {
                sock = BIO_accept_ex(asock, NULL, 0);
            } while (sock < 0 && BIO_sock_should_retry(sock));
            if (sock < 0) {
                ERR_print_errors(bio_err);
                BIO_closesocket(asock);
                break;
            }
            i = (*cb)(sock, type, context);

            /*
             * Give the socket time to send its last data before we close it.
             * No amount of setting SO_LINGER etc on the socket seems to
             * persuade Windows to send the data before closing the socket...
             * but sleeping for a short time seems to do it (units in ms)
             * TODO: Find a better way to do this
             */
#if defined(OPENSSL_SYS_WINDOWS)
            Sleep(50);
#elif defined(OPENSSL_SYS_CYGWIN)
            usleep(50000);
#endif

            /*
             * If we ended with an alert being sent, but still with data in the
             * network buffer to be read, then calling BIO_closesocket() will
             * result in a TCP-RST being sent. On some platforms (notably
             * Windows) then this will result in the peer immediately abandoning
             * the connection including any buffered alert data before it has
             * had a chance to be read. Shutting down the sending side first,
             * and then closing the socket sends TCP-FIN first followed by
             * TCP-RST. This seems to allow the peer to read the alert data.
             */
            shutdown(sock, 1); /* SHUT_WR */
            BIO_closesocket(sock);
        } else {
            i = (*cb)(asock, type, context);
        }

        if (naccept != -1)
            naccept--;
        if (i < 0 || naccept == 0) {
            BIO_closesocket(asock);
            ret = i;
            break;
        }
    }
 end:
# ifdef AF_UNIX
    if (family == AF_UNIX)
        unlink(host);
# endif
    return ret;
}

#endif  /* OPENSSL_NO_SOCK */

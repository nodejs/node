/*
 * Copyright 2024-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/quic.h>
#ifdef _WIN32 /* Windows */
# include <winsock2.h>
#else /* Linux/Unix */
# include <netinet/in.h>
# include <unistd.h>
#endif
#include <assert.h>

/*
 * This is a basic demo of QUIC server functionality in which one connection at
 * a time is accepted in a blocking loop.
 */

/* ALPN string for TLS handshake */
static const unsigned char alpn_ossltest[] = {
    /* "\x08ossltest" (hex for EBCDIC resilience) */
    0x08, 0x6f, 0x73, 0x73, 0x6c, 0x74, 0x65, 0x73, 0x74
};

/* This callback validates and negotiates the desired ALPN on the server side. */
static int select_alpn(SSL *ssl,
                       const unsigned char **out, unsigned char *out_len,
                       const unsigned char *in, unsigned int in_len,
                       void *arg)
{
    if (SSL_select_next_proto((unsigned char **)out, out_len,
                              alpn_ossltest, sizeof(alpn_ossltest), in, in_len)
            != OPENSSL_NPN_NEGOTIATED)
        return SSL_TLSEXT_ERR_ALERT_FATAL;

    return SSL_TLSEXT_ERR_OK;
}

/* Create SSL_CTX. */
static SSL_CTX *create_ctx(const char *cert_path, const char *key_path)
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(OSSL_QUIC_server_method());
    if (ctx == NULL)
        goto err;

    /* Load certificate and corresponding private key. */
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_path) <= 0) {
        fprintf(stderr, "couldn't load certificate file: %s\n", cert_path);
        goto err;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "couldn't load key file: %s\n", key_path);
        goto err;
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "private key check failed\n");
        goto err;
    }

    /* Setup ALPN negotiation callback. */
    SSL_CTX_set_alpn_select_cb(ctx, select_alpn, NULL);
    return ctx;

err:
    SSL_CTX_free(ctx);
    return NULL;
}

/* Create UDP socket using given port. */
static int create_socket(uint16_t port)
{
    int fd = -1;
    struct sockaddr_in sa = {0};

    if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fprintf(stderr, "cannot create socket");
        goto err;
    }

    sa.sin_family  = AF_INET;
    sa.sin_port    = htons(port);

    if (bind(fd, (const struct sockaddr *)&sa, sizeof(sa)) < 0) {
        fprintf(stderr, "cannot bind to %u\n", port);
        goto err;
    }

    return fd;

err:
    if (fd >= 0)
        BIO_closesocket(fd);

    return -1;
}

/* Main loop for servicing a single incoming QUIC connection. */
static int run_quic_conn(SSL *conn)
{
    size_t written = 0;

    fprintf(stderr, "=> Received connection\n");

    /*
     * Write the message "hello" on the connection using a default stream
     * and then immediately conclude the stream (end-of-stream). This
     * demonstrates the use of SSL_write_ex2 for optimised FIN generation.
     *
     * Since we inherit our blocking mode from the parent QUIC SSL object (the
     * listener) by default, this call is also blocking.
     */
    if (!SSL_write_ex2(conn, "hello\n", 6, SSL_WRITE_FLAG_CONCLUDE, &written)
        || written != 6) {
        fprintf(stderr, "couldn't write on connection\n");
        ERR_print_errors_fp(stderr);
        return 0;
    }

    /* Shut down the connection (blocking). */
    if (SSL_shutdown(conn) != 1) {
        ERR_print_errors_fp(stderr);
        return 0;
    }

    fprintf(stderr, "=> Finished with connection\n");
    return 1;
}

/* Main loop for server to accept QUIC connections. */
static int run_quic_server(SSL_CTX *ctx, int fd)
{
    int ok = 0;
    SSL *listener = NULL, *conn = NULL;

    /* Create a new QUIC listener. */
    if ((listener = SSL_new_listener(ctx, 0)) == NULL)
        goto err;

    /* Provide the listener with our UDP socket. */
    if (!SSL_set_fd(listener, fd))
        goto err;

    /* Begin listening. */
    if (!SSL_listen(listener))
        goto err;

    /*
     * Listeners, and other QUIC objects, default to operating in blocking mode,
     * so the below call is not actually necessary. The configured behaviour is
     * inherited by child objects.
     */
    if (!SSL_set_blocking_mode(listener, 1))
        goto err;

    for (;;) {
        /* Blocking wait for an incoming connection, similar to accept(2). */
        conn = SSL_accept_connection(listener, 0);
        if (conn == NULL) {
            fprintf(stderr, "error while accepting connection\n");
            goto err;
        }

        /*
         * Optionally, we could disable blocking mode on the accepted connection
         * here by calling SSL_set_blocking_mode().
         */

        /*
         * Service the connection. In a real application this would be done
         * concurrently. In this demonstration program a single connection is
         * accepted and serviced at a time.
         */
        if (!run_quic_conn(conn)) {
            SSL_free(conn);
            goto err;
        }

        /* Free the connection, then loop again, accepting another connection. */
        SSL_free(conn);
    }

    ok = 1;
err:
    if (!ok)
        ERR_print_errors_fp(stderr);

    SSL_free(listener);
    return ok;
}

int main(int argc, char **argv)
{
    int rc = 1;
    SSL_CTX *ctx = NULL;
    int fd = -1;
    unsigned long port;

    if (argc < 4) {
        fprintf(stderr, "usage: %s <port> <server.crt> <server.key>\n", argv[0]);
        goto err;
    }

    /* Create SSL_CTX. */
    if ((ctx = create_ctx(argv[2], argv[3])) == NULL)
        goto err;

    /* Parse port number from command line arguments. */
    port = strtoul(argv[1], NULL, 0);
    if (port == 0 || port > UINT16_MAX) {
        fprintf(stderr, "invalid port: %lu\n", port);
        goto err;
    }

    /* Create UDP socket. */
    if ((fd = create_socket((uint16_t)port)) < 0)
        goto err;

    /* Enter QUIC server connection acceptance loop. */
    if (!run_quic_server(ctx, fd))
        goto err;

    rc = 0;
err:
    if (rc != 0)
        ERR_print_errors_fp(stderr);

    SSL_CTX_free(ctx);

    if (fd != -1)
        BIO_closesocket(fd);

    return rc;
}

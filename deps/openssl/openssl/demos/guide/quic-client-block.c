/*
 *  Copyright 2023-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License 2.0 (the "License").  You may not use
 *  this file except in compliance with the License.  You can obtain a copy
 *  in the file LICENSE in the source distribution or at
 *  https://www.openssl.org/source/license.html
 */

/*
 * NB: Changes to this file should also be reflected in
 * doc/man7/ossl-guide-quic-client-block.pod
 */

#include <string.h>

/* Include the appropriate header file for SOCK_DGRAM */
#ifdef _WIN32 /* Windows */
# include <winsock2.h>
#else /* Linux/Unix */
# include <sys/socket.h>
#endif

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* Helper function to create a BIO connected to the server */
static BIO *create_socket_bio(const char *hostname, const char *port,
                              int family, BIO_ADDR **peer_addr)
{
    int sock = -1;
    BIO_ADDRINFO *res;
    const BIO_ADDRINFO *ai = NULL;
    BIO *bio;

    /*
     * Lookup IP address info for the server.
     */
    if (!BIO_lookup_ex(hostname, port, BIO_LOOKUP_CLIENT, family, SOCK_DGRAM, 0,
                       &res))
        return NULL;

    /*
     * Loop through all the possible addresses for the server and find one
     * we can connect to.
     */
    for (ai = res; ai != NULL; ai = BIO_ADDRINFO_next(ai)) {
        /*
         * Create a UDP socket. We could equally use non-OpenSSL calls such
         * as "socket" here for this and the subsequent connect and close
         * functions. But for portability reasons and also so that we get
         * errors on the OpenSSL stack in the event of a failure we use
         * OpenSSL's versions of these functions.
         */
        sock = BIO_socket(BIO_ADDRINFO_family(ai), SOCK_DGRAM, 0, 0);
        if (sock == -1)
            continue;

        /* Connect the socket to the server's address */
        if (!BIO_connect(sock, BIO_ADDRINFO_address(ai), 0)) {
            BIO_closesocket(sock);
            sock = -1;
            continue;
        }

        /* Set to nonblocking mode */
        if (!BIO_socket_nbio(sock, 1)) {
            BIO_closesocket(sock);
            sock = -1;
            continue;
        }

        break;
    }

    if (sock != -1) {
        *peer_addr = BIO_ADDR_dup(BIO_ADDRINFO_address(ai));
        if (*peer_addr == NULL) {
            BIO_closesocket(sock);
            return NULL;
        }
    }

    /* Free the address information resources we allocated earlier */
    BIO_ADDRINFO_free(res);

    /* If sock is -1 then we've been unable to connect to the server */
    if (sock == -1)
        return NULL;

    /* Create a BIO to wrap the socket */
    bio = BIO_new(BIO_s_datagram());
    if (bio == NULL) {
        BIO_closesocket(sock);
        return NULL;
    }

    /*
     * Associate the newly created BIO with the underlying socket. By
     * passing BIO_CLOSE here the socket will be automatically closed when
     * the BIO is freed. Alternatively you can use BIO_NOCLOSE, in which
     * case you must close the socket explicitly when it is no longer
     * needed.
     */
    BIO_set_fd(bio, sock, BIO_CLOSE);

    return bio;
}

/*
 * Simple application to send a basic HTTP/1.0 request to a server and
 * print the response on the screen. Note that HTTP/1.0 over QUIC is
 * non-standard and will not typically be supported by real world servers. This
 * is for demonstration purposes only.
 */
int main(int argc, char *argv[])
{
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    BIO *bio = NULL;
    int res = EXIT_FAILURE;
    int ret;
    unsigned char alpn[] = { 8, 'h', 't', 't', 'p', '/', '1', '.', '0' };
    const char *request_start = "GET / HTTP/1.0\r\nConnection: close\r\nHost: ";
    const char *request_end = "\r\n\r\n";
    size_t written, readbytes;
    char buf[160];
    BIO_ADDR *peer_addr = NULL;
    char *hostname, *port;
    int argnext = 1;
    int ipv6 = 0;

    if (argc < 3) {
        printf("Usage: quic-client-block [-6] hostname port\n");
        goto end;
    }

    if (!strcmp(argv[argnext], "-6")) {
        if (argc < 4) {
            printf("Usage: quic-client-block [-6] hostname port\n");
            goto end;
        }
        ipv6 = 1;
        argnext++;
    }
    hostname = argv[argnext++];
    port = argv[argnext];

    /*
     * Create an SSL_CTX which we can use to create SSL objects from. We
     * want an SSL_CTX for creating clients so we use
     * OSSL_QUIC_client_method() here.
     */
    ctx = SSL_CTX_new(OSSL_QUIC_client_method());
    if (ctx == NULL) {
        printf("Failed to create the SSL_CTX\n");
        goto end;
    }

    /*
     * Configure the client to abort the handshake if certificate
     * verification fails. Virtually all clients should do this unless you
     * really know what you are doing.
     */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    /* Use the default trusted certificate store */
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        printf("Failed to set the default trusted certificate store\n");
        goto end;
    }

    /* Create an SSL object to represent the TLS connection */
    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        printf("Failed to create the SSL object\n");
        goto end;
    }

    /*
     * Create the underlying transport socket/BIO and associate it with the
     * connection.
     */
    bio = create_socket_bio(hostname, port, ipv6 ? AF_INET6 : AF_INET, &peer_addr);
    if (bio == NULL) {
        printf("Failed to crete the BIO\n");
        goto end;
    }
    SSL_set_bio(ssl, bio, bio);

    /*
     * Tell the server during the handshake which hostname we are attempting
     * to connect to in case the server supports multiple hosts.
     */
    if (!SSL_set_tlsext_host_name(ssl, hostname)) {
        printf("Failed to set the SNI hostname\n");
        goto end;
    }

    /*
     * Ensure we check during certificate verification that the server has
     * supplied a certificate for the hostname that we were expecting.
     * Virtually all clients should do this unless you really know what you
     * are doing.
     */
    if (!SSL_set1_host(ssl, hostname)) {
        printf("Failed to set the certificate verification hostname");
        goto end;
    }

    /* SSL_set_alpn_protos returns 0 for success! */
    if (SSL_set_alpn_protos(ssl, alpn, sizeof(alpn)) != 0) {
        printf("Failed to set the ALPN for the connection\n");
        goto end;
    }

    /* Set the IP address of the remote peer */
    if (!SSL_set1_initial_peer_addr(ssl, peer_addr)) {
        printf("Failed to set the initial peer address\n");
        goto end;
    }

    /* Do the handshake with the server */
    if (SSL_connect(ssl) < 1) {
        printf("Failed to connect to the server\n");
        /*
         * If the failure is due to a verification error we can get more
         * information about it from SSL_get_verify_result().
         */
        if (SSL_get_verify_result(ssl) != X509_V_OK)
            printf("Verify error: %s\n",
                X509_verify_cert_error_string(SSL_get_verify_result(ssl)));
        goto end;
    }

    /* Write an HTTP GET request to the peer */
    if (!SSL_write_ex(ssl, request_start, strlen(request_start), &written)) {
        printf("Failed to write start of HTTP request\n");
        goto end;
    }
    if (!SSL_write_ex(ssl, hostname, strlen(hostname), &written)) {
        printf("Failed to write hostname in HTTP request\n");
        goto end;
    }
    if (!SSL_write_ex2(ssl, request_end, strlen(request_end),
                       SSL_WRITE_FLAG_CONCLUDE, &written)) {
        printf("Failed to write end of HTTP request\n");
        goto end;
    }

    /*
     * Get up to sizeof(buf) bytes of the response. We keep reading until the
     * server closes the connection.
     */
    while (SSL_read_ex(ssl, buf, sizeof(buf), &readbytes)) {
        /*
        * OpenSSL does not guarantee that the returned data is a string or
        * that it is NUL terminated so we use fwrite() to write the exact
        * number of bytes that we read. The data could be non-printable or
        * have NUL characters in the middle of it. For this simple example
        * we're going to print it to stdout anyway.
        */
        fwrite(buf, 1, readbytes, stdout);
    }
    /* In case the response didn't finish with a newline we add one now */
    printf("\n");

    /*
     * Check whether we finished the while loop above normally or as the
     * result of an error. The 0 argument to SSL_get_error() is the return
     * code we received from the SSL_read_ex() call. It must be 0 in order
     * to get here. Normal completion is indicated by SSL_ERROR_ZERO_RETURN. In
     * QUIC terms this means that the peer has sent FIN on the stream to
     * indicate that no further data will be sent.
     */
    switch (SSL_get_error(ssl, 0)) {
    case SSL_ERROR_ZERO_RETURN:
        /* Normal completion of the stream */
        break;

    case SSL_ERROR_SSL:
        /*
         * Some stream fatal error occurred. This could be because of a stream
         * reset - or some failure occurred on the underlying connection.
         */
        switch (SSL_get_stream_read_state(ssl)) {
        case SSL_STREAM_STATE_RESET_REMOTE:
            printf("Stream reset occurred\n");
            /* The stream has been reset but the connection is still healthy. */
            break;

        case SSL_STREAM_STATE_CONN_CLOSED:
            printf("Connection closed\n");
            /* Connection is already closed. Skip SSL_shutdown() */
            goto end;

        default:
            printf("Unknown stream failure\n");
            break;
        }
        break;

    default:
        /* Some other unexpected error occurred */
        printf ("Failed reading remaining data\n");
        break;
    }

    /*
     * Repeatedly call SSL_shutdown() until the connection is fully
     * closed.
     */
    do {
        ret = SSL_shutdown(ssl);
        if (ret < 0) {
            printf("Error shutting down: %d\n", ret);
            goto end;
        }
    } while (ret != 1);

    /* Success! */
    res = EXIT_SUCCESS;
 end:
    /*
     * If something bad happened then we will dump the contents of the
     * OpenSSL error stack to stderr. There might be some useful diagnostic
     * information there.
     */
    if (res == EXIT_FAILURE)
        ERR_print_errors_fp(stderr);

    /*
     * Free the resources we allocated. We do not free the BIO object here
     * because ownership of it was immediately transferred to the SSL object
     * via SSL_set_bio(). The BIO will be freed when we free the SSL object.
     */
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    BIO_ADDR_free(peer_addr);
    return res;
}

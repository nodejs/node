/*
 * Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../ssl/packet_locl.h"

#define CLIENT_VERSION_LEN      2


#define TOTAL_NUM_TESTS                         1

/*
 * Test that explicitly setting ticket data results in it appearing in the
 * ClientHello for a negotiated SSL/TLS version
 */
#define TEST_SET_SESSION_TICK_DATA_VER_NEG      0

int main(int argc, char *argv[])
{
    SSL_CTX *ctx = NULL;
    SSL *con = NULL;
    BIO *rbio;
    BIO *wbio;
    BIO *err;
    long len;
    unsigned char *data;
    PACKET pkt, pkt2, pkt3;
    char *dummytick = "Hello World!";
    unsigned int type;
    int testresult = 0;
    int currtest = 0;

    err = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    /*
     * For each test set up an SSL_CTX and SSL and see what ClientHello gets
     * produced when we try to connect
     */
    for (; currtest < TOTAL_NUM_TESTS; currtest++) {
        testresult = 0;
        ctx = SSL_CTX_new(TLS_method());
        if (!SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION))
            goto end;
        con = SSL_new(ctx);

        rbio = BIO_new(BIO_s_mem());
        wbio = BIO_new(BIO_s_mem());
        SSL_set_bio(con, rbio, wbio);
        SSL_set_connect_state(con);

        if (currtest == TEST_SET_SESSION_TICK_DATA_VER_NEG) {
            if (!SSL_set_session_ticket_ext(con, dummytick, strlen(dummytick)))
                goto end;
        }

        if (SSL_connect(con) > 0) {
            /* This shouldn't succeed because we don't have a server! */
            goto end;
        }

        len = BIO_get_mem_data(wbio, (char **)&data);
        if (!PACKET_buf_init(&pkt, data, len))
            goto end;

        /* Skip the record header */
        if (!PACKET_forward(&pkt, SSL3_RT_HEADER_LENGTH))
            goto end;

        /* Skip the handshake message header */
        if (!PACKET_forward(&pkt, SSL3_HM_HEADER_LENGTH))
            goto end;

        /* Skip client version and random */
        if (!PACKET_forward(&pkt, CLIENT_VERSION_LEN + SSL3_RANDOM_SIZE))
            goto end;

        /* Skip session id */
        if (!PACKET_get_length_prefixed_1(&pkt, &pkt2))
            goto end;

        /* Skip ciphers */
        if (!PACKET_get_length_prefixed_2(&pkt, &pkt2))
            goto end;

        /* Skip compression */
        if (!PACKET_get_length_prefixed_1(&pkt, &pkt2))
            goto end;

        /* Extensions len */
        if (!PACKET_as_length_prefixed_2(&pkt, &pkt2))
            goto end;

        /* Loop through all extensions */
        while (PACKET_remaining(&pkt2)) {

            if (!PACKET_get_net_2(&pkt2, &type) ||
                !PACKET_get_length_prefixed_2(&pkt2, &pkt3))
                goto end;

            if (type == TLSEXT_TYPE_session_ticket) {
                if (currtest == TEST_SET_SESSION_TICK_DATA_VER_NEG) {
                    if (PACKET_equal(&pkt3, dummytick, strlen(dummytick))) {
                        /* Ticket data is as we expected */
                        testresult = 1;
                    } else {
                        printf("Received session ticket is not as expected\n");
                    }
                    break;
                }
            }

        }

 end:
        SSL_free(con);
        SSL_CTX_free(ctx);
        if (!testresult) {
            printf("ClientHello test: FAILED (Test %d)\n", currtest);
            break;
        }
    }

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(err) <= 0)
        testresult = 0;
#endif
    BIO_free(err);

    return testresult?0:1;
}

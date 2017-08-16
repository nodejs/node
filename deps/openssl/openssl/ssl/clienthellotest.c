/* Written by Matt Caswell for the OpenSSL Project */
/* ====================================================================
 * Copyright (c) 1998-2015 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <string.h>

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


#define CLIENT_VERSION_LEN      2
#define SESSION_ID_LEN_LEN      1
#define CIPHERS_LEN_LEN         2
#define COMPRESSION_LEN_LEN     1
#define EXTENSIONS_LEN_LEN      2
#define EXTENSION_TYPE_LEN      2
#define EXTENSION_SIZE_LEN      2


#define TOTAL_NUM_TESTS                         2

/*
 * Test that explicitly setting ticket data results in it appearing in the
 * ClientHello for TLS1.2
 */
#define TEST_SET_SESSION_TICK_DATA_TLS_1_2      0

/*
 * Test that explicitly setting ticket data results in it appearing in the
 * ClientHello for a negotiated SSL/TLS version
 */
#define TEST_SET_SESSION_TICK_DATA_VER_NEG      1

int main(int argc, char *argv[])
{
    SSL_CTX *ctx;
    SSL *con;
    BIO *rbio;
    BIO *wbio;
    BIO *err;
    long len;
    unsigned char *data;
    unsigned char *dataend;
    char *dummytick = "Hello World!";
    unsigned int tmplen;
    unsigned int type;
    unsigned int size;
    int testresult = 0;
    int currtest = 0;

    SSL_library_init();
    SSL_load_error_strings();

    err = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    CRYPTO_malloc_debug_init();
    CRYPTO_set_mem_debug_options(V_CRYPTO_MDEBUG_ALL);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    /*
     * For each test set up an SSL_CTX and SSL and see what ClientHello gets
     * produced when we try to connect
     */
    for (; currtest < TOTAL_NUM_TESTS; currtest++) {
        testresult = 0;
        if (currtest == TEST_SET_SESSION_TICK_DATA_TLS_1_2) {
            ctx = SSL_CTX_new(TLSv1_2_method());
        } else {
            ctx = SSL_CTX_new(SSLv23_method());
        }
        con = SSL_new(ctx);

        rbio = BIO_new(BIO_s_mem());
        wbio = BIO_new(BIO_s_mem());
        SSL_set_bio(con, rbio, wbio);
        SSL_set_connect_state(con);

        if (currtest == TEST_SET_SESSION_TICK_DATA_TLS_1_2
                || currtest == TEST_SET_SESSION_TICK_DATA_VER_NEG) {
            if (!SSL_set_session_ticket_ext(con, dummytick, strlen(dummytick)))
                goto end;
        }

        if (SSL_connect(con) > 0) {
            /* This shouldn't succeed because we don't have a server! */
            goto end;
        }

        len = BIO_get_mem_data(wbio, (char **)&data);
        dataend = data + len;

        /* Skip the record header */
        data += SSL3_RT_HEADER_LENGTH;
        /* Skip the handshake message header */
        data += SSL3_HM_HEADER_LENGTH;
        /* Skip client version and random */
        data += CLIENT_VERSION_LEN + SSL3_RANDOM_SIZE;
        if (data + SESSION_ID_LEN_LEN > dataend)
            goto end;
        /* Skip session id */
        tmplen = *data;
        data += SESSION_ID_LEN_LEN + tmplen;
        if (data + CIPHERS_LEN_LEN > dataend)
            goto end;
        /* Skip ciphers */
        tmplen = ((*data) << 8) | *(data + 1);
        data += CIPHERS_LEN_LEN + tmplen;
        if (data + COMPRESSION_LEN_LEN > dataend)
            goto end;
        /* Skip compression */
        tmplen = *data;
        data += COMPRESSION_LEN_LEN + tmplen;
        if (data + EXTENSIONS_LEN_LEN > dataend)
            goto end;
        /* Extensions len */
        tmplen = ((*data) << 8) | *(data + 1);
        data += EXTENSIONS_LEN_LEN;
        if (data + tmplen > dataend)
            goto end;

        /* Loop through all extensions */
        while (tmplen > EXTENSION_TYPE_LEN + EXTENSION_SIZE_LEN) {
            type = ((*data) << 8) | *(data + 1);
            data += EXTENSION_TYPE_LEN;
            size = ((*data) << 8) | *(data + 1);
            data += EXTENSION_SIZE_LEN;
            if (data + size > dataend)
                goto end;

            if (type == TLSEXT_TYPE_session_ticket) {
                if (currtest == TEST_SET_SESSION_TICK_DATA_TLS_1_2
                        || currtest == TEST_SET_SESSION_TICK_DATA_VER_NEG) {
                    if (size == strlen(dummytick)
                            && memcmp(data, dummytick, size) == 0) {
                        /* Ticket data is as we expected */
                        testresult = 1;
                    } else {
                        printf("Received session ticket is not as expected\n");
                    }
                    break;
                }
            }

            tmplen -= EXTENSION_TYPE_LEN + EXTENSION_SIZE_LEN + size;
            data += size;
        }

 end:
        SSL_free(con);
        SSL_CTX_free(ctx);
        if (!testresult) {
            printf("ClientHello test: FAILED (Test %d)\n", currtest);
            break;
        }
    }

    ERR_free_strings();
    ERR_remove_thread_state(NULL);
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    CRYPTO_mem_leaks(err);
    BIO_free(err);

    return testresult?0:1;
}

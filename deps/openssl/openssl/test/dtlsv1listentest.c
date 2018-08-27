/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#ifndef OPENSSL_NO_ENGINE
 #include <openssl/engine.h>
#endif
#include "e_os.h"

#ifndef OPENSSL_NO_SOCK

/* Just a ClientHello without a cookie */
static const unsigned char clienthello_nocookie[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x3A, /* Record Length */
    0x01, /* ClientHello */
    0x00, 0x00, 0x2E, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x00, /* Fragment offset */
    0x00, 0x00, 0x2E, /* Fragment length */
    0xFE, 0xFD, /* DTLSv1.2 */
    0xCA, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
    0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
    0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, /* Random */
    0x00, /* Session id len */
    0x00, /* Cookie len */
    0x00, 0x04, /* Ciphersuites len */
    0x00, 0x2f, /* AES128-SHA */
    0x00, 0xff, /* Empty reneg info SCSV */
    0x01, /* Compression methods len */
    0x00, /* Null compression */
    0x00, 0x00 /* Extensions len */
};

/* First fragment of a ClientHello without a cookie */
static const unsigned char clienthello_nocookie_frag[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x30, /* Record Length */
    0x01, /* ClientHello */
    0x00, 0x00, 0x2E, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x00, /* Fragment offset */
    0x00, 0x00, 0x24, /* Fragment length */
    0xFE, 0xFD, /* DTLSv1.2 */
    0xCA, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
    0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
    0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, /* Random */
    0x00, /* Session id len */
    0x00 /* Cookie len */
};

/* First fragment of a ClientHello which is too short */
static const unsigned char clienthello_nocookie_short[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x2F, /* Record Length */
    0x01, /* ClientHello */
    0x00, 0x00, 0x2E, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x00, /* Fragment offset */
    0x00, 0x00, 0x23, /* Fragment length */
    0xFE, 0xFD, /* DTLSv1.2 */
    0xCA, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
    0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
    0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, /* Random */
    0x00 /* Session id len */
};

/* Second fragment of a ClientHello */
static const unsigned char clienthello_2ndfrag[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x38, /* Record Length */
    0x01, /* ClientHello */
    0x00, 0x00, 0x2E, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x02, /* Fragment offset */
    0x00, 0x00, 0x2C, /* Fragment length */
    /* Version skipped - sent in first fragment */
    0xCA, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
    0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
    0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, /* Random */
    0x00, /* Session id len */
    0x00, /* Cookie len */
    0x00, 0x04, /* Ciphersuites len */
    0x00, 0x2f, /* AES128-SHA */
    0x00, 0xff, /* Empty reneg info SCSV */
    0x01, /* Compression methods len */
    0x00, /* Null compression */
    0x00, 0x00 /* Extensions len */
};

/* A ClientHello with a good cookie */
static const unsigned char clienthello_cookie[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x4E, /* Record Length */
    0x01, /* ClientHello */
    0x00, 0x00, 0x42, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x00, /* Fragment offset */
    0x00, 0x00, 0x42, /* Fragment length */
    0xFE, 0xFD, /* DTLSv1.2 */
    0xCA, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
    0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
    0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, /* Random */
    0x00, /* Session id len */
    0x14, /* Cookie len */
    0x00, 0x01, 0x02, 0x03, 0x04, 005, 0x06, 007, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, /* Cookie */
    0x00, 0x04, /* Ciphersuites len */
    0x00, 0x2f, /* AES128-SHA */
    0x00, 0xff, /* Empty reneg info SCSV */
    0x01, /* Compression methods len */
    0x00, /* Null compression */
    0x00, 0x00 /* Extensions len */
};

/* A fragmented ClientHello with a good cookie */
static const unsigned char clienthello_cookie_frag[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x44, /* Record Length */
    0x01, /* ClientHello */
    0x00, 0x00, 0x42, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x00, /* Fragment offset */
    0x00, 0x00, 0x38, /* Fragment length */
    0xFE, 0xFD, /* DTLSv1.2 */
    0xCA, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
    0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
    0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, /* Random */
    0x00, /* Session id len */
    0x14, /* Cookie len */
    0x00, 0x01, 0x02, 0x03, 0x04, 005, 0x06, 007, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13 /* Cookie */
};


/* A ClientHello with a bad cookie */
static const unsigned char clienthello_badcookie[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x4E, /* Record Length */
    0x01, /* ClientHello */
    0x00, 0x00, 0x42, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x00, /* Fragment offset */
    0x00, 0x00, 0x42, /* Fragment length */
    0xFE, 0xFD, /* DTLSv1.2 */
    0xCA, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
    0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
    0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, /* Random */
    0x00, /* Session id len */
    0x14, /* Cookie len */
    0x01, 0x01, 0x02, 0x03, 0x04, 005, 0x06, 007, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, /* Cookie */
    0x00, 0x04, /* Ciphersuites len */
    0x00, 0x2f, /* AES128-SHA */
    0x00, 0xff, /* Empty reneg info SCSV */
    0x01, /* Compression methods len */
    0x00, /* Null compression */
    0x00, 0x00 /* Extensions len */
};

/* A fragmented ClientHello with the fragment boundary mid cookie */
static const unsigned char clienthello_cookie_short[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x43, /* Record Length */
    0x01, /* ClientHello */
    0x00, 0x00, 0x42, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x00, /* Fragment offset */
    0x00, 0x00, 0x37, /* Fragment length */
    0xFE, 0xFD, /* DTLSv1.2 */
    0xCA, 0x18, 0x9F, 0x76, 0xEC, 0x57, 0xCE, 0xE5, 0xB3, 0xAB, 0x79, 0x90,
    0xAD, 0xAC, 0x6E, 0xD1, 0x58, 0x35, 0x03, 0x97, 0x16, 0x10, 0x82, 0x56,
    0xD8, 0x55, 0xFF, 0xE1, 0x8A, 0xA3, 0x2E, 0xF6, /* Random */
    0x00, /* Session id len */
    0x14, /* Cookie len */
    0x00, 0x01, 0x02, 0x03, 0x04, 005, 0x06, 007, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12 /* Cookie */
};

/* Bad record - too short */
static const unsigned char record_short[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00 /* Record sequence number */
};

static const unsigned char verify[] = {
    0x16, /* Handshake */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x00, 0x00, /* Epoch */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Record sequence number */
    0x00, 0x23, /* Record Length */
    0x03, /* HelloVerifyRequest */
    0x00, 0x00, 0x17, /* Message length */
    0x00, 0x00, /* Message sequence */
    0x00, 0x00, 0x00, /* Fragment offset */
    0x00, 0x00, 0x17, /* Fragment length */
    0xFE, 0xFF, /* DTLSv1.0 */
    0x14, /* Cookie len */
    0x00, 0x01, 0x02, 0x03, 0x04, 005, 0x06, 007, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13 /* Cookie */
};

static struct {
    const unsigned char *in;
    unsigned int inlen;
    /*
     * GOOD == positive return value from DTLSv1_listen, no output yet
     * VERIFY == 0 return value, HelloVerifyRequest sent
     * DROP == 0 return value, no output
     */
    enum {GOOD, VERIFY, DROP} outtype;
} testpackets[9] = {
    {
        clienthello_nocookie,
        sizeof(clienthello_nocookie),
        VERIFY
    },
    {
        clienthello_nocookie_frag,
        sizeof(clienthello_nocookie_frag),
        VERIFY
    },
    {
        clienthello_nocookie_short,
        sizeof(clienthello_nocookie_short),
        DROP
    },
    {
        clienthello_2ndfrag,
        sizeof(clienthello_2ndfrag),
        DROP
    },
    {
        clienthello_cookie,
        sizeof(clienthello_cookie),
        GOOD
    },
    {
        clienthello_cookie_frag,
        sizeof(clienthello_cookie_frag),
        GOOD
    },
    {
        clienthello_badcookie,
        sizeof(clienthello_badcookie),
        VERIFY
    },
    {
        clienthello_cookie_short,
        sizeof(clienthello_cookie_short),
        DROP
    },
    {
        record_short,
        sizeof(record_short),
        DROP
    }
};

# define COOKIE_LEN  20

static int cookie_gen(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len)
{
    unsigned int i;

    for (i = 0; i < COOKIE_LEN; i++, cookie++) {
        *cookie = i;
    }
    *cookie_len = COOKIE_LEN;

    return 1;
}

static int cookie_verify(SSL *ssl, const unsigned char *cookie,
                         unsigned int cookie_len)
{
    unsigned int i;

    if (cookie_len != COOKIE_LEN)
        return 0;

    for (i = 0; i < COOKIE_LEN; i++, cookie++) {
        if (*cookie != i)
            return 0;
    }

    return 1;
}
#endif

int main(void)
{
#ifndef OPENSSL_NO_SOCK
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    BIO *outbio = NULL;
    BIO *inbio = NULL;
    BIO_ADDR *peer = BIO_ADDR_new();
    char *data;
    long datalen;
    int ret, success = 0;
    long i;

    ctx = SSL_CTX_new(DTLS_server_method());
    if (ctx == NULL || peer == NULL)
        goto err;

    SSL_CTX_set_cookie_generate_cb(ctx, cookie_gen);
    SSL_CTX_set_cookie_verify_cb(ctx, cookie_verify);

    /* Create an SSL object for the connection */
    ssl = SSL_new(ctx);
    if (ssl == NULL)
        goto err;

    outbio = BIO_new(BIO_s_mem());
    if (outbio == NULL)
        goto err;
    SSL_set0_wbio(ssl, outbio);

    success = 1;
    for (i = 0; i < (long)OSSL_NELEM(testpackets) && success; i++) {
        inbio = BIO_new_mem_buf((char *)testpackets[i].in,
                                testpackets[i].inlen);
        if (inbio == NULL) {
            success = 0;
            goto err;
        }
        /* Set Non-blocking IO behaviour */
        BIO_set_mem_eof_return(inbio, -1);

        SSL_set0_rbio(ssl, inbio);

        /* Process the incoming packet */
        ret = DTLSv1_listen(ssl, peer);
        if (ret < 0) {
            success = 0;
            goto err;
        }

        datalen = BIO_get_mem_data(outbio, &data);

        if (testpackets[i].outtype == VERIFY) {
            if (ret == 0) {
                if (datalen != sizeof(verify)
                        || (memcmp(data, verify, sizeof(verify)) != 0)) {
                    printf("Test %ld failure: incorrect HelloVerifyRequest\n", i);
                    success = 0;
                } else {
                    printf("Test %ld success\n", i);
                }
            } else {
                printf ("Test %ld failure: should not have succeeded\n", i);
                success = 0;
            }
        } else if (datalen == 0) {
            if ((ret == 0 && testpackets[i].outtype == DROP)
                    || (ret == 1 && testpackets[i].outtype == GOOD)) {
                printf("Test %ld success\n", i);
            } else {
                printf("Test %ld failure: wrong return value\n", i);
                success = 0;
            }
        } else {
            printf("Test %ld failure: Unexpected data output\n", i);
            success = 0;
        }
        (void)BIO_reset(outbio);
        inbio = NULL;
        /* Frees up inbio */
        SSL_set0_rbio(ssl, NULL);
    }

 err:
    if (!success)
        ERR_print_errors_fp(stderr);
    /* Also frees up outbio */
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    BIO_free(inbio);
    OPENSSL_free(peer);
# ifndef OPENSSL_NO_CRYPTO_MDEBUG
    CRYPTO_mem_leaks_fp(stderr);
# endif
    return success ? 0 : 1;
#else
    printf("DTLSv1_listen() is not supported by this build - skipping\n");
    return 0;
#endif
}

/*
 * Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL licenses, (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */

#include <string.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "../ssl/packet_locl.h"

#include "ssltestlib.h"

/* Should we fragment records or not? 0 = no, !0 = yes*/
static int fragment = 0;

static int async_new(BIO *bi);
static int async_free(BIO *a);
static int async_read(BIO *b, char *out, int outl);
static int async_write(BIO *b, const char *in, int inl);
static long async_ctrl(BIO *b, int cmd, long num, void *ptr);
static int async_gets(BIO *bp, char *buf, int size);
static int async_puts(BIO *bp, const char *str);

/* Choose a sufficiently large type likely to be unused for this custom BIO */
# define BIO_TYPE_ASYNC_FILTER  (0x80 | BIO_TYPE_FILTER)

static BIO_METHOD *methods_async = NULL;

struct async_ctrs {
    unsigned int rctr;
    unsigned int wctr;
};

static const BIO_METHOD *bio_f_async_filter()
{
    if (methods_async == NULL) {
        methods_async = BIO_meth_new(BIO_TYPE_ASYNC_FILTER, "Async filter");
        if (   methods_async == NULL
            || !BIO_meth_set_write(methods_async, async_write)
            || !BIO_meth_set_read(methods_async, async_read)
            || !BIO_meth_set_puts(methods_async, async_puts)
            || !BIO_meth_set_gets(methods_async, async_gets)
            || !BIO_meth_set_ctrl(methods_async, async_ctrl)
            || !BIO_meth_set_create(methods_async, async_new)
            || !BIO_meth_set_destroy(methods_async, async_free))
            return NULL;
    }
    return methods_async;
}

static int async_new(BIO *bio)
{
    struct async_ctrs *ctrs;

    ctrs = OPENSSL_zalloc(sizeof(struct async_ctrs));
    if (ctrs == NULL)
        return 0;

    BIO_set_data(bio, ctrs);
    BIO_set_init(bio, 1);
    return 1;
}

static int async_free(BIO *bio)
{
    struct async_ctrs *ctrs;

    if (bio == NULL)
        return 0;
    ctrs = BIO_get_data(bio);
    OPENSSL_free(ctrs);
    BIO_set_data(bio, NULL);
    BIO_set_init(bio, 0);

    return 1;
}

static int async_read(BIO *bio, char *out, int outl)
{
    struct async_ctrs *ctrs;
    int ret = -1;
    BIO *next = BIO_next(bio);

    if (outl <= 0)
        return 0;
    if (next == NULL)
        return 0;

    ctrs = BIO_get_data(bio);

    BIO_clear_retry_flags(bio);

    if (ctrs->rctr > 0) {
        ret = BIO_read(next, out, 1);
        if (ret <= 0 && BIO_should_read(next))
            BIO_set_retry_read(bio);
        ctrs->rctr = 0;
    } else {
        ctrs->rctr++;
        BIO_set_retry_read(bio);
    }

    return ret;
}

#define MIN_RECORD_LEN  6

#define CONTENTTYPEPOS  0
#define VERSIONHIPOS    1
#define VERSIONLOPOS    2
#define DATAPOS         5

static int async_write(BIO *bio, const char *in, int inl)
{
    struct async_ctrs *ctrs;
    int ret = -1;
    size_t written = 0;
    BIO *next = BIO_next(bio);

    if (inl <= 0)
        return 0;
    if (next == NULL)
        return 0;

    ctrs = BIO_get_data(bio);

    BIO_clear_retry_flags(bio);

    if (ctrs->wctr > 0) {
        ctrs->wctr = 0;
        if (fragment) {
            PACKET pkt;

            if (!PACKET_buf_init(&pkt, (const unsigned char *)in, inl))
                abort();

            while (PACKET_remaining(&pkt) > 0) {
                PACKET payload;
                unsigned int contenttype, versionhi, versionlo, data;

                if (   !PACKET_get_1(&pkt, &contenttype)
                    || !PACKET_get_1(&pkt, &versionhi)
                    || !PACKET_get_1(&pkt, &versionlo)
                    || !PACKET_get_length_prefixed_2(&pkt, &payload))
                    abort();

                /* Pretend we wrote out the record header */
                written += SSL3_RT_HEADER_LENGTH;

                while (PACKET_get_1(&payload, &data)) {
                    /* Create a new one byte long record for each byte in the
                     * record in the input buffer
                     */
                    char smallrec[MIN_RECORD_LEN] = {
                        0, /* Content type */
                        0, /* Version hi */
                        0, /* Version lo */
                        0, /* Length hi */
                        1, /* Length lo */
                        0  /* Data */
                    };

                    smallrec[CONTENTTYPEPOS] = contenttype;
                    smallrec[VERSIONHIPOS] = versionhi;
                    smallrec[VERSIONLOPOS] = versionlo;
                    smallrec[DATAPOS] = data;
                    ret = BIO_write(next, smallrec, MIN_RECORD_LEN);
                    if (ret <= 0)
                        abort();
                    written++;
                }
                /*
                 * We can't fragment anything after the CCS, otherwise we
                 * get a bad record MAC
                 */
                if (contenttype == SSL3_RT_CHANGE_CIPHER_SPEC) {
                    fragment = 0;
                    break;
                }
            }
        }
        /* Write any data we have left after fragmenting */
        ret = 0;
        if ((int)written < inl) {
            ret = BIO_write(next, in + written , inl - written);
        }

        if (ret <= 0 && BIO_should_write(next))
            BIO_set_retry_write(bio);
        else
            ret += written;
    } else {
        ctrs->wctr++;
        BIO_set_retry_write(bio);
    }

    return ret;
}

static long async_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
    long ret;
    BIO *next = BIO_next(bio);

    if (next == NULL)
        return 0;

    switch (cmd) {
    case BIO_CTRL_DUP:
        ret = 0L;
        break;
    default:
        ret = BIO_ctrl(next, cmd, num, ptr);
        break;
    }
    return ret;
}

static int async_gets(BIO *bio, char *buf, int size)
{
    /* We don't support this - not needed anyway */
    return -1;
}

static int async_puts(BIO *bio, const char *str)
{
    return async_write(bio, str, strlen(str));
}

#define MAX_ATTEMPTS    100

int main(int argc, char *argv[])
{
    SSL_CTX *serverctx = NULL, *clientctx = NULL;
    SSL *serverssl = NULL, *clientssl = NULL;
    BIO *s_to_c_fbio = NULL, *c_to_s_fbio = NULL;
    int test, err = 1, ret;
    size_t i, j;
    const char testdata[] = "Test data";
    char buf[sizeof(testdata)];

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    if (argc != 3) {
        printf("Invalid argument count\n");
        goto end;
    }

    if (!create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                             TLS1_VERSION, TLS_MAX_VERSION,
                             &serverctx, &clientctx, argv[1], argv[2])) {
        printf("Failed to create SSL_CTX pair\n");
        goto end;
    }

    /*
     * We do 2 test runs. The first time around we just do a normal handshake
     * with lots of async io going on. The second time around we also break up
     * all records so that the content is only one byte length (up until the
     * CCS)
     */
    for (test = 1; test < 3; test++) {
        if (test == 2)
            fragment = 1;


        s_to_c_fbio = BIO_new(bio_f_async_filter());
        c_to_s_fbio = BIO_new(bio_f_async_filter());
        if (s_to_c_fbio == NULL || c_to_s_fbio == NULL) {
            printf("Failed to create filter BIOs\n");
            BIO_free(s_to_c_fbio);
            BIO_free(c_to_s_fbio);
            goto end;
        }

        /* BIOs get freed on error */
        if (!create_ssl_objects(serverctx, clientctx, &serverssl, &clientssl,
                                s_to_c_fbio, c_to_s_fbio)) {
            printf("Test %d failed: Create SSL objects failed\n", test);
            goto end;
        }

        if (!create_ssl_connection(serverssl, clientssl)) {
            printf("Test %d failed: Create SSL connection failed\n", test);
            goto end;
        }

        /*
         * Send and receive some test data. Do the whole thing twice to ensure
         * we hit at least one async event in both reading and writing
         */
        for (j = 0; j < 2; j++) {
            int len;

            /*
             * Write some test data. It should never take more than 2 attempts
             * (the first one might be a retryable fail).
             */
            for (ret = -1, i = 0, len = 0; len != sizeof(testdata) && i < 2;
                i++) {
                ret = SSL_write(clientssl, testdata + len,
                    sizeof(testdata) - len);
                if (ret > 0) {
                    len += ret;
                } else {
                    int ssl_error = SSL_get_error(clientssl, ret);

                    if (ssl_error == SSL_ERROR_SYSCALL ||
                        ssl_error == SSL_ERROR_SSL) {
                        printf("Test %d failed: Failed to write app data\n", test);
                        err = -1;
                        goto end;
                    }
                }
            }
            if (len != sizeof(testdata)) {
                err = -1;
                printf("Test %d failed: Failed to write all app data\n", test);
                goto end;
            }
            /*
             * Now read the test data. It may take more attempts here because
             * it could fail once for each byte read, including all overhead
             * bytes from the record header/padding etc.
             */
            for (ret = -1, i = 0, len = 0; len != sizeof(testdata) &&
                i < MAX_ATTEMPTS; i++)
            {
                ret = SSL_read(serverssl, buf + len, sizeof(buf) - len);
                if (ret > 0) {
                    len += ret;
                } else {
                    int ssl_error = SSL_get_error(serverssl, ret);

                    if (ssl_error == SSL_ERROR_SYSCALL ||
                        ssl_error == SSL_ERROR_SSL) {
                        printf("Test %d failed: Failed to read app data\n", test);
                        err = -1;
                        goto end;
                    }
                }
            }
            if (len != sizeof(testdata)
                    || memcmp(buf, testdata, sizeof(testdata)) != 0) {
                err = -1;
                printf("Test %d failed: Unexpected app data received\n", test);
                goto end;
            }
        }

        /* Also frees the BIOs */
        SSL_free(clientssl);
        SSL_free(serverssl);
        clientssl = serverssl = NULL;
    }

    printf("Test success\n");

    err = 0;
 end:
    if (err)
        ERR_print_errors_fp(stderr);

    SSL_free(clientssl);
    SSL_free(serverssl);
    SSL_CTX_free(clientctx);
    SSL_CTX_free(serverctx);

# ifndef OPENSSL_NO_CRYPTO_MDEBUG
    CRYPTO_mem_leaks_fp(stderr);
# endif

    return err;
}

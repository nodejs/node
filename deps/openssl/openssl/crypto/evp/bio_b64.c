/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <errno.h>
#include "internal/cryptlib.h"
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include "internal/bio.h"
#include "crypto/evp.h"

static int b64_write(BIO *h, const char *buf, int num);
static int b64_read(BIO *h, char *buf, int size);
static int b64_puts(BIO *h, const char *str);
static long b64_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int b64_new(BIO *h);
static int b64_free(BIO *data);
static long b64_callback_ctrl(BIO *h, int cmd, BIO_info_cb *fp);
#define B64_BLOCK_SIZE 1024
#define B64_BLOCK_SIZE2 768
#define B64_NONE 0
#define B64_ENCODE 1
#define B64_DECODE 2

typedef struct b64_struct {
    /*
     * BIO *bio; moved to the BIO structure
     */
    int buf_len;
    int buf_off;
    int tmp_len; /* used to find the start when decoding */
    int tmp_nl; /* If true, scan until '\n' */
    int encode;
    int start; /* have we started decoding yet? */
    int cont; /* <= 0 when finished */
    EVP_ENCODE_CTX *base64;
    unsigned char buf[EVP_ENCODE_LENGTH(B64_BLOCK_SIZE) + 10];
    unsigned char tmp[B64_BLOCK_SIZE];
    unsigned char *encoded_buf;
    size_t encoded_buf_len;
    size_t encoded_len;
    size_t encoded_off;
} BIO_B64_CTX;

static const BIO_METHOD methods_b64 = {
    BIO_TYPE_BASE64,
    "base64 encoding",
    bwrite_conv,
    b64_write,
    bread_conv,
    b64_read,
    b64_puts,
    NULL, /* b64_gets, */
    b64_ctrl,
    b64_new,
    b64_free,
    b64_callback_ctrl,
};

const BIO_METHOD *BIO_f_base64(void)
{
    return &methods_b64;
}

static int b64_new(BIO *bi)
{
    BIO_B64_CTX *ctx;

    if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) == NULL)
        return 0;

    ctx->cont = 1;
    ctx->start = 1;
    ctx->encoded_buf = NULL;
    ctx->encoded_buf_len = 0;
    ctx->base64 = EVP_ENCODE_CTX_new();
    if (ctx->base64 == NULL) {
        OPENSSL_free(ctx);
        return 0;
    }

    BIO_set_data(bi, ctx);
    BIO_set_init(bi, 1);

    return 1;
}

static int b64_free(BIO *a)
{
    BIO_B64_CTX *ctx;

    if (a == NULL)
        return 0;

    ctx = BIO_get_data(a);
    if (ctx == NULL)
        return 0;

    OPENSSL_free(ctx->encoded_buf);
    ctx->encoded_buf = NULL;
    ctx->encoded_buf_len = 0;
    EVP_ENCODE_CTX_free(ctx->base64);
    OPENSSL_free(ctx);
    BIO_set_data(a, NULL);
    BIO_set_init(a, 0);

    return 1;
}

static int b64_write_buffer(BIO *b, BIO *next, unsigned char *buf,
    size_t *buf_len, size_t *buf_off)
{
    size_t n;
    int i, dlen;

    if (!ossl_assert(*buf_len >= *buf_off)) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }

    n = *buf_len - *buf_off;
    if (n > 0 && !ossl_assert(buf != NULL)) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }
    while (n > 0) {
        if (n > INT_MAX)
            dlen = INT_MAX;
        else
            dlen = (int)n;
        i = BIO_write(next, &(buf[*buf_off]), dlen);
        if (i <= 0) {
            BIO_copy_next_retry(b);
            return i;
        }
        if (!ossl_assert((size_t)i <= n)) {
            ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
            return -1;
        }
        *buf_off += (size_t)i;
        n -= (size_t)i;
    }
    *buf_off = 0;
    *buf_len = 0;

    return 1;
}

static int b64_write_buffer_int(BIO *b, BIO *next, unsigned char *buf,
    int *buf_len, int *buf_off)
{
    int ret;
    size_t len;
    size_t off;

    if (!ossl_assert(*buf_off >= 0)) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }

    len = (size_t)*buf_len;
    off = (size_t)*buf_off;
    ret = b64_write_buffer(b, next, buf, &len, &off);

    *buf_len = (int)len;
    *buf_off = (int)off;
    return ret;
}

static int b64_write_pending(BIO_B64_CTX *ctx)
{
    return ctx->encoded_len != ctx->encoded_off
        || ctx->buf_len != ctx->buf_off;
}

/*
 * Unless `BIO_FLAGS_BASE64_NO_NL` is set, this BIO ignores leading lines that
 * aren't exclusively composed of valid Base64 characters (followed by <CRLF>
 * or <LF>).  Once a valid Base64 line is found, `ctx->start` is set to 0 and
 * lines are processed until EOF or the first line that contains invalid Base64
 * characters.  In a nod to PEM, lines that start with a '-' (hyphen) are
 * treated as a soft EOF, rather than an error.
 */
static int b64_read(BIO *b, char *out, int outl)
{
    int ret = 0, i, ii, j, k, x, n, num, ret_code;
    BIO_B64_CTX *ctx;
    unsigned char *p, *q;
    BIO *next;

    if (out == NULL)
        return 0;
    ctx = (BIO_B64_CTX *)BIO_get_data(b);

    next = BIO_next(b);
    if (ctx == NULL || next == NULL)
        return 0;

    BIO_clear_retry_flags(b);

    if (ctx->encode != B64_DECODE) {
        ctx->encode = B64_DECODE;
        ctx->buf_len = 0;
        ctx->buf_off = 0;
        ctx->tmp_len = 0;
        ctx->encoded_len = 0;
        ctx->encoded_off = 0;
        EVP_DecodeInit(ctx->base64);
    }

    /* First check if there are buffered bytes already decoded */
    if (ctx->buf_len > 0) {
        if (!ossl_assert(ctx->buf_len >= ctx->buf_off)) {
            ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
            return -1;
        }
        i = ctx->buf_len - ctx->buf_off;
        if (i > outl)
            i = outl;
        if (!ossl_assert(ctx->buf_off + i < (int)sizeof(ctx->buf))) {
            ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
            return -1;
        }
        memcpy(out, &(ctx->buf[ctx->buf_off]), i);
        ret = i;
        out += i;
        outl -= i;
        ctx->buf_off += i;
        if (ctx->buf_len == ctx->buf_off) {
            ctx->buf_len = 0;
            ctx->buf_off = 0;
        }
    }

    /* Restore any non-retriable error condition (ctx->cont < 0) */
    ret_code = ctx->cont < 0 ? ctx->cont : 0;

    /*
     * At this point, we have room of outl bytes and an either an empty buffer,
     * or outl == 0, so we'll attempt to read in some more.
     */
    while (outl > 0) {
        int again = ctx->cont;

        if (again <= 0)
            break;

        i = BIO_read(next, &(ctx->tmp[ctx->tmp_len]),
            B64_BLOCK_SIZE - ctx->tmp_len);

        if (i <= 0) {
            ret_code = i;

            /* Should we continue next time we are called? */
            if (!BIO_should_retry(next)) {
                /* Incomplete final Base64 chunk in the decoder is an error */
                if (ctx->tmp_len == 0) {
                    if (EVP_DecodeFinal(ctx->base64, NULL, &num) < 0)
                        ret_code = -1;
                    EVP_DecodeInit(ctx->base64);
                }
                ctx->cont = ret_code;
            }
            if (ctx->tmp_len == 0)
                break;
            /* Fall through and process what we have */
            i = 0;
            /* But don't loop to top-up even if the buffer is not full! */
            again = 0;
        }

        i += ctx->tmp_len;
        ctx->tmp_len = i;

        /*
         * We need to scan, a line at a time until we have a valid line if we
         * are starting.
         */
        if (ctx->start && (BIO_get_flags(b) & BIO_FLAGS_BASE64_NO_NL) != 0) {
            ctx->tmp_len = 0;
        } else if (ctx->start) {
            q = p = ctx->tmp;
            num = 0;
            for (j = 0; j < i; j++) {
                if (*(q++) != '\n')
                    continue;

                /*
                 * due to a previous very long line, we need to keep on
                 * scanning for a '\n' before we even start looking for
                 * base64 encoded stuff.
                 */
                if (ctx->tmp_nl) {
                    p = q;
                    ctx->tmp_nl = 0;
                    continue;
                }

                k = EVP_DecodeUpdate(ctx->base64, ctx->buf, &num, p, (int)(q - p));
                EVP_DecodeInit(ctx->base64);
                if (k <= 0 && num == 0) {
                    p = q;
                    continue;
                }

                ctx->start = 0;
                if (p != ctx->tmp) {
                    i -= (int)(p - ctx->tmp);
                    for (x = 0; x < i; x++)
                        ctx->tmp[x] = p[x];
                }
                break;
            }

            /* we fell off the end without starting */
            if (ctx->start) {
                /*
                 * Is this is one long chunk?, if so, keep on reading until a
                 * new line.
                 */
                if (p == ctx->tmp) {
                    /* Check buffer full */
                    if (i == B64_BLOCK_SIZE) {
                        ctx->tmp_nl = 1;
                        ctx->tmp_len = 0;
                    }
                } else if (p != q) {
                    /* Retain partial line at end of buffer */
                    n = (int)(q - p);
                    for (ii = 0; ii < n; ii++)
                        ctx->tmp[ii] = p[ii];
                    ctx->tmp_len = n;
                } else {
                    /* All we have is newline terminated non-start data */
                    ctx->tmp_len = 0;
                }
                /*
                 * Try to read more if possible, otherwise we can't make
                 * progress unless the underlying BIO is retriable and may
                 * produce more data next time we're called.
                 */
                if (again > 0)
                    continue;
                else
                    break;
            } else {
                ctx->tmp_len = 0;
            }
        } else if (i < B64_BLOCK_SIZE && again > 0) {
            /*
             * If buffer isn't full and we can retry then restart to read in
             * more data.
             */
            continue;
        }

        i = EVP_DecodeUpdate(ctx->base64, ctx->buf, &ctx->buf_len,
            ctx->tmp, i);
        ctx->tmp_len = 0;
        /*
         * If eof or an error was signalled, then the condition
         * 'ctx->cont <= 0' will prevent b64_read() from reading
         * more data on subsequent calls. This assignment was
         * deleted accidentally in commit 5562cfaca4f3.
         */
        ctx->cont = i;

        ctx->buf_off = 0;
        if (i < 0) {
            ret_code = ctx->start ? 0 : i;
            ctx->buf_len = 0;
            break;
        }

        if (ctx->buf_len <= outl)
            i = ctx->buf_len;
        else
            i = outl;

        memcpy(out, ctx->buf, i);
        ret += i;
        ctx->buf_off = i;
        if (ctx->buf_off == ctx->buf_len) {
            ctx->buf_len = 0;
            ctx->buf_off = 0;
        }
        outl -= i;
        out += i;
    }
    BIO_copy_next_retry(b);
    return ret == 0 ? ret_code : ret;
}

static int b64_write(BIO *b, const char *in, int inl)
{
    int ret = 0;
    int i;
    BIO_B64_CTX *ctx;
    BIO *next;
    size_t encoded_length;
    unsigned char *encoded;
    int n_bytes_enc;

    ctx = (BIO_B64_CTX *)BIO_get_data(b);
    next = BIO_next(b);
    if (ctx == NULL || next == NULL)
        return 0;

    BIO_clear_retry_flags(b);

    if (ctx->encode != B64_ENCODE) {
        ctx->encode = B64_ENCODE;
        ctx->buf_len = 0;
        ctx->buf_off = 0;
        ctx->tmp_len = 0;
        ctx->encoded_len = 0;
        ctx->encoded_off = 0;
        EVP_EncodeInit(ctx->base64);
        if (BIO_get_flags(b) & BIO_FLAGS_BASE64_NO_NL)
            evp_encode_ctx_set_flags(ctx->base64, EVP_ENCODE_CTX_NO_NEWLINES);
    }
    if (!ossl_assert(ctx->buf_off < (int)sizeof(ctx->buf))) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }
    if (!ossl_assert(ctx->buf_len <= (int)sizeof(ctx->buf))) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }
    if (!ossl_assert(ctx->buf_len >= ctx->buf_off)) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }
    if (!ossl_assert(ctx->encoded_len >= ctx->encoded_off)) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }
    if (!ossl_assert(ctx->encoded_len <= ctx->encoded_buf_len)) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }

    i = b64_write_buffer(b, next, ctx->encoded_buf, &ctx->encoded_len,
        &ctx->encoded_off);
    if (i <= 0)
        return i;
    i = b64_write_buffer_int(b, next, ctx->buf, &ctx->buf_len, &ctx->buf_off);
    if (i <= 0)
        return i;
    if (!ossl_assert(ctx->buf_off <= (int)sizeof(ctx->buf))) {
        ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
        return -1;
    }

    if (in == NULL || inl <= 0)
        return 0;

    encoded_length = EVP_ENCODE_LENGTH(inl);

    if (encoded_length > SIZE_MAX / 2) {
        ERR_raise(ERR_LIB_BIO, BIO_R_LENGTH_TOO_LONG);
        return -1;
    }

    if (ctx->encoded_buf == NULL || encoded_length > ctx->encoded_buf_len) {
        OPENSSL_free(ctx->encoded_buf);
        ctx->encoded_buf = NULL;
        ctx->encoded_buf_len = 0;
        ctx->encoded_buf = OPENSSL_malloc(encoded_length);
        if (ctx->encoded_buf == NULL) {
            ERR_raise(ERR_LIB_BIO, ERR_R_MALLOC_FAILURE);
            return -1;
        }
        ctx->encoded_buf_len = encoded_length;
    }

    encoded = ctx->encoded_buf;

    if (encoded == NULL) {
        ERR_raise(ERR_LIB_BIO, ERR_R_MALLOC_FAILURE);
        return -1;
    }
    n_bytes_enc = 0;
    if (!EVP_EncodeUpdate(ctx->base64, encoded, &n_bytes_enc,
            (unsigned char *)in, inl)) {
        return -1;
    }
    /*
     * The encoder state has consumed the input.  Keep any unwritten encoded
     * output pending so a later retry or flush can complete it.
     */
    ret += inl;
    ctx->encoded_len = (size_t)n_bytes_enc;
    ctx->encoded_off = 0;
    /*
     * Try to write it now, but the input is consumed into encoder state, so
     * report it as written even if downstream cannot accept all encoded output.
     */
    (void)b64_write_buffer(b, next, ctx->encoded_buf, &ctx->encoded_len,
        &ctx->encoded_off);
    return ret;
}

static long b64_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    BIO_B64_CTX *ctx;
    long ret = 1;
    int i;
    BIO *next;

    ctx = (BIO_B64_CTX *)BIO_get_data(b);
    next = BIO_next(b);
    /*
     * If there is no ctx or no next BIO, BIO_read() returns 0, which means EOF.
     * BIO_eof() should return 1 in this case.
     */
    if (ctx == NULL || next == NULL)
        return cmd == BIO_CTRL_EOF;

    switch (cmd) {
    case BIO_CTRL_RESET:
        ctx->cont = 1;
        ctx->start = 1;
        ctx->encode = B64_NONE;
        ctx->encoded_len = 0;
        ctx->encoded_off = 0;
        ret = BIO_ctrl(next, cmd, num, ptr);
        break;
    case BIO_CTRL_EOF: /* More to read */
        if (ctx->cont <= 0)
            ret = 1;
        else
            ret = BIO_ctrl(next, cmd, num, ptr);
        break;
    case BIO_CTRL_WPENDING: /* More to write in buffer */
        if (!ossl_assert(ctx->encoded_len >= ctx->encoded_off)) {
            ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
            return -1;
        }
        if (!ossl_assert(ctx->buf_len >= ctx->buf_off)) {
            ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
            return -1;
        }
        ret = (long)(ctx->encoded_len - ctx->encoded_off)
            + (ctx->buf_len - ctx->buf_off);
        if (ret == 0 && ctx->encode != B64_NONE
            && EVP_ENCODE_CTX_num(ctx->base64) != 0)
            ret = 1;
        else if (ret <= 0)
            ret = BIO_ctrl(next, cmd, num, ptr);
        break;
    case BIO_CTRL_PENDING: /* More to read in buffer */
        if (!ossl_assert(ctx->buf_len >= ctx->buf_off)) {
            ERR_raise(ERR_LIB_BIO, ERR_R_INTERNAL_ERROR);
            return -1;
        }
        ret = ctx->buf_len - ctx->buf_off;
        if (ret <= 0)
            ret = BIO_ctrl(next, cmd, num, ptr);
        break;
    case BIO_CTRL_FLUSH:
        if (ctx->encode == B64_ENCODE) {
            /* do a final write */
        again:
            while (b64_write_pending(ctx)) {
                i = b64_write(b, NULL, 0);
                if (i < 0 || (i == 0 && b64_write_pending(ctx)))
                    return i;
            }
            if (EVP_ENCODE_CTX_num(ctx->base64) != 0) {
                ctx->buf_off = 0;
                EVP_EncodeFinal(ctx->base64, ctx->buf, &(ctx->buf_len));
                /* push out the bytes */
                goto again;
            }
        }
        /* Finally flush the underlying BIO */
        BIO_clear_retry_flags(b);
        ret = BIO_ctrl(next, cmd, num, ptr);
        BIO_copy_next_retry(b);
        break;

    case BIO_C_DO_STATE_MACHINE:
        BIO_clear_retry_flags(b);
        ret = BIO_ctrl(next, cmd, num, ptr);
        BIO_copy_next_retry(b);
        break;

    case BIO_CTRL_DUP:
        break;
    case BIO_CTRL_INFO:
    case BIO_CTRL_GET:
    case BIO_CTRL_SET:
    default:
        ret = BIO_ctrl(next, cmd, num, ptr);
        break;
    }
    return ret;
}

static long b64_callback_ctrl(BIO *b, int cmd, BIO_info_cb *fp)
{
    BIO *next = BIO_next(b);

    if (next == NULL)
        return 0;

    return BIO_callback_ctrl(next, cmd, fp);
}

static int b64_puts(BIO *b, const char *str)
{
    size_t len = strlen(str);

    if (len > INT_MAX)
        return -1;
    return b64_write(b, str, (int)len);
}

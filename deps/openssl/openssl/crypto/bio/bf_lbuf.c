/* crypto/bio/bf_buff.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <errno.h>
#include "cryptlib.h"
#include <openssl/bio.h>
#include <openssl/evp.h>

static int linebuffer_write(BIO *h, const char *buf, int num);
static int linebuffer_read(BIO *h, char *buf, int size);
static int linebuffer_puts(BIO *h, const char *str);
static int linebuffer_gets(BIO *h, char *str, int size);
static long linebuffer_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int linebuffer_new(BIO *h);
static int linebuffer_free(BIO *data);
static long linebuffer_callback_ctrl(BIO *h, int cmd, bio_info_cb *fp);

/* A 10k maximum should be enough for most purposes */
#define DEFAULT_LINEBUFFER_SIZE 1024*10

/* #define DEBUG */

static BIO_METHOD methods_linebuffer = {
    BIO_TYPE_LINEBUFFER,
    "linebuffer",
    linebuffer_write,
    linebuffer_read,
    linebuffer_puts,
    linebuffer_gets,
    linebuffer_ctrl,
    linebuffer_new,
    linebuffer_free,
    linebuffer_callback_ctrl,
};

BIO_METHOD *BIO_f_linebuffer(void)
{
    return (&methods_linebuffer);
}

typedef struct bio_linebuffer_ctx_struct {
    char *obuf;                 /* the output char array */
    int obuf_size;              /* how big is the output buffer */
    int obuf_len;               /* how many bytes are in it */
} BIO_LINEBUFFER_CTX;

static int linebuffer_new(BIO *bi)
{
    BIO_LINEBUFFER_CTX *ctx;

    ctx = (BIO_LINEBUFFER_CTX *)OPENSSL_malloc(sizeof(BIO_LINEBUFFER_CTX));
    if (ctx == NULL)
        return (0);
    ctx->obuf = (char *)OPENSSL_malloc(DEFAULT_LINEBUFFER_SIZE);
    if (ctx->obuf == NULL) {
        OPENSSL_free(ctx);
        return (0);
    }
    ctx->obuf_size = DEFAULT_LINEBUFFER_SIZE;
    ctx->obuf_len = 0;

    bi->init = 1;
    bi->ptr = (char *)ctx;
    bi->flags = 0;
    return (1);
}

static int linebuffer_free(BIO *a)
{
    BIO_LINEBUFFER_CTX *b;

    if (a == NULL)
        return (0);
    b = (BIO_LINEBUFFER_CTX *)a->ptr;
    if (b->obuf != NULL)
        OPENSSL_free(b->obuf);
    OPENSSL_free(a->ptr);
    a->ptr = NULL;
    a->init = 0;
    a->flags = 0;
    return (1);
}

static int linebuffer_read(BIO *b, char *out, int outl)
{
    int ret = 0;

    if (out == NULL)
        return (0);
    if (b->next_bio == NULL)
        return (0);
    ret = BIO_read(b->next_bio, out, outl);
    BIO_clear_retry_flags(b);
    BIO_copy_next_retry(b);
    return (ret);
}

static int linebuffer_write(BIO *b, const char *in, int inl)
{
    int i, num = 0, foundnl;
    BIO_LINEBUFFER_CTX *ctx;

    if ((in == NULL) || (inl <= 0))
        return (0);
    ctx = (BIO_LINEBUFFER_CTX *)b->ptr;
    if ((ctx == NULL) || (b->next_bio == NULL))
        return (0);

    BIO_clear_retry_flags(b);

    do {
        const char *p;

        for (p = in; p < in + inl && *p != '\n'; p++) ;
        if (*p == '\n') {
            p++;
            foundnl = 1;
        } else
            foundnl = 0;

        /*
         * If a NL was found and we already have text in the save buffer,
         * concatenate them and write
         */
        while ((foundnl || p - in > ctx->obuf_size - ctx->obuf_len)
               && ctx->obuf_len > 0) {
            int orig_olen = ctx->obuf_len;

            i = ctx->obuf_size - ctx->obuf_len;
            if (p - in > 0) {
                if (i >= p - in) {
                    memcpy(&(ctx->obuf[ctx->obuf_len]), in, p - in);
                    ctx->obuf_len += p - in;
                    inl -= p - in;
                    num += p - in;
                    in = p;
                } else {
                    memcpy(&(ctx->obuf[ctx->obuf_len]), in, i);
                    ctx->obuf_len += i;
                    inl -= i;
                    in += i;
                    num += i;
                }
            }
#if 0
            BIO_write(b->next_bio, "<*<", 3);
#endif
            i = BIO_write(b->next_bio, ctx->obuf, ctx->obuf_len);
            if (i <= 0) {
                ctx->obuf_len = orig_olen;
                BIO_copy_next_retry(b);

#if 0
                BIO_write(b->next_bio, ">*>", 3);
#endif
                if (i < 0)
                    return ((num > 0) ? num : i);
                if (i == 0)
                    return (num);
            }
#if 0
            BIO_write(b->next_bio, ">*>", 3);
#endif
            if (i < ctx->obuf_len)
                memmove(ctx->obuf, ctx->obuf + i, ctx->obuf_len - i);
            ctx->obuf_len -= i;
        }

        /*
         * Now that the save buffer is emptied, let's write the input buffer
         * if a NL was found and there is anything to write.
         */
        if ((foundnl || p - in > ctx->obuf_size) && p - in > 0) {
#if 0
            BIO_write(b->next_bio, "<*<", 3);
#endif
            i = BIO_write(b->next_bio, in, p - in);
            if (i <= 0) {
                BIO_copy_next_retry(b);
#if 0
                BIO_write(b->next_bio, ">*>", 3);
#endif
                if (i < 0)
                    return ((num > 0) ? num : i);
                if (i == 0)
                    return (num);
            }
#if 0
            BIO_write(b->next_bio, ">*>", 3);
#endif
            num += i;
            in += i;
            inl -= i;
        }
    }
    while (foundnl && inl > 0);
    /*
     * We've written as much as we can.  The rest of the input buffer, if
     * any, is text that doesn't and with a NL and therefore needs to be
     * saved for the next trip.
     */
    if (inl > 0) {
        memcpy(&(ctx->obuf[ctx->obuf_len]), in, inl);
        ctx->obuf_len += inl;
        num += inl;
    }
    return num;
}

static long linebuffer_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    BIO *dbio;
    BIO_LINEBUFFER_CTX *ctx;
    long ret = 1;
    char *p;
    int r;
    int obs;

    ctx = (BIO_LINEBUFFER_CTX *)b->ptr;

    switch (cmd) {
    case BIO_CTRL_RESET:
        ctx->obuf_len = 0;
        if (b->next_bio == NULL)
            return (0);
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        break;
    case BIO_CTRL_INFO:
        ret = (long)ctx->obuf_len;
        break;
    case BIO_CTRL_WPENDING:
        ret = (long)ctx->obuf_len;
        if (ret == 0) {
            if (b->next_bio == NULL)
                return (0);
            ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        }
        break;
    case BIO_C_SET_BUFF_SIZE:
        obs = (int)num;
        p = ctx->obuf;
        if ((obs > DEFAULT_LINEBUFFER_SIZE) && (obs != ctx->obuf_size)) {
            p = (char *)OPENSSL_malloc((int)num);
            if (p == NULL)
                goto malloc_error;
        }
        if (ctx->obuf != p) {
            if (ctx->obuf_len > obs) {
                ctx->obuf_len = obs;
            }
            memcpy(p, ctx->obuf, ctx->obuf_len);
            OPENSSL_free(ctx->obuf);
            ctx->obuf = p;
            ctx->obuf_size = obs;
        }
        break;
    case BIO_C_DO_STATE_MACHINE:
        if (b->next_bio == NULL)
            return (0);
        BIO_clear_retry_flags(b);
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        BIO_copy_next_retry(b);
        break;

    case BIO_CTRL_FLUSH:
        if (b->next_bio == NULL)
            return (0);
        if (ctx->obuf_len <= 0) {
            ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
            break;
        }

        for (;;) {
            BIO_clear_retry_flags(b);
            if (ctx->obuf_len > 0) {
                r = BIO_write(b->next_bio, ctx->obuf, ctx->obuf_len);
#if 0
                fprintf(stderr, "FLUSH %3d -> %3d\n", ctx->obuf_len, r);
#endif
                BIO_copy_next_retry(b);
                if (r <= 0)
                    return ((long)r);
                if (r < ctx->obuf_len)
                    memmove(ctx->obuf, ctx->obuf + r, ctx->obuf_len - r);
                ctx->obuf_len -= r;
            } else {
                ctx->obuf_len = 0;
                ret = 1;
                break;
            }
        }
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        break;
    case BIO_CTRL_DUP:
        dbio = (BIO *)ptr;
        if (!BIO_set_write_buffer_size(dbio, ctx->obuf_size))
            ret = 0;
        break;
    default:
        if (b->next_bio == NULL)
            return (0);
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        break;
    }
    return (ret);
 malloc_error:
    BIOerr(BIO_F_LINEBUFFER_CTRL, ERR_R_MALLOC_FAILURE);
    return (0);
}

static long linebuffer_callback_ctrl(BIO *b, int cmd, bio_info_cb *fp)
{
    long ret = 1;

    if (b->next_bio == NULL)
        return (0);
    switch (cmd) {
    default:
        ret = BIO_callback_ctrl(b->next_bio, cmd, fp);
        break;
    }
    return (ret);
}

static int linebuffer_gets(BIO *b, char *buf, int size)
{
    if (b->next_bio == NULL)
        return (0);
    return (BIO_gets(b->next_bio, buf, size));
}

static int linebuffer_puts(BIO *b, const char *str)
{
    return (linebuffer_write(b, str, strlen(str)));
}

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
#include "bio_local.h"
#include "internal/cryptlib.h"

static int buffer_write(BIO *h, const char *buf, int num);
static int buffer_read(BIO *h, char *buf, int size);
static int buffer_puts(BIO *h, const char *str);
static int buffer_gets(BIO *h, char *str, int size);
static long buffer_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int buffer_new(BIO *h);
static int buffer_free(BIO *data);
static long buffer_callback_ctrl(BIO *h, int cmd, BIO_info_cb *fp);
static int buffer_sendmmsg(BIO *b, BIO_MSG *msg, size_t stride,
    size_t num_msg, uint64_t flags,
    size_t *msgs_processed);
static int buffer_send_next(BIO *b, BIO_F_BUFFER_CTX *ctx,
    const char *data, int len);

#define DEFAULT_BUFFER_SIZE 4096

static const BIO_METHOD methods_buffer = {
    BIO_TYPE_BUFFER,
    "buffer",
    bwrite_conv,
    buffer_write,
    bread_conv,
    buffer_read,
    buffer_puts,
    buffer_gets,
    buffer_ctrl,
    buffer_new,
    buffer_free,
    buffer_callback_ctrl,
    buffer_sendmmsg,
};

const BIO_METHOD *BIO_f_buffer(void)
{
    return &methods_buffer;
}

static int buffer_new(BIO *bi)
{
    BIO_F_BUFFER_CTX *ctx = OPENSSL_zalloc(sizeof(*ctx));

    if (ctx == NULL)
        return 0;
    ctx->ibuf_size = DEFAULT_BUFFER_SIZE;
    ctx->ibuf = OPENSSL_malloc(DEFAULT_BUFFER_SIZE);
    if (ctx->ibuf == NULL) {
        OPENSSL_free(ctx);
        return 0;
    }
    ctx->obuf_size = DEFAULT_BUFFER_SIZE;
    ctx->obuf = OPENSSL_malloc(DEFAULT_BUFFER_SIZE);
    if (ctx->obuf == NULL) {
        OPENSSL_free(ctx->ibuf);
        OPENSSL_free(ctx);
        return 0;
    }

    bi->init = 1;
    bi->ptr = (char *)ctx;
    bi->flags = 0;
    return 1;
}

static int buffer_free(BIO *a)
{
    BIO_F_BUFFER_CTX *b;

    if (a == NULL)
        return 0;
    b = (BIO_F_BUFFER_CTX *)a->ptr;
    OPENSSL_free(b->ibuf);
    OPENSSL_free(b->obuf);
#ifndef OPENSSL_NO_SOCK
    BIO_ADDR_free(b->peer);
#endif
    OPENSSL_free(a->ptr);
    a->ptr = NULL;
    a->init = 0;
    a->flags = 0;
    return 1;
}

static int buffer_read(BIO *b, char *out, int outl)
{
    int i, num = 0;
    BIO_F_BUFFER_CTX *ctx;

    if (out == NULL)
        return 0;
    ctx = (BIO_F_BUFFER_CTX *)b->ptr;

    if ((ctx == NULL) || (b->next_bio == NULL))
        return 0;
    num = 0;
    BIO_clear_retry_flags(b);

start:
    i = ctx->ibuf_len;
    /* If there is stuff left over, grab it */
    if (i != 0) {
        if (i > outl)
            i = outl;
        memcpy(out, &(ctx->ibuf[ctx->ibuf_off]), i);
        ctx->ibuf_off += i;
        ctx->ibuf_len -= i;
        num += i;
        if (outl == i)
            return num;
        outl -= i;
        out += i;
    }

    /*
     * We may have done a partial read. try to do more. We have nothing in
     * the buffer. If we get an error and have read some data, just return it
     * and let them retry to get the error again. copy direct to parent
     * address space
     */
    if (outl > ctx->ibuf_size) {
        for (;;) {
            i = BIO_read(b->next_bio, out, outl);
            if (i <= 0) {
                BIO_copy_next_retry(b);
                if (i < 0)
                    return ((num > 0) ? num : i);
                if (i == 0)
                    return num;
            }
            num += i;
            if (outl == i)
                return num;
            out += i;
            outl -= i;
        }
    }
    /* else */

    /* we are going to be doing some buffering */
    i = BIO_read(b->next_bio, ctx->ibuf, ctx->ibuf_size);
    if (i <= 0) {
        BIO_copy_next_retry(b);
        if (i < 0)
            return ((num > 0) ? num : i);
        if (i == 0)
            return num;
    }
    ctx->ibuf_off = 0;
    ctx->ibuf_len = i;

    /* Lets re-read using ourselves :-) */
    goto start;
}

static int buffer_write(BIO *b, const char *in, int inl)
{
    int i, num = 0;
    BIO_F_BUFFER_CTX *ctx;

    if ((in == NULL) || (inl <= 0))
        return 0;
    ctx = (BIO_F_BUFFER_CTX *)b->ptr;
    if ((ctx == NULL) || (b->next_bio == NULL))
        return 0;

    BIO_clear_retry_flags(b);
start:
    i = ctx->obuf_size - (ctx->obuf_len + ctx->obuf_off);
    /* add to buffer and return */
    if (i >= inl) {
        memcpy(&(ctx->obuf[ctx->obuf_off + ctx->obuf_len]), in, inl);
        ctx->obuf_len += inl;
        return (num + inl);
    }
    /* else */
    /* stuff already in buffer, so add to it first, then flush */
    if (ctx->obuf_len != 0) {
        if (i > 0) { /* lets fill it up if we can */
            memcpy(&(ctx->obuf[ctx->obuf_off + ctx->obuf_len]), in, i);
            in += i;
            inl -= i;
            num += i;
            ctx->obuf_len += i;
        }
        /* we now have a full buffer needing flushing */
        for (;;) {
            i = buffer_send_next(b, ctx, &(ctx->obuf[ctx->obuf_off]),
                ctx->obuf_len);
            if (i <= 0) {
                BIO_copy_next_retry(b);

                if (i < 0)
                    return ((num > 0) ? num : i);
                if (i == 0)
                    return num;
            }
            ctx->obuf_off += i;
            ctx->obuf_len -= i;
            if (ctx->obuf_len == 0)
                break;
        }
    }
    /*
     * we only get here if the buffer has been flushed and we still have
     * stuff to write
     */
    ctx->obuf_off = 0;

    /* we now have inl bytes to write */
    while (inl >= ctx->obuf_size) {
        i = buffer_send_next(b, ctx, in, inl);
        if (i <= 0) {
            BIO_copy_next_retry(b);
            if (i < 0)
                return ((num > 0) ? num : i);
            if (i == 0)
                return num;
        }
        num += i;
        in += i;
        inl -= i;
        if (inl == 0)
            return num;
    }

    /*
     * copy the rest into the buffer since we have only a small amount left
     */
    goto start;
}

static long buffer_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    BIO *dbio;
    BIO_F_BUFFER_CTX *ctx;
    long ret = 1;
    char *p1, *p2;
    int r, i, *ip;
    int ibs, obs;

    ctx = (BIO_F_BUFFER_CTX *)b->ptr;

    switch (cmd) {
    case BIO_CTRL_RESET:
        ctx->ibuf_off = 0;
        ctx->ibuf_len = 0;
        ctx->obuf_off = 0;
        ctx->obuf_len = 0;
#ifndef OPENSSL_NO_SOCK
        BIO_ADDR_free(ctx->peer);
        ctx->peer = NULL;
#endif
        if (b->next_bio == NULL)
            return 0;
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        break;
    case BIO_CTRL_EOF:
        if (ctx->ibuf_len > 0)
            return 0;
        /*
         * If there is no next BIO, BIO_read() returns 0, which means EOF,
         * BIO_eof() should return 1 in this case.
         */
        if (b->next_bio == NULL)
            return 1;
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        break;
    case BIO_CTRL_INFO:
        ret = (long)ctx->obuf_len;
        break;
    case BIO_C_GET_BUFF_NUM_LINES:
        ret = 0;
        p1 = ctx->ibuf;
        for (i = 0; i < ctx->ibuf_len; i++) {
            if (p1[ctx->ibuf_off + i] == '\n')
                ret++;
        }
        break;
    case BIO_CTRL_WPENDING:
        ret = (long)ctx->obuf_len;
        if (ret == 0) {
            if (b->next_bio == NULL)
                return 0;
            ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        }
        break;
    case BIO_CTRL_PENDING:
        ret = (long)ctx->ibuf_len;
        if (ret == 0) {
            if (b->next_bio == NULL)
                return 0;
            ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        }
        break;
    case BIO_C_SET_BUFF_READ_DATA:
        if (num > ctx->ibuf_size) {
            if (num <= 0)
                return 0;
            p1 = OPENSSL_malloc((size_t)num);
            if (p1 == NULL)
                return 0;
            OPENSSL_free(ctx->ibuf);
            ctx->ibuf = p1;
        }
        ctx->ibuf_off = 0;
        ctx->ibuf_len = (int)num;
        memcpy(ctx->ibuf, ptr, (int)num);
        ret = 1;
        break;
    case BIO_C_SET_BUFF_SIZE:
        if (ptr != NULL) {
            ip = (int *)ptr;
            if (*ip == 0) {
                ibs = (int)num;
                obs = ctx->obuf_size;
            } else { /* if (*ip == 1) */

                ibs = ctx->ibuf_size;
                obs = (int)num;
            }
        } else {
            ibs = (int)num;
            obs = (int)num;
        }
        p1 = ctx->ibuf;
        p2 = ctx->obuf;
        if ((ibs > DEFAULT_BUFFER_SIZE) && (ibs != ctx->ibuf_size)) {
            if (num <= 0)
                return 0;
            p1 = OPENSSL_malloc((size_t)num);
            if (p1 == NULL)
                return 0;
        }
        if ((obs > DEFAULT_BUFFER_SIZE) && (obs != ctx->obuf_size)) {
            p2 = OPENSSL_malloc((size_t)num);
            if (p2 == NULL) {
                if (p1 != ctx->ibuf)
                    OPENSSL_free(p1);
                return 0;
            }
        }
        if (ctx->ibuf != p1) {
            OPENSSL_free(ctx->ibuf);
            ctx->ibuf = p1;
            ctx->ibuf_off = 0;
            ctx->ibuf_len = 0;
            ctx->ibuf_size = ibs;
        }
        if (ctx->obuf != p2) {
            OPENSSL_free(ctx->obuf);
            ctx->obuf = p2;
            ctx->obuf_off = 0;
            ctx->obuf_len = 0;
            ctx->obuf_size = obs;
#ifndef OPENSSL_NO_SOCK
            BIO_ADDR_free(ctx->peer);
            ctx->peer = NULL;
#endif
        }
        break;
    case BIO_C_DO_STATE_MACHINE:
        if (b->next_bio == NULL)
            return 0;
        BIO_clear_retry_flags(b);
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        BIO_copy_next_retry(b);
        break;

    case BIO_CTRL_FLUSH:
        if (b->next_bio == NULL)
            return 0;
        if (ctx->obuf_len <= 0) {
            ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
            BIO_copy_next_retry(b);
            break;
        }

        for (;;) {
            BIO_clear_retry_flags(b);
            if (ctx->obuf_len > 0) {
                r = buffer_send_next(b, ctx, &(ctx->obuf[ctx->obuf_off]),
                    ctx->obuf_len);
                BIO_copy_next_retry(b);
                if (r <= 0)
                    return (long)r;
                ctx->obuf_off += r;
                ctx->obuf_len -= r;
            } else {
                ctx->obuf_len = 0;
                ctx->obuf_off = 0;
                break;
            }
        }
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        BIO_copy_next_retry(b);
#ifndef OPENSSL_NO_SOCK
        BIO_ADDR_free(ctx->peer);
        ctx->peer = NULL;
#endif
        break;
    case BIO_CTRL_DUP:
        dbio = (BIO *)ptr;
        if (BIO_set_read_buffer_size(dbio, ctx->ibuf_size) <= 0 || BIO_set_write_buffer_size(dbio, ctx->obuf_size) <= 0)
            ret = 0;
        break;
    case BIO_CTRL_PEEK:
        /* Ensure there's stuff in the input buffer */
        {
            char fake_buf[1];
            (void)buffer_read(b, fake_buf, 0);
        }
        if (num > ctx->ibuf_len)
            num = ctx->ibuf_len;
        memcpy(ptr, &(ctx->ibuf[ctx->ibuf_off]), num);
        ret = num;
        break;
    default:
        if (b->next_bio == NULL)
            return 0;
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        break;
    }
    return ret;
}

static long buffer_callback_ctrl(BIO *b, int cmd, BIO_info_cb *fp)
{
    if (b->next_bio == NULL)
        return 0;
    return BIO_callback_ctrl(b->next_bio, cmd, fp);
}

static int buffer_gets(BIO *b, char *buf, int size)
{
    BIO_F_BUFFER_CTX *ctx;
    int num = 0, i, flag;
    char *p;

    ctx = (BIO_F_BUFFER_CTX *)b->ptr;
    size--; /* reserve space for a '\0' */
    BIO_clear_retry_flags(b);

    for (;;) {
        if (ctx->ibuf_len > 0) {
            p = &(ctx->ibuf[ctx->ibuf_off]);
            flag = 0;
            for (i = 0; (i < ctx->ibuf_len) && (i < size); i++) {
                *(buf++) = p[i];
                if (p[i] == '\n') {
                    flag = 1;
                    i++;
                    break;
                }
            }
            num += i;
            size -= i;
            ctx->ibuf_len -= i;
            ctx->ibuf_off += i;
            if (flag || size == 0) {
                *buf = '\0';
                return num;
            }
        } else { /* read another chunk */

            i = BIO_read(b->next_bio, ctx->ibuf, ctx->ibuf_size);
            if (i <= 0) {
                BIO_copy_next_retry(b);
                *buf = '\0';
                if (i < 0)
                    return ((num > 0) ? num : i);
                if (i == 0)
                    return num;
            }
            ctx->ibuf_len = i;
            ctx->ibuf_off = 0;
        }
    }
}

static int buffer_puts(BIO *b, const char *str)
{
    size_t len = strlen(str);

    if (len > INT_MAX)
        return -1;
    return buffer_write(b, str, (int)len);
}

/*
 * buffer_send_next - write one chunk of buffered output to the next BIO.
 *
 * For listener-created datagram connections a peer address has been recorded and
 * such connections share a single network BIO, so the data must be sent with
 * BIO_sendmmsg() carrying the explicit peer address rather than BIO_write().
 *
 * Returns the number of bytes sent - the full len for the datagram case, which
 * is all-or-nothing - or 0 / a negative value on a transient or fatal error.
 */
static int buffer_send_next(BIO *b, BIO_F_BUFFER_CTX *ctx,
    const char *data, int len)
{
#ifndef OPENSSL_NO_SOCK
    if (ctx->peer != NULL) {
        BIO_MSG msg;
        size_t processed = 0;

        memset(&msg, 0, sizeof(msg));
        msg.data = (void *)data;
        msg.data_len = (size_t)len;
        msg.peer = ctx->peer;

        /* Datagrams are all-or-nothing: either the whole chunk goes or none. */
        if (!BIO_sendmmsg(b->next_bio, &msg, sizeof(msg), 1, 0, &processed)
            || processed != 1)
            return -1;
        return len;
    }
#endif
    return BIO_write(b->next_bio, data, len);
}

/*
 * buffer_sendmmsg - accumulate a message into the buffer BIO.
 *
 * Listener-created DTLS connections share a single network BIO and so cannot
 * use BIO_write(). Instead the record layer sends each record via BIO_sendmmsg(),
 * which lands here. We record the peer address and then buffer the data exactly
 * like buffer_write() does, so that multiple handshake records accumulate and are
 * packed into a single datagram when the state machine flushes.
 */
static int buffer_sendmmsg(BIO *b, BIO_MSG *msg, size_t stride,
    size_t num_msg, uint64_t flags,
    size_t *msgs_processed)
{
#ifndef OPENSSL_NO_SOCK
    BIO_F_BUFFER_CTX *ctx;
    size_t i;

    *msgs_processed = 0;

    if (b == NULL || b->next_bio == NULL)
        return 0;

    ctx = (BIO_F_BUFFER_CTX *)b->ptr;
    if (ctx == NULL || msg == NULL || num_msg == 0)
        return 0;

    /*
     * Record the peer address on the first write so the flush path knows
     * where to send the accumulated data. The address is cleared after each
     * flush so it can be set again for the next batch.
     */
    if (ctx->peer == NULL) {
        if (msg->peer == NULL)
            return 0;
        ctx->peer = BIO_ADDR_new();
        if (ctx->peer == NULL)
            return 0;
        if (!BIO_ADDR_copy(ctx->peer, msg->peer)) {
            BIO_ADDR_free(ctx->peer);
            ctx->peer = NULL;
            return 0;
        }
    }

    for (i = 0; i < num_msg; i++) {
        BIO_MSG *m = (BIO_MSG *)((char *)msg + i * stride);
        int ret;

        /*
         * Buffer the message data. buffer_write() takes an int length, so
         * guard against an oversized datagram (this mirrors buffer_puts()).
         */
        if (m->data_len > INT_MAX)
            break;

        ret = buffer_write(b, m->data, (int)m->data_len);
        if (ret <= 0)
            break;

        ++(*msgs_processed);
    }

    return (*msgs_processed > 0) ? 1 : 0;
#else
    *msgs_processed = 0;
    return 0;
#endif
}

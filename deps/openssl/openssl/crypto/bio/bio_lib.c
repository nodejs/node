/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <errno.h>
#include <openssl/crypto.h>
#include "bio_lcl.h"
#include "internal/cryptlib.h"

BIO *BIO_new(const BIO_METHOD *method)
{
    BIO *bio = OPENSSL_zalloc(sizeof(*bio));

    if (bio == NULL) {
        BIOerr(BIO_F_BIO_NEW, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }

    bio->method = method;
    bio->shutdown = 1;
    bio->references = 1;

    if (!CRYPTO_new_ex_data(CRYPTO_EX_INDEX_BIO, bio, &bio->ex_data))
        goto err;

    bio->lock = CRYPTO_THREAD_lock_new();
    if (bio->lock == NULL) {
        BIOerr(BIO_F_BIO_NEW, ERR_R_MALLOC_FAILURE);
        CRYPTO_free_ex_data(CRYPTO_EX_INDEX_BIO, bio, &bio->ex_data);
        goto err;
    }

    if (method->create != NULL && !method->create(bio)) {
        BIOerr(BIO_F_BIO_NEW, ERR_R_INIT_FAIL);
        CRYPTO_free_ex_data(CRYPTO_EX_INDEX_BIO, bio, &bio->ex_data);
        CRYPTO_THREAD_lock_free(bio->lock);
        goto err;
    }
    if (method->create == NULL)
        bio->init = 1;

    return bio;

err:
    OPENSSL_free(bio);
    return NULL;
}

int BIO_free(BIO *a)
{
    int i;

    if (a == NULL)
        return 0;

    if (CRYPTO_atomic_add(&a->references, -1, &i, a->lock) <= 0)
        return 0;

    REF_PRINT_COUNT("BIO", a);
    if (i > 0)
        return 1;
    REF_ASSERT_ISNT(i < 0);
    if ((a->callback != NULL) &&
        ((i = (int)a->callback(a, BIO_CB_FREE, NULL, 0, 0L, 1L)) <= 0))
        return i;

    if ((a->method != NULL) && (a->method->destroy != NULL))
        a->method->destroy(a);

    CRYPTO_free_ex_data(CRYPTO_EX_INDEX_BIO, a, &a->ex_data);

    CRYPTO_THREAD_lock_free(a->lock);

    OPENSSL_free(a);

    return 1;
}

void BIO_set_data(BIO *a, void *ptr)
{
    a->ptr = ptr;
}

void *BIO_get_data(BIO *a)
{
    return a->ptr;
}

void BIO_set_init(BIO *a, int init)
{
    a->init = init;
}

int BIO_get_init(BIO *a)
{
    return a->init;
}

void BIO_set_shutdown(BIO *a, int shut)
{
    a->shutdown = shut;
}

int BIO_get_shutdown(BIO *a)
{
    return a->shutdown;
}

void BIO_vfree(BIO *a)
{
    BIO_free(a);
}

int BIO_up_ref(BIO *a)
{
    int i;

    if (CRYPTO_atomic_add(&a->references, 1, &i, a->lock) <= 0)
        return 0;

    REF_PRINT_COUNT("BIO", a);
    REF_ASSERT_ISNT(i < 2);
    return ((i > 1) ? 1 : 0);
}

void BIO_clear_flags(BIO *b, int flags)
{
    b->flags &= ~flags;
}

int BIO_test_flags(const BIO *b, int flags)
{
    return (b->flags & flags);
}

void BIO_set_flags(BIO *b, int flags)
{
    b->flags |= flags;
}

long (*BIO_get_callback(const BIO *b)) (struct bio_st *, int, const char *,
                                        int, long, long) {
    return b->callback;
}

void BIO_set_callback(BIO *b,
                      long (*cb) (struct bio_st *, int, const char *, int,
                                  long, long))
{
    b->callback = cb;
}

void BIO_set_callback_arg(BIO *b, char *arg)
{
    b->cb_arg = arg;
}

char *BIO_get_callback_arg(const BIO *b)
{
    return b->cb_arg;
}

const char *BIO_method_name(const BIO *b)
{
    return b->method->name;
}

int BIO_method_type(const BIO *b)
{
    return b->method->type;
}

int BIO_read(BIO *b, void *out, int outl)
{
    int i;
    long (*cb) (BIO *, int, const char *, int, long, long);

    if ((b == NULL) || (b->method == NULL) || (b->method->bread == NULL)) {
        BIOerr(BIO_F_BIO_READ, BIO_R_UNSUPPORTED_METHOD);
        return (-2);
    }

    cb = b->callback;
    if ((cb != NULL) &&
        ((i = (int)cb(b, BIO_CB_READ, out, outl, 0L, 1L)) <= 0))
        return (i);

    if (!b->init) {
        BIOerr(BIO_F_BIO_READ, BIO_R_UNINITIALIZED);
        return (-2);
    }

    i = b->method->bread(b, out, outl);

    if (i > 0)
        b->num_read += (uint64_t)i;

    if (cb != NULL)
        i = (int)cb(b, BIO_CB_READ | BIO_CB_RETURN, out, outl, 0L, (long)i);
    return (i);
}

int BIO_write(BIO *b, const void *in, int inl)
{
    int i;
    long (*cb) (BIO *, int, const char *, int, long, long);

    if (b == NULL)
        return (0);

    cb = b->callback;
    if ((b->method == NULL) || (b->method->bwrite == NULL)) {
        BIOerr(BIO_F_BIO_WRITE, BIO_R_UNSUPPORTED_METHOD);
        return (-2);
    }

    if ((cb != NULL) &&
        ((i = (int)cb(b, BIO_CB_WRITE, in, inl, 0L, 1L)) <= 0))
        return (i);

    if (!b->init) {
        BIOerr(BIO_F_BIO_WRITE, BIO_R_UNINITIALIZED);
        return (-2);
    }

    i = b->method->bwrite(b, in, inl);

    if (i > 0)
        b->num_write += (uint64_t)i;

    if (cb != NULL)
        i = (int)cb(b, BIO_CB_WRITE | BIO_CB_RETURN, in, inl, 0L, (long)i);
    return (i);
}

int BIO_puts(BIO *b, const char *in)
{
    int i;
    long (*cb) (BIO *, int, const char *, int, long, long);

    if ((b == NULL) || (b->method == NULL) || (b->method->bputs == NULL)) {
        BIOerr(BIO_F_BIO_PUTS, BIO_R_UNSUPPORTED_METHOD);
        return (-2);
    }

    cb = b->callback;

    if ((cb != NULL) && ((i = (int)cb(b, BIO_CB_PUTS, in, 0, 0L, 1L)) <= 0))
        return (i);

    if (!b->init) {
        BIOerr(BIO_F_BIO_PUTS, BIO_R_UNINITIALIZED);
        return (-2);
    }

    i = b->method->bputs(b, in);

    if (i > 0)
        b->num_write += (uint64_t)i;

    if (cb != NULL)
        i = (int)cb(b, BIO_CB_PUTS | BIO_CB_RETURN, in, 0, 0L, (long)i);
    return (i);
}

int BIO_gets(BIO *b, char *in, int inl)
{
    int i;
    long (*cb) (BIO *, int, const char *, int, long, long);

    if ((b == NULL) || (b->method == NULL) || (b->method->bgets == NULL)) {
        BIOerr(BIO_F_BIO_GETS, BIO_R_UNSUPPORTED_METHOD);
        return (-2);
    }

    cb = b->callback;

    if ((cb != NULL) && ((i = (int)cb(b, BIO_CB_GETS, in, inl, 0L, 1L)) <= 0))
        return (i);

    if (!b->init) {
        BIOerr(BIO_F_BIO_GETS, BIO_R_UNINITIALIZED);
        return (-2);
    }

    i = b->method->bgets(b, in, inl);

    if (cb != NULL)
        i = (int)cb(b, BIO_CB_GETS | BIO_CB_RETURN, in, inl, 0L, (long)i);
    return (i);
}

int BIO_indent(BIO *b, int indent, int max)
{
    if (indent < 0)
        indent = 0;
    if (indent > max)
        indent = max;
    while (indent--)
        if (BIO_puts(b, " ") != 1)
            return 0;
    return 1;
}

long BIO_int_ctrl(BIO *b, int cmd, long larg, int iarg)
{
    int i;

    i = iarg;
    return (BIO_ctrl(b, cmd, larg, (char *)&i));
}

void *BIO_ptr_ctrl(BIO *b, int cmd, long larg)
{
    void *p = NULL;

    if (BIO_ctrl(b, cmd, larg, (char *)&p) <= 0)
        return (NULL);
    else
        return (p);
}

long BIO_ctrl(BIO *b, int cmd, long larg, void *parg)
{
    long ret;
    long (*cb) (BIO *, int, const char *, int, long, long);

    if (b == NULL)
        return (0);

    if ((b->method == NULL) || (b->method->ctrl == NULL)) {
        BIOerr(BIO_F_BIO_CTRL, BIO_R_UNSUPPORTED_METHOD);
        return (-2);
    }

    cb = b->callback;

    if ((cb != NULL) &&
        ((ret = cb(b, BIO_CB_CTRL, parg, cmd, larg, 1L)) <= 0))
        return (ret);

    ret = b->method->ctrl(b, cmd, larg, parg);

    if (cb != NULL)
        ret = cb(b, BIO_CB_CTRL | BIO_CB_RETURN, parg, cmd, larg, ret);
    return (ret);
}

long BIO_callback_ctrl(BIO *b, int cmd, BIO_info_cb *fp)
{
    long ret;
    long (*cb) (BIO *, int, const char *, int, long, long);

    if (b == NULL)
        return (0);

    if ((b->method == NULL) || (b->method->callback_ctrl == NULL)) {
        BIOerr(BIO_F_BIO_CALLBACK_CTRL, BIO_R_UNSUPPORTED_METHOD);
        return (-2);
    }

    cb = b->callback;

    if ((cb != NULL) &&
        ((ret = cb(b, BIO_CB_CTRL, (void *)&fp, cmd, 0, 1L)) <= 0))
        return (ret);

    ret = b->method->callback_ctrl(b, cmd, fp);

    if (cb != NULL)
        ret = cb(b, BIO_CB_CTRL | BIO_CB_RETURN, (void *)&fp, cmd, 0, ret);
    return (ret);
}

/*
 * It is unfortunate to duplicate in functions what the BIO_(w)pending macros
 * do; but those macros have inappropriate return type, and for interfacing
 * from other programming languages, C macros aren't much of a help anyway.
 */
size_t BIO_ctrl_pending(BIO *bio)
{
    return BIO_ctrl(bio, BIO_CTRL_PENDING, 0, NULL);
}

size_t BIO_ctrl_wpending(BIO *bio)
{
    return BIO_ctrl(bio, BIO_CTRL_WPENDING, 0, NULL);
}

/* put the 'bio' on the end of b's list of operators */
BIO *BIO_push(BIO *b, BIO *bio)
{
    BIO *lb;

    if (b == NULL)
        return (bio);
    lb = b;
    while (lb->next_bio != NULL)
        lb = lb->next_bio;
    lb->next_bio = bio;
    if (bio != NULL)
        bio->prev_bio = lb;
    /* called to do internal processing */
    BIO_ctrl(b, BIO_CTRL_PUSH, 0, lb);
    return (b);
}

/* Remove the first and return the rest */
BIO *BIO_pop(BIO *b)
{
    BIO *ret;

    if (b == NULL)
        return (NULL);
    ret = b->next_bio;

    BIO_ctrl(b, BIO_CTRL_POP, 0, b);

    if (b->prev_bio != NULL)
        b->prev_bio->next_bio = b->next_bio;
    if (b->next_bio != NULL)
        b->next_bio->prev_bio = b->prev_bio;

    b->next_bio = NULL;
    b->prev_bio = NULL;
    return (ret);
}

BIO *BIO_get_retry_BIO(BIO *bio, int *reason)
{
    BIO *b, *last;

    b = last = bio;
    for (;;) {
        if (!BIO_should_retry(b))
            break;
        last = b;
        b = b->next_bio;
        if (b == NULL)
            break;
    }
    if (reason != NULL)
        *reason = last->retry_reason;
    return (last);
}

int BIO_get_retry_reason(BIO *bio)
{
    return (bio->retry_reason);
}

void BIO_set_retry_reason(BIO *bio, int reason)
{
    bio->retry_reason = reason;
}

BIO *BIO_find_type(BIO *bio, int type)
{
    int mt, mask;

    if (bio == NULL)
        return NULL;
    mask = type & 0xff;
    do {
        if (bio->method != NULL) {
            mt = bio->method->type;

            if (!mask) {
                if (mt & type)
                    return (bio);
            } else if (mt == type)
                return (bio);
        }
        bio = bio->next_bio;
    } while (bio != NULL);
    return (NULL);
}

BIO *BIO_next(BIO *b)
{
    if (b == NULL)
        return NULL;
    return b->next_bio;
}

void BIO_set_next(BIO *b, BIO *next)
{
    b->next_bio = next;
}

void BIO_free_all(BIO *bio)
{
    BIO *b;
    int ref;

    while (bio != NULL) {
        b = bio;
        ref = b->references;
        bio = bio->next_bio;
        BIO_free(b);
        /* Since ref count > 1, don't free anyone else. */
        if (ref > 1)
            break;
    }
}

BIO *BIO_dup_chain(BIO *in)
{
    BIO *ret = NULL, *eoc = NULL, *bio, *new_bio;

    for (bio = in; bio != NULL; bio = bio->next_bio) {
        if ((new_bio = BIO_new(bio->method)) == NULL)
            goto err;
        new_bio->callback = bio->callback;
        new_bio->cb_arg = bio->cb_arg;
        new_bio->init = bio->init;
        new_bio->shutdown = bio->shutdown;
        new_bio->flags = bio->flags;

        /* This will let SSL_s_sock() work with stdin/stdout */
        new_bio->num = bio->num;

        if (!BIO_dup_state(bio, (char *)new_bio)) {
            BIO_free(new_bio);
            goto err;
        }

        /* copy app data */
        if (!CRYPTO_dup_ex_data(CRYPTO_EX_INDEX_BIO, &new_bio->ex_data,
                                &bio->ex_data)) {
            BIO_free(new_bio);
            goto err;
        }

        if (ret == NULL) {
            eoc = new_bio;
            ret = eoc;
        } else {
            BIO_push(eoc, new_bio);
            eoc = new_bio;
        }
    }
    return (ret);
 err:
    BIO_free_all(ret);

    return (NULL);
}

void BIO_copy_next_retry(BIO *b)
{
    BIO_set_flags(b, BIO_get_retry_flags(b->next_bio));
    b->retry_reason = b->next_bio->retry_reason;
}

int BIO_set_ex_data(BIO *bio, int idx, void *data)
{
    return (CRYPTO_set_ex_data(&(bio->ex_data), idx, data));
}

void *BIO_get_ex_data(BIO *bio, int idx)
{
    return (CRYPTO_get_ex_data(&(bio->ex_data), idx));
}

uint64_t BIO_number_read(BIO *bio)
{
    if (bio)
        return bio->num_read;
    return 0;
}

uint64_t BIO_number_written(BIO *bio)
{
    if (bio)
        return bio->num_write;
    return 0;
}

void bio_free_ex_data(BIO *bio)
{
    CRYPTO_free_ex_data(CRYPTO_EX_INDEX_BIO, bio, &bio->ex_data);
}

void bio_cleanup(void)
{
#ifndef OPENSSL_NO_SOCK
    bio_sock_cleanup_int();
    CRYPTO_THREAD_lock_free(bio_lookup_lock);
    bio_lookup_lock = NULL;
#endif
    CRYPTO_THREAD_lock_free(bio_type_lock);
    bio_type_lock = NULL;
}

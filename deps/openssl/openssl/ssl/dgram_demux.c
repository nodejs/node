/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/e_os.h"
#include "internal/dgram_demux.h"
#include "internal/thread_arch.h"
#include "internal/common.h"
#include <openssl/err.h>
#include <string.h>

#if !defined(OPENSSL_NO_QUIC) || !defined(OPENSSL_NO_DTLS)

/* URXE_DEMUX_STATE_* are defined in dgram_demux.h */

#define DEMUX_MAX_MSGS_PER_CALL 32
#define DEMUX_DEFAULT_MTU 1500
#define DEMUX_MIN_INITIAL_DGRAM_LEN 1200

struct dgram_demux_st {
    /* The underlying transport BIO with datagram semantics. */
    BIO *net_bio;

    /*
     * Our current understanding of the upper bound on an incoming datagram size
     * in bytes.
     */
    size_t mtu;

    /* The datagram_id to use for the next datagram we receive. */
    uint64_t next_datagram_id;

    /* Time retrieval callback. */
    OSSL_TIME (*now)(void *arg);
    void *now_arg;

    /* The default packet handler, if any. */
    ossl_dgram_demux_cb_fn *default_cb;
    void *default_cb_arg;

    /*
     * List of URXEs which are not currently in use (i.e., not filled with
     * unconsumed data). These are moved to the pending list as they are filled.
     */
    DGRAM_URXE_LIST urx_free;

    /*
     * List of URXEs which are filled with received encrypted data. These are
     * removed from this list as we invoke the callbacks for each of them. They
     * are then not on any list managed by us; we forget about them until our
     * user calls ossl_dgram_demux_release_urxe to return the URXE to us, at
     * which point we add it to the free list.
     */
    DGRAM_URXE_LIST urx_pending;

    /* Whether to use local address support. */
    char use_local_addr;

    /* Whether internal locking is required */
    char require_mutex;

    /*
     * Mutex protecting the URXE lists (urx_free and urx_pending).
     */
    CRYPTO_MUTEX *mutex;
};

/* List management helpers */
void ossl_dgram_urxe_remove(DGRAM_URXE_LIST *l, DGRAM_URXE *e)
{
    ossl_list_urxe_remove(l, e);
}

void ossl_dgram_urxe_insert_head(DGRAM_URXE_LIST *l, DGRAM_URXE *e)
{
    ossl_list_urxe_insert_head(l, e);
}

void ossl_dgram_urxe_insert_tail(DGRAM_URXE_LIST *l, DGRAM_URXE *e)
{
    ossl_list_urxe_insert_tail(l, e);
}

DGRAM_DEMUX *ossl_dgram_demux_new(BIO *net_bio,
    int threadsafe,
    OSSL_TIME (*now)(void *arg),
    void *now_arg)
{
    DGRAM_DEMUX *demux;

    demux = OPENSSL_zalloc(sizeof(DGRAM_DEMUX));
    if (demux == NULL)
        return NULL;

    if (threadsafe) {
        demux->require_mutex = 1;
        demux->mutex = ossl_crypto_mutex_new();
#ifdef OPENSSL_THREADS
        if (demux->mutex == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_CRYPTO_LIB);
            OPENSSL_free(demux);
            return NULL;
        }
#endif
    }

    /* We update this if possible when we get a BIO. */
    demux->mtu = DEMUX_DEFAULT_MTU;
    demux->now = now;
    demux->now_arg = now_arg;

    ossl_dgram_demux_set_bio(demux, net_bio);

    return demux;
}

static void dgram_demux_free_urxl(DGRAM_URXE_LIST *l)
{
    DGRAM_URXE *e, *enext;

    for (e = ossl_list_urxe_head(l); e != NULL; e = enext) {
        enext = ossl_list_urxe_next(e);
        ossl_list_urxe_remove(l, e);
        OPENSSL_free(e);
    }
}

void ossl_dgram_demux_free(DGRAM_DEMUX *demux)
{
    if (demux == NULL)
        return;

    /* Free all URXEs we are holding. */
    dgram_demux_free_urxl(&demux->urx_free);
    dgram_demux_free_urxl(&demux->urx_pending);

    if (demux->require_mutex)
        ossl_crypto_mutex_free(&demux->mutex);

    OPENSSL_free(demux);
}

void ossl_dgram_demux_set_bio(DGRAM_DEMUX *demux, BIO *net_bio)
{
    unsigned int mtu;

    demux->net_bio = net_bio;

    if (net_bio != NULL) {
        /*
         * Try to determine our MTU if possible. The BIO is not required to
         * support this, in which case we remain at the last known MTU, or our
         * initial default.
         */
        mtu = BIO_dgram_get_mtu(net_bio);
        if (mtu >= DEMUX_MIN_INITIAL_DGRAM_LEN)
            ossl_dgram_demux_set_mtu(demux, mtu);

        if (BIO_dgram_get_local_addr_cap(net_bio)
            && BIO_dgram_set_local_addr_enable(net_bio, 1))
            demux->use_local_addr = 1;
    }
}

int ossl_dgram_demux_set_mtu(DGRAM_DEMUX *demux, unsigned int mtu)
{
    if (mtu < DEMUX_MIN_INITIAL_DGRAM_LEN)
        return 0;

    /*
     * mtu is read under demux->mutex by the receive path
     * so take the lock here.
     */
    if (demux->require_mutex)
        ossl_crypto_mutex_lock(demux->mutex);
    demux->mtu = mtu;
    if (demux->require_mutex)
        ossl_crypto_mutex_unlock(demux->mutex);
    return 1;
}

void ossl_dgram_demux_set_default_handler(DGRAM_DEMUX *demux,
    ossl_dgram_demux_cb_fn *cb,
    void *cb_arg)
{
    demux->default_cb = cb;
    demux->default_cb_arg = cb_arg;
}

static DGRAM_URXE *dgram_demux_alloc_urxe(size_t alloc_len)
{
    DGRAM_URXE *e;

    if (alloc_len >= SIZE_MAX - sizeof(DGRAM_URXE))
        return NULL;

    e = OPENSSL_zalloc(sizeof(DGRAM_URXE) + alloc_len);
    if (e == NULL)
        return NULL;

    ossl_list_urxe_init_elem(e);
    e->alloc_len = alloc_len;
    e->data_len = 0;
    return e;
}

static DGRAM_URXE *dgram_demux_resize_urxe(DGRAM_DEMUX *demux, DGRAM_URXE *e,
    size_t new_alloc_len)
{
    DGRAM_URXE *e2, *prev;

    if (!ossl_assert(e->demux_state == URXE_DEMUX_STATE_FREE))
        /* Never attempt to resize a URXE which is not on the free list. */
        return NULL;

    prev = ossl_list_urxe_prev(e);
    ossl_list_urxe_remove(&demux->urx_free, e);

    if (new_alloc_len >= SIZE_MAX - sizeof(DGRAM_URXE))
        goto rollback;

    e2 = OPENSSL_realloc(e, sizeof(DGRAM_URXE) + new_alloc_len);

    /* Failed to resize, abort. */
    if (e2 == NULL)
        goto rollback;

    if (prev == NULL)
        ossl_list_urxe_insert_head(&demux->urx_free, e2);
    else
        ossl_list_urxe_insert_after(&demux->urx_free, prev, e2);

    e2->alloc_len = new_alloc_len;
    return e2;

rollback:
    /* Reinsert e back into the list on failures */
    if (prev == NULL)
        ossl_list_urxe_insert_head(&demux->urx_free, e);
    else
        ossl_list_urxe_insert_after(&demux->urx_free, prev, e);

    return NULL;
}

static DGRAM_URXE *dgram_demux_reserve_urxe(DGRAM_DEMUX *demux, DGRAM_URXE *e,
    size_t alloc_len)
{
    return e->alloc_len < alloc_len
        ? dgram_demux_resize_urxe(demux, e, alloc_len)
        : e;
}

static int dgram_demux_ensure_free_urxe(DGRAM_DEMUX *demux, size_t min_num_free)
{
    DGRAM_URXE *e;

    /* Caller must hold the lock */
    while (ossl_list_urxe_num(&demux->urx_free) < min_num_free) {
        e = dgram_demux_alloc_urxe(demux->mtu);
        if (e == NULL)
            return 0;

        ossl_list_urxe_insert_tail(&demux->urx_free, e);
        e->demux_state = URXE_DEMUX_STATE_FREE;
    }

    return 1;
}

/*
 * Receive datagrams from network, placing them into URXEs.
 *
 * Returns DGRAM_DEMUX_PUMP_RES_* value.
 *
 * Precondition: at least one URXE is free
 * Precondition: there are no pending URXEs
 * Precondition: Caller holds the demux lock
 */
static int dgram_demux_recv(DGRAM_DEMUX *demux)
{
    BIO_MSG msg[DEMUX_MAX_MSGS_PER_CALL];
    size_t rd, i;
    DGRAM_URXE *urxe = ossl_list_urxe_head(&demux->urx_free), *unext;
    OSSL_TIME now;

    /* This should never be called when we have any pending URXE. */
    assert(ossl_list_urxe_head(&demux->urx_pending) == NULL);
    assert(urxe->demux_state == URXE_DEMUX_STATE_FREE);

    if (demux->net_bio == NULL)
        /*
         * If no BIO is plugged in, treat this as no datagram being available.
         */
        return DGRAM_DEMUX_PUMP_RES_TRANSIENT_FAIL;

    /*
     * Opportunistically receive as many messages as possible in a single
     * syscall, determined by how many free URXEs are available.
     */
    for (i = 0; i < (ossl_ssize_t)OSSL_NELEM(msg);
        ++i, urxe = ossl_list_urxe_next(urxe)) {
        if (urxe == NULL) {
            /* We need at least one URXE to receive into. */
            if (!ossl_assert(i > 0))
                return DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL;

            break;
        }

        /* Ensure the URXE is big enough. */
        urxe = dgram_demux_reserve_urxe(demux, urxe, demux->mtu);
        if (urxe == NULL)
            /* Allocation error, fail. */
            return DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL;

        /* Ensure we zero any fields added to BIO_MSG at a later date. */
        memset(&msg[i], 0, sizeof(BIO_MSG));
        msg[i].data = ossl_dgram_urxe_data(urxe);
        msg[i].data_len = urxe->alloc_len;
        msg[i].peer = &urxe->peer;
        BIO_ADDR_clear(&urxe->peer);
        if (demux->use_local_addr)
            msg[i].local = &urxe->local;
        else
            BIO_ADDR_clear(&urxe->local);
    }

    ERR_set_mark();
    if (!BIO_recvmmsg(demux->net_bio, msg, sizeof(BIO_MSG), i, 0, &rd)) {
        if (BIO_err_is_non_fatal(ERR_peek_last_error())) {
            /* Transient error, clear the error and stop. */
            ERR_pop_to_mark();
            return DGRAM_DEMUX_PUMP_RES_TRANSIENT_FAIL;
        } else {
            /* Non-transient error, do not clear the error. */
            ERR_clear_last_mark();
            return DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL;
        }
    }

    ERR_clear_last_mark();
    now = demux->now != NULL ? demux->now(demux->now_arg) : ossl_time_zero();

    urxe = ossl_list_urxe_head(&demux->urx_free);
    for (i = 0; i < rd; ++i, urxe = unext) {
        unext = ossl_list_urxe_next(urxe);
        /* Set URXE with actual length of received datagram. */
        urxe->data_len = msg[i].data_len;
        /* Time we received datagram. */
        urxe->time = now;
        urxe->datagram_id = demux->next_datagram_id++;
        /* Move from free list to pending list. */
        ossl_list_urxe_remove(&demux->urx_free, urxe);
        ossl_list_urxe_insert_tail(&demux->urx_pending, urxe);
        urxe->demux_state = URXE_DEMUX_STATE_PENDING;
    }

    return DGRAM_DEMUX_PUMP_RES_OK;
}

/*
 * Process a single pending URXE.
 * Returning 1 on success, 0 on failure.
 *
 * Precondition: Caller holds the demux lock
 * Note: Lock is released before callback and reacquired after.
 */
static int dgram_demux_process_pending_urxe(DGRAM_DEMUX *demux, DGRAM_URXE *e)
{
    /* The next URXE we process should be at the head of the pending list. */
    if (!ossl_assert(e == ossl_list_urxe_head(&demux->urx_pending)))
        return 0;

    assert(e->demux_state == URXE_DEMUX_STATE_PENDING);

    ossl_list_urxe_remove(&demux->urx_pending, e);
    if (demux->default_cb != NULL) {
        /*
         * Pass to handler for routing. The URXE now belongs to the callback.
         * Release lock before callback to avoid deadlock if callback calls
         * release_urxe or reinject_urxe.
         */
        e->demux_state = URXE_DEMUX_STATE_ISSUED;
        if (demux->require_mutex)
            ossl_crypto_mutex_unlock(demux->mutex);
        demux->default_cb(e, demux->default_cb_arg);
        if (demux->require_mutex)
            ossl_crypto_mutex_lock(demux->mutex);
    } else {
        /* No handler, discard. */
        ossl_list_urxe_insert_tail(&demux->urx_free, e);
        e->demux_state = URXE_DEMUX_STATE_FREE;
    }

    return 1; /* keep processing pending URXEs */
}

/*
 * Process pending URXEs to generate callbacks.
 * Precondition: Caller holds the demux lock
 */
static int dgram_demux_process_pending_urxl(DGRAM_DEMUX *demux)
{
    DGRAM_URXE *e;
    int ret;

    while ((e = ossl_list_urxe_head(&demux->urx_pending)) != NULL)
        if ((ret = dgram_demux_process_pending_urxe(demux, e)) <= 0)
            return ret;

    return 1;
}

/*
 * Drain the pending URXE list, processing any pending URXEs by making their
 * callbacks. If no URXEs are pending, a network read is attempted first.
 */
int ossl_dgram_demux_pump(DGRAM_DEMUX *demux)
{
    int ret;

    if (demux->require_mutex)
        ossl_crypto_mutex_lock(demux->mutex);

    if (ossl_list_urxe_head(&demux->urx_pending) == NULL) {
        if (!dgram_demux_ensure_free_urxe(demux, DEMUX_MAX_MSGS_PER_CALL)) {
            ret = DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL;
            goto end;
        }

        ret = dgram_demux_recv(demux);
        if (ret != DGRAM_DEMUX_PUMP_RES_OK)
            goto end;

        /*
         * If dgram_demux_recv returned successfully, we should always have
         * something.
         */
        assert(ossl_list_urxe_head(&demux->urx_pending) != NULL);
    }

    if (dgram_demux_process_pending_urxl(demux) <= 0) {
        ret = DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL;
        goto end;
    }

    ret = DGRAM_DEMUX_PUMP_RES_OK;

end:
    if (demux->require_mutex)
        ossl_crypto_mutex_unlock(demux->mutex);
    return ret;
}

/* Artificially inject a packet into the demuxer for testing purposes. */
int ossl_dgram_demux_inject(DGRAM_DEMUX *demux,
    const unsigned char *buf,
    size_t buf_len,
    const BIO_ADDR *peer,
    const BIO_ADDR *local)
{
    int ret = 0;
    DGRAM_URXE *urxe;

    if (demux->require_mutex)
        ossl_crypto_mutex_lock(demux->mutex);

    if (!dgram_demux_ensure_free_urxe(demux, 1))
        goto end;

    urxe = ossl_list_urxe_head(&demux->urx_free);

    assert(urxe->demux_state == URXE_DEMUX_STATE_FREE);

    urxe = dgram_demux_reserve_urxe(demux, urxe, buf_len);
    if (urxe == NULL)
        goto end;

    memcpy(ossl_dgram_urxe_data(urxe), buf, buf_len);
    urxe->data_len = buf_len;

    if (peer != NULL)
        BIO_ADDR_copy(&urxe->peer, peer);
    else
        BIO_ADDR_clear(&urxe->peer);

    if (local != NULL)
        BIO_ADDR_copy(&urxe->local, local);
    else
        BIO_ADDR_clear(&urxe->local);

    urxe->time
        = demux->now != NULL ? demux->now(demux->now_arg) : ossl_time_zero();

    /* Move from free list to pending list. */
    ossl_list_urxe_remove(&demux->urx_free, urxe);
    urxe->datagram_id = demux->next_datagram_id++;
    ossl_list_urxe_insert_tail(&demux->urx_pending, urxe);
    urxe->demux_state = URXE_DEMUX_STATE_PENDING;

    ret = dgram_demux_process_pending_urxl(demux) > 0;

end:
    if (demux->require_mutex)
        ossl_crypto_mutex_unlock(demux->mutex);
    return ret;
}

/* Called by our user to return a URXE to the free list. */
void ossl_dgram_demux_release_urxe(DGRAM_DEMUX *demux, DGRAM_URXE *e)
{
    assert(ossl_list_urxe_prev(e) == NULL && ossl_list_urxe_next(e) == NULL);
    assert(e->demux_state == URXE_DEMUX_STATE_ISSUED);

    if (demux->require_mutex)
        ossl_crypto_mutex_lock(demux->mutex);
    ossl_list_urxe_insert_tail(&demux->urx_free, e);
    e->demux_state = URXE_DEMUX_STATE_FREE;
    if (demux->require_mutex)
        ossl_crypto_mutex_unlock(demux->mutex);
}

void ossl_dgram_demux_reinject_urxe(DGRAM_DEMUX *demux, DGRAM_URXE *e)
{
    assert(ossl_list_urxe_prev(e) == NULL && ossl_list_urxe_next(e) == NULL);
    assert(e->demux_state == URXE_DEMUX_STATE_ISSUED);

    if (demux->require_mutex)
        ossl_crypto_mutex_lock(demux->mutex);
    ossl_list_urxe_insert_head(&demux->urx_pending, e);
    e->demux_state = URXE_DEMUX_STATE_PENDING;
    if (demux->require_mutex)
        ossl_crypto_mutex_unlock(demux->mutex);
}

int ossl_dgram_demux_has_pending(const DGRAM_DEMUX *demux)
{
    return ossl_list_urxe_head(&demux->urx_pending) != NULL;
}

#endif /* !OPENSSL_NO_QUIC || !OPENSSL_NO_DTLS */

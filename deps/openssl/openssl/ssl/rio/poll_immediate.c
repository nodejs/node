/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/common.h"
#include "internal/quic_ssl.h"
#include "internal/quic_reactor_wait_ctx.h"
#include "internal/ssl_unwrap.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../ssl_local.h"
#include "poll_builder.h"

#ifdef OPENSSL_NO_QUIC
/* Stub type when QUIC is disabled so function signatures remain consistent */
typedef int QUIC_REACTOR_WAIT_CTX;
#endif

#if defined(_AIX)
/*
 * Some versions of AIX define macros for events and revents for use when
 * accessing pollfd structures (see Github issue #24236). That interferes
 * with our use of these names here. We simply undef them.
 */
#undef revents
#undef events
#endif

#define ITEM_N(items, stride, n) \
    (*(SSL_POLL_ITEM *)((char *)(items) + (n) * (stride)))

#define FAIL_FROM(n)                              \
    do {                                          \
        size_t j;                                 \
                                                  \
        for (j = (n); j < num_items; ++j)         \
            ITEM_N(items, stride, j).revents = 0; \
                                                  \
        ok = 0;                                   \
        goto out;                                 \
    } while (0)

#define FAIL_ITEM(idx)                                          \
    do {                                                        \
        size_t idx_ = (idx);                                    \
                                                                \
        ITEM_N(items, stride, idx_).revents = SSL_POLL_EVENT_F; \
        ++result_count;                                         \
        FAIL_FROM(idx_ + 1);                                    \
    } while (0)

#ifndef OPENSSL_NO_QUIC
/*
 * Test instrumentation only; see poll_builder.h. Always NULL in production
 * use.
 */
void (*ossl_quic_poll_translate_test_step_cb)(size_t idx, void *arg) = NULL;
void *ossl_quic_poll_translate_test_step_cb_arg = NULL;

static int poll_translate_ssl_quic(SSL *ssl,
    QUIC_REACTOR_WAIT_CTX *wctx,
    RIO_POLL_BUILDER *rpb,
    uint64_t events,
    int *abort_blocking)
{
    BIO_POLL_DESCRIPTOR rd, wd;
    int fd1 = -1, fd2 = -1, fd_nfy = -1;
    int fd1_r = 0, fd1_w = 0, fd2_w = 0;

    if (SSL_net_read_desired(ssl)) {
        if (!SSL_get_rpoll_descriptor(ssl, &rd)) {
            ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                "SSL_poll requires the network BIOs underlying "
                "a QUIC SSL object provide poll descriptors");
            return 0;
        }

        if (rd.type != BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD) {
            ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                "SSL_poll requires the poll descriptors of the "
                "network BIOs underlying a QUIC SSL object be "
                "of socket type");
            return 0;
        }

        fd1 = rd.value.fd;
        fd1_r = 1;
    }

    if (SSL_net_write_desired(ssl)) {
        if (!SSL_get_wpoll_descriptor(ssl, &wd)) {
            ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                "SSL_poll requires the network BIOs underlying "
                "a QUIC SSL object provide poll descriptors");
            return 0;
        }

        if (wd.type != BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD) {
            ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                "SSL_poll requires the poll descriptors of the "
                "network BIOs underlying a QUIC SSL object be "
                "of socket type");
            return 0;
        }

        fd2 = wd.value.fd;
        fd2_w = 1;
    }

    if (fd2 == fd1) {
        fd2 = -1;
        fd1_w = fd2_w;
    }

    if (fd1 != -1)
        if (!ossl_rio_poll_builder_add_fd(rpb, fd1, fd1_r, fd1_w))
            return 0;

    if (fd2 != -1 && fd2_w)
        if (!ossl_rio_poll_builder_add_fd(rpb, fd2, /*r = */ 0, fd2_w))
            return 0;

    /*
     * Add the notifier FD for the QUIC domain this SSL object is a part of (if
     * there is one). This ensures we get woken up if another thread calls into
     * that QUIC domain and some readiness event relevant to the SSL_poll call
     * on this thread arises without the underlying network socket ever becoming
     * readable.
     */
    fd_nfy = ossl_quic_get_notifier_fd(ssl);
    if (fd_nfy != -1) {
        uint64_t revents = 0;

        if (!ossl_rio_poll_builder_add_fd(rpb, fd_nfy, /*r = */ 1, /*w = */ 0))
            return 0;

        /* Tell QUIC domain we need to receive notifications. */
        ossl_quic_enter_blocking_section(ssl, wctx);

        /*
         * Only after the above call returns is it guaranteed that any readiness
         * events will cause the above notifier to become readable. Therefore,
         * it is possible the object became ready after our initial
         * poll_readout() call (before we determined that nothing was ready and
         * we needed to block). We now need to do another readout, in which case
         * blocking is to be aborted.
         */
        if (!ossl_quic_conn_poll_events(ssl, events, /*do_tick = */ 0, &revents)) {
            ossl_quic_leave_blocking_section(ssl, wctx);
            return 0;
        }

        if (revents != 0) {
            ossl_quic_leave_blocking_section(ssl, wctx);
            *abort_blocking = 1;
            return 1;
        }
    }

    return 1;
}

static void postpoll_translation_cleanup_ssl_quic(SSL *ssl,
    QUIC_REACTOR_WAIT_CTX *wctx)
{
    if (ossl_quic_get_notifier_fd(ssl) != -1)
        ossl_quic_leave_blocking_section(ssl, wctx);
}
#endif /* OPENSSL_NO_QUIC */

#ifndef OPENSSL_NO_DTLS
static int poll_translate_ssl_dtls_listener(SSL *ssl,
    RIO_POLL_BUILDER *rpb,
    uint64_t events,
    int *abort_blocking)
{
    BIO *rbio;
    BIO_POLL_DESCRIPTOR desc;
    DTLS_LISTENER *dl = (DTLS_LISTENER *)ssl;
    uint64_t revents = 0;
    int nfd;

    rbio = SSL_get_rbio(ssl);
    if (rbio == NULL)
        return 0;

    if (!BIO_get_rpoll_descriptor(rbio, &desc)
        || desc.type != BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD) {
        ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
            "DTLS listener requires a socket BIO for blocking poll");
        return 0;
    }

    /*
     * Watch the socket for readability whatever was asked for. Every event a
     * listener reports is ultimately driven by a datagram arriving, both an
     * incoming connection and data pending on the listener itself, so there is
     * no event for which this is the wrong thing to wait on. events is
     * therefore only consulted for the readiness re-check below.
     */
    if (!ossl_rio_poll_builder_add_fd(rpb, desc.value.fd, /*r=*/1, /*w=*/0))
        return 0;

    /*
     * Add the notifier FD for the DTLS listener (if multi-threaded mode is
     * enabled). Another thread may queue an incoming connection for us, or
     * demux data to one of our connections, without the underlying network
     * socket ever becoming readable from our perspective.
     */
    if (dl->have_notifier) {
        nfd = ossl_rio_notifier_as_fd(&dl->notifier);
        if (nfd != -1) {
            if (!ossl_rio_poll_builder_add_fd(rpb, nfd, /*r=*/1, /*w=*/0))
                return 0;

            /* Tell the listener we need to receive notifications. */
            ossl_dtls_listener_enter_blocking_section(ssl);

            /*
             * Only after the above call returns is it guaranteed that any
             * readiness events will cause the notifier to become readable.
             * Therefore it is possible the listener became ready after the
             * readout which decided we needed to block. Re-check now.
             */
            if (!ossl_dtls_listener_poll_events(ssl, events, /*do_tick=*/0,
                    &revents)) {
                ossl_dtls_listener_leave_blocking_section(ssl);
                return 0;
            }

            if (revents != 0) {
                ossl_dtls_listener_leave_blocking_section(ssl);
                *abort_blocking = 1;
                return 1;
            }
        }
    }

    return 1;
}

static int poll_translate_ssl_dtls_conn(SSL *ssl,
    RIO_POLL_BUILDER *rpb,
    uint64_t events,
    int *abort_blocking)
{
    BIO *rbio, *wbio;
    BIO_POLL_DESCRIPTOR rdesc, wdesc;
    int rfd = -1, wfd = -1, nfd = -1;
    SSL_CONNECTION *sc;
    DTLS_LISTENER *dl = NULL;
    int has_pending;

    sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (sc != NULL && sc->d1 != NULL && sc->d1->listener != NULL)
        dl = (DTLS_LISTENER *)sc->d1->listener;

    if ((events & SSL_POLL_EVENT_R) != 0) {
        rbio = SSL_get_rbio(ssl);

        if (rbio == NULL && dl != NULL) {
            /*
             * Listener-based DTLS connection. First, pump the listener's
             * demux to ensure any pending datagrams on the socket are
             * routed to their respective connection URXE queues.
             */
            ossl_dtls_tick(dl);

            /*
             * Now check the URXE buffer. If data has been demuxed into
             * this connection's receive queue, abort blocking immediately -
             * there is no need to wait on the socket FD.
             */
            if (sc->d1->rx != NULL) {
                ossl_crypto_mutex_lock(sc->d1->rx->mutex);
                has_pending = !ossl_list_urxe_is_empty(&sc->d1->rx->urxe_pending);
                ossl_crypto_mutex_unlock(sc->d1->rx->mutex);
                if (has_pending) {
                    *abort_blocking = 1;
                    return 1;
                }
            }

            /*
             * Add the notifier FD for the DTLS listener (if multi-threaded
             * mode is enabled). This ensures we get woken up if another thread
             * demuxes data to this connection's URXE queue without the
             * underlying network socket ever becoming readable from our
             * perspective.
             */
            if (dl->have_notifier) {
                nfd = ossl_rio_notifier_as_fd(&dl->notifier);
                if (nfd != -1) {
                    if (!ossl_rio_poll_builder_add_fd(rpb, nfd, /*r=*/1, /*w=*/0))
                        return 0;

                    /* Tell listener we need to receive notifications. */
                    ossl_dtls_listener_enter_blocking_section(sc->d1->listener);

                    /*
                     * Only after the above call returns is it guaranteed that
                     * any readiness events will cause the notifier to become
                     * readable. Therefore, it is possible data was demuxed to
                     * our URXE queue after our initial check above but before
                     * we entered the blocking section. Re-check now.
                     */
                    if (sc->d1->rx != NULL) {
                        ossl_crypto_mutex_lock(sc->d1->rx->mutex);
                        has_pending = !ossl_list_urxe_is_empty(&sc->d1->rx->urxe_pending);
                        ossl_crypto_mutex_unlock(sc->d1->rx->mutex);
                        if (has_pending) {
                            ossl_dtls_listener_leave_blocking_section(sc->d1->listener);
                            *abort_blocking = 1;
                            return 1;
                        }
                    }
                }
            }

            /*
             * URXE buffer is empty. Fall back to the listener's rbio so the
             * OS-level poll() wakes us up when the shared socket becomes
             * readable and new datagrams may arrive.
             */
            rbio = SSL_get_rbio(sc->d1->listener);
        }

        if (rbio != NULL) {
            if (BIO_get_rpoll_descriptor(rbio, &rdesc)
                && rdesc.type == BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD)
                rfd = rdesc.value.fd;
        }
    }

    if ((events & SSL_POLL_EVENT_W) != 0) {
        wbio = SSL_get_wbio(ssl);
        if (wbio != NULL) {
            if (BIO_get_wpoll_descriptor(wbio, &wdesc)
                && wdesc.type == BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD)
                wfd = wdesc.value.fd;
        }
    }

    /* If same FD for read and write, combine them */
    if (rfd != -1 && wfd == rfd) {
        if (!ossl_rio_poll_builder_add_fd(rpb, rfd, /*r=*/1, /*w=*/1))
            return 0;
    } else {
        if (rfd != -1)
            if (!ossl_rio_poll_builder_add_fd(rpb, rfd, /*r=*/1, /*w=*/0))
                return 0;
        if (wfd != -1)
            if (!ossl_rio_poll_builder_add_fd(rpb, wfd, /*r=*/0, /*w=*/1))
                return 0;
    }

    return 1;
}

static void postpoll_translation_cleanup_ssl_dtls_listener(SSL *ssl)
{
    DTLS_LISTENER *dl = (DTLS_LISTENER *)ssl;

    /* Need to mirror the enter blocking section call */
    if (dl->have_notifier && ossl_rio_notifier_as_fd(&dl->notifier) != -1)
        ossl_dtls_listener_leave_blocking_section(ssl);
}

static void postpoll_translation_cleanup_ssl_dtls_conn(SSL *ssl, uint64_t events)
{
    SSL_CONNECTION *sc;
    DTLS_LISTENER *dl;

    /*
     * We only enter blocking section when read events are requested.
     * Don't call leave if we never entered.
     */
    if ((events & SSL_POLL_EVENT_R) == 0)
        return;

    sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);
    if (sc == NULL || sc->d1 == NULL || sc->d1->listener == NULL)
        return;

    /* Need to mirror the enter blocking section call */
    if (SSL_get_rbio(ssl) != NULL)
        return;

    dl = (DTLS_LISTENER *)sc->d1->listener;
    if (dl->have_notifier && ossl_rio_notifier_as_fd(&dl->notifier) != -1)
        ossl_dtls_listener_leave_blocking_section(sc->d1->listener);
}
#endif /* OPENSSL_NO_DTLS */

#if !defined(OPENSSL_NO_QUIC) || !defined(OPENSSL_NO_DTLS)
static void postpoll_translation_cleanup(SSL_POLL_ITEM *items,
    size_t num_items,
    size_t stride,
    ossl_unused QUIC_REACTOR_WAIT_CTX *wctx)
{
    SSL_POLL_ITEM *item;
    SSL *ssl;
    size_t i;

    for (i = 0; i < num_items; ++i) {
        item = &ITEM_N(items, stride, i);

        switch (item->desc.type) {
        case BIO_POLL_DESCRIPTOR_TYPE_SSL:
            ssl = item->desc.value.ssl;
            if (ssl == NULL)
                break;

            switch (ssl->type) {
#ifndef OPENSSL_NO_QUIC
            case SSL_TYPE_QUIC_LISTENER:
            case SSL_TYPE_QUIC_CONNECTION:
            case SSL_TYPE_QUIC_XSO:
                postpoll_translation_cleanup_ssl_quic(ssl, wctx);
                break;
#endif

#ifndef OPENSSL_NO_DTLS
            case SSL_TYPE_DTLS_LISTENER:
                postpoll_translation_cleanup_ssl_dtls_listener(ssl);
                break;
            case SSL_TYPE_SSL_CONNECTION:
                if (SSL_is_dtls(ssl))
                    postpoll_translation_cleanup_ssl_dtls_conn(ssl, item->events);
                break;
#endif

            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}

static int poll_translate(SSL_POLL_ITEM *items,
    size_t num_items,
    size_t stride,
    ossl_unused QUIC_REACTOR_WAIT_CTX *wctx,
    RIO_POLL_BUILDER *rpb,
    OSSL_TIME *p_earliest_wakeup_deadline,
    int *abort_blocking,
    int bound_by_event_timeout,
    size_t *p_result_count)
{
    int ok = 1;
    SSL_POLL_ITEM *item;
    size_t result_count = 0;
    SSL *ssl;
    OSSL_TIME earliest_wakeup_deadline = ossl_time_infinite();
#if !defined(OPENSSL_NO_QUIC) || !defined(OPENSSL_NO_DTLS)
    struct timeval timeout;
    int is_infinite = 0;
#endif
    size_t i;

    for (i = 0; i < num_items; ++i) {
        item = &ITEM_N(items, stride, i);

#ifndef OPENSSL_NO_QUIC
        if (ossl_quic_poll_translate_test_step_cb != NULL)
            ossl_quic_poll_translate_test_step_cb(i,
                ossl_quic_poll_translate_test_step_cb_arg);
#endif

        switch (item->desc.type) {
        case BIO_POLL_DESCRIPTOR_TYPE_SSL:
            ssl = item->desc.value.ssl;
            if (ssl == NULL)
                /* NULL items are no-ops and have revents reported as 0 */
                break;

            switch (ssl->type) {
#ifndef OPENSSL_NO_QUIC
            case SSL_TYPE_QUIC_LISTENER:
            case SSL_TYPE_QUIC_CONNECTION:
            case SSL_TYPE_QUIC_XSO:
                if (!poll_translate_ssl_quic(ssl, wctx, rpb, item->events,
                        abort_blocking))
                    FAIL_ITEM(i);

                if (*abort_blocking)
                    goto out;

                if (!SSL_get_event_timeout(ssl, &timeout, &is_infinite))
                    FAIL_ITEM(i++); /* need to clean up this item too */

                if (!is_infinite)
                    earliest_wakeup_deadline
                        = ossl_time_min(earliest_wakeup_deadline,
                            ossl_time_add(ossl_time_now(),
                                ossl_time_from_timeval(timeout)));

                break;
#endif

#ifndef OPENSSL_NO_DTLS
            case SSL_TYPE_DTLS_LISTENER:
                if (!poll_translate_ssl_dtls_listener(ssl, rpb, item->events,
                        abort_blocking))
                    FAIL_ITEM(i);

                if (*abort_blocking)
                    goto out;

                break;
            case SSL_TYPE_SSL_CONNECTION:
                if (SSL_is_dtls(ssl)) {
                    if (!poll_translate_ssl_dtls_conn(ssl, rpb, item->events,
                            abort_blocking))
                        FAIL_ITEM(i);

                    if (*abort_blocking)
                        goto out;

                    /*
                     * Bound the wait by the DTLS retransmission timer,
                     * otherwise a poll with no timeout sleeps straight through
                     * the point at which we should be retransmitting.
                     *
                     * Unless the caller has told us not to. A waiter which
                     * cannot service the timer must not be woken by it: it
                     * would find the timeout still expired on the next wait,
                     * which reads as a zero deadline, and spin.
                     */
                    if (bound_by_event_timeout) {
                        if (!SSL_get_event_timeout(ssl, &timeout, &is_infinite))
                            FAIL_ITEM(i++); /* need to clean up this item too */

                        if (!is_infinite)
                            earliest_wakeup_deadline
                                = ossl_time_min(earliest_wakeup_deadline,
                                    ossl_time_add(ossl_time_now(),
                                        ossl_time_from_timeval(timeout)));
                    }

                } else {
                    ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                        "SSL_poll currently only supports DTLS listeners for DTLS connections");
                }
                break;
#endif

            default:
                ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                    "SSL_poll currently only supports QUIC SSL "
                    "objects");
                FAIL_ITEM(i);
            }
            break;

        case BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD:
            ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                "SSL_poll currently does not support polling "
                "sockets");
            FAIL_ITEM(i);

        default:
            ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                "SSL_poll does not support unknown poll descriptor "
                "type %d",
                item->desc.type);
            FAIL_ITEM(i);
        }
    }

out:
    /*
     * On abort_blocking, the item which triggered the abort has already
     * balanced its own enter/leave of the blocking section (see
     * poll_translate_ssl_quic()); only items 0..i-1 still need cleanup here.
     */
    if (!ok || *abort_blocking)
        postpoll_translation_cleanup(items, i, stride, wctx);

    *p_earliest_wakeup_deadline = earliest_wakeup_deadline;
    *p_result_count = result_count;
    return ok;
}

static int poll_block(SSL_POLL_ITEM *items,
    size_t num_items,
    size_t stride,
    OSSL_TIME user_deadline,
    int bound_by_event_timeout,
    size_t *p_result_count)
{
    int ok = 0, abort_blocking = 0;
    RIO_POLL_BUILDER rpb;
    QUIC_REACTOR_WAIT_CTX wctx;
    OSSL_TIME earliest_wakeup_deadline;

    /*
     * Blocking is somewhat involved and involves the following steps:
     *
     * - Translation, in which the various logical items (SSL objects, etc.) to
     *   be polled are translated into items an OS polling API understands.
     *
     * - Synchronisation bookkeeping. This ensures that we can be woken up
     *   not just by readiness of any underlying file descriptor distilled from
     *   the provided items but also by other threads, which might do work
     *   on a relevant QUIC object to cause the object to be ready without the
     *   underlying file descriptor ever becoming ready from our perspective.
     *
     * - The blocking call to the OS polling API.
     *
     * - Currently we do not do reverse translation but simply call
     *   poll_readout() again to read out all readiness state for all
     *   descriptors which the user passed.
     *
     *   TODO(QUIC POLLING): In the future we will do reverse translation here
     *   also to facilitate a more efficient readout.
     */
#ifndef OPENSSL_NO_QUIC
    ossl_quic_reactor_wait_ctx_init(&wctx);
#endif
    ossl_rio_poll_builder_init(&rpb);

    if (!poll_translate(items, num_items, stride, &wctx, &rpb,
            &earliest_wakeup_deadline,
            &abort_blocking,
            bound_by_event_timeout,
            p_result_count))
        goto out;

    if (abort_blocking) {
        /*
         * Nothing actually failed; we just shouldn't block because an item
         * may have become ready while we were setting up. The caller's
         * retry loop will call poll_readout() again to pick this up.
         */
        ok = 1;
        goto out;
    }

    earliest_wakeup_deadline = ossl_time_min(earliest_wakeup_deadline,
        user_deadline);

    ok = ossl_rio_poll_builder_poll(&rpb, earliest_wakeup_deadline);

    postpoll_translation_cleanup(items, num_items, stride, &wctx);

out:
    ossl_rio_poll_builder_cleanup(&rpb);
#ifndef OPENSSL_NO_QUIC
    ossl_quic_reactor_wait_ctx_cleanup(&wctx);
#endif
    return ok;
}

#ifndef OPENSSL_NO_DTLS
/*
 * Wait until the given DTLS listener or listener-based connection may have
 * become ready for one of the given events, or until the deadline expires.
 *
 * This is the wait that libssl itself performs when a DTLS object is used in
 * blocking mode. It is the same wait SSL_poll() performs, and reuses it, so a
 * blocking call is woken by the same means an application polling the object
 * would be: readiness of the listener's socket, or the listener's notifier if
 * another thread produces readiness without the socket becoming readable here.
 *
 * No readout is performed. A spurious wakeup is always possible - the socket
 * becoming readable says nothing about which connection the datagram is for -
 * so the caller must re-test its own condition and wait again if needed.
 *
 * bound_by_event_timeout says whether the connection's own event timeout, which
 * for DTLS is the retransmission timer, should shorten the wait. Pass 1 unless
 * the caller is unable to service that timer when it fires: waking for a
 * timeout nothing then handles leaves it expired, and an expired timeout reads
 * as a zero deadline, so every later wait returns at once.
 *
 * Returns 1 if the wait completed and 0 on error.
 */
int ossl_dtls_block_until_ready(SSL *ssl, uint64_t events, OSSL_TIME deadline,
    int bound_by_event_timeout)
{
    SSL_POLL_ITEM item;
    size_t result_count = 0;

    item.desc.type = BIO_POLL_DESCRIPTOR_TYPE_SSL;
    item.desc.value.ssl = ssl;
    item.events = events;
    item.revents = 0;

    return poll_block(&item, 1, sizeof(item), deadline, bound_by_event_timeout,
        &result_count);
}
#endif /* OPENSSL_NO_DTLS */
#endif

static int poll_readout(SSL_POLL_ITEM *items,
    size_t num_items,
    size_t stride,
    int do_tick,
    size_t *p_result_count)
{
    int ok = 1;
    size_t i, result_count = 0;
    SSL_POLL_ITEM *item;
    SSL *ssl;
#if !defined(OPENSSL_NO_QUIC) || !defined(OPENSSL_NO_DTLS)
    uint64_t events;
#endif
    uint64_t revents;

    for (i = 0; i < num_items; ++i) {
        item = &ITEM_N(items, stride, i);
#if !defined(OPENSSL_NO_QUIC) || !defined(OPENSSL_NO_DTLS)
        events = item->events;
#endif
        revents = 0;

        switch (item->desc.type) {
        case BIO_POLL_DESCRIPTOR_TYPE_SSL:
            ssl = item->desc.value.ssl;
            if (ssl == NULL)
                /* NULL items are no-ops and have revents reported as 0 */
                break;

            switch (ssl->type) {
#ifndef OPENSSL_NO_QUIC
            case SSL_TYPE_QUIC_LISTENER:
            case SSL_TYPE_QUIC_CONNECTION:
            case SSL_TYPE_QUIC_XSO:
                if (!ossl_quic_conn_poll_events(ssl, events, do_tick, &revents))
                    /* above call raises ERR */
                    FAIL_ITEM(i);

                if (revents != 0)
                    ++result_count;

                break;
#endif

#ifndef OPENSSL_NO_DTLS
            case SSL_TYPE_DTLS_LISTENER:
                if (!ossl_dtls_listener_poll_events(ssl, events, do_tick, &revents))
                    /* above call raises ERR */
                    FAIL_ITEM(i);

                if (revents != 0)
                    ++result_count;
                break;
            case SSL_TYPE_SSL_CONNECTION:
                if (SSL_is_dtls(ssl)) {
                    if (!ossl_dtls_conn_poll_events(ssl, events, do_tick, &revents))
                        /* above call raises ERR */
                        FAIL_ITEM(i);

                    if (revents != 0)
                        ++result_count;
                } else {
                    /* TLS Connections not supported */
                    ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                        "SSL_poll currently only supports QUIC and DTLS SSL objects");
                    FAIL_ITEM(i);
                }
                break;
#endif

            default:
                ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                    "SSL_poll currently only supports QUIC and DTLS SSL objects");
                FAIL_ITEM(i);
            }
            break;
        case BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD:
            ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                "SSL_poll currently does not support polling "
                "sockets");
            FAIL_ITEM(i);
        default:
            ERR_raise_data(ERR_LIB_SSL, SSL_R_POLL_REQUEST_NOT_SUPPORTED,
                "SSL_poll does not support unknown poll descriptor "
                "type %d",
                item->desc.type);
            FAIL_ITEM(i);
        }

        item->revents = revents;
    }

out:
    if (p_result_count != NULL)
        *p_result_count = result_count;

    return ok;
}

int SSL_poll(SSL_POLL_ITEM *items,
    size_t num_items,
    size_t stride,
    const struct timeval *timeout,
    uint64_t flags,
    size_t *p_result_count)
{
    int ok = 1;
    size_t result_count = 0;
    ossl_unused int do_tick = ((flags & SSL_POLL_FLAG_NO_HANDLE_EVENTS) == 0);
    OSSL_TIME deadline;

    /* Trivial case. */
    if (num_items == 0) {
        if (timeout == NULL)
            goto out;
        OSSL_sleep(ossl_time2ms(ossl_time_from_timeval(*timeout)));
        goto out;
    }

    /* Convert timeout to deadline. */
    if (timeout == NULL)
        deadline = ossl_time_infinite();
    else if (timeout->tv_sec == 0 && timeout->tv_usec == 0)
        deadline = ossl_time_zero();
    else
        deadline = ossl_time_add(ossl_time_now(),
            ossl_time_from_timeval(*timeout));

    /* Loop until we have something to report. */
    for (;;) {
        /* Readout phase - poll current state of each item. */
        if (!poll_readout(items, num_items, stride, do_tick, &result_count)) {
            ok = 0;
            goto out;
        }

        /*
         * If we got anything, or we are in immediate mode (zero timeout), or
         * the deadline has expired, we're done.
         */
        if (result_count > 0
            || ossl_time_is_zero(deadline) /* (avoids now call) */
            || ossl_time_compare(ossl_time_now(), deadline) >= 0)
            goto out;

        /*
         * Block until something is ready. Ignore NO_HANDLE_EVENTS from this
         * point onwards.
         */
        do_tick = 1;
#if !defined(OPENSSL_NO_QUIC) || !defined(OPENSSL_NO_DTLS)
        if (!poll_block(items, num_items, stride, deadline,
                /*bound_by_event_timeout=*/1, &result_count)) {
            ok = 0;
            goto out;
        }
#endif
    }

    /* TODO(QUIC POLLING): Support for polling FDs */

out:
    if (p_result_count != NULL)
        *p_result_count = result_count;

    return ok;
}

/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_DGRAM_DEMUX_H
#define OSSL_DGRAM_DEMUX_H
#pragma once

#include <openssl/ssl.h>
#include "internal/bio_addr.h"
#include "internal/time.h"
#include "internal/list.h"

#if !defined(OPENSSL_NO_QUIC) || !defined(OPENSSL_NO_DTLS)

/*
 * Generic Datagram Demuxer
 * ========================
 *
 * The datagram demuxer is responsible for receiving datagrams from the network
 * via a datagram BIO. It maintains a pool of Unprocessed RX Entries (URXEs)
 * for efficient batch receiving via BIO_recvmmsg().
 *
 * The demuxer is protocol-agnostic. It receives datagrams and invokes a
 * callback for each one. The callback is responsible for routing the datagram
 * to the appropriate connection (e.g., by DCID for QUIC, or by peer address
 * for DTLS).
 */

/* Forward declarations */
typedef struct dgram_demux_st DGRAM_DEMUX;
typedef struct dgram_urxe_st DGRAM_URXE;

/*
 * URXE (Unprocessed RX Entry) structure.
 *
 * This structure is exposed so that callers can manage URXEs in their own
 * queues when needed. The data buffer follows immediately after this structure.
 *
 * This structure includes fields used by QUIC (processed, hpr_removed, deferred)
 * which are unused by DTLS. This allows QUIC_URXE to be a simple typedef to
 * DGRAM_URXE, enabling QUIC_DEMUX to wrap DGRAM_DEMUX without list type issues.
 */
struct dgram_urxe_st {
    OSSL_LIST_MEMBER(urxe, DGRAM_URXE);

    /*
     * The URXE data starts after this structure so we don't need a pointer.
     * data_len stores the current length (i.e., the length of the received
     * datagram) and alloc_len stores the allocation length. The URXE will be
     * reallocated if we need a larger allocation than is available, though this
     * should not be common as we will have a good idea of worst-case MTUs up
     * front.
     */
    size_t data_len, alloc_len;

    /*
     * Bitfields per packet. processed indicates the packet has been processed
     * and must not be processed again, hpr_removed indicates header protection
     * has already been removed. Used by QUIC QRX only; not used by the demuxer
     * or DTLS.
     */
    uint64_t processed, hpr_removed;

    /*
     * This monotonically increases with each datagram received. It is used for
     * diagnostic purposes only.
     */
    uint64_t datagram_id;

    /*
     * Address of peer we received the datagram from, and the local interface
     * address we received it on. If local address support is not enabled, local
     * is zeroed.
     */
    BIO_ADDR peer, local;

    /*
     * Time at which datagram was received (or ossl_time_zero()) if a now
     * function was not provided).
     */
    OSSL_TIME time;

    /*
     * Used by the QUIC QRX to mark whether a datagram has been deferred.
     * Not used by the demuxer or DTLS.
     */
    char deferred;

    /*
     * Used by the DEMUX to track if a URXE has been handed out. Used primarily
     * for debugging purposes.
     */
    char demux_state;
};

/* Values for demux_state field */
#define URXE_DEMUX_STATE_FREE 0 /* on urx_free list */
#define URXE_DEMUX_STATE_PENDING 1 /* on urx_pending list */
#define URXE_DEMUX_STATE_ISSUED 2 /* on neither list */

/* List structure tracking a queue of URXEs. */
DEFINE_LIST_OF(urxe, DGRAM_URXE);
typedef OSSL_LIST(urxe) DGRAM_URXE_LIST;

/*
 * List management helpers. These are used by the demuxer but can also be used
 * by users of the demuxer to manage URXEs.
 */
void ossl_dgram_urxe_remove(DGRAM_URXE_LIST *l, DGRAM_URXE *e);
void ossl_dgram_urxe_insert_head(DGRAM_URXE_LIST *l, DGRAM_URXE *e);
void ossl_dgram_urxe_insert_tail(DGRAM_URXE_LIST *l, DGRAM_URXE *e);

/*
 * Callback function type for datagram routing.
 *
 * Called when a datagram is received. e is a URXE containing the datagram
 * payload. It is permissible for the callee to mutate this buffer; once the
 * demuxer calls this callback, it will never read the buffer again.
 *
 * The callee must arrange for ossl_dgram_demux_release_urxe or
 * ossl_dgram_demux_reinject_urxe to be called on the URXE at some point in the
 * future (this need not be before the callback returns).
 *
 * At the time the callback is made, the URXE will not be in any queue,
 * therefore the callee can use the prev and next fields as it wishes.
 */
typedef void(ossl_dgram_demux_cb_fn)(DGRAM_URXE *e, void *arg);

/*
 * Creates a new demuxer. The given BIO is used to receive datagrams from the
 * network using BIO_recvmmsg.
 *
 * threadsafe: If non-zero, enables internal locking for thread safety. Use this
 *             when the demux may be accessed from multiple threads (DTLS
 *             where connections share a demux). Pass 0 if thread safety is
 *             handled at a higher level (QUIC).
 *
 * now is an optional function used to determine the time a datagram was
 * received. now_arg is an opaque argument passed to the function. If now is
 * NULL, ossl_time_zero() is used as the datagram reception time.
 */
DGRAM_DEMUX *ossl_dgram_demux_new(BIO *net_bio,
    int threadsafe,
    OSSL_TIME (*now)(void *arg),
    void *now_arg);

/*
 * Destroy a demuxer. All URXEs must have been released back to the demuxer
 * before calling this. No-op if demux is NULL.
 */
void ossl_dgram_demux_free(DGRAM_DEMUX *demux);

/*
 * Changes the BIO which the demuxer reads from. This also sets the MTU if the
 * BIO supports querying the MTU.
 */
void ossl_dgram_demux_set_bio(DGRAM_DEMUX *demux, BIO *net_bio);

/*
 * Changes the MTU in bytes we use to receive datagrams.
 * Returns 1 on success, 0 if mtu is below minimum.
 */
int ossl_dgram_demux_set_mtu(DGRAM_DEMUX *demux, unsigned int mtu);

/*
 * Set the default packet handler. This is called for every incoming datagram.
 * If a default packet handler is not set, received datagrams are silently
 * dropped. A default packet handler may be unset by passing NULL.
 *
 * The handler is responsible for ensuring that ossl_dgram_demux_reinject_urxe
 * or ossl_dgram_demux_release_urxe is called on the passed packet at some
 * point in the future, which may or may not be before the handler returns.
 */
void ossl_dgram_demux_set_default_handler(DGRAM_DEMUX *demux,
    ossl_dgram_demux_cb_fn *cb,
    void *cb_arg);

/*
 * Releases a URXE back to the demuxer. No reference must be made to the URXE or
 * its buffer after calling this function. The URXE must not be in any queue;
 * that is, its prev and next pointers must be NULL.
 */
void ossl_dgram_demux_release_urxe(DGRAM_DEMUX *demux, DGRAM_URXE *e);

/*
 * Reinjects a URXE back into the pending queue. This is useful when a packet
 * needs to be reprocessed. Once this has been called, the caller must not
 * touch the URXE anymore and must not also call ossl_dgram_demux_release_urxe().
 *
 * The URXE is reinjected at the head of the queue, so it will be reprocessed
 * immediately.
 */
void ossl_dgram_demux_reinject_urxe(DGRAM_DEMUX *demux, DGRAM_URXE *e);

/*
 * Process any unprocessed RX'd datagrams, by calling registered callbacks,
 * reading more datagrams from the BIO if necessary.
 *
 * Returns one of the following values:
 *
 *     DGRAM_DEMUX_PUMP_RES_OK
 *         At least one incoming datagram was processed.
 *
 *     DGRAM_DEMUX_PUMP_RES_TRANSIENT_FAIL
 *         No more incoming datagrams are currently available.
 *         Call again later.
 *
 *     DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL
 *         Either the network read BIO has failed in a non-transient fashion, or
 *         an internal state, assertion or allocation error occurred. The caller
 *         should tear down the connection.
 */
#define DGRAM_DEMUX_PUMP_RES_OK 1
#define DGRAM_DEMUX_PUMP_RES_TRANSIENT_FAIL (-1)
#define DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL (-2)

int ossl_dgram_demux_pump(DGRAM_DEMUX *demux);

/*
 * Artificially inject a packet into the demuxer for testing purposes. The
 * buffer must not exceed the URXE size being used by the demuxer.
 *
 * If peer or local are NULL, their respective fields are zeroed in the injected
 * URXE.
 *
 * Returns 1 on success or 0 on failure.
 */
int ossl_dgram_demux_inject(DGRAM_DEMUX *demux,
    const unsigned char *buf,
    size_t buf_len,
    const BIO_ADDR *peer,
    const BIO_ADDR *local);

/*
 * Returns 1 if there are any pending URXEs.
 */
int ossl_dgram_demux_has_pending(const DGRAM_DEMUX *demux);

/*
 * Accessor for URXE data buffer. This returns a pointer to the data buffer
 * that follows the URXE structure.
 */
static ossl_unused ossl_inline unsigned char *
ossl_dgram_urxe_data(const DGRAM_URXE *e)
{
    return (unsigned char *)&e[1];
}

static ossl_unused ossl_inline unsigned char *
ossl_dgram_urxe_data_end(const DGRAM_URXE *e)
{
    return ossl_dgram_urxe_data(e) + e->data_len;
}

#endif /* !OPENSSL_NO_QUIC || !OPENSSL_NO_DTLS */
#endif /* OSSL_DGRAM_DEMUX_H */

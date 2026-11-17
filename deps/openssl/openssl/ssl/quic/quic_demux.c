/*
 * Copyright 2022-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/quic_demux.h"
#include "internal/quic_wire_pkt.h"
#include "internal/dgram_demux.h"
#include "internal/common.h"
#include <openssl/lhash.h>
#include <openssl/err.h>

/*
 * QUIC Demuxer Implementation
 * ===========================
 *
 * The QUIC demuxer wraps the generic DGRAM_DEMUX to add QUIC-specific
 * functionality:
 *   - Extraction of DCID from the first packet in each datagram
 *   - QUIC-specific callback signature that includes the DCID
 *
 * QUIC_URXE is a typedef to DGRAM_URXE, so all URXE management is delegated
 * to the underlying DGRAM_DEMUX.
 */
struct quic_demux_st {
    /* The underlying generic datagram demuxer. */
    DGRAM_DEMUX *dgram_demux;

    /*
     * QUIC short packets do not contain the length of the connection ID field,
     * therefore it must be known contextually. The demuxer requires connection
     * IDs of the same length to be used for all incoming packets.
     */
    size_t short_conn_id_len;

    /* The QUIC-specific packet handler callback (includes DCID). */
    ossl_quic_demux_cb_fn *default_cb;
    void *default_cb_arg;
};

/*
 * Internal callback that wraps the QUIC callback. This is called by
 * DGRAM_DEMUX for each received datagram. We extract the DCID and forward
 * to the QUIC-specific callback.
 */
static void quic_demux_dgram_cb(DGRAM_URXE *e, void *arg)
{
    QUIC_DEMUX *demux = arg;

    if (demux->default_cb != NULL) {
        QUIC_CONN_ID dst_conn_id;
        int dst_conn_id_ok;

        /* Extract DCID from the first packet in the datagram. */
        dst_conn_id_ok = ossl_quic_wire_get_pkt_hdr_dst_conn_id(
            ossl_quic_urxe_data(e),
            e->data_len,
            demux->short_conn_id_len,
            &dst_conn_id);

        demux->default_cb(e, demux->default_cb_arg,
            dst_conn_id_ok ? &dst_conn_id : NULL);
    } else {
        /* No handler set, release the URXE back to the demuxer. */
        ossl_dgram_demux_release_urxe(demux->dgram_demux, e);
    }
}

QUIC_DEMUX *ossl_quic_demux_new(BIO *net_bio,
    size_t short_conn_id_len,
    OSSL_TIME (*now)(void *arg),
    void *now_arg)
{
    QUIC_DEMUX *demux;

    demux = OPENSSL_zalloc(sizeof(QUIC_DEMUX));
    if (demux == NULL)
        return NULL;

    demux->short_conn_id_len = short_conn_id_len;

    /* Create the underlying generic demuxer (no internal locking for QUIC). */
    demux->dgram_demux = ossl_dgram_demux_new(net_bio, 0, now, now_arg);
    if (demux->dgram_demux == NULL) {
        OPENSSL_free(demux);
        return NULL;
    }

    /*
     * Set our internal wrapper callback on the DGRAM_DEMUX. This will be
     * called for every received datagram, and we'll extract the DCID and
     * forward to the QUIC-specific callback.
     */
    ossl_dgram_demux_set_default_handler(demux->dgram_demux,
        quic_demux_dgram_cb, demux);

    return demux;
}

void ossl_quic_demux_free(QUIC_DEMUX *demux)
{
    if (demux == NULL)
        return;

    ossl_dgram_demux_free(demux->dgram_demux);
    OPENSSL_free(demux);
}

void ossl_quic_demux_set_bio(QUIC_DEMUX *demux, BIO *net_bio)
{
    ossl_dgram_demux_set_bio(demux->dgram_demux, net_bio);
}

int ossl_quic_demux_set_mtu(QUIC_DEMUX *demux, unsigned int mtu)
{
    return ossl_dgram_demux_set_mtu(demux->dgram_demux, mtu);
}

void ossl_quic_demux_set_default_handler(QUIC_DEMUX *demux,
    ossl_quic_demux_cb_fn *cb,
    void *cb_arg)
{
    demux->default_cb = cb;
    demux->default_cb_arg = cb_arg;
}

int ossl_quic_demux_pump(QUIC_DEMUX *demux)
{
    int ret;

    ret = ossl_dgram_demux_pump(demux->dgram_demux);

    /*
     * Map DGRAM_DEMUX_PUMP_RES_* to QUIC_DEMUX_PUMP_RES_*. The values are
     * identical, but this provides documentation and future-proofing.
     */
    switch (ret) {
    case DGRAM_DEMUX_PUMP_RES_OK:
        return QUIC_DEMUX_PUMP_RES_OK;
    case DGRAM_DEMUX_PUMP_RES_TRANSIENT_FAIL:
        return QUIC_DEMUX_PUMP_RES_TRANSIENT_FAIL;
    case DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL:
    default:
        return QUIC_DEMUX_PUMP_RES_PERMANENT_FAIL;
    }
}

/* Artificially inject a packet into the demuxer for testing purposes. */
int ossl_quic_demux_inject(QUIC_DEMUX *demux,
    const unsigned char *buf,
    size_t buf_len,
    const BIO_ADDR *peer,
    const BIO_ADDR *local)
{
    return ossl_dgram_demux_inject(demux->dgram_demux, buf, buf_len,
        peer, local);
}

/* Called by our user to return a URXE to the free list. */
void ossl_quic_demux_release_urxe(QUIC_DEMUX *demux, QUIC_URXE *e)
{
    ossl_dgram_demux_release_urxe(demux->dgram_demux, e);
}

void ossl_quic_demux_reinject_urxe(QUIC_DEMUX *demux, QUIC_URXE *e)
{
    ossl_dgram_demux_reinject_urxe(demux->dgram_demux, e);
}

int ossl_quic_demux_has_pending(const QUIC_DEMUX *demux)
{
    return ossl_dgram_demux_has_pending(demux->dgram_demux);
}

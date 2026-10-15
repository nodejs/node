/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "ssl_local.h"

#ifndef OPENSSL_NO_DTLS

/*
 * Creates a new DTLS_RX structure. The demuxer is
 * provided and owned by the DTLS listener.
 */
DTLS_RX *ossl_dtls_rx_new(DGRAM_DEMUX *demux)
{
    DTLS_RX *rx = OPENSSL_malloc(sizeof(*rx));
    if (rx == NULL)
        return NULL;

    rx->demux = demux;
    ossl_list_urxe_init(&rx->urxe_pending);
    rx->mutex = ossl_crypto_mutex_new();

#ifdef OPENSSL_THREADS
    if (rx->mutex == NULL) {
        OPENSSL_free(rx);
        return NULL;
    }
#endif

    return rx;
}

/*
 * Frees a DTLS_RX struct. The demuxer is not freed, as it is
 * owned by the DTLS listener. The list is walked and freed.
 */
void ossl_dtls_rx_free(DTLS_RX *rx)
{
    DGRAM_URXE *urxe, *urxe_next;

    if (rx == NULL)
        return;

    for (urxe = ossl_list_urxe_head(&rx->urxe_pending); urxe != NULL;
        urxe = urxe_next) {
        urxe_next = ossl_list_urxe_next(urxe);
        ossl_list_urxe_remove(&rx->urxe_pending, urxe);
        ossl_dgram_demux_release_urxe(rx->demux, urxe);
    }

    /*
     * The demuxer is not freed, as it is owned by the DTLS listener.
     */
    rx->demux = NULL;

    ossl_crypto_mutex_free(&rx->mutex);
    OPENSSL_free(rx);
}

/*
 * Injects a received URXE into the DTLS_RX struct.
 */
void ossl_dtls_rx_inject_urxe(DTLS_RX *rx, DGRAM_URXE *e)
{
    ossl_crypto_mutex_lock(rx->mutex);
    ossl_list_urxe_insert_tail(&rx->urxe_pending, e);
    ossl_crypto_mutex_unlock(rx->mutex);
}

/*
 * Release the URXE from the DTLS_RX struct.
 */
void ossl_dtls_rx_release_urxe(DTLS_RX *rx, DGRAM_URXE *e)
{
    ossl_dgram_demux_release_urxe(rx->demux, e);
}

/*
 * Reads a datagram from the DTLS_RX struct.
 */
DGRAM_URXE *ossl_dtls_read_datagram(DTLS_RX *rx)
{
    DGRAM_URXE *e;

    ossl_crypto_mutex_lock(rx->mutex);

    if (ossl_list_urxe_is_empty(&rx->urxe_pending)) {
        ossl_crypto_mutex_unlock(rx->mutex);
        return NULL;
    }

    e = ossl_list_urxe_head(&rx->urxe_pending);
    ossl_list_urxe_remove(&rx->urxe_pending, e);
    e->demux_state = URXE_DEMUX_STATE_ISSUED;

    ossl_crypto_mutex_unlock(rx->mutex);
    return e;
}

#endif /* OPENSSL_NO_DTLS */

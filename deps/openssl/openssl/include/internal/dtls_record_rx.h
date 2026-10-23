/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_DTLS_RECORD_RX_H
#define OSSL_DTLS_RECORD_RX_H
#pragma once

#include "internal/dgram_demux.h"
#include "internal/thread_arch.h"

#ifndef OPENSSL_NO_DTLS

/*
 * Maximum datagrams queued per connection in DTLS_RX::urxe_pending: a
 * count, not a byte cap. On overflow the oldest datagram is evicted
 * (DTLS permits datagram loss). Each queued URXE holds a buffer of the
 * listener's maximum datagram size, so 128 entries cost about 295 KB per
 * connection at the default 2000-byte size - roughly one default kernel
 * socket receive buffer, the bound that queuing into userspace would
 * otherwise bypass.
 */
#define OSSL_DTLS_RX_DEFAULT_MAX_QUEUED 128

typedef struct dtls_rx_st {
    DGRAM_DEMUX *demux;
    DGRAM_URXE_LIST urxe_pending;
    CRYPTO_MUTEX *mutex;
} DTLS_RX;

/*
 * Creates a new DTLS_RX structe. The demuxer is
 * provided and owned by the DTLS listener.
 */
DTLS_RX *ossl_dtls_rx_new(DGRAM_DEMUX *demux);

/*
 * Frees a DTLS_RX struct. The demuxer is not freed, as it is
 * owned by the DTLS listener. The list is walked and freed.
 */
void ossl_dtls_rx_free(DTLS_RX *rx);

/*
 * Injects a received URXE into the DTLS_RX struct.
 */
void ossl_dtls_rx_inject_urxe(DTLS_RX *rx, DGRAM_URXE *e);

/*
 * Release the URXE from the DTLS_RX struct.
 */
void ossl_dtls_rx_release_urxe(DTLS_RX *rx, DGRAM_URXE *e);

/*
 * Reads a datagram from the DTLS_RX struct.
 */
DGRAM_URXE *ossl_dtls_read_datagram(DTLS_RX *rx);

#endif /* OPENSSL_NO_DTLS */
#endif /* OSSL_DTLS_RECORD_RX_H */

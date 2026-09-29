/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_QUIC_RECORD_RX_LOCAL_H
#define OSSL_QUIC_RECORD_RX_LOCAL_H

#include "internal/quic_record_rx.h"
#include "internal/list.h"

#ifndef OPENSSL_NO_QUIC

/*
 * RXE
 * ===
 *
 * RX Entries (RXEs) store processed (i.e., decrypted) data received from the
 * network. One RXE is used per received QUIC packet.
 *
 * The OSSL_QRX_PKT handed out to users of the QRX is the first member, so a
 * packet pointer can be cast back to its RXE. It is intended that only the
 * QRX implementation access this structure directly, tests which need to
 * construct a packet without a QRX being the exception.
 */
typedef struct rxe_st RXE;

struct rxe_st {
    OSSL_QRX_PKT pkt;
    OSSL_LIST_MEMBER(rxe, RXE);
    size_t data_len, alloc_len, refcount;

    /* Extra fields for per-packet information. */
    QUIC_PKT_HDR hdr; /* data/len are decrypted payload */

    /* Decoded packet number. */
    QUIC_PN pn;

    /* Addresses copied from URXE. */
    BIO_ADDR peer, local;

    /* Time we received the packet (not when we processed it). */
    OSSL_TIME time;

    /* Total length of the datagram which contained this packet. */
    size_t datagram_len;

    /*
     * The key epoch the packet was received with. Always 0 for non-1-RTT
     * packets.
     */
    uint64_t key_epoch;

    /*
     * Monotonically increases with each datagram received.
     * For diagnostic use only.
     */
    uint64_t datagram_id;

    /*
     * alloc_len allocated bytes (of which data_len bytes are valid) follow this
     * structure.
     */
};

DEFINE_LIST_OF(rxe, RXE);
typedef OSSL_LIST(rxe) RXE_LIST;

static ossl_inline unsigned char *rxe_data(const RXE *e)
{
    return (unsigned char *)(e + 1);
}

#endif

#endif

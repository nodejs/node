/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "ssl_local.h"

/*
 * RFC 9147 §5.2: strip msg_seq, fragment_offset, and fragment_length from
 * each DTLS handshake message header before feeding into the transcript hash.
 * Used by both ssl3_finish_mac and tls13_change_cipher_state.
 */
int dtls13_transcript_hash_update(EVP_MD_CTX *mdctx,
    const unsigned char *buf, size_t len)
{
    while (len > 0) {
        PACKET hmhdr;
        unsigned long hmbodylen;
        unsigned int msgtype;
        size_t hmhdrlen;

        if (!ossl_assert(len >= SSL3_HM_HEADER_LENGTH)
            || !PACKET_buf_init(&hmhdr, buf, SSL3_HM_HEADER_LENGTH)
            || !PACKET_get_1(&hmhdr, &msgtype)
            || !PACKET_get_net_3(&hmhdr, &hmbodylen))
            return 0;

        hmhdrlen = (msgtype == SSL3_MT_MESSAGE_HASH)
            ? SSL3_HM_HEADER_LENGTH
            : DTLS1_HM_HEADER_LENGTH;

        if (!ossl_assert(hmhdrlen + hmbodylen <= len)
            || !EVP_DigestUpdate(mdctx, buf, SSL3_HM_HEADER_LENGTH)
            || !EVP_DigestUpdate(mdctx, buf + hmhdrlen, hmbodylen))
            return 0;

        buf += hmhdrlen + hmbodylen;
        len -= hmhdrlen + hmbodylen;
    }
    return 1;
}

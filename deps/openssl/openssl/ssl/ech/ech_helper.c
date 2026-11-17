/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/ssl.h>
#include <openssl/ech.h>
#include "../ssl_local.h"
#include "ech_local.h"
#include "internal/ech_helpers.h"

/* used in ECH crypto derivations (odd format for EBCDIC goodness) */
/* "tls ech" */
static const char OSSL_ECH_CONTEXT_STRING[] = "\x74\x6c\x73\x20\x65\x63\x68";

/*
 * Construct HPKE "info" input as per spec
 * encoding is the ECHconfig being used
 * encoding_length is the length of ECHconfig being used
 * info is a caller-allocated buffer for results
 * info_len is the buffer size on input, used-length on output
 * return 1 for success, zero otherwise
 */
int ossl_ech_make_enc_info(const unsigned char *encoding,
    size_t encoding_length,
    unsigned char *info, size_t *info_len)
{
    WPACKET ipkt = { 0 };

    if (encoding == NULL || info == NULL || info_len == NULL)
        return 0;
    if (!WPACKET_init_static_len(&ipkt, info, *info_len, 0)
        || !WPACKET_memcpy(&ipkt, OSSL_ECH_CONTEXT_STRING,
            sizeof(OSSL_ECH_CONTEXT_STRING) - 1)
        /*
         * the zero valued octet is required by the spec, section 7.1 so
         * a tiny bit better to add it explicitly rather than depend on
         * the context string being NUL terminated
         */
        || !WPACKET_put_bytes_u8(&ipkt, 0)
        || !WPACKET_memcpy(&ipkt, encoding, encoding_length)
        || !WPACKET_get_total_written(&ipkt, info_len)) {
        WPACKET_cleanup(&ipkt);
        return 0;
    }
    WPACKET_cleanup(&ipkt);
    return 1;
}

/*
 * Given a CH find the offsets of the session id, extensions and ECH
 * ch is the encoded client hello
 * ch_len is the length of ch
 * sessid_off returns offset of session_id length
 * exts_off points to offset of extensions
 * exts_len returns length of extensions
 * ech_off returns offset of ECH
 * echtype returns the ext type of the ECH
 * ech_len returns the length of the ECH
 * sni_off returns offset of (outer) SNI
 * sni_len returns the length of the SNI
 * inner 1 if the ECH is marked as an inner, 0 for outer
 * return 1 for success, other otherwise
 *
 * Offsets are set to zero if relevant thing not found.
 * Offsets are returned to the type or length field in question.
 *
 * Note: input here is untrusted!
 */
int ossl_ech_helper_get_ch_offsets(const unsigned char *ch, size_t ch_len,
    size_t *sessid_off, size_t *exts_off,
    size_t *exts_len,
    size_t *ech_off, uint16_t *echtype,
    size_t *ech_len, size_t *sni_off,
    size_t *sni_len, int *inner)
{
    unsigned int elen = 0, etype = 0, pi_tmp = 0;
    const unsigned char *pp_tmp = NULL, *chstart = NULL, *estart = NULL;
    PACKET pkt;
    int done = 0;

    if (ch == NULL || ch_len == 0 || sessid_off == NULL || exts_off == NULL
        || ech_off == NULL || echtype == NULL || ech_len == NULL
        || sni_off == NULL || inner == NULL || exts_len == NULL)
        return 0;
    *sessid_off = *exts_off = *ech_off = *sni_off = *sni_len = *ech_len = 0;
    *exts_len = 0;
    *inner = OSSL_ECH_UNKNOWN_CH_TYPE;
    *echtype = 0xffff;
    if (!PACKET_buf_init(&pkt, ch, ch_len))
        return 0;
    chstart = PACKET_data(&pkt);
    if (!PACKET_get_net_2(&pkt, &pi_tmp))
        return 0;
    /* if we're not TLSv1.2+ then we can bail, but it's not an error */
    if (pi_tmp != TLS1_2_VERSION)
        return 1;
    /* chew up the packet to extensions */
    if (!PACKET_get_bytes(&pkt, &pp_tmp, SSL3_RANDOM_SIZE)
        || (*sessid_off = PACKET_data(&pkt) - chstart) == 0
        || !PACKET_get_1(&pkt, &pi_tmp) /* sessid len */
        || !PACKET_get_bytes(&pkt, &pp_tmp, pi_tmp) /* sessid */
        || !PACKET_get_net_2(&pkt, &pi_tmp) /* ciphersuite len */
        || !PACKET_get_bytes(&pkt, &pp_tmp, pi_tmp) /* suites */
        || !PACKET_get_1(&pkt, &pi_tmp) /* compression meths */
        || !PACKET_get_bytes(&pkt, &pp_tmp, pi_tmp) /* comp meths */
        || (*exts_off = PACKET_data(&pkt) - chstart) == 0
        || !PACKET_get_net_2(&pkt, &pi_tmp) /* len(extensions) */
        || (*exts_len = (size_t)pi_tmp) == 0)
        /*
         * unexpectedly, we return 1 here, as doing otherwise will
         * break some non-ECH test code that truncates CH messages
         * The same is true below when looking through extensions.
         * That's ok though, we'll only set those offsets we've
         * found.
         */
        return 1;
    /* no extensions is theoretically ok, if uninteresting */
    if (*exts_len == 0)
        return 1;
    /* find what we want from extensions */
    estart = PACKET_data(&pkt);
    while (PACKET_remaining(&pkt) > 0
        && (size_t)(PACKET_data(&pkt) - estart) < *exts_len
        && done < 2) {
        if (!PACKET_get_net_2(&pkt, &etype)
            || !PACKET_get_net_2(&pkt, &elen))
            return 1; /* see note above */
        if (etype == TLSEXT_TYPE_ech) {
            if (elen == 0)
                return 0;
            *ech_off = PACKET_data(&pkt) - chstart - 4;
            *echtype = etype;
            *ech_len = elen;
            done++;
        }
        if (etype == TLSEXT_TYPE_server_name) {
            *sni_off = PACKET_data(&pkt) - chstart - 4;
            *sni_len = elen;
            done++;
        }
        if (!PACKET_get_bytes(&pkt, &pp_tmp, elen))
            return 1; /* see note above */
        if (etype == TLSEXT_TYPE_ech)
            *inner = pp_tmp[0];
    }
    return 1;
}

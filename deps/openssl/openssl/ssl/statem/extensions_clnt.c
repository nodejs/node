/*
 * Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/ocsp.h>
#include <openssl/rand.h>
#include "../ssl_local.h"
#include "internal/cryptlib.h"
#include "internal/ssl_unwrap.h"
#include "internal/tlsgroups.h"
#include "statem_local.h"
#ifndef OPENSSL_NO_ECH
#include "internal/ech_helpers.h"
#endif

/* Used in the negotiate_dhe function */
typedef enum {
    ffdhe_check,
    ecdhe_check,
    ptfmt_check
} dhe_check_t;

EXT_RETURN tls_construct_ctos_renegotiate(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    if (!s->renegotiate) {
        const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION
                                                         : TLS1_3_VERSION;

        /* If not renegotiating, send an empty RI extension to indicate support */
        if ((s->min_proto_version != 0
                && ssl_version_cmp(s, s->min_proto_version, version1_3) >= 0)
            || (!SSL_CONNECTION_IS_DTLS(s)
                && ssl_security(s, SSL_SECOP_VERSION, 0, TLS1_VERSION, NULL)
                && s->min_proto_version <= TLS1_VERSION)) {
            /*
             * For TLS <= 1.0 SCSV is used instead, and for TLS 1.3 this
             * extension isn't used at all.
             */
            return EXT_RETURN_NOT_SENT;
        }

#ifndef OPENSSL_NO_ECH
        ECH_SAME_EXT(s, context, pkt)
#endif

        if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_renegotiate)
            || !WPACKET_start_sub_packet_u16(pkt)
            || !WPACKET_put_bytes_u8(pkt, 0)
            || !WPACKET_close(pkt)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }

        return EXT_RETURN_SENT;
    }

#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    /* Add a complete RI extension if renegotiating */
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_renegotiate)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_sub_memcpy_u8(pkt, s->s3.previous_client_finished,
            s->s3.previous_client_finished_len)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

EXT_RETURN tls_construct_ctos_server_name(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    char *chosen = s->ext.hostname;
#ifndef OPENSSL_NO_ECH
    OSSL_HPKE_SUITE suite;
    OSSL_ECHSTORE_ENTRY *ee = NULL;

    if (s->ext.ech.es != NULL) {
        if (ossl_ech_pick_matching_cfg(s, &ee, &suite) != 1) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_NOT_SENT;
        }
        /* Don't send outer SNI if external API says so */
        if (s->ext.ech.ch_depth == 0 && s->ext.ech.no_outer == 1)
            return EXT_RETURN_NOT_SENT;
        if (s->ext.ech.ch_depth == 1) /* inner */
            chosen = s->ext.hostname;
        if (s->ext.ech.ch_depth == 0) { /* outer */
            if (s->ext.ech.outer_hostname != NULL) /* prefer API */
                chosen = s->ext.ech.outer_hostname;
            else /* use name from ECHConfig */
                chosen = ee->public_name;
        }
    }
#endif
    if (chosen == NULL)
        return EXT_RETURN_NOT_SENT;
    /* Add TLS extension servername to the Client Hello message */
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_server_name)
        /* Sub-packet for server_name extension */
        || !WPACKET_start_sub_packet_u16(pkt)
        /* Sub-packet for servername list (always 1 hostname)*/
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_put_bytes_u8(pkt, TLSEXT_NAMETYPE_host_name)
        || !WPACKET_sub_memcpy_u16(pkt, chosen, strlen(chosen))
        || !WPACKET_close(pkt)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    return EXT_RETURN_SENT;
}

/* Push a Max Fragment Len extension into ClientHello */
EXT_RETURN tls_construct_ctos_maxfragmentlen(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    if (s->ext.max_fragment_len_mode == TLSEXT_max_fragment_length_DISABLED)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    /* Add Max Fragment Length extension if client enabled it. */
    /*-
     * 4 bytes for this extension type and extension length
     * 1 byte for the Max Fragment Length code value.
     */
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_max_fragment_length)
        /* Sub-packet for Max Fragment Length extension (1 byte) */
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_put_bytes_u8(pkt, s->ext.max_fragment_len_mode)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

#ifndef OPENSSL_NO_SRP
EXT_RETURN tls_construct_ctos_srp(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    /* Add SRP username if there is one */
    if (s->srp_ctx.login == NULL)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_srp)
        /* Sub-packet for SRP extension */
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_start_sub_packet_u8(pkt)
        /* login must not be zero...internal error if so */
        || !WPACKET_set_flags(pkt, WPACKET_FLAGS_NON_ZERO_LENGTH)
        || !WPACKET_memcpy(pkt, s->srp_ctx.login,
            strlen(s->srp_ctx.login))
        || !WPACKET_close(pkt)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}
#endif

/*
 * With (D)TLS < 1.3 the only negotiated supported key exchange groups are
 * FFDHE (RFC7919) and ECDHE/ECX (RFC8422 + legacy).  With (D)TLS 1.3, we add
 * KEMs, and the supported groups are no longer cipher-dependent.
 *
 * This function serves two purposes:
 *
 * - To determine whether to send the supported point formats extension.
 *   This is no longer applicable with (D)TLS >= 1.3.
 * - To determine whether to send the supported groups extension.
 *
 * In the former case, we only care about whether both ECC ciphers and EC/ECX
 * supported groups are configured, and the (D)TLS min version is at most 1.2.
 *
 * In the latter case, we also admit DHE ciphers with FFDHE groups, or any TLS
 * 1.3 cipher, since the extension is effectively mandatory for (D)TLS 1.3,
 * with the sole exception of psk-ke resumption, provided the client is sure
 * that the server will not want elect a full handshake. The check type then
 * indicates whether ECDHE or FFDHE negotiation should be performed.
 *
 * It returns 1 if negotiation is supported, 0 if it's not and -1 on error.
 */
static int negotiate_dhe(SSL_CONNECTION *s, dhe_check_t check_type,
    int min_version, int max_version)
{
    int i, end, ret = 0;
    STACK_OF(SSL_CIPHER) *cipher_stack = NULL;
    const uint16_t *pgroups = NULL;
    size_t num_groups, j;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);
    int dtls = SSL_CONNECTION_IS_DTLS(s);

    /* See if we support any EC or FFDHE ciphersuites */
    cipher_stack = SSL_get1_supported_ciphers(ssl);
    if (cipher_stack == NULL)
        return -1;
    end = sk_SSL_CIPHER_num(cipher_stack);
    for (i = 0; i < end; i++) {
        const SSL_CIPHER *c = sk_SSL_CIPHER_value(cipher_stack, i);
        unsigned long alg_k = c->algorithm_mkey;
        unsigned long alg_a = c->algorithm_auth;

        int is_ffdhe_ciphersuite = (alg_k & (SSL_kDHE | SSL_kDHEPSK));
        int is_ec_ciphersuite = ((alg_k & (SSL_kECDHE | SSL_kECDHEPSK))
            || (alg_a & SSL_aECDSA));
        int is_tls13 = (dtls ? DTLS_VERSION_GT(c->min_dtls, DTLS1_2_VERSION)
                             : (c->min_tls > TLS1_2_VERSION));

        if ((check_type == ffdhe_check && (is_ffdhe_ciphersuite || is_tls13))
            || (check_type == ecdhe_check && (is_ec_ciphersuite || is_tls13))
            || (check_type == ptfmt_check && is_ec_ciphersuite)) {
            ret = 1;
            break;
        }
    }
    sk_SSL_CIPHER_free(cipher_stack);
    if (ret == 0)
        return 0;

    /* Check we have at least one EC or FFDHE supported group */
    tls1_get_supported_groups(s, &pgroups, &num_groups);
    for (j = 0; j < num_groups; j++) {
        uint16_t ctmp = pgroups[j];
        const TLS_GROUP_INFO *ginfo = NULL;

        if (!tls_valid_group(s, ctmp, min_version, max_version, NULL, &ginfo))
            continue;

        if (check_type == ffdhe_check && is_ffdhe_group(ginfo->group_id)
            && tls_group_allowed(s, ctmp, SSL_SECOP_CURVE_SUPPORTED))
            return 1;

        if (check_type != ffdhe_check && is_ecdhe_group(ginfo->group_id)
            && tls_group_allowed(s, ctmp, SSL_SECOP_CURVE_SUPPORTED))
            return 1;
    }
    return 0;
}

EXT_RETURN tls_construct_ctos_ec_pt_formats(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    const unsigned char *pformats;
    size_t num_formats;
    int reason, min_version, max_version, dhe_result;

    reason = ssl_get_min_max_version(s, &min_version, &max_version, NULL);
    if (reason != 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, reason);
        return EXT_RETURN_FAIL;
    }
    dhe_result = negotiate_dhe(s, ptfmt_check, min_version, max_version);
    if (dhe_result == 0)
        return EXT_RETURN_NOT_SENT;
    if (dhe_result < 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    tls1_get_formatlist(s, &pformats, &num_formats);
    if (num_formats == 0)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    /* Add TLS extension ECPointFormats to the ClientHello message */
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_ec_point_formats)
        /* Sub-packet for formats extension */
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_sub_memcpy_u8(pkt, pformats, num_formats)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

EXT_RETURN tls_construct_ctos_supported_groups(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    const uint16_t *pgroups = NULL;
    size_t num_groups = 0, i, tls13added = 0, added = 0;
    int min_version, max_version, reason;
    const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION : TLS1_3_VERSION;
    int dtls = SSL_CONNECTION_IS_DTLS(s);
    int use_ecdhe, use_ffdhe;

    reason = ssl_get_min_max_version(s, &min_version, &max_version, NULL);
    if (reason != 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, reason);
        return EXT_RETURN_FAIL;
    }

    /*
     * If we don't support suitable groups, don't send the extension
     */
    use_ecdhe = negotiate_dhe(s, ecdhe_check, min_version, max_version);
    use_ffdhe = negotiate_dhe(s, ffdhe_check, min_version, max_version);
    if (use_ecdhe < 0 || use_ffdhe < 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    if (use_ecdhe == 0 && use_ffdhe == 0
        && (dtls ? DTLS_VERSION_LE(max_version, DTLS1_2_VERSION)
                 : (max_version <= TLS1_2_VERSION)))
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    /*
     * Add TLS extension supported_groups to the ClientHello message
     */
    tls1_get_supported_groups(s, &pgroups, &num_groups);

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_supported_groups)
        /* Sub-packet for supported_groups extension */
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_set_flags(pkt, WPACKET_FLAGS_NON_ZERO_LENGTH)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    /* RFC 8701: prepend a GREASE group value */
    if ((s->options & SSL_OP_GREASE) && !s->server) {
        if (!WPACKET_put_bytes_u16(pkt,
                ossl_grease_value(s, OSSL_GREASE_GROUP))) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }
    /* Copy group ID if supported */
    for (i = 0; i < num_groups; i++) {
        const TLS_GROUP_INFO *ginfo = NULL;
        uint16_t ctmp = pgroups[i];
        int okfortls13;

        if (!tls_valid_group(s, ctmp, min_version, max_version, &okfortls13,
                &ginfo)
            || (!use_ecdhe && is_ecdhe_group(ginfo->group_id))
            || (!use_ffdhe && is_ffdhe_group(ginfo->group_id))
            /* Note: SSL_SECOP_CURVE_SUPPORTED covers all key exchange groups */
            || !tls_group_allowed(s, ctmp, SSL_SECOP_CURVE_SUPPORTED))
            continue;

        if (!WPACKET_put_bytes_u16(pkt, ctmp)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        if (okfortls13 && max_version == version1_3)
            tls13added++;
        added++;
    }
    if (!WPACKET_close(pkt) || !WPACKET_close(pkt)) {
        if (added == 0)
            SSLfatal_data(s, SSL_AD_INTERNAL_ERROR, SSL_R_NO_SUITABLE_GROUPS,
                "No groups enabled for max supported SSL/TLS version");
        else
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    if (tls13added == 0 && max_version == version1_3) {
        SSLfatal_data(s, SSL_AD_INTERNAL_ERROR, SSL_R_NO_SUITABLE_GROUPS,
            "No groups enabled for max supported SSL/TLS version");
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

EXT_RETURN tls_construct_ctos_session_ticket(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    size_t ticklen;
    const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION : TLS1_3_VERSION;

    if (!tls_use_ticket(s))
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!s->new_session && s->session != NULL
        && s->session->ext.tick != NULL
        && s->session->ssl_version != version1_3) {
        ticklen = s->session->ext.ticklen;
    } else if (s->session && s->ext.session_ticket != NULL
        && s->ext.session_ticket->data != NULL) {
        ticklen = s->ext.session_ticket->length;
        s->session->ext.tick = OPENSSL_malloc(ticklen);
        if (s->session->ext.tick == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        memcpy(s->session->ext.tick,
            s->ext.session_ticket->data, ticklen);
        s->session->ext.ticklen = ticklen;
    } else {
        ticklen = 0;
    }

    if (ticklen == 0 && s->ext.session_ticket != NULL && s->ext.session_ticket->data == NULL)
        return EXT_RETURN_NOT_SENT;

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_session_ticket)
        || !WPACKET_sub_memcpy_u16(pkt, s->session->ext.tick, ticklen)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

EXT_RETURN tls_construct_ctos_sig_algs(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    size_t salglen;
    const uint16_t *salg;

    /*
     * This used both in the initial hello and as part of renegotiation,
     * in the latter case, the client version may be already set and may
     * be lower than that initially offered in `client_version`.
     */
    if (!SSL_CONNECTION_IS_DTLS(s)) {
        if (s->client_version < TLS1_2_VERSION
            || (s->ssl.method->version != TLS_ANY_VERSION
                && s->version < TLS1_2_VERSION))
            return EXT_RETURN_NOT_SENT;
    } else {
        if (DTLS_VERSION_LT(s->client_version, DTLS1_2_VERSION)
            || (s->ssl.method->version != DTLS_ANY_VERSION
                && DTLS_VERSION_LT(s->version, DTLS1_2_VERSION)))
            return EXT_RETURN_NOT_SENT;
    }

#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    salglen = tls12_get_psigalgs(s, 1, &salg);
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_signature_algorithms)
        /* Sub-packet for sig-algs extension */
        || !WPACKET_start_sub_packet_u16(pkt)
        /* Sub-packet for the actual list */
        || !WPACKET_start_sub_packet_u16(pkt)
        || !tls12_copy_sigalgs(s, pkt, salg, salglen)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    /* RFC 8701: append a GREASE signature algorithm value */
    if ((s->options & SSL_OP_GREASE) && !s->server) {
        if (!WPACKET_put_bytes_u16(pkt,
                ossl_grease_value(s, OSSL_GREASE_SIGALG))) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }
    if (!WPACKET_close(pkt)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

#ifndef OPENSSL_NO_OCSP
EXT_RETURN tls_construct_ctos_status_request(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    int i;

    /* This extension isn't defined for client Certificates */
    if (x != NULL)
        return EXT_RETURN_NOT_SENT;

    if (s->ext.status_type != TLSEXT_STATUSTYPE_ocsp)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_status_request)
        /* Sub-packet for status request extension */
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_put_bytes_u8(pkt, TLSEXT_STATUSTYPE_ocsp)
        /* Sub-packet for the ids */
        || !WPACKET_start_sub_packet_u16(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    for (i = 0; i < sk_OCSP_RESPID_num(s->ext.ocsp.ids); i++) {
        unsigned char *idbytes;
        OCSP_RESPID *id = sk_OCSP_RESPID_value(s->ext.ocsp.ids, i);
        int idlen = i2d_OCSP_RESPID(id, NULL);

        if (idlen <= 0
            /* Sub-packet for an individual id */
            || !WPACKET_sub_allocate_bytes_u16(pkt, idlen, &idbytes)
            || i2d_OCSP_RESPID(id, &idbytes) != idlen) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }
    if (!WPACKET_close(pkt)
        || !WPACKET_start_sub_packet_u16(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    if (s->ext.ocsp.exts) {
        unsigned char *extbytes;
        int extlen = i2d_X509_EXTENSIONS(s->ext.ocsp.exts, NULL);

        if (extlen < 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        if (!WPACKET_allocate_bytes(pkt, extlen, &extbytes)
            || i2d_X509_EXTENSIONS(s->ext.ocsp.exts, &extbytes)
                != extlen) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }
    if (!WPACKET_close(pkt) || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}
#endif

#ifndef OPENSSL_NO_NEXTPROTONEG
EXT_RETURN tls_construct_ctos_npn(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    if (SSL_CONNECTION_GET_CTX(s)->ext.npn_select_cb == NULL
        || !SSL_IS_FIRST_HANDSHAKE(s))
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    /*
     * The client advertises an empty extension to indicate its support
     * for Next Protocol Negotiation
     */
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_next_proto_neg)
        || !WPACKET_put_bytes_u16(pkt, 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}
#endif

EXT_RETURN tls_construct_ctos_alpn(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    unsigned char *aval = s->ext.alpn;
    size_t alen = s->ext.alpn_len;

    s->s3.alpn_sent = 0;
    if (!SSL_IS_FIRST_HANDSHAKE(s))
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    /*
     * If we have different alpn and alpn_outer values, then we set
     * the appropriate one for inner and outer.
     * If no alpn is set (for inner or outer), we don't send any.
     * If only an inner is set then we send the same in both.
     * Logic above is on the basis that alpn's aren't that sensitive,
     * usually, so special action is needed to do better.
     * We also don't support a way to send alpn only in the inner.
     * If you don't want the inner value in the outer, you have to
     * pick what to send in the outer and send that.
     */
    if (s->ext.ech.ch_depth == 1 && s->ext.alpn == NULL) /* inner */
        return EXT_RETURN_NOT_SENT;
    if (s->ext.ech.ch_depth == 0 && s->ext.alpn == NULL
        && s->ext.ech.alpn_outer == NULL) /* outer */
        return EXT_RETURN_NOT_SENT;
    if (s->ext.ech.ch_depth == 0 && s->ext.ech.alpn_outer != NULL) {
        aval = s->ext.ech.alpn_outer;
        alen = s->ext.ech.alpn_outer_len;
    }
#endif
    if (aval == NULL)
        return EXT_RETURN_NOT_SENT;
    if (!WPACKET_put_bytes_u16(pkt,
            TLSEXT_TYPE_application_layer_protocol_negotiation)
        /* Sub-packet ALPN extension */
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_sub_memcpy_u16(pkt, aval, alen)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    s->s3.alpn_sent = 1;
    return EXT_RETURN_SENT;
}

#ifndef OPENSSL_NO_SRTP
EXT_RETURN tls_construct_ctos_use_srtp(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);
    STACK_OF(SRTP_PROTECTION_PROFILE) *clnt = SSL_get_srtp_profiles(ssl);
    int i, end;

    if (clnt == NULL)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_use_srtp)
        /* Sub-packet for SRTP extension */
        || !WPACKET_start_sub_packet_u16(pkt)
        /* Sub-packet for the protection profile list */
        || !WPACKET_start_sub_packet_u16(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    end = sk_SRTP_PROTECTION_PROFILE_num(clnt);
    for (i = 0; i < end; i++) {
        const SRTP_PROTECTION_PROFILE *prof = sk_SRTP_PROTECTION_PROFILE_value(clnt, i);

        if (prof == NULL || !WPACKET_put_bytes_u16(pkt, prof->id)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }
    if (!WPACKET_close(pkt)
        /* Add an empty use_mki value */
        || !WPACKET_put_bytes_u8(pkt, 0)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}
#endif

EXT_RETURN tls_construct_ctos_etm(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    if (s->options & SSL_OP_NO_ENCRYPT_THEN_MAC)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_encrypt_then_mac)
        || !WPACKET_put_bytes_u16(pkt, 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

#ifndef OPENSSL_NO_CT
EXT_RETURN tls_construct_ctos_sct(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    if (s->ct_validation_callback == NULL)
        return EXT_RETURN_NOT_SENT;

    /* Not defined for client Certificates */
    if (x != NULL)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_signed_certificate_timestamp)
        || !WPACKET_put_bytes_u16(pkt, 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}
#endif

EXT_RETURN tls_construct_ctos_ems(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    if (s->options & SSL_OP_NO_EXTENDED_MASTER_SECRET)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_extended_master_secret)
        || !WPACKET_put_bytes_u16(pkt, 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

EXT_RETURN tls_construct_ctos_supported_versions(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    int currv, min_version, max_version, reason;
    const int isdtls = SSL_CONNECTION_IS_DTLS(s);
    const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION : TLS1_3_VERSION;

    reason = ssl_get_min_max_version(s, &min_version, &max_version, NULL);
    if (reason != 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, reason);
        return EXT_RETURN_FAIL;
    }

    /*
     * Don't include this if we can't negotiate (D)TLSv1.3.
     */
    if (ssl_version_cmp(s, max_version, version1_3) < 0)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_supported_versions)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_start_sub_packet_u8(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    /* RFC 8701: prepend a GREASE version value */
    if ((s->options & SSL_OP_GREASE) && !s->server) {
        if (!WPACKET_put_bytes_u16(pkt,
                ossl_grease_value(s, OSSL_GREASE_VERSION))) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }
    for (currv = max_version; ssl_version_cmp(s, currv, min_version) >= 0;
        isdtls ? currv++ : currv--) {
        if (!WPACKET_put_bytes_u16(pkt, currv)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }
    if (!WPACKET_close(pkt) || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

/*
 * Construct a psk_kex_modes extension.
 */
EXT_RETURN tls_construct_ctos_psk_kex_modes(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
#if !(defined(OPENSSL_NO_TLS1_3) && defined(OPENSSL_NO_DTLS1_3))
    int nodhe = s->options & SSL_OP_ALLOW_NO_DHE_KEX;

#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_psk_kex_modes)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_start_sub_packet_u8(pkt)
        || !WPACKET_put_bytes_u8(pkt, TLSEXT_KEX_MODE_KE_DHE)
        || (nodhe && !WPACKET_put_bytes_u8(pkt, TLSEXT_KEX_MODE_KE))
        || !WPACKET_close(pkt)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    s->ext.psk_kex_mode = TLSEXT_KEX_MODE_FLAG_KE_DHE;
    if (nodhe)
        s->ext.psk_kex_mode |= TLSEXT_KEX_MODE_FLAG_KE;
#endif

    return EXT_RETURN_SENT;
}

#if !(defined(OPENSSL_NO_TLS1_3) && defined(OPENSSL_NO_DTLS1_3))
static int add_key_share(SSL_CONNECTION *s, WPACKET *pkt, unsigned int group_id, size_t loop_num)
{
    unsigned char *encoded_pubkey = NULL;
    EVP_PKEY *key_share_key = NULL;
    size_t encodedlen;

    if (loop_num < s->s3.tmp.num_ks_pkey) {
        if (!ossl_assert(s->hello_retry_request == SSL_HRR_PENDING
                || s->d1->hello_verify_request == SSL_HVR_RECEIVED)
            || !ossl_assert(s->s3.tmp.ks_pkey[loop_num] != NULL)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        /*
         * Could happen if we got a HRR that wasn't requesting a new key_share
         * or if we got a HelloVerifyRequest
         */
        key_share_key = s->s3.tmp.ks_pkey[loop_num];
    } else {
        key_share_key = ssl_generate_pkey_group(s, group_id);
        if (key_share_key == NULL) {
            /* SSLfatal() already called */
            return 0;
        }
    }

    /* Encode the public key. */
    encodedlen = EVP_PKEY_get1_encoded_public_key(key_share_key,
        &encoded_pubkey);
    if (encodedlen == 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EC_LIB);
        goto err;
    }

    /* Create KeyShareEntry */
    if (!WPACKET_put_bytes_u16(pkt, group_id)
        || !WPACKET_sub_memcpy_u16(pkt, encoded_pubkey, encodedlen)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    /* For backward compatibility, we use the first valid group to add a key share */
    if (loop_num == 0) {
        s->s3.tmp.pkey = key_share_key;
        s->s3.group_id = group_id;
    }
    /* We ensure in t1_lib.c that the loop number does not exceed OPENSSL_CLIENT_MAX_KEY_SHARES */
    s->s3.tmp.ks_pkey[loop_num] = key_share_key;
    s->s3.tmp.ks_group_id[loop_num] = group_id;
    if (loop_num >= s->s3.tmp.num_ks_pkey)
        s->s3.tmp.num_ks_pkey++;

    OPENSSL_free(encoded_pubkey);
    return 1;
err:
    if (key_share_key != s->s3.tmp.ks_pkey[loop_num])
        EVP_PKEY_free(key_share_key);
    OPENSSL_free(encoded_pubkey);
    return 0;
}
#endif

EXT_RETURN tls_construct_ctos_key_share(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
#if !(defined(OPENSSL_NO_TLS1_3) && defined(OPENSSL_NO_DTLS1_3))
    size_t i, num_groups = 0;
    const uint16_t *pgroups = NULL;
    uint16_t group_id = 0;
    int add_only_one = 0;
    size_t valid_keyshare = 0;

#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    /* key_share extension */
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_key_share)
        /* Extension data sub-packet */
        || !WPACKET_start_sub_packet_u16(pkt)
        /* KeyShare list sub-packet */
        || !WPACKET_start_sub_packet_u16(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    /*
     * RFC 8701: prepend a GREASE key share entry (1 byte of 0x00). After an
     * HRR requesting a new key share, RFC 9846 requires the requested entry
     * to be the only one in the second ClientHello.
     */
    if ((s->options & SSL_OP_GREASE) && !s->server
        && !(s->hello_retry_request == SSL_HRR_PENDING
            && s->s3.group_id != 0 && s->s3.tmp.pkey == NULL)) {
        uint16_t grease_group = ossl_grease_value(s, OSSL_GREASE_GROUP);

        if (!WPACKET_put_bytes_u16(pkt, grease_group)
            || !WPACKET_start_sub_packet_u16(pkt)
            || !WPACKET_put_bytes_u8(pkt, 0)
            || !WPACKET_close(pkt)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }

    tls1_get_requested_keyshare_groups(s, &pgroups, &num_groups);
    if (num_groups == 1 && pgroups[0] == 0) { /* Indication that no * prefix was used */
        tls1_get_supported_groups(s, &pgroups, &num_groups);
        add_only_one = 1;
    }

    /* If neither the default nor the keyshares have any entry --> fatal */
    if (num_groups == 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_NO_SUITABLE_KEY_SHARE);
        return EXT_RETURN_FAIL;
    }

    /* Add key shares */

    if (s->s3.group_id != 0 && s->s3.tmp.pkey == NULL) {
        /* new, single key share */
        group_id = s->s3.group_id;
        s->s3.tmp.num_ks_pkey = 0;
        if (!add_key_share(s, pkt, group_id, 0)) {
            /* SSLfatal() already called */
            return EXT_RETURN_FAIL;
        }
        valid_keyshare++;
    } else {
        if (s->ext.supportedgroups == NULL) /* use default */
            add_only_one = 1;

        for (i = 0; i < num_groups; i++) {
            const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION
                                                             : TLS1_3_VERSION;

            if (!tls_group_allowed(s, pgroups[i], SSL_SECOP_CURVE_SUPPORTED))
                continue;

            if (!tls_valid_group(s, pgroups[i], version1_3, version1_3,
                    NULL, NULL))
                continue;

            group_id = pgroups[i];

            if (group_id == 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_NO_SUITABLE_KEY_SHARE);
                return EXT_RETURN_FAIL;
            }
            if (!add_key_share(s, pkt, group_id, valid_keyshare)) {
                /* SSLfatal() already called */
                return EXT_RETURN_FAIL;
            }
            valid_keyshare++;
            if (add_only_one)
                break;
        }
    }

    if (valid_keyshare == 0) {
        /* No key shares were allowed */
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_NO_SUITABLE_KEY_SHARE);
        return EXT_RETURN_FAIL;
    }

#ifndef OPENSSL_NO_ECH
    /* stash inner key shares */
    if (s->ext.ech.ch_depth == 1 && ossl_ech_stash_keyshares(s) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
#endif

    if (!WPACKET_close(pkt) || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    return EXT_RETURN_SENT;
#else
    return EXT_RETURN_NOT_SENT;
#endif
}

EXT_RETURN tls_construct_ctos_cookie(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    EXT_RETURN ret = EXT_RETURN_FAIL;

    /* Should only be set if we've had an HRR */
    if (s->ext.tls13_cookie_len == 0)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_cookie)
        /* Extension data sub-packet */
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_sub_memcpy_u16(pkt, s->ext.tls13_cookie,
            s->ext.tls13_cookie_len)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto end;
    }

    ret = EXT_RETURN_SENT;
end:
    OPENSSL_free(s->ext.tls13_cookie);
    s->ext.tls13_cookie = NULL;
    s->ext.tls13_cookie_len = 0;

    return ret;
}

static int tls13_check_tick_lifetime_hint(SSL_CONNECTION *s)
{
    OSSL_TIME t;
    uint32_t agesec;

    if (s->ext.tick_age_checked)
        return s->ext.tick_age_ok;
    s->ext.tick_age_ok = 1;

    /*
     * Technically the C standard just says time() returns a time_t and says
     * nothing about the encoding of that type. In practice most
     * implementations follow POSIX which holds it as an integral type in
     * seconds since epoch. We've already made the assumption that we can do
     * this in multiple places in the code, so portability shouldn't be an
     * issue.
     */
    t = ossl_time_subtract(ossl_time_now(), s->session->time);
    agesec = (uint32_t)ossl_time2seconds(t);

    /*
     * We calculate the age in seconds but the server may work in ms. Due to
     * rounding errors we could overestimate the age by up to 1s. It is
     * better to underestimate it. Otherwise, if the RTT is very short, when
     * the server calculates the age reported by the client it could be
     * bigger than the age calculated on the server - which should never
     * happen.
     */
    if (agesec > 0)
        agesec--;

    /*
     * Calculate age in ms. We're just doing it to nearest second. Should be
     * good enough.
     */
    s->ext.tick_age_ms = agesec * (uint32_t)1000;

    /*
     * Ticket is too old. Ignore it. Overflow. Shouldn't happen unless this is a
     * *really* old session. If so we just ignore it.
     */
    if (s->session->ext.tick_lifetime_hint < agesec)
        s->ext.tick_age_ok = 0;
    else if (agesec != 0 && s->ext.tick_age_ms / (uint32_t)1000 != agesec)
        s->ext.tick_age_ok = 0;

    s->ext.tick_age_checked = 1;
    return s->ext.tick_age_ok;
}

/*
 * Mirrors the ticket-resumption gating checks in tls_construct_ctos_psk() so
 * that early_data is only advertised when the resumption PSK will actually
 * be sent.
 */
static int tls13_check_resumption_psk(SSL_CONNECTION *s, const EVP_MD *handmd)
{
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);
    const EVP_MD *mdres;
    const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION : TLS1_3_VERSION;

    if (s->session == NULL
        || s->session->ssl_version != version1_3
        || s->session->ext.ticklen == 0
        || s->session->cipher == NULL)
        return 0;

    mdres = ssl_md(sctx, s->session->cipher->algorithm2);
    if (mdres == NULL)
        return 0;
    if (s->hello_retry_request == SSL_HRR_PENDING && mdres != handmd)
        return 0;
    if (tls13_check_tick_lifetime_hint(s) == 0)
        return 0;

    return 1;
}

EXT_RETURN tls_construct_ctos_early_data(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
#ifndef OPENSSL_NO_PSK
    char identity[PSK_MAX_IDENTITY_LEN + 1];
#endif /* OPENSSL_NO_PSK */
    const unsigned char *id = NULL;
    size_t idlen = 0;
    SSL_SESSION *psksess = NULL;
    SSL_SESSION *edsess = NULL;
    const EVP_MD *handmd = NULL;
    SSL *ussl = SSL_CONNECTION_GET_USER_SSL(s);
    const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION : TLS1_3_VERSION;

    s->ext.tick_age_checked = 0;

#ifndef OPENSSL_NO_ECH
    /*
     * If we're attempting ECH and processing the outer CH
     * then we only need to check if the extension is to be
     * sent or not - any other processing (with side effects)
     * happened already for the inner CH.
     */
    if (s->ext.ech.es != NULL && s->ext.ech.ch_depth == 0) {
        /*
         * if we called this for inner and did send then
         * the following two things should be set, if so,
         * then send again in the outer CH.
         */
        if (s->ext.early_data == SSL_EARLY_DATA_REJECTED
            && s->ext.early_data_ok == 1) {
            if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_early_data)
                || !WPACKET_start_sub_packet_u16(pkt)
                || !WPACKET_close(pkt)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return EXT_RETURN_FAIL;
            }
            return EXT_RETURN_SENT;
        } else {
            return EXT_RETURN_NOT_SENT;
        }
    }
#endif
    if (s->hello_retry_request == SSL_HRR_PENDING)
        handmd = ssl_handshake_md(s);

    if (s->psk_use_session_cb != NULL
        && (!s->psk_use_session_cb(ussl, handmd, &id, &idlen, &psksess)
            || (psksess != NULL && psksess->ssl_version != version1_3))) {
        SSL_SESSION_free(psksess);
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_PSK);
        return EXT_RETURN_FAIL;
    }

#ifndef OPENSSL_NO_PSK
    if (psksess == NULL && s->psk_client_callback != NULL) {
        unsigned char psk[PSK_MAX_PSK_LEN];
        size_t psklen = 0;

        memset(identity, 0, sizeof(identity));
        psklen = s->psk_client_callback(ussl, NULL,
            identity, sizeof(identity) - 1,
            psk, sizeof(psk));

        if (psklen > PSK_MAX_PSK_LEN) {
            SSLfatal(s, SSL_AD_HANDSHAKE_FAILURE, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        } else if (psklen > 0) {
            const unsigned char tls13_aes128gcmsha256_id[] = { 0x13, 0x01 };
            const SSL_CIPHER *cipher;

            idlen = strlen(identity);
            if (idlen > PSK_MAX_IDENTITY_LEN) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return EXT_RETURN_FAIL;
            }
            id = (unsigned char *)identity;

            /*
             * We found a PSK using an old style callback. We don't know
             * the digest so we default to SHA256 as per the (D)TLSv1.3 spec
             */
            cipher = SSL_CIPHER_find(SSL_CONNECTION_GET_SSL(s),
                tls13_aes128gcmsha256_id);
            if (cipher == NULL) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return EXT_RETURN_FAIL;
            }

            psksess = SSL_SESSION_new();

            if (psksess == NULL
                || !SSL_SESSION_set1_master_key(psksess, psk, psklen)
                || !SSL_SESSION_set_cipher(psksess, cipher)
                || !SSL_SESSION_set_protocol_version(psksess, version1_3)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                OPENSSL_cleanse(psk, psklen);
                return EXT_RETURN_FAIL;
            }
            OPENSSL_cleanse(psk, psklen);
        }
    }
#endif /* OPENSSL_NO_PSK */

    SSL_SESSION_free(s->psksession);
    s->psksession = psksess;
    if (psksess != NULL) {
        OPENSSL_free(s->psksession_id);
        s->psksession_id = OPENSSL_memdup(id, idlen);
        if (s->psksession_id == NULL) {
            s->psksession_id_len = 0;
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        s->psksession_id_len = idlen;
    }

    /*
     * Suppress early_data unless a PSK is available and will be sent.
     *
     * RFC 9846 4.3.10: When a PSK is used and early data is allowed for that
     * PSK, the client can send Application Data in its first flight of
     * messages. If the client opts to do so, it MUST supply both the
     * "pre_shared_key" and "early_data" extensions.
     *
     * The PSK used to encrypt the early data MUST be the first PSK listed in
     * the client's "pre_shared_key" extension.
     */
    /*
     * Slot 0 -- the first identity we will offer -- is the only one that can
     * key 0-RTT. It is the resumption session when we are offering it, else
     * the external psksession. Offer early_data only when that slot-0 PSK is
     * itself 0-RTT-capable; never key it off a PSK in a later slot.
     */
    edsess = tls13_check_resumption_psk(s, handmd) ? s->session : psksess;
    if (s->early_data_state != SSL_EARLY_DATA_CONNECTING
        || edsess == NULL
        || edsess->ext.max_early_data == 0) {
        s->max_early_data = 0;
        if (s->early_data_state == SSL_EARLY_DATA_CONNECTING) {
            s->ext.early_data_suppressed = 1;
            s->ext.early_data = SSL_EARLY_DATA_REJECTED;
            /*
             * We report REJECTED (not NOT_SENT), so
             * SSL_export_keying_material_early() stays callable as it is for a
             * server-rejected attempt -- but no early exporter secret was
             * derived here. Randomise it so any such export yields a harmless
             * per-connection orphan, not an all-zero (predictable) or stale
             * (prior-handshake) value.
             */
            if (RAND_bytes_ex(SSL_CONNECTION_GET_CTX(s)->libctx,
                    s->early_exporter_master_secret,
                    sizeof(s->early_exporter_master_secret), 0)
                <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return EXT_RETURN_FAIL;
            }
        }
        s->early_data_state = SSL_EARLY_DATA_NONE;
        return EXT_RETURN_NOT_SENT;
    }
    s->max_early_data = edsess->ext.max_early_data;
    /*
     * Freeze slot 0 (candidate_at(0)) so the binder, the early-key derivation,
     * the early exporter, the byte-budget lookup and the post-ServerHello fixup
     * all key off the actual first-offered PSK rather than guessing the source
     * from s->session->ext.max_early_data. Held (up-ref'd) so it stays valid
     * across the swap that later folds a selected psksession into s->session.
     */
    SSL_SESSION_free(s->ext.early_data_session);
    s->ext.early_data_session = edsess;
    if (!SSL_SESSION_up_ref(edsess)) {
        s->ext.early_data_session = NULL;
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    if (edsess->ext.hostname != NULL) {
        if (s->ext.hostname == NULL
            || (s->ext.hostname != NULL
                && strcmp(s->ext.hostname, edsess->ext.hostname) != 0)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR,
                SSL_R_INCONSISTENT_EARLY_DATA_SNI);
            return EXT_RETURN_FAIL;
        }
    }

    if ((s->ext.alpn == NULL && edsess->ext.alpn_selected != NULL)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_INCONSISTENT_EARLY_DATA_ALPN);
        return EXT_RETURN_FAIL;
    }

    /*
     * Verify that we are offering an ALPN protocol consistent with the early
     * data.
     */
    if (edsess->ext.alpn_selected != NULL) {
        PACKET prots, alpnpkt;
        int found = 0;

        if (!PACKET_buf_init(&prots, s->ext.alpn, s->ext.alpn_len)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        while (PACKET_get_length_prefixed_1(&prots, &alpnpkt)) {
            if (PACKET_equal(&alpnpkt, edsess->ext.alpn_selected,
                    edsess->ext.alpn_selected_len)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR,
                SSL_R_INCONSISTENT_EARLY_DATA_ALPN);
            return EXT_RETURN_FAIL;
        }
    }

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_early_data)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    /*
     * We set this to rejected here. Later, if the server acknowledges the
     * extension, we set it to accepted.
     */
    s->ext.early_data = SSL_EARLY_DATA_REJECTED;
    s->ext.early_data_ok = 1;

    return EXT_RETURN_SENT;
}

/*
 * Construct the pre_shared_key extension
 */
EXT_RETURN tls_construct_ctos_psk(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
#if !(defined(OPENSSL_NO_TLS1_3) && defined(OPENSSL_NO_DTLS1_3))
    uint32_t agems = 0;
    size_t binderoffset, msglen;
    int reshashsize = 0, pskhashsize = 0;
    unsigned char *resbinder = NULL, *pskbinder = NULL, *msgstart = NULL;
    const EVP_MD *handmd = NULL, *mdres = NULL, *mdpsk = NULL;
    int dores = 0;
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);
    const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION : TLS1_3_VERSION;

    s->ext.tick_identity = 0;

    /*
     * If this is an incompatible or new session then we have nothing to resume
     * so don't add this extension.
     */
    if (s->session->ssl_version != version1_3
        || (s->session->ext.ticklen == 0 && s->psksession == NULL))
        return EXT_RETURN_NOT_SENT;

    if (s->hello_retry_request == SSL_HRR_PENDING)
        handmd = ssl_handshake_md(s);

    if (s->session->ext.ticklen != 0) {
        /* Get the digest associated with the ciphersuite in the session */
        if (s->session->cipher == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        mdres = ssl_md(sctx, s->session->cipher->algorithm2);
        if (mdres == NULL) {
            /*
             * Don't recognize this cipher so we can't use the session.
             * Ignore it
             */
            goto dopsksess;
        }

        if (s->hello_retry_request == SSL_HRR_PENDING && mdres != handmd) {
            /*
             * Selected ciphersuite hash does not match the hash for the session
             * so we can't use it.
             */
            goto dopsksess;
        }

#ifndef OPENSSL_NO_ECH
        /*
         * When doing ECH, we get here twice (for inner then outer). The
         * 2nd time (for outer) we can skip some checks as we know how
         * those went last time.
         */
        if (s->ext.ech.es != NULL && s->ext.ech.ch_depth == 0) {
            s->ext.tick_identity = s->ext.ech.tick_identity;
            dores = (s->ext.tick_identity > 0);
            goto dopsksess;
        }
#endif

        if (tls13_check_tick_lifetime_hint(s) == 0)
            goto dopsksess;
        /* tls13_check_tick_lifetime_hint() updates the tick_age_ms value. */
        agems = s->ext.tick_age_ms;

        /*
         * Obfuscate the age. Overflow here is fine, this addition is supposed
         * to be mod 2^32.
         */
        agems += s->session->ext.tick_age_add;

        reshashsize = EVP_MD_get_size(mdres);
        if (reshashsize <= 0)
            goto dopsksess;
        s->ext.tick_identity++;
#ifndef OPENSSL_NO_ECH
        /* stash this for re-use in outer CH */
        if (s->ext.ech.es != NULL && s->ext.ech.ch_depth == 1)
            s->ext.ech.tick_identity = s->ext.tick_identity;
#endif
        dores = 1;
    }

dopsksess:
    if (!dores && s->psksession == NULL)
        return EXT_RETURN_NOT_SENT;

    if (s->psksession != NULL) {
        mdpsk = ssl_md(sctx, s->psksession->cipher->algorithm2);
        if (mdpsk == NULL) {
            /*
             * Don't recognize this cipher so we can't use the session.
             * If this happens it's an application bug.
             */
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_PSK);
            return EXT_RETURN_FAIL;
        }

        if (s->hello_retry_request == SSL_HRR_PENDING && mdpsk != handmd) {
            /*
             * Selected ciphersuite hash does not match the hash for the PSK
             * session. This is an application bug.
             */
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_PSK);
            return EXT_RETURN_FAIL;
        }

        pskhashsize = EVP_MD_get_size(mdpsk);
        if (pskhashsize <= 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_PSK);
            return EXT_RETURN_FAIL;
        }
    }

    /* Create the extension, but skip over the binder for now */
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_psk)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_start_sub_packet_u16(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

#ifndef OPENSSL_NO_ECH
    /*
     * For ECH if we're processing the outer CH and the inner CH
     * has a PSK, then we want to send a GREASE PSK in the outer.
     * We'll do that by just replacing the ticket value itself
     * with random values of the same length.
     */
    if (s->ext.ech.es != NULL && s->ext.ech.ch_depth == 0) {
        unsigned char *rndbuf = NULL, *rndbufp = NULL;
        size_t totalrndsize = 0;

        totalrndsize = s->session->ext.ticklen
            + sizeof(agems)
            + s->psksession_id_len
            + reshashsize
            + pskhashsize;
        rndbuf = OPENSSL_malloc(totalrndsize);
        if (rndbuf == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        /* for outer CH allocate a similar sized random value */
        if (RAND_bytes_ex(sctx->libctx, rndbuf, totalrndsize, 0) <= 0) {
            OPENSSL_free(rndbuf);
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        /* set agems from random buffer */
        rndbufp = rndbuf;
        memcpy(&agems, rndbufp, sizeof(agems));
        rndbufp += sizeof(agems);
        if (dores != 0) {
            if (!WPACKET_sub_memcpy_u16(pkt, rndbufp,
                    s->session->ext.ticklen)
                || !WPACKET_put_bytes_u32(pkt, agems)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                OPENSSL_free(rndbuf);
                return EXT_RETURN_FAIL;
            }
            rndbufp += s->session->ext.ticklen;
        }
        if (s->psksession != NULL) {
            if (!WPACKET_sub_memcpy_u16(pkt, rndbufp, s->psksession_id_len)
                || !WPACKET_put_bytes_u32(pkt, 0)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                OPENSSL_free(rndbuf);
                return EXT_RETURN_FAIL;
            }
            rndbufp += s->psksession_id_len;
        }
        if (!WPACKET_close(pkt)
            || !WPACKET_start_sub_packet_u16(pkt)
            || (dores == 1
                && !WPACKET_sub_memcpy_u8(pkt, rndbufp, reshashsize))
            || (s->psksession != NULL
                && !WPACKET_sub_memcpy_u8(pkt, rndbufp, pskhashsize))
            || !WPACKET_close(pkt)
            || !WPACKET_close(pkt)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            OPENSSL_free(rndbuf);
            return EXT_RETURN_FAIL;
        }
        OPENSSL_free(rndbuf);
        return EXT_RETURN_SENT;
    }
#endif /* OPENSSL_NO_ECH */
    if (dores) {
        if (!WPACKET_sub_memcpy_u16(pkt, s->session->ext.tick,
                s->session->ext.ticklen)
            || !WPACKET_put_bytes_u32(pkt, agems)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
    }

    if (s->psksession != NULL) {
        if (!WPACKET_sub_memcpy_u16(pkt, s->psksession_id,
                s->psksession_id_len)
            || !WPACKET_put_bytes_u32(pkt, 0)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        s->ext.tick_identity++;
    }

    if (!WPACKET_close(pkt)
        || !WPACKET_get_total_written(pkt, &binderoffset)
        || !WPACKET_start_sub_packet_u16(pkt)
        || (dores
            && !WPACKET_sub_allocate_bytes_u8(pkt, reshashsize, &resbinder))
        || (s->psksession != NULL
            && !WPACKET_sub_allocate_bytes_u8(pkt, pskhashsize, &pskbinder))
        || !WPACKET_close(pkt)
        || !WPACKET_close(pkt)
        || !WPACKET_get_total_written(pkt, &msglen)
        /*
         * We need to fill in all the sub-packet lengths now so we can
         * calculate the HMAC of the message up to the binders
         */
        || !WPACKET_fill_lengths(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    msgstart = WPACKET_get_curr(pkt) - msglen;

    if (dores && tls_psk_do_binder(s, mdres, msgstart, binderoffset, NULL, resbinder, s->session, 1, 0) != 1) {
        /* SSLfatal() already called */
        return EXT_RETURN_FAIL;
    }

    if (s->psksession != NULL
        && tls_psk_do_binder(s, mdpsk, msgstart, binderoffset, NULL,
               pskbinder, s->psksession, 1, 1)
            != 1) {
        /* SSLfatal() already called */
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
#else
    return EXT_RETURN_NOT_SENT;
#endif
}

EXT_RETURN tls_construct_ctos_post_handshake_auth(SSL_CONNECTION *s, WPACKET *pkt,
    ossl_unused unsigned int context,
    ossl_unused X509 *x,
    ossl_unused size_t chainidx)
{
#if !(defined(OPENSSL_NO_TLS1_3) && defined(OPENSSL_NO_DTLS1_3))
    if (!s->pha_enabled)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(s, context, pkt)
#endif

    /* construct extension - 0 length, no contents */
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_post_handshake_auth)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    s->post_handshake_auth = SSL_PHA_EXT_SENT;

    return EXT_RETURN_SENT;
#else
    return EXT_RETURN_NOT_SENT;
#endif
}

/*
 * Parse the server's renegotiation binding and abort if it's not right
 */
int tls_parse_stoc_renegotiate(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    size_t expected_len = s->s3.previous_client_finished_len
        + s->s3.previous_server_finished_len;
    size_t ilen;
    const unsigned char *data;

    /* Check for logic errors */
    if (!ossl_assert(expected_len == 0
            || s->s3.previous_client_finished_len != 0)
        || !ossl_assert(expected_len == 0
            || s->s3.previous_server_finished_len != 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    /* Parse the length byte */
    if (!PACKET_get_1_len(pkt, &ilen)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_RENEGOTIATION_ENCODING_ERR);
        return 0;
    }

    /* Consistency check */
    if (PACKET_remaining(pkt) != ilen) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_RENEGOTIATION_ENCODING_ERR);
        return 0;
    }

    /* Check that the extension matches */
    if (ilen != expected_len) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_RENEGOTIATION_MISMATCH);
        return 0;
    }

    if (!PACKET_get_bytes(pkt, &data, s->s3.previous_client_finished_len)
        || memcmp(data, s->s3.previous_client_finished,
               s->s3.previous_client_finished_len)
            != 0) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_RENEGOTIATION_MISMATCH);
        return 0;
    }

    if (!PACKET_get_bytes(pkt, &data, s->s3.previous_server_finished_len)
        || memcmp(data, s->s3.previous_server_finished,
               s->s3.previous_server_finished_len)
            != 0) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_RENEGOTIATION_MISMATCH);
        return 0;
    }
    s->s3.send_connection_binding = 1;

    return 1;
}

/* Parse the server's max fragment len extension packet */
int tls_parse_stoc_maxfragmentlen(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    unsigned int value;

    if (PACKET_remaining(pkt) != 1 || !PACKET_get_1(pkt, &value)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }

    /* |value| should contains a valid max-fragment-length code. */
    if (!IS_MAX_FRAGMENT_LENGTH_EXT_VALID(value)) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER,
            SSL_R_TLS_EXT_INVALID_MAX_FRAGMENT_LENGTH);
        return 0;
    }

    /* Must be the same value as client-configured one who was sent to server */
    /*-
     * RFC 6066: if a client receives a maximum fragment length negotiation
     * response that differs from the length it requested, ...
     * It must abort with SSL_AD_ILLEGAL_PARAMETER alert
     */
    if (value != s->ext.max_fragment_len_mode) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER,
            SSL_R_TLS_EXT_INVALID_MAX_FRAGMENT_LENGTH);
        return 0;
    }

    /*
     * Maximum Fragment Length Negotiation succeeded.
     * The negotiated Maximum Fragment Length is binding now.
     */
    s->session->ext.max_fragment_len_mode = value;

    return 1;
}

int tls_parse_stoc_server_name(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    char *eff_sni = s->ext.hostname;

#ifndef OPENSSL_NO_ECH
    /* if we tried ECH and failed, the outer is what's expected */
    if (s->ext.ech.es != NULL && s->ext.ech.success == 0)
        eff_sni = s->ext.ech.outer_hostname;
#endif
    if (eff_sni == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if (PACKET_remaining(pkt) > 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    if (!s->hit) {
        if (s->session->ext.hostname != NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        s->session->ext.hostname = OPENSSL_strdup(eff_sni);
        if (s->session->ext.hostname == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }
    return 1;
}

int tls_parse_stoc_session_ticket(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    SSL *ssl = SSL_CONNECTION_GET_USER_SSL(s);

    if (s->ext.session_ticket_cb != NULL && !s->ext.session_ticket_cb(ssl, PACKET_data(pkt), (int)PACKET_remaining(pkt), s->ext.session_ticket_cb_arg)) {
        SSLfatal(s, SSL_AD_HANDSHAKE_FAILURE, SSL_R_BAD_EXTENSION);
        return 0;
    }

    if (!tls_use_ticket(s)) {
        SSLfatal(s, SSL_AD_UNSUPPORTED_EXTENSION, SSL_R_BAD_EXTENSION);
        return 0;
    }
    if (PACKET_remaining(pkt) > 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }

    s->ext.ticket_expected = 1;

    return 1;
}

#ifndef OPENSSL_NO_OCSP
int tls_parse_stoc_status_request(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    if (context == SSL_EXT_TLS1_3_CERTIFICATE_REQUEST) {
        /* We ignore this if the server sends a CertificateRequest */
        return 1;
    }

    /*
     * MUST only be sent if we've requested a status
     * request message. In (D)TLS <= 1.2 it must also be empty.
     */
    if (s->ext.status_type != TLSEXT_STATUSTYPE_ocsp) {
        SSLfatal(s, SSL_AD_UNSUPPORTED_EXTENSION, SSL_R_BAD_EXTENSION);
        return 0;
    }
    if (!SSL_CONNECTION_IS_VERSION13(s) && PACKET_remaining(pkt) > 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }

    if (SSL_CONNECTION_IS_VERSION13(s)) {
        /* SSLfatal() already called */
        return tls_process_cert_status_body(s, chainidx, pkt);
    }

    /* Set flag to expect CertificateStatus message */
    s->ext.status_expected = 1;

    return 1;
}
#endif

#ifndef OPENSSL_NO_CT
int tls_parse_stoc_sct(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
    X509 *x, size_t chainidx)
{
    if (context == SSL_EXT_TLS1_3_CERTIFICATE_REQUEST) {
        /* We ignore this if the server sends it in a CertificateRequest */
        return 1;
    }

    /*
     * Only take it if we asked for it - i.e if there is no CT validation
     * callback set, then a custom extension MAY be processing it, so we
     * need to let control continue to flow to that.
     */
    if (s->ct_validation_callback != NULL) {
        size_t size = PACKET_remaining(pkt);

        /* Simply copy it off for later processing */
        OPENSSL_free(s->ext.scts);
        s->ext.scts = NULL;

        s->ext.scts_len = (uint16_t)size;
        if (size > 0) {
            s->ext.scts = OPENSSL_malloc(size);
            if (s->ext.scts == NULL) {
                s->ext.scts_len = 0;
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_CRYPTO_LIB);
                return 0;
            }
            if (!PACKET_copy_bytes(pkt, s->ext.scts, size)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
    } else {
        ENDPOINT role = (context & SSL_EXT_TLS1_2_SERVER_HELLO) != 0
            ? ENDPOINT_CLIENT
            : ENDPOINT_BOTH;

        /*
         * If we didn't ask for it then there must be a custom extension,
         * otherwise this is unsolicited.
         */
        if (custom_ext_find(&s->cert->custext, role,
                TLSEXT_TYPE_signed_certificate_timestamp,
                NULL)
            == NULL) {
            SSLfatal(s, TLS1_AD_UNSUPPORTED_EXTENSION, SSL_R_BAD_EXTENSION);
            return 0;
        }

        if (!custom_ext_parse(s, context,
                TLSEXT_TYPE_signed_certificate_timestamp,
                PACKET_data(pkt), PACKET_remaining(pkt),
                x, chainidx)) {
            /* SSLfatal already called */
            return 0;
        }
    }

    return 1;
}
#endif

#ifndef OPENSSL_NO_NEXTPROTONEG
/*
 * ssl_next_proto_validate validates a Next Protocol Negotiation block. No
 * elements of zero length are allowed and the set of elements must exactly
 * fill the length of the block. Returns 1 on success or 0 on failure.
 */
static int ssl_next_proto_validate(SSL_CONNECTION *s, PACKET *pkt)
{
    PACKET tmp_protocol;

    while (PACKET_remaining(pkt)) {
        if (!PACKET_get_length_prefixed_1(pkt, &tmp_protocol)
            || PACKET_remaining(&tmp_protocol) == 0) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            return 0;
        }
    }

    return 1;
}

int tls_parse_stoc_npn(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
    X509 *x, size_t chainidx)
{
    unsigned char *selected;
    unsigned char selected_len;
    PACKET tmppkt;
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);

    /* Check if we are in a renegotiation. If so ignore this extension */
    if (!SSL_IS_FIRST_HANDSHAKE(s))
        return 1;

    /* We must have requested it. */
    if (sctx->ext.npn_select_cb == NULL) {
        SSLfatal(s, SSL_AD_UNSUPPORTED_EXTENSION, SSL_R_BAD_EXTENSION);
        return 0;
    }

    /* The data must be valid */
    tmppkt = *pkt;
    if (!ssl_next_proto_validate(s, &tmppkt)) {
        /* SSLfatal() already called */
        return 0;
    }
    if (sctx->ext.npn_select_cb(SSL_CONNECTION_GET_USER_SSL(s),
            &selected, &selected_len,
            PACKET_data(pkt), (unsigned int)PACKET_remaining(pkt),
            sctx->ext.npn_select_cb_arg)
            != SSL_TLSEXT_ERR_OK
        || selected_len == 0) {
        SSLfatal(s, SSL_AD_HANDSHAKE_FAILURE, SSL_R_BAD_EXTENSION);
        return 0;
    }

    /*
     * Could be non-NULL if server has sent multiple NPN extensions in
     * a single Serverhello
     */
    OPENSSL_free(s->ext.npn);
    s->ext.npn = OPENSSL_malloc(selected_len);
    if (s->ext.npn == NULL) {
        s->ext.npn_len = 0;
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    memcpy(s->ext.npn, selected, selected_len);
    s->ext.npn_len = selected_len;
    s->s3.npn_seen = 1;

    return 1;
}
#endif

int tls_parse_stoc_alpn(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
    X509 *x, size_t chainidx)
{
    size_t len;
    PACKET confpkt, protpkt;
    int valid = 0;

    /* We must have requested it. */
    if (!s->s3.alpn_sent) {
        SSLfatal(s, SSL_AD_UNSUPPORTED_EXTENSION, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /*-
     * The extension data consists of:
     *   uint16 list_length
     *   uint8 proto_length;
     *   uint8 proto[proto_length];
     */
    if (!PACKET_get_net_2_len(pkt, &len)
        || PACKET_remaining(pkt) != len || !PACKET_get_1_len(pkt, &len)
        || PACKET_remaining(pkt) != len) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }

    /* It must be a protocol that we sent */
    if (!PACKET_buf_init(&confpkt, s->ext.alpn, s->ext.alpn_len)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    while (PACKET_get_length_prefixed_1(&confpkt, &protpkt)) {
        if (PACKET_remaining(&protpkt) != len)
            continue;
        if (memcmp(PACKET_data(pkt), PACKET_data(&protpkt), len) == 0) {
            /* Valid protocol found */
            valid = 1;
            break;
        }
    }

    if (!valid) {
        /* The protocol sent from the server does not match one we advertised */
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }

    OPENSSL_free(s->s3.alpn_selected);
    s->s3.alpn_selected = OPENSSL_malloc(len);
    if (s->s3.alpn_selected == NULL) {
        s->s3.alpn_selected_len = 0;
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if (!PACKET_copy_bytes(pkt, s->s3.alpn_selected, len)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    s->s3.alpn_selected_len = len;

    if (s->session->ext.alpn_selected == NULL
        || s->session->ext.alpn_selected_len != len
        || memcmp(s->session->ext.alpn_selected, s->s3.alpn_selected, len)
            != 0) {
        /* ALPN not consistent with the old session so cannot use early_data */
        s->ext.early_data_ok = 0;
    }
    if (!s->hit) {
        /*
         * This is a new session and so alpn_selected should have been
         * initialised to NULL. We should update it with the selected ALPN.
         */
        if (!ossl_assert(s->session->ext.alpn_selected == NULL)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        s->session->ext.alpn_selected = OPENSSL_memdup(s->s3.alpn_selected, s->s3.alpn_selected_len);
        if (s->session->ext.alpn_selected == NULL) {
            s->session->ext.alpn_selected_len = 0;
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        s->session->ext.alpn_selected_len = s->s3.alpn_selected_len;
    }

    return 1;
}

#ifndef OPENSSL_NO_SRTP
int tls_parse_stoc_use_srtp(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context, X509 *x, size_t chainidx)
{
    unsigned int id, ct, mki;
    int i;
    STACK_OF(SRTP_PROTECTION_PROFILE) *clnt;
    SRTP_PROTECTION_PROFILE *prof;

    if (!PACKET_get_net_2(pkt, &ct) || ct != 2
        || !PACKET_get_net_2(pkt, &id)
        || !PACKET_get_1(pkt, &mki)
        || PACKET_remaining(pkt) != 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR,
            SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
        return 0;
    }

    if (mki != 0) {
        /* Must be no MKI, since we never offer one */
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_SRTP_MKI_VALUE);
        return 0;
    }

    /* Throw an error if the server gave us an unsolicited extension */
    clnt = SSL_get_srtp_profiles(SSL_CONNECTION_GET_SSL(s));
    if (clnt == NULL) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_NO_SRTP_PROFILES);
        return 0;
    }

    /*
     * Check to see if the server gave us something we support (and
     * presumably offered)
     */
    for (i = 0; i < sk_SRTP_PROTECTION_PROFILE_num(clnt); i++) {
        prof = sk_SRTP_PROTECTION_PROFILE_value(clnt, i);

        if (prof->id == id) {
            s->srtp_profile = prof;
            return 1;
        }
    }

    SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER,
        SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
    return 0;
}
#endif

int tls_parse_stoc_etm(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
    X509 *x, size_t chainidx)
{
    /* Ignore if inappropriate ciphersuite */
    if (!(s->options & SSL_OP_NO_ENCRYPT_THEN_MAC)
        && s->s3.tmp.new_cipher->algorithm_mac != SSL_AEAD
        && s->s3.tmp.new_cipher->algorithm_enc != SSL_RC4
        && s->s3.tmp.new_cipher->algorithm_enc != SSL_eGOST2814789CNT
        && s->s3.tmp.new_cipher->algorithm_enc != SSL_eGOST2814789CNT12
        && s->s3.tmp.new_cipher->algorithm_enc != SSL_MAGMA
        && s->s3.tmp.new_cipher->algorithm_enc != SSL_KUZNYECHIK)
        s->ext.use_etm = 1;

    return 1;
}

int tls_parse_stoc_ems(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
    X509 *x, size_t chainidx)
{
    if (s->options & SSL_OP_NO_EXTENDED_MASTER_SECRET)
        return 1;
    s->s3.flags |= TLS1_FLAGS_RECEIVED_EXTMS;
    if (!s->hit)
        s->session->flags |= SSL_SESS_FLAG_EXTMS;

    return 1;
}

int tls_parse_stoc_supported_versions(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    unsigned int version;
    const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION : TLS1_3_VERSION;

    if (!PACKET_get_net_2(pkt, &version)
        || PACKET_remaining(pkt) != 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    /*
     * The only protocol version we support which is valid in this extension in
     * a ServerHello is (D)TLSv1.3 therefore we shouldn't be getting anything else.
     */
    if ((int)version != version1_3) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER,
            SSL_R_BAD_PROTOCOL_VERSION_NUMBER);
        return 0;
    }

    /* We ignore this extension for HRRs except to sanity check it */
    if (context == SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST)
        return 1;

    /* We just set it here. We validate it in ssl_choose_client_version */
    s->version = version;
    if (!ssl_set_record_protocol_version(s, version)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    return 1;
}

int tls_parse_stoc_key_share(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
#if !(defined(OPENSSL_NO_TLS1_3) && defined(OPENSSL_NO_DTLS1_3))
    unsigned int group_id;
    PACKET encoded_pt;
    EVP_PKEY *ckey = s->s3.tmp.pkey, *skey = NULL;
    const TLS_GROUP_INFO *ginf = NULL;
    uint16_t valid_ks_id = 0;
    size_t i;

    /* Sanity check */
    if (ckey == NULL || s->s3.peer_tmp != NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    /* Which group ID does the server want -> group_id */
    if (!PACKET_get_net_2(pkt, &group_id)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    /* RFC 8701: reject GREASE values selected by the server */
    if (ossl_is_grease_value(group_id)) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_KEY_SHARE);
        return 0;
    }

    if ((context & SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST) != 0) {
        const uint16_t *pgroups = NULL;
        size_t num_groups;
        const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION
                                                         : TLS1_3_VERSION;

        if (PACKET_remaining(pkt) != 0) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_MISMATCH);
            return 0;
        }

        /*
         * It is an error if the HelloRetryRequest wants a key_share that we
         * already sent in the first ClientHello
         */
        for (i = 0; i < s->s3.tmp.num_ks_pkey; i++) {
            if (s->s3.tmp.ks_group_id[i] == group_id) {
                SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_KEY_SHARE);
                return 0;
            }
        }

        /* Validate the selected group is one we support */
        tls1_get_supported_groups(s, &pgroups, &num_groups);
        for (i = 0; i < num_groups; i++) {
            if (group_id == pgroups[i])
                break;
        }

        if (i >= num_groups
            || !tls_group_allowed(s, group_id, SSL_SECOP_CURVE_SUPPORTED)
            || !tls_valid_group(s, group_id, version1_3, version1_3, NULL, NULL)) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_KEY_SHARE);
            return 0;
        }

        /* Memorize which groupID the server wants */
        s->s3.group_id = group_id;

        /* The initial keyshares are obsolete now, hence free memory */
        for (i = 0; i < s->s3.tmp.num_ks_pkey; i++) {
            if (s->s3.tmp.ks_pkey[i] != NULL) {
                EVP_PKEY_free(s->s3.tmp.ks_pkey[i]);
                s->s3.tmp.ks_pkey[i] = NULL;
            }
        }
        s->s3.tmp.num_ks_pkey = 0;
        s->s3.tmp.pkey = NULL;

        return 1;
    }

    /*
     * check that the group requested by the server is one we've
     * sent a key share for, and if so: memorize which one
     */
    for (i = 0; i < s->s3.tmp.num_ks_pkey; i++) {
        if (s->s3.tmp.ks_group_id[i] == group_id) {
            valid_ks_id = group_id;
            ckey = s->s3.tmp.ks_pkey[i];
            s->s3.group_id = group_id;
            s->s3.tmp.pkey = ckey;
            break;
        }
    }
    if (valid_ks_id == 0) {
        /*
         * This isn't for the group that we sent in the original
         * key_share!
         */
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_KEY_SHARE);
        return 0;
    }
    /* Retain this group in the SSL_SESSION */
    if (!s->hit) {
        s->session->kex_group = group_id;
    } else if (group_id != s->session->kex_group) {
        /*
         * If this is a resumption but changed what group was used, we need
         * to record the new group in the session, but the session is not
         * a new session and could be in use by other threads.  So, make
         * a copy of the session to record the new information so that it's
         * useful for any sessions resumed from tickets issued on this
         * connection.
         */
        SSL_SESSION *new_sess;

        if ((new_sess = ssl_session_dup(s->session, 0)) == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_SSL_LIB);
            return 0;
        }
        SSL_SESSION_free(s->session);
        s->session = new_sess;
        s->session->kex_group = group_id;
    }

    if ((ginf = tls1_group_id_lookup(SSL_CONNECTION_GET_CTX(s),
             group_id))
        == NULL) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_KEY_SHARE);
        return 0;
    }

    if (!PACKET_as_length_prefixed_2(pkt, &encoded_pt)
        || PACKET_remaining(&encoded_pt) == 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    if (!ginf->is_kem) {
        /* Regular KEX */
        skey = EVP_PKEY_new();
        if (skey == NULL || EVP_PKEY_copy_parameters(skey, ckey) <= 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_COPY_PARAMETERS_FAILED);
            EVP_PKEY_free(skey);
            return 0;
        }

        if (tls13_set_encoded_pub_key(skey, PACKET_data(&encoded_pt),
                PACKET_remaining(&encoded_pt))
            <= 0) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_KEY_SHARE);
            EVP_PKEY_free(skey);
            return 0;
        }

        if (ssl_derive(s, ckey, skey, 1) == 0) {
            /* SSLfatal() already called */
            EVP_PKEY_free(skey);
            return 0;
        }
        s->s3.peer_tmp = skey;
    } else {
        /* KEM Mode */
        const unsigned char *ct = PACKET_data(&encoded_pt);
        size_t ctlen = PACKET_remaining(&encoded_pt);

        if (ssl_decapsulate(s, ckey, ct, ctlen, 1) == 0) {
            /* SSLfatal() already called */
            return 0;
        }
    }
    s->s3.did_kex = 1;
#endif

    return 1;
}

int tls_parse_stoc_cookie(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
    X509 *x, size_t chainidx)
{
    PACKET cookie;

    if (!PACKET_as_length_prefixed_2(pkt, &cookie)
        || PACKET_remaining(&cookie) == 0
        || !PACKET_memdup(&cookie, &s->ext.tls13_cookie,
            &s->ext.tls13_cookie_len)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    return 1;
}

int tls_parse_stoc_early_data(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    if (context == SSL_EXT_TLS1_3_NEW_SESSION_TICKET) {
        unsigned long max_early_data;

        if (!PACKET_get_net_4(pkt, &max_early_data)
            || PACKET_remaining(pkt) != 0) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_INVALID_MAX_EARLY_DATA);
            return 0;
        }

        s->session->ext.max_early_data = max_early_data;

        if (SSL_IS_QUIC_HANDSHAKE(s) && max_early_data != 0xffffffff) {
            /*
             * QUIC allows missing max_early_data, or a max_early_data value
             * of 0xffffffff. Missing max_early_data is stored in the session
             * as 0. This is indistinguishable in OpenSSL from a present
             * max_early_data value that was 0. In order that later checks for
             * invalid max_early_data correctly treat as an error the case where
             * max_early_data is present and it is 0, we store any invalid
             * value in the same (non-zero) way. Otherwise we would have to
             * introduce a new flag just for this.
             */
            s->session->ext.max_early_data = 1;
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_INVALID_MAX_EARLY_DATA);
            return 0;
        }

        return 1;
    }

    if (PACKET_remaining(pkt) != 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }

    if (!s->ext.early_data_ok
        || !s->hit) {
        /*
         * If we get here then we didn't send early data, or we didn't resume
         * using the first identity, or the SNI/ALPN is not consistent so the
         * server should not be accepting it.
         */
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        return 0;
    }

    s->ext.early_data = SSL_EARLY_DATA_ACCEPTED;

    return 1;
}

int tls_parse_stoc_psk(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
#if !(defined(OPENSSL_NO_TLS1_3) && defined(OPENSSL_NO_DTLS1_3))
    SSL_SESSION *sesstmp;
    unsigned int identity;

    if (!PACKET_get_net_2(pkt, &identity) || PACKET_remaining(pkt) != 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    if (identity >= (unsigned int)s->ext.tick_identity) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_PSK_IDENTITY);
        return 0;
    }

    /*
     * Session resumption tickets are always sent before PSK tickets. If the
     * ticket index is 0 then it must be for a session resumption ticket if we
     * sent two tickets, or if we didn't send a PSK ticket.
     */
    if (identity == 0 && (s->psksession == NULL || s->ext.tick_identity == 2)) {
        s->hit = 1;
        SSL_SESSION_free(s->psksession);
        s->psksession = NULL;
        return 1;
    }

    if (s->psksession == NULL) {
        /* Should never happen */
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    /*
     * If we used the external PSK for sending early_data then s->early_secret
     * is already set up, so don't overwrite it. Otherwise we copy the
     * early_secret across that we generated earlier.
     */
    if ((s->early_data_state != SSL_EARLY_DATA_WRITE_RETRY
            && s->early_data_state != SSL_EARLY_DATA_FINISHED_WRITING)
        || s->ext.early_data_session != s->psksession)
        memcpy(s->early_secret, s->psksession->early_secret, EVP_MAX_MD_SIZE);

    /*
     * The psk_use_session_cb()/psk_client_callback() may reuse
     * the session across connections we can't mutate it directly.
     */
    if ((sesstmp = ssl_session_dup(s->psksession, 0)) == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    SSL_SESSION_free(s->psksession);
    s->psksession = sesstmp;

    /*
     * s->psksession (now our private copy) was built by the callback, not via
     * ssl_get_new_session(), so it was never stamped with our own sid_ctx. Do
     * so now, to avoid rejection of the PSK session in tls_process_server_hello().
     */
    memcpy(s->psksession->sid_ctx, s->sid_ctx, s->sid_ctx_length);
    s->psksession->sid_ctx_length = s->sid_ctx_length;

    SSL_SESSION_free(s->session);
    s->session = s->psksession;
    s->psksession = NULL;
    s->hit = 1;
    /* Early data is only allowed if we used the first ticket */
    if (identity != 0)
        s->ext.early_data_ok = 0;
#endif

    return 1;
}

EXT_RETURN tls_construct_ctos_client_cert_type(SSL_CONNECTION *sc, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    sc->ext.client_cert_type_ctos = OSSL_CERT_TYPE_CTOS_NONE;
    if (sc->client_cert_type == NULL)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(sc, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_client_cert_type)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_sub_memcpy_u8(pkt, sc->client_cert_type, sc->client_cert_type_len)
        || !WPACKET_close(pkt)) {
        SSLfatal(sc, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    sc->ext.client_cert_type_ctos = OSSL_CERT_TYPE_CTOS_GOOD;
    return EXT_RETURN_SENT;
}

int tls_parse_stoc_client_cert_type(SSL_CONNECTION *sc, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    unsigned int type;

    if (PACKET_remaining(pkt) != 1) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    if (!PACKET_get_1(pkt, &type)) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /* We did not send/ask for this */
    if (!ossl_assert(sc->ext.client_cert_type_ctos == OSSL_CERT_TYPE_CTOS_GOOD)) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /* We don't have this enabled */
    if (sc->client_cert_type == NULL) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /* Given back a value we didn't configure */
    if (memchr(sc->client_cert_type, type, sc->client_cert_type_len) == NULL) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_VALUE);
        return 0;
    }
    sc->ext.client_cert_type = type;
    return 1;
}

EXT_RETURN tls_construct_ctos_server_cert_type(SSL_CONNECTION *sc, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    sc->ext.server_cert_type_ctos = OSSL_CERT_TYPE_CTOS_NONE;
    if (sc->server_cert_type == NULL)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(sc, context, pkt)
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_server_cert_type)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_sub_memcpy_u8(pkt, sc->server_cert_type, sc->server_cert_type_len)
        || !WPACKET_close(pkt)) {
        SSLfatal(sc, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    sc->ext.server_cert_type_ctos = OSSL_CERT_TYPE_CTOS_GOOD;
    return EXT_RETURN_SENT;
}

int tls_parse_stoc_server_cert_type(SSL_CONNECTION *sc, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    unsigned int type;

    if (PACKET_remaining(pkt) != 1) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    if (!PACKET_get_1(pkt, &type)) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /* We did not send/ask for this */
    if (!ossl_assert(sc->ext.server_cert_type_ctos == OSSL_CERT_TYPE_CTOS_GOOD)) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /* We don't have this enabled */
    if (sc->server_cert_type == NULL) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /* Given back a value we didn't configure */
    if (memchr(sc->server_cert_type, type, sc->server_cert_type_len) == NULL) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_VALUE);
        return 0;
    }
    sc->ext.server_cert_type = type;
    return 1;
}

#ifndef OPENSSL_NO_ECH
EXT_RETURN tls_construct_ctos_ech(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    int rv = 0, hpke_mode = OSSL_HPKE_MODE_BASE;
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);
    OSSL_ECHSTORE_ENTRY *ee = NULL;
    OSSL_HPKE_SUITE hpke_suite = OSSL_HPKE_SUITE_DEFAULT;
    unsigned char config_id_to_use = 0x00, info[OSSL_ECH_MAX_INFO_LEN];
    unsigned char *encoded = NULL, *mypub = NULL;
    size_t cipherlen = 0, aad_len = 0, lenclen = 0, mypub_len = 0;
    size_t info_len = OSSL_ECH_MAX_INFO_LEN, clear_len = 0, encoded_len = 0;
    /* whether or not we've been asked to GREASE, one way or another */
    int grease_opt_set = (s->ext.ech.attempted != 1
        && ((s->ext.ech.grease == OSSL_ECH_IS_GREASE)
            || ((s->options & SSL_OP_ECH_GREASE) != 0)));

    /* if we're not doing real ECH and not GREASEing then exit */
    if (s->ext.ech.attempted_type != TLSEXT_TYPE_ech && grease_opt_set == 0)
        return EXT_RETURN_NOT_SENT;
    /* send grease if not really attempting ECH */
    if (grease_opt_set == 1) {
        if (s->hello_retry_request == SSL_HRR_PENDING
            && s->ext.ech.sent != NULL) {
            /* re-tx already sent GREASEy ECH */
            if (WPACKET_memcpy(pkt, s->ext.ech.sent,
                    s->ext.ech.sent_len)
                != 1) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return EXT_RETURN_FAIL;
            }
            return EXT_RETURN_SENT;
        }
        /* if nobody set a type, use the default */
        if (s->ext.ech.attempted_type == OSSL_ECH_type_unknown)
            s->ext.ech.attempted_type = TLSEXT_TYPE_ech;
        if (ossl_ech_send_grease(s, pkt) != 1) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        return EXT_RETURN_SENT;
    }

    /* For the inner CH - we simply include one of these saying "inner" */
    if (s->ext.ech.ch_depth == 1) {
        if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_ech)
            || !WPACKET_start_sub_packet_u16(pkt)
            || !WPACKET_put_bytes_u8(pkt, OSSL_ECH_INNER_CH_TYPE)
            || !WPACKET_close(pkt)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return EXT_RETURN_FAIL;
        }
        return EXT_RETURN_SENT;
    }

    /*
     * If not GREASEing we prepare sending the outer value - after the
     * entire thing has been constructed, putting in zeros for now where
     * we'd otherwise include ECH ciphertext, we later encode and encrypt.
     * We need to do it that way as we need the rest of the outer CH to
     * be known and used as AAD input before we do encryption.
     */
    if (s->ext.ech.ch_depth != 0)
        return EXT_RETURN_NOT_SENT;
    /* Make ClientHelloInner and EncodedClientHelloInner as per spec. */
    if (ossl_ech_encode_inner(s, &encoded, &encoded_len) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    s->ext.ech.encoded_inner = encoded;
    s->ext.ech.encoded_inner_len = encoded_len;
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("encoded inner CH", encoded, encoded_len);
#endif
    rv = ossl_ech_pick_matching_cfg(s, &ee, &hpke_suite);
    if (rv != 1 || ee == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    s->ext.ech.attempted_type = ee->version;
    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "EAAE: selected: version: %4x, config %2x\n",
            ee->version, ee->config_id);
    }
    OSSL_TRACE_END(TLS);
    config_id_to_use = ee->config_id; /* if requested, use a random config_id instead */
    if ((s->options & SSL_OP_ECH_IGNORE_CID) != 0) {
        int max_iters = 1000, i = 0;

        /* rejection sample to get a different but random config_id */
        while (config_id_to_use == ee->config_id) {
#ifdef OSSL_ECH_SUPERVERBOSE
            if (i > 0) {
                OSSL_TRACE_BEGIN(TLS)
                {
                    BIO_printf(trc_out, "EAAE: rejected random-config %02x\n",
                        config_id_to_use);
                }
                OSSL_TRACE_END(TLS);
            }
#endif
            if (RAND_bytes_ex(sctx->libctx, &config_id_to_use, 1, 0) <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
            if (i++ >= max_iters) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
#ifdef OSSL_ECH_SUPERVERBOSE
        ossl_ech_pbuf("EAAE: random config_id", &config_id_to_use, 1);
#endif
    }
    s->ext.ech.attempted_cid = config_id_to_use;
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("EAAE: peer pub", ee->pub, ee->pub_len);
    ossl_ech_pbuf("EAAE: clear", encoded, encoded_len);
    ossl_ech_pbuf("EAAE: ECHConfig", ee->encoded, ee->encoded_len);
#endif
    /*
     * The AAD is the full outer client hello but with the correct number of
     * zeros for where the ECH ciphertext octets will later be placed. So we
     * add the ECH extension to the |pkt| but with zeros for ciphertext, that
     * forms up the AAD, then after we've encrypted, we'll splice in the actual
     * ciphertext.
     * Watch out for the "4" offsets that remove the type and 3-octet length
     * from the encoded CH as per the spec.
     */
    clear_len = ossl_ech_calc_padding(s, ee, encoded_len);
    if (clear_len == 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    lenclen = OSSL_HPKE_get_public_encap_size(hpke_suite);
    if (s->ext.ech.hpke_ctx == NULL) { /* 1st CH */
        if (ossl_ech_make_enc_info(ee->encoded, ee->encoded_len,
                info, &info_len)
            != 1) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto err;
        }
#ifdef OSSL_ECH_SUPERVERBOSE
        ossl_ech_pbuf("EAAE info", info, info_len);
#endif
        s->ext.ech.hpke_ctx = OSSL_HPKE_CTX_new(hpke_mode, hpke_suite,
            OSSL_HPKE_ROLE_SENDER,
            sctx->libctx, sctx->propq);
        if (s->ext.ech.hpke_ctx == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        mypub = OPENSSL_malloc(lenclen);
        if (mypub == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        mypub_len = lenclen;
        rv = OSSL_HPKE_encap(s->ext.ech.hpke_ctx, mypub, &mypub_len,
            ee->pub, ee->pub_len, info, info_len);
        if (rv != 1) {
            OPENSSL_free(mypub);
            mypub = NULL;
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        s->ext.ech.pub = mypub;
        s->ext.ech.pub_len = mypub_len;
    } else { /* HRR - retrieve public */
        mypub = s->ext.ech.pub;
        mypub_len = s->ext.ech.pub_len;
        if (mypub == NULL || mypub_len == 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("EAAE: mypub", mypub, mypub_len);
    WPACKET_get_total_written(pkt, &aad_len); /* use aad_len for tracing */
    ossl_ech_pbuf("EAAE pkt b4", WPACKET_get_curr(pkt) - aad_len, aad_len);
#endif
    cipherlen = OSSL_HPKE_get_ciphertext_size(hpke_suite, clear_len);
    if (cipherlen <= clear_len || cipherlen > OSSL_ECH_MAX_PAYLOAD_LEN) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    s->ext.ech.clearlen = clear_len;
    s->ext.ech.cipherlen = cipherlen;
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_ech)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_put_bytes_u8(pkt, OSSL_ECH_OUTER_CH_TYPE)
        || !WPACKET_put_bytes_u16(pkt, hpke_suite.kdf_id)
        || !WPACKET_put_bytes_u16(pkt, hpke_suite.aead_id)
        || !WPACKET_put_bytes_u8(pkt, config_id_to_use)
        || (s->hello_retry_request == SSL_HRR_PENDING
            && !WPACKET_put_bytes_u16(pkt, 0x00)) /* no pub */
        || (s->hello_retry_request != SSL_HRR_PENDING
            && !WPACKET_sub_memcpy_u16(pkt, mypub, mypub_len))
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_get_total_written(pkt, &s->ext.ech.cipher_offset)
        || !WPACKET_memset(pkt, 0, cipherlen)
        || !WPACKET_close(pkt)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* don't count the type + 3-octet length */
    s->ext.ech.cipher_offset -= 4;
    return EXT_RETURN_SENT;
err:
    return EXT_RETURN_FAIL;
}

/* if the server thinks we GREASE'd then we may get an ECHConfigList */
int tls_parse_stoc_ech(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
    X509 *x, size_t chainidx)
{
    size_t rlen = 0;
    const unsigned char *rval = NULL;
    unsigned char *srval = NULL;
    PACKET rcfgs_pkt;

    /*
     * An HRR will have an ECH extension with the 8-octet confirmation value.
     * Store it away for when we check it later
     */
    if (context == SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST) {
        if (PACKET_remaining(pkt) != OSSL_ECH_SIGNAL_LEN) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_MISMATCH);
            return 0;
        }
        s->ext.ech.hrrsignal_p = (unsigned char *)PACKET_data(pkt);
        memcpy(s->ext.ech.hrrsignal, s->ext.ech.hrrsignal_p,
            OSSL_ECH_SIGNAL_LEN);
        return 1;
    }
    /* otherwise we expect retry-configs */
    if (!PACKET_get_length_prefixed_2(pkt, &rcfgs_pkt)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_MISMATCH);
        return 0;
    }
    rval = PACKET_data(&rcfgs_pkt);
    rlen = (unsigned int)PACKET_remaining(&rcfgs_pkt);
    OPENSSL_free(s->ext.ech.returned);
    s->ext.ech.returned = NULL;
    srval = OPENSSL_malloc(rlen + 2);
    if (srval == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    srval[0] = (rlen >> 8) & 0xff;
    srval[1] = rlen & 0xff;
    memcpy(srval + 2, rval, rlen);
    s->ext.ech.returned = srval;
    s->ext.ech.returned_len = rlen + 2;
    return 1;
}
#endif /* END_OPENSSL_NO_ECH */

/*
 * RFC 8701 GREASE extension constructors.  Each writes an empty extension
 * whose type is a GREASE value (0x?A?A pattern).
 */
EXT_RETURN tls_construct_ctos_grease1(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    uint16_t grease_type;

    if (!(s->options & SSL_OP_GREASE) || s->server)
        return EXT_RETURN_NOT_SENT;

    grease_type = ossl_grease_value(s, OSSL_GREASE_EXT1);

    if (!WPACKET_put_bytes_u16(pkt, grease_type)
        || !WPACKET_put_bytes_u16(pkt, 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

EXT_RETURN tls_construct_ctos_grease2(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    uint16_t grease_type;

    if (!(s->options & SSL_OP_GREASE) || s->server)
        return EXT_RETURN_NOT_SENT;

    grease_type = ossl_grease_value(s, OSSL_GREASE_EXT2);

    /*
     * RFC 8701 recommends "varying length and contents" for GREASE
     * extensions.  Extension 1 is empty; extension 2 carries one zero byte
     * so that servers are tested against both empty and non-empty unknown
     * extensions.  This mirrors the BoringSSL behaviour.
     */
    if (!WPACKET_put_bytes_u16(pkt, grease_type)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_put_bytes_u8(pkt, 0)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

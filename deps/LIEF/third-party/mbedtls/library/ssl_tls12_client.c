/*
 *  TLS client-side functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_SSL_CLI_C) && defined(MBEDTLS_SSL_PROTO_TLS1_2)

#include "mbedtls/platform.h"

#include "mbedtls/ssl.h"
#include "ssl_client.h"
#include "ssl_misc.h"
#include "debug_internal.h"
#include "mbedtls/error.h"
#include "mbedtls/constant_time.h"

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa_util_internal.h"
#include "psa/crypto.h"
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
/* Define a local translating function to save code size by not using too many
 * arguments in each translating place. */
static int local_err_translation(psa_status_t status)
{
    return psa_status_to_mbedtls(status, psa_to_ssl_errors,
                                 ARRAY_LENGTH(psa_to_ssl_errors),
                                 psa_generic_status_to_mbedtls);
}
#define PSA_TO_MBEDTLS_ERR(status) local_err_translation(status)
#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED */
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#include <string.h>

#include <stdint.h>

#if defined(MBEDTLS_HAVE_TIME)
#include "mbedtls/platform_time.h"
#endif

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
#include "mbedtls/platform_util.h"
#endif

#if defined(MBEDTLS_SSL_RENEGOTIATION)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_renegotiation_ext(mbedtls_ssl_context *ssl,
                                       unsigned char *buf,
                                       const unsigned char *end,
                                       size_t *olen)
{
    unsigned char *p = buf;

    *olen = 0;

    /* We're always including a TLS_EMPTY_RENEGOTIATION_INFO_SCSV in the
     * initial ClientHello, in which case also adding the renegotiation
     * info extension is NOT RECOMMENDED as per RFC 5746 Section 3.4. */
    if (ssl->renego_status != MBEDTLS_SSL_RENEGOTIATION_IN_PROGRESS) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("client hello, adding renegotiation extension"));

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 5 + ssl->verify_data_len);

    /*
     * Secure renegotiation
     */
    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_RENEGOTIATION_INFO, p, 0);
    p += 2;

    *p++ = 0x00;
    *p++ = MBEDTLS_BYTE_0(ssl->verify_data_len + 1);
    *p++ = MBEDTLS_BYTE_0(ssl->verify_data_len);

    memcpy(p, ssl->own_verify_data, ssl->verify_data_len);

    *olen = 5 + ssl->verify_data_len;

    return 0;
}
#endif /* MBEDTLS_SSL_RENEGOTIATION */

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_supported_point_formats_ext(mbedtls_ssl_context *ssl,
                                                 unsigned char *buf,
                                                 const unsigned char *end,
                                                 size_t *olen)
{
    unsigned char *p = buf;
    (void) ssl; /* ssl used for debugging only */

    *olen = 0;

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("client hello, adding supported_point_formats extension"));
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 6);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_SUPPORTED_POINT_FORMATS, p, 0);
    p += 2;

    *p++ = 0x00;
    *p++ = 2;

    *p++ = 1;
    *p++ = MBEDTLS_ECP_PF_UNCOMPRESSED;

    *olen = 6;

    return 0;
}
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_ecjpake_kkpp_ext(mbedtls_ssl_context *ssl,
                                      unsigned char *buf,
                                      const unsigned char *end,
                                      size_t *olen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    size_t kkpp_len = 0;

    *olen = 0;

    /* Skip costly extension if we can't use EC J-PAKE anyway */
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if (ssl->handshake->psa_pake_ctx_is_ok != 1) {
        return 0;
    }
#else
    if (mbedtls_ecjpake_check(&ssl->handshake->ecjpake_ctx) != 0) {
        return 0;
    }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("client hello, adding ecjpake_kkpp extension"));

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 4);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_ECJPAKE_KKPP, p, 0);
    p += 2;

    /*
     * We may need to send ClientHello multiple times for Hello verification.
     * We don't want to compute fresh values every time (both for performance
     * and consistency reasons), so cache the extension content.
     */
    if (ssl->handshake->ecjpake_cache == NULL ||
        ssl->handshake->ecjpake_cache_len == 0) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("generating new ecjpake parameters"));

#if defined(MBEDTLS_USE_PSA_CRYPTO)
        ret = mbedtls_psa_ecjpake_write_round(&ssl->handshake->psa_pake_ctx,
                                              p + 2, end - p - 2, &kkpp_len,
                                              MBEDTLS_ECJPAKE_ROUND_ONE);
        if (ret != 0) {
            psa_destroy_key(ssl->handshake->psa_pake_password);
            psa_pake_abort(&ssl->handshake->psa_pake_ctx);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_pake_output", ret);
            return ret;
        }
#else
        ret = mbedtls_ecjpake_write_round_one(&ssl->handshake->ecjpake_ctx,
                                              p + 2, end - p - 2, &kkpp_len,
                                              ssl->conf->f_rng, ssl->conf->p_rng);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1,
                                  "mbedtls_ecjpake_write_round_one", ret);
            return ret;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        ssl->handshake->ecjpake_cache = mbedtls_calloc(1, kkpp_len);
        if (ssl->handshake->ecjpake_cache == NULL) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("allocation failed"));
            return MBEDTLS_ERR_SSL_ALLOC_FAILED;
        }

        memcpy(ssl->handshake->ecjpake_cache, p + 2, kkpp_len);
        ssl->handshake->ecjpake_cache_len = kkpp_len;
    } else {
        MBEDTLS_SSL_DEBUG_MSG(3, ("re-using cached ecjpake parameters"));

        kkpp_len = ssl->handshake->ecjpake_cache_len;
        MBEDTLS_SSL_CHK_BUF_PTR(p + 2, end, kkpp_len);

        memcpy(p + 2, ssl->handshake->ecjpake_cache, kkpp_len);
    }

    MBEDTLS_PUT_UINT16_BE(kkpp_len, p, 0);
    p += 2;

    *olen = kkpp_len + 4;

    return 0;
}
#endif /* MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_cid_ext(mbedtls_ssl_context *ssl,
                             unsigned char *buf,
                             const unsigned char *end,
                             size_t *olen)
{
    unsigned char *p = buf;
    size_t ext_len;

    /*
     *   struct {
     *      opaque cid<0..2^8-1>;
     *   } ConnectionId;
     */

    *olen = 0;
    if (ssl->conf->transport != MBEDTLS_SSL_TRANSPORT_DATAGRAM ||
        ssl->negotiate_cid == MBEDTLS_SSL_CID_DISABLED) {
        return 0;
    }
    MBEDTLS_SSL_DEBUG_MSG(3, ("client hello, adding CID extension"));

    /* ssl->own_cid_len is at most MBEDTLS_SSL_CID_IN_LEN_MAX
     * which is at most 255, so the increment cannot overflow. */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, (unsigned) (ssl->own_cid_len + 5));

    /* Add extension ID + size */
    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_CID, p, 0);
    p += 2;
    ext_len = (size_t) ssl->own_cid_len + 1;
    MBEDTLS_PUT_UINT16_BE(ext_len, p, 0);
    p += 2;

    *p++ = (uint8_t) ssl->own_cid_len;
    memcpy(p, ssl->own_cid, ssl->own_cid_len);

    *olen = ssl->own_cid_len + 5;

    return 0;
}
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_max_fragment_length_ext(mbedtls_ssl_context *ssl,
                                             unsigned char *buf,
                                             const unsigned char *end,
                                             size_t *olen)
{
    unsigned char *p = buf;

    *olen = 0;

    if (ssl->conf->mfl_code == MBEDTLS_SSL_MAX_FRAG_LEN_NONE) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("client hello, adding max_fragment_length extension"));

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 5);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_MAX_FRAGMENT_LENGTH, p, 0);
    p += 2;

    *p++ = 0x00;
    *p++ = 1;

    *p++ = ssl->conf->mfl_code;

    *olen = 5;

    return 0;
}
#endif /* MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_encrypt_then_mac_ext(mbedtls_ssl_context *ssl,
                                          unsigned char *buf,
                                          const unsigned char *end,
                                          size_t *olen)
{
    unsigned char *p = buf;

    *olen = 0;

    if (ssl->conf->encrypt_then_mac == MBEDTLS_SSL_ETM_DISABLED) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("client hello, adding encrypt_then_mac extension"));

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 4);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_ENCRYPT_THEN_MAC, p, 0);
    p += 2;

    *p++ = 0x00;
    *p++ = 0x00;

    *olen = 4;

    return 0;
}
#endif /* MBEDTLS_SSL_ENCRYPT_THEN_MAC */

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_extended_ms_ext(mbedtls_ssl_context *ssl,
                                     unsigned char *buf,
                                     const unsigned char *end,
                                     size_t *olen)
{
    unsigned char *p = buf;

    *olen = 0;

    if (ssl->conf->extended_ms == MBEDTLS_SSL_EXTENDED_MS_DISABLED) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("client hello, adding extended_master_secret extension"));

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 4);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_EXTENDED_MASTER_SECRET, p, 0);
    p += 2;

    *p++ = 0x00;
    *p++ = 0x00;

    *olen = 4;

    return 0;
}
#endif /* MBEDTLS_SSL_EXTENDED_MASTER_SECRET */

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_session_ticket_ext(mbedtls_ssl_context *ssl,
                                        unsigned char *buf,
                                        const unsigned char *end,
                                        size_t *olen)
{
    unsigned char *p = buf;
    size_t tlen = ssl->session_negotiate->ticket_len;

    *olen = 0;

    if (mbedtls_ssl_conf_get_session_tickets(ssl->conf) ==
        MBEDTLS_SSL_SESSION_TICKETS_DISABLED) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("client hello, adding session ticket extension"));

    /* The addition is safe here since the ticket length is 16 bit. */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 4 + tlen);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_SESSION_TICKET, p, 0);
    p += 2;

    MBEDTLS_PUT_UINT16_BE(tlen, p, 0);
    p += 2;

    *olen = 4;

    if (ssl->session_negotiate->ticket == NULL || tlen == 0) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("sending session ticket of length %" MBEDTLS_PRINTF_SIZET, tlen));

    memcpy(p, ssl->session_negotiate->ticket, tlen);

    *olen += tlen;

    return 0;
}
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

#if defined(MBEDTLS_SSL_DTLS_SRTP)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_use_srtp_ext(mbedtls_ssl_context *ssl,
                                  unsigned char *buf,
                                  const unsigned char *end,
                                  size_t *olen)
{
    unsigned char *p = buf;
    size_t protection_profiles_index = 0, ext_len = 0;
    uint16_t mki_len = 0, profile_value = 0;

    *olen = 0;

    if ((ssl->conf->transport != MBEDTLS_SSL_TRANSPORT_DATAGRAM) ||
        (ssl->conf->dtls_srtp_profile_list == NULL) ||
        (ssl->conf->dtls_srtp_profile_list_len == 0)) {
        return 0;
    }

    /* RFC 5764 section 4.1.1
     * uint8 SRTPProtectionProfile[2];
     *
     * struct {
     *   SRTPProtectionProfiles SRTPProtectionProfiles;
     *   opaque srtp_mki<0..255>;
     * } UseSRTPData;
     * SRTPProtectionProfile SRTPProtectionProfiles<2..2^16-1>;
     */
    if (ssl->conf->dtls_srtp_mki_support == MBEDTLS_SSL_DTLS_SRTP_MKI_SUPPORTED) {
        mki_len = ssl->dtls_srtp_info.mki_len;
    }
    /* Extension length = 2 bytes for profiles length,
     *                    ssl->conf->dtls_srtp_profile_list_len * 2 (each profile is 2 bytes length ),
     *                    1 byte for srtp_mki vector length and the mki_len value
     */
    ext_len = 2 + 2 * (ssl->conf->dtls_srtp_profile_list_len) + 1 + mki_len;

    MBEDTLS_SSL_DEBUG_MSG(3, ("client hello, adding use_srtp extension"));

    /* Check there is room in the buffer for the extension + 4 bytes
     * - the extension tag (2 bytes)
     * - the extension length (2 bytes)
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, ext_len + 4);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_USE_SRTP, p, 0);
    p += 2;

    MBEDTLS_PUT_UINT16_BE(ext_len, p, 0);
    p += 2;

    /* protection profile length: 2*(ssl->conf->dtls_srtp_profile_list_len) */
    /* micro-optimization:
     * the list size is limited to MBEDTLS_TLS_SRTP_MAX_PROFILE_LIST_LENGTH
     * which is lower than 127, so the upper byte of the length is always 0
     * For the documentation, the more generic code is left in comments
     * *p++ = (unsigned char)( ( ( 2 * ssl->conf->dtls_srtp_profile_list_len )
     *                        >> 8 ) & 0xFF );
     */
    *p++ = 0;
    *p++ = MBEDTLS_BYTE_0(2 * ssl->conf->dtls_srtp_profile_list_len);

    for (protection_profiles_index = 0;
         protection_profiles_index < ssl->conf->dtls_srtp_profile_list_len;
         protection_profiles_index++) {
        profile_value = mbedtls_ssl_check_srtp_profile_value
                            (ssl->conf->dtls_srtp_profile_list[protection_profiles_index]);
        if (profile_value != MBEDTLS_TLS_SRTP_UNSET) {
            MBEDTLS_SSL_DEBUG_MSG(3, ("ssl_write_use_srtp_ext, add profile: %04x",
                                      profile_value));
            MBEDTLS_PUT_UINT16_BE(profile_value, p, 0);
            p += 2;
        } else {
            /*
             * Note: we shall never arrive here as protection profiles
             * is checked by mbedtls_ssl_conf_dtls_srtp_protection_profiles function
             */
            MBEDTLS_SSL_DEBUG_MSG(3,
                                  ("client hello, "
                                   "illegal DTLS-SRTP protection profile %d",
                                   ssl->conf->dtls_srtp_profile_list[protection_profiles_index]
                                  ));
            return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
        }
    }

    *p++ = mki_len & 0xFF;

    if (mki_len != 0) {
        memcpy(p, ssl->dtls_srtp_info.mki_value, mki_len);
        /*
         * Increment p to point to the current position.
         */
        p += mki_len;
        MBEDTLS_SSL_DEBUG_BUF(3, "sending mki",  ssl->dtls_srtp_info.mki_value,
                              ssl->dtls_srtp_info.mki_len);
    }

    /*
     * total extension length: extension type (2 bytes)
     *                         + extension length (2 bytes)
     *                         + protection profile length (2 bytes)
     *                         + 2 * number of protection profiles
     *                         + srtp_mki vector length(1 byte)
     *                         + mki value
     */
    *olen = p - buf;

    return 0;
}
#endif /* MBEDTLS_SSL_DTLS_SRTP */

int mbedtls_ssl_tls12_write_client_hello_exts(mbedtls_ssl_context *ssl,
                                              unsigned char *buf,
                                              const unsigned char *end,
                                              int uses_ec,
                                              size_t *out_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    size_t ext_len = 0;

    (void) ssl;
    (void) end;
    (void) uses_ec;
    (void) ret;
    (void) ext_len;

    *out_len = 0;

    /* Note that TLS_EMPTY_RENEGOTIATION_INFO_SCSV is always added
     * even if MBEDTLS_SSL_RENEGOTIATION is not defined. */
#if defined(MBEDTLS_SSL_RENEGOTIATION)
    if ((ret = ssl_write_renegotiation_ext(ssl, p, end, &ext_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_renegotiation_ext", ret);
        return ret;
    }
    p += ext_len;
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
    if (uses_ec) {
        if ((ret = ssl_write_supported_point_formats_ext(ssl, p, end,
                                                         &ext_len)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_supported_point_formats_ext", ret);
            return ret;
        }
        p += ext_len;
    }
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
    if ((ret = ssl_write_ecjpake_kkpp_ext(ssl, p, end, &ext_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_ecjpake_kkpp_ext", ret);
        return ret;
    }
    p += ext_len;
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    if ((ret = ssl_write_cid_ext(ssl, p, end, &ext_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_cid_ext", ret);
        return ret;
    }
    p += ext_len;
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
    if ((ret = ssl_write_max_fragment_length_ext(ssl, p, end,
                                                 &ext_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_max_fragment_length_ext", ret);
        return ret;
    }
    p += ext_len;
#endif

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
    if ((ret = ssl_write_encrypt_then_mac_ext(ssl, p, end, &ext_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_encrypt_then_mac_ext", ret);
        return ret;
    }
    p += ext_len;
#endif

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET)
    if ((ret = ssl_write_extended_ms_ext(ssl, p, end, &ext_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_extended_ms_ext", ret);
        return ret;
    }
    p += ext_len;
#endif

#if defined(MBEDTLS_SSL_DTLS_SRTP)
    if ((ret = ssl_write_use_srtp_ext(ssl, p, end, &ext_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_use_srtp_ext", ret);
        return ret;
    }
    p += ext_len;
#endif

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
    if ((ret = ssl_write_session_ticket_ext(ssl, p, end, &ext_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_session_ticket_ext", ret);
        return ret;
    }
    p += ext_len;
#endif

    *out_len = (size_t) (p - buf);

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_renegotiation_info(mbedtls_ssl_context *ssl,
                                        const unsigned char *buf,
                                        size_t len)
{
#if defined(MBEDTLS_SSL_RENEGOTIATION)
    if (ssl->renego_status != MBEDTLS_SSL_INITIAL_HANDSHAKE) {
        /* Check verify-data in constant-time. The length OTOH is no secret */
        if (len    != 1 + ssl->verify_data_len * 2 ||
            buf[0] !=     ssl->verify_data_len * 2 ||
            mbedtls_ct_memcmp(buf + 1,
                              ssl->own_verify_data, ssl->verify_data_len) != 0 ||
            mbedtls_ct_memcmp(buf + 1 + ssl->verify_data_len,
                              ssl->peer_verify_data, ssl->verify_data_len) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("non-matching renegotiation info"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
            return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
        }
    } else
#endif /* MBEDTLS_SSL_RENEGOTIATION */
    {
        if (len != 1 || buf[0] != 0x00) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("non-zero length renegotiation info"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
            return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
        }

        ssl->secure_renegotiation = MBEDTLS_SSL_SECURE_RENEGOTIATION;
    }

    return 0;
}

#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_max_fragment_length_ext(mbedtls_ssl_context *ssl,
                                             const unsigned char *buf,
                                             size_t len)
{
    /*
     * server should use the extension only if we did,
     * and if so the server's value should match ours (and len is always 1)
     */
    if (ssl->conf->mfl_code == MBEDTLS_SSL_MAX_FRAG_LEN_NONE ||
        len != 1 ||
        buf[0] != ssl->conf->mfl_code) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("non-matching max fragment length extension"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }

    return 0;
}
#endif /* MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_cid_ext(mbedtls_ssl_context *ssl,
                             const unsigned char *buf,
                             size_t len)
{
    size_t peer_cid_len;

    if ( /* CID extension only makes sense in DTLS */
        ssl->conf->transport != MBEDTLS_SSL_TRANSPORT_DATAGRAM ||
        /* The server must only send the CID extension if we have offered it. */
        ssl->negotiate_cid == MBEDTLS_SSL_CID_DISABLED) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("CID extension unexpected"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_EXT);
        return MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION;
    }

    if (len == 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("CID extension invalid"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    peer_cid_len = *buf++;
    len--;

    if (peer_cid_len > MBEDTLS_SSL_CID_OUT_LEN_MAX) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("CID extension invalid"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }

    if (len != peer_cid_len) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("CID extension invalid"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    ssl->handshake->cid_in_use = MBEDTLS_SSL_CID_ENABLED;
    ssl->handshake->peer_cid_len = (uint8_t) peer_cid_len;
    memcpy(ssl->handshake->peer_cid, buf, peer_cid_len);

    MBEDTLS_SSL_DEBUG_MSG(3, ("Use of CID extension negotiated"));
    MBEDTLS_SSL_DEBUG_BUF(3, "Server CID", buf, peer_cid_len);

    return 0;
}
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_encrypt_then_mac_ext(mbedtls_ssl_context *ssl,
                                          const unsigned char *buf,
                                          size_t len)
{
    if (ssl->conf->encrypt_then_mac == MBEDTLS_SSL_ETM_DISABLED ||
        len != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("non-matching encrypt-then-MAC extension"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_EXT);
        return MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION;
    }

    ((void) buf);

    ssl->session_negotiate->encrypt_then_mac = MBEDTLS_SSL_ETM_ENABLED;

    return 0;
}
#endif /* MBEDTLS_SSL_ENCRYPT_THEN_MAC */

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_extended_ms_ext(mbedtls_ssl_context *ssl,
                                     const unsigned char *buf,
                                     size_t len)
{
    if (ssl->conf->extended_ms == MBEDTLS_SSL_EXTENDED_MS_DISABLED ||
        len != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("non-matching extended master secret extension"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_EXT);
        return MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION;
    }

    ((void) buf);

    ssl->handshake->extended_ms = MBEDTLS_SSL_EXTENDED_MS_ENABLED;

    return 0;
}
#endif /* MBEDTLS_SSL_EXTENDED_MASTER_SECRET */

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_session_ticket_ext(mbedtls_ssl_context *ssl,
                                        const unsigned char *buf,
                                        size_t len)
{
    if ((mbedtls_ssl_conf_get_session_tickets(ssl->conf) ==
         MBEDTLS_SSL_SESSION_TICKETS_DISABLED) ||
        len != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("non-matching session ticket extension"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_EXT);
        return MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION;
    }

    ((void) buf);

    ssl->handshake->new_session_ticket = 1;

    return 0;
}
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_supported_point_formats_ext(mbedtls_ssl_context *ssl,
                                                 const unsigned char *buf,
                                                 size_t len)
{
    size_t list_size;
    const unsigned char *p;

    if (len == 0 || (size_t) (buf[0] + 1) != len) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }
    list_size = buf[0];

    p = buf + 1;
    while (list_size > 0) {
        if (p[0] == MBEDTLS_ECP_PF_UNCOMPRESSED ||
            p[0] == MBEDTLS_ECP_PF_COMPRESSED) {
#if !defined(MBEDTLS_USE_PSA_CRYPTO) && \
            defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED)
            ssl->handshake->ecdh_ctx.point_format = p[0];
#endif /* !MBEDTLS_USE_PSA_CRYPTO && MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED */
#if !defined(MBEDTLS_USE_PSA_CRYPTO) &&                             \
            defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
            mbedtls_ecjpake_set_point_format(&ssl->handshake->ecjpake_ctx,
                                             p[0]);
#endif /* !MBEDTLS_USE_PSA_CRYPTO && MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */
            MBEDTLS_SSL_DEBUG_MSG(4, ("point format selected: %d", p[0]));
            return 0;
        }

        list_size--;
        p++;
    }

    MBEDTLS_SSL_DEBUG_MSG(1, ("no point format in common"));
    mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                   MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
    return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
}
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_ecjpake_kkpp(mbedtls_ssl_context *ssl,
                                  const unsigned char *buf,
                                  size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (ssl->handshake->ciphersuite_info->key_exchange !=
        MBEDTLS_KEY_EXCHANGE_ECJPAKE) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("skip ecjpake kkpp extension"));
        return 0;
    }

    /* If we got here, we no longer need our cached extension */
    mbedtls_free(ssl->handshake->ecjpake_cache);
    ssl->handshake->ecjpake_cache = NULL;
    ssl->handshake->ecjpake_cache_len = 0;

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if ((ret = mbedtls_psa_ecjpake_read_round(
             &ssl->handshake->psa_pake_ctx, buf, len,
             MBEDTLS_ECJPAKE_ROUND_ONE)) != 0) {
        psa_destroy_key(ssl->handshake->psa_pake_password);
        psa_pake_abort(&ssl->handshake->psa_pake_ctx);

        MBEDTLS_SSL_DEBUG_RET(1, "psa_pake_input round one", ret);
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
        return ret;
    }

    return 0;
#else
    if ((ret = mbedtls_ecjpake_read_round_one(&ssl->handshake->ecjpake_ctx,
                                              buf, len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ecjpake_read_round_one", ret);
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
        return ret;
    }

    return 0;
#endif /* MBEDTLS_USE_PSA_CRYPTO */
}
#endif /* MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */

#if defined(MBEDTLS_SSL_ALPN)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_alpn_ext(mbedtls_ssl_context *ssl,
                              const unsigned char *buf, size_t len)
{
    size_t list_len, name_len;
    const char **p;

    /* If we didn't send it, the server shouldn't send it */
    if (ssl->conf->alpn_list == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("non-matching ALPN extension"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_EXT);
        return MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION;
    }

    /*
     * opaque ProtocolName<1..2^8-1>;
     *
     * struct {
     *     ProtocolName protocol_name_list<2..2^16-1>
     * } ProtocolNameList;
     *
     * the "ProtocolNameList" MUST contain exactly one "ProtocolName"
     */

    /* Min length is 2 (list_len) + 1 (name_len) + 1 (name) */
    if (len < 4) {
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    list_len = MBEDTLS_GET_UINT16_BE(buf, 0);
    if (list_len != len - 2) {
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    name_len = buf[2];
    if (name_len != list_len - 1) {
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /* Check that the server chosen protocol was in our list and save it */
    for (p = ssl->conf->alpn_list; *p != NULL; p++) {
        if (name_len == strlen(*p) &&
            memcmp(buf + 3, *p, name_len) == 0) {
            ssl->alpn_chosen = *p;
            return 0;
        }
    }

    MBEDTLS_SSL_DEBUG_MSG(1, ("ALPN extension: no matching protocol"));
    mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                   MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
    return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
}
#endif /* MBEDTLS_SSL_ALPN */

#if defined(MBEDTLS_SSL_DTLS_SRTP)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_use_srtp_ext(mbedtls_ssl_context *ssl,
                                  const unsigned char *buf,
                                  size_t len)
{
    mbedtls_ssl_srtp_profile server_protection = MBEDTLS_TLS_SRTP_UNSET;
    size_t i, mki_len = 0;
    uint16_t server_protection_profile_value = 0;

    /* If use_srtp is not configured, just ignore the extension */
    if ((ssl->conf->transport != MBEDTLS_SSL_TRANSPORT_DATAGRAM) ||
        (ssl->conf->dtls_srtp_profile_list == NULL) ||
        (ssl->conf->dtls_srtp_profile_list_len == 0)) {
        return 0;
    }

    /* RFC 5764 section 4.1.1
     * uint8 SRTPProtectionProfile[2];
     *
     * struct {
     *   SRTPProtectionProfiles SRTPProtectionProfiles;
     *   opaque srtp_mki<0..255>;
     * } UseSRTPData;

     * SRTPProtectionProfile SRTPProtectionProfiles<2..2^16-1>;
     *
     */
    if (ssl->conf->dtls_srtp_mki_support == MBEDTLS_SSL_DTLS_SRTP_MKI_SUPPORTED) {
        mki_len = ssl->dtls_srtp_info.mki_len;
    }

    /*
     * Length is 5 + optional mki_value : one protection profile length (2 bytes)
     *                                      + protection profile (2 bytes)
     *                                      + mki_len(1 byte)
     *                                      and optional srtp_mki
     */
    if ((len < 5) || (len != (buf[4] + 5u))) {
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /*
     * get the server protection profile
     */

    /*
     * protection profile length must be 0x0002 as we must have only
     * one protection profile in server Hello
     */
    if ((buf[0] != 0) || (buf[1] != 2)) {
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    server_protection_profile_value = (buf[2] << 8) | buf[3];
    server_protection = mbedtls_ssl_check_srtp_profile_value(
        server_protection_profile_value);
    if (server_protection != MBEDTLS_TLS_SRTP_UNSET) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("found srtp profile: %s",
                                  mbedtls_ssl_get_srtp_profile_as_string(
                                      server_protection)));
    }

    ssl->dtls_srtp_info.chosen_dtls_srtp_profile = MBEDTLS_TLS_SRTP_UNSET;

    /*
     * Check we have the server profile in our list
     */
    for (i = 0; i < ssl->conf->dtls_srtp_profile_list_len; i++) {
        if (server_protection == ssl->conf->dtls_srtp_profile_list[i]) {
            ssl->dtls_srtp_info.chosen_dtls_srtp_profile = ssl->conf->dtls_srtp_profile_list[i];
            MBEDTLS_SSL_DEBUG_MSG(3, ("selected srtp profile: %s",
                                      mbedtls_ssl_get_srtp_profile_as_string(
                                          server_protection)));
            break;
        }
    }

    /* If no match was found : server problem, it shall never answer with incompatible profile */
    if (ssl->dtls_srtp_info.chosen_dtls_srtp_profile == MBEDTLS_TLS_SRTP_UNSET) {
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    /* If server does not use mki in its reply, make sure the client won't keep
     * one as negotiated */
    if (len == 5) {
        ssl->dtls_srtp_info.mki_len = 0;
    }

    /*
     * RFC5764:
     *  If the client detects a nonzero-length MKI in the server's response
     *  that is different than the one the client offered, then the client
     *  MUST abort the handshake and SHOULD send an invalid_parameter alert.
     */
    if (len > 5  && (buf[4] != mki_len ||
                     (memcmp(ssl->dtls_srtp_info.mki_value, &buf[5], mki_len)))) {
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }
#if defined(MBEDTLS_DEBUG_C)
    if (len > 5) {
        MBEDTLS_SSL_DEBUG_BUF(3, "received mki", ssl->dtls_srtp_info.mki_value,
                              ssl->dtls_srtp_info.mki_len);
    }
#endif
    return 0;
}
#endif /* MBEDTLS_SSL_DTLS_SRTP */

/*
 * Parse HelloVerifyRequest.  Only called after verifying the HS type.
 */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_hello_verify_request(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
    const unsigned char *p = ssl->in_msg + mbedtls_ssl_hs_hdr_len(ssl);
    uint16_t dtls_legacy_version;

#if !defined(MBEDTLS_SSL_PROTO_TLS1_3)
    uint8_t cookie_len;
#else
    uint16_t cookie_len;
#endif

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse hello verify request"));

    /* Check that there is enough room for:
     * - 2 bytes of version
     * - 1 byte of cookie_len
     */
    if (mbedtls_ssl_hs_hdr_len(ssl) + 3 > ssl->in_msglen) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("incoming HelloVerifyRequest message is too short"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /*
     * struct {
     *   ProtocolVersion server_version;
     *   opaque cookie<0..2^8-1>;
     * } HelloVerifyRequest;
     */
    MBEDTLS_SSL_DEBUG_BUF(3, "server version", p, 2);
    dtls_legacy_version = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;

    /*
     * Since the RFC is not clear on this point, accept DTLS 1.0 (0xfeff)
     * The DTLS 1.3 (current draft) renames ProtocolVersion server_version to
     * legacy_version and locks the value of legacy_version to 0xfefd (DTLS 1.2)
     */
    if (dtls_legacy_version != 0xfefd && dtls_legacy_version != 0xfeff) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server version"));

        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_PROTOCOL_VERSION);

        return MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION;
    }

    cookie_len = *p++;
    if ((ssl->in_msg + ssl->in_msglen) - p < cookie_len) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("cookie length does not match incoming message size"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }
    MBEDTLS_SSL_DEBUG_BUF(3, "cookie", p, cookie_len);

    mbedtls_free(ssl->handshake->cookie);

    ssl->handshake->cookie = mbedtls_calloc(1, cookie_len);
    if (ssl->handshake->cookie  == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("alloc failed (%d bytes)", cookie_len));
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }

    memcpy(ssl->handshake->cookie, p, cookie_len);
    ssl->handshake->cookie_len = cookie_len;

    /* Start over at ClientHello */
    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CLIENT_HELLO);
    ret = mbedtls_ssl_reset_checksum(ssl);
    if (0 != ret) {
        MBEDTLS_SSL_DEBUG_RET(1, ("mbedtls_ssl_reset_checksum"), ret);
        return ret;
    }

    mbedtls_ssl_recv_flight_completed(ssl);

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse hello verify request"));

    return 0;
}
#endif /* MBEDTLS_SSL_PROTO_DTLS */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_server_hello(mbedtls_ssl_context *ssl)
{
    int ret, i;
    size_t n;
    size_t ext_len;
    unsigned char *buf, *ext;
    unsigned char comp;
#if defined(MBEDTLS_SSL_RENEGOTIATION)
    int renegotiation_info_seen = 0;
#endif
    int handshake_failure = 0;
    const mbedtls_ssl_ciphersuite_t *suite_info;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse server hello"));

    if ((ret = mbedtls_ssl_read_record(ssl, 1)) != 0) {
        /* No alert on a read error. */
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
        return ret;
    }

    buf = ssl->in_msg;

    if (ssl->in_msgtype != MBEDTLS_SSL_MSG_HANDSHAKE) {
#if defined(MBEDTLS_SSL_RENEGOTIATION)
        if (ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_IN_PROGRESS) {
            ssl->renego_records_seen++;

            if (ssl->conf->renego_max_records >= 0 &&
                ssl->renego_records_seen > ssl->conf->renego_max_records) {
                MBEDTLS_SSL_DEBUG_MSG(1,
                                      ("renegotiation requested, but not honored by server"));
                return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
            }

            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("non-handshake message during renegotiation"));

            ssl->keep_current_message = 1;
            return MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO;
        }
#endif /* MBEDTLS_SSL_RENEGOTIATION */

        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE);
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        if (buf[0] == MBEDTLS_SSL_HS_HELLO_VERIFY_REQUEST) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("received hello verify request"));
            MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse server hello"));
            return ssl_parse_hello_verify_request(ssl);
        } else {
            /* We made it through the verification process */
            mbedtls_free(ssl->handshake->cookie);
            ssl->handshake->cookie = NULL;
            ssl->handshake->cookie_len = 0;
        }
    }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

    if (ssl->in_hslen < 38 + mbedtls_ssl_hs_hdr_len(ssl) ||
        buf[0] != MBEDTLS_SSL_HS_SERVER_HELLO) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /*
     *  0   .  1    server_version
     *  2   . 33    random (maybe including 4 bytes of Unix time)
     * 34   . 34    session_id length = n
     * 35   . 34+n  session_id
     * 35+n . 36+n  cipher_suite
     * 37+n . 37+n  compression_method
     *
     * 38+n . 39+n  extensions length (optional)
     * 40+n .  ..   extensions
     */
    buf += mbedtls_ssl_hs_hdr_len(ssl);

    MBEDTLS_SSL_DEBUG_BUF(3, "server hello, version", buf, 2);
    ssl->tls_version = (mbedtls_ssl_protocol_version) mbedtls_ssl_read_version(buf,
                                                                               ssl->conf->transport);
    ssl->session_negotiate->tls_version = ssl->tls_version;
    ssl->session_negotiate->endpoint = ssl->conf->endpoint;

    if (ssl->tls_version < ssl->conf->min_tls_version ||
        ssl->tls_version > ssl->conf->max_tls_version) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              (
                                  "server version out of bounds -  min: [0x%x], server: [0x%x], max: [0x%x]",
                                  (unsigned) ssl->conf->min_tls_version,
                                  (unsigned) ssl->tls_version,
                                  (unsigned) ssl->conf->max_tls_version));

        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_PROTOCOL_VERSION);

        return MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION;
    }

    MBEDTLS_SSL_DEBUG_MSG(3, ("server hello, current time: %lu",
                              ((unsigned long) buf[2] << 24) |
                              ((unsigned long) buf[3] << 16) |
                              ((unsigned long) buf[4] <<  8) |
                              ((unsigned long) buf[5])));

    memcpy(ssl->handshake->randbytes + 32, buf + 2, 32);

    n = buf[34];

    MBEDTLS_SSL_DEBUG_BUF(3,   "server hello, random bytes", buf + 2, 32);

    if (n > 32) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    if (ssl->in_hslen > mbedtls_ssl_hs_hdr_len(ssl) + 39 + n) {
        ext_len = MBEDTLS_GET_UINT16_BE(buf, 38 + n);

        if ((ext_len > 0 && ext_len < 4) ||
            ssl->in_hslen != mbedtls_ssl_hs_hdr_len(ssl) + 40 + n + ext_len) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
            return MBEDTLS_ERR_SSL_DECODE_ERROR;
        }
    } else if (ssl->in_hslen == mbedtls_ssl_hs_hdr_len(ssl) + 38 + n) {
        ext_len = 0;
    } else {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /* ciphersuite (used later) */
    i = (int) MBEDTLS_GET_UINT16_BE(buf, n + 35);

    /*
     * Read and check compression
     */
    comp = buf[37 + n];

    if (comp != MBEDTLS_SSL_COMPRESS_NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("server hello, bad compression: %d", comp));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
    }

    /*
     * Initialize update checksum functions
     */
    ssl->handshake->ciphersuite_info = mbedtls_ssl_ciphersuite_from_id(i);
    if (ssl->handshake->ciphersuite_info == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("ciphersuite info for %04x not found", (unsigned int) i));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR);
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    mbedtls_ssl_optimize_checksum(ssl, ssl->handshake->ciphersuite_info);

    MBEDTLS_SSL_DEBUG_MSG(3, ("server hello, session id len.: %" MBEDTLS_PRINTF_SIZET, n));
    MBEDTLS_SSL_DEBUG_BUF(3,   "server hello, session id", buf + 35, n);

    /*
     * Check if the session can be resumed
     */
    if (ssl->handshake->resume == 0 || n == 0 ||
#if defined(MBEDTLS_SSL_RENEGOTIATION)
        ssl->renego_status != MBEDTLS_SSL_INITIAL_HANDSHAKE ||
#endif
        ssl->session_negotiate->ciphersuite != i ||
        ssl->session_negotiate->id_len != n ||
        memcmp(ssl->session_negotiate->id, buf + 35, n) != 0) {
        mbedtls_ssl_handshake_increment_state(ssl);
        ssl->handshake->resume = 0;
#if defined(MBEDTLS_HAVE_TIME)
        ssl->session_negotiate->start = mbedtls_time(NULL);
#endif
        ssl->session_negotiate->ciphersuite = i;
        ssl->session_negotiate->id_len = n;
        memcpy(ssl->session_negotiate->id, buf + 35, n);
    } else {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_SERVER_CHANGE_CIPHER_SPEC);
    }

    MBEDTLS_SSL_DEBUG_MSG(3, ("%s session has been resumed",
                              ssl->handshake->resume ? "a" : "no"));

    MBEDTLS_SSL_DEBUG_MSG(3, ("server hello, chosen ciphersuite: %04x", (unsigned) i));
    MBEDTLS_SSL_DEBUG_MSG(3, ("server hello, compress alg.: %d",
                              buf[37 + n]));

    /*
     * Perform cipher suite validation in same way as in ssl_write_client_hello.
     */
    i = 0;
    while (1) {
        if (ssl->conf->ciphersuite_list[i] == 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
            return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
        }

        if (ssl->conf->ciphersuite_list[i++] ==
            ssl->session_negotiate->ciphersuite) {
            break;
        }
    }

    suite_info = mbedtls_ssl_ciphersuite_from_id(
        ssl->session_negotiate->ciphersuite);
    if (mbedtls_ssl_validate_ciphersuite(ssl, suite_info, ssl->tls_version,
                                         ssl->tls_version) != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("server hello, chosen ciphersuite: %s", suite_info->name));

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
    if (suite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA &&
        ssl->tls_version == MBEDTLS_SSL_VERSION_TLS1_2) {
        ssl->handshake->ecrs_enabled = 1;
    }
#endif

    if (comp != MBEDTLS_SSL_COMPRESS_NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }

    ext = buf + 40 + n;

    MBEDTLS_SSL_DEBUG_MSG(2,
                          ("server hello, total extension length: %" MBEDTLS_PRINTF_SIZET,
                           ext_len));

    while (ext_len) {
        unsigned int ext_id   = MBEDTLS_GET_UINT16_BE(ext, 0);
        unsigned int ext_size = MBEDTLS_GET_UINT16_BE(ext, 2);

        if (ext_size + 4 > ext_len) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
            mbedtls_ssl_send_alert_message(
                ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
            return MBEDTLS_ERR_SSL_DECODE_ERROR;
        }

        switch (ext_id) {
            case MBEDTLS_TLS_EXT_RENEGOTIATION_INFO:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found renegotiation extension"));
#if defined(MBEDTLS_SSL_RENEGOTIATION)
                renegotiation_info_seen = 1;
#endif

                if ((ret = ssl_parse_renegotiation_info(ssl, ext + 4,
                                                        ext_size)) != 0) {
                    return ret;
                }

                break;

#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
            case MBEDTLS_TLS_EXT_MAX_FRAGMENT_LENGTH:
                MBEDTLS_SSL_DEBUG_MSG(3,
                                      ("found max_fragment_length extension"));

                if ((ret = ssl_parse_max_fragment_length_ext(ssl,
                                                             ext + 4, ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
            case MBEDTLS_TLS_EXT_CID:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found CID extension"));

                if ((ret = ssl_parse_cid_ext(ssl,
                                             ext + 4,
                                             ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
            case MBEDTLS_TLS_EXT_ENCRYPT_THEN_MAC:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found encrypt_then_mac extension"));

                if ((ret = ssl_parse_encrypt_then_mac_ext(ssl,
                                                          ext + 4, ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_SSL_ENCRYPT_THEN_MAC */

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET)
            case MBEDTLS_TLS_EXT_EXTENDED_MASTER_SECRET:
                MBEDTLS_SSL_DEBUG_MSG(3,
                                      ("found extended_master_secret extension"));

                if ((ret = ssl_parse_extended_ms_ext(ssl,
                                                     ext + 4, ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_SSL_EXTENDED_MASTER_SECRET */

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
            case MBEDTLS_TLS_EXT_SESSION_TICKET:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found session_ticket extension"));

                if ((ret = ssl_parse_session_ticket_ext(ssl,
                                                        ext + 4, ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED) || \
                defined(MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED) || \
                defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
            case MBEDTLS_TLS_EXT_SUPPORTED_POINT_FORMATS:
                MBEDTLS_SSL_DEBUG_MSG(3,
                                      ("found supported_point_formats extension"));

                if ((ret = ssl_parse_supported_point_formats_ext(ssl,
                                                                 ext + 4, ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
            case MBEDTLS_TLS_EXT_ECJPAKE_KKPP:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found ecjpake_kkpp extension"));

                if ((ret = ssl_parse_ecjpake_kkpp(ssl,
                                                  ext + 4, ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */

#if defined(MBEDTLS_SSL_ALPN)
            case MBEDTLS_TLS_EXT_ALPN:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found alpn extension"));

                if ((ret = ssl_parse_alpn_ext(ssl, ext + 4, ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_SSL_ALPN */

#if defined(MBEDTLS_SSL_DTLS_SRTP)
            case MBEDTLS_TLS_EXT_USE_SRTP:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found use_srtp extension"));

                if ((ret = ssl_parse_use_srtp_ext(ssl, ext + 4, ext_size)) != 0) {
                    return ret;
                }

                break;
#endif /* MBEDTLS_SSL_DTLS_SRTP */

            default:
                MBEDTLS_SSL_DEBUG_MSG(3,
                                      ("unknown extension found: %u (ignoring)", ext_id));
        }

        ext_len -= 4 + ext_size;
        ext += 4 + ext_size;

        if (ext_len > 0 && ext_len < 4) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello message"));
            return MBEDTLS_ERR_SSL_DECODE_ERROR;
        }
    }

    /*
     * mbedtls_ssl_derive_keys() has to be called after the parsing of the
     * extensions. It sets the transform data for the resumed session which in
     * case of DTLS includes the server CID extracted from the CID extension.
     */
    if (ssl->handshake->resume) {
        if ((ret = mbedtls_ssl_derive_keys(ssl)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_derive_keys", ret);
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR);
            return ret;
        }
    }

    /*
     * Renegotiation security checks
     */
    if (ssl->secure_renegotiation == MBEDTLS_SSL_LEGACY_RENEGOTIATION &&
        ssl->conf->allow_legacy_renegotiation ==
        MBEDTLS_SSL_LEGACY_BREAK_HANDSHAKE) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("legacy renegotiation, breaking off handshake"));
        handshake_failure = 1;
    }
#if defined(MBEDTLS_SSL_RENEGOTIATION)
    else if (ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_IN_PROGRESS &&
             ssl->secure_renegotiation == MBEDTLS_SSL_SECURE_RENEGOTIATION &&
             renegotiation_info_seen == 0) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("renegotiation_info extension missing (secure)"));
        handshake_failure = 1;
    } else if (ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_IN_PROGRESS &&
               ssl->secure_renegotiation == MBEDTLS_SSL_LEGACY_RENEGOTIATION &&
               ssl->conf->allow_legacy_renegotiation ==
               MBEDTLS_SSL_LEGACY_NO_RENEGOTIATION) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("legacy renegotiation not allowed"));
        handshake_failure = 1;
    } else if (ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_IN_PROGRESS &&
               ssl->secure_renegotiation == MBEDTLS_SSL_LEGACY_RENEGOTIATION &&
               renegotiation_info_seen == 1) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("renegotiation_info extension present (legacy)"));
        handshake_failure = 1;
    }
#endif /* MBEDTLS_SSL_RENEGOTIATION */

    if (handshake_failure == 1) {
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse server hello"));

    return 0;
}

#if defined(MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                       \
    defined(MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_server_dh_params(mbedtls_ssl_context *ssl,
                                      unsigned char **p,
                                      unsigned char *end)
{
    int ret = MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
    size_t dhm_actual_bitlen;

    /*
     * Ephemeral DH parameters:
     *
     * struct {
     *     opaque dh_p<1..2^16-1>;
     *     opaque dh_g<1..2^16-1>;
     *     opaque dh_Ys<1..2^16-1>;
     * } ServerDHParams;
     */
    if ((ret = mbedtls_dhm_read_params(&ssl->handshake->dhm_ctx,
                                       p, end)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(2, ("mbedtls_dhm_read_params"), ret);
        return ret;
    }

    dhm_actual_bitlen = mbedtls_dhm_get_bitlen(&ssl->handshake->dhm_ctx);
    if (dhm_actual_bitlen < ssl->conf->dhm_min_bitlen) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("DHM prime too short: %" MBEDTLS_PRINTF_SIZET " < %u",
                                  dhm_actual_bitlen,
                                  ssl->conf->dhm_min_bitlen));
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    MBEDTLS_SSL_DEBUG_MPI(3, "DHM: P ", &ssl->handshake->dhm_ctx.P);
    MBEDTLS_SSL_DEBUG_MPI(3, "DHM: G ", &ssl->handshake->dhm_ctx.G);
    MBEDTLS_SSL_DEBUG_MPI(3, "DHM: GY", &ssl->handshake->dhm_ctx.GY);

    return ret;
}
#endif /* MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED */

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED)   ||   \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)   ||   \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_server_ecdh_params(mbedtls_ssl_context *ssl,
                                        unsigned char **p,
                                        unsigned char *end)
{
    uint16_t tls_id;
    size_t ecpoint_len;
    mbedtls_ssl_handshake_params *handshake = ssl->handshake;
    psa_key_type_t key_type = PSA_KEY_TYPE_NONE;
    size_t ec_bits = 0;

    /*
     * struct {
     *     ECParameters curve_params;
     *     ECPoint      public;
     * } ServerECDHParams;
     *
     *  1       curve_type (must be "named_curve")
     *  2..3    NamedCurve
     *  4       ECPoint.len
     *  5+      ECPoint contents
     */
    if (end - *p < 4) {
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /* First byte is curve_type; only named_curve is handled */
    if (*(*p)++ != MBEDTLS_ECP_TLS_NAMED_CURVE) {
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    /* Next two bytes are the namedcurve value */
    tls_id = MBEDTLS_GET_UINT16_BE(*p, 0);
    *p += 2;

    /* Check it's a curve we offered */
    if (mbedtls_ssl_check_curve_tls_id(ssl, tls_id) != 0) {
        MBEDTLS_SSL_DEBUG_MSG(2,
                              ("bad server key exchange message (ECDHE curve): %u",
                               (unsigned) tls_id));
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    /* Convert EC's TLS ID to PSA key type. */
    if (mbedtls_ssl_get_psa_curve_info_from_tls_id(tls_id, &key_type,
                                                   &ec_bits) == PSA_ERROR_NOT_SUPPORTED) {
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }
    handshake->xxdh_psa_type = key_type;
    handshake->xxdh_psa_bits = ec_bits;

    /* Keep a copy of the peer's public key */
    ecpoint_len = *(*p)++;
    if ((size_t) (end - *p) < ecpoint_len) {
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    if (ecpoint_len > sizeof(handshake->xxdh_psa_peerkey)) {
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    memcpy(handshake->xxdh_psa_peerkey, *p, ecpoint_len);
    handshake->xxdh_psa_peerkey_len = ecpoint_len;
    *p += ecpoint_len;

    return 0;
}
#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED   ||
          MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED   ||
          MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */
#else
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED)   ||   \
    defined(MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED)    ||   \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)   ||   \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) ||   \
    defined(MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_check_server_ecdh_params(const mbedtls_ssl_context *ssl)
{
    uint16_t tls_id;
    mbedtls_ecp_group_id grp_id;
#if defined(MBEDTLS_ECDH_LEGACY_CONTEXT)
    grp_id = ssl->handshake->ecdh_ctx.grp.id;
#else
    grp_id = ssl->handshake->ecdh_ctx.grp_id;
#endif

    tls_id = mbedtls_ssl_get_tls_id_from_ecp_group_id(grp_id);
    if (tls_id == 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("ECDH curve: %s",
                              mbedtls_ssl_get_curve_name_from_tls_id(tls_id)));

    if (mbedtls_ssl_check_curve(ssl, grp_id) != 0) {
        return -1;
    }

    MBEDTLS_SSL_DEBUG_ECDH(3, &ssl->handshake->ecdh_ctx,
                           MBEDTLS_DEBUG_ECDH_QP);

    return 0;
}

#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED   ||
          MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED    ||
          MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED   ||
          MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||     \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED) ||     \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_server_ecdh_params(mbedtls_ssl_context *ssl,
                                        unsigned char **p,
                                        unsigned char *end)
{
    int ret = MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;

    /*
     * Ephemeral ECDH parameters:
     *
     * struct {
     *     ECParameters curve_params;
     *     ECPoint      public;
     * } ServerECDHParams;
     */
    if ((ret = mbedtls_ecdh_read_params(&ssl->handshake->ecdh_ctx,
                                        (const unsigned char **) p, end)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, ("mbedtls_ecdh_read_params"), ret);
#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
        if (ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
            ret = MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS;
        }
#endif
        return ret;
    }

    if (ssl_check_server_ecdh_params(ssl) != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("bad server key exchange message (ECDHE curve)"));
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    return ret;
}
#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED || \
          MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED || \
          MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */
#endif /* !MBEDTLS_USE_PSA_CRYPTO */
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_server_psk_hint(mbedtls_ssl_context *ssl,
                                     unsigned char **p,
                                     unsigned char *end)
{
    int ret = MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
    uint16_t  len;
    ((void) ssl);

    /*
     * PSK parameters:
     *
     * opaque psk_identity_hint<0..2^16-1>;
     */
    if (end - (*p) < 2) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("bad server key exchange message (psk_identity_hint length)"));
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }
    len = MBEDTLS_GET_UINT16_BE(*p, 0);
    *p += 2;

    if (end - (*p) < len) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("bad server key exchange message (psk_identity_hint length)"));
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /*
     * Note: we currently ignore the PSK identity hint, as we only allow one
     * PSK to be provisioned on the client. This could be changed later if
     * someone needs that feature.
     */
    *p += len;
    ret = 0;

    return ret;
}
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_RSA_ENABLED) ||                           \
    defined(MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED)
/*
 * Generate a pre-master secret and encrypt it with the server's RSA key
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_encrypted_pms(mbedtls_ssl_context *ssl,
                                   size_t offset, size_t *olen,
                                   size_t pms_offset)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len_bytes = 2;
    unsigned char *p = ssl->handshake->premaster + pms_offset;
    mbedtls_pk_context *peer_pk;

    if (offset + len_bytes > MBEDTLS_SSL_OUT_CONTENT_LEN) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("buffer too small for encrypted pms"));
        return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
    }

    /*
     * Generate (part of) the pre-master as
     *  struct {
     *      ProtocolVersion client_version;
     *      opaque random[46];
     *  } PreMasterSecret;
     */
    mbedtls_ssl_write_version(p, ssl->conf->transport,
                              MBEDTLS_SSL_VERSION_TLS1_2);

    if ((ret = ssl->conf->f_rng(ssl->conf->p_rng, p + 2, 46)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "f_rng", ret);
        return ret;
    }

    ssl->handshake->pmslen = 48;

#if !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    peer_pk = &ssl->handshake->peer_pubkey;
#else /* !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
    if (ssl->session_negotiate->peer_cert == NULL) {
        /* Should never happen */
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    peer_pk = &ssl->session_negotiate->peer_cert->pk;
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */

    /*
     * Now write it out, encrypted
     */
    if (!mbedtls_pk_can_do(peer_pk, MBEDTLS_PK_RSA)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("certificate key type mismatch"));
        return MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH;
    }

    if ((ret = mbedtls_pk_encrypt(peer_pk,
                                  p, ssl->handshake->pmslen,
                                  ssl->out_msg + offset + len_bytes, olen,
                                  MBEDTLS_SSL_OUT_CONTENT_LEN - offset - len_bytes,
                                  ssl->conf->f_rng, ssl->conf->p_rng)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_pk_encrypt", ret);
        return ret;
    }

    if (len_bytes == 2) {
        MBEDTLS_PUT_UINT16_BE(*olen, ssl->out_msg, offset);
        *olen += 2;
    }

#if !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    /* We don't need the peer's public key anymore. Free it. */
    mbedtls_pk_free(peer_pk);
#endif /* !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
    return 0;
}
#endif /* MBEDTLS_KEY_EXCHANGE_RSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_get_ecdh_params_from_cert(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_pk_context *peer_pk;

#if !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    peer_pk = &ssl->handshake->peer_pubkey;
#else /* !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
    if (ssl->session_negotiate->peer_cert == NULL) {
        /* Should never happen */
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    peer_pk = &ssl->session_negotiate->peer_cert->pk;
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */

    /* This is a public key, so it can't be opaque, so can_do() is a good
     * enough check to ensure pk_ec() is safe to use below. */
    if (!mbedtls_pk_can_do(peer_pk, MBEDTLS_PK_ECKEY)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("server key not ECDH capable"));
        return MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH;
    }

#if !defined(MBEDTLS_PK_USE_PSA_EC_DATA)
    const mbedtls_ecp_keypair *peer_key = mbedtls_pk_ec_ro(*peer_pk);
#endif /* !defined(MBEDTLS_PK_USE_PSA_EC_DATA) */

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    uint16_t tls_id = 0;
    psa_key_type_t key_type = PSA_KEY_TYPE_NONE;
    mbedtls_ecp_group_id grp_id = mbedtls_pk_get_ec_group_id(peer_pk);

    if (mbedtls_ssl_check_curve(ssl, grp_id) != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server certificate (ECDH curve)"));
        return MBEDTLS_ERR_SSL_BAD_CERTIFICATE;
    }

    tls_id = mbedtls_ssl_get_tls_id_from_ecp_group_id(grp_id);
    if (tls_id == 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("ECC group %u not suported",
                                  grp_id));
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }

    /* If the above conversion to TLS ID was fine, then also this one will be,
       so there is no need to check the return value here */
    mbedtls_ssl_get_psa_curve_info_from_tls_id(tls_id, &key_type,
                                               &ssl->handshake->xxdh_psa_bits);

    ssl->handshake->xxdh_psa_type = key_type;

    /* Store peer's public key in psa format. */
#if defined(MBEDTLS_PK_USE_PSA_EC_DATA)
    memcpy(ssl->handshake->xxdh_psa_peerkey, peer_pk->pub_raw, peer_pk->pub_raw_len);
    ssl->handshake->xxdh_psa_peerkey_len = peer_pk->pub_raw_len;
    ret = 0;
#else /* MBEDTLS_PK_USE_PSA_EC_DATA */
    size_t olen = 0;
    ret = mbedtls_ecp_point_write_binary(&peer_key->grp, &peer_key->Q,
                                         MBEDTLS_ECP_PF_UNCOMPRESSED, &olen,
                                         ssl->handshake->xxdh_psa_peerkey,
                                         sizeof(ssl->handshake->xxdh_psa_peerkey));

    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, ("mbedtls_ecp_point_write_binary"), ret);
        return ret;
    }
    ssl->handshake->xxdh_psa_peerkey_len = olen;
#endif /* MBEDTLS_PK_USE_PSA_EC_DATA */
#else /* MBEDTLS_USE_PSA_CRYPTO */
    if ((ret = mbedtls_ecdh_get_params(&ssl->handshake->ecdh_ctx, peer_key,
                                       MBEDTLS_ECDH_THEIRS)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, ("mbedtls_ecdh_get_params"), ret);
        return ret;
    }

    if (ssl_check_server_ecdh_params(ssl) != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server certificate (ECDH curve)"));
        return MBEDTLS_ERR_SSL_BAD_CERTIFICATE;
    }
#endif /* MBEDTLS_USE_PSA_CRYPTO */
#if !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    /* We don't need the peer's public key anymore. Free it,
     * so that more RAM is available for upcoming expensive
     * operations like ECDHE. */
    mbedtls_pk_free(peer_pk);
#endif /* !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */

    return ret;
}
#endif /* MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED) ||
          MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_server_key_exchange(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info =
        ssl->handshake->ciphersuite_info;
    unsigned char *p = NULL, *end = NULL;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse server key exchange"));

#if defined(MBEDTLS_KEY_EXCHANGE_RSA_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_RSA) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("<= skip parse server key exchange"));
        mbedtls_ssl_handshake_increment_state(ssl);
        return 0;
    }
    ((void) p);
    ((void) end);
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDH_RSA ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA) {
        if ((ret = ssl_get_ecdh_params_from_cert(ssl)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "ssl_get_ecdh_params_from_cert", ret);
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
            return ret;
        }

        MBEDTLS_SSL_DEBUG_MSG(2, ("<= skip parse server key exchange"));
        mbedtls_ssl_handshake_increment_state(ssl);
        return 0;
    }
    ((void) p);
    ((void) end);
#endif /* MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED */

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
    if (ssl->handshake->ecrs_enabled &&
        ssl->handshake->ecrs_state == ssl_ecrs_ske_start_processing) {
        goto start_processing;
    }
#endif

    if ((ret = mbedtls_ssl_read_record(ssl, 1)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
        return ret;
    }

    if (ssl->in_msgtype != MBEDTLS_SSL_MSG_HANDSHAKE) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE);
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    /*
     * ServerKeyExchange may be skipped with PSK and RSA-PSK when the server
     * doesn't use a psk_identity_hint
     */
    if (ssl->in_msg[0] != MBEDTLS_SSL_HS_SERVER_KEY_EXCHANGE) {
        if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_PSK ||
            ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_RSA_PSK) {
            /* Current message is probably either
             * CertificateRequest or ServerHelloDone */
            ssl->keep_current_message = 1;
            goto exit;
        }

        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("server key exchange message must not be skipped"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE);

        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
    if (ssl->handshake->ecrs_enabled) {
        ssl->handshake->ecrs_state = ssl_ecrs_ske_start_processing;
    }

start_processing:
#endif
    p   = ssl->in_msg + mbedtls_ssl_hs_hdr_len(ssl);
    end = ssl->in_msg + ssl->in_hslen;
    MBEDTLS_SSL_DEBUG_BUF(3,   "server key exchange", p, (size_t) (end - p));

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_DHE_PSK ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_PSK) {
        if (ssl_parse_server_psk_hint(ssl, &p, end) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server key exchange message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
            return MBEDTLS_ERR_SSL_DECODE_ERROR;
        }
    } /* FALLTHROUGH */
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_PSK_ENABLED) ||                       \
    defined(MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_RSA_PSK) {
        ; /* nothing more to do */
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_PSK_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                       \
    defined(MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_DHE_RSA ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_DHE_PSK) {
        if (ssl_parse_server_dh_params(ssl, &p, end) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server key exchange message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
            return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
        }
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||     \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED) ||     \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_PSK ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA) {
        if (ssl_parse_server_ecdh_params(ssl, &p, end) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server key exchange message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
            return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
        }
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECJPAKE) {
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        /*
         * The first 3 bytes are:
         * [0] MBEDTLS_ECP_TLS_NAMED_CURVE
         * [1, 2] elliptic curve's TLS ID
         *
         * However since we only support secp256r1 for now, we check only
         * that TLS ID here
         */
        uint16_t read_tls_id = MBEDTLS_GET_UINT16_BE(p, 1);
        uint16_t exp_tls_id = mbedtls_ssl_get_tls_id_from_ecp_group_id(
            MBEDTLS_ECP_DP_SECP256R1);

        if (exp_tls_id == 0) {
            return MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
        }

        if ((*p != MBEDTLS_ECP_TLS_NAMED_CURVE) ||
            (read_tls_id != exp_tls_id)) {
            return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
        }

        p += 3;

        if ((ret = mbedtls_psa_ecjpake_read_round(
                 &ssl->handshake->psa_pake_ctx, p, end - p,
                 MBEDTLS_ECJPAKE_ROUND_TWO)) != 0) {
            psa_destroy_key(ssl->handshake->psa_pake_password);
            psa_pake_abort(&ssl->handshake->psa_pake_ctx);

            MBEDTLS_SSL_DEBUG_RET(1, "psa_pake_input round two", ret);
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
            return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
        }
#else
        ret = mbedtls_ecjpake_read_round_two(&ssl->handshake->ecjpake_ctx,
                                             p, end - p);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ecjpake_read_round_two", ret);
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
            return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */
    {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

#if defined(MBEDTLS_KEY_EXCHANGE_WITH_SERVER_SIGNATURE_ENABLED)
    if (mbedtls_ssl_ciphersuite_uses_server_signature(ciphersuite_info)) {
        size_t sig_len, hashlen;
        unsigned char hash[MBEDTLS_MD_MAX_SIZE];

        mbedtls_md_type_t md_alg = MBEDTLS_MD_NONE;
        mbedtls_pk_type_t pk_alg = MBEDTLS_PK_NONE;
        unsigned char *params = ssl->in_msg + mbedtls_ssl_hs_hdr_len(ssl);
        size_t params_len = (size_t) (p - params);
        void *rs_ctx = NULL;
        uint16_t sig_alg;

        mbedtls_pk_context *peer_pk;

#if !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
        peer_pk = &ssl->handshake->peer_pubkey;
#else /* !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
        if (ssl->session_negotiate->peer_cert == NULL) {
            /* Should never happen */
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }
        peer_pk = &ssl->session_negotiate->peer_cert->pk;
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */

        /*
         * Handle the digitally-signed structure
         */
        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 2);
        sig_alg = MBEDTLS_GET_UINT16_BE(p, 0);
        if (mbedtls_ssl_get_pk_type_and_md_alg_from_sig_alg(
                sig_alg, &pk_alg, &md_alg) != 0 &&
            !mbedtls_ssl_sig_alg_is_offered(ssl, sig_alg) &&
            !mbedtls_ssl_sig_alg_is_supported(ssl, sig_alg)) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("bad server key exchange message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
            return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
        }
        p += 2;

        if (!mbedtls_pk_can_do(peer_pk, pk_alg)) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("bad server key exchange message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER);
            return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
        }

        /*
         * Read signature
         */

        if (p > end - 2) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server key exchange message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
            return MBEDTLS_ERR_SSL_DECODE_ERROR;
        }
        sig_len = MBEDTLS_GET_UINT16_BE(p, 0);
        p += 2;

        if (p != end - sig_len) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server key exchange message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
            return MBEDTLS_ERR_SSL_DECODE_ERROR;
        }

        MBEDTLS_SSL_DEBUG_BUF(3, "signature", p, sig_len);

        /*
         * Compute the hash that has been signed
         */
        if (md_alg != MBEDTLS_MD_NONE) {
            ret = mbedtls_ssl_get_key_exchange_md_tls1_2(ssl, hash, &hashlen,
                                                         params, params_len,
                                                         md_alg);
            if (ret != 0) {
                return ret;
            }
        } else {
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        MBEDTLS_SSL_DEBUG_BUF(3, "parameters hash", hash, hashlen);

        /*
         * Verify signature
         */
        if (!mbedtls_pk_can_do(peer_pk, pk_alg)) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad server key exchange message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
            return MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH;
        }

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
        if (ssl->handshake->ecrs_enabled) {
            rs_ctx = &ssl->handshake->ecrs_ctx.pk;
        }
#endif

#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT)
        if (pk_alg == MBEDTLS_PK_RSASSA_PSS) {
            mbedtls_pk_rsassa_pss_options rsassa_pss_options;
            rsassa_pss_options.mgf1_hash_id = md_alg;
            rsassa_pss_options.expected_salt_len =
                mbedtls_md_get_size_from_type(md_alg);
            if (rsassa_pss_options.expected_salt_len == 0) {
                return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
            }

            ret = mbedtls_pk_verify_ext(pk_alg, &rsassa_pss_options,
                                        peer_pk,
                                        md_alg, hash, hashlen,
                                        p, sig_len);
        } else
#endif /* MBEDTLS_X509_RSASSA_PSS_SUPPORT */
        ret = mbedtls_pk_verify_restartable(peer_pk,
                                            md_alg, hash, hashlen, p, sig_len, rs_ctx);

        if (ret != 0) {
            int send_alert_msg = 1;
#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
            send_alert_msg = (ret != MBEDTLS_ERR_ECP_IN_PROGRESS);
#endif
            if (send_alert_msg) {
                mbedtls_ssl_send_alert_message(
                    ssl,
                    MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                    MBEDTLS_SSL_ALERT_MSG_DECRYPT_ERROR);
            }
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_pk_verify", ret);
#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
            if (ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
                ret = MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS;
            }
#endif
            return ret;
        }

#if !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
        /* We don't need the peer's public key anymore. Free it,
         * so that more RAM is available for upcoming expensive
         * operations like ECDHE. */
        mbedtls_pk_free(peer_pk);
#endif /* !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
    }
#endif /* MBEDTLS_KEY_EXCHANGE_WITH_SERVER_SIGNATURE_ENABLED */

exit:
    mbedtls_ssl_handshake_increment_state(ssl);

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse server key exchange"));

    return 0;
}

#if !defined(MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_certificate_request(mbedtls_ssl_context *ssl)
{
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info =
        ssl->handshake->ciphersuite_info;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse certificate request"));

    if (!mbedtls_ssl_ciphersuite_cert_req_allowed(ciphersuite_info)) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("<= skip parse certificate request"));
        mbedtls_ssl_handshake_increment_state(ssl);
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
#else /* MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_certificate_request(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *buf;
    size_t n = 0;
    size_t cert_type_len = 0, dn_len = 0;
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info =
        ssl->handshake->ciphersuite_info;
    size_t sig_alg_len;
#if defined(MBEDTLS_DEBUG_C)
    unsigned char *sig_alg;
    unsigned char *dn;
#endif

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse certificate request"));

    if (!mbedtls_ssl_ciphersuite_cert_req_allowed(ciphersuite_info)) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("<= skip parse certificate request"));
        mbedtls_ssl_handshake_increment_state(ssl);
        return 0;
    }

    if ((ret = mbedtls_ssl_read_record(ssl, 1)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
        return ret;
    }

    if (ssl->in_msgtype != MBEDTLS_SSL_MSG_HANDSHAKE) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad certificate request message"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE);
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    mbedtls_ssl_handshake_increment_state(ssl);
    ssl->handshake->client_auth =
        (ssl->in_msg[0] == MBEDTLS_SSL_HS_CERTIFICATE_REQUEST);

    MBEDTLS_SSL_DEBUG_MSG(3, ("got %s certificate request",
                              ssl->handshake->client_auth ? "a" : "no"));

    if (ssl->handshake->client_auth == 0) {
        /* Current message is probably the ServerHelloDone */
        ssl->keep_current_message = 1;
        goto exit;
    }

    /*
     *  struct {
     *      ClientCertificateType certificate_types<1..2^8-1>;
     *      SignatureAndHashAlgorithm
     *        supported_signature_algorithms<2^16-1>; -- TLS 1.2 only
     *      DistinguishedName certificate_authorities<0..2^16-1>;
     *  } CertificateRequest;
     *
     *  Since we only support a single certificate on clients, let's just
     *  ignore all the information that's supposed to help us pick a
     *  certificate.
     *
     *  We could check that our certificate matches the request, and bail out
     *  if it doesn't, but it's simpler to just send the certificate anyway,
     *  and give the server the opportunity to decide if it should terminate
     *  the connection when it doesn't like our certificate.
     *
     *  Same goes for the hash in TLS 1.2's signature_algorithms: at this
     *  point we only have one hash available (see comments in
     *  write_certificate_verify), so let's just use what we have.
     *
     *  However, we still minimally parse the message to check it is at least
     *  superficially sane.
     */
    buf = ssl->in_msg;

    /* certificate_types */
    if (ssl->in_hslen <= mbedtls_ssl_hs_hdr_len(ssl)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad certificate request message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }
    cert_type_len = buf[mbedtls_ssl_hs_hdr_len(ssl)];
    n = cert_type_len;

    /*
     * In the subsequent code there are two paths that read from buf:
     *     * the length of the signature algorithms field (if minor version of
     *       SSL is 3),
     *     * distinguished name length otherwise.
     * Both reach at most the index:
     *    ...hdr_len + 2 + n,
     * therefore the buffer length at this point must be greater than that
     * regardless of the actual code path.
     */
    if (ssl->in_hslen <= mbedtls_ssl_hs_hdr_len(ssl) + 2 + n) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad certificate request message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /* supported_signature_algorithms */
    sig_alg_len = MBEDTLS_GET_UINT16_BE(buf, mbedtls_ssl_hs_hdr_len(ssl) + 1 + n);

    /*
     * The furthest access in buf is in the loop few lines below:
     *     sig_alg[i + 1],
     * where:
     *     sig_alg = buf + ...hdr_len + 3 + n,
     *     max(i) = sig_alg_len - 1.
     * Therefore the furthest access is:
     *     buf[...hdr_len + 3 + n + sig_alg_len - 1 + 1],
     * which reduces to:
     *     buf[...hdr_len + 3 + n + sig_alg_len],
     * which is one less than we need the buf to be.
     */
    if (ssl->in_hslen <= mbedtls_ssl_hs_hdr_len(ssl) + 3 + n + sig_alg_len) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad certificate request message"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

#if defined(MBEDTLS_DEBUG_C)
    sig_alg = buf + mbedtls_ssl_hs_hdr_len(ssl) + 3 + n;
    for (size_t i = 0; i < sig_alg_len; i += 2) {
        MBEDTLS_SSL_DEBUG_MSG(3,
                              ("Supported Signature Algorithm found: %02x %02x",
                               sig_alg[i], sig_alg[i + 1]));
    }
#endif

    n += 2 + sig_alg_len;

    /* certificate_authorities */
    dn_len = MBEDTLS_GET_UINT16_BE(buf, mbedtls_ssl_hs_hdr_len(ssl) + 1 + n);

    n += dn_len;
    if (ssl->in_hslen != mbedtls_ssl_hs_hdr_len(ssl) + 3 + n) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad certificate request message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

#if defined(MBEDTLS_DEBUG_C)
    dn = buf + mbedtls_ssl_hs_hdr_len(ssl) + 3 + n - dn_len;
    for (size_t i = 0, dni_len = 0; i < dn_len; i += 2 + dni_len) {
        unsigned char *p = dn + i + 2;
        mbedtls_x509_name name;
        size_t asn1_len;
        char s[MBEDTLS_X509_MAX_DN_NAME_SIZE];
        memset(&name, 0, sizeof(name));
        dni_len = MBEDTLS_GET_UINT16_BE(dn + i, 0);
        if (dni_len > dn_len - i - 2 ||
            mbedtls_asn1_get_tag(&p, p + dni_len, &asn1_len,
                                 MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) != 0 ||
            mbedtls_x509_get_name(&p, p + asn1_len, &name) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad certificate request message"));
            mbedtls_ssl_send_alert_message(
                ssl,
                MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
            return MBEDTLS_ERR_SSL_DECODE_ERROR;
        }
        MBEDTLS_SSL_DEBUG_MSG(3,
                              ("DN hint: %.*s",
                               mbedtls_x509_dn_gets(s, sizeof(s), &name), s));
        mbedtls_asn1_free_named_data_list_shallow(name.next);
    }
#endif

exit:
    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse certificate request"));

    return 0;
}
#endif /* MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_server_hello_done(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse server hello done"));

    if ((ret = mbedtls_ssl_read_record(ssl, 1)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
        return ret;
    }

    if (ssl->in_msgtype != MBEDTLS_SSL_MSG_HANDSHAKE) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello done message"));
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    if (ssl->in_hslen  != mbedtls_ssl_hs_hdr_len(ssl) ||
        ssl->in_msg[0] != MBEDTLS_SSL_HS_SERVER_HELLO_DONE) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad server hello done message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    mbedtls_ssl_handshake_increment_state(ssl);

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        mbedtls_ssl_recv_flight_completed(ssl);
    }
#endif

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse server hello done"));

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_client_key_exchange(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    size_t header_len;
    size_t content_len;
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info =
        ssl->handshake->ciphersuite_info;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write client key exchange"));

#if defined(MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_DHE_RSA) {
        /*
         * DHM key exchange -- send G^X mod P
         */
        content_len = mbedtls_dhm_get_len(&ssl->handshake->dhm_ctx);

        MBEDTLS_PUT_UINT16_BE(content_len, ssl->out_msg, 4);
        header_len = 6;

        ret = mbedtls_dhm_make_public(&ssl->handshake->dhm_ctx,
                                      (int) mbedtls_dhm_get_len(&ssl->handshake->dhm_ctx),
                                      &ssl->out_msg[header_len], content_len,
                                      ssl->conf->f_rng, ssl->conf->p_rng);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_dhm_make_public", ret);
            return ret;
        }

        MBEDTLS_SSL_DEBUG_MPI(3, "DHM: X ", &ssl->handshake->dhm_ctx.X);
        MBEDTLS_SSL_DEBUG_MPI(3, "DHM: GX", &ssl->handshake->dhm_ctx.GX);

        if ((ret = mbedtls_dhm_calc_secret(&ssl->handshake->dhm_ctx,
                                           ssl->handshake->premaster,
                                           MBEDTLS_PREMASTER_SIZE,
                                           &ssl->handshake->pmslen,
                                           ssl->conf->f_rng, ssl->conf->p_rng)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_dhm_calc_secret", ret);
            return ret;
        }

        MBEDTLS_SSL_DEBUG_MPI(3, "DHM: K ", &ssl->handshake->dhm_ctx.K);
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) ||                   \
    defined(MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED) ||                      \
    defined(MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDH_RSA ||
        ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA) {
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
        psa_status_t destruction_status = PSA_ERROR_CORRUPTION_DETECTED;
        psa_key_attributes_t key_attributes;

        mbedtls_ssl_handshake_params *handshake = ssl->handshake;

        header_len = 4;

        MBEDTLS_SSL_DEBUG_MSG(1, ("Perform PSA-based ECDH computation."));

        /*
         * Generate EC private key for ECDHE exchange.
         */

        /* The master secret is obtained from the shared ECDH secret by
         * applying the TLS 1.2 PRF with a specific salt and label. While
         * the PSA Crypto API encourages combining key agreement schemes
         * such as ECDH with fixed KDFs such as TLS 1.2 PRF, it does not
         * yet support the provisioning of salt + label to the KDF.
         * For the time being, we therefore need to split the computation
         * of the ECDH secret and the application of the TLS 1.2 PRF. */
        key_attributes = psa_key_attributes_init();
        psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE);
        psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
        psa_set_key_type(&key_attributes, handshake->xxdh_psa_type);
        psa_set_key_bits(&key_attributes, handshake->xxdh_psa_bits);

        /* Generate ECDH private key. */
        status = psa_generate_key(&key_attributes,
                                  &handshake->xxdh_psa_privkey);
        if (status != PSA_SUCCESS) {
            return MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
        }

        /* Export the public part of the ECDH private key from PSA.
         * The export format is an ECPoint structure as expected by TLS,
         * but we just need to add a length byte before that. */
        unsigned char *own_pubkey = ssl->out_msg + header_len + 1;
        unsigned char *end = ssl->out_msg + MBEDTLS_SSL_OUT_CONTENT_LEN;
        size_t own_pubkey_max_len = (size_t) (end - own_pubkey);
        size_t own_pubkey_len;

        status = psa_export_public_key(handshake->xxdh_psa_privkey,
                                       own_pubkey, own_pubkey_max_len,
                                       &own_pubkey_len);
        if (status != PSA_SUCCESS) {
            psa_destroy_key(handshake->xxdh_psa_privkey);
            handshake->xxdh_psa_privkey = MBEDTLS_SVC_KEY_ID_INIT;
            return MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
        }

        ssl->out_msg[header_len] = (unsigned char) own_pubkey_len;
        content_len = own_pubkey_len + 1;

        /* The ECDH secret is the premaster secret used for key derivation. */

        /* Compute ECDH shared secret. */
        status = psa_raw_key_agreement(PSA_ALG_ECDH,
                                       handshake->xxdh_psa_privkey,
                                       handshake->xxdh_psa_peerkey,
                                       handshake->xxdh_psa_peerkey_len,
                                       ssl->handshake->premaster,
                                       sizeof(ssl->handshake->premaster),
                                       &ssl->handshake->pmslen);

        destruction_status = psa_destroy_key(handshake->xxdh_psa_privkey);
        handshake->xxdh_psa_privkey = MBEDTLS_SVC_KEY_ID_INIT;

        if (status != PSA_SUCCESS || destruction_status != PSA_SUCCESS) {
            return MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
        }
#else
        /*
         * ECDH key exchange -- send client public value
         */
        header_len = 4;

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
        if (ssl->handshake->ecrs_enabled) {
            if (ssl->handshake->ecrs_state == ssl_ecrs_cke_ecdh_calc_secret) {
                goto ecdh_calc_secret;
            }

            mbedtls_ecdh_enable_restart(&ssl->handshake->ecdh_ctx);
        }
#endif

        ret = mbedtls_ecdh_make_public(&ssl->handshake->ecdh_ctx,
                                       &content_len,
                                       &ssl->out_msg[header_len], 1000,
                                       ssl->conf->f_rng, ssl->conf->p_rng);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ecdh_make_public", ret);
#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
            if (ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
                ret = MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS;
            }
#endif
            return ret;
        }

        MBEDTLS_SSL_DEBUG_ECDH(3, &ssl->handshake->ecdh_ctx,
                               MBEDTLS_DEBUG_ECDH_Q);

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
        if (ssl->handshake->ecrs_enabled) {
            ssl->handshake->ecrs_n = content_len;
            ssl->handshake->ecrs_state = ssl_ecrs_cke_ecdh_calc_secret;
        }

ecdh_calc_secret:
        if (ssl->handshake->ecrs_enabled) {
            content_len = ssl->handshake->ecrs_n;
        }
#endif
        if ((ret = mbedtls_ecdh_calc_secret(&ssl->handshake->ecdh_ctx,
                                            &ssl->handshake->pmslen,
                                            ssl->handshake->premaster,
                                            MBEDTLS_MPI_MAX_SIZE,
                                            ssl->conf->f_rng, ssl->conf->p_rng)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ecdh_calc_secret", ret);
#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
            if (ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
                ret = MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS;
            }
#endif
            return ret;
        }

        MBEDTLS_SSL_DEBUG_ECDH(3, &ssl->handshake->ecdh_ctx,
                               MBEDTLS_DEBUG_ECDH_Z);
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED ||
          MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED */
#if defined(MBEDTLS_USE_PSA_CRYPTO) &&                           \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_PSK) {
        psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
        psa_status_t destruction_status = PSA_ERROR_CORRUPTION_DETECTED;
        psa_key_attributes_t key_attributes;

        mbedtls_ssl_handshake_params *handshake = ssl->handshake;

        /*
         * opaque psk_identity<0..2^16-1>;
         */
        if (mbedtls_ssl_conf_has_static_psk(ssl->conf) == 0) {
            /* We don't offer PSK suites if we don't have a PSK,
             * and we check that the server's choice is among the
             * ciphersuites we offered, so this should never happen. */
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        /* uint16 to store content length */
        const size_t content_len_size = 2;

        header_len = 4;

        if (header_len + content_len_size + ssl->conf->psk_identity_len
            > MBEDTLS_SSL_OUT_CONTENT_LEN) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("psk identity too long or SSL buffer too short"));
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        }

        unsigned char *p = ssl->out_msg + header_len;

        *p++ = MBEDTLS_BYTE_1(ssl->conf->psk_identity_len);
        *p++ = MBEDTLS_BYTE_0(ssl->conf->psk_identity_len);
        header_len += content_len_size;

        memcpy(p, ssl->conf->psk_identity,
               ssl->conf->psk_identity_len);
        p += ssl->conf->psk_identity_len;

        header_len += ssl->conf->psk_identity_len;

        MBEDTLS_SSL_DEBUG_MSG(1, ("Perform PSA-based ECDH computation."));

        /*
         * Generate EC private key for ECDHE exchange.
         */

        /* The master secret is obtained from the shared ECDH secret by
         * applying the TLS 1.2 PRF with a specific salt and label. While
         * the PSA Crypto API encourages combining key agreement schemes
         * such as ECDH with fixed KDFs such as TLS 1.2 PRF, it does not
         * yet support the provisioning of salt + label to the KDF.
         * For the time being, we therefore need to split the computation
         * of the ECDH secret and the application of the TLS 1.2 PRF. */
        key_attributes = psa_key_attributes_init();
        psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE);
        psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
        psa_set_key_type(&key_attributes, handshake->xxdh_psa_type);
        psa_set_key_bits(&key_attributes, handshake->xxdh_psa_bits);

        /* Generate ECDH private key. */
        status = psa_generate_key(&key_attributes,
                                  &handshake->xxdh_psa_privkey);
        if (status != PSA_SUCCESS) {
            return PSA_TO_MBEDTLS_ERR(status);
        }

        /* Export the public part of the ECDH private key from PSA.
         * The export format is an ECPoint structure as expected by TLS,
         * but we just need to add a length byte before that. */
        unsigned char *own_pubkey = p + 1;
        unsigned char *end = ssl->out_msg + MBEDTLS_SSL_OUT_CONTENT_LEN;
        size_t own_pubkey_max_len = (size_t) (end - own_pubkey);
        size_t own_pubkey_len = 0;

        status = psa_export_public_key(handshake->xxdh_psa_privkey,
                                       own_pubkey, own_pubkey_max_len,
                                       &own_pubkey_len);
        if (status != PSA_SUCCESS) {
            psa_destroy_key(handshake->xxdh_psa_privkey);
            handshake->xxdh_psa_privkey = MBEDTLS_SVC_KEY_ID_INIT;
            return PSA_TO_MBEDTLS_ERR(status);
        }

        *p = (unsigned char) own_pubkey_len;
        content_len = own_pubkey_len + 1;

        /* As RFC 5489 section 2, the premaster secret is formed as follows:
         * - a uint16 containing the length (in octets) of the ECDH computation
         * - the octet string produced by the ECDH computation
         * - a uint16 containing the length (in octets) of the PSK
         * - the PSK itself
         */
        unsigned char *pms = ssl->handshake->premaster;
        const unsigned char * const pms_end = pms +
                                              sizeof(ssl->handshake->premaster);
        /* uint16 to store length (in octets) of the ECDH computation */
        const size_t zlen_size = 2;
        size_t zlen = 0;

        /* Perform ECDH computation after the uint16 reserved for the length */
        status = psa_raw_key_agreement(PSA_ALG_ECDH,
                                       handshake->xxdh_psa_privkey,
                                       handshake->xxdh_psa_peerkey,
                                       handshake->xxdh_psa_peerkey_len,
                                       pms + zlen_size,
                                       pms_end - (pms + zlen_size),
                                       &zlen);

        destruction_status = psa_destroy_key(handshake->xxdh_psa_privkey);
        handshake->xxdh_psa_privkey = MBEDTLS_SVC_KEY_ID_INIT;

        if (status != PSA_SUCCESS) {
            return PSA_TO_MBEDTLS_ERR(status);
        } else if (destruction_status != PSA_SUCCESS) {
            return PSA_TO_MBEDTLS_ERR(destruction_status);
        }

        /* Write the ECDH computation length before the ECDH computation */
        MBEDTLS_PUT_UINT16_BE(zlen, pms, 0);
        pms += zlen_size + zlen;
    } else
#endif /* MBEDTLS_USE_PSA_CRYPTO &&
          MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
    if (mbedtls_ssl_ciphersuite_uses_psk(ciphersuite_info)) {
        /*
         * opaque psk_identity<0..2^16-1>;
         */
        if (mbedtls_ssl_conf_has_static_psk(ssl->conf) == 0) {
            /* We don't offer PSK suites if we don't have a PSK,
             * and we check that the server's choice is among the
             * ciphersuites we offered, so this should never happen. */
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        header_len = 4;
        content_len = ssl->conf->psk_identity_len;

        if (header_len + 2 + content_len > MBEDTLS_SSL_OUT_CONTENT_LEN) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("psk identity too long or SSL buffer too short"));
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        }

        ssl->out_msg[header_len++] = MBEDTLS_BYTE_1(content_len);
        ssl->out_msg[header_len++] = MBEDTLS_BYTE_0(content_len);

        memcpy(ssl->out_msg + header_len,
               ssl->conf->psk_identity,
               ssl->conf->psk_identity_len);
        header_len += ssl->conf->psk_identity_len;

#if defined(MBEDTLS_KEY_EXCHANGE_PSK_ENABLED)
        if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_PSK) {
            content_len = 0;
        } else
#endif
#if defined(MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED)
        if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_RSA_PSK) {
            if ((ret = ssl_write_encrypted_pms(ssl, header_len,
                                               &content_len, 2)) != 0) {
                return ret;
            }
        } else
#endif
#if defined(MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED)
        if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_DHE_PSK) {
            /*
             * ClientDiffieHellmanPublic public (DHM send G^X mod P)
             */
            content_len = mbedtls_dhm_get_len(&ssl->handshake->dhm_ctx);

            if (header_len + 2 + content_len >
                MBEDTLS_SSL_OUT_CONTENT_LEN) {
                MBEDTLS_SSL_DEBUG_MSG(1,
                                      ("psk identity or DHM size too long or SSL buffer too short"));
                return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
            }

            ssl->out_msg[header_len++] = MBEDTLS_BYTE_1(content_len);
            ssl->out_msg[header_len++] = MBEDTLS_BYTE_0(content_len);

            ret = mbedtls_dhm_make_public(&ssl->handshake->dhm_ctx,
                                          (int) mbedtls_dhm_get_len(&ssl->handshake->dhm_ctx),
                                          &ssl->out_msg[header_len], content_len,
                                          ssl->conf->f_rng, ssl->conf->p_rng);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_dhm_make_public", ret);
                return ret;
            }

#if defined(MBEDTLS_USE_PSA_CRYPTO)
            unsigned char *pms = ssl->handshake->premaster;
            unsigned char *pms_end = pms + sizeof(ssl->handshake->premaster);
            size_t pms_len;

            /* Write length only when we know the actual value */
            if ((ret = mbedtls_dhm_calc_secret(&ssl->handshake->dhm_ctx,
                                               pms + 2, pms_end - (pms + 2), &pms_len,
                                               ssl->conf->f_rng, ssl->conf->p_rng)) != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_dhm_calc_secret", ret);
                return ret;
            }
            MBEDTLS_PUT_UINT16_BE(pms_len, pms, 0);
            pms += 2 + pms_len;

            MBEDTLS_SSL_DEBUG_MPI(3, "DHM: K ", &ssl->handshake->dhm_ctx.K);
#endif
        } else
#endif /* MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED */
#if !defined(MBEDTLS_USE_PSA_CRYPTO) &&                             \
        defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
        if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECDHE_PSK) {
            /*
             * ClientECDiffieHellmanPublic public;
             */
            ret = mbedtls_ecdh_make_public(&ssl->handshake->ecdh_ctx,
                                           &content_len,
                                           &ssl->out_msg[header_len],
                                           MBEDTLS_SSL_OUT_CONTENT_LEN - header_len,
                                           ssl->conf->f_rng, ssl->conf->p_rng);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ecdh_make_public", ret);
                return ret;
            }

            MBEDTLS_SSL_DEBUG_ECDH(3, &ssl->handshake->ecdh_ctx,
                                   MBEDTLS_DEBUG_ECDH_Q);
        } else
#endif /* !MBEDTLS_USE_PSA_CRYPTO && MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED */
        {
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

#if !defined(MBEDTLS_USE_PSA_CRYPTO)
        if ((ret = mbedtls_ssl_psk_derive_premaster(ssl,
                                                    (mbedtls_key_exchange_type_t) ciphersuite_info->
                                                    key_exchange)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1,
                                  "mbedtls_ssl_psk_derive_premaster", ret);
            return ret;
        }
#endif /* !MBEDTLS_USE_PSA_CRYPTO */
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_RSA_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_RSA) {
        header_len = 4;
        if ((ret = ssl_write_encrypted_pms(ssl, header_len,
                                           &content_len, 0)) != 0) {
            return ret;
        }
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_RSA_ENABLED */
#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
    if (ciphersuite_info->key_exchange == MBEDTLS_KEY_EXCHANGE_ECJPAKE) {
        header_len = 4;

#if defined(MBEDTLS_USE_PSA_CRYPTO)
        unsigned char *out_p = ssl->out_msg + header_len;
        unsigned char *end_p = ssl->out_msg + MBEDTLS_SSL_OUT_CONTENT_LEN -
                               header_len;
        ret = mbedtls_psa_ecjpake_write_round(&ssl->handshake->psa_pake_ctx,
                                              out_p, end_p - out_p, &content_len,
                                              MBEDTLS_ECJPAKE_ROUND_TWO);
        if (ret != 0) {
            psa_destroy_key(ssl->handshake->psa_pake_password);
            psa_pake_abort(&ssl->handshake->psa_pake_ctx);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_pake_output", ret);
            return ret;
        }
#else
        ret = mbedtls_ecjpake_write_round_two(&ssl->handshake->ecjpake_ctx,
                                              ssl->out_msg + header_len,
                                              MBEDTLS_SSL_OUT_CONTENT_LEN - header_len,
                                              &content_len,
                                              ssl->conf->f_rng, ssl->conf->p_rng);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ecjpake_write_round_two", ret);
            return ret;
        }

        ret = mbedtls_ecjpake_derive_secret(&ssl->handshake->ecjpake_ctx,
                                            ssl->handshake->premaster, 32, &ssl->handshake->pmslen,
                                            ssl->conf->f_rng, ssl->conf->p_rng);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ecjpake_derive_secret", ret);
            return ret;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    } else
#endif /* MBEDTLS_KEY_EXCHANGE_RSA_ENABLED */
    {
        ((void) ciphersuite_info);
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    ssl->out_msglen  = header_len + content_len;
    ssl->out_msgtype = MBEDTLS_SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = MBEDTLS_SSL_HS_CLIENT_KEY_EXCHANGE;

    mbedtls_ssl_handshake_increment_state(ssl);

    if ((ret = mbedtls_ssl_write_handshake_msg(ssl)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_write_handshake_msg", ret);
        return ret;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write client key exchange"));

    return 0;
}

#if !defined(MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_certificate_verify(mbedtls_ssl_context *ssl)
{
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info =
        ssl->handshake->ciphersuite_info;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write certificate verify"));

    if ((ret = mbedtls_ssl_derive_keys(ssl)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_derive_keys", ret);
        return ret;
    }

    if (!mbedtls_ssl_ciphersuite_cert_req_allowed(ciphersuite_info)) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("<= skip write certificate verify"));
        mbedtls_ssl_handshake_increment_state(ssl);
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
#else /* !MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_certificate_verify(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info =
        ssl->handshake->ciphersuite_info;
    size_t n = 0, offset = 0;
    unsigned char hash[48];
    unsigned char *hash_start = hash;
    mbedtls_md_type_t md_alg = MBEDTLS_MD_NONE;
    size_t hashlen;
    void *rs_ctx = NULL;
#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
    size_t out_buf_len = ssl->out_buf_len - (size_t) (ssl->out_msg - ssl->out_buf);
#else
    size_t out_buf_len = MBEDTLS_SSL_OUT_BUFFER_LEN - (size_t) (ssl->out_msg - ssl->out_buf);
#endif

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write certificate verify"));

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
    if (ssl->handshake->ecrs_enabled &&
        ssl->handshake->ecrs_state == ssl_ecrs_crt_vrfy_sign) {
        goto sign;
    }
#endif

    if ((ret = mbedtls_ssl_derive_keys(ssl)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_derive_keys", ret);
        return ret;
    }

    if (!mbedtls_ssl_ciphersuite_cert_req_allowed(ciphersuite_info)) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("<= skip write certificate verify"));
        mbedtls_ssl_handshake_increment_state(ssl);
        return 0;
    }

    if (ssl->handshake->client_auth == 0 ||
        mbedtls_ssl_own_cert(ssl) == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("<= skip write certificate verify"));
        mbedtls_ssl_handshake_increment_state(ssl);
        return 0;
    }

    if (mbedtls_ssl_own_key(ssl) == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("got no private key for certificate"));
        return MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED;
    }

    /*
     * Make a signature of the handshake digests
     */
#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
    if (ssl->handshake->ecrs_enabled) {
        ssl->handshake->ecrs_state = ssl_ecrs_crt_vrfy_sign;
    }

sign:
#endif

    ret = ssl->handshake->calc_verify(ssl, hash, &hashlen);
    if (0 != ret) {
        MBEDTLS_SSL_DEBUG_RET(1, ("calc_verify"), ret);
        return ret;
    }

    /*
     * digitally-signed struct {
     *     opaque handshake_messages[handshake_messages_length];
     * };
     *
     * Taking shortcut here. We assume that the server always allows the
     * PRF Hash function and has sent it in the allowed signature
     * algorithms list received in the Certificate Request message.
     *
     * Until we encounter a server that does not, we will take this
     * shortcut.
     *
     * Reason: Otherwise we should have running hashes for SHA512 and
     *         SHA224 in order to satisfy 'weird' needs from the server
     *         side.
     */
    if (ssl->handshake->ciphersuite_info->mac == MBEDTLS_MD_SHA384) {
        md_alg = MBEDTLS_MD_SHA384;
        ssl->out_msg[4] = MBEDTLS_SSL_HASH_SHA384;
    } else {
        md_alg = MBEDTLS_MD_SHA256;
        ssl->out_msg[4] = MBEDTLS_SSL_HASH_SHA256;
    }
    ssl->out_msg[5] = mbedtls_ssl_sig_from_pk(mbedtls_ssl_own_key(ssl));

    /* Info from md_alg will be used instead */
    hashlen = 0;
    offset = 2;

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
    if (ssl->handshake->ecrs_enabled) {
        rs_ctx = &ssl->handshake->ecrs_ctx.pk;
    }
#endif

    if ((ret = mbedtls_pk_sign_restartable(mbedtls_ssl_own_key(ssl),
                                           md_alg, hash_start, hashlen,
                                           ssl->out_msg + 6 + offset,
                                           out_buf_len - 6 - offset,
                                           &n,
                                           ssl->conf->f_rng, ssl->conf->p_rng, rs_ctx)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_pk_sign", ret);
#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
        if (ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
            ret = MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS;
        }
#endif
        return ret;
    }

    MBEDTLS_PUT_UINT16_BE(n, ssl->out_msg, offset + 4);

    ssl->out_msglen  = 6 + n + offset;
    ssl->out_msgtype = MBEDTLS_SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = MBEDTLS_SSL_HS_CERTIFICATE_VERIFY;

    mbedtls_ssl_handshake_increment_state(ssl);

    if ((ret = mbedtls_ssl_write_handshake_msg(ssl)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_write_handshake_msg", ret);
        return ret;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write certificate verify"));

    return ret;
}
#endif /* MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED */

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_new_session_ticket(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    uint32_t lifetime;
    size_t ticket_len;
    unsigned char *ticket;
    const unsigned char *msg;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse new session ticket"));

    if ((ret = mbedtls_ssl_read_record(ssl, 1)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
        return ret;
    }

    if (ssl->in_msgtype != MBEDTLS_SSL_MSG_HANDSHAKE) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad new session ticket message"));
        mbedtls_ssl_send_alert_message(
            ssl,
            MBEDTLS_SSL_ALERT_LEVEL_FATAL,
            MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE);
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    /*
     * struct {
     *     uint32 ticket_lifetime_hint;
     *     opaque ticket<0..2^16-1>;
     * } NewSessionTicket;
     *
     * 0  .  3   ticket_lifetime_hint
     * 4  .  5   ticket_len (n)
     * 6  .  5+n ticket content
     */
    if (ssl->in_msg[0] != MBEDTLS_SSL_HS_NEW_SESSION_TICKET ||
        ssl->in_hslen < 6 + mbedtls_ssl_hs_hdr_len(ssl)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad new session ticket message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    msg = ssl->in_msg + mbedtls_ssl_hs_hdr_len(ssl);

    lifetime = MBEDTLS_GET_UINT32_BE(msg, 0);

    ticket_len = MBEDTLS_GET_UINT16_BE(msg, 4);

    if (ticket_len + 6 + mbedtls_ssl_hs_hdr_len(ssl) != ssl->in_hslen) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad new session ticket message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    MBEDTLS_SSL_DEBUG_MSG(3, ("ticket length: %" MBEDTLS_PRINTF_SIZET, ticket_len));

    /* We're not waiting for a NewSessionTicket message any more */
    ssl->handshake->new_session_ticket = 0;
    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_SERVER_CHANGE_CIPHER_SPEC);

    /*
     * Zero-length ticket means the server changed his mind and doesn't want
     * to send a ticket after all, so just forget it
     */
    if (ticket_len == 0) {
        return 0;
    }

    if (ssl->session != NULL && ssl->session->ticket != NULL) {
        mbedtls_zeroize_and_free(ssl->session->ticket,
                                 ssl->session->ticket_len);
        ssl->session->ticket = NULL;
        ssl->session->ticket_len = 0;
    }

    mbedtls_zeroize_and_free(ssl->session_negotiate->ticket,
                             ssl->session_negotiate->ticket_len);
    ssl->session_negotiate->ticket = NULL;
    ssl->session_negotiate->ticket_len = 0;

    if ((ticket = mbedtls_calloc(1, ticket_len)) == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("ticket alloc failed"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR);
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }

    memcpy(ticket, msg + 6, ticket_len);

    ssl->session_negotiate->ticket = ticket;
    ssl->session_negotiate->ticket_len = ticket_len;
    ssl->session_negotiate->ticket_lifetime = lifetime;

    /*
     * RFC 5077 section 3.4:
     * "If the client receives a session ticket from the server, then it
     * discards any Session ID that was sent in the ServerHello."
     */
    MBEDTLS_SSL_DEBUG_MSG(3, ("ticket in use, discarding session id"));
    ssl->session_negotiate->id_len = 0;

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse new session ticket"));

    return 0;
}
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

/*
 * SSL handshake -- client side -- single step
 */
int mbedtls_ssl_handshake_client_step(mbedtls_ssl_context *ssl)
{
    int ret = 0;

    /* Change state now, so that it is right in mbedtls_ssl_read_record(), used
     * by DTLS for dropping out-of-sequence ChangeCipherSpec records */
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
    if (ssl->state == MBEDTLS_SSL_SERVER_CHANGE_CIPHER_SPEC &&
        ssl->handshake->new_session_ticket != 0) {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_NEW_SESSION_TICKET);
    }
#endif

    switch (ssl->state) {
        case MBEDTLS_SSL_HELLO_REQUEST:
            mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CLIENT_HELLO);
            break;

        /*
         *  ==>   ClientHello
         */
        case MBEDTLS_SSL_CLIENT_HELLO:
            ret = mbedtls_ssl_write_client_hello(ssl);
            break;

        /*
         *  <==   ServerHello
         *        Certificate
         *      ( ServerKeyExchange  )
         *      ( CertificateRequest )
         *        ServerHelloDone
         */
        case MBEDTLS_SSL_SERVER_HELLO:
            ret = ssl_parse_server_hello(ssl);
            break;

        case MBEDTLS_SSL_SERVER_CERTIFICATE:
            ret = mbedtls_ssl_parse_certificate(ssl);
            break;

        case MBEDTLS_SSL_SERVER_KEY_EXCHANGE:
            ret = ssl_parse_server_key_exchange(ssl);
            break;

        case MBEDTLS_SSL_CERTIFICATE_REQUEST:
            ret = ssl_parse_certificate_request(ssl);
            break;

        case MBEDTLS_SSL_SERVER_HELLO_DONE:
            ret = ssl_parse_server_hello_done(ssl);
            break;

        /*
         *  ==> ( Certificate/Alert  )
         *        ClientKeyExchange
         *      ( CertificateVerify  )
         *        ChangeCipherSpec
         *        Finished
         */
        case MBEDTLS_SSL_CLIENT_CERTIFICATE:
            ret = mbedtls_ssl_write_certificate(ssl);
            break;

        case MBEDTLS_SSL_CLIENT_KEY_EXCHANGE:
            ret = ssl_write_client_key_exchange(ssl);
            break;

        case MBEDTLS_SSL_CERTIFICATE_VERIFY:
            ret = ssl_write_certificate_verify(ssl);
            break;

        case MBEDTLS_SSL_CLIENT_CHANGE_CIPHER_SPEC:
            ret = mbedtls_ssl_write_change_cipher_spec(ssl);
            break;

        case MBEDTLS_SSL_CLIENT_FINISHED:
            ret = mbedtls_ssl_write_finished(ssl);
            break;

            /*
             *  <==   ( NewSessionTicket )
             *        ChangeCipherSpec
             *        Finished
             */
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
        case MBEDTLS_SSL_NEW_SESSION_TICKET:
            ret = ssl_parse_new_session_ticket(ssl);
            break;
#endif

        case MBEDTLS_SSL_SERVER_CHANGE_CIPHER_SPEC:
            ret = mbedtls_ssl_parse_change_cipher_spec(ssl);
            break;

        case MBEDTLS_SSL_SERVER_FINISHED:
            ret = mbedtls_ssl_parse_finished(ssl);
            break;

        case MBEDTLS_SSL_FLUSH_BUFFERS:
            MBEDTLS_SSL_DEBUG_MSG(2, ("handshake: done"));
            mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_HANDSHAKE_WRAPUP);
            break;

        case MBEDTLS_SSL_HANDSHAKE_WRAPUP:
            mbedtls_ssl_handshake_wrapup(ssl);
            break;

        default:
            MBEDTLS_SSL_DEBUG_MSG(1, ("invalid state %d", ssl->state));
            return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    return ret;
}

#endif /* MBEDTLS_SSL_CLI_C && MBEDTLS_SSL_PROTO_TLS1_2 */

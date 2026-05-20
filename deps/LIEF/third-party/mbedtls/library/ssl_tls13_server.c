/*
 *  TLS 1.3 server-side functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_SSL_SRV_C) && defined(MBEDTLS_SSL_PROTO_TLS1_3)

#include "debug_internal.h"
#include "mbedtls/error.h"
#include "mbedtls/platform.h"
#include "mbedtls/constant_time.h"
#include "mbedtls/oid.h"
#include "mbedtls/psa_util.h"

#include "ssl_misc.h"
#include "ssl_tls13_keys.h"
#include "ssl_debug_helpers.h"


static const mbedtls_ssl_ciphersuite_t *ssl_tls13_validate_peer_ciphersuite(
    mbedtls_ssl_context *ssl,
    unsigned int cipher_suite)
{
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info;
    if (!mbedtls_ssl_tls13_cipher_suite_is_offered(ssl, cipher_suite)) {
        return NULL;
    }

    ciphersuite_info = mbedtls_ssl_ciphersuite_from_id(cipher_suite);
    if ((mbedtls_ssl_validate_ciphersuite(ssl, ciphersuite_info,
                                          ssl->tls_version,
                                          ssl->tls_version) != 0)) {
        return NULL;
    }
    return ciphersuite_info;
}

static void ssl_tls13_select_ciphersuite(
    mbedtls_ssl_context *ssl,
    const unsigned char *cipher_suites,
    const unsigned char *cipher_suites_end,
    int psk_ciphersuite_id,
    psa_algorithm_t psk_hash_alg,
    const mbedtls_ssl_ciphersuite_t **selected_ciphersuite_info)
{
    *selected_ciphersuite_info = NULL;

    /*
     * In a compliant ClientHello the byte-length of the list of ciphersuites
     * is even and this function relies on this fact. This should have been
     * checked in the main ClientHello parsing function. Double check here.
     */
    if ((cipher_suites_end - cipher_suites) & 1) {
        return;
    }

    for (const unsigned char *p = cipher_suites;
         p < cipher_suites_end; p += 2) {
        /*
         * "cipher_suites_end - p is even" is an invariant of the loop. As
         * cipher_suites_end - p > 0, we have cipher_suites_end - p >= 2 and it
         * is thus safe to read two bytes.
         */
        uint16_t id = MBEDTLS_GET_UINT16_BE(p, 0);

        const mbedtls_ssl_ciphersuite_t *info =
            ssl_tls13_validate_peer_ciphersuite(ssl, id);
        if (info == NULL) {
            continue;
        }

        /*
         * If a valid PSK ciphersuite identifier has been passed in, we want
         * an exact match.
         */
        if (psk_ciphersuite_id != 0) {
            if (id != psk_ciphersuite_id) {
                continue;
            }
        } else if (psk_hash_alg != PSA_ALG_NONE) {
            if (mbedtls_md_psa_alg_from_type((mbedtls_md_type_t) info->mac) !=
                psk_hash_alg) {
                continue;
            }
        }

        *selected_ciphersuite_info = info;
        return;
    }

    MBEDTLS_SSL_DEBUG_MSG(1, ("No matched ciphersuite, psk_ciphersuite_id=%x, psk_hash_alg=%lx",
                              (unsigned) psk_ciphersuite_id,
                              (unsigned long) psk_hash_alg));
}

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
/* From RFC 8446:
 *
 *   enum { psk_ke(0), psk_dhe_ke(1), (255) } PskKeyExchangeMode;
 *   struct {
 *       PskKeyExchangeMode ke_modes<1..255>;
 *   } PskKeyExchangeModes;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_key_exchange_modes_ext(mbedtls_ssl_context *ssl,
                                                  const unsigned char *buf,
                                                  const unsigned char *end)
{
    const unsigned char *p = buf;
    size_t ke_modes_len;
    int ke_modes = 0;

    /* Read ke_modes length (1 Byte) */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 1);
    ke_modes_len = *p++;
    /* Currently, there are only two PSK modes, so even without looking
     * at the content, something's wrong if the list has more than 2 items. */
    if (ke_modes_len > 2) {
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER,
                                     MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, ke_modes_len);

    while (ke_modes_len-- != 0) {
        switch (*p++) {
            case MBEDTLS_SSL_TLS1_3_PSK_MODE_PURE:
                ke_modes |= MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK;
                MBEDTLS_SSL_DEBUG_MSG(3, ("Found PSK KEX MODE"));
                break;
            case MBEDTLS_SSL_TLS1_3_PSK_MODE_ECDHE:
                ke_modes |= MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL;
                MBEDTLS_SSL_DEBUG_MSG(3, ("Found PSK_EPHEMERAL KEX MODE"));
                break;
            default:
                MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER,
                                             MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
                return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
        }
    }

    ssl->handshake->tls13_kex_modes = ke_modes;
    return 0;
}

/*
 * Non-error return values of
 * ssl_tls13_offered_psks_check_identity_match_ticket() and
 * ssl_tls13_offered_psks_check_identity_match(). They are positive to
 * not collide with error codes that are negative. Zero
 * (SSL_TLS1_3_PSK_IDENTITY_MATCH) in case of success as it may be propagated
 * up by the callers of this function as a generic success condition.
 *
 * The return value SSL_TLS1_3_PSK_IDENTITY_MATCH_BUT_PSK_NOT_USABLE means
 * that the pre-shared-key identity matches that of a ticket or an externally-
 * provisioned pre-shared-key. We have thus been able to retrieve the
 * attributes of the pre-shared-key but at least one of them does not meet
 * some criteria and the pre-shared-key cannot be used. For example, a ticket
 * is expired or its version is not TLS 1.3. Note eventually that the return
 * value SSL_TLS1_3_PSK_IDENTITY_MATCH_BUT_PSK_NOT_USABLE does not have
 * anything to do with binder check. A binder check is done only when a
 * suitable pre-shared-key has been selected and only for that selected
 * pre-shared-key: if the binder check fails, we fail the handshake and we do
 * not try to find another pre-shared-key for which the binder check would
 * succeed as recommended by the specification.
 */
#define SSL_TLS1_3_PSK_IDENTITY_DOES_NOT_MATCH 2
#define SSL_TLS1_3_PSK_IDENTITY_MATCH_BUT_PSK_NOT_USABLE 1
#define SSL_TLS1_3_PSK_IDENTITY_MATCH 0

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_key_exchange_is_psk_available(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_key_exchange_is_psk_ephemeral_available(mbedtls_ssl_context *ssl);

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_offered_psks_check_identity_match_ticket(
    mbedtls_ssl_context *ssl,
    const unsigned char *identity,
    size_t identity_len,
    uint32_t obfuscated_ticket_age,
    mbedtls_ssl_session *session)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *ticket_buffer;
#if defined(MBEDTLS_HAVE_TIME)
    mbedtls_ms_time_t now;
    mbedtls_ms_time_t server_age;
    uint32_t client_age;
    mbedtls_ms_time_t age_diff;
#endif

    ((void) obfuscated_ticket_age);

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> check_identity_match_ticket"));

    /* Ticket parser is not configured, Skip */
    if (ssl->conf->f_ticket_parse == NULL || identity_len == 0) {
        return SSL_TLS1_3_PSK_IDENTITY_DOES_NOT_MATCH;
    }

    /* We create a copy of the encrypted ticket since the ticket parsing
     * function is allowed to use its input buffer as an output buffer
     * (in-place decryption). We do, however, need the original buffer for
     * computing the PSK binder value.
     */
    ticket_buffer = mbedtls_calloc(1, identity_len);
    if (ticket_buffer == NULL) {
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }
    memcpy(ticket_buffer, identity, identity_len);

    ret = ssl->conf->f_ticket_parse(ssl->conf->p_ticket,
                                    session,
                                    ticket_buffer, identity_len);
    switch (ret) {
        case 0:
            ret = SSL_TLS1_3_PSK_IDENTITY_MATCH;
            break;

        case MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED:
            MBEDTLS_SSL_DEBUG_MSG(3, ("ticket is expired"));
            ret = SSL_TLS1_3_PSK_IDENTITY_MATCH_BUT_PSK_NOT_USABLE;
            break;

        case MBEDTLS_ERR_SSL_INVALID_MAC:
            MBEDTLS_SSL_DEBUG_MSG(3, ("ticket is not authentic"));
            ret = SSL_TLS1_3_PSK_IDENTITY_DOES_NOT_MATCH;
            break;

        default:
            MBEDTLS_SSL_DEBUG_RET(1, "ticket_parse", ret);
            ret = SSL_TLS1_3_PSK_IDENTITY_DOES_NOT_MATCH;
    }

    /* We delete the temporary buffer */
    mbedtls_free(ticket_buffer);

    if (ret != SSL_TLS1_3_PSK_IDENTITY_MATCH) {
        goto exit;
    }

    /*
     * The identity matches that of a ticket. Now check that it has suitable
     * attributes and bet it will not be the case.
     */
    ret = SSL_TLS1_3_PSK_IDENTITY_MATCH_BUT_PSK_NOT_USABLE;

    if (session->tls_version != MBEDTLS_SSL_VERSION_TLS1_3) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("Ticket TLS version is not 1.3."));
        goto exit;
    }

#if defined(MBEDTLS_HAVE_TIME)
    now = mbedtls_ms_time();

    if (now < session->ticket_creation_time) {
        MBEDTLS_SSL_DEBUG_MSG(
            3, ("Invalid ticket creation time ( now = %" MBEDTLS_PRINTF_MS_TIME
                ", creation_time = %" MBEDTLS_PRINTF_MS_TIME " )",
                now, session->ticket_creation_time));
        goto exit;
    }

    server_age = now - session->ticket_creation_time;

    /* RFC 8446 section 4.6.1
     *
     * Servers MUST NOT use any value greater than 604800 seconds (7 days).
     *
     * RFC 8446 section 4.2.11.1
     *
     * Clients MUST NOT attempt to use tickets which have ages greater than
     * the "ticket_lifetime" value which was provided with the ticket.
     *
     */
    if (server_age > MBEDTLS_SSL_TLS1_3_MAX_ALLOWED_TICKET_LIFETIME * 1000) {
        MBEDTLS_SSL_DEBUG_MSG(
            3, ("Ticket age exceeds limitation ticket_age = %" MBEDTLS_PRINTF_MS_TIME,
                server_age));
        goto exit;
    }

    /* RFC 8446 section 4.2.10
     *
     * For PSKs provisioned via NewSessionTicket, a server MUST validate that
     * the ticket age for the selected PSK identity (computed by subtracting
     * ticket_age_add from PskIdentity.obfuscated_ticket_age modulo 2^32) is
     * within a small tolerance of the time since the ticket was issued.
     *
     * NOTE: The typical accuracy of an RTC crystal is ±100 to ±20 parts per
     *       million (360 to 72 milliseconds per hour). Default tolerance
     *       window is 6s, thus in the worst case clients and servers must
     *       sync up their system time every 6000/360/2~=8 hours.
     */
    client_age = obfuscated_ticket_age - session->ticket_age_add;
    age_diff = server_age - (mbedtls_ms_time_t) client_age;
    if (age_diff < -MBEDTLS_SSL_TLS1_3_TICKET_AGE_TOLERANCE ||
        age_diff > MBEDTLS_SSL_TLS1_3_TICKET_AGE_TOLERANCE) {
        MBEDTLS_SSL_DEBUG_MSG(
            3, ("Ticket age outside tolerance window ( diff = %"
                MBEDTLS_PRINTF_MS_TIME ")",
                age_diff));
        goto exit;
    }
#endif /* MBEDTLS_HAVE_TIME */

    /*
     * All good, we have found a suitable ticket.
     */
    ret = SSL_TLS1_3_PSK_IDENTITY_MATCH;

exit:
    if (ret != SSL_TLS1_3_PSK_IDENTITY_MATCH) {
        mbedtls_ssl_session_free(session);
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= check_identity_match_ticket"));
    return ret;
}
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_offered_psks_check_identity_match(
    mbedtls_ssl_context *ssl,
    const unsigned char *identity,
    size_t identity_len,
    uint32_t obfuscated_ticket_age,
    int *psk_type,
    mbedtls_ssl_session *session)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ((void) session);
    ((void) obfuscated_ticket_age);
    *psk_type = MBEDTLS_SSL_TLS1_3_PSK_EXTERNAL;

    MBEDTLS_SSL_DEBUG_BUF(4, "identity", identity, identity_len);

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
    ret = ssl_tls13_offered_psks_check_identity_match_ticket(
        ssl, identity, identity_len, obfuscated_ticket_age, session);
    if (ret == SSL_TLS1_3_PSK_IDENTITY_MATCH) {
        *psk_type = MBEDTLS_SSL_TLS1_3_PSK_RESUMPTION;
        ret = mbedtls_ssl_set_hs_psk(ssl,
                                     session->resumption_key,
                                     session->resumption_key_len);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_set_hs_psk", ret);
            return ret;
        }

        MBEDTLS_SSL_DEBUG_BUF(4, "Ticket-resumed PSK:",
                              session->resumption_key,
                              session->resumption_key_len);
        MBEDTLS_SSL_DEBUG_MSG(4, ("ticket: obfuscated_ticket_age: %u",
                                  (unsigned) obfuscated_ticket_age));
        return SSL_TLS1_3_PSK_IDENTITY_MATCH;
    } else if (ret == SSL_TLS1_3_PSK_IDENTITY_MATCH_BUT_PSK_NOT_USABLE) {
        return SSL_TLS1_3_PSK_IDENTITY_MATCH_BUT_PSK_NOT_USABLE;
    }
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

    /* Check identity with external configured function */
    if (ssl->conf->f_psk != NULL) {
        if (ssl->conf->f_psk(
                ssl->conf->p_psk, ssl, identity, identity_len) == 0) {
            return SSL_TLS1_3_PSK_IDENTITY_MATCH;
        }
        return SSL_TLS1_3_PSK_IDENTITY_DOES_NOT_MATCH;
    }

    MBEDTLS_SSL_DEBUG_BUF(5, "identity", identity, identity_len);
    /* Check identity with pre-configured psk */
    if (ssl->conf->psk_identity != NULL &&
        identity_len == ssl->conf->psk_identity_len &&
        mbedtls_ct_memcmp(ssl->conf->psk_identity,
                          identity, identity_len) == 0) {
        ret = mbedtls_ssl_set_hs_psk(ssl, ssl->conf->psk, ssl->conf->psk_len);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_set_hs_psk", ret);
            return ret;
        }
        return SSL_TLS1_3_PSK_IDENTITY_MATCH;
    }

    return SSL_TLS1_3_PSK_IDENTITY_DOES_NOT_MATCH;
}

/*
 * Non-error return values of ssl_tls13_offered_psks_check_binder_match().
 * They are positive to not collide with error codes that are negative. Zero
 * (SSL_TLS1_3_BINDER_MATCH) in case of success as it may be propagated up
 * by the callers of this function as a generic success condition.
 */
#define SSL_TLS1_3_BINDER_DOES_NOT_MATCH 1
#define SSL_TLS1_3_BINDER_MATCH 0
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_offered_psks_check_binder_match(
    mbedtls_ssl_context *ssl,
    const unsigned char *binder, size_t binder_len,
    int psk_type, psa_algorithm_t psk_hash_alg)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    unsigned char transcript[PSA_HASH_MAX_SIZE];
    size_t transcript_len;
    unsigned char *psk;
    size_t psk_len;
    unsigned char server_computed_binder[PSA_HASH_MAX_SIZE];

    if (binder_len != PSA_HASH_LENGTH(psk_hash_alg)) {
        return SSL_TLS1_3_BINDER_DOES_NOT_MATCH;
    }

    /* Get current state of handshake transcript. */
    ret = mbedtls_ssl_get_handshake_transcript(
        ssl, mbedtls_md_type_from_psa_alg(psk_hash_alg),
        transcript, sizeof(transcript), &transcript_len);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_ssl_tls13_export_handshake_psk(ssl, &psk, &psk_len);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_ssl_tls13_create_psk_binder(ssl, psk_hash_alg,
                                              psk, psk_len, psk_type,
                                              transcript,
                                              server_computed_binder);
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    mbedtls_free((void *) psk);
#endif
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("PSK binder calculation failed."));
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    MBEDTLS_SSL_DEBUG_BUF(3, "psk binder ( computed ): ",
                          server_computed_binder, transcript_len);
    MBEDTLS_SSL_DEBUG_BUF(3, "psk binder ( received ): ", binder, binder_len);

    if (mbedtls_ct_memcmp(server_computed_binder,
                          binder,
                          PSA_HASH_LENGTH(psk_hash_alg)) == 0) {
        return SSL_TLS1_3_BINDER_MATCH;
    }

    mbedtls_platform_zeroize(server_computed_binder,
                             sizeof(server_computed_binder));
    return SSL_TLS1_3_BINDER_DOES_NOT_MATCH;
}

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_session_copy_ticket(mbedtls_ssl_session *dst,
                                         const mbedtls_ssl_session *src)
{
    dst->ticket_age_add = src->ticket_age_add;
    dst->ticket_flags = src->ticket_flags;
    dst->resumption_key_len = src->resumption_key_len;
    if (src->resumption_key_len == 0) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    memcpy(dst->resumption_key, src->resumption_key, src->resumption_key_len);

#if defined(MBEDTLS_SSL_EARLY_DATA)
    dst->max_early_data_size = src->max_early_data_size;

#if defined(MBEDTLS_SSL_ALPN)
    int ret = mbedtls_ssl_session_set_ticket_alpn(dst, src->ticket_alpn);
    if (ret != 0) {
        return ret;
    }
#endif /* MBEDTLS_SSL_ALPN */
#endif /* MBEDTLS_SSL_EARLY_DATA*/

    return 0;
}
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

struct psk_attributes {
    int type;
    int key_exchange_mode;
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info;
};
#define PSK_ATTRIBUTES_INIT { 0, 0, NULL }

/* Parser for pre_shared_key extension in client hello
 *    struct {
 *        opaque identity<1..2^16-1>;
 *        uint32 obfuscated_ticket_age;
 *    } PskIdentity;
 *
 *    opaque PskBinderEntry<32..255>;
 *
 *    struct {
 *        PskIdentity identities<7..2^16-1>;
 *        PskBinderEntry binders<33..2^16-1>;
 *    } OfferedPsks;
 *
 *    struct {
 *        select (Handshake.msg_type) {
 *            case client_hello: OfferedPsks;
 *            ....
 *        };
 *    } PreSharedKeyExtension;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_pre_shared_key_ext(
    mbedtls_ssl_context *ssl,
    const unsigned char *pre_shared_key_ext,
    const unsigned char *pre_shared_key_ext_end,
    const unsigned char *ciphersuites,
    const unsigned char *ciphersuites_end,
    struct psk_attributes *psk)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const unsigned char *identities = pre_shared_key_ext;
    const unsigned char *p_identity_len;
    size_t identities_len;
    const unsigned char *identities_end;
    const unsigned char *binders;
    const unsigned char *p_binder_len;
    size_t binders_len;
    const unsigned char *binders_end;
    int matched_identity = -1;
    int identity_id = -1;

    MBEDTLS_SSL_DEBUG_BUF(3, "pre_shared_key extension",
                          pre_shared_key_ext,
                          pre_shared_key_ext_end - pre_shared_key_ext);

    /* identities_len       2 bytes
     * identities_data   >= 7 bytes
     */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(identities, pre_shared_key_ext_end, 7 + 2);
    identities_len = MBEDTLS_GET_UINT16_BE(identities, 0);
    p_identity_len = identities + 2;
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p_identity_len, pre_shared_key_ext_end,
                                 identities_len);
    identities_end = p_identity_len + identities_len;

    /* binders_len     2  bytes
     * binders      >= 33 bytes
     */
    binders = identities_end;
    MBEDTLS_SSL_CHK_BUF_READ_PTR(binders, pre_shared_key_ext_end, 33 + 2);
    binders_len = MBEDTLS_GET_UINT16_BE(binders, 0);
    p_binder_len = binders + 2;
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p_binder_len, pre_shared_key_ext_end, binders_len);
    binders_end = p_binder_len + binders_len;

    ret = ssl->handshake->update_checksum(ssl, pre_shared_key_ext,
                                          identities_end - pre_shared_key_ext);
    if (0 != ret) {
        MBEDTLS_SSL_DEBUG_RET(1, ("update_checksum"), ret);
        return ret;
    }

    while (p_identity_len < identities_end && p_binder_len < binders_end) {
        const unsigned char *identity;
        size_t identity_len;
        uint32_t obfuscated_ticket_age;
        const unsigned char *binder;
        size_t binder_len;
        int psk_ciphersuite_id;
        psa_algorithm_t psk_hash_alg;
        int allowed_key_exchange_modes;

        mbedtls_ssl_session session;
        mbedtls_ssl_session_init(&session);

        MBEDTLS_SSL_CHK_BUF_READ_PTR(p_identity_len, identities_end, 2 + 1 + 4);
        identity_len = MBEDTLS_GET_UINT16_BE(p_identity_len, 0);
        identity = p_identity_len + 2;
        MBEDTLS_SSL_CHK_BUF_READ_PTR(identity, identities_end, identity_len + 4);
        obfuscated_ticket_age = MBEDTLS_GET_UINT32_BE(identity, identity_len);
        p_identity_len += identity_len + 6;

        MBEDTLS_SSL_CHK_BUF_READ_PTR(p_binder_len, binders_end, 1 + 32);
        binder_len = *p_binder_len;
        binder = p_binder_len + 1;
        MBEDTLS_SSL_CHK_BUF_READ_PTR(binder, binders_end, binder_len);
        p_binder_len += binder_len + 1;

        identity_id++;
        if (matched_identity != -1) {
            continue;
        }

        ret = ssl_tls13_offered_psks_check_identity_match(
            ssl, identity, identity_len, obfuscated_ticket_age,
            &psk->type, &session);
        if (ret != SSL_TLS1_3_PSK_IDENTITY_MATCH) {
            continue;
        }

        MBEDTLS_SSL_DEBUG_MSG(4, ("found matched identity"));

        switch (psk->type) {
            case MBEDTLS_SSL_TLS1_3_PSK_EXTERNAL:
                psk_ciphersuite_id = 0;
                psk_hash_alg = PSA_ALG_SHA_256;
                allowed_key_exchange_modes =
                    MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ALL;
                break;
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
            case MBEDTLS_SSL_TLS1_3_PSK_RESUMPTION:
                psk_ciphersuite_id = session.ciphersuite;
                psk_hash_alg = PSA_ALG_NONE;
                ssl->session_negotiate->ticket_flags = session.ticket_flags;
                allowed_key_exchange_modes =
                    session.ticket_flags &
                    MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ALL;
                break;
#endif
            default:
                return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        psk->key_exchange_mode = MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_NONE;

        if ((allowed_key_exchange_modes &
             MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL) &&
            ssl_tls13_key_exchange_is_psk_ephemeral_available(ssl)) {
            psk->key_exchange_mode = MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL;
        } else if ((allowed_key_exchange_modes &
                    MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK) &&
                   ssl_tls13_key_exchange_is_psk_available(ssl)) {
            psk->key_exchange_mode = MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK;
        }

        if (psk->key_exchange_mode == MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_NONE) {
            MBEDTLS_SSL_DEBUG_MSG(3, ("No suitable PSK key exchange mode"));
            continue;
        }

        ssl_tls13_select_ciphersuite(ssl, ciphersuites, ciphersuites_end,
                                     psk_ciphersuite_id, psk_hash_alg,
                                     &psk->ciphersuite_info);

        if (psk->ciphersuite_info == NULL) {
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
            mbedtls_ssl_session_free(&session);
#endif
            /*
             * We consider finding a ciphersuite suitable for the PSK as part
             * of the validation of its binder. Thus if we do not find one, we
             * abort the handshake with a decrypt_error alert.
             */
            MBEDTLS_SSL_PEND_FATAL_ALERT(
                MBEDTLS_SSL_ALERT_MSG_DECRYPT_ERROR,
                MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
            return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
        }

        ret = ssl_tls13_offered_psks_check_binder_match(
            ssl, binder, binder_len, psk->type,
            mbedtls_md_psa_alg_from_type((mbedtls_md_type_t) psk->ciphersuite_info->mac));
        if (ret != SSL_TLS1_3_BINDER_MATCH) {
            /* For security reasons, the handshake should be aborted when we
             * fail to validate a binder value. See RFC 8446 section 4.2.11.2
             * and appendix E.6. */
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
            mbedtls_ssl_session_free(&session);
#endif
            MBEDTLS_SSL_DEBUG_MSG(3, ("Invalid binder."));
            MBEDTLS_SSL_DEBUG_RET(
                1, "ssl_tls13_offered_psks_check_binder_match", ret);
            MBEDTLS_SSL_PEND_FATAL_ALERT(
                MBEDTLS_SSL_ALERT_MSG_DECRYPT_ERROR,
                MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
            return ret;
        }

        matched_identity = identity_id;

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
        if (psk->type == MBEDTLS_SSL_TLS1_3_PSK_RESUMPTION) {
            ret = ssl_tls13_session_copy_ticket(ssl->session_negotiate,
                                                &session);
            mbedtls_ssl_session_free(&session);
            if (ret != 0) {
                return ret;
            }
        }
#endif /* MBEDTLS_SSL_SESSION_TICKETS */
    }

    if (p_identity_len != identities_end || p_binder_len != binders_end) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("pre_shared_key extension decode error"));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR,
                                     MBEDTLS_ERR_SSL_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /* Update the handshake transcript with the binder list. */
    ret = ssl->handshake->update_checksum(
        ssl, identities_end, (size_t) (binders_end - identities_end));
    if (0 != ret) {
        MBEDTLS_SSL_DEBUG_RET(1, ("update_checksum"), ret);
        return ret;
    }
    if (matched_identity == -1) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("No usable PSK or ticket."));
        return MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY;
    }

    ssl->handshake->selected_identity = (uint16_t) matched_identity;
    MBEDTLS_SSL_DEBUG_MSG(3, ("Pre shared key found"));

    return 0;
}

/*
 * struct {
 *   select ( Handshake.msg_type ) {
 *      ....
 *      case server_hello:
 *          uint16 selected_identity;
 *   }
 * } PreSharedKeyExtension;
 */
static int ssl_tls13_write_server_pre_shared_key_ext(mbedtls_ssl_context *ssl,
                                                     unsigned char *buf,
                                                     unsigned char *end,
                                                     size_t *olen)
{
    unsigned char *p = (unsigned char *) buf;

    *olen = 0;

    int not_using_psk = 0;
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    not_using_psk = (mbedtls_svc_key_id_is_null(ssl->handshake->psk_opaque));
#else
    not_using_psk = (ssl->handshake->psk == NULL);
#endif
    if (not_using_psk) {
        /* We shouldn't have called this extension writer unless we've
         * chosen to use a PSK. */
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    MBEDTLS_SSL_DEBUG_MSG(3, ("server hello, adding pre_shared_key extension"));
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 6);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_PRE_SHARED_KEY, p, 0);
    MBEDTLS_PUT_UINT16_BE(2, p, 2);

    MBEDTLS_PUT_UINT16_BE(ssl->handshake->selected_identity, p, 4);

    *olen = 6;

    MBEDTLS_SSL_DEBUG_MSG(4, ("sent selected_identity: %u",
                              ssl->handshake->selected_identity));

    mbedtls_ssl_tls13_set_hs_sent_ext_mask(ssl, MBEDTLS_TLS_EXT_PRE_SHARED_KEY);

    return 0;
}

#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED */

/* From RFC 8446:
 *   struct {
 *          ProtocolVersion versions<2..254>;
 *   } SupportedVersions;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_supported_versions_ext(mbedtls_ssl_context *ssl,
                                                  const unsigned char *buf,
                                                  const unsigned char *end)
{
    const unsigned char *p = buf;
    size_t versions_len;
    const unsigned char *versions_end;
    uint16_t tls_version;
    int found_supported_version = 0;

    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 1);
    versions_len = p[0];
    p += 1;

    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, versions_len);
    versions_end = p + versions_len;
    while (p < versions_end) {
        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, versions_end, 2);
        tls_version = mbedtls_ssl_read_version(p, ssl->conf->transport);
        p += 2;

        if (MBEDTLS_SSL_VERSION_TLS1_3 == tls_version) {
            found_supported_version = 1;
            break;
        }

        if ((MBEDTLS_SSL_VERSION_TLS1_2 == tls_version) &&
            mbedtls_ssl_conf_is_tls12_enabled(ssl->conf)) {
            found_supported_version = 1;
            break;
        }
    }

    if (!found_supported_version) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("No supported version found."));

        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_PROTOCOL_VERSION,
                                     MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION);
        return MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION;
    }

    MBEDTLS_SSL_DEBUG_MSG(1, ("Negotiated version: [%04x]",
                              (unsigned int) tls_version));

    return (int) tls_version;
}

#if defined(PSA_WANT_ALG_ECDH) || defined(PSA_WANT_ALG_FFDH)
/*
 *
 * From RFC 8446:
 *   enum {
 *       ... (0xFFFF)
 *   } NamedGroup;
 *   struct {
 *       NamedGroup named_group_list<2..2^16-1>;
 *   } NamedGroupList;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_supported_groups_ext(mbedtls_ssl_context *ssl,
                                                const unsigned char *buf,
                                                const unsigned char *end)
{
    const unsigned char *p = buf;
    size_t named_group_list_len;
    const unsigned char *named_group_list_end;

    MBEDTLS_SSL_DEBUG_BUF(3, "supported_groups extension", p, end - buf);
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 2);
    named_group_list_len = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, named_group_list_len);
    named_group_list_end = p + named_group_list_len;
    ssl->handshake->hrr_selected_group = 0;

    while (p < named_group_list_end) {
        uint16_t named_group;
        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, named_group_list_end, 2);
        named_group = MBEDTLS_GET_UINT16_BE(p, 0);
        p += 2;

        MBEDTLS_SSL_DEBUG_MSG(2,
                              ("got named group: %s(%04x)",
                               mbedtls_ssl_named_group_to_str(named_group),
                               named_group));

        if (!mbedtls_ssl_named_group_is_offered(ssl, named_group) ||
            !mbedtls_ssl_named_group_is_supported(named_group) ||
            ssl->handshake->hrr_selected_group != 0) {
            continue;
        }

        MBEDTLS_SSL_DEBUG_MSG(2,
                              ("add named group %s(%04x) into received list.",
                               mbedtls_ssl_named_group_to_str(named_group),
                               named_group));

        ssl->handshake->hrr_selected_group = named_group;
    }

    return 0;

}
#endif /* PSA_WANT_ALG_ECDH || PSA_WANT_ALG_FFDH */

#define SSL_TLS1_3_PARSE_KEY_SHARES_EXT_NO_MATCH 1

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED)
/*
 *  ssl_tls13_parse_key_shares_ext() verifies whether the information in the
 *  extension is correct and stores the first acceptable key share and its
 *  associated group.
 *
 *  Possible return values are:
 *  - 0: Successful processing of the client provided key share extension.
 *  - SSL_TLS1_3_PARSE_KEY_SHARES_EXT_NO_MATCH: The key shares provided by
 *    the client does not match a group supported by the server. A
 *    HelloRetryRequest will be needed.
 *  - A negative value for fatal errors.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_key_shares_ext(mbedtls_ssl_context *ssl,
                                          const unsigned char *buf,
                                          const unsigned char *end)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char const *p = buf;
    unsigned char const *client_shares_end;
    size_t client_shares_len;

    /* From RFC 8446:
     *
     * struct {
     *     KeyShareEntry client_shares<0..2^16-1>;
     * } KeyShareClientHello;
     *
     */

    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 2);
    client_shares_len = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, client_shares_len);

    ssl->handshake->offered_group_id = 0;
    client_shares_end = p + client_shares_len;

    /* We try to find a suitable key share entry and copy it to the
     * handshake context. Later, we have to find out whether we can do
     * something with the provided key share or whether we have to
     * dismiss it and send a HelloRetryRequest message.
     */

    while (p < client_shares_end) {
        uint16_t group;
        size_t key_exchange_len;
        const unsigned char *key_exchange;

        /*
         * struct {
         *    NamedGroup group;
         *    opaque key_exchange<1..2^16-1>;
         * } KeyShareEntry;
         */
        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, client_shares_end, 4);
        group = MBEDTLS_GET_UINT16_BE(p, 0);
        key_exchange_len = MBEDTLS_GET_UINT16_BE(p, 2);
        p += 4;
        key_exchange = p;
        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, client_shares_end, key_exchange_len);
        p += key_exchange_len;

        /* Continue parsing even if we have already found a match,
         * for input validation purposes.
         */
        if (!mbedtls_ssl_named_group_is_offered(ssl, group) ||
            !mbedtls_ssl_named_group_is_supported(group) ||
            ssl->handshake->offered_group_id != 0) {
            continue;
        }

        /*
         * ECDHE and FFDHE groups are supported
         */
        if (mbedtls_ssl_tls13_named_group_is_ecdhe(group) ||
            mbedtls_ssl_tls13_named_group_is_ffdh(group)) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("ECDH/FFDH group: %s (%04x)",
                                      mbedtls_ssl_named_group_to_str(group),
                                      group));
            ret = mbedtls_ssl_tls13_read_public_xxdhe_share(
                ssl, key_exchange - 2, key_exchange_len + 2);
            if (ret != 0) {
                return ret;
            }

        } else {
            MBEDTLS_SSL_DEBUG_MSG(4, ("Unrecognized NamedGroup %u",
                                      (unsigned) group));
            continue;
        }

        ssl->handshake->offered_group_id = group;
    }


    if (ssl->handshake->offered_group_id == 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("no matching key share"));
        return SSL_TLS1_3_PARSE_KEY_SHARES_EXT_NO_MATCH;
    }
    return 0;
}
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_client_hello_has_exts(mbedtls_ssl_context *ssl,
                                           int exts_mask)
{
    int masked = ssl->handshake->received_extensions & exts_mask;
    return masked == exts_mask;
}

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_client_hello_has_exts_for_ephemeral_key_exchange(
    mbedtls_ssl_context *ssl)
{
    return ssl_tls13_client_hello_has_exts(
        ssl,
        MBEDTLS_SSL_EXT_MASK(SUPPORTED_GROUPS) |
        MBEDTLS_SSL_EXT_MASK(KEY_SHARE)        |
        MBEDTLS_SSL_EXT_MASK(SIG_ALG));
}
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_client_hello_has_exts_for_psk_key_exchange(
    mbedtls_ssl_context *ssl)
{
    return ssl_tls13_client_hello_has_exts(
        ssl,
        MBEDTLS_SSL_EXT_MASK(PRE_SHARED_KEY)          |
        MBEDTLS_SSL_EXT_MASK(PSK_KEY_EXCHANGE_MODES));
}
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED */

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_client_hello_has_exts_for_psk_ephemeral_key_exchange(
    mbedtls_ssl_context *ssl)
{
    return ssl_tls13_client_hello_has_exts(
        ssl,
        MBEDTLS_SSL_EXT_MASK(SUPPORTED_GROUPS)        |
        MBEDTLS_SSL_EXT_MASK(KEY_SHARE)               |
        MBEDTLS_SSL_EXT_MASK(PRE_SHARED_KEY)          |
        MBEDTLS_SSL_EXT_MASK(PSK_KEY_EXCHANGE_MODES));
}
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED */

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_key_exchange_is_psk_available(mbedtls_ssl_context *ssl)
{
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED)
    return mbedtls_ssl_conf_tls13_is_psk_enabled(ssl) &&
           mbedtls_ssl_tls13_is_psk_supported(ssl) &&
           ssl_tls13_client_hello_has_exts_for_psk_key_exchange(ssl);
#else
    ((void) ssl);
    return 0;
#endif
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_key_exchange_is_psk_ephemeral_available(mbedtls_ssl_context *ssl)
{
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED)
    return mbedtls_ssl_conf_tls13_is_psk_ephemeral_enabled(ssl) &&
           mbedtls_ssl_tls13_is_psk_ephemeral_supported(ssl) &&
           ssl_tls13_client_hello_has_exts_for_psk_ephemeral_key_exchange(ssl);
#else
    ((void) ssl);
    return 0;
#endif
}
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_key_exchange_is_ephemeral_available(mbedtls_ssl_context *ssl)
{
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
    return mbedtls_ssl_conf_tls13_is_ephemeral_enabled(ssl) &&
           ssl_tls13_client_hello_has_exts_for_ephemeral_key_exchange(ssl);
#else
    ((void) ssl);
    return 0;
#endif
}

#if defined(MBEDTLS_X509_CRT_PARSE_C) && \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)

#if defined(MBEDTLS_USE_PSA_CRYPTO)
static psa_algorithm_t ssl_tls13_iana_sig_alg_to_psa_alg(uint16_t sig_alg)
{
    switch (sig_alg) {
        case MBEDTLS_TLS1_3_SIG_ECDSA_SECP256R1_SHA256:
            return PSA_ALG_ECDSA(PSA_ALG_SHA_256);
        case MBEDTLS_TLS1_3_SIG_ECDSA_SECP384R1_SHA384:
            return PSA_ALG_ECDSA(PSA_ALG_SHA_384);
        case MBEDTLS_TLS1_3_SIG_ECDSA_SECP521R1_SHA512:
            return PSA_ALG_ECDSA(PSA_ALG_SHA_512);
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA256:
            return PSA_ALG_RSA_PSS(PSA_ALG_SHA_256);
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA384:
            return PSA_ALG_RSA_PSS(PSA_ALG_SHA_384);
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA512:
            return PSA_ALG_RSA_PSS(PSA_ALG_SHA_512);
        case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA256:
            return PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256);
        case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA384:
            return PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_384);
        case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA512:
            return PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_512);
        default:
            return PSA_ALG_NONE;
    }
}
#endif /* MBEDTLS_USE_PSA_CRYPTO */

/*
 * Pick best ( private key, certificate chain ) pair based on the signature
 * algorithms supported by the client.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_pick_key_cert(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_key_cert *key_cert, *key_cert_list;
    const uint16_t *sig_alg = ssl->handshake->received_sig_algs;

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
    if (ssl->handshake->sni_key_cert != NULL) {
        key_cert_list = ssl->handshake->sni_key_cert;
    } else
#endif /* MBEDTLS_SSL_SERVER_NAME_INDICATION */
    key_cert_list = ssl->conf->key_cert;

    if (key_cert_list == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("server has no certificate"));
        return -1;
    }

    for (; *sig_alg != MBEDTLS_TLS1_3_SIG_NONE; sig_alg++) {
        if (!mbedtls_ssl_sig_alg_is_offered(ssl, *sig_alg)) {
            continue;
        }

        if (!mbedtls_ssl_tls13_sig_alg_for_cert_verify_is_supported(*sig_alg)) {
            continue;
        }

        for (key_cert = key_cert_list; key_cert != NULL;
             key_cert = key_cert->next) {
#if defined(MBEDTLS_USE_PSA_CRYPTO)
            psa_algorithm_t psa_alg = PSA_ALG_NONE;
#endif /* MBEDTLS_USE_PSA_CRYPTO */

            MBEDTLS_SSL_DEBUG_CRT(3, "certificate (chain) candidate",
                                  key_cert->cert);

            /*
             * This avoids sending the client a cert it'll reject based on
             * keyUsage or other extensions.
             */
            if (mbedtls_x509_crt_check_key_usage(
                    key_cert->cert, MBEDTLS_X509_KU_DIGITAL_SIGNATURE) != 0 ||
                mbedtls_x509_crt_check_extended_key_usage(
                    key_cert->cert, MBEDTLS_OID_SERVER_AUTH,
                    MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH)) != 0) {
                MBEDTLS_SSL_DEBUG_MSG(3, ("certificate mismatch: "
                                          "(extended) key usage extension"));
                continue;
            }

            MBEDTLS_SSL_DEBUG_MSG(3,
                                  ("ssl_tls13_pick_key_cert:"
                                   "check signature algorithm %s [%04x]",
                                   mbedtls_ssl_sig_alg_to_str(*sig_alg),
                                   *sig_alg));
#if defined(MBEDTLS_USE_PSA_CRYPTO)
            psa_alg = ssl_tls13_iana_sig_alg_to_psa_alg(*sig_alg);
#endif /* MBEDTLS_USE_PSA_CRYPTO */

            if (mbedtls_ssl_tls13_check_sig_alg_cert_key_match(
                    *sig_alg, &key_cert->cert->pk)
#if defined(MBEDTLS_USE_PSA_CRYPTO)
                && psa_alg != PSA_ALG_NONE &&
                mbedtls_pk_can_do_ext(&key_cert->cert->pk, psa_alg,
                                      PSA_KEY_USAGE_SIGN_HASH) == 1
#endif /* MBEDTLS_USE_PSA_CRYPTO */
                ) {
                ssl->handshake->key_cert = key_cert;
                MBEDTLS_SSL_DEBUG_MSG(3,
                                      ("ssl_tls13_pick_key_cert:"
                                       "selected signature algorithm"
                                       " %s [%04x]",
                                       mbedtls_ssl_sig_alg_to_str(*sig_alg),
                                       *sig_alg));
                MBEDTLS_SSL_DEBUG_CRT(
                    3, "selected certificate (chain)",
                    ssl->handshake->key_cert->cert);
                return 0;
            }
        }
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("ssl_tls13_pick_key_cert:"
                              "no suitable certificate found"));
    return -1;
}
#endif /* MBEDTLS_X509_CRT_PARSE_C &&
          MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

/*
 *
 * STATE HANDLING: ClientHello
 *
 * There are three possible classes of outcomes when parsing the ClientHello:
 *
 * 1) The ClientHello was well-formed and matched the server's configuration.
 *
 *    In this case, the server progresses to sending its ServerHello.
 *
 * 2) The ClientHello was well-formed but didn't match the server's
 *    configuration.
 *
 *    For example, the client might not have offered a key share which
 *    the server supports, or the server might require a cookie.
 *
 *    In this case, the server sends a HelloRetryRequest.
 *
 * 3) The ClientHello was ill-formed
 *
 *    In this case, we abort the handshake.
 *
 */

/*
 * Structure of this message:
 *
 * uint16 ProtocolVersion;
 * opaque Random[32];
 * uint8 CipherSuite[2];    // Cryptographic suite selector
 *
 * struct {
 *      ProtocolVersion legacy_version = 0x0303;    // TLS v1.2
 *      Random random;
 *      opaque legacy_session_id<0..32>;
 *      CipherSuite cipher_suites<2..2^16-2>;
 *      opaque legacy_compression_methods<1..2^8-1>;
 *      Extension extensions<8..2^16-1>;
 * } ClientHello;
 */

#define SSL_CLIENT_HELLO_OK           0
#define SSL_CLIENT_HELLO_HRR_REQUIRED 1
#define SSL_CLIENT_HELLO_TLS1_2       2

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_client_hello(mbedtls_ssl_context *ssl,
                                        const unsigned char *buf,
                                        const unsigned char *end)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const unsigned char *p = buf;
    const unsigned char *random;
    size_t legacy_session_id_len;
    const unsigned char *legacy_session_id;
    size_t cipher_suites_len;
    const unsigned char *cipher_suites;
    const unsigned char *cipher_suites_end;
    size_t extensions_len;
    const unsigned char *extensions_end;
    const unsigned char *supported_versions_data;
    const unsigned char *supported_versions_data_end;
    mbedtls_ssl_handshake_params *handshake = ssl->handshake;
    int hrr_required = 0;
    int no_usable_share_for_key_agreement = 0;

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
    int got_psk = 0;
    struct psk_attributes psk = PSK_ATTRIBUTES_INIT;
    const unsigned char *pre_shared_key_ext = NULL;
    const unsigned char *pre_shared_key_ext_end = NULL;
#endif

    /*
     * ClientHello layout:
     *     0  .   1   protocol version
     *     2  .  33   random bytes
     *    34  .  34   session id length ( 1 byte )
     *    35  . 34+x  session id
     *    ..  .  ..   ciphersuite list length ( 2 bytes )
     *    ..  .  ..   ciphersuite list
     *    ..  .  ..   compression alg. list length ( 1 byte )
     *    ..  .  ..   compression alg. list
     *    ..  .  ..   extensions length ( 2 bytes, optional )
     *    ..  .  ..   extensions ( optional )
     */

    /*
     * Minimal length ( with everything empty and extensions omitted ) is
     * 2 + 32 + 1 + 2 + 1 = 38 bytes. Check that first, so that we can
     * read at least up to session id length without worrying.
     */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 38);

    /* ...
     * ProtocolVersion legacy_version = 0x0303; // TLS 1.2
     * ...
     * with ProtocolVersion defined as:
     * uint16 ProtocolVersion;
     */
    if (mbedtls_ssl_read_version(p, ssl->conf->transport) !=
        MBEDTLS_SSL_VERSION_TLS1_2) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Unsupported version of TLS."));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_PROTOCOL_VERSION,
                                     MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION);
        return MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION;
    }
    p += 2;

    /* ...
     * Random random;
     * ...
     * with Random defined as:
     * opaque Random[32];
     */
    random = p;
    p += MBEDTLS_CLIENT_HELLO_RANDOM_LEN;

    /* ...
     * opaque legacy_session_id<0..32>;
     * ...
     */
    legacy_session_id_len = *(p++);
    legacy_session_id = p;

    /*
     * Check we have enough data for the legacy session identifier
     * and the ciphersuite list length.
     */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, legacy_session_id_len + 2);
    p += legacy_session_id_len;

    /* ...
     * CipherSuite cipher_suites<2..2^16-2>;
     * ...
     * with CipherSuite defined as:
     * uint8 CipherSuite[2];
     */
    cipher_suites_len = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;
    cipher_suites = p;

    /*
     * The length of the ciphersuite list has to be even.
     */
    if (cipher_suites_len & 1) {
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR,
                                     MBEDTLS_ERR_SSL_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /* Check we have enough data for the ciphersuite list, the legacy
     * compression methods and the length of the extensions.
     *
     * cipher_suites                cipher_suites_len bytes
     * legacy_compression_methods length            1 byte
     */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, cipher_suites_len + 1);
    p += cipher_suites_len;
    cipher_suites_end = p;

    /* Check if we have enough data for legacy_compression_methods
     * and the length of the extensions (2 bytes).
     */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p + 1, end, p[0] + 2);

    /*
     * Search for the supported versions extension and parse it to determine
     * if the client supports TLS 1.3.
     */
    ret = mbedtls_ssl_tls13_is_supported_versions_ext_present_in_exts(
        ssl, p + 1 + p[0], end,
        &supported_versions_data, &supported_versions_data_end);
    if (ret < 0) {
        MBEDTLS_SSL_DEBUG_RET(1,
                              ("mbedtls_ssl_tls13_is_supported_versions_ext_present_in_exts"), ret);
        return ret;
    }

    if (ret == 0) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("no supported_versions extension"));
        return SSL_CLIENT_HELLO_TLS1_2;
    }

    if (ret == 1) {
        ret = ssl_tls13_parse_supported_versions_ext(ssl,
                                                     supported_versions_data,
                                                     supported_versions_data_end);
        if (ret < 0) {
            MBEDTLS_SSL_DEBUG_RET(1,
                                  ("ssl_tls13_parse_supported_versions_ext"), ret);
            return ret;
        }

        /*
         * The supported versions extension was parsed successfully as the
         * value returned by ssl_tls13_parse_supported_versions_ext() is
         * positive. The return value is then equal to
         * MBEDTLS_SSL_VERSION_TLS1_2 or MBEDTLS_SSL_VERSION_TLS1_3, defining
         * the TLS version to negotiate.
         */
        if (MBEDTLS_SSL_VERSION_TLS1_2 == ret) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("supported_versions without 1.3"));
            return SSL_CLIENT_HELLO_TLS1_2;
        }
    }

    /*
     * We negotiate TLS 1.3.
     */
    ssl->tls_version = MBEDTLS_SSL_VERSION_TLS1_3;
    ssl->session_negotiate->tls_version = MBEDTLS_SSL_VERSION_TLS1_3;
    ssl->session_negotiate->endpoint = ssl->conf->endpoint;

    /* Before doing any crypto, make sure we can. */
    ret = mbedtls_ssl_tls13_crypto_init(ssl);
    if (ret != 0) {
        return ret;
    }

    /*
     * We are negotiating the version 1.3 of the protocol. Do what we have
     * postponed: copy of the client random bytes, copy of the legacy session
     * identifier and selection of the TLS 1.3 cipher suite.
     */
    MBEDTLS_SSL_DEBUG_BUF(3, "client hello, random bytes",
                          random, MBEDTLS_CLIENT_HELLO_RANDOM_LEN);
    memcpy(&handshake->randbytes[0], random, MBEDTLS_CLIENT_HELLO_RANDOM_LEN);

    if (legacy_session_id_len > sizeof(ssl->session_negotiate->id)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad client hello message"));
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }
    ssl->session_negotiate->id_len = legacy_session_id_len;
    MBEDTLS_SSL_DEBUG_BUF(3, "client hello, session id",
                          legacy_session_id, legacy_session_id_len);
    memcpy(&ssl->session_negotiate->id[0],
           legacy_session_id, legacy_session_id_len);

    /*
     * Search for a matching ciphersuite
     */
    MBEDTLS_SSL_DEBUG_BUF(3, "client hello, list of cipher suites",
                          cipher_suites, cipher_suites_len);

    ssl_tls13_select_ciphersuite(ssl, cipher_suites, cipher_suites_end,
                                 0, PSA_ALG_NONE, &handshake->ciphersuite_info);

    if (handshake->ciphersuite_info == NULL) {
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE,
                                     MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }
    ssl->session_negotiate->ciphersuite = handshake->ciphersuite_info->id;

    MBEDTLS_SSL_DEBUG_MSG(2, ("selected ciphersuite: %04x - %s",
                              ((unsigned) handshake->ciphersuite_info->id),
                              handshake->ciphersuite_info->name));

    /* ...
     * opaque legacy_compression_methods<1..2^8-1>;
     * ...
     */
    if (p[0] != 1 || p[1] != MBEDTLS_SSL_COMPRESS_NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad legacy compression method"));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER,
                                     MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }
    p += 2;

    /* ...
     * Extension extensions<8..2^16-1>;
     * ...
     * with Extension defined as:
     * struct {
     *    ExtensionType extension_type;
     *    opaque extension_data<0..2^16-1>;
     * } Extension;
     */
    extensions_len = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, extensions_len);
    extensions_end = p + extensions_len;

    MBEDTLS_SSL_DEBUG_BUF(3, "client hello extensions", p, extensions_len);
    handshake->received_extensions = MBEDTLS_SSL_EXT_MASK_NONE;

    while (p < extensions_end) {
        unsigned int extension_type;
        size_t extension_data_len;
        const unsigned char *extension_data_end;
        uint32_t allowed_exts = MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_CH;

        if (ssl->handshake->hello_retry_request_flag) {
            /* Do not accept early data extension in 2nd ClientHello */
            allowed_exts &= ~MBEDTLS_SSL_EXT_MASK(EARLY_DATA);
        }

        /* RFC 8446, section 4.2.11
         *
         * The "pre_shared_key" extension MUST be the last extension in the
         * ClientHello (this facilitates implementation as described below).
         * Servers MUST check that it is the last extension and otherwise fail
         * the handshake with an "illegal_parameter" alert.
         */
        if (handshake->received_extensions & MBEDTLS_SSL_EXT_MASK(PRE_SHARED_KEY)) {
            MBEDTLS_SSL_DEBUG_MSG(
                3, ("pre_shared_key is not last extension."));
            MBEDTLS_SSL_PEND_FATAL_ALERT(
                MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER,
                MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
            return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
        }

        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, extensions_end, 4);
        extension_type = MBEDTLS_GET_UINT16_BE(p, 0);
        extension_data_len = MBEDTLS_GET_UINT16_BE(p, 2);
        p += 4;

        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, extensions_end, extension_data_len);
        extension_data_end = p + extension_data_len;

        ret = mbedtls_ssl_tls13_check_received_extension(
            ssl, MBEDTLS_SSL_HS_CLIENT_HELLO, extension_type,
            allowed_exts);
        if (ret != 0) {
            return ret;
        }

        switch (extension_type) {
#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
            case MBEDTLS_TLS_EXT_SERVERNAME:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found ServerName extension"));
                ret = mbedtls_ssl_parse_server_name_ext(ssl, p,
                                                        extension_data_end);
                if (ret != 0) {
                    MBEDTLS_SSL_DEBUG_RET(
                        1, "mbedtls_ssl_parse_servername_ext", ret);
                    return ret;
                }
                break;
#endif /* MBEDTLS_SSL_SERVER_NAME_INDICATION */

#if defined(PSA_WANT_ALG_ECDH) || defined(PSA_WANT_ALG_FFDH)
            case MBEDTLS_TLS_EXT_SUPPORTED_GROUPS:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found supported group extension"));

                /* Supported Groups Extension
                 *
                 * When sent by the client, the "supported_groups" extension
                 * indicates the named groups which the client supports,
                 * ordered from most preferred to least preferred.
                 */
                ret = ssl_tls13_parse_supported_groups_ext(
                    ssl, p, extension_data_end);
                if (ret != 0) {
                    MBEDTLS_SSL_DEBUG_RET(
                        1, "ssl_tls13_parse_supported_groups_ext", ret);
                    return ret;
                }

                break;
#endif /* PSA_WANT_ALG_ECDH || PSA_WANT_ALG_FFDH*/

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED)
            case MBEDTLS_TLS_EXT_KEY_SHARE:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found key share extension"));

                /*
                 * Key Share Extension
                 *
                 * When sent by the client, the "key_share" extension
                 * contains the endpoint's cryptographic parameters for
                 * ECDHE/DHE key establishment methods.
                 */
                ret = ssl_tls13_parse_key_shares_ext(
                    ssl, p, extension_data_end);
                if (ret == SSL_TLS1_3_PARSE_KEY_SHARES_EXT_NO_MATCH) {
                    MBEDTLS_SSL_DEBUG_MSG(2, ("No usable share for key agreement."));
                    no_usable_share_for_key_agreement = 1;
                }

                if (ret < 0) {
                    MBEDTLS_SSL_DEBUG_RET(
                        1, "ssl_tls13_parse_key_shares_ext", ret);
                    return ret;
                }

                break;
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED */

            case MBEDTLS_TLS_EXT_SUPPORTED_VERSIONS:
                /* Already parsed */
                break;

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
            case MBEDTLS_TLS_EXT_PSK_KEY_EXCHANGE_MODES:
                MBEDTLS_SSL_DEBUG_MSG(
                    3, ("found psk key exchange modes extension"));

                ret = ssl_tls13_parse_key_exchange_modes_ext(
                    ssl, p, extension_data_end);
                if (ret != 0) {
                    MBEDTLS_SSL_DEBUG_RET(
                        1, "ssl_tls13_parse_key_exchange_modes_ext", ret);
                    return ret;
                }

                break;
#endif

            case MBEDTLS_TLS_EXT_PRE_SHARED_KEY:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found pre_shared_key extension"));
                if ((handshake->received_extensions &
                     MBEDTLS_SSL_EXT_MASK(PSK_KEY_EXCHANGE_MODES)) == 0) {
                    MBEDTLS_SSL_PEND_FATAL_ALERT(
                        MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER,
                        MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
                    return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
                }
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
                /* Delay processing of the PSK identity once we have
                 * found out which algorithms to use. We keep a pointer
                 * to the buffer and the size for later processing.
                 */
                pre_shared_key_ext = p;
                pre_shared_key_ext_end = extension_data_end;
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED */
                break;

#if defined(MBEDTLS_SSL_ALPN)
            case MBEDTLS_TLS_EXT_ALPN:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found alpn extension"));

                ret = mbedtls_ssl_parse_alpn_ext(ssl, p, extension_data_end);
                if (ret != 0) {
                    MBEDTLS_SSL_DEBUG_RET(
                        1, ("mbedtls_ssl_parse_alpn_ext"), ret);
                    return ret;
                }
                break;
#endif /* MBEDTLS_SSL_ALPN */

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
            case MBEDTLS_TLS_EXT_SIG_ALG:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found signature_algorithms extension"));

                ret = mbedtls_ssl_parse_sig_alg_ext(
                    ssl, p, extension_data_end);
                if (ret != 0) {
                    MBEDTLS_SSL_DEBUG_RET(
                        1, "mbedtls_ssl_parse_sig_alg_ext", ret);
                    return ret;
                }
                break;
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

#if defined(MBEDTLS_SSL_RECORD_SIZE_LIMIT)
            case MBEDTLS_TLS_EXT_RECORD_SIZE_LIMIT:
                MBEDTLS_SSL_DEBUG_MSG(3, ("found record_size_limit extension"));

                ret = mbedtls_ssl_tls13_parse_record_size_limit_ext(
                    ssl, p, extension_data_end);
                if (ret != 0) {
                    MBEDTLS_SSL_DEBUG_RET(
                        1, ("mbedtls_ssl_tls13_parse_record_size_limit_ext"), ret);
                    return ret;
                }
                break;
#endif /* MBEDTLS_SSL_RECORD_SIZE_LIMIT */

            default:
                MBEDTLS_SSL_PRINT_EXT(
                    3, MBEDTLS_SSL_HS_CLIENT_HELLO,
                    extension_type, "( ignored )");
                break;
        }

        p += extension_data_len;
    }

    MBEDTLS_SSL_PRINT_EXTS(3, MBEDTLS_SSL_HS_CLIENT_HELLO,
                           handshake->received_extensions);

    ret = mbedtls_ssl_add_hs_hdr_to_checksum(ssl,
                                             MBEDTLS_SSL_HS_CLIENT_HELLO,
                                             p - buf);
    if (0 != ret) {
        MBEDTLS_SSL_DEBUG_RET(1, ("mbedtls_ssl_add_hs_hdr_to_checksum"), ret);
        return ret;
    }

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
    /* Update checksum with either
     * - The entire content of the CH message, if no PSK extension is present
     * - The content up to but excluding the PSK extension, if present.
     * Always parse the pre-shared-key extension when present in the
     * ClientHello even if some pre-requisites for PSK key exchange modes are
     * not met. That way we always validate the syntax of the extension.
     */
    if (handshake->received_extensions & MBEDTLS_SSL_EXT_MASK(PRE_SHARED_KEY)) {
        ret = handshake->update_checksum(ssl, buf,
                                         pre_shared_key_ext - buf);
        if (0 != ret) {
            MBEDTLS_SSL_DEBUG_RET(1, ("update_checksum"), ret);
            return ret;
        }
        ret = ssl_tls13_parse_pre_shared_key_ext(ssl,
                                                 pre_shared_key_ext,
                                                 pre_shared_key_ext_end,
                                                 cipher_suites,
                                                 cipher_suites_end,
                                                 &psk);
        if (ret == 0) {
            got_psk = 1;
        } else if (ret != MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY) {
            MBEDTLS_SSL_DEBUG_RET(
                1, "ssl_tls13_parse_pre_shared_key_ext", ret);
            return ret;
        }
    } else
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED */
    {
        ret = handshake->update_checksum(ssl, buf, p - buf);
        if (0 != ret) {
            MBEDTLS_SSL_DEBUG_RET(1, ("update_checksum"), ret);
            return ret;
        }
    }

    /*
     * Determine the key exchange algorithm to use.
     * There are three types of key exchanges supported in TLS 1.3:
     * - (EC)DH with ECDSA,
     * - (EC)DH with PSK,
     * - plain PSK.
     *
     * The PSK-based key exchanges may additionally be used with 0-RTT.
     *
     * Our built-in order of preference is
     *  1 ) (EC)DHE-PSK Mode ( psk_ephemeral )
     *  2 ) Certificate Mode ( ephemeral )
     *  3 ) Plain PSK Mode ( psk )
     */
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
    if (got_psk && (psk.key_exchange_mode ==
                    MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL)) {
        handshake->key_exchange_mode =
            MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL;
        MBEDTLS_SSL_DEBUG_MSG(2, ("key exchange mode: psk_ephemeral"));

    } else
#endif
    if (ssl_tls13_key_exchange_is_ephemeral_available(ssl)) {
        handshake->key_exchange_mode =
            MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL;
        MBEDTLS_SSL_DEBUG_MSG(2, ("key exchange mode: ephemeral"));

    }
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
    else if (got_psk && (psk.key_exchange_mode ==
                         MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK)) {
        handshake->key_exchange_mode = MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK;
        MBEDTLS_SSL_DEBUG_MSG(2, ("key exchange mode: psk"));
    }
#endif
    else {
        MBEDTLS_SSL_DEBUG_MSG(
            1,
            ("ClientHello message misses mandatory extensions."));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_MISSING_EXTENSION,
                                     MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
    if (handshake->key_exchange_mode &
        MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ALL) {
        handshake->ciphersuite_info = psk.ciphersuite_info;
        ssl->session_negotiate->ciphersuite = psk.ciphersuite_info->id;

        MBEDTLS_SSL_DEBUG_MSG(2, ("Select PSK ciphersuite: %04x - %s",
                                  ((unsigned) psk.ciphersuite_info->id),
                                  psk.ciphersuite_info->name));

        if (psk.type == MBEDTLS_SSL_TLS1_3_PSK_RESUMPTION) {
            handshake->resume = 1;
        }
    }
#endif

    if (handshake->key_exchange_mode !=
        MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK) {
        hrr_required = (no_usable_share_for_key_agreement != 0);
    }

    mbedtls_ssl_optimize_checksum(ssl, handshake->ciphersuite_info);

    return hrr_required ? SSL_CLIENT_HELLO_HRR_REQUIRED : SSL_CLIENT_HELLO_OK;
}

#if defined(MBEDTLS_SSL_EARLY_DATA)
static int ssl_tls13_check_early_data_requirements(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_handshake_params *handshake = ssl->handshake;

    if (ssl->conf->early_data_enabled == MBEDTLS_SSL_EARLY_DATA_DISABLED) {
        MBEDTLS_SSL_DEBUG_MSG(
            1,
            ("EarlyData: rejected, feature disabled in server configuration."));
        return -1;
    }

    if (!handshake->resume) {
        /* We currently support early data only in the case of PSKs established
           via a NewSessionTicket message thus in the case of a session
           resumption. */
        MBEDTLS_SSL_DEBUG_MSG(
            1, ("EarlyData: rejected, not a session resumption."));
        return -1;
    }

    /* RFC 8446 4.2.10
     *
     * In order to accept early data, the server MUST have accepted a PSK cipher
     * suite and selected the first key offered in the client's "pre_shared_key"
     * extension. In addition, it MUST verify that the following values are the
     * same as those associated with the selected PSK:
     * - The TLS version number
     * - The selected cipher suite
     * - The selected ALPN [RFC7301] protocol, if any
     *
     * NOTE:
     *  - The TLS version number is checked in
     *    ssl_tls13_offered_psks_check_identity_match_ticket().
     */

    if (handshake->selected_identity != 0) {
        MBEDTLS_SSL_DEBUG_MSG(
            1, ("EarlyData: rejected, the selected key in "
                "`pre_shared_key` is not the first one."));
        return -1;
    }

    if (handshake->ciphersuite_info->id !=
        ssl->session_negotiate->ciphersuite) {
        MBEDTLS_SSL_DEBUG_MSG(
            1, ("EarlyData: rejected, the selected ciphersuite is not the one "
                "of the selected pre-shared key."));
        return -1;

    }

    if (!mbedtls_ssl_tls13_session_ticket_allow_early_data(ssl->session_negotiate)) {
        MBEDTLS_SSL_DEBUG_MSG(
            1,
            ("EarlyData: rejected, early_data not allowed in ticket "
             "permission bits."));
        return -1;
    }

#if defined(MBEDTLS_SSL_ALPN)
    const char *alpn = mbedtls_ssl_get_alpn_protocol(ssl);
    size_t alpn_len;

    if (alpn == NULL && ssl->session_negotiate->ticket_alpn == NULL) {
        return 0;
    }

    if (alpn != NULL) {
        alpn_len = strlen(alpn);
    }

    if (alpn == NULL ||
        ssl->session_negotiate->ticket_alpn == NULL ||
        alpn_len != strlen(ssl->session_negotiate->ticket_alpn) ||
        (memcmp(alpn, ssl->session_negotiate->ticket_alpn, alpn_len) != 0)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("EarlyData: rejected, the selected ALPN is different "
                                  "from the one associated with the pre-shared key."));
        return -1;
    }
#endif

    return 0;
}
#endif /* MBEDTLS_SSL_EARLY_DATA */

/* Update the handshake state machine */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_postprocess_client_hello(mbedtls_ssl_context *ssl,
                                              int hrr_required)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /*
     * Server certificate selection
     */
    if (ssl->conf->f_cert_cb && (ret = ssl->conf->f_cert_cb(ssl)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "f_cert_cb", ret);
        return ret;
    }
#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
    ssl->handshake->sni_name = NULL;
    ssl->handshake->sni_name_len = 0;
#endif /* MBEDTLS_SSL_SERVER_NAME_INDICATION */

    ret = mbedtls_ssl_tls13_key_schedule_stage_early(ssl);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1,
                              "mbedtls_ssl_tls1_3_key_schedule_stage_early", ret);
        return ret;
    }

#if defined(MBEDTLS_SSL_EARLY_DATA)
    if (ssl->handshake->received_extensions & MBEDTLS_SSL_EXT_MASK(EARLY_DATA)) {
        ssl->handshake->early_data_accepted =
            (!hrr_required) && (ssl_tls13_check_early_data_requirements(ssl) == 0);

        if (ssl->handshake->early_data_accepted) {
            ret = mbedtls_ssl_tls13_compute_early_transform(ssl);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(
                    1, "mbedtls_ssl_tls13_compute_early_transform", ret);
                return ret;
            }
        } else {
            ssl->discard_early_data_record =
                hrr_required ?
                MBEDTLS_SSL_EARLY_DATA_DISCARD :
                MBEDTLS_SSL_EARLY_DATA_TRY_TO_DEPROTECT_AND_DISCARD;
        }
    }
#else
    ((void) hrr_required);
#endif /* MBEDTLS_SSL_EARLY_DATA */

    return 0;
}

/*
 * Main entry point from the state machine; orchestrates the otherfunctions.
 */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_process_client_hello(mbedtls_ssl_context *ssl)
{

    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *buf = NULL;
    size_t buflen = 0;
    int parse_client_hello_ret;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse client hello"));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_tls13_fetch_handshake_msg(
                             ssl, MBEDTLS_SSL_HS_CLIENT_HELLO,
                             &buf, &buflen));

    MBEDTLS_SSL_PROC_CHK_NEG(ssl_tls13_parse_client_hello(ssl, buf,
                                                          buf + buflen));
    parse_client_hello_ret = ret; /* Store positive return value of
                                   * parse_client_hello,
                                   * as negative error codes are handled
                                   * by MBEDTLS_SSL_PROC_CHK_NEG. */

    /*
     * Version 1.2 of the protocol has to be used for the handshake.
     * If TLS 1.2 is not supported, abort the handshake. Otherwise, set the
     * ssl->keep_current_message flag for the ClientHello to be kept and parsed
     * as a TLS 1.2 ClientHello. We also change ssl->tls_version to
     * MBEDTLS_SSL_VERSION_TLS1_2 thus from now on mbedtls_ssl_handshake_step()
     * will dispatch to the TLS 1.2 state machine.
     */
    if (SSL_CLIENT_HELLO_TLS1_2 == parse_client_hello_ret) {
        /* Check if server supports TLS 1.2 */
        if (!mbedtls_ssl_conf_is_tls12_enabled(ssl->conf)) {
            MBEDTLS_SSL_DEBUG_MSG(
                1, ("TLS 1.2 not supported."));
            MBEDTLS_SSL_PEND_FATAL_ALERT(
                MBEDTLS_SSL_ALERT_MSG_PROTOCOL_VERSION,
                MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION);
            return MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION;
        }
        ssl->keep_current_message = 1;
        ssl->tls_version = MBEDTLS_SSL_VERSION_TLS1_2;
        MBEDTLS_SSL_DEBUG_MSG(1, ("non-1.3 ClientHello left for later processing"));
        return 0;
    }

    MBEDTLS_SSL_PROC_CHK(
        ssl_tls13_postprocess_client_hello(ssl, parse_client_hello_ret ==
                                           SSL_CLIENT_HELLO_HRR_REQUIRED));

    if (SSL_CLIENT_HELLO_OK == parse_client_hello_ret) {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_SERVER_HELLO);
    } else {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_HELLO_RETRY_REQUEST);
    }

cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse client hello"));
    return ret;
}

/*
 * Handler for MBEDTLS_SSL_SERVER_HELLO
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_prepare_server_hello(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *server_randbytes =
        ssl->handshake->randbytes + MBEDTLS_CLIENT_HELLO_RANDOM_LEN;

    if ((ret = ssl->conf->f_rng(ssl->conf->p_rng, server_randbytes,
                                MBEDTLS_SERVER_HELLO_RANDOM_LEN)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "f_rng", ret);
        return ret;
    }

    MBEDTLS_SSL_DEBUG_BUF(3, "server hello, random bytes", server_randbytes,
                          MBEDTLS_SERVER_HELLO_RANDOM_LEN);

#if defined(MBEDTLS_HAVE_TIME)
    ssl->session_negotiate->start = mbedtls_time(NULL);
#endif /* MBEDTLS_HAVE_TIME */

    return ret;
}

/*
 * ssl_tls13_write_server_hello_supported_versions_ext ():
 *
 * struct {
 *      ProtocolVersion selected_version;
 * } SupportedVersions;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_server_hello_supported_versions_ext(
    mbedtls_ssl_context *ssl,
    unsigned char *buf,
    unsigned char *end,
    size_t *out_len)
{
    *out_len = 0;

    MBEDTLS_SSL_DEBUG_MSG(3, ("server hello, write selected version"));

    /* Check if we have space to write the extension:
     * - extension_type         (2 bytes)
     * - extension_data_length  (2 bytes)
     * - selected_version       (2 bytes)
     */
    MBEDTLS_SSL_CHK_BUF_PTR(buf, end, 6);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_SUPPORTED_VERSIONS, buf, 0);

    MBEDTLS_PUT_UINT16_BE(2, buf, 2);

    mbedtls_ssl_write_version(buf + 4,
                              ssl->conf->transport,
                              ssl->tls_version);

    MBEDTLS_SSL_DEBUG_MSG(3, ("supported version: [%04x]",
                              ssl->tls_version));

    *out_len = 6;

    mbedtls_ssl_tls13_set_hs_sent_ext_mask(
        ssl, MBEDTLS_TLS_EXT_SUPPORTED_VERSIONS);

    return 0;
}



/* Generate and export a single key share. For hybrid KEMs, this can
 * be called multiple times with the different components of the hybrid. */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_generate_and_write_key_share(mbedtls_ssl_context *ssl,
                                                  uint16_t named_group,
                                                  unsigned char *buf,
                                                  unsigned char *end,
                                                  size_t *out_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    *out_len = 0;

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED)
    if (mbedtls_ssl_tls13_named_group_is_ecdhe(named_group) ||
        mbedtls_ssl_tls13_named_group_is_ffdh(named_group)) {
        ret = mbedtls_ssl_tls13_generate_and_write_xxdh_key_exchange(
            ssl, named_group, buf, end, out_len);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(
                1, "mbedtls_ssl_tls13_generate_and_write_xxdh_key_exchange",
                ret);
            return ret;
        }
    } else
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED */
    if (0 /* Other kinds of KEMs */) {
    } else {
        ((void) ssl);
        ((void) named_group);
        ((void) buf);
        ((void) end);
        ret = MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return ret;
}

/*
 * ssl_tls13_write_key_share_ext
 *
 * Structure of key_share extension in ServerHello:
 *
 * struct {
 *     NamedGroup group;
 *     opaque key_exchange<1..2^16-1>;
 * } KeyShareEntry;
 * struct {
 *     KeyShareEntry server_share;
 * } KeyShareServerHello;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_key_share_ext(mbedtls_ssl_context *ssl,
                                         unsigned char *buf,
                                         unsigned char *end,
                                         size_t *out_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    uint16_t group = ssl->handshake->offered_group_id;
    unsigned char *server_share = buf + 4;
    size_t key_exchange_length;

    *out_len = 0;

    MBEDTLS_SSL_DEBUG_MSG(3, ("server hello, adding key share extension"));

    MBEDTLS_SSL_DEBUG_MSG(2, ("server hello, write selected_group: %s (%04x)",
                              mbedtls_ssl_named_group_to_str(group),
                              group));

    /* Check if we have space for header and length fields:
     * - extension_type         (2 bytes)
     * - extension_data_length  (2 bytes)
     * - group                  (2 bytes)
     * - key_exchange_length    (2 bytes)
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 8);
    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_KEY_SHARE, p, 0);
    MBEDTLS_PUT_UINT16_BE(group, server_share, 0);
    p += 8;

    /* When we introduce PQC-ECDHE hybrids, we'll want to call this
     * function multiple times. */
    ret = ssl_tls13_generate_and_write_key_share(
        ssl, group, server_share + 4, end, &key_exchange_length);
    if (ret != 0) {
        return ret;
    }
    p += key_exchange_length;

    MBEDTLS_PUT_UINT16_BE(key_exchange_length, server_share + 2, 0);

    MBEDTLS_PUT_UINT16_BE(p - server_share, buf, 2);

    *out_len = p - buf;

    mbedtls_ssl_tls13_set_hs_sent_ext_mask(ssl, MBEDTLS_TLS_EXT_KEY_SHARE);

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_hrr_key_share_ext(mbedtls_ssl_context *ssl,
                                             unsigned char *buf,
                                             unsigned char *end,
                                             size_t *out_len)
{
    uint16_t selected_group = ssl->handshake->hrr_selected_group;
    /* key_share Extension
     *
     *  struct {
     *    select (Handshake.msg_type) {
     *      ...
     *      case hello_retry_request:
     *          NamedGroup selected_group;
     *      ...
     *    };
     * } KeyShare;
     */

    *out_len = 0;

    /*
     * For a pure PSK key exchange, there is no group to agree upon. The purpose
     * of the HRR is then to transmit a cookie to force the client to demonstrate
     * reachability at their apparent network address (primarily useful for DTLS).
     */
    if (!mbedtls_ssl_tls13_key_exchange_mode_with_ephemeral(ssl)) {
        return 0;
    }

    /* We should only send the key_share extension if the client's initial
     * key share was not acceptable. */
    if (ssl->handshake->offered_group_id != 0) {
        MBEDTLS_SSL_DEBUG_MSG(4, ("Skip key_share extension in HRR"));
        return 0;
    }

    if (selected_group == 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("no matching named group found"));
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    /* Check if we have enough space:
     * - extension_type         (2 bytes)
     * - extension_data_length  (2 bytes)
     * - selected_group         (2 bytes)
     */
    MBEDTLS_SSL_CHK_BUF_PTR(buf, end, 6);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_KEY_SHARE, buf, 0);
    MBEDTLS_PUT_UINT16_BE(2, buf, 2);
    MBEDTLS_PUT_UINT16_BE(selected_group, buf, 4);

    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("HRR selected_group: %s (%x)",
                           mbedtls_ssl_named_group_to_str(selected_group),
                           selected_group));

    *out_len = 6;

    mbedtls_ssl_tls13_set_hs_sent_ext_mask(ssl, MBEDTLS_TLS_EXT_KEY_SHARE);

    return 0;
}

/*
 * Structure of ServerHello message:
 *
 *     struct {
 *        ProtocolVersion legacy_version = 0x0303;    // TLS v1.2
 *        Random random;
 *        opaque legacy_session_id_echo<0..32>;
 *        CipherSuite cipher_suite;
 *        uint8 legacy_compression_method = 0;
 *        Extension extensions<6..2^16-1>;
 *    } ServerHello;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_server_hello_body(mbedtls_ssl_context *ssl,
                                             unsigned char *buf,
                                             unsigned char *end,
                                             size_t *out_len,
                                             int is_hrr)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    unsigned char *p_extensions_len;
    size_t output_len;

    *out_len = 0;
    ssl->handshake->sent_extensions = MBEDTLS_SSL_EXT_MASK_NONE;

    /* ...
     * ProtocolVersion legacy_version = 0x0303; // TLS 1.2
     * ...
     * with ProtocolVersion defined as:
     * uint16 ProtocolVersion;
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 2);
    MBEDTLS_PUT_UINT16_BE(0x0303, p, 0);
    p += 2;

    /* ...
     * Random random;
     * ...
     * with Random defined as:
     * opaque Random[MBEDTLS_SERVER_HELLO_RANDOM_LEN];
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, MBEDTLS_SERVER_HELLO_RANDOM_LEN);
    if (is_hrr) {
        memcpy(p, mbedtls_ssl_tls13_hello_retry_request_magic,
               MBEDTLS_SERVER_HELLO_RANDOM_LEN);
    } else {
        memcpy(p, &ssl->handshake->randbytes[MBEDTLS_CLIENT_HELLO_RANDOM_LEN],
               MBEDTLS_SERVER_HELLO_RANDOM_LEN);
    }
    MBEDTLS_SSL_DEBUG_BUF(3, "server hello, random bytes",
                          p, MBEDTLS_SERVER_HELLO_RANDOM_LEN);
    p += MBEDTLS_SERVER_HELLO_RANDOM_LEN;

    /* ...
     * opaque legacy_session_id_echo<0..32>;
     * ...
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 1 + ssl->session_negotiate->id_len);
    *p++ = (unsigned char) ssl->session_negotiate->id_len;
    if (ssl->session_negotiate->id_len > 0) {
        memcpy(p, &ssl->session_negotiate->id[0],
               ssl->session_negotiate->id_len);
        p += ssl->session_negotiate->id_len;

        MBEDTLS_SSL_DEBUG_BUF(3, "session id", ssl->session_negotiate->id,
                              ssl->session_negotiate->id_len);
    }

    /* ...
     * CipherSuite cipher_suite;
     * ...
     * with CipherSuite defined as:
     * uint8 CipherSuite[2];
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 2);
    MBEDTLS_PUT_UINT16_BE(ssl->session_negotiate->ciphersuite, p, 0);
    p += 2;
    MBEDTLS_SSL_DEBUG_MSG(3,
                          ("server hello, chosen ciphersuite: %s ( id=%d )",
                           mbedtls_ssl_get_ciphersuite_name(
                               ssl->session_negotiate->ciphersuite),
                           ssl->session_negotiate->ciphersuite));

    /* ...
     * uint8 legacy_compression_method = 0;
     * ...
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 1);
    *p++ = MBEDTLS_SSL_COMPRESS_NULL;

    /* ...
     * Extension extensions<6..2^16-1>;
     * ...
     * struct {
     *      ExtensionType extension_type; (2 bytes)
     *      opaque extension_data<0..2^16-1>;
     * } Extension;
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 2);
    p_extensions_len = p;
    p += 2;

    if ((ret = ssl_tls13_write_server_hello_supported_versions_ext(
             ssl, p, end, &output_len)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(
            1, "ssl_tls13_write_server_hello_supported_versions_ext", ret);
        return ret;
    }
    p += output_len;

    if (mbedtls_ssl_tls13_key_exchange_mode_with_ephemeral(ssl)) {
        if (is_hrr) {
            ret = ssl_tls13_write_hrr_key_share_ext(ssl, p, end, &output_len);
        } else {
            ret = ssl_tls13_write_key_share_ext(ssl, p, end, &output_len);
        }
        if (ret != 0) {
            return ret;
        }
        p += output_len;
    }

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
    if (!is_hrr && mbedtls_ssl_tls13_key_exchange_mode_with_psk(ssl)) {
        ret = ssl_tls13_write_server_pre_shared_key_ext(ssl, p, end, &output_len);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "ssl_tls13_write_server_pre_shared_key_ext",
                                  ret);
            return ret;
        }
        p += output_len;
    }
#endif

    MBEDTLS_PUT_UINT16_BE(p - p_extensions_len - 2, p_extensions_len, 0);

    MBEDTLS_SSL_DEBUG_BUF(4, "server hello extensions",
                          p_extensions_len, p - p_extensions_len);

    *out_len = p - buf;

    MBEDTLS_SSL_DEBUG_BUF(3, "server hello", buf, *out_len);

    MBEDTLS_SSL_PRINT_EXTS(
        3, is_hrr ? MBEDTLS_SSL_TLS1_3_HS_HELLO_RETRY_REQUEST :
        MBEDTLS_SSL_HS_SERVER_HELLO,
        ssl->handshake->sent_extensions);

    return ret;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_finalize_server_hello(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    ret = mbedtls_ssl_tls13_compute_handshake_transform(ssl);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1,
                              "mbedtls_ssl_tls13_compute_handshake_transform",
                              ret);
        return ret;
    }

    return ret;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_server_hello(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *buf;
    size_t buf_len, msg_len;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write server hello"));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_prepare_server_hello(ssl));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_start_handshake_msg(
                             ssl, MBEDTLS_SSL_HS_SERVER_HELLO, &buf, &buf_len));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_server_hello_body(ssl, buf,
                                                           buf + buf_len,
                                                           &msg_len,
                                                           0));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                             ssl, MBEDTLS_SSL_HS_SERVER_HELLO, buf, msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_finish_handshake_msg(
                             ssl, buf_len, msg_len));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_finalize_server_hello(ssl));

#if defined(MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE)
    /* The server sends a dummy change_cipher_spec record immediately
     * after its first handshake message. This may either be after
     * a ServerHello or a HelloRetryRequest.
     */
    mbedtls_ssl_handshake_set_state(
        ssl, MBEDTLS_SSL_SERVER_CCS_AFTER_SERVER_HELLO);
#else
    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_ENCRYPTED_EXTENSIONS);
#endif /* MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE */

cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write server hello"));
    return ret;
}


/*
 * Handler for MBEDTLS_SSL_HELLO_RETRY_REQUEST
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_prepare_hello_retry_request(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    if (ssl->handshake->hello_retry_request_flag) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Too many HRRs"));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE,
                                     MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    /*
     * Create stateless transcript hash for HRR
     */
    MBEDTLS_SSL_DEBUG_MSG(4, ("Reset transcript for HRR"));
    ret = mbedtls_ssl_reset_transcript_for_hrr(ssl);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_reset_transcript_for_hrr", ret);
        return ret;
    }
    mbedtls_ssl_session_reset_msg_layer(ssl, 0);

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_hello_retry_request(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *buf;
    size_t buf_len, msg_len;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write hello retry request"));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_prepare_hello_retry_request(ssl));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_start_handshake_msg(
                             ssl, MBEDTLS_SSL_HS_SERVER_HELLO,
                             &buf, &buf_len));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_server_hello_body(ssl, buf,
                                                           buf + buf_len,
                                                           &msg_len,
                                                           1));
    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                             ssl, MBEDTLS_SSL_HS_SERVER_HELLO, buf, msg_len));


    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_finish_handshake_msg(ssl, buf_len,
                                                          msg_len));

    ssl->handshake->hello_retry_request_flag = 1;

#if defined(MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE)
    /* The server sends a dummy change_cipher_spec record immediately
     * after its first handshake message. This may either be after
     * a ServerHello or a HelloRetryRequest.
     */
    mbedtls_ssl_handshake_set_state(
        ssl, MBEDTLS_SSL_SERVER_CCS_AFTER_HELLO_RETRY_REQUEST);
#else
    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CLIENT_HELLO);
#endif /* MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE */

cleanup:
    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write hello retry request"));
    return ret;
}

/*
 * Handler for MBEDTLS_SSL_ENCRYPTED_EXTENSIONS
 */

/*
 * struct {
 *    Extension extensions<0..2 ^ 16 - 1>;
 * } EncryptedExtensions;
 *
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_encrypted_extensions_body(mbedtls_ssl_context *ssl,
                                                     unsigned char *buf,
                                                     unsigned char *end,
                                                     size_t *out_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    size_t extensions_len = 0;
    unsigned char *p_extensions_len;
    size_t output_len;

    *out_len = 0;

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 2);
    p_extensions_len = p;
    p += 2;

    ((void) ssl);
    ((void) ret);
    ((void) output_len);

#if defined(MBEDTLS_SSL_ALPN)
    ret = mbedtls_ssl_write_alpn_ext(ssl, p, end, &output_len);
    if (ret != 0) {
        return ret;
    }
    p += output_len;
#endif /* MBEDTLS_SSL_ALPN */

#if defined(MBEDTLS_SSL_EARLY_DATA)
    if (ssl->handshake->early_data_accepted) {
        ret = mbedtls_ssl_tls13_write_early_data_ext(
            ssl, 0, p, end, &output_len);
        if (ret != 0) {
            return ret;
        }
        p += output_len;
    }
#endif /* MBEDTLS_SSL_EARLY_DATA */

#if defined(MBEDTLS_SSL_RECORD_SIZE_LIMIT)
    if (ssl->handshake->received_extensions & MBEDTLS_SSL_EXT_MASK(RECORD_SIZE_LIMIT)) {
        ret = mbedtls_ssl_tls13_write_record_size_limit_ext(
            ssl, p, end, &output_len);
        if (ret != 0) {
            return ret;
        }
        p += output_len;
    }
#endif

    extensions_len = (p - p_extensions_len) - 2;
    MBEDTLS_PUT_UINT16_BE(extensions_len, p_extensions_len, 0);

    *out_len = p - buf;

    MBEDTLS_SSL_DEBUG_BUF(4, "encrypted extensions", buf, *out_len);

    MBEDTLS_SSL_PRINT_EXTS(
        3, MBEDTLS_SSL_HS_ENCRYPTED_EXTENSIONS, ssl->handshake->sent_extensions);

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_encrypted_extensions(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *buf;
    size_t buf_len, msg_len;

    mbedtls_ssl_set_outbound_transform(ssl,
                                       ssl->handshake->transform_handshake);
    MBEDTLS_SSL_DEBUG_MSG(
        3, ("switching to handshake transform for outbound data"));

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write encrypted extensions"));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_start_handshake_msg(
                             ssl, MBEDTLS_SSL_HS_ENCRYPTED_EXTENSIONS,
                             &buf, &buf_len));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_encrypted_extensions_body(
                             ssl, buf, buf + buf_len, &msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                             ssl, MBEDTLS_SSL_HS_ENCRYPTED_EXTENSIONS,
                             buf, msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_finish_handshake_msg(
                             ssl, buf_len, msg_len));

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
    if (mbedtls_ssl_tls13_key_exchange_mode_with_psk(ssl)) {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_SERVER_FINISHED);
    } else {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CERTIFICATE_REQUEST);
    }
#else
    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_SERVER_FINISHED);
#endif

cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write encrypted extensions"));
    return ret;
}

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#define SSL_CERTIFICATE_REQUEST_SEND_REQUEST 0
#define SSL_CERTIFICATE_REQUEST_SKIP         1
/* Coordination:
 * Check whether a CertificateRequest message should be written.
 * Returns a negative code on failure, or
 * - SSL_CERTIFICATE_REQUEST_SEND_REQUEST
 * - SSL_CERTIFICATE_REQUEST_SKIP
 * indicating if the writing of the CertificateRequest
 * should be skipped or not.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_certificate_request_coordinate(mbedtls_ssl_context *ssl)
{
    int authmode;

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
    if (ssl->handshake->sni_authmode != MBEDTLS_SSL_VERIFY_UNSET) {
        authmode = ssl->handshake->sni_authmode;
    } else
#endif
    authmode = ssl->conf->authmode;

    if (authmode == MBEDTLS_SSL_VERIFY_NONE) {
        ssl->session_negotiate->verify_result = MBEDTLS_X509_BADCERT_SKIP_VERIFY;
        return SSL_CERTIFICATE_REQUEST_SKIP;
    }

    ssl->handshake->certificate_request_sent = 1;

    return SSL_CERTIFICATE_REQUEST_SEND_REQUEST;
}

/*
 * struct {
 *   opaque certificate_request_context<0..2^8-1>;
 *   Extension extensions<2..2^16-1>;
 * } CertificateRequest;
 *
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_certificate_request_body(mbedtls_ssl_context *ssl,
                                                    unsigned char *buf,
                                                    const unsigned char *end,
                                                    size_t *out_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    size_t output_len = 0;
    unsigned char *p_extensions_len;

    *out_len = 0;

    /* Check if we have enough space:
     * - certificate_request_context (1 byte)
     * - extensions length           (2 bytes)
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 3);

    /*
     * Write certificate_request_context
     */
    /*
     * We use a zero length context for the normal handshake
     * messages. For post-authentication handshake messages
     * this request context would be set to a non-zero value.
     */
    *p++ = 0x0;

    /*
     * Write extensions
     */
    /* The extensions must contain the signature_algorithms. */
    p_extensions_len = p;
    p += 2;
    ret = mbedtls_ssl_write_sig_alg_ext(ssl, p, end, &output_len);
    if (ret != 0) {
        return ret;
    }

    p += output_len;
    MBEDTLS_PUT_UINT16_BE(p - p_extensions_len - 2, p_extensions_len, 0);

    *out_len = p - buf;

    MBEDTLS_SSL_PRINT_EXTS(
        3, MBEDTLS_SSL_HS_CERTIFICATE_REQUEST, ssl->handshake->sent_extensions);

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_certificate_request(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write certificate request"));

    MBEDTLS_SSL_PROC_CHK_NEG(ssl_tls13_certificate_request_coordinate(ssl));

    if (ret == SSL_CERTIFICATE_REQUEST_SEND_REQUEST) {
        unsigned char *buf;
        size_t buf_len, msg_len;

        MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_start_handshake_msg(
                                 ssl, MBEDTLS_SSL_HS_CERTIFICATE_REQUEST,
                                 &buf, &buf_len));

        MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_certificate_request_body(
                                 ssl, buf, buf + buf_len, &msg_len));

        MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                                 ssl, MBEDTLS_SSL_HS_CERTIFICATE_REQUEST,
                                 buf, msg_len));

        MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_finish_handshake_msg(
                                 ssl, buf_len, msg_len));
    } else if (ret == SSL_CERTIFICATE_REQUEST_SKIP) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("<= skip write certificate request"));
        ret = 0;
    } else {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        ret = MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        goto cleanup;
    }

    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_SERVER_CERTIFICATE);
cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write certificate request"));
    return ret;
}

/*
 * Handler for MBEDTLS_SSL_SERVER_CERTIFICATE
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_server_certificate(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(MBEDTLS_X509_CRT_PARSE_C)
    if ((ssl_tls13_pick_key_cert(ssl) != 0) ||
        mbedtls_ssl_own_cert(ssl) == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("No certificate available."));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE,
                                     MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }
#endif /* MBEDTLS_X509_CRT_PARSE_C */

    ret = mbedtls_ssl_tls13_write_certificate(ssl);
    if (ret != 0) {
        return ret;
    }
    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CERTIFICATE_VERIFY);
    return 0;
}

/*
 * Handler for MBEDTLS_SSL_CERTIFICATE_VERIFY
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_certificate_verify(mbedtls_ssl_context *ssl)
{
    int ret = mbedtls_ssl_tls13_write_certificate_verify(ssl);
    if (ret != 0) {
        return ret;
    }
    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_SERVER_FINISHED);
    return 0;
}
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

/*
 * RFC 8446 section A.2
 *
 *                                | Send ServerHello
 *                                | K_send = handshake
 *                                | Send EncryptedExtensions
 *                                | [Send CertificateRequest]
 * Can send                       | [Send Certificate + CertificateVerify]
 * app data                       | Send Finished
 * after   -->                    | K_send = application
 * here                  +--------+--------+
 *              No 0-RTT |                 | 0-RTT
 *                       |                 |
 *   K_recv = handshake  |                 | K_recv = early data
 * [Skip decrypt errors] |    +------> WAIT_EOED -+
 *                       |    |       Recv |      | Recv EndOfEarlyData
 *                       |    | early data |      | K_recv = handshake
 *                       |    +------------+      |
 *                       |                        |
 *                       +> WAIT_FLIGHT2 <--------+
 *                                |
 *                       +--------+--------+
 *               No auth |                 | Client auth
 *                       |                 |
 *                       |                 v
 *                       |             WAIT_CERT
 *                       |        Recv |       | Recv Certificate
 *                       |       empty |       v
 *                       | Certificate |    WAIT_CV
 *                       |             |       | Recv
 *                       |             v       | CertificateVerify
 *                       +-> WAIT_FINISHED <---+
 *                                | Recv Finished
 *
 *
 * The following function handles the state changes after WAIT_FLIGHT2 in the
 * above diagram. We are not going to receive early data related messages
 * anymore, prepare to receive the first handshake message of the client
 * second flight.
 */
static void ssl_tls13_prepare_for_handshake_second_flight(
    mbedtls_ssl_context *ssl)
{
    if (ssl->handshake->certificate_request_sent) {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CLIENT_CERTIFICATE);
    } else {
        MBEDTLS_SSL_DEBUG_MSG(2, ("skip parse certificate"));
        MBEDTLS_SSL_DEBUG_MSG(2, ("skip parse certificate verify"));

        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CLIENT_FINISHED);
    }
}

/*
 * Handler for MBEDTLS_SSL_SERVER_FINISHED
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_server_finished(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = mbedtls_ssl_tls13_write_finished_message(ssl);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_ssl_tls13_compute_application_transform(ssl);
    if (ret != 0) {
        MBEDTLS_SSL_PEND_FATAL_ALERT(
            MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE,
            MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
        return ret;
    }

#if defined(MBEDTLS_SSL_EARLY_DATA)
    if (ssl->handshake->early_data_accepted) {
        /* See RFC 8446 section A.2 for more information */
        MBEDTLS_SSL_DEBUG_MSG(
            1, ("Switch to early keys for inbound traffic. "
                "( K_recv = early data )"));
        mbedtls_ssl_set_inbound_transform(
            ssl, ssl->handshake->transform_earlydata);
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_END_OF_EARLY_DATA);
        return 0;
    }
#endif /* MBEDTLS_SSL_EARLY_DATA */
    MBEDTLS_SSL_DEBUG_MSG(
        1, ("Switch to handshake keys for inbound traffic "
            "( K_recv = handshake )"));
    mbedtls_ssl_set_inbound_transform(ssl, ssl->handshake->transform_handshake);

    ssl_tls13_prepare_for_handshake_second_flight(ssl);

    return 0;
}

#if defined(MBEDTLS_SSL_EARLY_DATA)
/*
 * Handler for MBEDTLS_SSL_END_OF_EARLY_DATA
 */
#define SSL_GOT_END_OF_EARLY_DATA      0
#define SSL_GOT_EARLY_DATA             1
/* Coordination:
 * Deals with the ambiguity of not knowing if the next message is an
 * EndOfEarlyData message or an application message containing early data.
 * Returns a negative code on failure, or
 * - SSL_GOT_END_OF_EARLY_DATA
 * - SSL_GOT_EARLY_DATA
 * indicating which message is received.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_end_of_early_data_coordinate(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if ((ret = mbedtls_ssl_read_record(ssl, 0)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
        return ret;
    }
    ssl->keep_current_message = 1;

    if (ssl->in_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE        &&
        ssl->in_msg[0]  == MBEDTLS_SSL_HS_END_OF_EARLY_DATA) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("Received an end_of_early_data message."));
        return SSL_GOT_END_OF_EARLY_DATA;
    }

    if (ssl->in_msgtype == MBEDTLS_SSL_MSG_APPLICATION_DATA) {
        if (ssl->in_offt == NULL) {
            MBEDTLS_SSL_DEBUG_MSG(3, ("Received early data"));
            /* Set the reading pointer */
            ssl->in_offt = ssl->in_msg;
            ret = mbedtls_ssl_tls13_check_early_data_len(ssl, ssl->in_msglen);
            if (ret != 0) {
                return ret;
            }
        }
        return SSL_GOT_EARLY_DATA;
    }

    MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE,
                                 MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE);
    return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_end_of_early_data(mbedtls_ssl_context *ssl,
                                             const unsigned char *buf,
                                             const unsigned char *end)
{
    /* RFC 8446 section 4.5
     *
     * struct {} EndOfEarlyData;
     */
    if (buf != end) {
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR,
                                     MBEDTLS_ERR_SSL_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }
    return 0;
}

/*
 * RFC 8446 section A.2
 *
 *                                | Send ServerHello
 *                                | K_send = handshake
 *                                | Send EncryptedExtensions
 *                                | [Send CertificateRequest]
 * Can send                       | [Send Certificate + CertificateVerify]
 * app data                       | Send Finished
 * after   -->                    | K_send = application
 * here                  +--------+--------+
 *              No 0-RTT |                 | 0-RTT
 *                       |                 |
 *   K_recv = handshake  |                 | K_recv = early data
 * [Skip decrypt errors] |    +------> WAIT_EOED -+
 *                       |    |       Recv |      | Recv EndOfEarlyData
 *                       |    | early data |      | K_recv = handshake
 *                       |    +------------+      |
 *                       |                        |
 *                       +> WAIT_FLIGHT2 <--------+
 *                                |
 *                       +--------+--------+
 *               No auth |                 | Client auth
 *                       |                 |
 *                       |                 v
 *                       |             WAIT_CERT
 *                       |        Recv |       | Recv Certificate
 *                       |       empty |       v
 *                       | Certificate |    WAIT_CV
 *                       |             |       | Recv
 *                       |             v       | CertificateVerify
 *                       +-> WAIT_FINISHED <---+
 *                                | Recv Finished
 *
 * The function handles actions and state changes from 0-RTT to WAIT_FLIGHT2 in
 * the above diagram.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_process_end_of_early_data(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> ssl_tls13_process_end_of_early_data"));

    MBEDTLS_SSL_PROC_CHK_NEG(ssl_tls13_end_of_early_data_coordinate(ssl));

    if (ret == SSL_GOT_END_OF_EARLY_DATA) {
        unsigned char *buf;
        size_t buf_len;

        MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_tls13_fetch_handshake_msg(
                                 ssl, MBEDTLS_SSL_HS_END_OF_EARLY_DATA,
                                 &buf, &buf_len));

        MBEDTLS_SSL_PROC_CHK(ssl_tls13_parse_end_of_early_data(
                                 ssl, buf, buf + buf_len));

        MBEDTLS_SSL_DEBUG_MSG(
            1, ("Switch to handshake keys for inbound traffic"
                "( K_recv = handshake )"));
        mbedtls_ssl_set_inbound_transform(
            ssl, ssl->handshake->transform_handshake);

        MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                                 ssl, MBEDTLS_SSL_HS_END_OF_EARLY_DATA,
                                 buf, buf_len));

        ssl_tls13_prepare_for_handshake_second_flight(ssl);

    } else if (ret == SSL_GOT_EARLY_DATA) {
        ret = MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA;
        goto cleanup;
    } else {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        ret = MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        goto cleanup;
    }

cleanup:
    MBEDTLS_SSL_DEBUG_MSG(2, ("<= ssl_tls13_process_end_of_early_data"));
    return ret;
}
#endif /* MBEDTLS_SSL_EARLY_DATA */

/*
 * Handler for MBEDTLS_SSL_CLIENT_FINISHED
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_process_client_finished(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = mbedtls_ssl_tls13_process_finished_message(ssl);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_ssl_tls13_compute_resumption_master_secret(ssl);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(
            1, "mbedtls_ssl_tls13_compute_resumption_master_secret", ret);
    }

    mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_HANDSHAKE_WRAPUP);
    return 0;
}

/*
 * Handler for MBEDTLS_SSL_HANDSHAKE_WRAPUP
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_handshake_wrapup(mbedtls_ssl_context *ssl)
{
    MBEDTLS_SSL_DEBUG_MSG(2, ("handshake: done"));

    mbedtls_ssl_tls13_handshake_wrapup(ssl);

#if defined(MBEDTLS_SSL_SESSION_TICKETS) && \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
/* TODO: Remove the check of SOME_PSK_ENABLED since SESSION_TICKETS requires
 *       SOME_PSK_ENABLED to be enabled. Here is just to make CI happy. It is
 *       expected to be resolved with issue#6395.
 */
    /* Sent NewSessionTicket message only when client supports PSK */
    if (mbedtls_ssl_tls13_is_some_psk_supported(ssl)) {
        mbedtls_ssl_handshake_set_state(
            ssl, MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET);
    } else
#endif
    {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_HANDSHAKE_OVER);
    }
    return 0;
}

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
/*
 * Handler for MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET
 */
#define SSL_NEW_SESSION_TICKET_SKIP  0
#define SSL_NEW_SESSION_TICKET_WRITE 1
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_new_session_ticket_coordinate(mbedtls_ssl_context *ssl)
{
    /* Check whether the use of session tickets is enabled */
    if (ssl->conf->f_ticket_write == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("NewSessionTicket: disabled,"
                                  " callback is not set"));
        return SSL_NEW_SESSION_TICKET_SKIP;
    }
    if (ssl->conf->new_session_tickets_count == 0) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("NewSessionTicket: disabled,"
                                  " configured count is zero"));
        return SSL_NEW_SESSION_TICKET_SKIP;
    }

    if (ssl->handshake->new_session_tickets_count == 0) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("NewSessionTicket: all tickets have "
                                  "been sent."));
        return SSL_NEW_SESSION_TICKET_SKIP;
    }

    return SSL_NEW_SESSION_TICKET_WRITE;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_prepare_new_session_ticket(mbedtls_ssl_context *ssl,
                                                unsigned char *ticket_nonce,
                                                size_t ticket_nonce_size)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ssl_session *session = ssl->session;
    mbedtls_ssl_ciphersuite_t *ciphersuite_info;
    psa_algorithm_t psa_hash_alg;
    int hash_length;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> prepare NewSessionTicket msg"));

    /* Set ticket_flags depends on the advertised psk key exchange mode */
    mbedtls_ssl_tls13_session_clear_ticket_flags(
        session, MBEDTLS_SSL_TLS1_3_TICKET_FLAGS_MASK);
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
    mbedtls_ssl_tls13_session_set_ticket_flags(
        session, ssl->handshake->tls13_kex_modes);
#endif

#if defined(MBEDTLS_SSL_EARLY_DATA)
    if (ssl->conf->early_data_enabled == MBEDTLS_SSL_EARLY_DATA_ENABLED &&
        ssl->conf->max_early_data_size > 0) {
        mbedtls_ssl_tls13_session_set_ticket_flags(
            session, MBEDTLS_SSL_TLS1_3_TICKET_ALLOW_EARLY_DATA);
        session->max_early_data_size = ssl->conf->max_early_data_size;
    }
#endif /* MBEDTLS_SSL_EARLY_DATA */

    MBEDTLS_SSL_PRINT_TICKET_FLAGS(4, session->ticket_flags);

#if defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_ALPN)
    if (session->ticket_alpn == NULL) {
        ret = mbedtls_ssl_session_set_ticket_alpn(session, ssl->alpn_chosen);
        if (ret != 0) {
            return ret;
        }
    }
#endif

    /* Generate ticket_age_add */
    if ((ret = ssl->conf->f_rng(ssl->conf->p_rng,
                                (unsigned char *) &session->ticket_age_add,
                                sizeof(session->ticket_age_add)) != 0)) {
        MBEDTLS_SSL_DEBUG_RET(1, "generate_ticket_age_add", ret);
        return ret;
    }
    MBEDTLS_SSL_DEBUG_MSG(3, ("ticket_age_add: %u",
                              (unsigned int) session->ticket_age_add));

    /* Generate ticket_nonce */
    ret = ssl->conf->f_rng(ssl->conf->p_rng, ticket_nonce, ticket_nonce_size);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "generate_ticket_nonce", ret);
        return ret;
    }
    MBEDTLS_SSL_DEBUG_BUF(3, "ticket_nonce:",
                          ticket_nonce, ticket_nonce_size);

    ciphersuite_info =
        (mbedtls_ssl_ciphersuite_t *) ssl->handshake->ciphersuite_info;
    psa_hash_alg = mbedtls_md_psa_alg_from_type((mbedtls_md_type_t) ciphersuite_info->mac);
    hash_length = PSA_HASH_LENGTH(psa_hash_alg);
    if (hash_length == -1 ||
        (size_t) hash_length > sizeof(session->resumption_key)) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    /* In this code the psk key length equals the length of the hash */
    session->resumption_key_len = hash_length;
    session->ciphersuite = ciphersuite_info->id;

    /* Compute resumption key
     *
     *  HKDF-Expand-Label( resumption_master_secret,
     *                    "resumption", ticket_nonce, Hash.length )
     */
    ret = mbedtls_ssl_tls13_hkdf_expand_label(
        psa_hash_alg,
        session->app_secrets.resumption_master_secret,
        hash_length,
        MBEDTLS_SSL_TLS1_3_LBL_WITH_LEN(resumption),
        ticket_nonce,
        ticket_nonce_size,
        session->resumption_key,
        hash_length);

    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(2,
                              "Creating the ticket-resumed PSK failed",
                              ret);
        return ret;
    }
    MBEDTLS_SSL_DEBUG_BUF(3, "Ticket-resumed PSK",
                          session->resumption_key,
                          session->resumption_key_len);

    MBEDTLS_SSL_DEBUG_BUF(3, "resumption_master_secret",
                          session->app_secrets.resumption_master_secret,
                          hash_length);

    return 0;
}

/* This function creates a NewSessionTicket message in the following format:
 *
 * struct {
 *    uint32 ticket_lifetime;
 *    uint32 ticket_age_add;
 *    opaque ticket_nonce<0..255>;
 *    opaque ticket<1..2^16-1>;
 *    Extension extensions<0..2^16-2>;
 * } NewSessionTicket;
 *
 * The ticket inside the NewSessionTicket message is an encrypted container
 * carrying the necessary information so that the server is later able to
 * re-start the communication.
 *
 * The following fields are placed inside the ticket by the
 * f_ticket_write() function:
 *
 *  - creation time (ticket_creation_time)
 *  - flags (ticket_flags)
 *  - age add (ticket_age_add)
 *  - key (resumption_key)
 *  - key length (resumption_key_len)
 *  - ciphersuite (ciphersuite)
 *  - max_early_data_size (max_early_data_size)
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_new_session_ticket_body(mbedtls_ssl_context *ssl,
                                                   unsigned char *buf,
                                                   unsigned char *end,
                                                   size_t *out_len,
                                                   unsigned char *ticket_nonce,
                                                   size_t ticket_nonce_size)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    mbedtls_ssl_session *session = ssl->session;
    size_t ticket_len;
    uint32_t ticket_lifetime;
    unsigned char *p_extensions_len;

    *out_len = 0;
    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write NewSessionTicket msg"));

    /*
     *    ticket_lifetime   4 bytes
     *    ticket_age_add    4 bytes
     *    ticket_nonce      1 + ticket_nonce_size bytes
     *    ticket            >=2 bytes
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 4 + 4 + 1 + ticket_nonce_size + 2);

    /* Generate ticket and ticket_lifetime */
#if defined(MBEDTLS_HAVE_TIME)
    session->ticket_creation_time = mbedtls_ms_time();
#endif
    ret = ssl->conf->f_ticket_write(ssl->conf->p_ticket,
                                    session,
                                    p + 9 + ticket_nonce_size + 2,
                                    end,
                                    &ticket_len,
                                    &ticket_lifetime);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "write_ticket", ret);
        return ret;
    }

    /* RFC 8446 section 4.6.1
     *
     *  ticket_lifetime:  Indicates the lifetime in seconds as a 32-bit
     *     unsigned integer in network byte order from the time of ticket
     *     issuance.  Servers MUST NOT use any value greater than
     *     604800 seconds (7 days) ...
     */
    if (ticket_lifetime > MBEDTLS_SSL_TLS1_3_MAX_ALLOWED_TICKET_LIFETIME) {
        MBEDTLS_SSL_DEBUG_MSG(
            1, ("Ticket lifetime (%u) is greater than 7 days.",
                (unsigned int) ticket_lifetime));
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    MBEDTLS_PUT_UINT32_BE(ticket_lifetime, p, 0);
    MBEDTLS_SSL_DEBUG_MSG(3, ("ticket_lifetime: %u",
                              (unsigned int) ticket_lifetime));

    /* Write ticket_age_add */
    MBEDTLS_PUT_UINT32_BE(session->ticket_age_add, p, 4);
    MBEDTLS_SSL_DEBUG_MSG(3, ("ticket_age_add: %u",
                              (unsigned int) session->ticket_age_add));

    /* Write ticket_nonce */
    p[8] = (unsigned char) ticket_nonce_size;
    if (ticket_nonce_size > 0) {
        memcpy(p + 9, ticket_nonce, ticket_nonce_size);
    }
    p += 9 + ticket_nonce_size;

    /* Write ticket */
    MBEDTLS_PUT_UINT16_BE(ticket_len, p, 0);
    p += 2;
    MBEDTLS_SSL_DEBUG_BUF(4, "ticket", p, ticket_len);
    p += ticket_len;

    /* Ticket Extensions
     *
     * Extension extensions<0..2^16-2>;
     */
    ssl->handshake->sent_extensions = MBEDTLS_SSL_EXT_MASK_NONE;

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 2);
    p_extensions_len = p;
    p += 2;

#if defined(MBEDTLS_SSL_EARLY_DATA)
    if (mbedtls_ssl_tls13_session_ticket_allow_early_data(session)) {
        size_t output_len;

        if ((ret = mbedtls_ssl_tls13_write_early_data_ext(
                 ssl, 1, p, end, &output_len)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(
                1, "mbedtls_ssl_tls13_write_early_data_ext", ret);
            return ret;
        }
        p += output_len;
    } else {
        MBEDTLS_SSL_DEBUG_MSG(
            4, ("early_data not allowed, "
                "skip early_data extension in NewSessionTicket"));
    }

#endif /* MBEDTLS_SSL_EARLY_DATA */

    MBEDTLS_PUT_UINT16_BE(p - p_extensions_len - 2, p_extensions_len, 0);

    *out_len = p - buf;
    MBEDTLS_SSL_DEBUG_BUF(4, "ticket", buf, *out_len);
    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write new session ticket"));

    MBEDTLS_SSL_PRINT_EXTS(
        3, MBEDTLS_SSL_HS_NEW_SESSION_TICKET, ssl->handshake->sent_extensions);

    return 0;
}

/*
 * Handler for MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET
 */
static int ssl_tls13_write_new_session_ticket(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_PROC_CHK_NEG(ssl_tls13_write_new_session_ticket_coordinate(ssl));

    if (ret == SSL_NEW_SESSION_TICKET_WRITE) {
        unsigned char ticket_nonce[MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH];
        unsigned char *buf;
        size_t buf_len, msg_len;

        MBEDTLS_SSL_PROC_CHK(ssl_tls13_prepare_new_session_ticket(
                                 ssl, ticket_nonce, sizeof(ticket_nonce)));

        MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_start_handshake_msg(
                                 ssl, MBEDTLS_SSL_HS_NEW_SESSION_TICKET,
                                 &buf, &buf_len));

        MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_new_session_ticket_body(
                                 ssl, buf, buf + buf_len, &msg_len,
                                 ticket_nonce, sizeof(ticket_nonce)));

        MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_finish_handshake_msg(
                                 ssl, buf_len, msg_len));

        /* Limit session tickets count to one when resumption connection.
         *
         * See document of mbedtls_ssl_conf_new_session_tickets.
         */
        if (ssl->handshake->resume == 1) {
            ssl->handshake->new_session_tickets_count = 0;
        } else {
            ssl->handshake->new_session_tickets_count--;
        }

        mbedtls_ssl_handshake_set_state(
            ssl, MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET_FLUSH);
    } else {
        mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_HANDSHAKE_OVER);
    }

cleanup:

    return ret;
}
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

/*
 * TLS 1.3 State Machine -- server side
 */
int mbedtls_ssl_tls13_handshake_server_step(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (ssl->state == MBEDTLS_SSL_HANDSHAKE_OVER || ssl->handshake == NULL) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("tls13 server state: %s(%d)",
                              mbedtls_ssl_states_str((mbedtls_ssl_states) ssl->state),
                              ssl->state));

    switch (ssl->state) {
        /* start state */
        case MBEDTLS_SSL_HELLO_REQUEST:
            mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CLIENT_HELLO);
            ret = 0;
            break;

        case MBEDTLS_SSL_CLIENT_HELLO:
            ret = ssl_tls13_process_client_hello(ssl);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "ssl_tls13_process_client_hello", ret);
            }
            break;

        case MBEDTLS_SSL_HELLO_RETRY_REQUEST:
            ret = ssl_tls13_write_hello_retry_request(ssl);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "ssl_tls13_write_hello_retry_request", ret);
                return ret;
            }
            break;

        case MBEDTLS_SSL_SERVER_HELLO:
            ret = ssl_tls13_write_server_hello(ssl);
            break;

        case MBEDTLS_SSL_ENCRYPTED_EXTENSIONS:
            ret = ssl_tls13_write_encrypted_extensions(ssl);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "ssl_tls13_write_encrypted_extensions", ret);
                return ret;
            }
            break;

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
        case MBEDTLS_SSL_CERTIFICATE_REQUEST:
            ret = ssl_tls13_write_certificate_request(ssl);
            break;

        case MBEDTLS_SSL_SERVER_CERTIFICATE:
            ret = ssl_tls13_write_server_certificate(ssl);
            break;

        case MBEDTLS_SSL_CERTIFICATE_VERIFY:
            ret = ssl_tls13_write_certificate_verify(ssl);
            break;
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

            /*
             * Injection of dummy-CCS's for middlebox compatibility
             */
#if defined(MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE)
        case MBEDTLS_SSL_SERVER_CCS_AFTER_HELLO_RETRY_REQUEST:
            ret = mbedtls_ssl_tls13_write_change_cipher_spec(ssl);
            if (ret == 0) {
                mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_CLIENT_HELLO);
            }
            break;

        case MBEDTLS_SSL_SERVER_CCS_AFTER_SERVER_HELLO:
            ret = mbedtls_ssl_tls13_write_change_cipher_spec(ssl);
            if (ret != 0) {
                break;
            }
            mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_ENCRYPTED_EXTENSIONS);
            break;
#endif /* MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE */

        case MBEDTLS_SSL_SERVER_FINISHED:
            ret = ssl_tls13_write_server_finished(ssl);
            break;

#if defined(MBEDTLS_SSL_EARLY_DATA)
        case MBEDTLS_SSL_END_OF_EARLY_DATA:
            ret = ssl_tls13_process_end_of_early_data(ssl);
            break;
#endif /* MBEDTLS_SSL_EARLY_DATA */

        case MBEDTLS_SSL_CLIENT_FINISHED:
            ret = ssl_tls13_process_client_finished(ssl);
            break;

        case MBEDTLS_SSL_HANDSHAKE_WRAPUP:
            ret = ssl_tls13_handshake_wrapup(ssl);
            break;

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
        case MBEDTLS_SSL_CLIENT_CERTIFICATE:
            ret = mbedtls_ssl_tls13_process_certificate(ssl);
            if (ret == 0) {
                if (ssl->session_negotiate->peer_cert != NULL) {
                    mbedtls_ssl_handshake_set_state(
                        ssl, MBEDTLS_SSL_CLIENT_CERTIFICATE_VERIFY);
                } else {
                    MBEDTLS_SSL_DEBUG_MSG(2, ("skip parse certificate verify"));
                    mbedtls_ssl_handshake_set_state(
                        ssl, MBEDTLS_SSL_CLIENT_FINISHED);
                }
            }
            break;

        case MBEDTLS_SSL_CLIENT_CERTIFICATE_VERIFY:
            ret = mbedtls_ssl_tls13_process_certificate_verify(ssl);
            if (ret == 0) {
                mbedtls_ssl_handshake_set_state(
                    ssl, MBEDTLS_SSL_CLIENT_FINISHED);
            }
            break;
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
        case MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET:
            ret = ssl_tls13_write_new_session_ticket(ssl);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1,
                                      "ssl_tls13_write_new_session_ticket ",
                                      ret);
            }
            break;
        case MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET_FLUSH:
            /* This state is necessary to do the flush of the New Session
             * Ticket message written in MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET
             * as part of ssl_prepare_handshake_step.
             */
            ret = 0;

            if (ssl->handshake->new_session_tickets_count == 0) {
                mbedtls_ssl_handshake_set_state(ssl, MBEDTLS_SSL_HANDSHAKE_OVER);
            } else {
                mbedtls_ssl_handshake_set_state(
                    ssl, MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET);
            }
            break;

#endif /* MBEDTLS_SSL_SESSION_TICKETS */

        default:
            MBEDTLS_SSL_DEBUG_MSG(1, ("invalid state %d", ssl->state));
            return MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
    }

    return ret;
}

#endif /* MBEDTLS_SSL_SRV_C && MBEDTLS_SSL_PROTO_TLS1_3 */

/*
 *  TLS 1.3 functionality shared between client and server
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_SSL_TLS_C) && defined(MBEDTLS_SSL_PROTO_TLS1_3)

#include <string.h>

#include "mbedtls/error.h"
#include "debug_internal.h"
#include "mbedtls/oid.h"
#include "mbedtls/platform.h"
#include "mbedtls/constant_time.h"
#include "psa/crypto.h"
#include "mbedtls/psa_util.h"

#include "ssl_misc.h"
#include "ssl_tls13_invasive.h"
#include "ssl_tls13_keys.h"
#include "ssl_debug_helpers.h"

#include "psa/crypto.h"
#include "psa_util_internal.h"

/* Define a local translating function to save code size by not using too many
 * arguments in each translating place. */
static int local_err_translation(psa_status_t status)
{
    return psa_status_to_mbedtls(status, psa_to_ssl_errors,
                                 ARRAY_LENGTH(psa_to_ssl_errors),
                                 psa_generic_status_to_mbedtls);
}
#define PSA_TO_MBEDTLS_ERR(status) local_err_translation(status)

int mbedtls_ssl_tls13_crypto_init(mbedtls_ssl_context *ssl)
{
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        (void) ssl; // unused when debugging is disabled
        MBEDTLS_SSL_DEBUG_RET(1, "psa_crypto_init", status);
    }
    return PSA_TO_MBEDTLS_ERR(status);
}

const uint8_t mbedtls_ssl_tls13_hello_retry_request_magic[
    MBEDTLS_SERVER_HELLO_RANDOM_LEN] =
{ 0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11,
  0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
  0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E,
  0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C };

int mbedtls_ssl_tls13_fetch_handshake_msg(mbedtls_ssl_context *ssl,
                                          unsigned hs_type,
                                          unsigned char **buf,
                                          size_t *buf_len)
{
    int ret;

    if ((ret = mbedtls_ssl_read_record(ssl, 0)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
        goto cleanup;
    }

    if (ssl->in_msgtype != MBEDTLS_SSL_MSG_HANDSHAKE ||
        ssl->in_msg[0]  != hs_type) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Receive unexpected handshake message."));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE,
                                     MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE);
        ret = MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
        goto cleanup;
    }

    /*
     * Jump handshake header (4 bytes, see Section 4 of RFC 8446).
     *    ...
     *    HandshakeType msg_type;
     *    uint24 length;
     *    ...
     */
    *buf = ssl->in_msg   + 4;
    *buf_len = ssl->in_hslen - 4;

cleanup:

    return ret;
}

int mbedtls_ssl_tls13_is_supported_versions_ext_present_in_exts(
    mbedtls_ssl_context *ssl,
    const unsigned char *buf, const unsigned char *end,
    const unsigned char **supported_versions_data,
    const unsigned char **supported_versions_data_end)
{
    const unsigned char *p = buf;
    size_t extensions_len;
    const unsigned char *extensions_end;

    *supported_versions_data = NULL;
    *supported_versions_data_end = NULL;

    /* Case of no extension */
    if (p == end) {
        return 0;
    }

    /* ...
     * Extension extensions<x..2^16-1>;
     * ...
     * struct {
     *      ExtensionType extension_type; (2 bytes)
     *      opaque extension_data<0..2^16-1>;
     * } Extension;
     */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 2);
    extensions_len = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;

    /* Check extensions do not go beyond the buffer of data. */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, extensions_len);
    extensions_end = p + extensions_len;

    while (p < extensions_end) {
        unsigned int extension_type;
        size_t extension_data_len;

        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, extensions_end, 4);
        extension_type = MBEDTLS_GET_UINT16_BE(p, 0);
        extension_data_len = MBEDTLS_GET_UINT16_BE(p, 2);
        p += 4;
        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, extensions_end, extension_data_len);

        if (extension_type == MBEDTLS_TLS_EXT_SUPPORTED_VERSIONS) {
            *supported_versions_data = p;
            *supported_versions_data_end = p + extension_data_len;
            return 1;
        }
        p += extension_data_len;
    }

    return 0;
}

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
/*
 * STATE HANDLING: Read CertificateVerify
 */
/* Macro to express the maximum length of the verify structure.
 *
 * The structure is computed per TLS 1.3 specification as:
 *   - 64 bytes of octet 32,
 *   - 33 bytes for the context string
 *        (which is either "TLS 1.3, client CertificateVerify"
 *         or "TLS 1.3, server CertificateVerify"),
 *   - 1 byte for the octet 0x0, which serves as a separator,
 *   - 32 or 48 bytes for the Transcript-Hash(Handshake Context, Certificate)
 *     (depending on the size of the transcript_hash)
 *
 * This results in a total size of
 * - 130 bytes for a SHA256-based transcript hash, or
 *   (64 + 33 + 1 + 32 bytes)
 * - 146 bytes for a SHA384-based transcript hash.
 *   (64 + 33 + 1 + 48 bytes)
 *
 */
#define SSL_VERIFY_STRUCT_MAX_SIZE  (64 +                          \
                                     33 +                          \
                                     1 +                          \
                                     MBEDTLS_TLS1_3_MD_MAX_SIZE    \
                                     )

/*
 * The ssl_tls13_create_verify_structure() creates the verify structure.
 * As input, it requires the transcript hash.
 *
 * The caller has to ensure that the buffer has size at least
 * SSL_VERIFY_STRUCT_MAX_SIZE bytes.
 */
static void ssl_tls13_create_verify_structure(const unsigned char *transcript_hash,
                                              size_t transcript_hash_len,
                                              unsigned char *verify_buffer,
                                              size_t *verify_buffer_len,
                                              int from)
{
    size_t idx;

    /* RFC 8446, Section 4.4.3:
     *
     * The digital signature [in the CertificateVerify message] is then
     * computed over the concatenation of:
     * -  A string that consists of octet 32 (0x20) repeated 64 times
     * -  The context string
     * -  A single 0 byte which serves as the separator
     * -  The content to be signed
     */
    memset(verify_buffer, 0x20, 64);
    idx = 64;

    if (from == MBEDTLS_SSL_IS_CLIENT) {
        memcpy(verify_buffer + idx, mbedtls_ssl_tls13_labels.client_cv,
               MBEDTLS_SSL_TLS1_3_LBL_LEN(client_cv));
        idx += MBEDTLS_SSL_TLS1_3_LBL_LEN(client_cv);
    } else { /* from == MBEDTLS_SSL_IS_SERVER */
        memcpy(verify_buffer + idx, mbedtls_ssl_tls13_labels.server_cv,
               MBEDTLS_SSL_TLS1_3_LBL_LEN(server_cv));
        idx += MBEDTLS_SSL_TLS1_3_LBL_LEN(server_cv);
    }

    verify_buffer[idx++] = 0x0;

    memcpy(verify_buffer + idx, transcript_hash, transcript_hash_len);
    idx += transcript_hash_len;

    *verify_buffer_len = idx;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_certificate_verify(mbedtls_ssl_context *ssl,
                                              const unsigned char *buf,
                                              const unsigned char *end,
                                              const unsigned char *verify_buffer,
                                              size_t verify_buffer_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    const unsigned char *p = buf;
    uint16_t algorithm;
    size_t signature_len;
    mbedtls_pk_type_t sig_alg;
    mbedtls_md_type_t md_alg;
    psa_algorithm_t hash_alg = PSA_ALG_NONE;
    unsigned char verify_hash[PSA_HASH_MAX_SIZE];
    size_t verify_hash_len;

    void const *options = NULL;
#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT)
    mbedtls_pk_rsassa_pss_options rsassa_pss_options;
#endif /* MBEDTLS_X509_RSASSA_PSS_SUPPORT */

    /*
     * struct {
     *     SignatureScheme algorithm;
     *     opaque signature<0..2^16-1>;
     * } CertificateVerify;
     */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 2);
    algorithm = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;

    /* RFC 8446 section 4.4.3
     *
     * If the CertificateVerify message is sent by a server, the signature
     * algorithm MUST be one offered in the client's "signature_algorithms"
     * extension unless no valid certificate chain can be produced without
     * unsupported algorithms
     *
     * RFC 8446 section 4.4.2.2
     *
     * If the client cannot construct an acceptable chain using the provided
     * certificates and decides to abort the handshake, then it MUST abort the
     * handshake with an appropriate certificate-related alert
     * (by default, "unsupported_certificate").
     *
     * Check if algorithm is an offered signature algorithm.
     */
    if (!mbedtls_ssl_sig_alg_is_offered(ssl, algorithm)) {
        /* algorithm not in offered signature algorithms list */
        MBEDTLS_SSL_DEBUG_MSG(1, ("Received signature algorithm(%04x) is not "
                                  "offered.",
                                  (unsigned int) algorithm));
        goto error;
    }

    if (mbedtls_ssl_get_pk_type_and_md_alg_from_sig_alg(
            algorithm, &sig_alg, &md_alg) != 0) {
        goto error;
    }

    hash_alg = mbedtls_md_psa_alg_from_type(md_alg);
    if (hash_alg == 0) {
        goto error;
    }

    MBEDTLS_SSL_DEBUG_MSG(3, ("Certificate Verify: Signature algorithm ( %04x )",
                              (unsigned int) algorithm));

    /*
     * Check the certificate's key type matches the signature alg
     */
    if (!mbedtls_pk_can_do(&ssl->session_negotiate->peer_cert->pk, sig_alg)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("signature algorithm doesn't match cert key"));
        goto error;
    }

    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 2);
    signature_len = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, signature_len);

    status = psa_hash_compute(hash_alg,
                              verify_buffer,
                              verify_buffer_len,
                              verify_hash,
                              sizeof(verify_hash),
                              &verify_hash_len);
    if (status != PSA_SUCCESS) {
        MBEDTLS_SSL_DEBUG_RET(1, "hash computation PSA error", status);
        goto error;
    }

    MBEDTLS_SSL_DEBUG_BUF(3, "verify hash", verify_hash, verify_hash_len);
#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT)
    if (sig_alg == MBEDTLS_PK_RSASSA_PSS) {
        rsassa_pss_options.mgf1_hash_id = md_alg;

        rsassa_pss_options.expected_salt_len = PSA_HASH_LENGTH(hash_alg);
        options = (const void *) &rsassa_pss_options;
    }
#endif /* MBEDTLS_X509_RSASSA_PSS_SUPPORT */

    if ((ret = mbedtls_pk_verify_ext(sig_alg, options,
                                     &ssl->session_negotiate->peer_cert->pk,
                                     md_alg, verify_hash, verify_hash_len,
                                     p, signature_len)) == 0) {
        return 0;
    }
    MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_pk_verify_ext", ret);

error:
    /* RFC 8446 section 4.4.3
     *
     * If the verification fails, the receiver MUST terminate the handshake
     * with a "decrypt_error" alert.
     */
    MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECRYPT_ERROR,
                                 MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
    return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;

}
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

int mbedtls_ssl_tls13_process_certificate_verify(mbedtls_ssl_context *ssl)
{

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char verify_buffer[SSL_VERIFY_STRUCT_MAX_SIZE];
    size_t verify_buffer_len;
    unsigned char transcript[MBEDTLS_TLS1_3_MD_MAX_SIZE];
    size_t transcript_len;
    unsigned char *buf;
    size_t buf_len;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse certificate verify"));

    MBEDTLS_SSL_PROC_CHK(
        mbedtls_ssl_tls13_fetch_handshake_msg(
            ssl, MBEDTLS_SSL_HS_CERTIFICATE_VERIFY, &buf, &buf_len));

    /* Need to calculate the hash of the transcript first
     * before reading the message since otherwise it gets
     * included in the transcript
     */
    ret = mbedtls_ssl_get_handshake_transcript(
        ssl,
        (mbedtls_md_type_t) ssl->handshake->ciphersuite_info->mac,
        transcript, sizeof(transcript),
        &transcript_len);
    if (ret != 0) {
        MBEDTLS_SSL_PEND_FATAL_ALERT(
            MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR,
            MBEDTLS_ERR_SSL_INTERNAL_ERROR);
        return ret;
    }

    MBEDTLS_SSL_DEBUG_BUF(3, "handshake hash", transcript, transcript_len);

    /* Create verify structure */
    ssl_tls13_create_verify_structure(transcript,
                                      transcript_len,
                                      verify_buffer,
                                      &verify_buffer_len,
                                      (ssl->conf->endpoint == MBEDTLS_SSL_IS_CLIENT) ?
                                      MBEDTLS_SSL_IS_SERVER :
                                      MBEDTLS_SSL_IS_CLIENT);

    /* Process the message contents */
    MBEDTLS_SSL_PROC_CHK(ssl_tls13_parse_certificate_verify(
                             ssl, buf, buf + buf_len,
                             verify_buffer, verify_buffer_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                             ssl, MBEDTLS_SSL_HS_CERTIFICATE_VERIFY,
                             buf, buf_len));

cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse certificate verify"));
    MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_tls13_process_certificate_verify", ret);
    return ret;
#else
    ((void) ssl);
    MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */
}

/*
 *
 * STATE HANDLING: Incoming Certificate.
 *
 */

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
/*
 * Structure of Certificate message:
 *
 * enum {
 *     X509(0),
 *     RawPublicKey(2),
 *     (255)
 * } CertificateType;
 *
 * struct {
 *     select (certificate_type) {
 *         case RawPublicKey:
 *           * From RFC 7250 ASN.1_subjectPublicKeyInfo *
 *           opaque ASN1_subjectPublicKeyInfo<1..2^24-1>;
 *         case X509:
 *           opaque cert_data<1..2^24-1>;
 *     };
 *     Extension extensions<0..2^16-1>;
 * } CertificateEntry;
 *
 * struct {
 *     opaque certificate_request_context<0..2^8-1>;
 *     CertificateEntry certificate_list<0..2^24-1>;
 * } Certificate;
 *
 */

/* Parse certificate chain send by the server. */
MBEDTLS_CHECK_RETURN_CRITICAL
MBEDTLS_STATIC_TESTABLE
int mbedtls_ssl_tls13_parse_certificate(mbedtls_ssl_context *ssl,
                                        const unsigned char *buf,
                                        const unsigned char *end)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t certificate_request_context_len = 0;
    size_t certificate_list_len = 0;
    const unsigned char *p = buf;
    const unsigned char *certificate_list_end;
    mbedtls_ssl_handshake_params *handshake = ssl->handshake;

    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 4);
    certificate_request_context_len = p[0];
    certificate_list_len = MBEDTLS_GET_UINT24_BE(p, 1);
    p += 4;

    /* In theory, the certificate list can be up to 2^24 Bytes, but we don't
     * support anything beyond 2^16 = 64K.
     */
    if ((certificate_request_context_len != 0) ||
        (certificate_list_len >= 0x10000)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad certificate message"));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR,
                                     MBEDTLS_ERR_SSL_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    /* In case we tried to reuse a session but it failed */
    if (ssl->session_negotiate->peer_cert != NULL) {
        mbedtls_x509_crt_free(ssl->session_negotiate->peer_cert);
        mbedtls_free(ssl->session_negotiate->peer_cert);
    }

    /* This is used by ssl_tls13_validate_certificate() */
    if (certificate_list_len == 0) {
        ssl->session_negotiate->peer_cert = NULL;
        ret = 0;
        goto exit;
    }

    if ((ssl->session_negotiate->peer_cert =
             mbedtls_calloc(1, sizeof(mbedtls_x509_crt))) == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("alloc( %" MBEDTLS_PRINTF_SIZET " bytes ) failed",
                                  sizeof(mbedtls_x509_crt)));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR,
                                     MBEDTLS_ERR_SSL_ALLOC_FAILED);
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }

    mbedtls_x509_crt_init(ssl->session_negotiate->peer_cert);

    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, certificate_list_len);
    certificate_list_end = p + certificate_list_len;
    while (p < certificate_list_end) {
        size_t cert_data_len, extensions_len;
        const unsigned char *extensions_end;

        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, certificate_list_end, 3);
        cert_data_len = MBEDTLS_GET_UINT24_BE(p, 0);
        p += 3;

        /* In theory, the CRT can be up to 2^24 Bytes, but we don't support
         * anything beyond 2^16 = 64K. Otherwise as in the TLS 1.2 code,
         * check that we have a minimum of 128 bytes of data, this is not
         * clear why we need that though.
         */
        if ((cert_data_len < 128) || (cert_data_len >= 0x10000)) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad Certificate message"));
            MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR,
                                         MBEDTLS_ERR_SSL_DECODE_ERROR);
            return MBEDTLS_ERR_SSL_DECODE_ERROR;
        }

        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, certificate_list_end, cert_data_len);
        ret = mbedtls_x509_crt_parse_der(ssl->session_negotiate->peer_cert,
                                         p, cert_data_len);

        switch (ret) {
            case 0: /*ok*/
                break;
            case MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG + MBEDTLS_ERR_OID_NOT_FOUND:
                /* Ignore certificate with an unknown algorithm: maybe a
                   prior certificate was already trusted. */
                break;

            case MBEDTLS_ERR_X509_ALLOC_FAILED:
                MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR,
                                             MBEDTLS_ERR_X509_ALLOC_FAILED);
                MBEDTLS_SSL_DEBUG_RET(1, " mbedtls_x509_crt_parse_der", ret);
                return ret;

            case MBEDTLS_ERR_X509_UNKNOWN_VERSION:
                MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_CERT,
                                             MBEDTLS_ERR_X509_UNKNOWN_VERSION);
                MBEDTLS_SSL_DEBUG_RET(1, " mbedtls_x509_crt_parse_der", ret);
                return ret;

            default:
                MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_BAD_CERT,
                                             ret);
                MBEDTLS_SSL_DEBUG_RET(1, " mbedtls_x509_crt_parse_der", ret);
                return ret;
        }

        p += cert_data_len;

        /* Certificate extensions length */
        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, certificate_list_end, 2);
        extensions_len = MBEDTLS_GET_UINT16_BE(p, 0);
        p += 2;
        MBEDTLS_SSL_CHK_BUF_READ_PTR(p, certificate_list_end, extensions_len);

        extensions_end = p + extensions_len;
        handshake->received_extensions = MBEDTLS_SSL_EXT_MASK_NONE;

        while (p < extensions_end) {
            unsigned int extension_type;
            size_t extension_data_len;

            /*
             * struct {
             *     ExtensionType extension_type; (2 bytes)
             *     opaque extension_data<0..2^16-1>;
             * } Extension;
             */
            MBEDTLS_SSL_CHK_BUF_READ_PTR(p, extensions_end, 4);
            extension_type = MBEDTLS_GET_UINT16_BE(p, 0);
            extension_data_len = MBEDTLS_GET_UINT16_BE(p, 2);
            p += 4;

            MBEDTLS_SSL_CHK_BUF_READ_PTR(p, extensions_end, extension_data_len);

            ret = mbedtls_ssl_tls13_check_received_extension(
                ssl, MBEDTLS_SSL_HS_CERTIFICATE, extension_type,
                MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_CT);
            if (ret != 0) {
                return ret;
            }

            switch (extension_type) {
                default:
                    MBEDTLS_SSL_PRINT_EXT(
                        3, MBEDTLS_SSL_HS_CERTIFICATE,
                        extension_type, "( ignored )");
                    break;
            }

            p += extension_data_len;
        }

        MBEDTLS_SSL_PRINT_EXTS(3, MBEDTLS_SSL_HS_CERTIFICATE,
                               handshake->received_extensions);
    }

exit:
    /* Check that all the message is consumed. */
    if (p != end) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad Certificate message"));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR,
                                     MBEDTLS_ERR_SSL_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    MBEDTLS_SSL_DEBUG_CRT(3, "peer certificate",
                          ssl->session_negotiate->peer_cert);

    return ret;
}
#else
MBEDTLS_CHECK_RETURN_CRITICAL
MBEDTLS_STATIC_TESTABLE
int mbedtls_ssl_tls13_parse_certificate(mbedtls_ssl_context *ssl,
                                        const unsigned char *buf,
                                        const unsigned char *end)
{
    ((void) ssl);
    ((void) buf);
    ((void) end);
    return MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
}
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
/* Validate certificate chain sent by the server. */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_validate_certificate(mbedtls_ssl_context *ssl)
{
    /* Authmode: precedence order is SNI if used else configuration */
#if defined(MBEDTLS_SSL_SRV_C) && defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
    const int authmode = ssl->handshake->sni_authmode != MBEDTLS_SSL_VERIFY_UNSET
                       ? ssl->handshake->sni_authmode
                       : ssl->conf->authmode;
#else
    const int authmode = ssl->conf->authmode;
#endif

    /*
     * If the peer hasn't sent a certificate ( i.e. it sent
     * an empty certificate chain ), this is reflected in the peer CRT
     * structure being unset.
     * Check for that and handle it depending on the
     * authentication mode.
     */
    if (ssl->session_negotiate->peer_cert == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("peer has no certificate"));

#if defined(MBEDTLS_SSL_SRV_C)
        if (ssl->conf->endpoint == MBEDTLS_SSL_IS_SERVER) {
            /* The client was asked for a certificate but didn't send
             * one. The client should know what's going on, so we
             * don't send an alert.
             */
            ssl->session_negotiate->verify_result = MBEDTLS_X509_BADCERT_MISSING;
            if (authmode == MBEDTLS_SSL_VERIFY_OPTIONAL) {
                return 0;
            } else {
                MBEDTLS_SSL_PEND_FATAL_ALERT(
                    MBEDTLS_SSL_ALERT_MSG_NO_CERT,
                    MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE);
                return MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE;
            }
        }
#endif /* MBEDTLS_SSL_SRV_C */

#if defined(MBEDTLS_SSL_CLI_C)
        /* Regardless of authmode, the server is not allowed to send an empty
         * certificate chain. (Last paragraph before 4.4.2.1 in RFC 8446: "The
         * server's certificate_list MUST always be non-empty.") With authmode
         * optional/none, we continue the handshake if we can't validate the
         * server's cert, but we still break it if no certificate was sent. */
        if (ssl->conf->endpoint == MBEDTLS_SSL_IS_CLIENT) {
            MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_NO_CERT,
                                         MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE);
            return MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE;
        }
#endif /* MBEDTLS_SSL_CLI_C */
    }

    return mbedtls_ssl_verify_certificate(ssl, authmode,
                                          ssl->session_negotiate->peer_cert,
                                          NULL, NULL);
}
#else /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_validate_certificate(mbedtls_ssl_context *ssl)
{
    ((void) ssl);
    return MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
}
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

int mbedtls_ssl_tls13_process_certificate(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse certificate"));

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
    unsigned char *buf;
    size_t buf_len;

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_tls13_fetch_handshake_msg(
                             ssl, MBEDTLS_SSL_HS_CERTIFICATE,
                             &buf, &buf_len));

    /* Parse the certificate chain sent by the peer. */
    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_tls13_parse_certificate(ssl, buf,
                                                             buf + buf_len));
    /* Validate the certificate chain and set the verification results. */
    MBEDTLS_SSL_PROC_CHK(ssl_tls13_validate_certificate(ssl));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                             ssl, MBEDTLS_SSL_HS_CERTIFICATE, buf, buf_len));

cleanup:
#else /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */
    (void) ssl;
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse certificate"));
    return ret;
}
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
/*
 *  enum {
 *        X509(0),
 *        RawPublicKey(2),
 *        (255)
 *    } CertificateType;
 *
 *    struct {
 *        select (certificate_type) {
 *            case RawPublicKey:
 *              // From RFC 7250 ASN.1_subjectPublicKeyInfo
 *              opaque ASN1_subjectPublicKeyInfo<1..2^24-1>;
 *
 *            case X509:
 *              opaque cert_data<1..2^24-1>;
 *        };
 *        Extension extensions<0..2^16-1>;
 *    } CertificateEntry;
 *
 *    struct {
 *        opaque certificate_request_context<0..2^8-1>;
 *        CertificateEntry certificate_list<0..2^24-1>;
 *    } Certificate;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_certificate_body(mbedtls_ssl_context *ssl,
                                            unsigned char *buf,
                                            unsigned char *end,
                                            size_t *out_len)
{
    const mbedtls_x509_crt *crt = mbedtls_ssl_own_cert(ssl);
    unsigned char *p = buf;
    unsigned char *certificate_request_context =
        ssl->handshake->certificate_request_context;
    unsigned char certificate_request_context_len =
        ssl->handshake->certificate_request_context_len;
    unsigned char *p_certificate_list_len;


    /* ...
     * opaque certificate_request_context<0..2^8-1>;
     * ...
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, certificate_request_context_len + 1);
    *p++ = certificate_request_context_len;
    if (certificate_request_context_len > 0) {
        memcpy(p, certificate_request_context, certificate_request_context_len);
        p += certificate_request_context_len;
    }

    /* ...
     * CertificateEntry certificate_list<0..2^24-1>;
     * ...
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 3);
    p_certificate_list_len = p;
    p += 3;

    MBEDTLS_SSL_DEBUG_CRT(3, "own certificate", crt);

    while (crt != NULL) {
        size_t cert_data_len = crt->raw.len;

        MBEDTLS_SSL_CHK_BUF_PTR(p, end, cert_data_len + 3 + 2);
        MBEDTLS_PUT_UINT24_BE(cert_data_len, p, 0);
        p += 3;

        memcpy(p, crt->raw.p, cert_data_len);
        p += cert_data_len;
        crt = crt->next;

        /* Currently, we don't have any certificate extensions defined.
         * Hence, we are sending an empty extension with length zero.
         */
        MBEDTLS_PUT_UINT16_BE(0, p, 0);
        p += 2;
    }

    MBEDTLS_PUT_UINT24_BE(p - p_certificate_list_len - 3,
                          p_certificate_list_len, 0);

    *out_len = p - buf;

    MBEDTLS_SSL_PRINT_EXTS(
        3, MBEDTLS_SSL_HS_CERTIFICATE, ssl->handshake->sent_extensions);

    return 0;
}

int mbedtls_ssl_tls13_write_certificate(mbedtls_ssl_context *ssl)
{
    int ret;
    unsigned char *buf;
    size_t buf_len, msg_len;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write certificate"));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_start_handshake_msg(
                             ssl, MBEDTLS_SSL_HS_CERTIFICATE, &buf, &buf_len));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_certificate_body(ssl,
                                                          buf,
                                                          buf + buf_len,
                                                          &msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                             ssl, MBEDTLS_SSL_HS_CERTIFICATE, buf, msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_finish_handshake_msg(
                             ssl, buf_len, msg_len));
cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write certificate"));
    return ret;
}

/*
 * STATE HANDLING: Output Certificate Verify
 */
int mbedtls_ssl_tls13_check_sig_alg_cert_key_match(uint16_t sig_alg,
                                                   mbedtls_pk_context *key)
{
    mbedtls_pk_type_t pk_type = (mbedtls_pk_type_t) mbedtls_ssl_sig_from_pk(key);
    size_t key_size = mbedtls_pk_get_bitlen(key);

    switch (pk_type) {
        case MBEDTLS_SSL_SIG_ECDSA:
            switch (key_size) {
                case 256:
                    return
                        sig_alg == MBEDTLS_TLS1_3_SIG_ECDSA_SECP256R1_SHA256;

                case 384:
                    return
                        sig_alg == MBEDTLS_TLS1_3_SIG_ECDSA_SECP384R1_SHA384;

                case 521:
                    return
                        sig_alg == MBEDTLS_TLS1_3_SIG_ECDSA_SECP521R1_SHA512;
                default:
                    break;
            }
            break;

        case MBEDTLS_SSL_SIG_RSA:
            switch (sig_alg) {
                case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA256: /* Intentional fallthrough */
                case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA384: /* Intentional fallthrough */
                case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA512:
                    return 1;

                default:
                    break;
            }
            break;

        default:
            break;
    }

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_certificate_verify_body(mbedtls_ssl_context *ssl,
                                                   unsigned char *buf,
                                                   unsigned char *end,
                                                   size_t *out_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    mbedtls_pk_context *own_key;

    unsigned char handshake_hash[MBEDTLS_TLS1_3_MD_MAX_SIZE];
    size_t handshake_hash_len;
    unsigned char verify_buffer[SSL_VERIFY_STRUCT_MAX_SIZE];
    size_t verify_buffer_len;

    uint16_t *sig_alg = ssl->handshake->received_sig_algs;
    size_t signature_len = 0;

    *out_len = 0;

    own_key = mbedtls_ssl_own_key(ssl);
    if (own_key == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    ret = mbedtls_ssl_get_handshake_transcript(
        ssl, (mbedtls_md_type_t) ssl->handshake->ciphersuite_info->mac,
        handshake_hash, sizeof(handshake_hash), &handshake_hash_len);
    if (ret != 0) {
        return ret;
    }

    MBEDTLS_SSL_DEBUG_BUF(3, "handshake hash",
                          handshake_hash,
                          handshake_hash_len);

    ssl_tls13_create_verify_structure(handshake_hash, handshake_hash_len,
                                      verify_buffer, &verify_buffer_len,
                                      ssl->conf->endpoint);

    /*
     *  struct {
     *    SignatureScheme algorithm;
     *    opaque signature<0..2^16-1>;
     *  } CertificateVerify;
     */
    /* Check there is space for the algorithm identifier (2 bytes) and the
     * signature length (2 bytes).
     */
    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 4);

    for (; *sig_alg != MBEDTLS_TLS1_3_SIG_NONE; sig_alg++) {
        psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
        mbedtls_pk_type_t pk_type = MBEDTLS_PK_NONE;
        mbedtls_md_type_t md_alg = MBEDTLS_MD_NONE;
        psa_algorithm_t psa_algorithm = PSA_ALG_NONE;
        unsigned char verify_hash[PSA_HASH_MAX_SIZE];
        size_t verify_hash_len;

        if (!mbedtls_ssl_sig_alg_is_offered(ssl, *sig_alg)) {
            continue;
        }

        if (!mbedtls_ssl_tls13_sig_alg_for_cert_verify_is_supported(*sig_alg)) {
            continue;
        }

        if (!mbedtls_ssl_tls13_check_sig_alg_cert_key_match(*sig_alg, own_key)) {
            continue;
        }

        if (mbedtls_ssl_get_pk_type_and_md_alg_from_sig_alg(
                *sig_alg, &pk_type, &md_alg) != 0) {
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        /* Hash verify buffer with indicated hash function */
        psa_algorithm = mbedtls_md_psa_alg_from_type(md_alg);
        status = psa_hash_compute(psa_algorithm,
                                  verify_buffer,
                                  verify_buffer_len,
                                  verify_hash, sizeof(verify_hash),
                                  &verify_hash_len);
        if (status != PSA_SUCCESS) {
            return PSA_TO_MBEDTLS_ERR(status);
        }

        MBEDTLS_SSL_DEBUG_BUF(3, "verify hash", verify_hash, verify_hash_len);

        if ((ret = mbedtls_pk_sign_ext(pk_type, own_key,
                                       md_alg, verify_hash, verify_hash_len,
                                       p + 4, (size_t) (end - (p + 4)), &signature_len,
                                       ssl->conf->f_rng, ssl->conf->p_rng)) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("CertificateVerify signature failed with %s",
                                      mbedtls_ssl_sig_alg_to_str(*sig_alg)));
            MBEDTLS_SSL_DEBUG_RET(2, "mbedtls_pk_sign_ext", ret);

            /* The signature failed. This is possible if the private key
             * was not suitable for the signature operation as purposely we
             * did not check its suitability completely. Let's try with
             * another signature algorithm.
             */
            continue;
        }

        MBEDTLS_SSL_DEBUG_MSG(2, ("CertificateVerify signature with %s",
                                  mbedtls_ssl_sig_alg_to_str(*sig_alg)));

        break;
    }

    if (*sig_alg == MBEDTLS_TLS1_3_SIG_NONE) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("no suitable signature algorithm"));
        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE,
                                     MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    MBEDTLS_PUT_UINT16_BE(*sig_alg, p, 0);
    MBEDTLS_PUT_UINT16_BE(signature_len, p, 2);

    *out_len = 4 + signature_len;

    return 0;
}

int mbedtls_ssl_tls13_write_certificate_verify(mbedtls_ssl_context *ssl)
{
    int ret = 0;
    unsigned char *buf;
    size_t buf_len, msg_len;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write certificate verify"));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_start_handshake_msg(
                             ssl, MBEDTLS_SSL_HS_CERTIFICATE_VERIFY,
                             &buf, &buf_len));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_certificate_verify_body(
                             ssl, buf, buf + buf_len, &msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                             ssl, MBEDTLS_SSL_HS_CERTIFICATE_VERIFY,
                             buf, msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_finish_handshake_msg(
                             ssl, buf_len, msg_len));

cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write certificate verify"));
    return ret;
}

#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

/*
 *
 * STATE HANDLING: Incoming Finished message.
 */
/*
 * Implementation
 */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_preprocess_finished_message(mbedtls_ssl_context *ssl)
{
    int ret;

    ret = mbedtls_ssl_tls13_calculate_verify_data(
        ssl,
        ssl->handshake->state_local.finished_in.digest,
        sizeof(ssl->handshake->state_local.finished_in.digest),
        &ssl->handshake->state_local.finished_in.digest_len,
        ssl->conf->endpoint == MBEDTLS_SSL_IS_CLIENT ?
        MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_tls13_calculate_verify_data", ret);
        return ret;
    }

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_parse_finished_message(mbedtls_ssl_context *ssl,
                                            const unsigned char *buf,
                                            const unsigned char *end)
{
    /*
     * struct {
     *     opaque verify_data[Hash.length];
     * } Finished;
     */
    const unsigned char *expected_verify_data =
        ssl->handshake->state_local.finished_in.digest;
    size_t expected_verify_data_len =
        ssl->handshake->state_local.finished_in.digest_len;
    /* Structural validation */
    if ((size_t) (end - buf) != expected_verify_data_len) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad finished message"));

        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR,
                                     MBEDTLS_ERR_SSL_DECODE_ERROR);
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    MBEDTLS_SSL_DEBUG_BUF(4, "verify_data (self-computed):",
                          expected_verify_data,
                          expected_verify_data_len);
    MBEDTLS_SSL_DEBUG_BUF(4, "verify_data (received message):", buf,
                          expected_verify_data_len);

    /* Semantic validation */
    if (mbedtls_ct_memcmp(buf,
                          expected_verify_data,
                          expected_verify_data_len) != 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad finished message"));

        MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECRYPT_ERROR,
                                     MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE);
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }
    return 0;
}

int mbedtls_ssl_tls13_process_finished_message(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *buf;
    size_t buf_len;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse finished message"));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_tls13_fetch_handshake_msg(
                             ssl, MBEDTLS_SSL_HS_FINISHED, &buf, &buf_len));

    /* Preprocessing step: Compute handshake digest */
    MBEDTLS_SSL_PROC_CHK(ssl_tls13_preprocess_finished_message(ssl));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_parse_finished_message(
                             ssl, buf, buf + buf_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(
                             ssl, MBEDTLS_SSL_HS_FINISHED, buf, buf_len));

cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse finished message"));
    return ret;
}

/*
 *
 * STATE HANDLING: Write and send Finished message.
 *
 */
/*
 * Implement
 */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_prepare_finished_message(mbedtls_ssl_context *ssl)
{
    int ret;

    /* Compute transcript of handshake up to now. */
    ret = mbedtls_ssl_tls13_calculate_verify_data(ssl,
                                                  ssl->handshake->state_local.finished_out.digest,
                                                  sizeof(ssl->handshake->state_local.finished_out.
                                                         digest),
                                                  &ssl->handshake->state_local.finished_out.digest_len,
                                                  ssl->conf->endpoint);

    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "calculate_verify_data failed", ret);
        return ret;
    }

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_finished_message_body(mbedtls_ssl_context *ssl,
                                                 unsigned char *buf,
                                                 unsigned char *end,
                                                 size_t *out_len)
{
    size_t verify_data_len = ssl->handshake->state_local.finished_out.digest_len;
    /*
     * struct {
     *     opaque verify_data[Hash.length];
     * } Finished;
     */
    MBEDTLS_SSL_CHK_BUF_PTR(buf, end, verify_data_len);

    memcpy(buf, ssl->handshake->state_local.finished_out.digest,
           verify_data_len);

    *out_len = verify_data_len;
    return 0;
}

/* Main entry point: orchestrates the other functions */
int mbedtls_ssl_tls13_write_finished_message(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *buf;
    size_t buf_len, msg_len;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write finished message"));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_prepare_finished_message(ssl));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_start_handshake_msg(ssl,
                                                         MBEDTLS_SSL_HS_FINISHED, &buf, &buf_len));

    MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_finished_message_body(
                             ssl, buf, buf + buf_len, &msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_add_hs_msg_to_checksum(ssl,
                                                            MBEDTLS_SSL_HS_FINISHED, buf, msg_len));

    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_finish_handshake_msg(
                             ssl, buf_len, msg_len));
cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write finished message"));
    return ret;
}

void mbedtls_ssl_tls13_handshake_wrapup(mbedtls_ssl_context *ssl)
{

    MBEDTLS_SSL_DEBUG_MSG(3, ("=> handshake wrapup"));

    MBEDTLS_SSL_DEBUG_MSG(1, ("Switch to application keys for inbound traffic"));
    mbedtls_ssl_set_inbound_transform(ssl, ssl->transform_application);

    MBEDTLS_SSL_DEBUG_MSG(1, ("Switch to application keys for outbound traffic"));
    mbedtls_ssl_set_outbound_transform(ssl, ssl->transform_application);

    /*
     * Free the previous session and switch to the current one.
     */
    if (ssl->session) {
        mbedtls_ssl_session_free(ssl->session);
        mbedtls_free(ssl->session);
    }
    ssl->session = ssl->session_negotiate;
    ssl->session_negotiate = NULL;

    MBEDTLS_SSL_DEBUG_MSG(3, ("<= handshake wrapup"));
}

/*
 *
 * STATE HANDLING: Write ChangeCipherSpec
 *
 */
#if defined(MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_write_change_cipher_spec_body(mbedtls_ssl_context *ssl,
                                                   unsigned char *buf,
                                                   unsigned char *end,
                                                   size_t *olen)
{
    ((void) ssl);

    MBEDTLS_SSL_CHK_BUF_PTR(buf, end, 1);
    buf[0] = 1;
    *olen = 1;

    return 0;
}

int mbedtls_ssl_tls13_write_change_cipher_spec(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write change cipher spec"));

    /* Only one CCS to send. */
    if (ssl->handshake->ccs_sent) {
        ret = 0;
        goto cleanup;
    }

    /* Write CCS message */
    MBEDTLS_SSL_PROC_CHK(ssl_tls13_write_change_cipher_spec_body(
                             ssl, ssl->out_msg,
                             ssl->out_msg + MBEDTLS_SSL_OUT_CONTENT_LEN,
                             &ssl->out_msglen));

    ssl->out_msgtype = MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC;

    /* Dispatch message */
    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_write_record(ssl, 0));

    ssl->handshake->ccs_sent = 1;

cleanup:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write change cipher spec"));
    return ret;
}

#endif /* MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE */

/* Early Data Indication Extension
 *
 * struct {
 *   select ( Handshake.msg_type ) {
 *     case new_session_ticket:   uint32 max_early_data_size;
 *     case client_hello:         Empty;
 *     case encrypted_extensions: Empty;
 *   };
 * } EarlyDataIndication;
 */
#if defined(MBEDTLS_SSL_EARLY_DATA)
int mbedtls_ssl_tls13_write_early_data_ext(mbedtls_ssl_context *ssl,
                                           int in_new_session_ticket,
                                           unsigned char *buf,
                                           const unsigned char *end,
                                           size_t *out_len)
{
    unsigned char *p = buf;

#if defined(MBEDTLS_SSL_SRV_C)
    const size_t needed = in_new_session_ticket ? 8 : 4;
#else
    const size_t needed = 4;
    ((void) in_new_session_ticket);
#endif

    *out_len = 0;

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, needed);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_EARLY_DATA, p, 0);
    MBEDTLS_PUT_UINT16_BE(needed - 4, p, 2);

#if defined(MBEDTLS_SSL_SRV_C)
    if (in_new_session_ticket) {
        MBEDTLS_PUT_UINT32_BE(ssl->conf->max_early_data_size, p, 4);
        MBEDTLS_SSL_DEBUG_MSG(
            4, ("Sent max_early_data_size=%u",
                (unsigned int) ssl->conf->max_early_data_size));
    }
#endif

    *out_len = needed;

    mbedtls_ssl_tls13_set_hs_sent_ext_mask(ssl, MBEDTLS_TLS_EXT_EARLY_DATA);

    return 0;
}

#if defined(MBEDTLS_SSL_SRV_C)
int mbedtls_ssl_tls13_check_early_data_len(mbedtls_ssl_context *ssl,
                                           size_t early_data_len)
{
    /*
     * This function should be called only while an handshake is in progress
     * and thus a session under negotiation. Add a sanity check to detect a
     * misuse.
     */
    if (ssl->session_negotiate == NULL) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    /* RFC 8446 section 4.6.1
     *
     * A server receiving more than max_early_data_size bytes of 0-RTT data
     * SHOULD terminate the connection with an "unexpected_message" alert.
     * Note that if it is still possible to send early_data_len bytes of early
     * data, it means that early_data_len is smaller than max_early_data_size
     * (type uint32_t) and can fit in an uint32_t. We use this further
     * down.
     */
    if (early_data_len >
        (ssl->session_negotiate->max_early_data_size -
         ssl->total_early_data_size)) {

        MBEDTLS_SSL_DEBUG_MSG(
            2, ("EarlyData: Too much early data received, "
                "%lu + %" MBEDTLS_PRINTF_SIZET " > %lu",
                (unsigned long) ssl->total_early_data_size,
                early_data_len,
                (unsigned long) ssl->session_negotiate->max_early_data_size));

        MBEDTLS_SSL_PEND_FATAL_ALERT(
            MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE,
            MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE);
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    /*
     * early_data_len has been checked to be less than max_early_data_size
     * that is uint32_t. Its cast to an uint32_t below is thus safe. We need
     * the cast to appease some compilers.
     */
    ssl->total_early_data_size += (uint32_t) early_data_len;

    return 0;
}
#endif /* MBEDTLS_SSL_SRV_C */
#endif /* MBEDTLS_SSL_EARLY_DATA */

/* Reset SSL context and update hash for handling HRR.
 *
 * Replace Transcript-Hash(X) by
 * Transcript-Hash( message_hash     ||
 *                 00 00 Hash.length ||
 *                 X )
 * A few states of the handshake are preserved, including:
 *   - session ID
 *   - session ticket
 *   - negotiated ciphersuite
 */
int mbedtls_ssl_reset_transcript_for_hrr(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char hash_transcript[PSA_HASH_MAX_SIZE + 4];
    size_t hash_len;
    const mbedtls_ssl_ciphersuite_t *ciphersuite_info =
        ssl->handshake->ciphersuite_info;

    MBEDTLS_SSL_DEBUG_MSG(3, ("Reset SSL session for HRR"));

    ret = mbedtls_ssl_get_handshake_transcript(ssl, (mbedtls_md_type_t) ciphersuite_info->mac,
                                               hash_transcript + 4,
                                               PSA_HASH_MAX_SIZE,
                                               &hash_len);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_get_handshake_transcript", ret);
        return ret;
    }

    hash_transcript[0] = MBEDTLS_SSL_HS_MESSAGE_HASH;
    hash_transcript[1] = 0;
    hash_transcript[2] = 0;
    hash_transcript[3] = (unsigned char) hash_len;

    hash_len += 4;

    MBEDTLS_SSL_DEBUG_BUF(4, "Truncated handshake transcript",
                          hash_transcript, hash_len);

    /* Reset running hash and replace it with a hash of the transcript */
    ret = mbedtls_ssl_reset_checksum(ssl);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_reset_checksum", ret);
        return ret;
    }
    ret = ssl->handshake->update_checksum(ssl, hash_transcript, hash_len);
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "update_checksum", ret);
        return ret;
    }

    return ret;
}

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED)

int mbedtls_ssl_tls13_read_public_xxdhe_share(mbedtls_ssl_context *ssl,
                                              const unsigned char *buf,
                                              size_t buf_len)
{
    uint8_t *p = (uint8_t *) buf;
    const uint8_t *end = buf + buf_len;
    mbedtls_ssl_handshake_params *handshake = ssl->handshake;

    /* Get size of the TLS opaque key_exchange field of the KeyShareEntry struct. */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 2);
    uint16_t peerkey_len = MBEDTLS_GET_UINT16_BE(p, 0);
    p += 2;

    /* Check if key size is consistent with given buffer length. */
    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, peerkey_len);

    /* Store peer's ECDH/FFDH public key. */
    if (peerkey_len > sizeof(handshake->xxdh_psa_peerkey)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Invalid public key length: %u > %" MBEDTLS_PRINTF_SIZET,
                                  (unsigned) peerkey_len,
                                  sizeof(handshake->xxdh_psa_peerkey)));
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }
    memcpy(handshake->xxdh_psa_peerkey, p, peerkey_len);
    handshake->xxdh_psa_peerkey_len = peerkey_len;

    return 0;
}

#if defined(PSA_WANT_ALG_FFDH)
static psa_status_t  mbedtls_ssl_get_psa_ffdh_info_from_tls_id(
    uint16_t tls_id, size_t *bits, psa_key_type_t *key_type)
{
    switch (tls_id) {
#if defined(PSA_WANT_DH_RFC7919_2048)
        case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE2048:
            *bits = 2048;
            *key_type = PSA_KEY_TYPE_DH_KEY_PAIR(PSA_DH_FAMILY_RFC7919);
            return PSA_SUCCESS;
#endif /* PSA_WANT_DH_RFC7919_2048 */
#if defined(PSA_WANT_DH_RFC7919_3072)
        case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE3072:
            *bits = 3072;
            *key_type =  PSA_KEY_TYPE_DH_KEY_PAIR(PSA_DH_FAMILY_RFC7919);
            return PSA_SUCCESS;
#endif /* PSA_WANT_DH_RFC7919_3072 */
#if defined(PSA_WANT_DH_RFC7919_4096)
        case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE4096:
            *bits = 4096;
            *key_type =  PSA_KEY_TYPE_DH_KEY_PAIR(PSA_DH_FAMILY_RFC7919);
            return PSA_SUCCESS;
#endif /* PSA_WANT_DH_RFC7919_4096 */
#if defined(PSA_WANT_DH_RFC7919_6144)
        case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE6144:
            *bits = 6144;
            *key_type =  PSA_KEY_TYPE_DH_KEY_PAIR(PSA_DH_FAMILY_RFC7919);
            return PSA_SUCCESS;
#endif /* PSA_WANT_DH_RFC7919_6144 */
#if defined(PSA_WANT_DH_RFC7919_8192)
        case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE8192:
            *bits = 8192;
            *key_type =  PSA_KEY_TYPE_DH_KEY_PAIR(PSA_DH_FAMILY_RFC7919);
            return PSA_SUCCESS;
#endif /* PSA_WANT_DH_RFC7919_8192 */
        default:
            return PSA_ERROR_NOT_SUPPORTED;
    }
}
#endif /* PSA_WANT_ALG_FFDH */

int mbedtls_ssl_tls13_generate_and_write_xxdh_key_exchange(
    mbedtls_ssl_context *ssl,
    uint16_t named_group,
    unsigned char *buf,
    unsigned char *end,
    size_t *out_len)
{
    psa_status_t status = PSA_ERROR_GENERIC_ERROR;
    int ret = MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
    psa_key_attributes_t key_attributes;
    size_t own_pubkey_len;
    mbedtls_ssl_handshake_params *handshake = ssl->handshake;
    size_t bits = 0;
    psa_key_type_t key_type = PSA_KEY_TYPE_NONE;
    psa_algorithm_t alg = PSA_ALG_NONE;
    size_t buf_size = (size_t) (end - buf);

    MBEDTLS_SSL_DEBUG_MSG(1, ("Perform PSA-based ECDH/FFDH computation."));

    /* Convert EC's TLS ID to PSA key type. */
#if defined(PSA_WANT_ALG_ECDH)
    if (mbedtls_ssl_get_psa_curve_info_from_tls_id(
            named_group, &key_type, &bits) == PSA_SUCCESS) {
        alg = PSA_ALG_ECDH;
    }
#endif
#if defined(PSA_WANT_ALG_FFDH)
    if (mbedtls_ssl_get_psa_ffdh_info_from_tls_id(named_group, &bits,
                                                  &key_type) == PSA_SUCCESS) {
        alg = PSA_ALG_FFDH;
    }
#endif

    if (key_type == PSA_KEY_TYPE_NONE) {
        return MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE;
    }

    if (buf_size < PSA_BITS_TO_BYTES(bits)) {
        return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
    }

    handshake->xxdh_psa_type = key_type;
    ssl->handshake->xxdh_psa_bits = bits;

    key_attributes = psa_key_attributes_init();
    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&key_attributes, alg);
    psa_set_key_type(&key_attributes, handshake->xxdh_psa_type);
    psa_set_key_bits(&key_attributes, handshake->xxdh_psa_bits);

    /* Generate ECDH/FFDH private key. */
    status = psa_generate_key(&key_attributes,
                              &handshake->xxdh_psa_privkey);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        MBEDTLS_SSL_DEBUG_RET(1, "psa_generate_key", ret);
        return ret;

    }

    /* Export the public part of the ECDH/FFDH private key from PSA. */
    status = psa_export_public_key(handshake->xxdh_psa_privkey,
                                   buf, buf_size,
                                   &own_pubkey_len);

    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        MBEDTLS_SSL_DEBUG_RET(1, "psa_export_public_key", ret);
        return ret;
    }

    *out_len = own_pubkey_len;

    return 0;
}
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED */

/* RFC 8446 section 4.2
 *
 * If an implementation receives an extension which it recognizes and which is
 * not specified for the message in which it appears, it MUST abort the handshake
 * with an "illegal_parameter" alert.
 *
 */
int mbedtls_ssl_tls13_check_received_extension(
    mbedtls_ssl_context *ssl,
    int hs_msg_type,
    unsigned int received_extension_type,
    uint32_t hs_msg_allowed_extensions_mask)
{
    uint32_t extension_mask = mbedtls_ssl_get_extension_mask(
        received_extension_type);

    MBEDTLS_SSL_PRINT_EXT(
        3, hs_msg_type, received_extension_type, "received");

    if ((extension_mask & hs_msg_allowed_extensions_mask) == 0) {
        MBEDTLS_SSL_PRINT_EXT(
            3, hs_msg_type, received_extension_type, "is illegal");
        MBEDTLS_SSL_PEND_FATAL_ALERT(
            MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER,
            MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }

    ssl->handshake->received_extensions |= extension_mask;
    /*
     * If it is a message containing extension responses, check that we
     * previously sent the extension.
     */
    switch (hs_msg_type) {
        case MBEDTLS_SSL_HS_SERVER_HELLO:
        case MBEDTLS_SSL_TLS1_3_HS_HELLO_RETRY_REQUEST:
        case MBEDTLS_SSL_HS_ENCRYPTED_EXTENSIONS:
        case MBEDTLS_SSL_HS_CERTIFICATE:
            /* Check if the received extension is sent by peer message.*/
            if ((ssl->handshake->sent_extensions & extension_mask) != 0) {
                return 0;
            }
            break;
        default:
            return 0;
    }

    MBEDTLS_SSL_PRINT_EXT(
        3, hs_msg_type, received_extension_type, "is unsupported");
    MBEDTLS_SSL_PEND_FATAL_ALERT(
        MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_EXT,
        MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION);
    return MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION;
}

#if defined(MBEDTLS_SSL_RECORD_SIZE_LIMIT)

/* RFC 8449, section 4:
 *
 * The ExtensionData of the "record_size_limit" extension is
 * RecordSizeLimit:
 *     uint16 RecordSizeLimit;
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_parse_record_size_limit_ext(mbedtls_ssl_context *ssl,
                                                  const unsigned char *buf,
                                                  const unsigned char *end)
{
    const unsigned char *p = buf;
    uint16_t record_size_limit;
    const size_t extension_data_len = end - buf;

    if (extension_data_len !=
        MBEDTLS_SSL_RECORD_SIZE_LIMIT_EXTENSION_DATA_LENGTH) {
        MBEDTLS_SSL_DEBUG_MSG(2,
                              ("record_size_limit extension has invalid length: %"
                               MBEDTLS_PRINTF_SIZET " Bytes",
                               extension_data_len));

        MBEDTLS_SSL_PEND_FATAL_ALERT(
            MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER,
            MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }

    MBEDTLS_SSL_CHK_BUF_READ_PTR(p, end, 2);
    record_size_limit = MBEDTLS_GET_UINT16_BE(p, 0);

    MBEDTLS_SSL_DEBUG_MSG(2, ("RecordSizeLimit: %u Bytes", record_size_limit));

    /* RFC 8449, section 4:
     *
     * Endpoints MUST NOT send a "record_size_limit" extension with a value
     * smaller than 64.  An endpoint MUST treat receipt of a smaller value
     * as a fatal error and generate an "illegal_parameter" alert.
     */
    if (record_size_limit < MBEDTLS_SSL_RECORD_SIZE_LIMIT_MIN) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Invalid record size limit : %u Bytes",
                                  record_size_limit));
        MBEDTLS_SSL_PEND_FATAL_ALERT(
            MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER,
            MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER);
        return MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER;
    }

    ssl->session_negotiate->record_size_limit = record_size_limit;

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_record_size_limit_ext(mbedtls_ssl_context *ssl,
                                                  unsigned char *buf,
                                                  const unsigned char *end,
                                                  size_t *out_len)
{
    unsigned char *p = buf;
    *out_len = 0;

    MBEDTLS_STATIC_ASSERT(MBEDTLS_SSL_IN_CONTENT_LEN >= MBEDTLS_SSL_RECORD_SIZE_LIMIT_MIN,
                          "MBEDTLS_SSL_IN_CONTENT_LEN is less than the "
                          "minimum record size limit");

    MBEDTLS_SSL_CHK_BUF_PTR(p, end, 6);

    MBEDTLS_PUT_UINT16_BE(MBEDTLS_TLS_EXT_RECORD_SIZE_LIMIT, p, 0);
    MBEDTLS_PUT_UINT16_BE(MBEDTLS_SSL_RECORD_SIZE_LIMIT_EXTENSION_DATA_LENGTH,
                          p, 2);
    MBEDTLS_PUT_UINT16_BE(MBEDTLS_SSL_IN_CONTENT_LEN, p, 4);

    *out_len = 6;

    MBEDTLS_SSL_DEBUG_MSG(2, ("Sent RecordSizeLimit: %d Bytes",
                              MBEDTLS_SSL_IN_CONTENT_LEN));

    mbedtls_ssl_tls13_set_hs_sent_ext_mask(ssl, MBEDTLS_TLS_EXT_RECORD_SIZE_LIMIT);

    return 0;
}

#endif /* MBEDTLS_SSL_RECORD_SIZE_LIMIT */

#endif /* MBEDTLS_SSL_TLS_C && MBEDTLS_SSL_PROTO_TLS1_3 */

/**
 * \file ssl_misc.h
 *
 * \brief Internal functions shared by the SSL modules
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_SSL_MISC_H
#define MBEDTLS_SSL_MISC_H

#include "mbedtls/build_info.h"
#include "common.h"

#include "mbedtls/error.h"

#include "mbedtls/ssl.h"
#include "mbedtls/debug.h"
#include "debug_internal.h"

#include "mbedtls/cipher.h"

#if defined(MBEDTLS_USE_PSA_CRYPTO) || defined(MBEDTLS_SSL_PROTO_TLS1_3)
#include "psa/crypto.h"
#include "psa_util_internal.h"
#endif

#if defined(MBEDTLS_MD_CAN_MD5)
#include "mbedtls/md5.h"
#endif

#if defined(MBEDTLS_MD_CAN_SHA1)
#include "mbedtls/sha1.h"
#endif

#if defined(MBEDTLS_MD_CAN_SHA256)
#include "mbedtls/sha256.h"
#endif

#if defined(MBEDTLS_MD_CAN_SHA512)
#include "mbedtls/sha512.h"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) && \
    !defined(MBEDTLS_USE_PSA_CRYPTO)
#include "mbedtls/ecjpake.h"
#endif

#include "mbedtls/pk.h"
#include "ssl_ciphersuites_internal.h"
#include "x509_internal.h"
#include "pk_internal.h"


/* Shorthand for restartable ECC */
#if defined(MBEDTLS_ECP_RESTARTABLE) && \
    defined(MBEDTLS_SSL_CLI_C) && \
    defined(MBEDTLS_SSL_PROTO_TLS1_2) && \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
#define MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED
#endif

#define MBEDTLS_SSL_INITIAL_HANDSHAKE           0
#define MBEDTLS_SSL_RENEGOTIATION_IN_PROGRESS   1   /* In progress */
#define MBEDTLS_SSL_RENEGOTIATION_DONE          2   /* Done or aborted */
#define MBEDTLS_SSL_RENEGOTIATION_PENDING       3   /* Requested (server only) */

/* Faked handshake message identity for HelloRetryRequest. */
#define MBEDTLS_SSL_TLS1_3_HS_HELLO_RETRY_REQUEST (-MBEDTLS_SSL_HS_SERVER_HELLO)

/*
 * Internal identity of handshake extensions
 */
#define MBEDTLS_SSL_EXT_ID_UNRECOGNIZED                0
#define MBEDTLS_SSL_EXT_ID_SERVERNAME                  1
#define MBEDTLS_SSL_EXT_ID_SERVERNAME_HOSTNAME         1
#define MBEDTLS_SSL_EXT_ID_MAX_FRAGMENT_LENGTH         2
#define MBEDTLS_SSL_EXT_ID_STATUS_REQUEST              3
#define MBEDTLS_SSL_EXT_ID_SUPPORTED_GROUPS            4
#define MBEDTLS_SSL_EXT_ID_SUPPORTED_ELLIPTIC_CURVES   4
#define MBEDTLS_SSL_EXT_ID_SIG_ALG                     5
#define MBEDTLS_SSL_EXT_ID_USE_SRTP                    6
#define MBEDTLS_SSL_EXT_ID_HEARTBEAT                   7
#define MBEDTLS_SSL_EXT_ID_ALPN                        8
#define MBEDTLS_SSL_EXT_ID_SCT                         9
#define MBEDTLS_SSL_EXT_ID_CLI_CERT_TYPE              10
#define MBEDTLS_SSL_EXT_ID_SERV_CERT_TYPE             11
#define MBEDTLS_SSL_EXT_ID_PADDING                    12
#define MBEDTLS_SSL_EXT_ID_PRE_SHARED_KEY             13
#define MBEDTLS_SSL_EXT_ID_EARLY_DATA                 14
#define MBEDTLS_SSL_EXT_ID_SUPPORTED_VERSIONS         15
#define MBEDTLS_SSL_EXT_ID_COOKIE                     16
#define MBEDTLS_SSL_EXT_ID_PSK_KEY_EXCHANGE_MODES     17
#define MBEDTLS_SSL_EXT_ID_CERT_AUTH                  18
#define MBEDTLS_SSL_EXT_ID_OID_FILTERS                19
#define MBEDTLS_SSL_EXT_ID_POST_HANDSHAKE_AUTH        20
#define MBEDTLS_SSL_EXT_ID_SIG_ALG_CERT               21
#define MBEDTLS_SSL_EXT_ID_KEY_SHARE                  22
#define MBEDTLS_SSL_EXT_ID_TRUNCATED_HMAC             23
#define MBEDTLS_SSL_EXT_ID_SUPPORTED_POINT_FORMATS    24
#define MBEDTLS_SSL_EXT_ID_ENCRYPT_THEN_MAC           25
#define MBEDTLS_SSL_EXT_ID_EXTENDED_MASTER_SECRET     26
#define MBEDTLS_SSL_EXT_ID_SESSION_TICKET             27
#define MBEDTLS_SSL_EXT_ID_RECORD_SIZE_LIMIT          28

/* Utility for translating IANA extension type. */
uint32_t mbedtls_ssl_get_extension_id(unsigned int extension_type);
uint32_t mbedtls_ssl_get_extension_mask(unsigned int extension_type);
/* Macros used to define mask constants */
#define MBEDTLS_SSL_EXT_MASK(id)       (1ULL << (MBEDTLS_SSL_EXT_ID_##id))
/* Reset value of extension mask */
#define MBEDTLS_SSL_EXT_MASK_NONE                                              0

/* In messages containing extension requests, we should ignore unrecognized
 * extensions. In messages containing extension responses, unrecognized
 * extensions should result in handshake abortion. Messages containing
 * extension requests include ClientHello, CertificateRequest and
 * NewSessionTicket. Messages containing extension responses include
 * ServerHello, HelloRetryRequest, EncryptedExtensions and Certificate.
 *
 * RFC 8446 section 4.1.3
 *
 * The ServerHello MUST only include extensions which are required to establish
 * the cryptographic context and negotiate the protocol version.
 *
 * RFC 8446 section 4.2
 *
 * If an implementation receives an extension which it recognizes and which is
 * not specified for the message in which it appears, it MUST abort the handshake
 * with an "illegal_parameter" alert.
 */

/* Extensions that are not recognized by TLS 1.3 */
#define MBEDTLS_SSL_TLS1_3_EXT_MASK_UNRECOGNIZED                               \
    (MBEDTLS_SSL_EXT_MASK(SUPPORTED_POINT_FORMATS)                | \
     MBEDTLS_SSL_EXT_MASK(ENCRYPT_THEN_MAC)                       | \
     MBEDTLS_SSL_EXT_MASK(EXTENDED_MASTER_SECRET)                 | \
     MBEDTLS_SSL_EXT_MASK(SESSION_TICKET)                         | \
     MBEDTLS_SSL_EXT_MASK(TRUNCATED_HMAC)                         | \
     MBEDTLS_SSL_EXT_MASK(UNRECOGNIZED))

/* RFC 8446 section 4.2. Allowed extensions for ClientHello */
#define MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_CH                                  \
    (MBEDTLS_SSL_EXT_MASK(SERVERNAME)                             | \
     MBEDTLS_SSL_EXT_MASK(MAX_FRAGMENT_LENGTH)                    | \
     MBEDTLS_SSL_EXT_MASK(STATUS_REQUEST)                         | \
     MBEDTLS_SSL_EXT_MASK(SUPPORTED_GROUPS)                       | \
     MBEDTLS_SSL_EXT_MASK(SIG_ALG)                                | \
     MBEDTLS_SSL_EXT_MASK(USE_SRTP)                               | \
     MBEDTLS_SSL_EXT_MASK(HEARTBEAT)                              | \
     MBEDTLS_SSL_EXT_MASK(ALPN)                                   | \
     MBEDTLS_SSL_EXT_MASK(SCT)                                    | \
     MBEDTLS_SSL_EXT_MASK(CLI_CERT_TYPE)                          | \
     MBEDTLS_SSL_EXT_MASK(SERV_CERT_TYPE)                         | \
     MBEDTLS_SSL_EXT_MASK(PADDING)                                | \
     MBEDTLS_SSL_EXT_MASK(KEY_SHARE)                              | \
     MBEDTLS_SSL_EXT_MASK(PRE_SHARED_KEY)                         | \
     MBEDTLS_SSL_EXT_MASK(PSK_KEY_EXCHANGE_MODES)                 | \
     MBEDTLS_SSL_EXT_MASK(EARLY_DATA)                             | \
     MBEDTLS_SSL_EXT_MASK(COOKIE)                                 | \
     MBEDTLS_SSL_EXT_MASK(SUPPORTED_VERSIONS)                     | \
     MBEDTLS_SSL_EXT_MASK(CERT_AUTH)                              | \
     MBEDTLS_SSL_EXT_MASK(POST_HANDSHAKE_AUTH)                    | \
     MBEDTLS_SSL_EXT_MASK(SIG_ALG_CERT)                           | \
     MBEDTLS_SSL_EXT_MASK(RECORD_SIZE_LIMIT)                      | \
     MBEDTLS_SSL_TLS1_3_EXT_MASK_UNRECOGNIZED)

/* RFC 8446 section 4.2. Allowed extensions for EncryptedExtensions */
#define MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_EE                                  \
    (MBEDTLS_SSL_EXT_MASK(SERVERNAME)                             | \
     MBEDTLS_SSL_EXT_MASK(MAX_FRAGMENT_LENGTH)                    | \
     MBEDTLS_SSL_EXT_MASK(SUPPORTED_GROUPS)                       | \
     MBEDTLS_SSL_EXT_MASK(USE_SRTP)                               | \
     MBEDTLS_SSL_EXT_MASK(HEARTBEAT)                              | \
     MBEDTLS_SSL_EXT_MASK(ALPN)                                   | \
     MBEDTLS_SSL_EXT_MASK(CLI_CERT_TYPE)                          | \
     MBEDTLS_SSL_EXT_MASK(SERV_CERT_TYPE)                         | \
     MBEDTLS_SSL_EXT_MASK(EARLY_DATA)                             | \
     MBEDTLS_SSL_EXT_MASK(RECORD_SIZE_LIMIT))

/* RFC 8446 section 4.2. Allowed extensions for CertificateRequest */
#define MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_CR                                  \
    (MBEDTLS_SSL_EXT_MASK(STATUS_REQUEST)                         | \
     MBEDTLS_SSL_EXT_MASK(SIG_ALG)                                | \
     MBEDTLS_SSL_EXT_MASK(SCT)                                    | \
     MBEDTLS_SSL_EXT_MASK(CERT_AUTH)                              | \
     MBEDTLS_SSL_EXT_MASK(OID_FILTERS)                            | \
     MBEDTLS_SSL_EXT_MASK(SIG_ALG_CERT)                           | \
     MBEDTLS_SSL_TLS1_3_EXT_MASK_UNRECOGNIZED)

/* RFC 8446 section 4.2. Allowed extensions for Certificate */
#define MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_CT                                  \
    (MBEDTLS_SSL_EXT_MASK(STATUS_REQUEST)                         | \
     MBEDTLS_SSL_EXT_MASK(SCT))

/* RFC 8446 section 4.2. Allowed extensions for ServerHello */
#define MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_SH                                  \
    (MBEDTLS_SSL_EXT_MASK(KEY_SHARE)                              | \
     MBEDTLS_SSL_EXT_MASK(PRE_SHARED_KEY)                         | \
     MBEDTLS_SSL_EXT_MASK(SUPPORTED_VERSIONS))

/* RFC 8446 section 4.2. Allowed extensions for HelloRetryRequest */
#define MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_HRR                                 \
    (MBEDTLS_SSL_EXT_MASK(KEY_SHARE)                              | \
     MBEDTLS_SSL_EXT_MASK(COOKIE)                                 | \
     MBEDTLS_SSL_EXT_MASK(SUPPORTED_VERSIONS))

/* RFC 8446 section 4.2. Allowed extensions for NewSessionTicket */
#define MBEDTLS_SSL_TLS1_3_ALLOWED_EXTS_OF_NST                                 \
    (MBEDTLS_SSL_EXT_MASK(EARLY_DATA)                             | \
     MBEDTLS_SSL_TLS1_3_EXT_MASK_UNRECOGNIZED)

/*
 * Helper macros for function call with return check.
 */
/*
 * Exit when return non-zero value
 */
#define MBEDTLS_SSL_PROC_CHK(f)                               \
    do {                                                        \
        ret = (f);                                            \
        if (ret != 0)                                          \
        {                                                       \
            goto cleanup;                                       \
        }                                                       \
    } while (0)
/*
 * Exit when return negative value
 */
#define MBEDTLS_SSL_PROC_CHK_NEG(f)                           \
    do {                                                        \
        ret = (f);                                            \
        if (ret < 0)                                           \
        {                                                       \
            goto cleanup;                                       \
        }                                                       \
    } while (0)

/*
 * DTLS retransmission states, see RFC 6347 4.2.4
 *
 * The SENDING state is merged in PREPARING for initial sends,
 * but is distinct for resends.
 *
 * Note: initial state is wrong for server, but is not used anyway.
 */
#define MBEDTLS_SSL_RETRANS_PREPARING       0
#define MBEDTLS_SSL_RETRANS_SENDING         1
#define MBEDTLS_SSL_RETRANS_WAITING         2
#define MBEDTLS_SSL_RETRANS_FINISHED        3

/*
 * Allow extra bytes for record, authentication and encryption overhead:
 * counter (8) + header (5) + IV(16) + MAC (16-48) + padding (0-256).
 */

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)

/* This macro determines whether CBC is supported. */
#if defined(MBEDTLS_SSL_HAVE_CBC)      &&                                  \
    (defined(MBEDTLS_SSL_HAVE_AES)     ||                                  \
    defined(MBEDTLS_SSL_HAVE_CAMELLIA) ||                                  \
    defined(MBEDTLS_SSL_HAVE_ARIA))
#define MBEDTLS_SSL_SOME_SUITES_USE_CBC
#endif

/* This macro determines whether a ciphersuite using a
 * stream cipher can be used. */
#if defined(MBEDTLS_CIPHER_NULL_CIPHER)
#define MBEDTLS_SSL_SOME_SUITES_USE_STREAM
#endif

/* This macro determines whether the CBC construct used in TLS 1.2 is supported. */
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC) && \
    defined(MBEDTLS_SSL_PROTO_TLS1_2)
#define MBEDTLS_SSL_SOME_SUITES_USE_TLS_CBC
#endif

#if defined(MBEDTLS_SSL_SOME_SUITES_USE_STREAM) || \
    defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC)
#define MBEDTLS_SSL_SOME_SUITES_USE_MAC
#endif

/* This macro determines whether a ciphersuite uses Encrypt-then-MAC with CBC */
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC) && \
    defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
#define MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM
#endif

#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)
/* Ciphersuites using HMAC */
#if defined(MBEDTLS_MD_CAN_SHA384)
#define MBEDTLS_SSL_MAC_ADD                 48  /* SHA-384 used for HMAC */
#elif defined(MBEDTLS_MD_CAN_SHA256)
#define MBEDTLS_SSL_MAC_ADD                 32  /* SHA-256 used for HMAC */
#else
#define MBEDTLS_SSL_MAC_ADD                 20  /* SHA-1   used for HMAC */
#endif
#else /* MBEDTLS_SSL_SOME_SUITES_USE_MAC */
/* AEAD ciphersuites: GCM and CCM use a 128 bits tag */
#define MBEDTLS_SSL_MAC_ADD                 16
#endif

#if defined(MBEDTLS_SSL_HAVE_CBC)
#define MBEDTLS_SSL_PADDING_ADD            256
#else
#define MBEDTLS_SSL_PADDING_ADD              0
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
#define MBEDTLS_SSL_MAX_CID_EXPANSION      MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY
#else
#define MBEDTLS_SSL_MAX_CID_EXPANSION        0
#endif

#define MBEDTLS_SSL_PAYLOAD_OVERHEAD (MBEDTLS_MAX_IV_LENGTH +          \
                                      MBEDTLS_SSL_MAC_ADD +            \
                                      MBEDTLS_SSL_PADDING_ADD +        \
                                      MBEDTLS_SSL_MAX_CID_EXPANSION    \
                                      )

#define MBEDTLS_SSL_IN_PAYLOAD_LEN (MBEDTLS_SSL_PAYLOAD_OVERHEAD + \
                                    (MBEDTLS_SSL_IN_CONTENT_LEN))

#define MBEDTLS_SSL_OUT_PAYLOAD_LEN (MBEDTLS_SSL_PAYLOAD_OVERHEAD + \
                                     (MBEDTLS_SSL_OUT_CONTENT_LEN))

/* The maximum number of buffered handshake messages. */
#define MBEDTLS_SSL_MAX_BUFFERED_HS 4

/* Maximum length we can advertise as our max content length for
   RFC 6066 max_fragment_length extension negotiation purposes
   (the lesser of both sizes, if they are unequal.)
 */
#define MBEDTLS_TLS_EXT_ADV_CONTENT_LEN (                            \
        (MBEDTLS_SSL_IN_CONTENT_LEN > MBEDTLS_SSL_OUT_CONTENT_LEN)   \
        ? (MBEDTLS_SSL_OUT_CONTENT_LEN)                            \
        : (MBEDTLS_SSL_IN_CONTENT_LEN)                             \
        )

/* Maximum size in bytes of list in signature algorithms ext., RFC 5246/8446 */
#define MBEDTLS_SSL_MAX_SIG_ALG_LIST_LEN       65534

/* Minimum size in bytes of list in signature algorithms ext., RFC 5246/8446 */
#define MBEDTLS_SSL_MIN_SIG_ALG_LIST_LEN       2

/* Maximum size in bytes of list in supported elliptic curve ext., RFC 4492 */
#define MBEDTLS_SSL_MAX_CURVE_LIST_LEN         65535

#define MBEDTLS_RECEIVED_SIG_ALGS_SIZE         20

#if defined(MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED)

#define MBEDTLS_TLS_SIG_NONE MBEDTLS_TLS1_3_SIG_NONE

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
#define MBEDTLS_SSL_TLS12_SIG_AND_HASH_ALG(sig, hash) ((hash << 8) | sig)
#define MBEDTLS_SSL_TLS12_SIG_ALG_FROM_SIG_AND_HASH_ALG(alg) (alg & 0xFF)
#define MBEDTLS_SSL_TLS12_HASH_ALG_FROM_SIG_AND_HASH_ALG(alg) (alg >> 8)
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

#endif /* MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED */

/*
 * Check that we obey the standard's message size bounds
 */

#if MBEDTLS_SSL_IN_CONTENT_LEN > 16384
#error "Bad configuration - incoming record content too large."
#endif

#if MBEDTLS_SSL_OUT_CONTENT_LEN > 16384
#error "Bad configuration - outgoing record content too large."
#endif

#if MBEDTLS_SSL_IN_PAYLOAD_LEN > MBEDTLS_SSL_IN_CONTENT_LEN + 2048
#error "Bad configuration - incoming protected record payload too large."
#endif

#if MBEDTLS_SSL_OUT_PAYLOAD_LEN > MBEDTLS_SSL_OUT_CONTENT_LEN + 2048
#error "Bad configuration - outgoing protected record payload too large."
#endif

/* Calculate buffer sizes */

/* Note: Even though the TLS record header is only 5 bytes
   long, we're internally using 8 bytes to store the
   implicit sequence number. */
#define MBEDTLS_SSL_HEADER_LEN 13

#if !defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
#define MBEDTLS_SSL_IN_BUFFER_LEN  \
    ((MBEDTLS_SSL_HEADER_LEN) + (MBEDTLS_SSL_IN_PAYLOAD_LEN))
#else
#define MBEDTLS_SSL_IN_BUFFER_LEN  \
    ((MBEDTLS_SSL_HEADER_LEN) + (MBEDTLS_SSL_IN_PAYLOAD_LEN) \
     + (MBEDTLS_SSL_CID_IN_LEN_MAX))
#endif

#if !defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
#define MBEDTLS_SSL_OUT_BUFFER_LEN  \
    ((MBEDTLS_SSL_HEADER_LEN) + (MBEDTLS_SSL_OUT_PAYLOAD_LEN))
#else
#define MBEDTLS_SSL_OUT_BUFFER_LEN                               \
    ((MBEDTLS_SSL_HEADER_LEN) + (MBEDTLS_SSL_OUT_PAYLOAD_LEN)    \
     + (MBEDTLS_SSL_CID_OUT_LEN_MAX))
#endif

#define MBEDTLS_CLIENT_HELLO_RANDOM_LEN 32
#define MBEDTLS_SERVER_HELLO_RANDOM_LEN 32

#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
/**
 * \brief          Return the maximum fragment length (payload, in bytes) for
 *                 the output buffer. For the client, this is the configured
 *                 value. For the server, it is the minimum of two - the
 *                 configured value and the negotiated one.
 *
 * \sa             mbedtls_ssl_conf_max_frag_len()
 * \sa             mbedtls_ssl_get_max_out_record_payload()
 *
 * \param ssl      SSL context
 *
 * \return         Current maximum fragment length for the output buffer.
 */
size_t mbedtls_ssl_get_output_max_frag_len(const mbedtls_ssl_context *ssl);

/**
 * \brief          Return the maximum fragment length (payload, in bytes) for
 *                 the input buffer. This is the negotiated maximum fragment
 *                 length, or, if there is none, MBEDTLS_SSL_IN_CONTENT_LEN.
 *                 If it is not defined either, the value is 2^14. This function
 *                 works as its predecessor, \c mbedtls_ssl_get_max_frag_len().
 *
 * \sa             mbedtls_ssl_conf_max_frag_len()
 * \sa             mbedtls_ssl_get_max_in_record_payload()
 *
 * \param ssl      SSL context
 *
 * \return         Current maximum fragment length for the output buffer.
 */
size_t mbedtls_ssl_get_input_max_frag_len(const mbedtls_ssl_context *ssl);
#endif /* MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */

#if defined(MBEDTLS_SSL_RECORD_SIZE_LIMIT)
/**
 * \brief    Get the size limit in bytes for the protected outgoing records
 *           as defined in RFC 8449
 *
 * \param ssl      SSL context
 *
 * \return         The size limit in bytes for the protected outgoing
 *                 records as defined in RFC 8449.
 */
size_t mbedtls_ssl_get_output_record_size_limit(const mbedtls_ssl_context *ssl);
#endif /* MBEDTLS_SSL_RECORD_SIZE_LIMIT */

#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
static inline size_t mbedtls_ssl_get_output_buflen(const mbedtls_ssl_context *ctx)
{
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    return mbedtls_ssl_get_output_max_frag_len(ctx)
           + MBEDTLS_SSL_HEADER_LEN + MBEDTLS_SSL_PAYLOAD_OVERHEAD
           + MBEDTLS_SSL_CID_OUT_LEN_MAX;
#else
    return mbedtls_ssl_get_output_max_frag_len(ctx)
           + MBEDTLS_SSL_HEADER_LEN + MBEDTLS_SSL_PAYLOAD_OVERHEAD;
#endif
}

static inline size_t mbedtls_ssl_get_input_buflen(const mbedtls_ssl_context *ctx)
{
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    return mbedtls_ssl_get_input_max_frag_len(ctx)
           + MBEDTLS_SSL_HEADER_LEN + MBEDTLS_SSL_PAYLOAD_OVERHEAD
           + MBEDTLS_SSL_CID_IN_LEN_MAX;
#else
    return mbedtls_ssl_get_input_max_frag_len(ctx)
           + MBEDTLS_SSL_HEADER_LEN + MBEDTLS_SSL_PAYLOAD_OVERHEAD;
#endif
}
#endif

/*
 * TLS extension flags (for extensions with outgoing ServerHello content
 * that need it (e.g. for RENEGOTIATION_INFO the server already knows because
 * of state of the renegotiation flag, so no indicator is required)
 */
#define MBEDTLS_TLS_EXT_SUPPORTED_POINT_FORMATS_PRESENT (1 << 0)
#define MBEDTLS_TLS_EXT_ECJPAKE_KKPP_OK                 (1 << 1)

/**
 * \brief        This function checks if the remaining size in a buffer is
 *               greater or equal than a needed space.
 *
 * \param cur    Pointer to the current position in the buffer.
 * \param end    Pointer to one past the end of the buffer.
 * \param need   Needed space in bytes.
 *
 * \return       Zero if the needed space is available in the buffer, non-zero
 *               otherwise.
 */
#if !defined(MBEDTLS_TEST_HOOKS)
static inline int mbedtls_ssl_chk_buf_ptr(const uint8_t *cur,
                                          const uint8_t *end, size_t need)
{
    return (cur > end) || (need > (size_t) (end - cur));
}
#else
typedef struct {
    const uint8_t *cur;
    const uint8_t *end;
    size_t need;
} mbedtls_ssl_chk_buf_ptr_args;

void mbedtls_ssl_set_chk_buf_ptr_fail_args(
    const uint8_t *cur, const uint8_t *end, size_t need);
void mbedtls_ssl_reset_chk_buf_ptr_fail_args(void);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_cmp_chk_buf_ptr_fail_args(mbedtls_ssl_chk_buf_ptr_args *args);

static inline int mbedtls_ssl_chk_buf_ptr(const uint8_t *cur,
                                          const uint8_t *end, size_t need)
{
    if ((cur > end) || (need > (size_t) (end - cur))) {
        mbedtls_ssl_set_chk_buf_ptr_fail_args(cur, end, need);
        return 1;
    }
    return 0;
}
#endif /* MBEDTLS_TEST_HOOKS */

/**
 * \brief        This macro checks if the remaining size in a buffer is
 *               greater or equal than a needed space. If it is not the case,
 *               it returns an SSL_BUFFER_TOO_SMALL error.
 *
 * \param cur    Pointer to the current position in the buffer.
 * \param end    Pointer to one past the end of the buffer.
 * \param need   Needed space in bytes.
 *
 */
#define MBEDTLS_SSL_CHK_BUF_PTR(cur, end, need)                        \
    do {                                                                 \
        if (mbedtls_ssl_chk_buf_ptr((cur), (end), (need)) != 0) \
        {                                                                \
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;                  \
        }                                                                \
    } while (0)

/**
 * \brief        This macro checks if the remaining length in an input buffer is
 *               greater or equal than a needed length. If it is not the case, it
 *               returns #MBEDTLS_ERR_SSL_DECODE_ERROR error and pends a
 *               #MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR alert message.
 *
 *               This is a function-like macro. It is guaranteed to evaluate each
 *               argument exactly once.
 *
 * \param cur    Pointer to the current position in the buffer.
 * \param end    Pointer to one past the end of the buffer.
 * \param need   Needed length in bytes.
 *
 */
#define MBEDTLS_SSL_CHK_BUF_READ_PTR(cur, end, need)                          \
    do {                                                                        \
        if (mbedtls_ssl_chk_buf_ptr((cur), (end), (need)) != 0)        \
        {                                                                       \
            MBEDTLS_SSL_DEBUG_MSG(1,                                           \
                                  ("missing input data in %s", __func__));  \
            MBEDTLS_SSL_PEND_FATAL_ALERT(MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR,   \
                                         MBEDTLS_ERR_SSL_DECODE_ERROR);       \
            return MBEDTLS_ERR_SSL_DECODE_ERROR;                             \
        }                                                                       \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef int  mbedtls_ssl_tls_prf_cb(const unsigned char *secret, size_t slen,
                                    const char *label,
                                    const unsigned char *random, size_t rlen,
                                    unsigned char *dstbuf, size_t dlen);

/* cipher.h exports the maximum IV, key and block length from
 * all ciphers enabled in the config, regardless of whether those
 * ciphers are actually usable in SSL/TLS. Notably, XTS is enabled
 * in the default configuration and uses 64 Byte keys, but it is
 * not used for record protection in SSL/TLS.
 *
 * In order to prevent unnecessary inflation of key structures,
 * we introduce SSL-specific variants of the max-{key,block,IV}
 * macros here which are meant to only take those ciphers into
 * account which can be negotiated in SSL/TLS.
 *
 * Since the current definitions of MBEDTLS_MAX_{KEY|BLOCK|IV}_LENGTH
 * in cipher.h are rough overapproximations of the real maxima, here
 * we content ourselves with replicating those overapproximations
 * for the maximum block and IV length, and excluding XTS from the
 * computation of the maximum key length. */
#define MBEDTLS_SSL_MAX_BLOCK_LENGTH 16
#define MBEDTLS_SSL_MAX_IV_LENGTH    16
#define MBEDTLS_SSL_MAX_KEY_LENGTH   32

/**
 * \brief   The data structure holding the cryptographic material (key and IV)
 *          used for record protection in TLS 1.3.
 */
struct mbedtls_ssl_key_set {
    /*! The key for client->server records. */
    unsigned char client_write_key[MBEDTLS_SSL_MAX_KEY_LENGTH];
    /*! The key for server->client records. */
    unsigned char server_write_key[MBEDTLS_SSL_MAX_KEY_LENGTH];
    /*! The IV  for client->server records. */
    unsigned char client_write_iv[MBEDTLS_SSL_MAX_IV_LENGTH];
    /*! The IV  for server->client records. */
    unsigned char server_write_iv[MBEDTLS_SSL_MAX_IV_LENGTH];

    size_t key_len; /*!< The length of client_write_key and
                     *   server_write_key, in Bytes. */
    size_t iv_len;  /*!< The length of client_write_iv and
                     *   server_write_iv, in Bytes. */
};
typedef struct mbedtls_ssl_key_set mbedtls_ssl_key_set;

typedef struct {
    unsigned char binder_key[MBEDTLS_TLS1_3_MD_MAX_SIZE];
    unsigned char client_early_traffic_secret[MBEDTLS_TLS1_3_MD_MAX_SIZE];
    unsigned char early_exporter_master_secret[MBEDTLS_TLS1_3_MD_MAX_SIZE];
} mbedtls_ssl_tls13_early_secrets;

typedef struct {
    unsigned char client_handshake_traffic_secret[MBEDTLS_TLS1_3_MD_MAX_SIZE];
    unsigned char server_handshake_traffic_secret[MBEDTLS_TLS1_3_MD_MAX_SIZE];
} mbedtls_ssl_tls13_handshake_secrets;

/*
 * This structure contains the parameters only needed during handshake.
 */
struct mbedtls_ssl_handshake_params {
    /* Frequently-used boolean or byte fields (placed early to take
     * advantage of smaller code size for indirect access on Arm Thumb) */
    uint8_t resume;                     /*!<  session resume indicator*/
    uint8_t cli_exts;                   /*!< client extension presence*/

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
    uint8_t sni_authmode;               /*!< authmode from SNI callback     */
#endif

#if defined(MBEDTLS_SSL_SRV_C)
    /* Flag indicating if a CertificateRequest message has been sent
     * to the client or not. */
    uint8_t certificate_request_sent;
#if defined(MBEDTLS_SSL_EARLY_DATA)
    /* Flag indicating if the server has accepted early data or not. */
    uint8_t early_data_accepted;
#endif
#endif /* MBEDTLS_SSL_SRV_C */

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
    uint8_t new_session_ticket;         /*!< use NewSessionTicket?    */
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

#if defined(MBEDTLS_SSL_CLI_C)
    /** Minimum TLS version to be negotiated.
     *
     * It is set up in the ClientHello writing preparation stage and used
     * throughout the ClientHello writing. Not relevant anymore as soon as
     * the protocol version has been negotiated thus as soon as the
     * ServerHello is received.
     * For a fresh handshake not linked to any previous handshake, it is
     * equal to the configured minimum minor version to be negotiated. When
     * renegotiating or resuming a session, it is equal to the previously
     * negotiated minor version.
     *
     * There is no maximum TLS version field in this handshake context.
     * From the start of the handshake, we need to define a current protocol
     * version for the record layer which we define as the maximum TLS
     * version to be negotiated. The `tls_version` field of the SSL context is
     * used to store this maximum value until it contains the actual
     * negotiated value.
     */
    mbedtls_ssl_protocol_version min_tls_version;
#endif

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET)
    uint8_t extended_ms;                /*!< use Extended Master Secret? */
#endif

#if defined(MBEDTLS_SSL_ASYNC_PRIVATE)
    uint8_t async_in_progress; /*!< an asynchronous operation is in progress */
#endif /* MBEDTLS_SSL_ASYNC_PRIVATE */

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    unsigned char retransmit_state;     /*!<  Retransmission state           */
#endif

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
    unsigned char group_list_heap_allocated;
    unsigned char sig_algs_heap_allocated;
#endif

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
    uint8_t ecrs_enabled;               /*!< Handshake supports EC restart? */
    enum { /* this complements ssl->state with info on intra-state operations */
        ssl_ecrs_none = 0,              /*!< nothing going on (yet)         */
        ssl_ecrs_crt_verify,            /*!< Certificate: crt_verify()      */
        ssl_ecrs_ske_start_processing,  /*!< ServerKeyExchange: pk_verify() */
        ssl_ecrs_cke_ecdh_calc_secret,  /*!< ClientKeyExchange: ECDH step 2 */
        ssl_ecrs_crt_vrfy_sign,         /*!< CertificateVerify: pk_sign()   */
    } ecrs_state;                       /*!< current (or last) operation    */
    mbedtls_x509_crt *ecrs_peer_cert;   /*!< The peer's CRT chain.          */
    size_t ecrs_n;                      /*!< place for saving a length      */
#endif

    mbedtls_ssl_ciphersuite_t const *ciphersuite_info;

    MBEDTLS_CHECK_RETURN_CRITICAL
    int (*update_checksum)(mbedtls_ssl_context *, const unsigned char *, size_t);
    MBEDTLS_CHECK_RETURN_CRITICAL
    int (*calc_verify)(const mbedtls_ssl_context *, unsigned char *, size_t *);
    MBEDTLS_CHECK_RETURN_CRITICAL
    int (*calc_finished)(mbedtls_ssl_context *, unsigned char *, int);
    mbedtls_ssl_tls_prf_cb *tls_prf;

    /*
     * Handshake specific crypto variables
     */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    uint8_t key_exchange_mode; /*!< Selected key exchange mode */

    /**
     * Flag indicating if, in the course of the current handshake, an
     * HelloRetryRequest message has been sent by the server or received by
     * the client (<> 0) or not (0).
     */
    uint8_t hello_retry_request_flag;

#if defined(MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE)
    /**
     * Flag indicating if, in the course of the current handshake, a dummy
     * change_cipher_spec (CCS) record has already been sent. Used to send only
     * one CCS per handshake while not complicating the handshake state
     * transitions for that purpose.
     */
    uint8_t ccs_sent;
#endif

#if defined(MBEDTLS_SSL_SRV_C)
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
    uint8_t tls13_kex_modes; /*!< Key exchange modes supported by the client */
#endif
    /** selected_group of key_share extension in HelloRetryRequest message. */
    uint16_t hrr_selected_group;
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
    uint16_t new_session_tickets_count;         /*!< number of session tickets */
#endif
#endif /* MBEDTLS_SSL_SRV_C */

#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED)
    uint16_t received_sig_algs[MBEDTLS_RECEIVED_SIG_ALGS_SIZE];
#endif

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
    const uint16_t *group_list;
    const uint16_t *sig_algs;
#endif

#if defined(MBEDTLS_DHM_C)
    mbedtls_dhm_context dhm_ctx;                /*!<  DHM key exchange        */
#endif

#if !defined(MBEDTLS_USE_PSA_CRYPTO) && \
    defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED)
    mbedtls_ecdh_context ecdh_ctx;              /*!<  ECDH key exchange       */
#endif /* !MBEDTLS_USE_PSA_CRYPTO &&
          MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_XXDH_PSA_ANY_ENABLED)
    psa_key_type_t xxdh_psa_type;
    size_t xxdh_psa_bits;
    mbedtls_svc_key_id_t xxdh_psa_privkey;
    uint8_t xxdh_psa_privkey_is_external;
    unsigned char xxdh_psa_peerkey[PSA_EXPORT_PUBLIC_KEY_MAX_SIZE];
    size_t xxdh_psa_peerkey_len;
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_XXDH_PSA_ANY_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_pake_operation_t psa_pake_ctx;        /*!< EC J-PAKE key exchange */
    mbedtls_svc_key_id_t psa_pake_password;
    uint8_t psa_pake_ctx_is_ok;
#else
    mbedtls_ecjpake_context ecjpake_ctx;        /*!< EC J-PAKE key exchange */
#endif /* MBEDTLS_USE_PSA_CRYPTO */
#if defined(MBEDTLS_SSL_CLI_C)
    unsigned char *ecjpake_cache;               /*!< Cache for ClientHello ext */
    size_t ecjpake_cache_len;                   /*!< Length of cached data */
#endif
#endif /* MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_ANY_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ANY_ALLOWED_ENABLED) || \
    defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
    uint16_t *curves_tls_id;      /*!<  List of TLS IDs of supported elliptic curves */
#endif

#if defined(MBEDTLS_SSL_HANDSHAKE_WITH_PSK_ENABLED)
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    mbedtls_svc_key_id_t psk_opaque;            /*!< Opaque PSK from the callback   */
    uint8_t psk_opaque_is_internal;
#else
    unsigned char *psk;                 /*!<  PSK from the callback         */
    size_t psk_len;                     /*!<  Length of PSK from callback   */
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    uint16_t    selected_identity;
#endif /* MBEDTLS_SSL_HANDSHAKE_WITH_PSK_ENABLED */

#if defined(MBEDTLS_SSL_ECP_RESTARTABLE_ENABLED)
    mbedtls_x509_crt_restart_ctx ecrs_ctx;  /*!< restart context            */
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C)
    mbedtls_ssl_key_cert *key_cert;     /*!< chosen key/cert pair (server)  */
#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
    mbedtls_ssl_key_cert *sni_key_cert; /*!< key/cert list from SNI         */
    mbedtls_x509_crt *sni_ca_chain;     /*!< trusted CAs from SNI callback  */
    mbedtls_x509_crl *sni_ca_crl;       /*!< trusted CAs CRLs from SNI      */
#endif /* MBEDTLS_SSL_SERVER_NAME_INDICATION */
#endif /* MBEDTLS_X509_CRT_PARSE_C */

#if defined(MBEDTLS_X509_CRT_PARSE_C) &&        \
    !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    mbedtls_pk_context peer_pubkey;     /*!< The public key from the peer.  */
#endif /* MBEDTLS_X509_CRT_PARSE_C && !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */

    struct {
        size_t total_bytes_buffered; /*!< Cumulative size of heap allocated
                                      *   buffers used for message buffering. */

        uint8_t seen_ccs;               /*!< Indicates if a CCS message has
                                         *   been seen in the current flight. */

        struct mbedtls_ssl_hs_buffer {
            unsigned is_valid      : 1;
            unsigned is_fragmented : 1;
            unsigned is_complete   : 1;
            unsigned char *data;
            size_t data_len;
        } hs[MBEDTLS_SSL_MAX_BUFFERED_HS];

        struct {
            unsigned char *data;
            size_t len;
            unsigned epoch;
        } future_record;

    } buffering;

#if defined(MBEDTLS_SSL_CLI_C) && \
    (defined(MBEDTLS_SSL_PROTO_DTLS) || \
    defined(MBEDTLS_SSL_PROTO_TLS1_3))
    unsigned char *cookie;              /*!< HelloVerifyRequest cookie for DTLS
                                         *   HelloRetryRequest cookie for TLS 1.3 */
#if !defined(MBEDTLS_SSL_PROTO_TLS1_3)
    /* RFC 6347 page 15
       ...
       opaque cookie<0..2^8-1>;
       ...
     */
    uint8_t cookie_len;
#else
    /* RFC 8446 page 39
       ...
       opaque cookie<0..2^16-1>;
       ...
       If TLS1_3 is enabled, the max length is 2^16 - 1
     */
    uint16_t cookie_len;                /*!< DTLS: HelloVerifyRequest cookie length
                                         *   TLS1_3: HelloRetryRequest cookie length */
#endif
#endif /* MBEDTLS_SSL_CLI_C &&
          ( MBEDTLS_SSL_PROTO_DTLS ||
            MBEDTLS_SSL_PROTO_TLS1_3 ) */
#if defined(MBEDTLS_SSL_SRV_C) && defined(MBEDTLS_SSL_PROTO_DTLS)
    unsigned char cookie_verify_result; /*!< Srv: flag for sending a cookie */
#endif /* MBEDTLS_SSL_SRV_C && MBEDTLS_SSL_PROTO_DTLS */

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    unsigned int out_msg_seq;           /*!<  Outgoing handshake sequence number */
    unsigned int in_msg_seq;            /*!<  Incoming handshake sequence number */

    uint32_t retransmit_timeout;        /*!<  Current value of timeout       */
    mbedtls_ssl_flight_item *flight;    /*!<  Current outgoing flight        */
    mbedtls_ssl_flight_item *cur_msg;   /*!<  Current message in flight      */
    unsigned char *cur_msg_p;           /*!<  Position in current message    */
    unsigned int in_flight_start_seq;   /*!<  Minimum message sequence in the
                                              flight being received          */
    mbedtls_ssl_transform *alt_transform_out;   /*!<  Alternative transform for
                                                   resending messages             */
    unsigned char alt_out_ctr[MBEDTLS_SSL_SEQUENCE_NUMBER_LEN]; /*!<  Alternative record epoch/counter
                                                                      for resending messages         */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    /* The state of CID configuration in this handshake. */

    uint8_t cid_in_use; /*!< This indicates whether the use of the CID extension
                         *   has been negotiated. Possible values are
                         *   #MBEDTLS_SSL_CID_ENABLED and
                         *   #MBEDTLS_SSL_CID_DISABLED. */
    unsigned char peer_cid[MBEDTLS_SSL_CID_OUT_LEN_MAX];   /*! The peer's CID */
    uint8_t peer_cid_len;                                  /*!< The length of
                                                            *   \c peer_cid.  */
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

    uint16_t mtu;                       /*!<  Handshake mtu, used to fragment outgoing messages */
#endif /* MBEDTLS_SSL_PROTO_DTLS */

    /*
     * Checksum contexts
     */
#if defined(MBEDTLS_MD_CAN_SHA256)
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_hash_operation_t fin_sha256_psa;
#else
    mbedtls_md_context_t fin_sha256;
#endif
#endif
#if defined(MBEDTLS_MD_CAN_SHA384)
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_hash_operation_t fin_sha384_psa;
#else
    mbedtls_md_context_t fin_sha384;
#endif
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    uint16_t offered_group_id; /* The NamedGroup value for the group
                                * that is being used for ephemeral
                                * key exchange.
                                *
                                * On the client: Defaults to the first
                                * entry in the client's group list,
                                * but can be overwritten by the HRR. */
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_CLI_C)
    uint8_t client_auth;       /*!< used to check if CertificateRequest has been
                                    received from server side. If CertificateRequest
                                    has been received, Certificate and CertificateVerify
                                    should be sent to server */
#endif /* MBEDTLS_SSL_CLI_C */
    /*
     * State-local variables used during the processing
     * of a specific handshake state.
     */
    union {
        /* Outgoing Finished message */
        struct {
            uint8_t preparation_done;

            /* Buffer holding digest of the handshake up to
             * but excluding the outgoing finished message. */
            unsigned char digest[MBEDTLS_TLS1_3_MD_MAX_SIZE];
            size_t digest_len;
        } finished_out;

        /* Incoming Finished message */
        struct {
            uint8_t preparation_done;

            /* Buffer holding digest of the handshake up to but
             * excluding the peer's incoming finished message. */
            unsigned char digest[MBEDTLS_TLS1_3_MD_MAX_SIZE];
            size_t digest_len;
        } finished_in;

    } state_local;

    /* End of state-local variables. */

    unsigned char randbytes[MBEDTLS_CLIENT_HELLO_RANDOM_LEN +
                            MBEDTLS_SERVER_HELLO_RANDOM_LEN];
    /*!<  random bytes            */
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
    unsigned char premaster[MBEDTLS_PREMASTER_SIZE];
    /*!<  premaster secret        */
    size_t pmslen;                      /*!<  premaster length        */
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    uint32_t sent_extensions;       /*!< extensions sent by endpoint */
    uint32_t received_extensions;   /*!< extensions received by endpoint */

#if defined(MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED)
    unsigned char certificate_request_context_len;
    unsigned char *certificate_request_context;
#endif

    /** TLS 1.3 transform for encrypted handshake messages. */
    mbedtls_ssl_transform *transform_handshake;
    union {
        unsigned char early[MBEDTLS_TLS1_3_MD_MAX_SIZE];
        unsigned char handshake[MBEDTLS_TLS1_3_MD_MAX_SIZE];
        unsigned char app[MBEDTLS_TLS1_3_MD_MAX_SIZE];
    } tls13_master_secrets;

    mbedtls_ssl_tls13_handshake_secrets tls13_hs_secrets;
#if defined(MBEDTLS_SSL_EARLY_DATA)
    /** TLS 1.3 transform for early data and handshake messages. */
    mbedtls_ssl_transform *transform_earlydata;
#endif
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_ASYNC_PRIVATE)
    /** Asynchronous operation context. This field is meant for use by the
     * asynchronous operation callbacks (mbedtls_ssl_config::f_async_sign_start,
     * mbedtls_ssl_config::f_async_decrypt_start,
     * mbedtls_ssl_config::f_async_resume, mbedtls_ssl_config::f_async_cancel).
     * The library does not use it internally. */
    void *user_async_ctx;
#endif /* MBEDTLS_SSL_ASYNC_PRIVATE */

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
    const unsigned char *sni_name;      /*!< raw SNI                        */
    size_t sni_name_len;                /*!< raw SNI len                    */
#if defined(MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED)
    const mbedtls_x509_crt *dn_hints;   /*!< acceptable client cert issuers */
#endif
#endif /* MBEDTLS_SSL_SERVER_NAME_INDICATION */
};

typedef struct mbedtls_ssl_hs_buffer mbedtls_ssl_hs_buffer;

/*
 * Representation of decryption/encryption transformations on records
 *
 * There are the following general types of record transformations:
 * - Stream transformations (TLS versions == 1.2 only)
 *   Transformation adding a MAC and applying a stream-cipher
 *   to the authenticated message.
 * - CBC block cipher transformations ([D]TLS versions == 1.2 only)
 *   For TLS 1.2, no IV is generated at key extraction time, but every
 *   encrypted record is explicitly prefixed by the IV with which it was
 *   encrypted.
 * - AEAD transformations ([D]TLS versions == 1.2 only)
 *   These come in two fundamentally different versions, the first one
 *   used in TLS 1.2, excluding ChaChaPoly ciphersuites, and the second
 *   one used for ChaChaPoly ciphersuites in TLS 1.2 as well as for TLS 1.3.
 *   In the first transformation, the IV to be used for a record is obtained
 *   as the concatenation of an explicit, static 4-byte IV and the 8-byte
 *   record sequence number, and explicitly prepending this sequence number
 *   to the encrypted record. In contrast, in the second transformation
 *   the IV is obtained by XOR'ing a static IV obtained at key extraction
 *   time with the 8-byte record sequence number, without prepending the
 *   latter to the encrypted record.
 *
 * Additionally, DTLS 1.2 + CID as well as TLS 1.3 use an inner plaintext
 * which allows to add flexible length padding and to hide a record's true
 * content type.
 *
 * In addition to type and version, the following parameters are relevant:
 * - The symmetric cipher algorithm to be used.
 * - The (static) encryption/decryption keys for the cipher.
 * - For stream/CBC, the type of message digest to be used.
 * - For stream/CBC, (static) encryption/decryption keys for the digest.
 * - For AEAD transformations, the size (potentially 0) of an explicit,
 *   random initialization vector placed in encrypted records.
 * - For some transformations (currently AEAD) an implicit IV. It is static
 *   and (if present) is combined with the explicit IV in a transformation-
 *   -dependent way (e.g. appending in TLS 1.2 and XOR'ing in TLS 1.3).
 * - For stream/CBC, a flag determining the order of encryption and MAC.
 * - The details of the transformation depend on the SSL/TLS version.
 * - The length of the authentication tag.
 *
 * The struct below refines this abstract view as follows:
 * - The cipher underlying the transformation is managed in
 *   cipher contexts cipher_ctx_{enc/dec}, which must have the
 *   same cipher type. The mode of these cipher contexts determines
 *   the type of the transformation in the sense above: e.g., if
 *   the type is MBEDTLS_CIPHER_AES_256_CBC resp. MBEDTLS_CIPHER_AES_192_GCM
 *   then the transformation has type CBC resp. AEAD.
 * - The cipher keys are never stored explicitly but
 *   are maintained within cipher_ctx_{enc/dec}.
 * - For stream/CBC transformations, the message digest contexts
 *   used for the MAC's are stored in md_ctx_{enc/dec}. These contexts
 *   are unused for AEAD transformations.
 * - For stream/CBC transformations, the MAC keys are not stored explicitly
 *   but maintained within md_ctx_{enc/dec}.
 * - The mac_enc and mac_dec fields are unused for EAD transformations.
 * - For transformations using an implicit IV maintained within
 *   the transformation context, its contents are stored within
 *   iv_{enc/dec}.
 * - The value of ivlen indicates the length of the IV.
 *   This is redundant in case of stream/CBC transformations
 *   which always use 0 resp. the cipher's block length as the
 *   IV length, but is needed for AEAD ciphers and may be
 *   different from the underlying cipher's block length
 *   in this case.
 * - The field fixed_ivlen is nonzero for AEAD transformations only
 *   and indicates the length of the static part of the IV which is
 *   constant throughout the communication, and which is stored in
 *   the first fixed_ivlen bytes of the iv_{enc/dec} arrays.
 * - tls_version denotes the 2-byte TLS version
 * - For stream/CBC transformations, maclen denotes the length of the
 *   authentication tag, while taglen is unused and 0.
 * - For AEAD transformations, taglen denotes the length of the
 *   authentication tag, while maclen is unused and 0.
 * - For CBC transformations, encrypt_then_mac determines the
 *   order of encryption and authentication. This field is unused
 *   in other transformations.
 *
 */
struct mbedtls_ssl_transform {
    /*
     * Session specific crypto layer
     */
    size_t minlen;                      /*!<  min. ciphertext length  */
    size_t ivlen;                       /*!<  IV length               */
    size_t fixed_ivlen;                 /*!<  Fixed part of IV (AEAD) */
    size_t maclen;                      /*!<  MAC(CBC) len            */
    size_t taglen;                      /*!<  TAG(AEAD) len           */

    unsigned char iv_enc[16];           /*!<  IV (encryption)         */
    unsigned char iv_dec[16];           /*!<  IV (decryption)         */

#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    mbedtls_svc_key_id_t psa_mac_enc;           /*!<  MAC (encryption)        */
    mbedtls_svc_key_id_t psa_mac_dec;           /*!<  MAC (decryption)        */
    psa_algorithm_t psa_mac_alg;                /*!<  psa MAC algorithm       */
#else
    mbedtls_md_context_t md_ctx_enc;            /*!<  MAC (encryption)        */
    mbedtls_md_context_t md_ctx_dec;            /*!<  MAC (decryption)        */
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
    int encrypt_then_mac;       /*!< flag for EtM activation                */
#endif

#endif /* MBEDTLS_SSL_SOME_SUITES_USE_MAC */

    mbedtls_ssl_protocol_version tls_version;

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    mbedtls_svc_key_id_t psa_key_enc;           /*!<  psa encryption key      */
    mbedtls_svc_key_id_t psa_key_dec;           /*!<  psa decryption key      */
    psa_algorithm_t psa_alg;                    /*!<  psa algorithm           */
#else
    mbedtls_cipher_context_t cipher_ctx_enc;    /*!<  encryption context      */
    mbedtls_cipher_context_t cipher_ctx_dec;    /*!<  decryption context      */
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    uint8_t in_cid_len;
    uint8_t out_cid_len;
    unsigned char in_cid[MBEDTLS_SSL_CID_IN_LEN_MAX];
    unsigned char out_cid[MBEDTLS_SSL_CID_OUT_LEN_MAX];
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

#if defined(MBEDTLS_SSL_KEEP_RANDBYTES)
    /* We need the Hello random bytes in order to re-derive keys from the
     * Master Secret and other session info and for the keying material
     * exporter in TLS 1.2.
     * See ssl_tls12_populate_transform() */
    unsigned char randbytes[MBEDTLS_SERVER_HELLO_RANDOM_LEN +
                            MBEDTLS_CLIENT_HELLO_RANDOM_LEN];
    /*!< ServerHello.random+ClientHello.random */
#endif /* defined(MBEDTLS_SSL_KEEP_RANDBYTES) */
};

/*
 * Return 1 if the transform uses an AEAD cipher, 0 otherwise.
 * Equivalently, return 0 if a separate MAC is used, 1 otherwise.
 */
static inline int mbedtls_ssl_transform_uses_aead(
    const mbedtls_ssl_transform *transform)
{
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)
    return transform->maclen == 0 && transform->taglen != 0;
#else
    (void) transform;
    return 1;
#endif
}

/*
 * Internal representation of record frames
 *
 * Instances come in two flavors:
 * (1) Encrypted
 *     These always have data_offset = 0
 * (2) Unencrypted
 *     These have data_offset set to the amount of
 *     pre-expansion during record protection. Concretely,
 *     this is the length of the fixed part of the explicit IV
 *     used for encryption, or 0 if no explicit IV is used
 *     (e.g. for stream ciphers).
 *
 * The reason for the data_offset in the unencrypted case
 * is to allow for in-place conversion of an unencrypted to
 * an encrypted record. If the offset wasn't included, the
 * encrypted content would need to be shifted afterwards to
 * make space for the fixed IV.
 *
 */
#if MBEDTLS_SSL_CID_OUT_LEN_MAX > MBEDTLS_SSL_CID_IN_LEN_MAX
#define MBEDTLS_SSL_CID_LEN_MAX MBEDTLS_SSL_CID_OUT_LEN_MAX
#else
#define MBEDTLS_SSL_CID_LEN_MAX MBEDTLS_SSL_CID_IN_LEN_MAX
#endif

typedef struct {
    uint8_t ctr[MBEDTLS_SSL_SEQUENCE_NUMBER_LEN];  /* In TLS:  The implicit record sequence number.
                                                    * In DTLS: The 2-byte epoch followed by
                                                    *          the 6-byte sequence number.
                                                    * This is stored as a raw big endian byte array
                                                    * as opposed to a uint64_t because we rarely
                                                    * need to perform arithmetic on this, but do
                                                    * need it as a Byte array for the purpose of
                                                    * MAC computations.                             */
    uint8_t type;           /* The record content type.                      */
    uint8_t ver[2];         /* SSL/TLS version as present on the wire.
                             * Convert to internal presentation of versions
                             * using mbedtls_ssl_read_version() and
                             * mbedtls_ssl_write_version().
                             * Keep wire-format for MAC computations.        */

    unsigned char *buf;     /* Memory buffer enclosing the record content    */
    size_t buf_len;         /* Buffer length                                 */
    size_t data_offset;     /* Offset of record content                      */
    size_t data_len;        /* Length of record content                      */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    uint8_t cid_len;        /* Length of the CID (0 if not present)          */
    unsigned char cid[MBEDTLS_SSL_CID_LEN_MAX];   /* The CID                 */
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
} mbedtls_record;

#if defined(MBEDTLS_X509_CRT_PARSE_C)
/*
 * List of certificate + private key pairs
 */
struct mbedtls_ssl_key_cert {
    mbedtls_x509_crt *cert;                 /*!< cert                       */
    mbedtls_pk_context *key;                /*!< private key                */
    mbedtls_ssl_key_cert *next;             /*!< next key/cert pair         */
};
#endif /* MBEDTLS_X509_CRT_PARSE_C */

#if defined(MBEDTLS_SSL_PROTO_DTLS)
/*
 * List of handshake messages kept around for resending
 */
struct mbedtls_ssl_flight_item {
    unsigned char *p;       /*!< message, including handshake headers   */
    size_t len;             /*!< length of p                            */
    unsigned char type;     /*!< type of the message: handshake or CCS  */
    mbedtls_ssl_flight_item *next;  /*!< next handshake message(s)              */
};
#endif /* MBEDTLS_SSL_PROTO_DTLS */

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
/**
 * \brief Given an SSL context and its associated configuration, write the TLS
 *        1.2 specific extensions of the ClientHello message.
 *
 * \param[in]   ssl     SSL context
 * \param[in]   buf     Base address of the buffer where to write the extensions
 * \param[in]   end     End address of the buffer where to write the extensions
 * \param       uses_ec Whether one proposed ciphersuite uses an elliptic curve
 *                      (<> 0) or not ( 0 ).
 * \param[out]  out_len Length of the data written into the buffer \p buf
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls12_write_client_hello_exts(mbedtls_ssl_context *ssl,
                                              unsigned char *buf,
                                              const unsigned char *end,
                                              int uses_ec,
                                              size_t *out_len);
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_2) && \
    defined(MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED)

/**
 * \brief Find the preferred hash for a given signature algorithm.
 *
 * \param[in]   ssl     SSL context
 * \param[in]   sig_alg A signature algorithm identifier as defined in the
 *                      TLS 1.2 SignatureAlgorithm enumeration.
 *
 * \return  The preferred hash algorithm for \p sig_alg. It is a hash algorithm
 *          identifier as defined in the TLS 1.2 HashAlgorithm enumeration.
 */
unsigned int mbedtls_ssl_tls12_get_preferred_hash_for_sig_alg(
    mbedtls_ssl_context *ssl,
    unsigned int sig_alg);

#endif /* MBEDTLS_SSL_PROTO_TLS1_2 &&
          MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED */

/**
 * \brief           Free referenced items in an SSL transform context and clear
 *                  memory
 *
 * \param transform SSL transform context
 */
void mbedtls_ssl_transform_free(mbedtls_ssl_transform *transform);

/**
 * \brief           Free referenced items in an SSL handshake context and clear
 *                  memory
 *
 * \param ssl       SSL context
 */
void mbedtls_ssl_handshake_free(mbedtls_ssl_context *ssl);

/* set inbound transform of ssl context */
void mbedtls_ssl_set_inbound_transform(mbedtls_ssl_context *ssl,
                                       mbedtls_ssl_transform *transform);

/* set outbound transform of ssl context */
void mbedtls_ssl_set_outbound_transform(mbedtls_ssl_context *ssl,
                                        mbedtls_ssl_transform *transform);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_handshake_client_step(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_handshake_server_step(mbedtls_ssl_context *ssl);
void mbedtls_ssl_handshake_wrapup(mbedtls_ssl_context *ssl);

#if defined(MBEDTLS_DEBUG_C)
/* Declared in "ssl_debug_helpers.h". We can't include this file from
 * "ssl_misc.h" because it includes "ssl_misc.h" because it needs some
 * type definitions. TODO: split the type definitions and the helper
 * functions into different headers.
 */
const char *mbedtls_ssl_states_str(mbedtls_ssl_states state);
#endif

static inline void mbedtls_ssl_handshake_set_state(mbedtls_ssl_context *ssl,
                                                   mbedtls_ssl_states state)
{
    MBEDTLS_SSL_DEBUG_MSG(3, ("handshake state: %d (%s) -> %d (%s)",
                              ssl->state, mbedtls_ssl_states_str(ssl->state),
                              (int) state, mbedtls_ssl_states_str(state)));
    ssl->state = (int) state;
}

static inline void mbedtls_ssl_handshake_increment_state(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_handshake_set_state(ssl, ssl->state + 1);
}

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_send_fatal_handshake_failure(mbedtls_ssl_context *ssl);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_reset_checksum(mbedtls_ssl_context *ssl);

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_derive_keys(mbedtls_ssl_context *ssl);
#endif /* MBEDTLS_SSL_PROTO_TLS1_2  */

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_handle_message_type(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_prepare_handshake_record(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_update_handshake_status(mbedtls_ssl_context *ssl);

/**
 * \brief       Update record layer
 *
 *              This function roughly separates the implementation
 *              of the logic of (D)TLS from the implementation
 *              of the secure transport.
 *
 * \param  ssl              The SSL context to use.
 * \param  update_hs_digest This indicates if the handshake digest
 *                          should be automatically updated in case
 *                          a handshake message is found.
 *
 * \return      0 or non-zero error code.
 *
 * \note        A clarification on what is called 'record layer' here
 *              is in order, as many sensible definitions are possible:
 *
 *              The record layer takes as input an untrusted underlying
 *              transport (stream or datagram) and transforms it into
 *              a serially multiplexed, secure transport, which
 *              conceptually provides the following:
 *
 *              (1) Three datagram based, content-agnostic transports
 *                  for handshake, alert and CCS messages.
 *              (2) One stream- or datagram-based transport
 *                  for application data.
 *              (3) Functionality for changing the underlying transform
 *                  securing the contents.
 *
 *              The interface to this functionality is given as follows:
 *
 *              a Updating
 *                [Currently implemented by mbedtls_ssl_read_record]
 *
 *                Check if and on which of the four 'ports' data is pending:
 *                Nothing, a controlling datagram of type (1), or application
 *                data (2). In any case data is present, internal buffers
 *                provide access to the data for the user to process it.
 *                Consumption of type (1) datagrams is done automatically
 *                on the next update, invalidating that the internal buffers
 *                for previous datagrams, while consumption of application
 *                data (2) is user-controlled.
 *
 *              b Reading of application data
 *                [Currently manual adaption of ssl->in_offt pointer]
 *
 *                As mentioned in the last paragraph, consumption of data
 *                is different from the automatic consumption of control
 *                datagrams (1) because application data is treated as a stream.
 *
 *              c Tracking availability of application data
 *                [Currently manually through decreasing ssl->in_msglen]
 *
 *                For efficiency and to retain datagram semantics for
 *                application data in case of DTLS, the record layer
 *                provides functionality for checking how much application
 *                data is still available in the internal buffer.
 *
 *              d Changing the transformation securing the communication.
 *
 *              Given an opaque implementation of the record layer in the
 *              above sense, it should be possible to implement the logic
 *              of (D)TLS on top of it without the need to know anything
 *              about the record layer's internals. This is done e.g.
 *              in all the handshake handling functions, and in the
 *              application data reading function mbedtls_ssl_read.
 *
 * \note        The above tries to give a conceptual picture of the
 *              record layer, but the current implementation deviates
 *              from it in some places. For example, our implementation of
 *              the update functionality through mbedtls_ssl_read_record
 *              discards datagrams depending on the current state, which
 *              wouldn't fall under the record layer's responsibility
 *              following the above definition.
 *
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_read_record(mbedtls_ssl_context *ssl,
                            unsigned update_hs_digest);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_fetch_input(mbedtls_ssl_context *ssl, size_t nb_want);

/*
 * Write handshake message header
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_start_handshake_msg(mbedtls_ssl_context *ssl, unsigned char hs_type,
                                    unsigned char **buf, size_t *buf_len);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_write_handshake_msg_ext(mbedtls_ssl_context *ssl,
                                        int update_checksum,
                                        int force_flush);
static inline int mbedtls_ssl_write_handshake_msg(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_write_handshake_msg_ext(ssl, 1 /* update checksum */, 1 /* force flush */);
}

/*
 * Write handshake message tail
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_finish_handshake_msg(mbedtls_ssl_context *ssl,
                                     size_t buf_len, size_t msg_len);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_write_record(mbedtls_ssl_context *ssl, int force_flush);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_flush_output(mbedtls_ssl_context *ssl);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_parse_certificate(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_write_certificate(mbedtls_ssl_context *ssl);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_parse_change_cipher_spec(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_write_change_cipher_spec(mbedtls_ssl_context *ssl);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_parse_finished(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_write_finished(mbedtls_ssl_context *ssl);

void mbedtls_ssl_optimize_checksum(mbedtls_ssl_context *ssl,
                                   const mbedtls_ssl_ciphersuite_t *ciphersuite_info);

/*
 * Update checksum of handshake messages.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_add_hs_msg_to_checksum(mbedtls_ssl_context *ssl,
                                       unsigned hs_type,
                                       unsigned char const *msg,
                                       size_t msg_len);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_add_hs_hdr_to_checksum(mbedtls_ssl_context *ssl,
                                       unsigned hs_type,
                                       size_t total_hs_len);

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
#if !defined(MBEDTLS_USE_PSA_CRYPTO)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_psk_derive_premaster(mbedtls_ssl_context *ssl,
                                     mbedtls_key_exchange_type_t key_ex);
#endif /* !MBEDTLS_USE_PSA_CRYPTO */
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED */

#if defined(MBEDTLS_SSL_HANDSHAKE_WITH_PSK_ENABLED)
#if defined(MBEDTLS_SSL_CLI_C) || defined(MBEDTLS_SSL_SRV_C)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_conf_has_static_psk(mbedtls_ssl_config const *conf);
#endif
#if defined(MBEDTLS_USE_PSA_CRYPTO)
/**
 * Get the first defined opaque PSK by order of precedence:
 * 1. handshake PSK set by \c mbedtls_ssl_set_hs_psk_opaque() in the PSK
 *    callback
 * 2. static PSK configured by \c mbedtls_ssl_conf_psk_opaque()
 * Return an opaque PSK
 */
static inline mbedtls_svc_key_id_t mbedtls_ssl_get_opaque_psk(
    const mbedtls_ssl_context *ssl)
{
    if (!mbedtls_svc_key_id_is_null(ssl->handshake->psk_opaque)) {
        return ssl->handshake->psk_opaque;
    }

    if (!mbedtls_svc_key_id_is_null(ssl->conf->psk_opaque)) {
        return ssl->conf->psk_opaque;
    }

    return MBEDTLS_SVC_KEY_ID_INIT;
}
#else
/**
 * Get the first defined PSK by order of precedence:
 * 1. handshake PSK set by \c mbedtls_ssl_set_hs_psk() in the PSK callback
 * 2. static PSK configured by \c mbedtls_ssl_conf_psk()
 * Return a code and update the pair (PSK, PSK length) passed to this function
 */
static inline int mbedtls_ssl_get_psk(const mbedtls_ssl_context *ssl,
                                      const unsigned char **psk, size_t *psk_len)
{
    if (ssl->handshake->psk != NULL && ssl->handshake->psk_len > 0) {
        *psk = ssl->handshake->psk;
        *psk_len = ssl->handshake->psk_len;
    } else if (ssl->conf->psk != NULL && ssl->conf->psk_len > 0) {
        *psk = ssl->conf->psk;
        *psk_len = ssl->conf->psk_len;
    } else {
        *psk = NULL;
        *psk_len = 0;
        return MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED;
    }

    return 0;
}
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#endif /* MBEDTLS_SSL_HANDSHAKE_WITH_PSK_ENABLED */

#if defined(MBEDTLS_PK_C)
unsigned char mbedtls_ssl_sig_from_pk(mbedtls_pk_context *pk);
unsigned char mbedtls_ssl_sig_from_pk_alg(mbedtls_pk_type_t type);
mbedtls_pk_type_t mbedtls_ssl_pk_alg_from_sig(unsigned char sig);
#endif

mbedtls_md_type_t mbedtls_ssl_md_alg_from_hash(unsigned char hash);
unsigned char mbedtls_ssl_hash_from_md_alg(int md);

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_set_calc_verify_md(mbedtls_ssl_context *ssl, int md);
#endif

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_check_curve_tls_id(const mbedtls_ssl_context *ssl, uint16_t tls_id);
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_check_curve(const mbedtls_ssl_context *ssl, mbedtls_ecp_group_id grp_id);
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

/**
 * \brief Return PSA EC info for the specified TLS ID.
 *
 * \param tls_id    The TLS ID to look for
 * \param type      If the TLD ID is supported, then proper \c psa_key_type_t
 *                  value is returned here. Can be NULL.
 * \param bits      If the TLD ID is supported, then proper bit size is returned
 *                  here. Can be NULL.
 * \return          PSA_SUCCESS if the TLS ID is supported,
 *                  PSA_ERROR_NOT_SUPPORTED otherwise
 *
 * \note            If either \c family or \c bits parameters are NULL, then
 *                  the corresponding value is not returned.
 *                  The function can be called with both parameters as NULL
 *                  simply to check if a specific TLS ID is supported.
 */
int mbedtls_ssl_get_psa_curve_info_from_tls_id(uint16_t tls_id,
                                               psa_key_type_t *type,
                                               size_t *bits);

/**
 * \brief Return \c mbedtls_ecp_group_id for the specified TLS ID.
 *
 * \param tls_id    The TLS ID to look for
 * \return          Proper \c mbedtls_ecp_group_id if the TLS ID is supported,
 *                  or MBEDTLS_ECP_DP_NONE otherwise
 */
mbedtls_ecp_group_id mbedtls_ssl_get_ecp_group_id_from_tls_id(uint16_t tls_id);

/**
 * \brief Return TLS ID for the specified \c mbedtls_ecp_group_id.
 *
 * \param grp_id    The \c mbedtls_ecp_group_id ID to look for
 * \return          Proper TLS ID if the \c mbedtls_ecp_group_id is supported,
 *                  or 0 otherwise
 */
uint16_t mbedtls_ssl_get_tls_id_from_ecp_group_id(mbedtls_ecp_group_id grp_id);

#if defined(MBEDTLS_DEBUG_C)
/**
 * \brief Return EC's name for the specified TLS ID.
 *
 * \param tls_id    The TLS ID to look for
 * \return          A pointer to a const string with the proper name. If TLS
 *                  ID is not supported, a NULL pointer is returned instead.
 */
const char *mbedtls_ssl_get_curve_name_from_tls_id(uint16_t tls_id);
#endif

#if defined(MBEDTLS_SSL_DTLS_SRTP)
static inline mbedtls_ssl_srtp_profile mbedtls_ssl_check_srtp_profile_value
    (const uint16_t srtp_profile_value)
{
    switch (srtp_profile_value) {
        case MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80:
        case MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_32:
        case MBEDTLS_TLS_SRTP_NULL_HMAC_SHA1_80:
        case MBEDTLS_TLS_SRTP_NULL_HMAC_SHA1_32:
            return srtp_profile_value;
        default: break;
    }
    return MBEDTLS_TLS_SRTP_UNSET;
}
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C)
static inline mbedtls_pk_context *mbedtls_ssl_own_key(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_key_cert *key_cert;

    if (ssl->handshake != NULL && ssl->handshake->key_cert != NULL) {
        key_cert = ssl->handshake->key_cert;
    } else {
        key_cert = ssl->conf->key_cert;
    }

    return key_cert == NULL ? NULL : key_cert->key;
}

static inline mbedtls_x509_crt *mbedtls_ssl_own_cert(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_key_cert *key_cert;

    if (ssl->handshake != NULL && ssl->handshake->key_cert != NULL) {
        key_cert = ssl->handshake->key_cert;
    } else {
        key_cert = ssl->conf->key_cert;
    }

    return key_cert == NULL ? NULL : key_cert->cert;
}

/*
 * Verify a certificate.
 *
 * [in/out] ssl: misc. things read
 *               ssl->session_negotiate->verify_result updated
 * [in] authmode: one of MBEDTLS_SSL_VERIFY_{NONE,OPTIONAL,REQUIRED}
 * [in] chain: the certificate chain to verify (ie the peer's chain)
 * [in] ciphersuite_info: For TLS 1.2, this session's ciphersuite;
 *                        for TLS 1.3, may be left NULL.
 * [in] rs_ctx: restart context if restartable ECC is in use;
 *              leave NULL for no restartable behaviour.
 *
 * Return:
 * - 0 if the handshake should continue. Depending on the
 *   authmode it means:
 *   - REQUIRED: the certificate was found to be valid, trusted & acceptable.
 *     ssl->session_negotiate->verify_result is 0.
 *   - OPTIONAL: the certificate may or may not be acceptable, but
 *     ssl->session_negotiate->verify_result was updated with the result.
 *   - NONE: the certificate wasn't even checked.
 * - MBEDTLS_ERR_X509_CERT_VERIFY_FAILED or MBEDTLS_ERR_SSL_BAD_CERTIFICATE if
 *   the certificate was found to be invalid/untrusted/unacceptable and the
 *   handshake should be aborted (can only happen with REQUIRED).
 * - another error code if another error happened (out-of-memory, etc.)
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_verify_certificate(mbedtls_ssl_context *ssl,
                                   int authmode,
                                   mbedtls_x509_crt *chain,
                                   const mbedtls_ssl_ciphersuite_t *ciphersuite_info,
                                   void *rs_ctx);

/*
 * Check usage of a certificate wrt usage extensions:
 * keyUsage and extendedKeyUsage.
 * (Note: nSCertType is deprecated and not standard, we don't check it.)
 *
 * Note: if tls_version is 1.3, ciphersuite is ignored and can be NULL.
 *
 * Note: recv_endpoint is the receiver's endpoint.
 *
 * Return 0 if everything is OK, -1 if not.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_check_cert_usage(const mbedtls_x509_crt *cert,
                                 const mbedtls_ssl_ciphersuite_t *ciphersuite,
                                 int recv_endpoint,
                                 mbedtls_ssl_protocol_version tls_version,
                                 uint32_t *flags);
#endif /* MBEDTLS_X509_CRT_PARSE_C */

void mbedtls_ssl_write_version(unsigned char version[2], int transport,
                               mbedtls_ssl_protocol_version tls_version);
uint16_t mbedtls_ssl_read_version(const unsigned char version[2],
                                  int transport);

static inline size_t mbedtls_ssl_in_hdr_len(const mbedtls_ssl_context *ssl)
{
#if !defined(MBEDTLS_SSL_PROTO_DTLS)
    ((void) ssl);
#endif

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        return 13;
    } else
#endif /* MBEDTLS_SSL_PROTO_DTLS */
    {
        return 5;
    }
}

static inline size_t mbedtls_ssl_out_hdr_len(const mbedtls_ssl_context *ssl)
{
    return (size_t) (ssl->out_iv - ssl->out_hdr);
}

static inline size_t mbedtls_ssl_hs_hdr_len(const mbedtls_ssl_context *ssl)
{
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        return 12;
    }
#else
    ((void) ssl);
#endif
    return 4;
}

#if defined(MBEDTLS_SSL_PROTO_DTLS)
void mbedtls_ssl_send_flight_completed(mbedtls_ssl_context *ssl);
void mbedtls_ssl_recv_flight_completed(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_resend(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_flight_transmit(mbedtls_ssl_context *ssl);
#endif

/* Visible for testing purposes only */
#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_dtls_replay_check(mbedtls_ssl_context const *ssl);
void mbedtls_ssl_dtls_replay_update(mbedtls_ssl_context *ssl);
#endif

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_session_copy(mbedtls_ssl_session *dst,
                             const mbedtls_ssl_session *src);

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
/* The hash buffer must have at least MBEDTLS_MD_MAX_SIZE bytes of length. */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_get_key_exchange_md_tls1_2(mbedtls_ssl_context *ssl,
                                           unsigned char *hash, size_t *hashlen,
                                           unsigned char *data, size_t data_len,
                                           mbedtls_md_type_t md_alg);
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

#ifdef __cplusplus
}
#endif

void mbedtls_ssl_transform_init(mbedtls_ssl_transform *transform);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_encrypt_buf(mbedtls_ssl_context *ssl,
                            mbedtls_ssl_transform *transform,
                            mbedtls_record *rec,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_decrypt_buf(mbedtls_ssl_context const *ssl,
                            mbedtls_ssl_transform *transform,
                            mbedtls_record *rec);

/* Length of the "epoch" field in the record header */
static inline size_t mbedtls_ssl_ep_len(const mbedtls_ssl_context *ssl)
{
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        return 2;
    }
#else
    ((void) ssl);
#endif
    return 0;
}

#if defined(MBEDTLS_SSL_PROTO_DTLS)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_resend_hello_request(mbedtls_ssl_context *ssl);
#endif /* MBEDTLS_SSL_PROTO_DTLS */

void mbedtls_ssl_set_timer(mbedtls_ssl_context *ssl, uint32_t millisecs);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_check_timer(mbedtls_ssl_context *ssl);

void mbedtls_ssl_reset_in_pointers(mbedtls_ssl_context *ssl);
void mbedtls_ssl_update_in_pointers(mbedtls_ssl_context *ssl);
void mbedtls_ssl_reset_out_pointers(mbedtls_ssl_context *ssl);
void mbedtls_ssl_update_out_pointers(mbedtls_ssl_context *ssl,
                                     mbedtls_ssl_transform *transform);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_session_reset_int(mbedtls_ssl_context *ssl, int partial);
void mbedtls_ssl_session_reset_msg_layer(mbedtls_ssl_context *ssl,
                                         int partial);

/*
 * Send pending alert
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_handle_pending_alert(mbedtls_ssl_context *ssl);

/*
 * Set pending fatal alert flag.
 */
void mbedtls_ssl_pend_fatal_alert(mbedtls_ssl_context *ssl,
                                  unsigned char alert_type,
                                  int alert_reason);

/* Alias of mbedtls_ssl_pend_fatal_alert */
#define MBEDTLS_SSL_PEND_FATAL_ALERT(type, user_return_value)         \
    mbedtls_ssl_pend_fatal_alert(ssl, type, user_return_value)

#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY)
void mbedtls_ssl_dtls_replay_reset(mbedtls_ssl_context *ssl);
#endif

void mbedtls_ssl_handshake_wrapup_free_hs_transform(mbedtls_ssl_context *ssl);

#if defined(MBEDTLS_SSL_RENEGOTIATION)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_start_renegotiation(mbedtls_ssl_context *ssl);
#endif /* MBEDTLS_SSL_RENEGOTIATION */

#if defined(MBEDTLS_SSL_PROTO_DTLS)
size_t mbedtls_ssl_get_current_mtu(const mbedtls_ssl_context *ssl);
void mbedtls_ssl_buffering_free(mbedtls_ssl_context *ssl);
void mbedtls_ssl_flight_free(mbedtls_ssl_flight_item *flight);
#endif /* MBEDTLS_SSL_PROTO_DTLS */

/**
 * ssl utils functions for checking configuration.
 */

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
static inline int mbedtls_ssl_conf_is_tls13_only(const mbedtls_ssl_config *conf)
{
    return conf->min_tls_version == MBEDTLS_SSL_VERSION_TLS1_3 &&
           conf->max_tls_version == MBEDTLS_SSL_VERSION_TLS1_3;
}

#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
static inline int mbedtls_ssl_conf_is_tls12_only(const mbedtls_ssl_config *conf)
{
    return conf->min_tls_version == MBEDTLS_SSL_VERSION_TLS1_2 &&
           conf->max_tls_version == MBEDTLS_SSL_VERSION_TLS1_2;
}

#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

static inline int mbedtls_ssl_conf_is_tls13_enabled(const mbedtls_ssl_config *conf)
{
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    return conf->min_tls_version <= MBEDTLS_SSL_VERSION_TLS1_3 &&
           conf->max_tls_version >= MBEDTLS_SSL_VERSION_TLS1_3;
#else
    ((void) conf);
    return 0;
#endif
}

static inline int mbedtls_ssl_conf_is_tls12_enabled(const mbedtls_ssl_config *conf)
{
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
    return conf->min_tls_version <= MBEDTLS_SSL_VERSION_TLS1_2 &&
           conf->max_tls_version >= MBEDTLS_SSL_VERSION_TLS1_2;
#else
    ((void) conf);
    return 0;
#endif
}

#if defined(MBEDTLS_SSL_PROTO_TLS1_2) && defined(MBEDTLS_SSL_PROTO_TLS1_3)
static inline int mbedtls_ssl_conf_is_hybrid_tls12_tls13(const mbedtls_ssl_config *conf)
{
    return conf->min_tls_version == MBEDTLS_SSL_VERSION_TLS1_2 &&
           conf->max_tls_version == MBEDTLS_SSL_VERSION_TLS1_3;
}
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 && MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)

/** \brief Initialize the PSA crypto subsystem if necessary.
 *
 * Call this function before doing any cryptography in a TLS 1.3 handshake.
 *
 * This is necessary in Mbed TLS 3.x for backward compatibility.
 * Up to Mbed TLS 3.5, in the default configuration, you could perform
 * a TLS connection with default parameters without having called
 * psa_crypto_init(), since the TLS layer only supported TLS 1.2 and
 * did not use PSA crypto. (TLS 1.2 only uses PSA crypto if
 * MBEDTLS_USE_PSA_CRYPTO is enabled, which is not the case in the default
 * configuration.) Starting with Mbed TLS 3.6.0, TLS 1.3 is enabled
 * by default, and the TLS 1.3 layer uses PSA crypto. This means that
 * applications that are not otherwise using PSA crypto and that worked
 * with Mbed TLS 3.5 started failing in TLS 3.6.0 if they connected to
 * a peer that supports TLS 1.3. See
 * https://github.com/Mbed-TLS/mbedtls/issues/9072
 */
int mbedtls_ssl_tls13_crypto_init(mbedtls_ssl_context *ssl);

extern const uint8_t mbedtls_ssl_tls13_hello_retry_request_magic[
    MBEDTLS_SERVER_HELLO_RANDOM_LEN];
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_process_finished_message(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_finished_message(mbedtls_ssl_context *ssl);
void mbedtls_ssl_tls13_handshake_wrapup(mbedtls_ssl_context *ssl);

/**
 * \brief Given an SSL context and its associated configuration, write the TLS
 *        1.3 specific extensions of the ClientHello message.
 *
 * \param[in]   ssl     SSL context
 * \param[in]   buf     Base address of the buffer where to write the extensions
 * \param[in]   end     End address of the buffer where to write the extensions
 * \param[out]  out_len Length of the data written into the buffer \p buf
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_client_hello_exts(mbedtls_ssl_context *ssl,
                                              unsigned char *buf,
                                              unsigned char *end,
                                              size_t *out_len);

/**
 * \brief           TLS 1.3 client side state machine entry
 *
 * \param ssl       SSL context
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_handshake_client_step(mbedtls_ssl_context *ssl);

/**
 * \brief           TLS 1.3 server side state machine entry
 *
 * \param ssl       SSL context
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_handshake_server_step(mbedtls_ssl_context *ssl);


/*
 * Helper functions around key exchange modes.
 */
static inline int mbedtls_ssl_conf_tls13_is_kex_mode_enabled(mbedtls_ssl_context *ssl,
                                                             int kex_mode_mask)
{
    return (ssl->conf->tls13_kex_modes & kex_mode_mask) != 0;
}

static inline int mbedtls_ssl_conf_tls13_is_psk_enabled(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_conf_tls13_is_kex_mode_enabled(ssl,
                                                      MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK);
}

static inline int mbedtls_ssl_conf_tls13_is_psk_ephemeral_enabled(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_conf_tls13_is_kex_mode_enabled(ssl,
                                                      MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL);
}

static inline int mbedtls_ssl_conf_tls13_is_ephemeral_enabled(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_conf_tls13_is_kex_mode_enabled(ssl,
                                                      MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL);
}

static inline int mbedtls_ssl_conf_tls13_is_some_ephemeral_enabled(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_conf_tls13_is_kex_mode_enabled(ssl,
                                                      MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ALL);
}

static inline int mbedtls_ssl_conf_tls13_is_some_psk_enabled(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_conf_tls13_is_kex_mode_enabled(ssl,
                                                      MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ALL);
}

#if defined(MBEDTLS_SSL_SRV_C) && \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
/**
 * Given a list of key exchange modes, check if at least one of them is
 * supported by peer.
 *
 * \param[in] ssl  SSL context
 * \param kex_modes_mask  Mask of the key exchange modes to check
 *
 * \return Non-zero if at least one of the key exchange modes is supported by
 *         the peer, otherwise \c 0.
 */
static inline int mbedtls_ssl_tls13_is_kex_mode_supported(mbedtls_ssl_context *ssl,
                                                          int kex_modes_mask)
{
    return (ssl->handshake->tls13_kex_modes & kex_modes_mask) != 0;
}

static inline int mbedtls_ssl_tls13_is_psk_supported(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_tls13_is_kex_mode_supported(ssl,
                                                   MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK);
}

static inline int mbedtls_ssl_tls13_is_psk_ephemeral_supported(
    mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_tls13_is_kex_mode_supported(ssl,
                                                   MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL);
}

static inline int mbedtls_ssl_tls13_is_ephemeral_supported(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_tls13_is_kex_mode_supported(ssl,
                                                   MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL);
}

static inline int mbedtls_ssl_tls13_is_some_ephemeral_supported(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_tls13_is_kex_mode_supported(ssl,
                                                   MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ALL);
}

static inline int mbedtls_ssl_tls13_is_some_psk_supported(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_tls13_is_kex_mode_supported(ssl,
                                                   MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ALL);
}
#endif /* MBEDTLS_SSL_SRV_C &&
          MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED */

/*
 * Helper functions for extensions checking.
 */

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_check_received_extension(
    mbedtls_ssl_context *ssl,
    int hs_msg_type,
    unsigned int received_extension_type,
    uint32_t hs_msg_allowed_extensions_mask);

static inline void mbedtls_ssl_tls13_set_hs_sent_ext_mask(
    mbedtls_ssl_context *ssl, unsigned int extension_type)
{
    ssl->handshake->sent_extensions |=
        mbedtls_ssl_get_extension_mask(extension_type);
}

/*
 * Helper functions to check the selected key exchange mode.
 */
static inline int mbedtls_ssl_tls13_key_exchange_mode_check(
    mbedtls_ssl_context *ssl, int kex_mask)
{
    return (ssl->handshake->key_exchange_mode & kex_mask) != 0;
}

static inline int mbedtls_ssl_tls13_key_exchange_mode_with_psk(
    mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_tls13_key_exchange_mode_check(ssl,
                                                     MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ALL);
}

static inline int mbedtls_ssl_tls13_key_exchange_mode_with_ephemeral(
    mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_tls13_key_exchange_mode_check(ssl,
                                                     MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ALL);
}

/*
 * Fetch TLS 1.3 handshake message header
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_fetch_handshake_msg(mbedtls_ssl_context *ssl,
                                          unsigned hs_type,
                                          unsigned char **buf,
                                          size_t *buf_len);

/**
 * \brief Detect if a list of extensions contains a supported_versions
 *        extension or not.
 *
 * \param[in] ssl  SSL context
 * \param[in] buf  Address of the first byte of the extensions vector.
 * \param[in] end  End of the buffer containing the list of extensions.
 * \param[out] supported_versions_data  If the extension is present, address of
 *                                      its first byte of data, NULL otherwise.
 * \param[out] supported_versions_data_end  If the extension is present, address
 *                                          of the first byte immediately
 *                                          following the extension data, NULL
 *                                          otherwise.
 * \return 0  if the list of extensions does not contain a supported_versions
 *            extension.
 * \return 1  if the list of extensions contains a supported_versions
 *            extension.
 * \return    A negative value if an error occurred while parsing the
 *            extensions.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_is_supported_versions_ext_present_in_exts(
    mbedtls_ssl_context *ssl,
    const unsigned char *buf, const unsigned char *end,
    const unsigned char **supported_versions_data,
    const unsigned char **supported_versions_data_end);

/*
 * Handler of TLS 1.3 server certificate message
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_process_certificate(mbedtls_ssl_context *ssl);

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
/*
 * Handler of TLS 1.3 write Certificate message
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_certificate(mbedtls_ssl_context *ssl);

/*
 * Handler of TLS 1.3 write Certificate Verify message
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_certificate_verify(mbedtls_ssl_context *ssl);

#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

/*
 * Generic handler of Certificate Verify
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_process_certificate_verify(mbedtls_ssl_context *ssl);

/*
 * Write of dummy-CCS's for middlebox compatibility
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_change_cipher_spec(mbedtls_ssl_context *ssl);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_reset_transcript_for_hrr(mbedtls_ssl_context *ssl);

#if defined(PSA_WANT_ALG_ECDH) || defined(PSA_WANT_ALG_FFDH)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_generate_and_write_xxdh_key_exchange(
    mbedtls_ssl_context *ssl,
    uint16_t named_group,
    unsigned char *buf,
    unsigned char *end,
    size_t *out_len);
#endif /* PSA_WANT_ALG_ECDH || PSA_WANT_ALG_FFDH */

#if defined(MBEDTLS_SSL_EARLY_DATA)
int mbedtls_ssl_tls13_write_early_data_ext(mbedtls_ssl_context *ssl,
                                           int in_new_session_ticket,
                                           unsigned char *buf,
                                           const unsigned char *end,
                                           size_t *out_len);

int mbedtls_ssl_tls13_check_early_data_len(mbedtls_ssl_context *ssl,
                                           size_t early_data_len);

typedef enum {
/*
 * The client has not sent the first ClientHello yet, the negotiation of early
 * data has not started yet.
 */
    MBEDTLS_SSL_EARLY_DATA_STATE_IDLE,

/*
 * In its ClientHello, the client has not included an early data indication
 * extension.
 */
    MBEDTLS_SSL_EARLY_DATA_STATE_NO_IND_SENT,

/*
 * The client has sent an early data indication extension in its first
 * ClientHello, it has not received the response (ServerHello or
 * HelloRetryRequest) from the server yet. The transform to protect early data
 * is not set either as for middlebox compatibility a dummy CCS may have to be
 * sent in clear. Early data cannot be sent to the server yet.
 */
    MBEDTLS_SSL_EARLY_DATA_STATE_IND_SENT,

/*
 * The client has sent an early data indication extension in its first
 * ClientHello, it has not received the response (ServerHello or
 * HelloRetryRequest) from the server yet. The transform to protect early data
 * has been set and early data can be written now.
 */
    MBEDTLS_SSL_EARLY_DATA_STATE_CAN_WRITE,

/*
 * The client has indicated the use of early data and the server has accepted
 * it.
 */
    MBEDTLS_SSL_EARLY_DATA_STATE_ACCEPTED,

/*
 * The client has indicated the use of early data but the server has rejected
 * it.
 */
    MBEDTLS_SSL_EARLY_DATA_STATE_REJECTED,

/*
 * The client has sent an early data indication extension in its first
 * ClientHello, the server has accepted them and the client has received the
 * server Finished message. It cannot send early data to the server anymore.
 */
    MBEDTLS_SSL_EARLY_DATA_STATE_SERVER_FINISHED_RECEIVED,

} mbedtls_ssl_early_data_state;
#endif /* MBEDTLS_SSL_EARLY_DATA */

#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED)
/*
 * Write Signature Algorithm extension
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_write_sig_alg_ext(mbedtls_ssl_context *ssl, unsigned char *buf,
                                  const unsigned char *end, size_t *out_len);
/*
 * Parse TLS Signature Algorithm extension
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_parse_sig_alg_ext(mbedtls_ssl_context *ssl,
                                  const unsigned char *buf,
                                  const unsigned char *end);
#endif /* MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED */

/* Get handshake transcript */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_get_handshake_transcript(mbedtls_ssl_context *ssl,
                                         const mbedtls_md_type_t md,
                                         unsigned char *dst,
                                         size_t dst_len,
                                         size_t *olen);

/*
 * Return supported groups.
 *
 * In future, invocations can be changed to ssl->conf->group_list
 * when mbedtls_ssl_conf_curves() is deleted.
 *
 * ssl->handshake->group_list is either a translation of curve_list to IANA TLS group
 * identifiers when mbedtls_ssl_conf_curves() has been used, or a pointer to
 * ssl->conf->group_list when mbedtls_ssl_conf_groups() has been more recently invoked.
 *
 */
static inline const void *mbedtls_ssl_get_groups(const mbedtls_ssl_context *ssl)
{
    #if defined(MBEDTLS_DEPRECATED_REMOVED) || !defined(MBEDTLS_ECP_C)
    return ssl->conf->group_list;
    #else
    if ((ssl->handshake != NULL) && (ssl->handshake->group_list != NULL)) {
        return ssl->handshake->group_list;
    } else {
        return ssl->conf->group_list;
    }
    #endif
}

/*
 * Helper functions for NamedGroup.
 */
static inline int mbedtls_ssl_tls12_named_group_is_ecdhe(uint16_t named_group)
{
    /*
     * RFC 8422 section 5.1.1
     */
    return named_group == MBEDTLS_SSL_IANA_TLS_GROUP_X25519    ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_BP256R1   ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_BP384R1   ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_BP512R1   ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_X448      ||
           /* Below deprecated curves should be removed with notice to users */
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP192K1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP192R1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP224K1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP224R1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP256K1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP384R1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP521R1;
}

static inline int mbedtls_ssl_tls13_named_group_is_ecdhe(uint16_t named_group)
{
    return named_group == MBEDTLS_SSL_IANA_TLS_GROUP_X25519    ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP384R1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_SECP521R1 ||
           named_group == MBEDTLS_SSL_IANA_TLS_GROUP_X448;
}

static inline int mbedtls_ssl_tls13_named_group_is_ffdh(uint16_t named_group)
{
    return named_group >= MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE2048 &&
           named_group <= MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE8192;
}

static inline int mbedtls_ssl_named_group_is_offered(
    const mbedtls_ssl_context *ssl, uint16_t named_group)
{
    const uint16_t *group_list = mbedtls_ssl_get_groups(ssl);

    if (group_list == NULL) {
        return 0;
    }

    for (; *group_list != 0; group_list++) {
        if (*group_list == named_group) {
            return 1;
        }
    }

    return 0;
}

static inline int mbedtls_ssl_named_group_is_supported(uint16_t named_group)
{
#if defined(PSA_WANT_ALG_ECDH)
    if (mbedtls_ssl_tls13_named_group_is_ecdhe(named_group)) {
        if (mbedtls_ssl_get_ecp_group_id_from_tls_id(named_group) !=
            MBEDTLS_ECP_DP_NONE) {
            return 1;
        }
    }
#endif
#if defined(PSA_WANT_ALG_FFDH)
    if (mbedtls_ssl_tls13_named_group_is_ffdh(named_group)) {
        return 1;
    }
#endif
#if !defined(PSA_WANT_ALG_ECDH) && !defined(PSA_WANT_ALG_FFDH)
    (void) named_group;
#endif
    return 0;
}

/*
 * Return supported signature algorithms.
 *
 * In future, invocations can be changed to ssl->conf->sig_algs when
 * mbedtls_ssl_conf_sig_hashes() is deleted.
 *
 * ssl->handshake->sig_algs is either a translation of sig_hashes to IANA TLS
 * signature algorithm identifiers when mbedtls_ssl_conf_sig_hashes() has been
 * used, or a pointer to ssl->conf->sig_algs when mbedtls_ssl_conf_sig_algs() has
 * been more recently invoked.
 *
 */
static inline const void *mbedtls_ssl_get_sig_algs(
    const mbedtls_ssl_context *ssl)
{
#if defined(MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED)

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
    if (ssl->handshake != NULL &&
        ssl->handshake->sig_algs_heap_allocated == 1 &&
        ssl->handshake->sig_algs != NULL) {
        return ssl->handshake->sig_algs;
    }
#endif
    return ssl->conf->sig_algs;

#else /* MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED */

    ((void) ssl);
    return NULL;
#endif /* MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED */
}

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
static inline int mbedtls_ssl_sig_alg_is_received(const mbedtls_ssl_context *ssl,
                                                  uint16_t own_sig_alg)
{
    const uint16_t *sig_alg = ssl->handshake->received_sig_algs;
    if (sig_alg == NULL) {
        return 0;
    }

    for (; *sig_alg != MBEDTLS_TLS_SIG_NONE; sig_alg++) {
        if (*sig_alg == own_sig_alg) {
            return 1;
        }
    }
    return 0;
}

static inline int mbedtls_ssl_tls13_sig_alg_for_cert_verify_is_supported(
    const uint16_t sig_alg)
{
    switch (sig_alg) {
#if defined(MBEDTLS_PK_CAN_ECDSA_SOME)
#if defined(PSA_WANT_ALG_SHA_256) && defined(PSA_WANT_ECC_SECP_R1_256)
        case MBEDTLS_TLS1_3_SIG_ECDSA_SECP256R1_SHA256:
            break;
#endif /* PSA_WANT_ALG_SHA_256 && MBEDTLS_ECP_DP_SECP256R1_ENABLED */
#if defined(PSA_WANT_ALG_SHA_384) && defined(PSA_WANT_ECC_SECP_R1_384)
        case MBEDTLS_TLS1_3_SIG_ECDSA_SECP384R1_SHA384:
            break;
#endif /* PSA_WANT_ALG_SHA_384 && MBEDTLS_ECP_DP_SECP384R1_ENABLED */
#if defined(PSA_WANT_ALG_SHA_512) && defined(PSA_WANT_ECC_SECP_R1_521)
        case MBEDTLS_TLS1_3_SIG_ECDSA_SECP521R1_SHA512:
            break;
#endif /* PSA_WANT_ALG_SHA_512 && MBEDTLS_ECP_DP_SECP521R1_ENABLED */
#endif /* MBEDTLS_PK_CAN_ECDSA_SOME */

#if defined(MBEDTLS_PKCS1_V21)
#if defined(PSA_WANT_ALG_SHA_256)
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA256:
            break;
#endif /* PSA_WANT_ALG_SHA_256  */
#if defined(PSA_WANT_ALG_SHA_384)
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA384:
            break;
#endif /* PSA_WANT_ALG_SHA_384 */
#if defined(PSA_WANT_ALG_SHA_512)
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA512:
            break;
#endif /* PSA_WANT_ALG_SHA_512 */
#endif /* MBEDTLS_PKCS1_V21 */
        default:
            return 0;
    }
    return 1;

}

static inline int mbedtls_ssl_tls13_sig_alg_is_supported(
    const uint16_t sig_alg)
{
    switch (sig_alg) {
#if defined(MBEDTLS_PKCS1_V15)
#if defined(MBEDTLS_MD_CAN_SHA256)
        case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA256:
            break;
#endif /* MBEDTLS_MD_CAN_SHA256 */
#if defined(MBEDTLS_MD_CAN_SHA384)
        case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA384:
            break;
#endif /* MBEDTLS_MD_CAN_SHA384 */
#if defined(MBEDTLS_MD_CAN_SHA512)
        case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA512:
            break;
#endif /* MBEDTLS_MD_CAN_SHA512 */
#endif /* MBEDTLS_PKCS1_V15 */
        default:
            return mbedtls_ssl_tls13_sig_alg_for_cert_verify_is_supported(
                sig_alg);
    }
    return 1;
}

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_check_sig_alg_cert_key_match(uint16_t sig_alg,
                                                   mbedtls_pk_context *key);
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED */

#if defined(MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED)
static inline int mbedtls_ssl_sig_alg_is_offered(const mbedtls_ssl_context *ssl,
                                                 uint16_t proposed_sig_alg)
{
    const uint16_t *sig_alg = mbedtls_ssl_get_sig_algs(ssl);
    if (sig_alg == NULL) {
        return 0;
    }

    for (; *sig_alg != MBEDTLS_TLS_SIG_NONE; sig_alg++) {
        if (*sig_alg == proposed_sig_alg) {
            return 1;
        }
    }
    return 0;
}

static inline int mbedtls_ssl_get_pk_type_and_md_alg_from_sig_alg(
    uint16_t sig_alg, mbedtls_pk_type_t *pk_type, mbedtls_md_type_t *md_alg)
{
    *pk_type = mbedtls_ssl_pk_alg_from_sig(sig_alg & 0xff);
    *md_alg = mbedtls_ssl_md_alg_from_hash((sig_alg >> 8) & 0xff);

    if (*pk_type != MBEDTLS_PK_NONE && *md_alg != MBEDTLS_MD_NONE) {
        return 0;
    }

    switch (sig_alg) {
#if defined(MBEDTLS_PKCS1_V21)
#if defined(MBEDTLS_MD_CAN_SHA256)
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA256:
            *md_alg = MBEDTLS_MD_SHA256;
            *pk_type = MBEDTLS_PK_RSASSA_PSS;
            break;
#endif /* MBEDTLS_MD_CAN_SHA256  */
#if defined(MBEDTLS_MD_CAN_SHA384)
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA384:
            *md_alg = MBEDTLS_MD_SHA384;
            *pk_type = MBEDTLS_PK_RSASSA_PSS;
            break;
#endif /* MBEDTLS_MD_CAN_SHA384 */
#if defined(MBEDTLS_MD_CAN_SHA512)
        case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA512:
            *md_alg = MBEDTLS_MD_SHA512;
            *pk_type = MBEDTLS_PK_RSASSA_PSS;
            break;
#endif /* MBEDTLS_MD_CAN_SHA512 */
#endif /* MBEDTLS_PKCS1_V21 */
        default:
            return MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
    }
    return 0;
}

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
static inline int mbedtls_ssl_tls12_sig_alg_is_supported(
    const uint16_t sig_alg)
{
    /* High byte is hash */
    unsigned char hash = MBEDTLS_BYTE_1(sig_alg);
    unsigned char sig = MBEDTLS_BYTE_0(sig_alg);

    switch (hash) {
#if defined(MBEDTLS_MD_CAN_MD5)
        case MBEDTLS_SSL_HASH_MD5:
            break;
#endif

#if defined(MBEDTLS_MD_CAN_SHA1)
        case MBEDTLS_SSL_HASH_SHA1:
            break;
#endif

#if defined(MBEDTLS_MD_CAN_SHA224)
        case MBEDTLS_SSL_HASH_SHA224:
            break;
#endif

#if defined(MBEDTLS_MD_CAN_SHA256)
        case MBEDTLS_SSL_HASH_SHA256:
            break;
#endif

#if defined(MBEDTLS_MD_CAN_SHA384)
        case MBEDTLS_SSL_HASH_SHA384:
            break;
#endif

#if defined(MBEDTLS_MD_CAN_SHA512)
        case MBEDTLS_SSL_HASH_SHA512:
            break;
#endif

        default:
            return 0;
    }

    switch (sig) {
#if defined(MBEDTLS_RSA_C)
        case MBEDTLS_SSL_SIG_RSA:
            break;
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED)
        case MBEDTLS_SSL_SIG_ECDSA:
            break;
#endif

        default:
            return 0;
    }

    return 1;
}
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

static inline int mbedtls_ssl_sig_alg_is_supported(
    const mbedtls_ssl_context *ssl,
    const uint16_t sig_alg)
{

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
    if (ssl->tls_version == MBEDTLS_SSL_VERSION_TLS1_2) {
        return mbedtls_ssl_tls12_sig_alg_is_supported(sig_alg);
    }
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
    if (ssl->tls_version == MBEDTLS_SSL_VERSION_TLS1_3) {
        return mbedtls_ssl_tls13_sig_alg_is_supported(sig_alg);
    }
#endif
    ((void) ssl);
    ((void) sig_alg);
    return 0;
}
#endif /* MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED */

#if defined(MBEDTLS_USE_PSA_CRYPTO) || defined(MBEDTLS_SSL_PROTO_TLS1_3)
/* Corresponding PSA algorithm for MBEDTLS_CIPHER_NULL.
 * Same value is used for PSA_ALG_CATEGORY_CIPHER, hence it is
 * guaranteed to not be a valid PSA algorithm identifier.
 */
#define MBEDTLS_SSL_NULL_CIPHER 0x04000000

/**
 * \brief       Translate mbedtls cipher type/taglen pair to psa:
 *              algorithm, key type and key size.
 *
 * \param  mbedtls_cipher_type [in] given mbedtls cipher type
 * \param  taglen              [in] given tag length
 *                                  0 - default tag length
 * \param  alg                 [out] corresponding PSA alg
 *                                   There is no corresponding PSA
 *                                   alg for MBEDTLS_CIPHER_NULL, so
 *                                   in this case MBEDTLS_SSL_NULL_CIPHER
 *                                   is returned via this parameter
 * \param  key_type            [out] corresponding PSA key type
 * \param  key_size            [out] corresponding PSA key size
 *
 * \return                     PSA_SUCCESS on success or PSA_ERROR_NOT_SUPPORTED if
 *                             conversion is not supported.
 */
psa_status_t mbedtls_ssl_cipher_to_psa(mbedtls_cipher_type_t mbedtls_cipher_type,
                                       size_t taglen,
                                       psa_algorithm_t *alg,
                                       psa_key_type_t *key_type,
                                       size_t *key_size);

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
/**
 * \brief       Convert given PSA status to mbedtls error code.
 *
 * \param  status      [in] given PSA status
 *
 * \return             corresponding mbedtls error code
 */
static inline MBEDTLS_DEPRECATED int psa_ssl_status_to_mbedtls(psa_status_t status)
{
    switch (status) {
        case PSA_SUCCESS:
            return 0;
        case PSA_ERROR_INSUFFICIENT_MEMORY:
            return MBEDTLS_ERR_SSL_ALLOC_FAILED;
        case PSA_ERROR_NOT_SUPPORTED:
            return MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
        case PSA_ERROR_INVALID_SIGNATURE:
            return MBEDTLS_ERR_SSL_INVALID_MAC;
        case PSA_ERROR_INVALID_ARGUMENT:
            return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
        case PSA_ERROR_BAD_STATE:
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        case PSA_ERROR_BUFFER_TOO_SMALL:
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        default:
            return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
    }
}
#endif /* !MBEDTLS_DEPRECATED_REMOVED */
#endif /* MBEDTLS_USE_PSA_CRYPTO || MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) && \
    defined(MBEDTLS_USE_PSA_CRYPTO)

typedef enum {
    MBEDTLS_ECJPAKE_ROUND_ONE,
    MBEDTLS_ECJPAKE_ROUND_TWO
} mbedtls_ecjpake_rounds_t;

/**
 * \brief       Parse the provided input buffer for getting the first round
 *              of key exchange. This code is common between server and client
 *
 * \param  pake_ctx [in] the PAKE's operation/context structure
 * \param  buf      [in] input buffer to parse
 * \param  len      [in] length of the input buffer
 * \param  round    [in] either MBEDTLS_ECJPAKE_ROUND_ONE or
 *                       MBEDTLS_ECJPAKE_ROUND_TWO
 *
 * \return               0 on success or a negative error code in case of failure
 */
int mbedtls_psa_ecjpake_read_round(
    psa_pake_operation_t *pake_ctx,
    const unsigned char *buf,
    size_t len, mbedtls_ecjpake_rounds_t round);

/**
 * \brief       Write the first round of key exchange into the provided output
 *              buffer. This code is common between server and client
 *
 * \param  pake_ctx [in] the PAKE's operation/context structure
 * \param  buf      [out] the output buffer in which data will be written to
 * \param  len      [in] length of the output buffer
 * \param  olen     [out] the length of the data really written on the buffer
 * \param  round    [in] either MBEDTLS_ECJPAKE_ROUND_ONE or
 *                       MBEDTLS_ECJPAKE_ROUND_TWO
 *
 * \return               0 on success or a negative error code in case of failure
 */
int mbedtls_psa_ecjpake_write_round(
    psa_pake_operation_t *pake_ctx,
    unsigned char *buf,
    size_t len, size_t *olen,
    mbedtls_ecjpake_rounds_t round);

#endif //MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED && MBEDTLS_USE_PSA_CRYPTO

/**
 * \brief       TLS record protection modes
 */
typedef enum {
    MBEDTLS_SSL_MODE_STREAM = 0,
    MBEDTLS_SSL_MODE_CBC,
    MBEDTLS_SSL_MODE_CBC_ETM,
    MBEDTLS_SSL_MODE_AEAD
} mbedtls_ssl_mode_t;

mbedtls_ssl_mode_t mbedtls_ssl_get_mode_from_transform(
    const mbedtls_ssl_transform *transform);

#if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM)
mbedtls_ssl_mode_t mbedtls_ssl_get_mode_from_ciphersuite(
    int encrypt_then_mac,
    const mbedtls_ssl_ciphersuite_t *suite);
#else
mbedtls_ssl_mode_t mbedtls_ssl_get_mode_from_ciphersuite(
    const mbedtls_ssl_ciphersuite_t *suite);
#endif /* MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM */

#if defined(PSA_WANT_ALG_ECDH) || defined(PSA_WANT_ALG_FFDH)

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_read_public_xxdhe_share(mbedtls_ssl_context *ssl,
                                              const unsigned char *buf,
                                              size_t buf_len);

#endif /* PSA_WANT_ALG_ECDH || PSA_WANT_ALG_FFDH */

static inline int mbedtls_ssl_tls13_cipher_suite_is_offered(
    mbedtls_ssl_context *ssl, int cipher_suite)
{
    const int *ciphersuite_list = ssl->conf->ciphersuite_list;

    /* Check whether we have offered this ciphersuite */
    for (size_t i = 0; ciphersuite_list[i] != 0; i++) {
        if (ciphersuite_list[i] == cipher_suite) {
            return 1;
        }
    }
    return 0;
}

/**
 * \brief Validate cipher suite against config in SSL context.
 *
 * \param ssl              SSL context
 * \param suite_info       Cipher suite to validate
 * \param min_tls_version  Minimal TLS version to accept a cipher suite
 * \param max_tls_version  Maximal TLS version to accept a cipher suite
 *
 * \return 0 if valid, negative value otherwise.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_validate_ciphersuite(
    const mbedtls_ssl_context *ssl,
    const mbedtls_ssl_ciphersuite_t *suite_info,
    mbedtls_ssl_protocol_version min_tls_version,
    mbedtls_ssl_protocol_version max_tls_version);

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_parse_server_name_ext(mbedtls_ssl_context *ssl,
                                      const unsigned char *buf,
                                      const unsigned char *end);
#endif /* MBEDTLS_SSL_SERVER_NAME_INDICATION */

#if defined(MBEDTLS_SSL_RECORD_SIZE_LIMIT)
#define MBEDTLS_SSL_RECORD_SIZE_LIMIT_EXTENSION_DATA_LENGTH (2)
#define MBEDTLS_SSL_RECORD_SIZE_LIMIT_MIN (64)      /* As defined in RFC 8449 */

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_parse_record_size_limit_ext(mbedtls_ssl_context *ssl,
                                                  const unsigned char *buf,
                                                  const unsigned char *end);

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_record_size_limit_ext(mbedtls_ssl_context *ssl,
                                                  unsigned char *buf,
                                                  const unsigned char *end,
                                                  size_t *out_len);
#endif /* MBEDTLS_SSL_RECORD_SIZE_LIMIT */

#if defined(MBEDTLS_SSL_ALPN)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_parse_alpn_ext(mbedtls_ssl_context *ssl,
                               const unsigned char *buf,
                               const unsigned char *end);


MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_write_alpn_ext(mbedtls_ssl_context *ssl,
                               unsigned char *buf,
                               unsigned char *end,
                               size_t *out_len);
#endif /* MBEDTLS_SSL_ALPN */

#if defined(MBEDTLS_TEST_HOOKS)
int mbedtls_ssl_check_dtls_clihlo_cookie(
    mbedtls_ssl_context *ssl,
    const unsigned char *cli_id, size_t cli_id_len,
    const unsigned char *in, size_t in_len,
    unsigned char *obuf, size_t buf_len, size_t *olen);
#endif

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
/**
 * \brief Given an SSL context and its associated configuration, write the TLS
 *        1.3 specific Pre-Shared key extension.
 *
 * \param[in]   ssl     SSL context
 * \param[in]   buf     Base address of the buffer where to write the extension
 * \param[in]   end     End address of the buffer where to write the extension
 * \param[out]  out_len Length in bytes of the Pre-Shared key extension: data
 *                      written into the buffer \p buf by this function plus
 *                      the length of the binders to be written.
 * \param[out]  binders_len Length of the binders to be written at the end of
 *                          the extension.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_identities_of_pre_shared_key_ext(
    mbedtls_ssl_context *ssl,
    unsigned char *buf, unsigned char *end,
    size_t *out_len, size_t *binders_len);

/**
 * \brief Given an SSL context and its associated configuration, write the TLS
 *        1.3 specific Pre-Shared key extension binders at the end of the
 *        ClientHello.
 *
 * \param[in]   ssl     SSL context
 * \param[in]   buf     Base address of the buffer where to write the binders
 * \param[in]   end     End address of the buffer where to write the binders
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_write_binders_of_pre_shared_key_ext(
    mbedtls_ssl_context *ssl,
    unsigned char *buf, unsigned char *end);
#endif /* MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED */

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
/** Get the host name from the SSL context.
 *
 * \param[in]   ssl     SSL context
 *
 * \return The \p hostname pointer from the SSL context.
 *         \c NULL if mbedtls_ssl_set_hostname() has never been called on
 *         \p ssl or if it was last called with \p NULL.
 */
const char *mbedtls_ssl_get_hostname_pointer(const mbedtls_ssl_context *ssl);
#endif /* MBEDTLS_SSL_SERVER_NAME_INDICATION */

#if defined(MBEDTLS_SSL_PROTO_TLS1_3) && \
    defined(MBEDTLS_SSL_SESSION_TICKETS) && \
    defined(MBEDTLS_SSL_SERVER_NAME_INDICATION) && \
    defined(MBEDTLS_SSL_CLI_C)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_session_set_hostname(mbedtls_ssl_session *session,
                                     const char *hostname);
#endif

#if defined(MBEDTLS_SSL_SRV_C) && defined(MBEDTLS_SSL_EARLY_DATA) && \
    defined(MBEDTLS_SSL_ALPN)
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_session_set_ticket_alpn(mbedtls_ssl_session *session,
                                        const char *alpn);
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_3) && defined(MBEDTLS_SSL_SESSION_TICKETS)

#define MBEDTLS_SSL_TLS1_3_MAX_ALLOWED_TICKET_LIFETIME (604800)

static inline unsigned int mbedtls_ssl_tls13_session_get_ticket_flags(
    mbedtls_ssl_session *session, unsigned int flags)
{
    return session->ticket_flags &
           (flags & MBEDTLS_SSL_TLS1_3_TICKET_FLAGS_MASK);
}

/**
 * Check if at least one of the given flags is set in
 * the session ticket. See the definition of
 * `MBEDTLS_SSL_TLS1_3_TICKET_FLAGS_MASK` to get all
 * permitted flags.
 */
static inline int mbedtls_ssl_tls13_session_ticket_has_flags(
    mbedtls_ssl_session *session, unsigned int flags)
{
    return mbedtls_ssl_tls13_session_get_ticket_flags(session, flags) != 0;
}

static inline int mbedtls_ssl_tls13_session_ticket_allow_psk(
    mbedtls_ssl_session *session)
{
    return mbedtls_ssl_tls13_session_ticket_has_flags(
        session, MBEDTLS_SSL_TLS1_3_TICKET_ALLOW_PSK_RESUMPTION);
}

static inline int mbedtls_ssl_tls13_session_ticket_allow_psk_ephemeral(
    mbedtls_ssl_session *session)
{
    return mbedtls_ssl_tls13_session_ticket_has_flags(
        session, MBEDTLS_SSL_TLS1_3_TICKET_ALLOW_PSK_EPHEMERAL_RESUMPTION);
}

static inline unsigned int mbedtls_ssl_tls13_session_ticket_allow_early_data(
    mbedtls_ssl_session *session)
{
    return mbedtls_ssl_tls13_session_ticket_has_flags(
        session, MBEDTLS_SSL_TLS1_3_TICKET_ALLOW_EARLY_DATA);
}

static inline void mbedtls_ssl_tls13_session_set_ticket_flags(
    mbedtls_ssl_session *session, unsigned int flags)
{
    session->ticket_flags |= (flags & MBEDTLS_SSL_TLS1_3_TICKET_FLAGS_MASK);
}

static inline void mbedtls_ssl_tls13_session_clear_ticket_flags(
    mbedtls_ssl_session *session, unsigned int flags)
{
    session->ticket_flags &= ~(flags & MBEDTLS_SSL_TLS1_3_TICKET_FLAGS_MASK);
}

#endif /* MBEDTLS_SSL_PROTO_TLS1_3 && MBEDTLS_SSL_SESSION_TICKETS */

#if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_CLI_C)
#define MBEDTLS_SSL_SESSION_TICKETS_TLS1_2_BIT 0
#define MBEDTLS_SSL_SESSION_TICKETS_TLS1_3_BIT 1

#define MBEDTLS_SSL_SESSION_TICKETS_TLS1_2_MASK \
    (1 << MBEDTLS_SSL_SESSION_TICKETS_TLS1_2_BIT)
#define MBEDTLS_SSL_SESSION_TICKETS_TLS1_3_MASK \
    (1 << MBEDTLS_SSL_SESSION_TICKETS_TLS1_3_BIT)

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
static inline int mbedtls_ssl_conf_get_session_tickets(
    const mbedtls_ssl_config *conf)
{
    return conf->session_tickets & MBEDTLS_SSL_SESSION_TICKETS_TLS1_2_MASK ?
           MBEDTLS_SSL_SESSION_TICKETS_ENABLED :
           MBEDTLS_SSL_SESSION_TICKETS_DISABLED;
}
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
static inline int mbedtls_ssl_conf_is_signal_new_session_tickets_enabled(
    const mbedtls_ssl_config *conf)
{
    return conf->session_tickets & MBEDTLS_SSL_SESSION_TICKETS_TLS1_3_MASK ?
           MBEDTLS_SSL_TLS1_3_SIGNAL_NEW_SESSION_TICKETS_ENABLED :
           MBEDTLS_SSL_TLS1_3_SIGNAL_NEW_SESSION_TICKETS_DISABLED;
}
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */
#endif /* MBEDTLS_SSL_SESSION_TICKETS && MBEDTLS_SSL_CLI_C */

#if defined(MBEDTLS_SSL_CLI_C) && defined(MBEDTLS_SSL_PROTO_TLS1_3)
int mbedtls_ssl_tls13_finalize_client_hello(mbedtls_ssl_context *ssl);
#endif

#if defined(MBEDTLS_TEST_HOOKS) && defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)

/** Compute the HMAC of variable-length data with constant flow.
 *
 * This function computes the HMAC of the concatenation of \p add_data and \p
 * data, and does with a code flow and memory access pattern that does not
 * depend on \p data_len_secret, but only on \p min_data_len and \p
 * max_data_len. In particular, this function always reads exactly \p
 * max_data_len bytes from \p data.
 *
 * \param ctx               The HMAC context. It must have keys configured
 *                          with mbedtls_md_hmac_starts() and use one of the
 *                          following hashes: SHA-384, SHA-256, SHA-1 or MD-5.
 *                          It is reset using mbedtls_md_hmac_reset() after
 *                          the computation is complete to prepare for the
 *                          next computation.
 * \param add_data          The first part of the message whose HMAC is being
 *                          calculated. This must point to a readable buffer
 *                          of \p add_data_len bytes.
 * \param add_data_len      The length of \p add_data in bytes.
 * \param data              The buffer containing the second part of the
 *                          message. This must point to a readable buffer
 *                          of \p max_data_len bytes.
 * \param data_len_secret   The length of the data to process in \p data.
 *                          This must be no less than \p min_data_len and no
 *                          greater than \p max_data_len.
 * \param min_data_len      The minimal length of the second part of the
 *                          message, read from \p data.
 * \param max_data_len      The maximal length of the second part of the
 *                          message, read from \p data.
 * \param output            The HMAC will be written here. This must point to
 *                          a writable buffer of sufficient size to hold the
 *                          HMAC value.
 *
 * \retval 0 on success.
 * \retval #MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED
 *         The hardware accelerator failed.
 */
#if defined(MBEDTLS_USE_PSA_CRYPTO)
int mbedtls_ct_hmac(mbedtls_svc_key_id_t key,
                    psa_algorithm_t mac_alg,
                    const unsigned char *add_data,
                    size_t add_data_len,
                    const unsigned char *data,
                    size_t data_len_secret,
                    size_t min_data_len,
                    size_t max_data_len,
                    unsigned char *output);
#else
int mbedtls_ct_hmac(mbedtls_md_context_t *ctx,
                    const unsigned char *add_data,
                    size_t add_data_len,
                    const unsigned char *data,
                    size_t data_len_secret,
                    size_t min_data_len,
                    size_t max_data_len,
                    unsigned char *output);
#endif /* defined(MBEDTLS_USE_PSA_CRYPTO) */
#endif /* MBEDTLS_TEST_HOOKS && defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC) */

#endif /* ssl_misc.h */

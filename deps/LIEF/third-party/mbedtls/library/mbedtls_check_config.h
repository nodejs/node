/**
 * \file mbedtls/check_config.h
 *
 * \brief Consistency checks for configuration options
 *
 * This is an internal header. Do not include it directly.
 *
 * This header is included automatically by all public Mbed TLS headers
 * (via mbedtls/build_info.h). Do not include it directly in a configuration
 * file such as mbedtls/mbedtls_config.h or #MBEDTLS_USER_CONFIG_FILE!
 * It would run at the wrong time due to missing derived symbols.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CHECK_CONFIG_H
#define MBEDTLS_CHECK_CONFIG_H

/* *INDENT-OFF* */

#if !defined(MBEDTLS_CONFIG_IS_FINALIZED)
#warning "Do not include mbedtls/check_config.h manually! " \
         "This may cause spurious errors. " \
         "It is included automatically at the right point since Mbed TLS 3.0."
#endif /* !MBEDTLS_CONFIG_IS_FINALIZED */

#if defined(TARGET_LIKE_MBED) && defined(MBEDTLS_NET_C)
#error "The NET module is not available for mbed OS - please use the network functions provided by Mbed OS"
#endif

#if defined(MBEDTLS_HAVE_TIME_DATE) && !defined(MBEDTLS_HAVE_TIME)
#error "MBEDTLS_HAVE_TIME_DATE without MBEDTLS_HAVE_TIME does not make sense"
#endif

/* Limitations on ECC curves acceleration: partial curve acceleration is only
 * supported with crypto excluding PK, X.509 or TLS.
 * Note: no need to check X.509 as it depends on PK. */
#if defined(MBEDTLS_PSA_ACCEL_ECC_BRAINPOOL_P_R1_256) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_BRAINPOOL_P_R1_384) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_BRAINPOOL_P_R1_512) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_MONTGOMERY_255) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_MONTGOMERY_448) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_K1_192) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_K1_256) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_192) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_256) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_384) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_521)
#if defined(MBEDTLS_PSA_ECC_ACCEL_INCOMPLETE_CURVES)
#if defined(MBEDTLS_SSL_TLS_C)
#error "Unsupported partial support for ECC curves acceleration, see docs/driver-only-builds.md"
#endif /* modules beyond what's supported */
#endif /* not all curves accelerated */
#endif /* some curve accelerated */

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED) &&                     \
    !defined(MBEDTLS_CAN_ECDH)
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) &&                 \
    ( !defined(MBEDTLS_CAN_ECDH) || !defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR_BASIC) || \
      !defined(MBEDTLS_X509_CRT_PARSE_C) || !defined(PSA_WANT_ALG_RSA_PKCS1V15_CRYPT) || !defined(PSA_WANT_ALG_RSA_PKCS1V15_SIGN) )
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) &&                \
    ( !defined(MBEDTLS_CAN_ECDH) ||                                       \
      !defined(PSA_HAVE_ALG_ECDSA_SIGN) ||                                \
      !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) &&    \
    ( !defined(PSA_WANT_ALG_JPAKE) ||                   \
      !defined(PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_BASIC) || \
      !defined(PSA_WANT_ECC_SECP_R1_256) )
#error "MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED defined, but not all prerequisites"
#endif

/* Use of EC J-PAKE in TLS requires SHA-256. */
#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) &&                    \
    !defined(PSA_WANT_ALG_SHA_256)
#error "MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED) &&        \
    !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE) &&            \
    !defined(PSA_WANT_ALG_SHA_256) &&                        \
    !defined(PSA_WANT_ALG_SHA_512) &&                        \
    !defined(PSA_WANT_ALG_SHA_1)
#error "!MBEDTLS_SSL_KEEP_PEER_CERTIFICATE requires SHA-512, SHA-256 or SHA-1".
#endif

#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT) &&                        \
    ( !defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR_BASIC) || !defined(PSA_WANT_ALG_RSA_OAEP) )
#error "MBEDTLS_X509_RSASSA_PSS_SUPPORT defined, but not all prerequisites"
#endif

/* TLS 1.3 requires separate HKDF parts from PSA,
 * and at least one ciphersuite, so at least SHA-256 or SHA-384
 * from PSA to use with HKDF.
 *
 * Note: for dependencies common with TLS 1.2 (running handshake hash),
 * see MBEDTLS_SSL_TLS_C. */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3) && \
    !(defined(MBEDTLS_PSA_CRYPTO_CLIENT) && \
      defined(PSA_WANT_ALG_HKDF_EXTRACT) && \
      defined(PSA_WANT_ALG_HKDF_EXPAND) && \
      (defined(PSA_WANT_ALG_SHA_256) || defined(PSA_WANT_ALG_SHA_384)))
#error "MBEDTLS_SSL_PROTO_TLS1_3 defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#if !( (defined(PSA_WANT_ALG_ECDH) || defined(PSA_WANT_ALG_FFDH)) && \
       defined(MBEDTLS_X509_CRT_PARSE_C) && \
       ( defined(PSA_HAVE_ALG_ECDSA_SIGN) || defined(PSA_WANT_ALG_RSA_OAEP) ) )
#error "MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED defined, but not all prerequisites"
#endif
#endif

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED)
#if !( defined(PSA_WANT_ALG_ECDH) || defined(PSA_WANT_ALG_FFDH) )
#error "MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED defined, but not all prerequisites"
#endif
#endif

/*
 * The current implementation of TLS 1.3 requires MBEDTLS_SSL_KEEP_PEER_CERTIFICATE.
 */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3) && !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
#error "MBEDTLS_SSL_PROTO_TLS1_3 defined without MBEDTLS_SSL_KEEP_PEER_CERTIFICATE"
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_2) &&                                    \
    !(defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                    \
      defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) ||                  \
      defined(MBEDTLS_KEY_EXCHANGE_PSK_ENABLED) ||                          \
      defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED) ||                    \
      defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) )
#error "One or more versions of the TLS protocol are enabled " \
        "but no key exchange methods defined with MBEDTLS_KEY_EXCHANGE_xxxx"
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_2) && \
    !(defined(PSA_WANT_ALG_SHA_1) || defined(PSA_WANT_ALG_SHA_256) || defined(PSA_WANT_ALG_SHA_512))
#error "MBEDTLS_SSL_PROTO_TLS1_2 defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_EARLY_DATA) && \
    ( !defined(MBEDTLS_SSL_SESSION_TICKETS) || \
      ( !defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED) && \
        !defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED) ) )
#error "MBEDTLS_SSL_EARLY_DATA  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_SRV_C) && \
    defined(MBEDTLS_SSL_MAX_EARLY_DATA_SIZE) &&                      \
        ((MBEDTLS_SSL_MAX_EARLY_DATA_SIZE < 0) ||                    \
         (MBEDTLS_SSL_MAX_EARLY_DATA_SIZE > UINT32_MAX))
#error "MBEDTLS_SSL_MAX_EARLY_DATA_SIZE must be in the range(0..UINT32_MAX)"
#endif

#if defined(MBEDTLS_SSL_PROTO_DTLS)     && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_PROTO_DTLS defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_CLI_C) && !defined(MBEDTLS_SSL_TLS_C)
#error "MBEDTLS_SSL_CLI_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_ASYNC_PRIVATE) && !defined(MBEDTLS_X509_CRT_PARSE_C)
#error "MBEDTLS_SSL_ASYNC_PRIVATE defined, but not all prerequisites"
#endif

/* TLS 1.2 and 1.3 require SHA-256 or SHA-384 (running handshake hash) */
#if defined(MBEDTLS_SSL_TLS_C) && \
    !(defined(PSA_WANT_ALG_SHA_256) || defined(PSA_WANT_ALG_SHA_384))
#error "MBEDTLS_SSL_TLS_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_SRV_C) && !defined(MBEDTLS_SSL_TLS_C)
#error "MBEDTLS_SSL_SRV_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS_C) && \
    !( defined(MBEDTLS_SSL_PROTO_TLS1_2) || defined(MBEDTLS_SSL_PROTO_TLS1_3) )
#error "MBEDTLS_SSL_TLS_C defined, but no protocols are active"
#endif

#if defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY) && !defined(MBEDTLS_SSL_PROTO_DTLS)
#error "MBEDTLS_SSL_DTLS_HELLO_VERIFY  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE) && \
    !defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY)
#error "MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY) &&                              \
    ( !defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_PROTO_DTLS) )
#error "MBEDTLS_SSL_DTLS_ANTI_REPLAY  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID) &&                              \
    ( !defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_PROTO_DTLS) )
#error "MBEDTLS_SSL_DTLS_CONNECTION_ID  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)            &&                 \
    defined(MBEDTLS_SSL_CID_IN_LEN_MAX) &&                 \
    MBEDTLS_SSL_CID_IN_LEN_MAX > 255
#error "MBEDTLS_SSL_CID_IN_LEN_MAX too large (max 255)"
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)            &&                  \
    defined(MBEDTLS_SSL_CID_OUT_LEN_MAX) &&                 \
    MBEDTLS_SSL_CID_OUT_LEN_MAX > 255
#error "MBEDTLS_SSL_CID_OUT_LEN_MAX too large (max 255)"
#endif

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC) &&   \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_ENCRYPT_THEN_MAC defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET) && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_EXTENDED_MASTER_SECRET defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_RENEGOTIATION) && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_RENEGOTIATION defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TICKET_C) && \
    !( defined(PSA_WANT_ALG_CCM) || defined(PSA_WANT_ALG_GCM) || \
    defined(PSA_WANT_ALG_CHACHA20_POLY1305) )
#error "MBEDTLS_SSL_TICKET_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH) && \
    MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH >= 256
#error "MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH must be less than 256"
#endif

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION) && \
        !defined(MBEDTLS_X509_CRT_PARSE_C)
#error "MBEDTLS_SSL_SERVER_NAME_INDICATION defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_VERSION_FEATURES) && !defined(MBEDTLS_VERSION_C)
#error "MBEDTLS_VERSION_FEATURES defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_USE_C) && \
    (!defined(MBEDTLS_ASN1_PARSE_C) || !defined(MBEDTLS_PK_PARSE_C))
#error "MBEDTLS_X509_USE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CREATE_C) && \
    (!defined(MBEDTLS_ASN1_WRITE_C) || !defined(MBEDTLS_PK_PARSE_C))
#error "MBEDTLS_X509_CREATE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CRT_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRL_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CRL_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CSR_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CSR_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRT_WRITE_C) && ( !defined(MBEDTLS_X509_CREATE_C) )
#error "MBEDTLS_X509_CRT_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CSR_WRITE_C) && ( !defined(MBEDTLS_X509_CREATE_C) )
#error "MBEDTLS_X509_CSR_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK) && \
            ( !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_SRTP) && ( !defined(MBEDTLS_SSL_PROTO_DTLS) )
#error "MBEDTLS_SSL_DTLS_SRTP defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH) && ( !defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH) )
#error "MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_RECORD_SIZE_LIMIT) && ( !defined(MBEDTLS_SSL_PROTO_TLS1_3) )
#error "MBEDTLS_SSL_RECORD_SIZE_LIMIT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_CONTEXT_SERIALIZATION) && \
    !( defined(PSA_WANT_ALG_CCM) || defined(PSA_WANT_ALG_GCM) || \
    defined(PSA_WANT_ALG_CHACHA20_POLY1305) )
#error "MBEDTLS_SSL_CONTEXT_SERIALIZATION defined, but not all prerequisites"
#endif

/* Reject attempts to enable options that have been removed and that could
 * cause a build to succeed but with features removed. */

#if defined(MBEDTLS_HAVEGE_C) //no-check-names
#error "MBEDTLS_HAVEGE_C was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/2599"
#endif

#if defined(MBEDTLS_SSL_HW_RECORD_ACCEL) //no-check-names
#error "MBEDTLS_SSL_HW_RECORD_ACCEL was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_SSL_PROTO_SSL3) //no-check-names
#error "MBEDTLS_SSL_PROTO_SSL3 (SSL v3.0 support) was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO) //no-check-names
#error "MBEDTLS_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO (SSL v2 ClientHello support) was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_SSL_TRUNCATED_HMAC_COMPAT) //no-check-names
#error "MBEDTLS_SSL_TRUNCATED_HMAC_COMPAT (compatibility with the buggy implementation of truncated HMAC in Mbed TLS up to 2.7) was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_TLS_DEFAULT_ALLOW_SHA1_IN_CERTIFICATES) //no-check-names
#error "MBEDTLS_TLS_DEFAULT_ALLOW_SHA1_IN_CERTIFICATES was removed in Mbed TLS 3.0. See the ChangeLog entry if you really need SHA-1-signed certificates."
#endif

#if defined(MBEDTLS_ZLIB_SUPPORT) //no-check-names
#error "MBEDTLS_ZLIB_SUPPORT was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_CHECK_PARAMS) //no-check-names
#error "MBEDTLS_CHECK_PARAMS was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4313"
#endif

#if defined(MBEDTLS_SSL_CID_PADDING_GRANULARITY) //no-check-names
#error "MBEDTLS_SSL_CID_PADDING_GRANULARITY was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4335"
#endif

#if defined(MBEDTLS_SSL_TLS1_3_PADDING_GRANULARITY) //no-check-names
#error "MBEDTLS_SSL_TLS1_3_PADDING_GRANULARITY was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4335"
#endif

#if defined(MBEDTLS_SSL_TRUNCATED_HMAC) //no-check-names
#error "MBEDTLS_SSL_TRUNCATED_HMAC was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4341"
#endif

#if defined(MBEDTLS_PKCS7_C) && ( ( !defined(MBEDTLS_ASN1_PARSE_C) ) || \
    ( !defined(MBEDTLS_PK_PARSE_C) ) || \
    ( !defined(MBEDTLS_X509_CRT_PARSE_C) ) || \
    ( !defined(MBEDTLS_X509_CRL_PARSE_C) ) || \
    ( !defined(MBEDTLS_MD_C) ) )
#error  "MBEDTLS_PKCS7_C is defined, but not all prerequisites"
#endif

/* *INDENT-ON* */
#endif /* MBEDTLS_CHECK_CONFIG_H */

/**
 * \file ssl_ciphersuites_internal.h
 *
 * \brief Internal part of the public "ssl_ciphersuites.h".
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_SSL_CIPHERSUITES_INTERNAL_H
#define MBEDTLS_SSL_CIPHERSUITES_INTERNAL_H

#include "mbedtls/pk.h"
#if defined(MBEDTLS_PK_HAVE_PRIVATE_HEADER)
#include <mbedtls/private/pk_private.h>
#endif /* MBEDTLS_PK_HAVE_PRIVATE_HEADER */

#if defined(MBEDTLS_PK_C)
mbedtls_pk_type_t mbedtls_ssl_get_ciphersuite_sig_pk_alg(const mbedtls_ssl_ciphersuite_t *info);
psa_algorithm_t mbedtls_ssl_get_ciphersuite_sig_pk_psa_alg(const mbedtls_ssl_ciphersuite_t *info);
psa_key_usage_t mbedtls_ssl_get_ciphersuite_sig_pk_psa_usage(const mbedtls_ssl_ciphersuite_t *info);
mbedtls_pk_type_t mbedtls_ssl_get_ciphersuite_sig_alg(const mbedtls_ssl_ciphersuite_t *info);
#endif /* MBEDTLS_PK_C */

int mbedtls_ssl_ciphersuite_uses_ec(const mbedtls_ssl_ciphersuite_t *info);
int mbedtls_ssl_ciphersuite_uses_psk(const mbedtls_ssl_ciphersuite_t *info);

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PFS_ENABLED)
static inline int mbedtls_ssl_ciphersuite_has_pfs(const mbedtls_ssl_ciphersuite_t *info)
{
    switch (info->MBEDTLS_PRIVATE(key_exchange)) {
        case MBEDTLS_KEY_EXCHANGE_ECDHE_RSA:
        case MBEDTLS_KEY_EXCHANGE_ECDHE_PSK:
        case MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA:
        case MBEDTLS_KEY_EXCHANGE_ECJPAKE:
            return 1;

        default:
            return 0;
    }
}
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_PFS_ENABLED */

#if defined(MBEDTLS_KEY_EXCHANGE_PSK_ENABLED)
static inline int mbedtls_ssl_ciphersuite_no_pfs(const mbedtls_ssl_ciphersuite_t *info)
{
    switch (info->MBEDTLS_PRIVATE(key_exchange)) {
        case MBEDTLS_KEY_EXCHANGE_PSK:
            return 1;

        default:
            return 0;
    }
}
#endif /* MBEDTLS_KEY_EXCHANGE_PSK_ENABLED */

static inline int mbedtls_ssl_ciphersuite_cert_req_allowed(const mbedtls_ssl_ciphersuite_t *info)
{
    switch (info->MBEDTLS_PRIVATE(key_exchange)) {
        case MBEDTLS_KEY_EXCHANGE_ECDHE_RSA:
        case MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA:
            return 1;

        default:
            return 0;
    }
}

static inline int mbedtls_ssl_ciphersuite_uses_srv_cert(const mbedtls_ssl_ciphersuite_t *info)
{
    switch (info->MBEDTLS_PRIVATE(key_exchange)) {
        case MBEDTLS_KEY_EXCHANGE_ECDHE_RSA:
        case MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA:
            return 1;

        default:
            return 0;
    }
}

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDHE_ENABLED)
static inline int mbedtls_ssl_ciphersuite_uses_ecdhe(const mbedtls_ssl_ciphersuite_t *info)
{
    switch (info->MBEDTLS_PRIVATE(key_exchange)) {
        case MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA:
        case MBEDTLS_KEY_EXCHANGE_ECDHE_RSA:
        case MBEDTLS_KEY_EXCHANGE_ECDHE_PSK:
            return 1;

        default:
            return 0;
    }
}
#endif /* MBEDTLS_KEY_EXCHANGE_SOME_ECDHE_ENABLED) */

#if defined(MBEDTLS_KEY_EXCHANGE_WITH_SERVER_SIGNATURE_ENABLED)
static inline int mbedtls_ssl_ciphersuite_uses_server_signature(
    const mbedtls_ssl_ciphersuite_t *info)
{
    switch (info->MBEDTLS_PRIVATE(key_exchange)) {
        case MBEDTLS_KEY_EXCHANGE_ECDHE_RSA:
        case MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA:
            return 1;

        default:
            return 0;
    }
}
#endif /* MBEDTLS_KEY_EXCHANGE_WITH_SERVER_SIGNATURE_ENABLED */

#endif /* MBEDTLS_SSL_CIPHERSUITES_INTERNAL_H */

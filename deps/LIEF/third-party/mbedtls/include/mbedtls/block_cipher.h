/**
 * \file block_cipher.h
 *
 * \brief Internal abstraction layer.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_BLOCK_CIPHER_H
#define MBEDTLS_BLOCK_CIPHER_H

#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_AES_C)
#include "mbedtls/aes.h"
#endif
#if defined(MBEDTLS_ARIA_C)
#include "mbedtls/aria.h"
#endif
#if defined(MBEDTLS_CAMELLIA_C)
#include "mbedtls/camellia.h"
#endif

#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
#include "psa/crypto_types.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MBEDTLS_BLOCK_CIPHER_ID_NONE = 0,  /**< Unset. */
    MBEDTLS_BLOCK_CIPHER_ID_AES,       /**< The AES cipher. */
    MBEDTLS_BLOCK_CIPHER_ID_CAMELLIA,  /**< The Camellia cipher. */
    MBEDTLS_BLOCK_CIPHER_ID_ARIA,      /**< The Aria cipher. */
} mbedtls_block_cipher_id_t;

/**
 * Used internally to indicate whether a context uses legacy or PSA.
 *
 * Internal use only.
 */
typedef enum {
    MBEDTLS_BLOCK_CIPHER_ENGINE_LEGACY = 0,
    MBEDTLS_BLOCK_CIPHER_ENGINE_PSA,
} mbedtls_block_cipher_engine_t;

typedef struct {
    mbedtls_block_cipher_id_t MBEDTLS_PRIVATE(id);
#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
    mbedtls_block_cipher_engine_t MBEDTLS_PRIVATE(engine);
    mbedtls_svc_key_id_t MBEDTLS_PRIVATE(psa_key_id);
#endif
    union {
        unsigned dummy; /* Make the union non-empty even with no supported algorithms. */
#if defined(MBEDTLS_AES_C)
        mbedtls_aes_context MBEDTLS_PRIVATE(aes);
#endif
#if defined(MBEDTLS_ARIA_C)
        mbedtls_aria_context MBEDTLS_PRIVATE(aria);
#endif
#if defined(MBEDTLS_CAMELLIA_C)
        mbedtls_camellia_context MBEDTLS_PRIVATE(camellia);
#endif
    } MBEDTLS_PRIVATE(ctx);
} mbedtls_block_cipher_context_t;

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_BLOCK_CIPHER_H */

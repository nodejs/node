/** \file psa_crypto_random_impl.h
 *
 * \brief PSA crypto random generator implementation abstraction.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_RANDOM_IMPL_H
#define PSA_CRYPTO_RANDOM_IMPL_H

#include "psa_util_internal.h"

#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)

typedef mbedtls_psa_external_random_context_t mbedtls_psa_random_context_t;

#else /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */

#include "mbedtls/entropy.h"

/* Choose a DRBG based on configuration and availability */
#if defined(MBEDTLS_CTR_DRBG_C)

#include "mbedtls/ctr_drbg.h"
#undef MBEDTLS_PSA_HMAC_DRBG_MD_TYPE

#elif defined(MBEDTLS_HMAC_DRBG_C)

#include "mbedtls/hmac_drbg.h"
#if defined(MBEDTLS_MD_CAN_SHA512) && defined(MBEDTLS_MD_CAN_SHA256)
#include <limits.h>
#if SIZE_MAX > 0xffffffff
/* Looks like a 64-bit system, so prefer SHA-512. */
#define MBEDTLS_PSA_HMAC_DRBG_MD_TYPE MBEDTLS_MD_SHA512
#else
/* Looks like a 32-bit system, so prefer SHA-256. */
#define MBEDTLS_PSA_HMAC_DRBG_MD_TYPE MBEDTLS_MD_SHA256
#endif
#elif defined(MBEDTLS_MD_CAN_SHA512)
#define MBEDTLS_PSA_HMAC_DRBG_MD_TYPE MBEDTLS_MD_SHA512
#elif defined(MBEDTLS_MD_CAN_SHA256)
#define MBEDTLS_PSA_HMAC_DRBG_MD_TYPE MBEDTLS_MD_SHA256
#else
#error "No hash algorithm available for HMAC_DBRG."
#endif

#else /* !MBEDTLS_CTR_DRBG_C && !MBEDTLS_HMAC_DRBG_C*/

#error "No DRBG module available for the psa_crypto module."

#endif /* !MBEDTLS_CTR_DRBG_C && !MBEDTLS_HMAC_DRBG_C*/

/* The maximum number of bytes that mbedtls_psa_get_random() is expected to return. */
#if defined(MBEDTLS_CTR_DRBG_C)
#define MBEDTLS_PSA_RANDOM_MAX_REQUEST MBEDTLS_CTR_DRBG_MAX_REQUEST
#elif defined(MBEDTLS_HMAC_DRBG_C)
#define MBEDTLS_PSA_RANDOM_MAX_REQUEST MBEDTLS_HMAC_DRBG_MAX_REQUEST
#endif

#if defined(MBEDTLS_CTR_DRBG_C)
typedef mbedtls_ctr_drbg_context            mbedtls_psa_drbg_context_t;
#elif defined(MBEDTLS_HMAC_DRBG_C)
typedef mbedtls_hmac_drbg_context           mbedtls_psa_drbg_context_t;
#endif /* !MBEDTLS_CTR_DRBG_C && !MBEDTLS_HMAC_DRBG_C */

typedef struct {
    void (* entropy_init)(mbedtls_entropy_context *ctx);
    void (* entropy_free)(mbedtls_entropy_context *ctx);
    mbedtls_entropy_context entropy;
    mbedtls_psa_drbg_context_t drbg;
} mbedtls_psa_random_context_t;

/** Initialize the PSA DRBG.
 *
 * \param p_rng        Pointer to the Mbed TLS DRBG state.
 */
static inline void mbedtls_psa_drbg_init(mbedtls_psa_drbg_context_t *p_rng)
{
#if defined(MBEDTLS_CTR_DRBG_C)
    mbedtls_ctr_drbg_init(p_rng);
#elif defined(MBEDTLS_HMAC_DRBG_C)
    mbedtls_hmac_drbg_init(p_rng);
#endif
}

/** Deinitialize the PSA DRBG.
 *
 * \param p_rng        Pointer to the Mbed TLS DRBG state.
 */
static inline void mbedtls_psa_drbg_free(mbedtls_psa_drbg_context_t *p_rng)
{
#if defined(MBEDTLS_CTR_DRBG_C)
    mbedtls_ctr_drbg_free(p_rng);
#elif defined(MBEDTLS_HMAC_DRBG_C)
    mbedtls_hmac_drbg_free(p_rng);
#endif
}

/** Seed the PSA DRBG.
 *
 * \param entropy       An entropy context to read the seed from.
 * \param custom        The personalization string.
 *                      This can be \c NULL, in which case the personalization
 *                      string is empty regardless of the value of \p len.
 * \param len           The length of the personalization string.
 *
 * \return              \c 0 on success.
 * \return              An Mbed TLS error code (\c MBEDTLS_ERR_xxx) on failure.
 */
static inline int mbedtls_psa_drbg_seed(mbedtls_psa_drbg_context_t *drbg_ctx,
                                        mbedtls_entropy_context *entropy,
                                        const unsigned char *custom, size_t len)
{
#if defined(MBEDTLS_CTR_DRBG_C)
    return mbedtls_ctr_drbg_seed(drbg_ctx, mbedtls_entropy_func, entropy, custom, len);
#elif defined(MBEDTLS_HMAC_DRBG_C)
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_PSA_HMAC_DRBG_MD_TYPE);
    return mbedtls_hmac_drbg_seed(drbg_ctx, md_info, mbedtls_entropy_func, entropy, custom, len);
#endif
}

#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */

#endif /* PSA_CRYPTO_RANDOM_IMPL_H */

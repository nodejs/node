/*
 *  Context structure declaration of the Mbed TLS software-based PSA drivers
 *  called through the PSA Crypto driver dispatch layer.
 *  This file contains the context structures of those algorithms which need to
 *  rely on other algorithms, i.e. are 'composite' algorithms.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 *
 * \note This header and its content are not part of the Mbed TLS API and
 * applications must not depend on it. Its main purpose is to define the
 * multi-part state objects of the Mbed TLS software-based PSA drivers. The
 * definitions of these objects are then used by crypto_struct.h to define the
 * implementation-defined types of PSA multi-part state objects.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_BUILTIN_COMPOSITES_H
#define PSA_CRYPTO_BUILTIN_COMPOSITES_H
#include "mbedtls/private_access.h"

#include <psa/crypto_driver_common.h>

#include "mbedtls/cmac.h"
#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
#include "mbedtls/gcm.h"
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
#include "mbedtls/ccm.h"
#endif
#include "mbedtls/chachapoly.h"

/*
 * MAC multi-part operation definitions.
 */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
#define MBEDTLS_PSA_BUILTIN_MAC
#endif

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
typedef struct {
    /** The HMAC algorithm in use */
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    /** The hash context. */
    struct psa_hash_operation_s hash_ctx;
    /** The HMAC part of the context. */
    uint8_t MBEDTLS_PRIVATE(opad)[PSA_HMAC_MAX_HASH_BLOCK_SIZE];
} mbedtls_psa_hmac_operation_t;

#define MBEDTLS_PSA_HMAC_OPERATION_INIT { 0, PSA_HASH_OPERATION_INIT, { 0 } }
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */

typedef struct {
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    union {
        unsigned MBEDTLS_PRIVATE(dummy); /* Make the union non-empty even with no supported algorithms. */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
        mbedtls_psa_hmac_operation_t MBEDTLS_PRIVATE(hmac);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
        mbedtls_cipher_context_t MBEDTLS_PRIVATE(cmac);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */
    } MBEDTLS_PRIVATE(ctx);
} mbedtls_psa_mac_operation_t;

#define MBEDTLS_PSA_MAC_OPERATION_INIT { 0, { 0 } }

#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CCM) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
#define MBEDTLS_PSA_BUILTIN_AEAD  1
#endif

/* Context structure for the Mbed TLS AEAD implementation. */
typedef struct {
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    psa_key_type_t MBEDTLS_PRIVATE(key_type);

    unsigned int MBEDTLS_PRIVATE(is_encrypt) : 1;

    uint8_t MBEDTLS_PRIVATE(tag_length);

    union {
        unsigned dummy; /* Enable easier initializing of the union. */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
        mbedtls_ccm_context MBEDTLS_PRIVATE(ccm);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
        mbedtls_gcm_context MBEDTLS_PRIVATE(gcm);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_GCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
        mbedtls_chachapoly_context MBEDTLS_PRIVATE(chachapoly);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305 */

    } ctx;

} mbedtls_psa_aead_operation_t;

#define MBEDTLS_PSA_AEAD_OPERATION_INIT { 0, 0, 0, 0, { 0 } }

#include "mbedtls/ecdsa.h"

/* Context structure for the Mbed TLS interruptible sign hash implementation. */
typedef struct {
#if (defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)) && \
    defined(MBEDTLS_ECP_RESTARTABLE)
    mbedtls_ecdsa_context *MBEDTLS_PRIVATE(ctx);
    mbedtls_ecdsa_restart_ctx MBEDTLS_PRIVATE(restart_ctx);

    uint32_t MBEDTLS_PRIVATE(num_ops);

    size_t MBEDTLS_PRIVATE(coordinate_bytes);
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    mbedtls_md_type_t MBEDTLS_PRIVATE(md_alg);
    uint8_t MBEDTLS_PRIVATE(hash)[PSA_BITS_TO_BYTES(PSA_VENDOR_ECC_MAX_CURVE_BITS)];
    size_t MBEDTLS_PRIVATE(hash_length);

#else
    /* Make the struct non-empty if algs not supported. */
    unsigned MBEDTLS_PRIVATE(dummy);

#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA) &&
        * defined( MBEDTLS_ECP_RESTARTABLE ) */
} mbedtls_psa_sign_hash_interruptible_operation_t;

#if (defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)) && \
    defined(MBEDTLS_ECP_RESTARTABLE)
#define MBEDTLS_PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { { 0 }, { 0 }, 0, 0, 0, 0, 0, 0 }
#else
#define MBEDTLS_PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { 0 }
#endif

/* Context structure for the Mbed TLS interruptible verify hash
 * implementation.*/
typedef struct {
#if (defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)) && \
    defined(MBEDTLS_ECP_RESTARTABLE)

    mbedtls_ecdsa_context *MBEDTLS_PRIVATE(ctx);
    mbedtls_ecdsa_restart_ctx MBEDTLS_PRIVATE(restart_ctx);

    uint32_t MBEDTLS_PRIVATE(num_ops);

    uint8_t MBEDTLS_PRIVATE(hash)[PSA_BITS_TO_BYTES(PSA_VENDOR_ECC_MAX_CURVE_BITS)];
    size_t MBEDTLS_PRIVATE(hash_length);

    mbedtls_mpi MBEDTLS_PRIVATE(r);
    mbedtls_mpi MBEDTLS_PRIVATE(s);

#else
    /* Make the struct non-empty if algs not supported. */
    unsigned MBEDTLS_PRIVATE(dummy);

#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA) &&
        * defined( MBEDTLS_ECP_RESTARTABLE ) */

} mbedtls_psa_verify_hash_interruptible_operation_t;

#if (defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)) && \
    defined(MBEDTLS_ECP_RESTARTABLE)
#define MBEDTLS_VERIFY_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { { 0 }, { 0 }, 0, 0, 0, 0, { 0 }, \
        { 0 } }
#else
#define MBEDTLS_VERIFY_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { 0 }
#endif


/* EC-JPAKE operation definitions */

#include "mbedtls/ecjpake.h"

#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
#define MBEDTLS_PSA_BUILTIN_PAKE  1
#endif

/* Note: the format for mbedtls_ecjpake_read/write function has an extra
 * length byte for each step, plus an extra 3 bytes for ECParameters in the
 * server's 2nd round. */
#define MBEDTLS_PSA_JPAKE_BUFFER_SIZE ((3 + 1 + 65 + 1 + 65 + 1 + 32) * 2)

typedef struct {
    psa_algorithm_t MBEDTLS_PRIVATE(alg);

    uint8_t *MBEDTLS_PRIVATE(password);
    size_t MBEDTLS_PRIVATE(password_len);
#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
    mbedtls_ecjpake_role MBEDTLS_PRIVATE(role);
    uint8_t MBEDTLS_PRIVATE(buffer[MBEDTLS_PSA_JPAKE_BUFFER_SIZE]);
    size_t MBEDTLS_PRIVATE(buffer_length);
    size_t MBEDTLS_PRIVATE(buffer_offset);
#endif
    /* Context structure for the Mbed TLS EC-JPAKE implementation. */
    union {
        unsigned int MBEDTLS_PRIVATE(dummy);
#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
        mbedtls_ecjpake_context MBEDTLS_PRIVATE(jpake);
#endif
    } MBEDTLS_PRIVATE(ctx);

} mbedtls_psa_pake_operation_t;

#define MBEDTLS_PSA_PAKE_OPERATION_INIT { { 0 } }

#endif /* PSA_CRYPTO_BUILTIN_COMPOSITES_H */

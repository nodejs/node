/*
 *  Declaration of context structures for use with the PSA driver wrapper
 *  interface. This file contains the context structures for 'primitive'
 *  operations, i.e. those operations which do not rely on other contexts.
 *
 *  Warning: This file will be auto-generated in the future.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 *
 * \note This header and its content are not part of the Mbed TLS API and
 * applications must not depend on it. Its main purpose is to define the
 * multi-part state objects of the PSA drivers included in the cryptographic
 * library. The definitions of these objects are then used by crypto_struct.h
 * to define the implementation-defined types of PSA multi-part state objects.
 */
/*  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_DRIVER_CONTEXTS_PRIMITIVES_H
#define PSA_CRYPTO_DRIVER_CONTEXTS_PRIMITIVES_H

#include "psa/crypto_driver_common.h"

/* Include the context structure definitions for the Mbed TLS software drivers */
#include "psa/crypto_builtin_primitives.h"

/* Include the context structure definitions for those drivers that were
 * declared during the autogeneration process. */

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1)
#include <libtestdriver1/include/psa/crypto.h>
#endif

#if defined(PSA_CRYPTO_DRIVER_TEST)

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1) && \
    defined(LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_CIPHER)
typedef libtestdriver1_mbedtls_psa_cipher_operation_t
    mbedtls_transparent_test_driver_cipher_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_CIPHER_OPERATION_INIT \
    LIBTESTDRIVER1_MBEDTLS_PSA_CIPHER_OPERATION_INIT
#else
typedef mbedtls_psa_cipher_operation_t
    mbedtls_transparent_test_driver_cipher_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_CIPHER_OPERATION_INIT \
    MBEDTLS_PSA_CIPHER_OPERATION_INIT
#endif /* MBEDTLS_TEST_LIBTESTDRIVER1 &&
          LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_CIPHER */

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1) && \
    defined(LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_HASH)
typedef libtestdriver1_mbedtls_psa_hash_operation_t
    mbedtls_transparent_test_driver_hash_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_HASH_OPERATION_INIT \
    LIBTESTDRIVER1_MBEDTLS_PSA_HASH_OPERATION_INIT
#else
typedef mbedtls_psa_hash_operation_t
    mbedtls_transparent_test_driver_hash_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_HASH_OPERATION_INIT \
    MBEDTLS_PSA_HASH_OPERATION_INIT
#endif /* MBEDTLS_TEST_LIBTESTDRIVER1 &&
          LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_HASH */

typedef struct {
    unsigned int initialised : 1;
    mbedtls_transparent_test_driver_cipher_operation_t ctx;
} mbedtls_opaque_test_driver_cipher_operation_t;

#define MBEDTLS_OPAQUE_TEST_DRIVER_CIPHER_OPERATION_INIT \
    { 0, MBEDTLS_TRANSPARENT_TEST_DRIVER_CIPHER_OPERATION_INIT }

#endif /* PSA_CRYPTO_DRIVER_TEST */

/* Define the context to be used for an operation that is executed through the
 * PSA Driver wrapper layer as the union of all possible driver's contexts.
 *
 * The union members are the driver's context structures, and the member names
 * are formatted as `'drivername'_ctx`. This allows for procedural generation
 * of both this file and the content of psa_crypto_driver_wrappers.h */

typedef union {
    unsigned dummy; /* Make sure this union is always non-empty */
    mbedtls_psa_hash_operation_t mbedtls_ctx;
#if defined(PSA_CRYPTO_DRIVER_TEST)
    mbedtls_transparent_test_driver_hash_operation_t test_driver_ctx;
#endif
} psa_driver_hash_context_t;

typedef union {
    unsigned dummy; /* Make sure this union is always non-empty */
    mbedtls_psa_cipher_operation_t mbedtls_ctx;
#if defined(PSA_CRYPTO_DRIVER_TEST)
    mbedtls_transparent_test_driver_cipher_operation_t transparent_test_driver_ctx;
    mbedtls_opaque_test_driver_cipher_operation_t opaque_test_driver_ctx;
#endif
} psa_driver_cipher_context_t;

#endif /* PSA_CRYPTO_DRIVER_CONTEXTS_PRIMITIVES_H */
/* End of automatically generated file. */

/**
 * \file sha3.h
 *
 * \brief This file contains SHA-3 definitions and functions.
 *
 * The Secure Hash Algorithms cryptographic
 * hash functions are defined in <em>FIPS 202: SHA-3 Standard:
 * Permutation-Based Hash and Extendable-Output Functions </em>.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_SHA3_H
#define MBEDTLS_SHA3_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SHA-3 input data was malformed. */
#define MBEDTLS_ERR_SHA3_BAD_INPUT_DATA                 -0x0076

/**
 * SHA-3 family id.
 *
 * It identifies the family (SHA3-256, SHA3-512, etc.)
 */

typedef enum {
    MBEDTLS_SHA3_NONE = 0, /*!< Operation not defined. */
    MBEDTLS_SHA3_224, /*!< SHA3-224 */
    MBEDTLS_SHA3_256, /*!< SHA3-256 */
    MBEDTLS_SHA3_384, /*!< SHA3-384 */
    MBEDTLS_SHA3_512, /*!< SHA3-512 */
} mbedtls_sha3_id;

/**
 * \brief          The SHA-3 context structure.
 *
 *                 The structure is used SHA-3 checksum calculations.
 */
typedef struct {
    uint64_t MBEDTLS_PRIVATE(state[25]);
    uint32_t MBEDTLS_PRIVATE(index);
    uint16_t MBEDTLS_PRIVATE(olen);
    uint16_t MBEDTLS_PRIVATE(max_block_size);
}
mbedtls_sha3_context;

/**
 * \brief          This function initializes a SHA-3 context.
 *
 * \param ctx      The SHA-3 context to initialize. This must not be \c NULL.
 */
void mbedtls_sha3_init(mbedtls_sha3_context *ctx);

/**
 * \brief          This function clears a SHA-3 context.
 *
 * \param ctx      The SHA-3 context to clear. This may be \c NULL, in which
 *                 case this function returns immediately. If it is not \c NULL,
 *                 it must point to an initialized SHA-3 context.
 */
void mbedtls_sha3_free(mbedtls_sha3_context *ctx);

/**
 * \brief          This function clones the state of a SHA-3 context.
 *
 * \param dst      The destination context. This must be initialized.
 * \param src      The context to clone. This must be initialized.
 */
void mbedtls_sha3_clone(mbedtls_sha3_context *dst,
                        const mbedtls_sha3_context *src);

/**
 * \brief          This function starts a SHA-3 checksum
 *                 calculation.
 *
 * \param ctx      The context to use. This must be initialized.
 * \param id       The id of the SHA-3 family.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha3_starts(mbedtls_sha3_context *ctx, mbedtls_sha3_id id);

/**
 * \brief          This function feeds an input buffer into an ongoing
 *                 SHA-3 checksum calculation.
 *
 * \param ctx      The SHA-3 context. This must be initialized
 *                 and have a hash operation started.
 * \param input    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha3_update(mbedtls_sha3_context *ctx,
                        const uint8_t *input,
                        size_t ilen);

/**
 * \brief          This function finishes the SHA-3 operation, and writes
 *                 the result to the output buffer.
 *
 * \param ctx      The SHA-3 context. This must be initialized
 *                 and have a hash operation started.
 * \param output   The SHA-3 checksum result.
 *                 This must be a writable buffer of length \c olen bytes.
 * \param olen     Defines the length of output buffer (in bytes). For SHA-3 224, SHA-3 256,
 *                 SHA-3 384 and SHA-3 512 \c olen must equal to 28, 32, 48 and 64,
 *                 respectively.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha3_finish(mbedtls_sha3_context *ctx,
                        uint8_t *output, size_t olen);

/**
 * \brief          This function calculates the SHA-3
 *                 checksum of a buffer.
 *
 *                 The function allocates the context, performs the
 *                 calculation, and frees the context.
 *
 *                 The SHA-3 result is calculated as
 *                 output = SHA-3(id, input buffer, d).
 *
 * \param id       The id of the SHA-3 family.
 * \param input    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 * \param output   The SHA-3 checksum result.
 *                 This must be a writable buffer of length \c olen bytes.
 * \param olen     Defines the length of output buffer (in bytes). For SHA-3 224, SHA-3 256,
 *                 SHA-3 384 and SHA-3 512 \c olen must equal to 28, 32, 48 and 64,
 *                 respectively.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha3(mbedtls_sha3_id id, const uint8_t *input,
                 size_t ilen,
                 uint8_t *output,
                 size_t olen);

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief          Checkup routine for the algorithms implemented
 *                 by this module: SHA3-224, SHA3-256, SHA3-384, SHA3-512.
 *
 * \return         0 if successful, or 1 if the test failed.
 */
int mbedtls_sha3_self_test(int verbose);
#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* mbedtls_sha3.h */

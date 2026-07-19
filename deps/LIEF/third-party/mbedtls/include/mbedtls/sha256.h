/**
 * \file sha256.h
 *
 * \brief This file contains SHA-224 and SHA-256 definitions and functions.
 *
 * The Secure Hash Algorithms 224 and 256 (SHA-224 and SHA-256) cryptographic
 * hash functions are defined in <em>FIPS 180-4: Secure Hash Standard (SHS)</em>.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_SHA256_H
#define MBEDTLS_SHA256_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stddef.h>
#include <stdint.h>

/** SHA-256 input data was malformed. */
#define MBEDTLS_ERR_SHA256_BAD_INPUT_DATA                 -0x0074

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_SHA256_ALT)
// Regular implementation
//

/**
 * \brief          The SHA-256 context structure.
 *
 *                 The structure is used both for SHA-256 and for SHA-224
 *                 checksum calculations. The choice between these two is
 *                 made in the call to mbedtls_sha256_starts().
 */
typedef struct mbedtls_sha256_context {
    unsigned char MBEDTLS_PRIVATE(buffer)[64];   /*!< The data block being processed. */
    uint32_t MBEDTLS_PRIVATE(total)[2];          /*!< The number of Bytes processed.  */
    uint32_t MBEDTLS_PRIVATE(state)[8];          /*!< The intermediate digest state.  */
#if defined(MBEDTLS_SHA224_C)
    int MBEDTLS_PRIVATE(is224);                  /*!< Determines which function to use:
                                                    0: Use SHA-256, or 1: Use SHA-224. */
#endif
}
mbedtls_sha256_context;

#else  /* MBEDTLS_SHA256_ALT */
#include "sha256_alt.h"
#endif /* MBEDTLS_SHA256_ALT */

/**
 * \brief          This function initializes a SHA-256 context.
 *
 * \param ctx      The SHA-256 context to initialize. This must not be \c NULL.
 */
void mbedtls_sha256_init(mbedtls_sha256_context *ctx);

/**
 * \brief          This function clears a SHA-256 context.
 *
 * \param ctx      The SHA-256 context to clear. This may be \c NULL, in which
 *                 case this function returns immediately. If it is not \c NULL,
 *                 it must point to an initialized SHA-256 context.
 */
void mbedtls_sha256_free(mbedtls_sha256_context *ctx);

/**
 * \brief          This function clones the state of a SHA-256 context.
 *
 * \param dst      The destination context. This must be initialized.
 * \param src      The context to clone. This must be initialized.
 */
void mbedtls_sha256_clone(mbedtls_sha256_context *dst,
                          const mbedtls_sha256_context *src);

/**
 * \brief          This function starts a SHA-224 or SHA-256 checksum
 *                 calculation.
 *
 * \param ctx      The context to use. This must be initialized.
 * \param is224    This determines which function to use. This must be
 *                 either \c 0 for SHA-256, or \c 1 for SHA-224.
 *
 * \note           is224 must be defined accordingly to the enabled
 *                 MBEDTLS_SHA224_C/MBEDTLS_SHA256_C symbols otherwise the
 *                 function will return #MBEDTLS_ERR_SHA512_BAD_INPUT_DATA.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int is224);

/**
 * \brief          This function feeds an input buffer into an ongoing
 *                 SHA-256 checksum calculation.
 *
 * \param ctx      The SHA-256 context. This must be initialized
 *                 and have a hash operation started.
 * \param input    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_update(mbedtls_sha256_context *ctx,
                          const unsigned char *input,
                          size_t ilen);

/**
 * \brief          This function finishes the SHA-256 operation, and writes
 *                 the result to the output buffer.
 *
 * \param ctx      The SHA-256 context. This must be initialized
 *                 and have a hash operation started.
 * \param output   The SHA-224 or SHA-256 checksum result.
 *                 This must be a writable buffer of length \c 32 bytes
 *                 for SHA-256, \c 28 bytes for SHA-224.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_finish(mbedtls_sha256_context *ctx,
                          unsigned char *output);

/**
 * \brief          This function processes a single data block within
 *                 the ongoing SHA-256 computation. This function is for
 *                 internal use only.
 *
 * \param ctx      The SHA-256 context. This must be initialized.
 * \param data     The buffer holding one block of data. This must
 *                 be a readable buffer of length \c 64 Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_internal_sha256_process(mbedtls_sha256_context *ctx,
                                    const unsigned char data[64]);

/**
 * \brief          This function calculates the SHA-224 or SHA-256
 *                 checksum of a buffer.
 *
 *                 The function allocates the context, performs the
 *                 calculation, and frees the context.
 *
 *                 The SHA-256 result is calculated as
 *                 output = SHA-256(input buffer).
 *
 * \param input    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 * \param output   The SHA-224 or SHA-256 checksum result.
 *                 This must be a writable buffer of length \c 32 bytes
 *                 for SHA-256, \c 28 bytes for SHA-224.
 * \param is224    Determines which function to use. This must be
 *                 either \c 0 for SHA-256, or \c 1 for SHA-224.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256(const unsigned char *input,
                   size_t ilen,
                   unsigned char *output,
                   int is224);

#if defined(MBEDTLS_SELF_TEST)

#if defined(MBEDTLS_SHA224_C)
/**
 * \brief          The SHA-224 checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_sha224_self_test(int verbose);
#endif /* MBEDTLS_SHA224_C */

#if defined(MBEDTLS_SHA256_C)
/**
 * \brief          The SHA-256 checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_sha256_self_test(int verbose);
#endif /* MBEDTLS_SHA256_C */

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* mbedtls_sha256.h */

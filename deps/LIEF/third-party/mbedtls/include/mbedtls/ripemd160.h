/**
 * \file ripemd160.h
 *
 * \brief RIPE MD-160 message digest
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_RIPEMD160_H
#define MBEDTLS_RIPEMD160_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_RIPEMD160_ALT)
// Regular implementation
//

/**
 * \brief          RIPEMD-160 context structure
 */
typedef struct mbedtls_ripemd160_context {
    uint32_t MBEDTLS_PRIVATE(total)[2];          /*!< number of bytes processed  */
    uint32_t MBEDTLS_PRIVATE(state)[5];          /*!< intermediate digest state  */
    unsigned char MBEDTLS_PRIVATE(buffer)[64];   /*!< data block being processed */
}
mbedtls_ripemd160_context;

#else  /* MBEDTLS_RIPEMD160_ALT */
#include "ripemd160_alt.h"
#endif /* MBEDTLS_RIPEMD160_ALT */

/**
 * \brief          Initialize RIPEMD-160 context
 *
 * \param ctx      RIPEMD-160 context to be initialized
 */
void mbedtls_ripemd160_init(mbedtls_ripemd160_context *ctx);

/**
 * \brief          Clear RIPEMD-160 context
 *
 * \param ctx      RIPEMD-160 context to be cleared
 */
void mbedtls_ripemd160_free(mbedtls_ripemd160_context *ctx);

/**
 * \brief          Clone (the state of) a RIPEMD-160 context
 *
 * \param dst      The destination context
 * \param src      The context to be cloned
 */
void mbedtls_ripemd160_clone(mbedtls_ripemd160_context *dst,
                             const mbedtls_ripemd160_context *src);

/**
 * \brief          RIPEMD-160 context setup
 *
 * \param ctx      context to be initialized
 *
 * \return         0 if successful
 */
int mbedtls_ripemd160_starts(mbedtls_ripemd160_context *ctx);

/**
 * \brief          RIPEMD-160 process buffer
 *
 * \param ctx      RIPEMD-160 context
 * \param input    buffer holding the data
 * \param ilen     length of the input data
 *
 * \return         0 if successful
 */
int mbedtls_ripemd160_update(mbedtls_ripemd160_context *ctx,
                             const unsigned char *input,
                             size_t ilen);

/**
 * \brief          RIPEMD-160 final digest
 *
 * \param ctx      RIPEMD-160 context
 * \param output   RIPEMD-160 checksum result
 *
 * \return         0 if successful
 */
int mbedtls_ripemd160_finish(mbedtls_ripemd160_context *ctx,
                             unsigned char output[20]);

/**
 * \brief          RIPEMD-160 process data block (internal use only)
 *
 * \param ctx      RIPEMD-160 context
 * \param data     buffer holding one block of data
 *
 * \return         0 if successful
 */
int mbedtls_internal_ripemd160_process(mbedtls_ripemd160_context *ctx,
                                       const unsigned char data[64]);

/**
 * \brief          Output = RIPEMD-160( input buffer )
 *
 * \param input    buffer holding the data
 * \param ilen     length of the input data
 * \param output   RIPEMD-160 checksum result
 *
 * \return         0 if successful
 */
int mbedtls_ripemd160(const unsigned char *input,
                      size_t ilen,
                      unsigned char output[20]);

#if defined(MBEDTLS_SELF_TEST)

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int mbedtls_ripemd160_self_test(int verbose);

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* mbedtls_ripemd160.h */

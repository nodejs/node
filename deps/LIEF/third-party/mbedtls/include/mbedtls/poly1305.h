/**
 * \file poly1305.h
 *
 * \brief   This file contains Poly1305 definitions and functions.
 *
 *          Poly1305 is a one-time message authenticator that can be used to
 *          authenticate messages. Poly1305-AES was created by Daniel
 *          Bernstein https://cr.yp.to/mac/poly1305-20050329.pdf The generic
 *          Poly1305 algorithm (not tied to AES) was also standardized in RFC
 *          7539.
 *
 * \author Daniel King <damaki.gh@gmail.com>
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_POLY1305_H
#define MBEDTLS_POLY1305_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stdint.h>
#include <stddef.h>

/** Invalid input parameter(s). */
#define MBEDTLS_ERR_POLY1305_BAD_INPUT_DATA         -0x0057

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_POLY1305_ALT)

typedef struct mbedtls_poly1305_context {
    uint32_t MBEDTLS_PRIVATE(r)[4];      /** The value for 'r' (low 128 bits of the key). */
    uint32_t MBEDTLS_PRIVATE(s)[4];      /** The value for 's' (high 128 bits of the key). */
    uint32_t MBEDTLS_PRIVATE(acc)[5];    /** The accumulator number. */
    uint8_t MBEDTLS_PRIVATE(queue)[16];  /** The current partial block of data. */
    size_t MBEDTLS_PRIVATE(queue_len);   /** The number of bytes stored in 'queue'. */
}
mbedtls_poly1305_context;

#else  /* MBEDTLS_POLY1305_ALT */
#include "poly1305_alt.h"
#endif /* MBEDTLS_POLY1305_ALT */

/**
 * \brief           This function initializes the specified Poly1305 context.
 *
 *                  It must be the first API called before using
 *                  the context.
 *
 *                  It is usually followed by a call to
 *                  \c mbedtls_poly1305_starts(), then one or more calls to
 *                  \c mbedtls_poly1305_update(), then one call to
 *                  \c mbedtls_poly1305_finish(), then finally
 *                  \c mbedtls_poly1305_free().
 *
 * \param ctx       The Poly1305 context to initialize. This must
 *                  not be \c NULL.
 */
void mbedtls_poly1305_init(mbedtls_poly1305_context *ctx);

/**
 * \brief           This function releases and clears the specified
 *                  Poly1305 context.
 *
 * \param ctx       The Poly1305 context to clear. This may be \c NULL, in which
 *                  case this function is a no-op. If it is not \c NULL, it must
 *                  point to an initialized Poly1305 context.
 */
void mbedtls_poly1305_free(mbedtls_poly1305_context *ctx);

/**
 * \brief           This function sets the one-time authentication key.
 *
 * \warning         The key must be unique and unpredictable for each
 *                  invocation of Poly1305.
 *
 * \param ctx       The Poly1305 context to which the key should be bound.
 *                  This must be initialized.
 * \param key       The buffer containing the \c 32 Byte (\c 256 Bit) key.
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_poly1305_starts(mbedtls_poly1305_context *ctx,
                            const unsigned char key[32]);

/**
 * \brief           This functions feeds an input buffer into an ongoing
 *                  Poly1305 computation.
 *
 *                  It is called between \c mbedtls_cipher_poly1305_starts() and
 *                  \c mbedtls_cipher_poly1305_finish().
 *                  It can be called repeatedly to process a stream of data.
 *
 * \param ctx       The Poly1305 context to use for the Poly1305 operation.
 *                  This must be initialized and bound to a key.
 * \param ilen      The length of the input data in Bytes.
 *                  Any value is accepted.
 * \param input     The buffer holding the input data.
 *                  This pointer can be \c NULL if `ilen == 0`.
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_poly1305_update(mbedtls_poly1305_context *ctx,
                            const unsigned char *input,
                            size_t ilen);

/**
 * \brief           This function generates the Poly1305 Message
 *                  Authentication Code (MAC).
 *
 * \param ctx       The Poly1305 context to use for the Poly1305 operation.
 *                  This must be initialized and bound to a key.
 * \param mac       The buffer to where the MAC is written. This must
 *                  be a writable buffer of length \c 16 Bytes.
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_poly1305_finish(mbedtls_poly1305_context *ctx,
                            unsigned char mac[16]);

/**
 * \brief           This function calculates the Poly1305 MAC of the input
 *                  buffer with the provided key.
 *
 * \warning         The key must be unique and unpredictable for each
 *                  invocation of Poly1305.
 *
 * \param key       The buffer containing the \c 32 Byte (\c 256 Bit) key.
 * \param ilen      The length of the input data in Bytes.
 *                  Any value is accepted.
 * \param input     The buffer holding the input data.
 *                  This pointer can be \c NULL if `ilen == 0`.
 * \param mac       The buffer to where the MAC is written. This must be
 *                  a writable buffer of length \c 16 Bytes.
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_poly1305_mac(const unsigned char key[32],
                         const unsigned char *input,
                         size_t ilen,
                         unsigned char mac[16]);

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief           The Poly1305 checkup routine.
 *
 * \return          \c 0 on success.
 * \return          \c 1 on failure.
 */
int mbedtls_poly1305_self_test(int verbose);
#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_POLY1305_H */

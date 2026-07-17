/**
 * \file chacha20.h
 *
 * \brief   This file contains ChaCha20 definitions and functions.
 *
 *          ChaCha20 is a stream cipher that can encrypt and decrypt
 *          information. ChaCha was created by Daniel Bernstein as a variant of
 *          its Salsa cipher https://cr.yp.to/chacha/chacha-20080128.pdf
 *          ChaCha20 is the variant with 20 rounds, that was also standardized
 *          in RFC 7539.
 *
 * \author Daniel King <damaki.gh@gmail.com>
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CHACHA20_H
#define MBEDTLS_CHACHA20_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stdint.h>
#include <stddef.h>

/** Invalid input parameter(s). */
#define MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA         -0x0051

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_CHACHA20_ALT)

typedef struct mbedtls_chacha20_context {
    uint32_t MBEDTLS_PRIVATE(state)[16];          /*! The state (before round operations). */
    uint8_t  MBEDTLS_PRIVATE(keystream8)[64];     /*! Leftover keystream bytes. */
    size_t MBEDTLS_PRIVATE(keystream_bytes_used); /*! Number of keystream bytes already used. */
}
mbedtls_chacha20_context;

#else  /* MBEDTLS_CHACHA20_ALT */
#include "chacha20_alt.h"
#endif /* MBEDTLS_CHACHA20_ALT */

/**
 * \brief           This function initializes the specified ChaCha20 context.
 *
 *                  It must be the first API called before using
 *                  the context.
 *
 *                  It is usually followed by calls to
 *                  \c mbedtls_chacha20_setkey() and
 *                  \c mbedtls_chacha20_starts(), then one or more calls to
 *                  to \c mbedtls_chacha20_update(), and finally to
 *                  \c mbedtls_chacha20_free().
 *
 * \param ctx       The ChaCha20 context to initialize.
 *                  This must not be \c NULL.
 */
void mbedtls_chacha20_init(mbedtls_chacha20_context *ctx);

/**
 * \brief           This function releases and clears the specified
 *                  ChaCha20 context.
 *
 * \param ctx       The ChaCha20 context to clear. This may be \c NULL,
 *                  in which case this function is a no-op. If it is not
 *                  \c NULL, it must point to an initialized context.
 *
 */
void mbedtls_chacha20_free(mbedtls_chacha20_context *ctx);

/**
 * \brief           This function sets the encryption/decryption key.
 *
 * \note            After using this function, you must also call
 *                  \c mbedtls_chacha20_starts() to set a nonce before you
 *                  start encrypting/decrypting data with
 *                  \c mbedtls_chacha_update().
 *
 * \param ctx       The ChaCha20 context to which the key should be bound.
 *                  It must be initialized.
 * \param key       The encryption/decryption key. This must be \c 32 Bytes
 *                  in length.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA if ctx or key is NULL.
 */
int mbedtls_chacha20_setkey(mbedtls_chacha20_context *ctx,
                            const unsigned char key[32]);

/**
 * \brief           This function sets the nonce and initial counter value.
 *
 * \note            A ChaCha20 context can be re-used with the same key by
 *                  calling this function to change the nonce.
 *
 * \warning         You must never use the same nonce twice with the same key.
 *                  This would void any confidentiality guarantees for the
 *                  messages encrypted with the same nonce and key.
 *
 * \param ctx       The ChaCha20 context to which the nonce should be bound.
 *                  It must be initialized and bound to a key.
 * \param nonce     The nonce. This must be \c 12 Bytes in size.
 * \param counter   The initial counter value. This is usually \c 0.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA if ctx or nonce is
 *                  NULL.
 */
int mbedtls_chacha20_starts(mbedtls_chacha20_context *ctx,
                            const unsigned char nonce[12],
                            uint32_t counter);

/**
 * \brief           This function encrypts or decrypts data.
 *
 *                  Since ChaCha20 is a stream cipher, the same operation is
 *                  used for encrypting and decrypting data.
 *
 * \note            The \p input and \p output pointers must either be equal or
 *                  point to non-overlapping buffers.
 *
 * \note            \c mbedtls_chacha20_setkey() and
 *                  \c mbedtls_chacha20_starts() must be called at least once
 *                  to setup the context before this function can be called.
 *
 * \note            This function can be called multiple times in a row in
 *                  order to encrypt of decrypt data piecewise with the same
 *                  key and nonce.
 *
 * \param ctx       The ChaCha20 context to use for encryption or decryption.
 *                  It must be initialized and bound to a key and nonce.
 * \param size      The length of the input data in Bytes.
 * \param input     The buffer holding the input data.
 *                  This pointer can be \c NULL if `size == 0`.
 * \param output    The buffer holding the output data.
 *                  This must be able to hold \p size Bytes.
 *                  This pointer can be \c NULL if `size == 0`.
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_chacha20_update(mbedtls_chacha20_context *ctx,
                            size_t size,
                            const unsigned char *input,
                            unsigned char *output);

/**
 * \brief           This function encrypts or decrypts data with ChaCha20 and
 *                  the given key and nonce.
 *
 *                  Since ChaCha20 is a stream cipher, the same operation is
 *                  used for encrypting and decrypting data.
 *
 * \warning         You must never use the same (key, nonce) pair more than
 *                  once. This would void any confidentiality guarantees for
 *                  the messages encrypted with the same nonce and key.
 *
 * \note            The \p input and \p output pointers must either be equal or
 *                  point to non-overlapping buffers.
 *
 * \param key       The encryption/decryption key.
 *                  This must be \c 32 Bytes in length.
 * \param nonce     The nonce. This must be \c 12 Bytes in size.
 * \param counter   The initial counter value. This is usually \c 0.
 * \param size      The length of the input data in Bytes.
 * \param input     The buffer holding the input data.
 *                  This pointer can be \c NULL if `size == 0`.
 * \param output    The buffer holding the output data.
 *                  This must be able to hold \p size Bytes.
 *                  This pointer can be \c NULL if `size == 0`.
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_chacha20_crypt(const unsigned char key[32],
                           const unsigned char nonce[12],
                           uint32_t counter,
                           size_t size,
                           const unsigned char *input,
                           unsigned char *output);

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief           The ChaCha20 checkup routine.
 *
 * \return          \c 0 on success.
 * \return          \c 1 on failure.
 */
int mbedtls_chacha20_self_test(int verbose);
#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_CHACHA20_H */

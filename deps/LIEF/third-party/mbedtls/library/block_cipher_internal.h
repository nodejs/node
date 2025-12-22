/**
 * \file block_cipher_internal.h
 *
 * \brief Lightweight abstraction layer for block ciphers with 128 bit blocks,
 * for use by the GCM and CCM modules.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_BLOCK_CIPHER_INTERNAL_H
#define MBEDTLS_BLOCK_CIPHER_INTERNAL_H

#include "mbedtls/build_info.h"

#include "mbedtls/cipher.h"

#include "mbedtls/block_cipher.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Initialize the context.
 *                  This must be the first API call before using the context.
 *
 * \param ctx       The context to initialize.
 */
static inline void mbedtls_block_cipher_init(mbedtls_block_cipher_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

/**
 * \brief           Set the block cipher to use with this context.
 *                  This must be called after mbedtls_block_cipher_init().
 *
 * \param ctx       The context to set up.
 * \param cipher_id The identifier of the cipher to use.
 *                  This must be either AES, ARIA or Camellia.
 *                  Warning: this is a ::mbedtls_cipher_id_t,
 *                  not a ::mbedtls_block_cipher_id_t!
 *
 * \retval          \c 0 on success.
 * \retval          #MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA if \p cipher_id was
 *                  invalid.
 */
int mbedtls_block_cipher_setup(mbedtls_block_cipher_context_t *ctx,
                               mbedtls_cipher_id_t cipher_id);

/**
 * \brief           Set the key into the context.
 *
 * \param ctx       The context to configure.
 * \param key       The buffer holding the key material.
 * \param key_bitlen    The size of the key in bits.
 *
 * \retval          \c 0 on success.
 * \retval          #MBEDTLS_ERR_CIPHER_INVALID_CONTEXT if the context was not
 *                  properly set up before calling this function.
 * \retval          One of #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH,
 *                  #MBEDTLS_ERR_ARIA_BAD_INPUT_DATA,
 *                  #MBEDTLS_ERR_CAMELLIA_BAD_INPUT_DATA if \p key_bitlen is
 *                  invalid.
 */
int mbedtls_block_cipher_setkey(mbedtls_block_cipher_context_t *ctx,
                                const unsigned char *key,
                                unsigned key_bitlen);

/**
 * \brief           Encrypt one block (16 bytes) with the configured key.
 *
 * \param ctx       The context holding the key.
 * \param input     The buffer holding the input block. Must be 16 bytes.
 * \param output    The buffer to which the output block will be written.
 *                  Must be writable and 16 bytes long.
 *                  This must either not overlap with \p input, or be equal.
 *
 * \retval          \c 0 on success.
 * \retval          #MBEDTLS_ERR_CIPHER_INVALID_CONTEXT if the context was not
 *                  properly set up before calling this function.
 * \retval          Another negative value if encryption failed.
 */
int mbedtls_block_cipher_encrypt(mbedtls_block_cipher_context_t *ctx,
                                 const unsigned char input[16],
                                 unsigned char output[16]);
/**
 * \brief           Clear the context.
 *
 * \param ctx       The context to clear.
 */
void mbedtls_block_cipher_free(mbedtls_block_cipher_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_BLOCK_CIPHER_INTERNAL_H */

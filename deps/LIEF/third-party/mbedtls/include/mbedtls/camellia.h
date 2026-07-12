/**
 * \file camellia.h
 *
 * \brief Camellia block cipher
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_CAMELLIA_H
#define MBEDTLS_CAMELLIA_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stddef.h>
#include <stdint.h>

#include "mbedtls/platform_util.h"

#define MBEDTLS_CAMELLIA_ENCRYPT     1
#define MBEDTLS_CAMELLIA_DECRYPT     0

/** Bad input data. */
#define MBEDTLS_ERR_CAMELLIA_BAD_INPUT_DATA -0x0024

/** Invalid data input length. */
#define MBEDTLS_ERR_CAMELLIA_INVALID_INPUT_LENGTH -0x0026

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_CAMELLIA_ALT)
// Regular implementation
//

/**
 * \brief          CAMELLIA context structure
 */
typedef struct mbedtls_camellia_context {
    int MBEDTLS_PRIVATE(nr);                     /*!<  number of rounds  */
    uint32_t MBEDTLS_PRIVATE(rk)[68];            /*!<  CAMELLIA round keys    */
}
mbedtls_camellia_context;

#else  /* MBEDTLS_CAMELLIA_ALT */
#include "camellia_alt.h"
#endif /* MBEDTLS_CAMELLIA_ALT */

/**
 * \brief          Initialize a CAMELLIA context.
 *
 * \param ctx      The CAMELLIA context to be initialized.
 *                 This must not be \c NULL.
 */
void mbedtls_camellia_init(mbedtls_camellia_context *ctx);

/**
 * \brief          Clear a CAMELLIA context.
 *
 * \param ctx      The CAMELLIA context to be cleared. This may be \c NULL,
 *                 in which case this function returns immediately. If it is not
 *                 \c NULL, it must be initialized.
 */
void mbedtls_camellia_free(mbedtls_camellia_context *ctx);

/**
 * \brief          Perform a CAMELLIA key schedule operation for encryption.
 *
 * \param ctx      The CAMELLIA context to use. This must be initialized.
 * \param key      The encryption key to use. This must be a readable buffer
 *                 of size \p keybits Bits.
 * \param keybits  The length of \p key in Bits. This must be either \c 128,
 *                 \c 192 or \c 256.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_camellia_setkey_enc(mbedtls_camellia_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits);

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
/**
 * \brief          Perform a CAMELLIA key schedule operation for decryption.
 *
 * \param ctx      The CAMELLIA context to use. This must be initialized.
 * \param key      The decryption key. This must be a readable buffer
 *                 of size \p keybits Bits.
 * \param keybits  The length of \p key in Bits. This must be either \c 128,
 *                 \c 192 or \c 256.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_camellia_setkey_dec(mbedtls_camellia_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits);
#endif /* !MBEDTLS_BLOCK_CIPHER_NO_DECRYPT */

/**
 * \brief          Perform a CAMELLIA-ECB block encryption/decryption operation.
 *
 * \param ctx      The CAMELLIA context to use. This must be initialized
 *                 and bound to a key.
 * \param mode     The mode of operation. This must be either
 *                 #MBEDTLS_CAMELLIA_ENCRYPT or #MBEDTLS_CAMELLIA_DECRYPT.
 * \param input    The input block. This must be a readable buffer
 *                 of size \c 16 Bytes.
 * \param output   The output block. This must be a writable buffer
 *                 of size \c 16 Bytes.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_camellia_crypt_ecb(mbedtls_camellia_context *ctx,
                               int mode,
                               const unsigned char input[16],
                               unsigned char output[16]);

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/**
 * \brief          Perform a CAMELLIA-CBC buffer encryption/decryption operation.
 *
 * \note           Upon exit, the content of the IV is updated so that you can
 *                 call the function same function again on the following
 *                 block(s) of data and get the same result as if it was
 *                 encrypted in one call. This allows a "streaming" usage.
 *                 If on the other hand you need to retain the contents of the
 *                 IV, you should either save it manually or use the cipher
 *                 module instead.
 *
 * \param ctx      The CAMELLIA context to use. This must be initialized
 *                 and bound to a key.
 * \param mode     The mode of operation. This must be either
 *                 #MBEDTLS_CAMELLIA_ENCRYPT or #MBEDTLS_CAMELLIA_DECRYPT.
 * \param length   The length in Bytes of the input data \p input.
 *                 This must be a multiple of \c 16 Bytes.
 * \param iv       The initialization vector. This must be a read/write buffer
 *                 of length \c 16 Bytes. It is updated to allow streaming
 *                 use as explained above.
 * \param input    The buffer holding the input data. This must point to a
 *                 readable buffer of length \p length Bytes.
 * \param output   The buffer holding the output data. This must point to a
 *                 writable buffer of length \p length Bytes.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_camellia_crypt_cbc(mbedtls_camellia_context *ctx,
                               int mode,
                               size_t length,
                               unsigned char iv[16],
                               const unsigned char *input,
                               unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
/**
 * \brief          Perform a CAMELLIA-CFB128 buffer encryption/decryption
 *                 operation.
 *
 * \note           Due to the nature of CFB mode, you should use the same
 *                 key for both encryption and decryption. In particular, calls
 *                 to this function should be preceded by a key-schedule via
 *                 mbedtls_camellia_setkey_enc() regardless of whether \p mode
 *                 is #MBEDTLS_CAMELLIA_ENCRYPT or #MBEDTLS_CAMELLIA_DECRYPT.
 *
 * \note           Upon exit, the content of the IV is updated so that you can
 *                 call the function same function again on the following
 *                 block(s) of data and get the same result as if it was
 *                 encrypted in one call. This allows a "streaming" usage.
 *                 If on the other hand you need to retain the contents of the
 *                 IV, you should either save it manually or use the cipher
 *                 module instead.
 *
 * \param ctx      The CAMELLIA context to use. This must be initialized
 *                 and bound to a key.
 * \param mode     The mode of operation. This must be either
 *                 #MBEDTLS_CAMELLIA_ENCRYPT or #MBEDTLS_CAMELLIA_DECRYPT.
 * \param length   The length of the input data \p input. Any value is allowed.
 * \param iv_off   The current offset in the IV. This must be smaller
 *                 than \c 16 Bytes. It is updated after this call to allow
 *                 the aforementioned streaming usage.
 * \param iv       The initialization vector. This must be a read/write buffer
 *                 of length \c 16 Bytes. It is updated after this call to
 *                 allow the aforementioned streaming usage.
 * \param input    The buffer holding the input data. This must be a readable
 *                 buffer of size \p length Bytes.
 * \param output   The buffer to hold the output data. This must be a writable
 *                 buffer of length \p length Bytes.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_camellia_crypt_cfb128(mbedtls_camellia_context *ctx,
                                  int mode,
                                  size_t length,
                                  size_t *iv_off,
                                  unsigned char iv[16],
                                  const unsigned char *input,
                                  unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
/**
 * \brief      Perform a CAMELLIA-CTR buffer encryption/decryption operation.
 *
 * *note       Due to the nature of CTR mode, you should use the same
 *             key for both encryption and decryption. In particular, calls
 *             to this function should be preceded by a key-schedule via
 *             mbedtls_camellia_setkey_enc() regardless of whether the mode
 *             is #MBEDTLS_CAMELLIA_ENCRYPT or #MBEDTLS_CAMELLIA_DECRYPT.
 *
 * \warning    You must never reuse a nonce value with the same key. Doing so
 *             would void the encryption for the two messages encrypted with
 *             the same nonce and key.
 *
 *             There are two common strategies for managing nonces with CTR:
 *
 *             1. You can handle everything as a single message processed over
 *             successive calls to this function. In that case, you want to
 *             set \p nonce_counter and \p nc_off to 0 for the first call, and
 *             then preserve the values of \p nonce_counter, \p nc_off and \p
 *             stream_block across calls to this function as they will be
 *             updated by this function.
 *
 *             With this strategy, you must not encrypt more than 2**128
 *             blocks of data with the same key.
 *
 *             2. You can encrypt separate messages by dividing the \p
 *             nonce_counter buffer in two areas: the first one used for a
 *             per-message nonce, handled by yourself, and the second one
 *             updated by this function internally.
 *
 *             For example, you might reserve the first \c 12 Bytes for the
 *             per-message nonce, and the last \c 4 Bytes for internal use.
 *             In that case, before calling this function on a new message you
 *             need to set the first \c 12 Bytes of \p nonce_counter to your
 *             chosen nonce value, the last four to \c 0, and \p nc_off to \c 0
 *             (which will cause \p stream_block to be ignored). That way, you
 *             can encrypt at most \c 2**96 messages of up to \c 2**32 blocks
 *             each  with the same key.
 *
 *             The per-message nonce (or information sufficient to reconstruct
 *             it) needs to be communicated with the ciphertext and must be
 *             unique. The recommended way to ensure uniqueness is to use a
 *             message counter. An alternative is to generate random nonces,
 *             but this limits the number of messages that can be securely
 *             encrypted: for example, with 96-bit random nonces, you should
 *             not encrypt more than 2**32 messages with the same key.
 *
 *             Note that for both strategies, sizes are measured in blocks and
 *             that a CAMELLIA block is \c 16 Bytes.
 *
 * \warning    Upon return, \p stream_block contains sensitive data. Its
 *             content must not be written to insecure storage and should be
 *             securely discarded as soon as it's no longer needed.
 *
 * \param ctx           The CAMELLIA context to use. This must be initialized
 *                      and bound to a key.
 * \param length        The length of the input data \p input in Bytes.
 *                      Any value is allowed.
 * \param nc_off        The offset in the current \p stream_block (for resuming
 *                      within current cipher stream). The offset pointer to
 *                      should be \c 0 at the start of a stream. It is updated
 *                      at the end of this call.
 * \param nonce_counter The 128-bit nonce and counter. This must be a read/write
 *                      buffer of length \c 16 Bytes.
 * \param stream_block  The saved stream-block for resuming. This must be a
 *                      read/write buffer of length \c 16 Bytes.
 * \param input         The input data stream. This must be a readable buffer of
 *                      size \p length Bytes.
 * \param output        The output data stream. This must be a writable buffer
 *                      of size \p length Bytes.
 *
 * \return              \c 0 if successful.
 * \return              A negative error code on failure.
 */
int mbedtls_camellia_crypt_ctr(mbedtls_camellia_context *ctx,
                               size_t length,
                               size_t *nc_off,
                               unsigned char nonce_counter[16],
                               unsigned char stream_block[16],
                               const unsigned char *input,
                               unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if defined(MBEDTLS_SELF_TEST)

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int mbedtls_camellia_self_test(int verbose);

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* camellia.h */

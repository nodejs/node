/**
 * \file aes.h
 *
 * \brief   This file contains AES definitions and functions.
 *
 *          The Advanced Encryption Standard (AES) specifies a FIPS-approved
 *          cryptographic algorithm that can be used to protect electronic
 *          data.
 *
 *          The AES algorithm is a symmetric block cipher that can
 *          encrypt and decrypt information. For more information, see
 *          <em>FIPS Publication 197: Advanced Encryption Standard</em> and
 *          <em>ISO/IEC 18033-2:2006: Information technology -- Security
 *          techniques -- Encryption algorithms -- Part 2: Asymmetric
 *          ciphers</em>.
 *
 *          The AES-XTS block mode is standardized by NIST SP 800-38E
 *          <https://nvlpubs.nist.gov/nistpubs/legacy/sp/nistspecialpublication800-38e.pdf>
 *          and described in detail by IEEE P1619
 *          <https://ieeexplore.ieee.org/servlet/opac?punumber=4375278>.
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_AES_H
#define MBEDTLS_AES_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"
#include "mbedtls/platform_util.h"

#include <stddef.h>
#include <stdint.h>

/* padlock.c and aesni.c rely on these values! */
#define MBEDTLS_AES_ENCRYPT     1 /**< AES encryption. */
#define MBEDTLS_AES_DECRYPT     0 /**< AES decryption. */

/* Error codes in range 0x0020-0x0022 */
/** Invalid key length. */
#define MBEDTLS_ERR_AES_INVALID_KEY_LENGTH                -0x0020
/** Invalid data input length. */
#define MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH              -0x0022

/* Error codes in range 0x0021-0x0025 */
/** Invalid input data. */
#define MBEDTLS_ERR_AES_BAD_INPUT_DATA                    -0x0021

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_AES_ALT)
// Regular implementation
//

/**
 * \brief The AES context-type definition.
 */
typedef struct mbedtls_aes_context {
    int MBEDTLS_PRIVATE(nr);                     /*!< The number of rounds. */
    size_t MBEDTLS_PRIVATE(rk_offset);           /*!< The offset in array elements to AES
                                                    round keys in the buffer. */
#if defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH) && !defined(MBEDTLS_PADLOCK_C)
    uint32_t MBEDTLS_PRIVATE(buf)[44];           /*!< Aligned data buffer to hold
                                                    10 round keys for 128-bit case. */
#else
    uint32_t MBEDTLS_PRIVATE(buf)[68];           /*!< Unaligned data buffer. This buffer can
                                                    hold 32 extra Bytes, which can be used for
                                                    one of the following purposes:
                                                    <ul><li>Alignment if VIA padlock is
                                                    used.</li>
                                                    <li>Simplifying key expansion in the 256-bit
                                                    case by generating an extra round key.
                                                    </li></ul> */
#endif /* MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH && !MBEDTLS_PADLOCK_C */
}
mbedtls_aes_context;

#if defined(MBEDTLS_CIPHER_MODE_XTS)
/**
 * \brief The AES XTS context-type definition.
 */
typedef struct mbedtls_aes_xts_context {
    mbedtls_aes_context MBEDTLS_PRIVATE(crypt); /*!< The AES context to use for AES block
                                                   encryption or decryption. */
    mbedtls_aes_context MBEDTLS_PRIVATE(tweak); /*!< The AES context used for tweak
                                                   computation. */
} mbedtls_aes_xts_context;
#endif /* MBEDTLS_CIPHER_MODE_XTS */

#else  /* MBEDTLS_AES_ALT */
#include "aes_alt.h"
#endif /* MBEDTLS_AES_ALT */

/**
 * \brief          This function initializes the specified AES context.
 *
 *                 It must be the first API called before using
 *                 the context.
 *
 * \param ctx      The AES context to initialize. This must not be \c NULL.
 */
void mbedtls_aes_init(mbedtls_aes_context *ctx);

/**
 * \brief          This function releases and clears the specified AES context.
 *
 * \param ctx      The AES context to clear.
 *                 If this is \c NULL, this function does nothing.
 *                 Otherwise, the context must have been at least initialized.
 */
void mbedtls_aes_free(mbedtls_aes_context *ctx);

#if defined(MBEDTLS_CIPHER_MODE_XTS)
/**
 * \brief          This function initializes the specified AES XTS context.
 *
 *                 It must be the first API called before using
 *                 the context.
 *
 * \param ctx      The AES XTS context to initialize. This must not be \c NULL.
 */
void mbedtls_aes_xts_init(mbedtls_aes_xts_context *ctx);

/**
 * \brief          This function releases and clears the specified AES XTS context.
 *
 * \param ctx      The AES XTS context to clear.
 *                 If this is \c NULL, this function does nothing.
 *                 Otherwise, the context must have been at least initialized.
 */
void mbedtls_aes_xts_free(mbedtls_aes_xts_context *ctx);
#endif /* MBEDTLS_CIPHER_MODE_XTS */

/**
 * \brief          This function sets the encryption key.
 *
 * \param ctx      The AES context to which the key should be bound.
 *                 It must be initialized.
 * \param key      The encryption key.
 *                 This must be a readable buffer of size \p keybits bits.
 * \param keybits  The size of data passed in bits. Valid options are:
 *                 <ul><li>128 bits</li>
 *                 <li>192 bits</li>
 *                 <li>256 bits</li></ul>
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH on failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx, const unsigned char *key,
                           unsigned int keybits);

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
/**
 * \brief          This function sets the decryption key.
 *
 * \param ctx      The AES context to which the key should be bound.
 *                 It must be initialized.
 * \param key      The decryption key.
 *                 This must be a readable buffer of size \p keybits bits.
 * \param keybits  The size of data passed. Valid options are:
 *                 <ul><li>128 bits</li>
 *                 <li>192 bits</li>
 *                 <li>256 bits</li></ul>
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH on failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_setkey_dec(mbedtls_aes_context *ctx, const unsigned char *key,
                           unsigned int keybits);
#endif /* !MBEDTLS_BLOCK_CIPHER_NO_DECRYPT */

#if defined(MBEDTLS_CIPHER_MODE_XTS)
/**
 * \brief          This function prepares an XTS context for encryption and
 *                 sets the encryption key.
 *
 * \param ctx      The AES XTS context to which the key should be bound.
 *                 It must be initialized.
 * \param key      The encryption key. This is comprised of the XTS key1
 *                 concatenated with the XTS key2.
 *                 This must be a readable buffer of size \p keybits bits.
 * \param keybits  The size of \p key passed in bits. Valid options are:
 *                 <ul><li>256 bits (each of key1 and key2 is a 128-bit key)</li>
 *                 <li>512 bits (each of key1 and key2 is a 256-bit key)</li></ul>
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH on failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_xts_setkey_enc(mbedtls_aes_xts_context *ctx,
                               const unsigned char *key,
                               unsigned int keybits);

/**
 * \brief          This function prepares an XTS context for decryption and
 *                 sets the decryption key.
 *
 * \param ctx      The AES XTS context to which the key should be bound.
 *                 It must be initialized.
 * \param key      The decryption key. This is comprised of the XTS key1
 *                 concatenated with the XTS key2.
 *                 This must be a readable buffer of size \p keybits bits.
 * \param keybits  The size of \p key passed in bits. Valid options are:
 *                 <ul><li>256 bits (each of key1 and key2 is a 128-bit key)</li>
 *                 <li>512 bits (each of key1 and key2 is a 256-bit key)</li></ul>
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH on failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_xts_setkey_dec(mbedtls_aes_xts_context *ctx,
                               const unsigned char *key,
                               unsigned int keybits);
#endif /* MBEDTLS_CIPHER_MODE_XTS */

/**
 * \brief          This function performs an AES single-block encryption or
 *                 decryption operation.
 *
 *                 It performs the operation defined in the \p mode parameter
 *                 (encrypt or decrypt), on the input data buffer defined in
 *                 the \p input parameter.
 *
 *                 mbedtls_aes_init(), and either mbedtls_aes_setkey_enc() or
 *                 mbedtls_aes_setkey_dec() must be called before the first
 *                 call to this API with the same context.
 *
 * \param ctx      The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * \param mode     The AES operation: #MBEDTLS_AES_ENCRYPT or
 *                 #MBEDTLS_AES_DECRYPT.
 * \param input    The buffer holding the input data.
 *                 It must be readable and at least \c 16 Bytes long.
 * \param output   The buffer where the output data will be written.
 *                 It must be writeable and at least \c 16 Bytes long.

 * \return         \c 0 on success.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx,
                          int mode,
                          const unsigned char input[16],
                          unsigned char output[16]);

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/**
 * \brief  This function performs an AES-CBC encryption or decryption operation
 *         on full blocks.
 *
 *         It performs the operation defined in the \p mode
 *         parameter (encrypt/decrypt), on the input data buffer defined in
 *         the \p input parameter.
 *
 *         It can be called as many times as needed, until all the input
 *         data is processed. mbedtls_aes_init(), and either
 *         mbedtls_aes_setkey_enc() or mbedtls_aes_setkey_dec() must be called
 *         before the first call to this API with the same context.
 *
 * \note   This function operates on full blocks, that is, the input size
 *         must be a multiple of the AES block size of \c 16 Bytes.
 *
 * \note   Upon exit, the content of the IV is updated so that you can
 *         call the same function again on the next
 *         block(s) of data and get the same result as if it was
 *         encrypted in one call. This allows a "streaming" usage.
 *         If you need to retain the contents of the IV, you should
 *         either save it manually or use the cipher module instead.
 *
 *
 * \param ctx      The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * \param mode     The AES operation: #MBEDTLS_AES_ENCRYPT or
 *                 #MBEDTLS_AES_DECRYPT.
 * \param length   The length of the input data in Bytes. This must be a
 *                 multiple of the block size (\c 16 Bytes).
 * \param iv       Initialization vector (updated after use).
 *                 It must be a readable and writeable buffer of \c 16 Bytes.
 * \param input    The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * \param output   The buffer holding the output data.
 *                 It must be writeable and of size \p length Bytes.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH
 *                 on failure.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_crypt_cbc(mbedtls_aes_context *ctx,
                          int mode,
                          size_t length,
                          unsigned char iv[16],
                          const unsigned char *input,
                          unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_XTS)
/**
 * \brief      This function performs an AES-XTS encryption or decryption
 *             operation for an entire XTS data unit.
 *
 *             AES-XTS encrypts or decrypts blocks based on their location as
 *             defined by a data unit number. The data unit number must be
 *             provided by \p data_unit.
 *
 *             NIST SP 800-38E limits the maximum size of a data unit to 2^20
 *             AES blocks. If the data unit is larger than this, this function
 *             returns #MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH.
 *
 * \param ctx          The AES XTS context to use for AES XTS operations.
 *                     It must be initialized and bound to a key.
 * \param mode         The AES operation: #MBEDTLS_AES_ENCRYPT or
 *                     #MBEDTLS_AES_DECRYPT.
 * \param length       The length of a data unit in Bytes. This can be any
 *                     length between 16 bytes and 2^24 bytes inclusive
 *                     (between 1 and 2^20 block cipher blocks).
 * \param data_unit    The address of the data unit encoded as an array of 16
 *                     bytes in little-endian format. For disk encryption, this
 *                     is typically the index of the block device sector that
 *                     contains the data.
 * \param input        The buffer holding the input data (which is an entire
 *                     data unit). This function reads \p length Bytes from \p
 *                     input.
 * \param output       The buffer holding the output data (which is an entire
 *                     data unit). This function writes \p length Bytes to \p
 *                     output.
 *
 * \return             \c 0 on success.
 * \return             #MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH if \p length is
 *                     smaller than an AES block in size (16 Bytes) or if \p
 *                     length is larger than 2^20 blocks (16 MiB).
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_crypt_xts(mbedtls_aes_xts_context *ctx,
                          int mode,
                          size_t length,
                          const unsigned char data_unit[16],
                          const unsigned char *input,
                          unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_XTS */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
/**
 * \brief This function performs an AES-CFB128 encryption or decryption
 *        operation.
 *
 *        It performs the operation defined in the \p mode
 *        parameter (encrypt or decrypt), on the input data buffer
 *        defined in the \p input parameter.
 *
 *        For CFB, you must set up the context with mbedtls_aes_setkey_enc(),
 *        regardless of whether you are performing an encryption or decryption
 *        operation, that is, regardless of the \p mode parameter. This is
 *        because CFB mode uses the same key schedule for encryption and
 *        decryption.
 *
 * \note  Upon exit, the content of the IV is updated so that you can
 *        call the same function again on the next
 *        block(s) of data and get the same result as if it was
 *        encrypted in one call. This allows a "streaming" usage.
 *        If you need to retain the contents of the
 *        IV, you must either save it manually or use the cipher
 *        module instead.
 *
 *
 * \param ctx      The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * \param mode     The AES operation: #MBEDTLS_AES_ENCRYPT or
 *                 #MBEDTLS_AES_DECRYPT.
 * \param length   The length of the input data in Bytes.
 * \param iv_off   The offset in IV (updated after use).
 *                 It must point to a valid \c size_t.
 * \param iv       The initialization vector (updated after use).
 *                 It must be a readable and writeable buffer of \c 16 Bytes.
 * \param input    The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * \param output   The buffer holding the output data.
 *                 It must be writeable and of size \p length Bytes.
 *
 * \return         \c 0 on success.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_crypt_cfb128(mbedtls_aes_context *ctx,
                             int mode,
                             size_t length,
                             size_t *iv_off,
                             unsigned char iv[16],
                             const unsigned char *input,
                             unsigned char *output);

/**
 * \brief This function performs an AES-CFB8 encryption or decryption
 *        operation.
 *
 *        It performs the operation defined in the \p mode
 *        parameter (encrypt/decrypt), on the input data buffer defined
 *        in the \p input parameter.
 *
 *        Due to the nature of CFB, you must use the same key schedule for
 *        both encryption and decryption operations. Therefore, you must
 *        use the context initialized with mbedtls_aes_setkey_enc() for
 *        both #MBEDTLS_AES_ENCRYPT and #MBEDTLS_AES_DECRYPT.
 *
 * \note  Upon exit, the content of the IV is updated so that you can
 *        call the same function again on the next
 *        block(s) of data and get the same result as if it was
 *        encrypted in one call. This allows a "streaming" usage.
 *        If you need to retain the contents of the
 *        IV, you should either save it manually or use the cipher
 *        module instead.
 *
 *
 * \param ctx      The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * \param mode     The AES operation: #MBEDTLS_AES_ENCRYPT or
 *                 #MBEDTLS_AES_DECRYPT
 * \param length   The length of the input data.
 * \param iv       The initialization vector (updated after use).
 *                 It must be a readable and writeable buffer of \c 16 Bytes.
 * \param input    The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * \param output   The buffer holding the output data.
 *                 It must be writeable and of size \p length Bytes.
 *
 * \return         \c 0 on success.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_crypt_cfb8(mbedtls_aes_context *ctx,
                           int mode,
                           size_t length,
                           unsigned char iv[16],
                           const unsigned char *input,
                           unsigned char *output);
#endif /*MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_OFB)
/**
 * \brief       This function performs an AES-OFB (Output Feedback Mode)
 *              encryption or decryption operation.
 *
 *              For OFB, you must set up the context with
 *              mbedtls_aes_setkey_enc(), regardless of whether you are
 *              performing an encryption or decryption operation. This is
 *              because OFB mode uses the same key schedule for encryption and
 *              decryption.
 *
 *              The OFB operation is identical for encryption or decryption,
 *              therefore no operation mode needs to be specified.
 *
 * \note        Upon exit, the content of iv, the Initialisation Vector, is
 *              updated so that you can call the same function again on the next
 *              block(s) of data and get the same result as if it was encrypted
 *              in one call. This allows a "streaming" usage, by initialising
 *              iv_off to 0 before the first call, and preserving its value
 *              between calls.
 *
 *              For non-streaming use, the iv should be initialised on each call
 *              to a unique value, and iv_off set to 0 on each call.
 *
 *              If you need to retain the contents of the initialisation vector,
 *              you must either save it manually or use the cipher module
 *              instead.
 *
 * \warning     For the OFB mode, the initialisation vector must be unique
 *              every encryption operation. Reuse of an initialisation vector
 *              will compromise security.
 *
 * \param ctx      The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * \param length   The length of the input data.
 * \param iv_off   The offset in IV (updated after use).
 *                 It must point to a valid \c size_t.
 * \param iv       The initialization vector (updated after use).
 *                 It must be a readable and writeable buffer of \c 16 Bytes.
 * \param input    The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * \param output   The buffer holding the output data.
 *                 It must be writeable and of size \p length Bytes.
 *
 * \return         \c 0 on success.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_crypt_ofb(mbedtls_aes_context *ctx,
                          size_t length,
                          size_t *iv_off,
                          unsigned char iv[16],
                          const unsigned char *input,
                          unsigned char *output);

#endif /* MBEDTLS_CIPHER_MODE_OFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
/**
 * \brief      This function performs an AES-CTR encryption or decryption
 *             operation.
 *
 *             Due to the nature of CTR, you must use the same key schedule
 *             for both encryption and decryption operations. Therefore, you
 *             must use the context initialized with mbedtls_aes_setkey_enc()
 *             for both #MBEDTLS_AES_ENCRYPT and #MBEDTLS_AES_DECRYPT.
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
 *             For example, you might reserve the first 12 bytes for the
 *             per-message nonce, and the last 4 bytes for internal use. In that
 *             case, before calling this function on a new message you need to
 *             set the first 12 bytes of \p nonce_counter to your chosen nonce
 *             value, the last 4 to 0, and \p nc_off to 0 (which will cause \p
 *             stream_block to be ignored). That way, you can encrypt at most
 *             2**96 messages of up to 2**32 blocks each with the same key.
 *
 *             The per-message nonce (or information sufficient to reconstruct
 *             it) needs to be communicated with the ciphertext and must be unique.
 *             The recommended way to ensure uniqueness is to use a message
 *             counter. An alternative is to generate random nonces, but this
 *             limits the number of messages that can be securely encrypted:
 *             for example, with 96-bit random nonces, you should not encrypt
 *             more than 2**32 messages with the same key.
 *
 *             Note that for both strategies, sizes are measured in blocks and
 *             that an AES block is 16 bytes.
 *
 * \warning    Upon return, \p stream_block contains sensitive data. Its
 *             content must not be written to insecure storage and should be
 *             securely discarded as soon as it's no longer needed.
 *
 * \param ctx              The AES context to use for encryption or decryption.
 *                         It must be initialized and bound to a key.
 * \param length           The length of the input data.
 * \param nc_off           The offset in the current \p stream_block, for
 *                         resuming within the current cipher stream. The
 *                         offset pointer should be 0 at the start of a stream.
 *                         It must point to a valid \c size_t.
 * \param nonce_counter    The 128-bit nonce and counter.
 *                         It must be a readable-writeable buffer of \c 16 Bytes.
 * \param stream_block     The saved stream block for resuming. This is
 *                         overwritten by the function.
 *                         It must be a readable-writeable buffer of \c 16 Bytes.
 * \param input            The buffer holding the input data.
 *                         It must be readable and of size \p length Bytes.
 * \param output           The buffer holding the output data.
 *                         It must be writeable and of size \p length Bytes.
 *
 * \return                 \c 0 on success.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_aes_crypt_ctr(mbedtls_aes_context *ctx,
                          size_t length,
                          size_t *nc_off,
                          unsigned char nonce_counter[16],
                          unsigned char stream_block[16],
                          const unsigned char *input,
                          unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_CTR */

/**
 * \brief           Internal AES block encryption function. This is only
 *                  exposed to allow overriding it using
 *                  \c MBEDTLS_AES_ENCRYPT_ALT.
 *
 * \param ctx       The AES context to use for encryption.
 * \param input     The plaintext block.
 * \param output    The output (ciphertext) block.
 *
 * \return          \c 0 on success.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_internal_aes_encrypt(mbedtls_aes_context *ctx,
                                 const unsigned char input[16],
                                 unsigned char output[16]);

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
/**
 * \brief           Internal AES block decryption function. This is only
 *                  exposed to allow overriding it using see
 *                  \c MBEDTLS_AES_DECRYPT_ALT.
 *
 * \param ctx       The AES context to use for decryption.
 * \param input     The ciphertext block.
 * \param output    The output (plaintext) block.
 *
 * \return          \c 0 on success.
 */
MBEDTLS_CHECK_RETURN_TYPICAL
int mbedtls_internal_aes_decrypt(mbedtls_aes_context *ctx,
                                 const unsigned char input[16],
                                 unsigned char output[16]);
#endif /* !MBEDTLS_BLOCK_CIPHER_NO_DECRYPT */

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief          Checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_aes_self_test(int verbose);

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* aes.h */

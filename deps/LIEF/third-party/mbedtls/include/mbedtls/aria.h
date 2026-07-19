/**
 * \file aria.h
 *
 * \brief ARIA block cipher
 *
 *        The ARIA algorithm is a symmetric block cipher that can encrypt and
 *        decrypt information. It is defined by the Korean Agency for
 *        Technology and Standards (KATS) in <em>KS X 1213:2004</em> (in
 *        Korean, but see http://210.104.33.10/ARIA/index-e.html in English)
 *        and also described by the IETF in <em>RFC 5794</em>.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_ARIA_H
#define MBEDTLS_ARIA_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stddef.h>
#include <stdint.h>

#include "mbedtls/platform_util.h"

#define MBEDTLS_ARIA_ENCRYPT     1 /**< ARIA encryption. */
#define MBEDTLS_ARIA_DECRYPT     0 /**< ARIA decryption. */

#define MBEDTLS_ARIA_BLOCKSIZE   16 /**< ARIA block size in bytes. */
#define MBEDTLS_ARIA_MAX_ROUNDS  16 /**< Maximum number of rounds in ARIA. */
#define MBEDTLS_ARIA_MAX_KEYSIZE 32 /**< Maximum size of an ARIA key in bytes. */

/** Bad input data. */
#define MBEDTLS_ERR_ARIA_BAD_INPUT_DATA -0x005C

/** Invalid data input length. */
#define MBEDTLS_ERR_ARIA_INVALID_INPUT_LENGTH -0x005E

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_ARIA_ALT)
// Regular implementation
//

/**
 * \brief The ARIA context-type definition.
 */
typedef struct mbedtls_aria_context {
    unsigned char MBEDTLS_PRIVATE(nr);           /*!< The number of rounds (12, 14 or 16) */
    /*! The ARIA round keys. */
    uint32_t MBEDTLS_PRIVATE(rk)[MBEDTLS_ARIA_MAX_ROUNDS + 1][MBEDTLS_ARIA_BLOCKSIZE / 4];
}
mbedtls_aria_context;

#else  /* MBEDTLS_ARIA_ALT */
#include "aria_alt.h"
#endif /* MBEDTLS_ARIA_ALT */

/**
 * \brief          This function initializes the specified ARIA context.
 *
 *                 It must be the first API called before using
 *                 the context.
 *
 * \param ctx      The ARIA context to initialize. This must not be \c NULL.
 */
void mbedtls_aria_init(mbedtls_aria_context *ctx);

/**
 * \brief          This function releases and clears the specified ARIA context.
 *
 * \param ctx      The ARIA context to clear. This may be \c NULL, in which
 *                 case this function returns immediately. If it is not \c NULL,
 *                 it must point to an initialized ARIA context.
 */
void mbedtls_aria_free(mbedtls_aria_context *ctx);

/**
 * \brief          This function sets the encryption key.
 *
 * \param ctx      The ARIA context to which the key should be bound.
 *                 This must be initialized.
 * \param key      The encryption key. This must be a readable buffer
 *                 of size \p keybits Bits.
 * \param keybits  The size of \p key in Bits. Valid options are:
 *                 <ul><li>128 bits</li>
 *                 <li>192 bits</li>
 *                 <li>256 bits</li></ul>
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_aria_setkey_enc(mbedtls_aria_context *ctx,
                            const unsigned char *key,
                            unsigned int keybits);

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
/**
 * \brief          This function sets the decryption key.
 *
 * \param ctx      The ARIA context to which the key should be bound.
 *                 This must be initialized.
 * \param key      The decryption key. This must be a readable buffer
 *                 of size \p keybits Bits.
 * \param keybits  The size of data passed. Valid options are:
 *                 <ul><li>128 bits</li>
 *                 <li>192 bits</li>
 *                 <li>256 bits</li></ul>
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_aria_setkey_dec(mbedtls_aria_context *ctx,
                            const unsigned char *key,
                            unsigned int keybits);
#endif /* !MBEDTLS_BLOCK_CIPHER_NO_DECRYPT */

/**
 * \brief          This function performs an ARIA single-block encryption or
 *                 decryption operation.
 *
 *                 It performs encryption or decryption (depending on whether
 *                 the key was set for encryption on decryption) on the input
 *                 data buffer defined in the \p input parameter.
 *
 *                 mbedtls_aria_init(), and either mbedtls_aria_setkey_enc() or
 *                 mbedtls_aria_setkey_dec() must be called before the first
 *                 call to this API with the same context.
 *
 * \param ctx      The ARIA context to use for encryption or decryption.
 *                 This must be initialized and bound to a key.
 * \param input    The 16-Byte buffer holding the input data.
 * \param output   The 16-Byte buffer holding the output data.

 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_aria_crypt_ecb(mbedtls_aria_context *ctx,
                           const unsigned char input[MBEDTLS_ARIA_BLOCKSIZE],
                           unsigned char output[MBEDTLS_ARIA_BLOCKSIZE]);

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/**
 * \brief  This function performs an ARIA-CBC encryption or decryption operation
 *         on full blocks.
 *
 *         It performs the operation defined in the \p mode
 *         parameter (encrypt/decrypt), on the input data buffer defined in
 *         the \p input parameter.
 *
 *         It can be called as many times as needed, until all the input
 *         data is processed. mbedtls_aria_init(), and either
 *         mbedtls_aria_setkey_enc() or mbedtls_aria_setkey_dec() must be called
 *         before the first call to this API with the same context.
 *
 * \note   This function operates on aligned blocks, that is, the input size
 *         must be a multiple of the ARIA block size of 16 Bytes.
 *
 * \note   Upon exit, the content of the IV is updated so that you can
 *         call the same function again on the next
 *         block(s) of data and get the same result as if it was
 *         encrypted in one call. This allows a "streaming" usage.
 *         If you need to retain the contents of the IV, you should
 *         either save it manually or use the cipher module instead.
 *
 *
 * \param ctx      The ARIA context to use for encryption or decryption.
 *                 This must be initialized and bound to a key.
 * \param mode     The mode of operation. This must be either
 *                 #MBEDTLS_ARIA_ENCRYPT for encryption, or
 *                 #MBEDTLS_ARIA_DECRYPT for decryption.
 * \param length   The length of the input data in Bytes. This must be a
 *                 multiple of the block size (16 Bytes).
 * \param iv       Initialization vector (updated after use).
 *                 This must be a readable buffer of size 16 Bytes.
 * \param input    The buffer holding the input data. This must
 *                 be a readable buffer of length \p length Bytes.
 * \param output   The buffer holding the output data. This must
 *                 be a writable buffer of length \p length Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_aria_crypt_cbc(mbedtls_aria_context *ctx,
                           int mode,
                           size_t length,
                           unsigned char iv[MBEDTLS_ARIA_BLOCKSIZE],
                           const unsigned char *input,
                           unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
/**
 * \brief This function performs an ARIA-CFB128 encryption or decryption
 *        operation.
 *
 *        It performs the operation defined in the \p mode
 *        parameter (encrypt or decrypt), on the input data buffer
 *        defined in the \p input parameter.
 *
 *        For CFB, you must set up the context with mbedtls_aria_setkey_enc(),
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
 * \param ctx      The ARIA context to use for encryption or decryption.
 *                 This must be initialized and bound to a key.
 * \param mode     The mode of operation. This must be either
 *                 #MBEDTLS_ARIA_ENCRYPT for encryption, or
 *                 #MBEDTLS_ARIA_DECRYPT for decryption.
 * \param length   The length of the input data \p input in Bytes.
 * \param iv_off   The offset in IV (updated after use).
 *                 This must not be larger than 15.
 * \param iv       The initialization vector (updated after use).
 *                 This must be a readable buffer of size 16 Bytes.
 * \param input    The buffer holding the input data. This must
 *                 be a readable buffer of length \p length Bytes.
 * \param output   The buffer holding the output data. This must
 *                 be a writable buffer of length \p length Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_aria_crypt_cfb128(mbedtls_aria_context *ctx,
                              int mode,
                              size_t length,
                              size_t *iv_off,
                              unsigned char iv[MBEDTLS_ARIA_BLOCKSIZE],
                              const unsigned char *input,
                              unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
/**
 * \brief      This function performs an ARIA-CTR encryption or decryption
 *             operation.
 *
 *             Due to the nature of CTR, you must use the same key schedule
 *             for both encryption and decryption operations. Therefore, you
 *             must use the context initialized with mbedtls_aria_setkey_enc()
 *             for both #MBEDTLS_ARIA_ENCRYPT and #MBEDTLS_ARIA_DECRYPT.
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
 *             that an ARIA block is 16 bytes.
 *
 * \warning    Upon return, \p stream_block contains sensitive data. Its
 *             content must not be written to insecure storage and should be
 *             securely discarded as soon as it's no longer needed.
 *
 * \param ctx              The ARIA context to use for encryption or decryption.
 *                         This must be initialized and bound to a key.
 * \param length           The length of the input data \p input in Bytes.
 * \param nc_off           The offset in Bytes in the current \p stream_block,
 *                         for resuming within the current cipher stream. The
 *                         offset pointer should be \c 0 at the start of a
 *                         stream. This must not be larger than \c 15 Bytes.
 * \param nonce_counter    The 128-bit nonce and counter. This must point to
 *                         a read/write buffer of length \c 16 bytes.
 * \param stream_block     The saved stream block for resuming. This must
 *                         point to a read/write buffer of length \c 16 bytes.
 *                         This is overwritten by the function.
 * \param input            The buffer holding the input data. This must
 *                         be a readable buffer of length \p length Bytes.
 * \param output           The buffer holding the output data. This must
 *                         be a writable buffer of length \p length Bytes.
 *
 * \return                 \c 0 on success.
 * \return                 A negative error code on failure.
 */
int mbedtls_aria_crypt_ctr(mbedtls_aria_context *ctx,
                           size_t length,
                           size_t *nc_off,
                           unsigned char nonce_counter[MBEDTLS_ARIA_BLOCKSIZE],
                           unsigned char stream_block[MBEDTLS_ARIA_BLOCKSIZE],
                           const unsigned char *input,
                           unsigned char *output);
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief          Checkup routine.
 *
 * \return         \c 0 on success, or \c 1 on failure.
 */
int mbedtls_aria_self_test(int verbose);
#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* aria.h */

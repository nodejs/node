/**
 * \file chachapoly.h
 *
 * \brief   This file contains the AEAD-ChaCha20-Poly1305 definitions and
 *          functions.
 *
 *          ChaCha20-Poly1305 is an algorithm for Authenticated Encryption
 *          with Associated Data (AEAD) that can be used to encrypt and
 *          authenticate data. It is based on ChaCha20 and Poly1305 by Daniel
 *          Bernstein and was standardized in RFC 7539.
 *
 * \author Daniel King <damaki.gh@gmail.com>
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CHACHAPOLY_H
#define MBEDTLS_CHACHAPOLY_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

/* for shared error codes */
#include "mbedtls/poly1305.h"

/** The requested operation is not permitted in the current state. */
#define MBEDTLS_ERR_CHACHAPOLY_BAD_STATE            -0x0054
/** Authenticated decryption failed: data was not authentic. */
#define MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED          -0x0056

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MBEDTLS_CHACHAPOLY_ENCRYPT,     /**< The mode value for performing encryption. */
    MBEDTLS_CHACHAPOLY_DECRYPT      /**< The mode value for performing decryption. */
}
mbedtls_chachapoly_mode_t;

#if !defined(MBEDTLS_CHACHAPOLY_ALT)

#include "mbedtls/chacha20.h"

typedef struct mbedtls_chachapoly_context {
    mbedtls_chacha20_context MBEDTLS_PRIVATE(chacha20_ctx);  /**< The ChaCha20 context. */
    mbedtls_poly1305_context MBEDTLS_PRIVATE(poly1305_ctx);  /**< The Poly1305 context. */
    uint64_t MBEDTLS_PRIVATE(aad_len);                       /**< The length (bytes) of the Additional Authenticated Data. */
    uint64_t MBEDTLS_PRIVATE(ciphertext_len);                /**< The length (bytes) of the ciphertext. */
    int MBEDTLS_PRIVATE(state);                              /**< The current state of the context. */
    mbedtls_chachapoly_mode_t MBEDTLS_PRIVATE(mode);         /**< Cipher mode (encrypt or decrypt). */
}
mbedtls_chachapoly_context;

#else /* !MBEDTLS_CHACHAPOLY_ALT */
#include "chachapoly_alt.h"
#endif /* !MBEDTLS_CHACHAPOLY_ALT */

/**
 * \brief           This function initializes the specified ChaCha20-Poly1305 context.
 *
 *                  It must be the first API called before using
 *                  the context. It must be followed by a call to
 *                  \c mbedtls_chachapoly_setkey() before any operation can be
 *                  done, and to \c mbedtls_chachapoly_free() once all
 *                  operations with that context have been finished.
 *
 *                  In order to encrypt or decrypt full messages at once, for
 *                  each message you should make a single call to
 *                  \c mbedtls_chachapoly_crypt_and_tag() or
 *                  \c mbedtls_chachapoly_auth_decrypt().
 *
 *                  In order to encrypt messages piecewise, for each
 *                  message you should make a call to
 *                  \c mbedtls_chachapoly_starts(), then 0 or more calls to
 *                  \c mbedtls_chachapoly_update_aad(), then 0 or more calls to
 *                  \c mbedtls_chachapoly_update(), then one call to
 *                  \c mbedtls_chachapoly_finish().
 *
 * \warning         Decryption with the piecewise API is discouraged! Always
 *                  use \c mbedtls_chachapoly_auth_decrypt() when possible!
 *
 *                  If however this is not possible because the data is too
 *                  large to fit in memory, you need to:
 *
 *                  - call \c mbedtls_chachapoly_starts() and (if needed)
 *                  \c mbedtls_chachapoly_update_aad() as above,
 *                  - call \c mbedtls_chachapoly_update() multiple times and
 *                  ensure its output (the plaintext) is NOT used in any other
 *                  way than placing it in temporary storage at this point,
 *                  - call \c mbedtls_chachapoly_finish() to compute the
 *                  authentication tag and compared it in constant time to the
 *                  tag received with the ciphertext.
 *
 *                  If the tags are not equal, you must immediately discard
 *                  all previous outputs of \c mbedtls_chachapoly_update(),
 *                  otherwise you can now safely use the plaintext.
 *
 * \param ctx       The ChachaPoly context to initialize. Must not be \c NULL.
 */
void mbedtls_chachapoly_init(mbedtls_chachapoly_context *ctx);

/**
 * \brief           This function releases and clears the specified
 *                  ChaCha20-Poly1305 context.
 *
 * \param ctx       The ChachaPoly context to clear. This may be \c NULL, in which
 *                  case this function is a no-op.
 */
void mbedtls_chachapoly_free(mbedtls_chachapoly_context *ctx);

/**
 * \brief           This function sets the ChaCha20-Poly1305
 *                  symmetric encryption key.
 *
 * \param ctx       The ChaCha20-Poly1305 context to which the key should be
 *                  bound. This must be initialized.
 * \param key       The \c 256 Bit (\c 32 Bytes) key.
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_chachapoly_setkey(mbedtls_chachapoly_context *ctx,
                              const unsigned char key[32]);

/**
 * \brief           This function starts a ChaCha20-Poly1305 encryption or
 *                  decryption operation.
 *
 * \warning         You must never use the same nonce twice with the same key.
 *                  This would void any confidentiality and authenticity
 *                  guarantees for the messages encrypted with the same nonce
 *                  and key.
 *
 * \note            If the context is being used for AAD only (no data to
 *                  encrypt or decrypt) then \p mode can be set to any value.
 *
 * \warning         Decryption with the piecewise API is discouraged, see the
 *                  warning on \c mbedtls_chachapoly_init().
 *
 * \param ctx       The ChaCha20-Poly1305 context. This must be initialized
 *                  and bound to a key.
 * \param nonce     The nonce/IV to use for the message.
 *                  This must be a readable buffer of length \c 12 Bytes.
 * \param mode      The operation to perform: #MBEDTLS_CHACHAPOLY_ENCRYPT or
 *                  #MBEDTLS_CHACHAPOLY_DECRYPT (discouraged, see warning).
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_chachapoly_starts(mbedtls_chachapoly_context *ctx,
                              const unsigned char nonce[12],
                              mbedtls_chachapoly_mode_t mode);

/**
 * \brief           This function feeds additional data to be authenticated
 *                  into an ongoing ChaCha20-Poly1305 operation.
 *
 *                  The Additional Authenticated Data (AAD), also called
 *                  Associated Data (AD) is only authenticated but not
 *                  encrypted nor included in the encrypted output. It is
 *                  usually transmitted separately from the ciphertext or
 *                  computed locally by each party.
 *
 * \note            This function is called before data is encrypted/decrypted.
 *                  I.e. call this function to process the AAD before calling
 *                  \c mbedtls_chachapoly_update().
 *
 *                  You may call this function multiple times to process
 *                  an arbitrary amount of AAD. It is permitted to call
 *                  this function 0 times, if no AAD is used.
 *
 *                  This function cannot be called any more if data has
 *                  been processed by \c mbedtls_chachapoly_update(),
 *                  or if the context has been finished.
 *
 * \warning         Decryption with the piecewise API is discouraged, see the
 *                  warning on \c mbedtls_chachapoly_init().
 *
 * \param ctx       The ChaCha20-Poly1305 context. This must be initialized
 *                  and bound to a key.
 * \param aad_len   The length in Bytes of the AAD. The length has no
 *                  restrictions.
 * \param aad       Buffer containing the AAD.
 *                  This pointer can be \c NULL if `aad_len == 0`.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_POLY1305_BAD_INPUT_DATA
 *                  if \p ctx or \p aad are NULL.
 * \return          #MBEDTLS_ERR_CHACHAPOLY_BAD_STATE
 *                  if the operations has not been started or has been
 *                  finished, or if the AAD has been finished.
 */
int mbedtls_chachapoly_update_aad(mbedtls_chachapoly_context *ctx,
                                  const unsigned char *aad,
                                  size_t aad_len);

/**
 * \brief           Thus function feeds data to be encrypted or decrypted
 *                  into an on-going ChaCha20-Poly1305
 *                  operation.
 *
 *                  The direction (encryption or decryption) depends on the
 *                  mode that was given when calling
 *                  \c mbedtls_chachapoly_starts().
 *
 *                  You may call this function multiple times to process
 *                  an arbitrary amount of data. It is permitted to call
 *                  this function 0 times, if no data is to be encrypted
 *                  or decrypted.
 *
 * \warning         Decryption with the piecewise API is discouraged, see the
 *                  warning on \c mbedtls_chachapoly_init().
 *
 * \param ctx       The ChaCha20-Poly1305 context to use. This must be initialized.
 * \param len       The length (in bytes) of the data to encrypt or decrypt.
 * \param input     The buffer containing the data to encrypt or decrypt.
 *                  This pointer can be \c NULL if `len == 0`.
 * \param output    The buffer to where the encrypted or decrypted data is
 *                  written. This must be able to hold \p len bytes.
 *                  This pointer can be \c NULL if `len == 0`.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CHACHAPOLY_BAD_STATE
 *                  if the operation has not been started or has been
 *                  finished.
 * \return          Another negative error code on other kinds of failure.
 */
int mbedtls_chachapoly_update(mbedtls_chachapoly_context *ctx,
                              size_t len,
                              const unsigned char *input,
                              unsigned char *output);

/**
 * \brief           This function finished the ChaCha20-Poly1305 operation and
 *                  generates the MAC (authentication tag).
 *
 * \param ctx       The ChaCha20-Poly1305 context to use. This must be initialized.
 * \param mac       The buffer to where the 128-bit (16 bytes) MAC is written.
 *
 * \warning         Decryption with the piecewise API is discouraged, see the
 *                  warning on \c mbedtls_chachapoly_init().
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CHACHAPOLY_BAD_STATE
 *                  if the operation has not been started or has been
 *                  finished.
 * \return          Another negative error code on other kinds of failure.
 */
int mbedtls_chachapoly_finish(mbedtls_chachapoly_context *ctx,
                              unsigned char mac[16]);

/**
 * \brief           This function performs a complete ChaCha20-Poly1305
 *                  authenticated encryption with the previously-set key.
 *
 * \note            Before using this function, you must set the key with
 *                  \c mbedtls_chachapoly_setkey().
 *
 * \warning         You must never use the same nonce twice with the same key.
 *                  This would void any confidentiality and authenticity
 *                  guarantees for the messages encrypted with the same nonce
 *                  and key.
 *
 * \param ctx       The ChaCha20-Poly1305 context to use (holds the key).
 *                  This must be initialized.
 * \param length    The length (in bytes) of the data to encrypt or decrypt.
 * \param nonce     The 96-bit (12 bytes) nonce/IV to use.
 * \param aad       The buffer containing the additional authenticated
 *                  data (AAD). This pointer can be \c NULL if `aad_len == 0`.
 * \param aad_len   The length (in bytes) of the AAD data to process.
 * \param input     The buffer containing the data to encrypt or decrypt.
 *                  This pointer can be \c NULL if `ilen == 0`.
 * \param output    The buffer to where the encrypted or decrypted data
 *                  is written. This pointer can be \c NULL if `ilen == 0`.
 * \param tag       The buffer to where the computed 128-bit (16 bytes) MAC
 *                  is written. This must not be \c NULL.
 *
 * \return          \c 0 on success.
 * \return          A negative error code on failure.
 */
int mbedtls_chachapoly_encrypt_and_tag(mbedtls_chachapoly_context *ctx,
                                       size_t length,
                                       const unsigned char nonce[12],
                                       const unsigned char *aad,
                                       size_t aad_len,
                                       const unsigned char *input,
                                       unsigned char *output,
                                       unsigned char tag[16]);

/**
 * \brief           This function performs a complete ChaCha20-Poly1305
 *                  authenticated decryption with the previously-set key.
 *
 * \note            Before using this function, you must set the key with
 *                  \c mbedtls_chachapoly_setkey().
 *
 * \param ctx       The ChaCha20-Poly1305 context to use (holds the key).
 * \param length    The length (in Bytes) of the data to decrypt.
 * \param nonce     The \c 96 Bit (\c 12 bytes) nonce/IV to use.
 * \param aad       The buffer containing the additional authenticated data (AAD).
 *                  This pointer can be \c NULL if `aad_len == 0`.
 * \param aad_len   The length (in bytes) of the AAD data to process.
 * \param tag       The buffer holding the authentication tag.
 *                  This must be a readable buffer of length \c 16 Bytes.
 * \param input     The buffer containing the data to decrypt.
 *                  This pointer can be \c NULL if `ilen == 0`.
 * \param output    The buffer to where the decrypted data is written.
 *                  This pointer can be \c NULL if `ilen == 0`.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED
 *                  if the data was not authentic.
 * \return          Another negative error code on other kinds of failure.
 */
int mbedtls_chachapoly_auth_decrypt(mbedtls_chachapoly_context *ctx,
                                    size_t length,
                                    const unsigned char nonce[12],
                                    const unsigned char *aad,
                                    size_t aad_len,
                                    const unsigned char tag[16],
                                    const unsigned char *input,
                                    unsigned char *output);

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief           The ChaCha20-Poly1305 checkup routine.
 *
 * \return          \c 0 on success.
 * \return          \c 1 on failure.
 */
int mbedtls_chachapoly_self_test(int verbose);
#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_CHACHAPOLY_H */

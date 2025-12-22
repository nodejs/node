/**
 * \file ccm.h
 *
 * \brief This file provides an API for the CCM authenticated encryption
 *        mode for block ciphers.
 *
 * CCM combines Counter mode encryption with CBC-MAC authentication
 * for 128-bit block ciphers.
 *
 * Input to CCM includes the following elements:
 * <ul><li>Payload - data that is both authenticated and encrypted.</li>
 * <li>Associated data (Adata) - data that is authenticated but not
 * encrypted, For example, a header.</li>
 * <li>Nonce - A unique value that is assigned to the payload and the
 * associated data.</li></ul>
 *
 * Definition of CCM:
 * http://csrc.nist.gov/publications/nistpubs/800-38C/SP800-38C_updated-July20_2007.pdf
 * RFC 3610 "Counter with CBC-MAC (CCM)"
 *
 * Related:
 * RFC 5116 "An Interface and Algorithms for Authenticated Encryption"
 *
 * Definition of CCM*:
 * IEEE 802.15.4 - IEEE Standard for Local and metropolitan area networks
 * Integer representation is fixed most-significant-octet-first order and
 * the representation of octets is most-significant-bit-first order. This is
 * consistent with RFC 3610.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CCM_H
#define MBEDTLS_CCM_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/cipher.h"

#if defined(MBEDTLS_BLOCK_CIPHER_C)
#include "mbedtls/block_cipher.h"
#endif

#define MBEDTLS_CCM_DECRYPT       0
#define MBEDTLS_CCM_ENCRYPT       1
#define MBEDTLS_CCM_STAR_DECRYPT  2
#define MBEDTLS_CCM_STAR_ENCRYPT  3

/** Bad input parameters to the function. */
#define MBEDTLS_ERR_CCM_BAD_INPUT       -0x000D
/** Authenticated decryption failed. */
#define MBEDTLS_ERR_CCM_AUTH_FAILED     -0x000F

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_CCM_ALT)
// Regular implementation
//

/**
 * \brief    The CCM context-type definition. The CCM context is passed
 *           to the APIs called.
 */
typedef struct mbedtls_ccm_context {
    unsigned char MBEDTLS_PRIVATE(y)[16];    /*!< The Y working buffer */
    unsigned char MBEDTLS_PRIVATE(ctr)[16];  /*!< The counter buffer */
    size_t MBEDTLS_PRIVATE(plaintext_len);   /*!< Total plaintext length */
    size_t MBEDTLS_PRIVATE(add_len);         /*!< Total authentication data length */
    size_t MBEDTLS_PRIVATE(tag_len);         /*!< Total tag length */
    size_t MBEDTLS_PRIVATE(processed);       /*!< Track how many bytes of input data
                                                  were processed (chunked input).
                                                  Used independently for both auth data
                                                  and plaintext/ciphertext.
                                                  This variable is set to zero after
                                                  auth data input is finished. */
    unsigned int MBEDTLS_PRIVATE(q);         /*!< The Q working value */
    unsigned int MBEDTLS_PRIVATE(mode);      /*!< The operation to perform:
                                              #MBEDTLS_CCM_ENCRYPT or
                                              #MBEDTLS_CCM_DECRYPT or
                                              #MBEDTLS_CCM_STAR_ENCRYPT or
                                              #MBEDTLS_CCM_STAR_DECRYPT. */
#if defined(MBEDTLS_BLOCK_CIPHER_C)
    mbedtls_block_cipher_context_t MBEDTLS_PRIVATE(block_cipher_ctx);    /*!< The cipher context used. */
#else
    mbedtls_cipher_context_t MBEDTLS_PRIVATE(cipher_ctx);    /*!< The cipher context used. */
#endif
    int MBEDTLS_PRIVATE(state);              /*!< Working value holding context's
                                                  state. Used for chunked data input */
}
mbedtls_ccm_context;

#else  /* MBEDTLS_CCM_ALT */
#include "ccm_alt.h"
#endif /* MBEDTLS_CCM_ALT */

/**
 * \brief           This function initializes the specified CCM context,
 *                  to make references valid, and prepare the context
 *                  for mbedtls_ccm_setkey() or mbedtls_ccm_free().
 *
 * \param ctx       The CCM context to initialize. This must not be \c NULL.
 */
void mbedtls_ccm_init(mbedtls_ccm_context *ctx);

/**
 * \brief           This function initializes the CCM context set in the
 *                  \p ctx parameter and sets the encryption key.
 *
 * \param ctx       The CCM context to initialize. This must be an initialized
 *                  context.
 * \param cipher    The 128-bit block cipher to use.
 * \param key       The encryption key. This must not be \c NULL.
 * \param keybits   The key size in bits. This must be acceptable by the cipher.
 *
 * \return          \c 0 on success.
 * \return          A CCM or cipher-specific error code on failure.
 */
int mbedtls_ccm_setkey(mbedtls_ccm_context *ctx,
                       mbedtls_cipher_id_t cipher,
                       const unsigned char *key,
                       unsigned int keybits);

/**
 * \brief   This function releases and clears the specified CCM context
 *          and underlying cipher sub-context.
 *
 * \param ctx       The CCM context to clear. If this is \c NULL, the function
 *                  has no effect. Otherwise, this must be initialized.
 */
void mbedtls_ccm_free(mbedtls_ccm_context *ctx);

/**
 * \brief           This function encrypts a buffer using CCM.
 *
 * \note            The tag is written to a separate buffer. To concatenate
 *                  the \p tag with the \p output, as done in <em>RFC-3610:
 *                  Counter with CBC-MAC (CCM)</em>, use
 *                  \p tag = \p output + \p length, and make sure that the
 *                  output buffer is at least \p length + \p tag_len wide.
 *
 * \param ctx       The CCM context to use for encryption. This must be
 *                  initialized and bound to a key.
 * \param length    The length of the input data in Bytes.
 * \param iv        The initialization vector (nonce). This must be a readable
 *                  buffer of at least \p iv_len Bytes.
 * \param iv_len    The length of the nonce in Bytes: 7, 8, 9, 10, 11, 12,
 *                  or 13. The length L of the message length field is
 *                  15 - \p iv_len.
 * \param ad        The additional data field. If \p ad_len is greater than
 *                  zero, \p ad must be a readable buffer of at least that
 *                  length.
 * \param ad_len    The length of additional data in Bytes.
 *                  This must be less than `2^16 - 2^8`.
 * \param input     The buffer holding the input data. If \p length is greater
 *                  than zero, \p input must be a readable buffer of at least
 *                  that length.
 * \param output    The buffer holding the output data. If \p length is greater
 *                  than zero, \p output must be a writable buffer of at least
 *                  that length.
 * \param tag       The buffer holding the authentication field. This must be a
 *                  writable buffer of at least \p tag_len Bytes.
 * \param tag_len   The length of the authentication field to generate in Bytes:
 *                  4, 6, 8, 10, 12, 14 or 16.
 *
 * \return          \c 0 on success.
 * \return          A CCM or cipher-specific error code on failure.
 */
int mbedtls_ccm_encrypt_and_tag(mbedtls_ccm_context *ctx, size_t length,
                                const unsigned char *iv, size_t iv_len,
                                const unsigned char *ad, size_t ad_len,
                                const unsigned char *input, unsigned char *output,
                                unsigned char *tag, size_t tag_len);

/**
 * \brief           This function encrypts a buffer using CCM*.
 *
 * \note            The tag is written to a separate buffer. To concatenate
 *                  the \p tag with the \p output, as done in <em>RFC-3610:
 *                  Counter with CBC-MAC (CCM)</em>, use
 *                  \p tag = \p output + \p length, and make sure that the
 *                  output buffer is at least \p length + \p tag_len wide.
 *
 * \note            When using this function in a variable tag length context,
 *                  the tag length has to be encoded into the \p iv passed to
 *                  this function.
 *
 * \param ctx       The CCM context to use for encryption. This must be
 *                  initialized and bound to a key.
 * \param length    The length of the input data in Bytes.
 *                  For tag length = 0, input length is ignored.
 * \param iv        The initialization vector (nonce). This must be a readable
 *                  buffer of at least \p iv_len Bytes.
 * \param iv_len    The length of the nonce in Bytes: 7, 8, 9, 10, 11, 12,
 *                  or 13. The length L of the message length field is
 *                  15 - \p iv_len.
 * \param ad        The additional data field. This must be a readable buffer of
 *                  at least \p ad_len Bytes.
 * \param ad_len    The length of additional data in Bytes.
 *                  This must be less than 2^16 - 2^8.
 * \param input     The buffer holding the input data. If \p length is greater
 *                  than zero, \p input must be a readable buffer of at least
 *                  that length.
 * \param output    The buffer holding the output data. If \p length is greater
 *                  than zero, \p output must be a writable buffer of at least
 *                  that length.
 * \param tag       The buffer holding the authentication field. This must be a
 *                  writable buffer of at least \p tag_len Bytes.
 * \param tag_len   The length of the authentication field to generate in Bytes:
 *                  0, 4, 6, 8, 10, 12, 14 or 16.
 *
 * \warning         Passing \c 0 as \p tag_len means that the message is no
 *                  longer authenticated.
 *
 * \return          \c 0 on success.
 * \return          A CCM or cipher-specific error code on failure.
 */
int mbedtls_ccm_star_encrypt_and_tag(mbedtls_ccm_context *ctx, size_t length,
                                     const unsigned char *iv, size_t iv_len,
                                     const unsigned char *ad, size_t ad_len,
                                     const unsigned char *input, unsigned char *output,
                                     unsigned char *tag, size_t tag_len);

/**
 * \brief           This function performs a CCM authenticated decryption of a
 *                  buffer.
 *
 * \param ctx       The CCM context to use for decryption. This must be
 *                  initialized and bound to a key.
 * \param length    The length of the input data in Bytes.
 * \param iv        The initialization vector (nonce). This must be a readable
 *                  buffer of at least \p iv_len Bytes.
 * \param iv_len    The length of the nonce in Bytes: 7, 8, 9, 10, 11, 12,
 *                  or 13. The length L of the message length field is
 *                  15 - \p iv_len.
 * \param ad        The additional data field. This must be a readable buffer
 *                  of at least that \p ad_len Bytes..
 * \param ad_len    The length of additional data in Bytes.
 *                  This must be less than 2^16 - 2^8.
 * \param input     The buffer holding the input data. If \p length is greater
 *                  than zero, \p input must be a readable buffer of at least
 *                  that length.
 * \param output    The buffer holding the output data. If \p length is greater
 *                  than zero, \p output must be a writable buffer of at least
 *                  that length.
 * \param tag       The buffer holding the authentication field. This must be a
 *                  readable buffer of at least \p tag_len Bytes.
 * \param tag_len   The length of the authentication field to generate in Bytes:
 *                  4, 6, 8, 10, 12, 14 or 16.
 *
 * \return          \c 0 on success. This indicates that the message is authentic.
 * \return          #MBEDTLS_ERR_CCM_AUTH_FAILED if the tag does not match.
 * \return          A cipher-specific error code on calculation failure.
 */
int mbedtls_ccm_auth_decrypt(mbedtls_ccm_context *ctx, size_t length,
                             const unsigned char *iv, size_t iv_len,
                             const unsigned char *ad, size_t ad_len,
                             const unsigned char *input, unsigned char *output,
                             const unsigned char *tag, size_t tag_len);

/**
 * \brief           This function performs a CCM* authenticated decryption of a
 *                  buffer.
 *
 * \note            When using this function in a variable tag length context,
 *                  the tag length has to be decoded from \p iv and passed to
 *                  this function as \p tag_len. (\p tag needs to be adjusted
 *                  accordingly.)
 *
 * \param ctx       The CCM context to use for decryption. This must be
 *                  initialized and bound to a key.
 * \param length    The length of the input data in Bytes.
 *                  For tag length = 0, input length is ignored.
 * \param iv        The initialization vector (nonce). This must be a readable
 *                  buffer of at least \p iv_len Bytes.
 * \param iv_len    The length of the nonce in Bytes: 7, 8, 9, 10, 11, 12,
 *                  or 13. The length L of the message length field is
 *                  15 - \p iv_len.
 * \param ad        The additional data field. This must be a readable buffer of
 *                  at least that \p ad_len Bytes.
 * \param ad_len    The length of additional data in Bytes.
 *                  This must be less than 2^16 - 2^8.
 * \param input     The buffer holding the input data. If \p length is greater
 *                  than zero, \p input must be a readable buffer of at least
 *                  that length.
 * \param output    The buffer holding the output data. If \p length is greater
 *                  than zero, \p output must be a writable buffer of at least
 *                  that length.
 * \param tag       The buffer holding the authentication field. This must be a
 *                  readable buffer of at least \p tag_len Bytes.
 * \param tag_len   The length of the authentication field in Bytes.
 *                  0, 4, 6, 8, 10, 12, 14 or 16.
 *
 * \warning         Passing \c 0 as \p tag_len means that the message is nos
 *                  longer authenticated.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CCM_AUTH_FAILED if the tag does not match.
 * \return          A cipher-specific error code on calculation failure.
 */
int mbedtls_ccm_star_auth_decrypt(mbedtls_ccm_context *ctx, size_t length,
                                  const unsigned char *iv, size_t iv_len,
                                  const unsigned char *ad, size_t ad_len,
                                  const unsigned char *input, unsigned char *output,
                                  const unsigned char *tag, size_t tag_len);

/**
 * \brief           This function starts a CCM encryption or decryption
 *                  operation.
 *
 *                  This function and mbedtls_ccm_set_lengths() must be called
 *                  before calling mbedtls_ccm_update_ad() or
 *                  mbedtls_ccm_update(). This function can be called before
 *                  or after mbedtls_ccm_set_lengths().
 *
 * \note            This function is not implemented in Mbed TLS yet.
 *
 * \param ctx       The CCM context. This must be initialized.
 * \param mode      The operation to perform: #MBEDTLS_CCM_ENCRYPT or
 *                  #MBEDTLS_CCM_DECRYPT or #MBEDTLS_CCM_STAR_ENCRYPT or
 *                  #MBEDTLS_CCM_STAR_DECRYPT.
 * \param iv        The initialization vector. This must be a readable buffer
 *                  of at least \p iv_len Bytes.
 * \param iv_len    The length of the nonce in Bytes: 7, 8, 9, 10, 11, 12,
 *                  or 13. The length L of the message length field is
 *                  15 - \p iv_len.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CCM_BAD_INPUT on failure:
 *                  \p ctx is in an invalid state,
 *                  \p mode is invalid,
 *                  \p iv_len is invalid (lower than \c 7 or greater than
 *                  \c 13).
 */
int mbedtls_ccm_starts(mbedtls_ccm_context *ctx,
                       int mode,
                       const unsigned char *iv,
                       size_t iv_len);

/**
 * \brief           This function declares the lengths of the message
 *                  and additional data for a CCM encryption or decryption
 *                  operation.
 *
 *                  This function and mbedtls_ccm_starts() must be called
 *                  before calling mbedtls_ccm_update_ad() or
 *                  mbedtls_ccm_update(). This function can be called before
 *                  or after mbedtls_ccm_starts().
 *
 * \note            This function is not implemented in Mbed TLS yet.
 *
 * \param ctx       The CCM context. This must be initialized.
 * \param total_ad_len   The total length of additional data in bytes.
 *                       This must be less than `2^16 - 2^8`.
 * \param plaintext_len  The length in bytes of the plaintext to encrypt or
 *                       result of the decryption (thus not encompassing the
 *                       additional data that are not encrypted).
 * \param tag_len   The length of the tag to generate in Bytes:
 *                  4, 6, 8, 10, 12, 14 or 16.
 *                  For CCM*, zero is also valid.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CCM_BAD_INPUT on failure:
 *                  \p ctx is in an invalid state,
 *                  \p total_ad_len is greater than \c 0xFF00.
 */
int mbedtls_ccm_set_lengths(mbedtls_ccm_context *ctx,
                            size_t total_ad_len,
                            size_t plaintext_len,
                            size_t tag_len);

/**
 * \brief           This function feeds an input buffer as associated data
 *                  (authenticated but not encrypted data) in a CCM
 *                  encryption or decryption operation.
 *
 *                  You may call this function zero, one or more times
 *                  to pass successive parts of the additional data. The
 *                  lengths \p ad_len of the data parts should eventually add
 *                  up exactly to the total length of additional data
 *                  \c total_ad_len passed to mbedtls_ccm_set_lengths(). You
 *                  may not call this function after calling
 *                  mbedtls_ccm_update().
 *
 * \note            This function is not implemented in Mbed TLS yet.
 *
 * \param ctx       The CCM context. This must have been started with
 *                  mbedtls_ccm_starts(), the lengths of the message and
 *                  additional data must have been declared with
 *                  mbedtls_ccm_set_lengths() and this must not have yet
 *                  received any input with mbedtls_ccm_update().
 * \param ad        The buffer holding the additional data, or \c NULL
 *                  if \p ad_len is \c 0.
 * \param ad_len    The length of the additional data. If \c 0,
 *                  \p ad may be \c NULL.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CCM_BAD_INPUT on failure:
 *                  \p ctx is in an invalid state,
 *                  total input length too long.
 */
int mbedtls_ccm_update_ad(mbedtls_ccm_context *ctx,
                          const unsigned char *ad,
                          size_t ad_len);

/**
 * \brief           This function feeds an input buffer into an ongoing CCM
 *                  encryption or decryption operation.
 *
 *                  You may call this function zero, one or more times
 *                  to pass successive parts of the input: the plaintext to
 *                  encrypt, or the ciphertext (not including the tag) to
 *                  decrypt. After the last part of the input, call
 *                  mbedtls_ccm_finish(). The lengths \p input_len of the
 *                  data parts should eventually add up exactly to the
 *                  plaintext length \c plaintext_len passed to
 *                  mbedtls_ccm_set_lengths().
 *
 *                  This function may produce output in one of the following
 *                  ways:
 *                  - Immediate output: the output length is always equal
 *                    to the input length.
 *                  - Buffered output: except for the last part of input data,
 *                    the output consists of a whole number of 16-byte blocks.
 *                    If the total input length so far (not including
 *                    associated data) is 16 \* *B* + *A* with *A* < 16 then
 *                    the total output length is 16 \* *B*.
 *                    For the last part of input data, the output length is
 *                    equal to the input length plus the number of bytes (*A*)
 *                    buffered in the previous call to the function (if any).
 *                    The function uses the plaintext length
 *                    \c plaintext_len passed to mbedtls_ccm_set_lengths()
 *                    to detect the last part of input data.
 *
 *                  In particular:
 *                  - It is always correct to call this function with
 *                    \p output_size >= \p input_len + 15.
 *                  - If \p input_len is a multiple of 16 for all the calls
 *                    to this function during an operation (not necessary for
 *                    the last one) then it is correct to use \p output_size
 *                    =\p input_len.
 *
 * \note            This function is not implemented in Mbed TLS yet.
 *
 * \param ctx           The CCM context. This must have been started with
 *                      mbedtls_ccm_starts() and the lengths of the message and
 *                      additional data must have been declared with
 *                      mbedtls_ccm_set_lengths().
 * \param input         The buffer holding the input data. If \p input_len
 *                      is greater than zero, this must be a readable buffer
 *                      of at least \p input_len bytes.
 * \param input_len     The length of the input data in bytes.
 * \param output        The buffer for the output data. If \p output_size
 *                      is greater than zero, this must be a writable buffer of
 *                      at least \p output_size bytes.
 * \param output_size   The size of the output buffer in bytes.
 *                      See the function description regarding the output size.
 * \param output_len    On success, \p *output_len contains the actual
 *                      length of the output written in \p output.
 *                      On failure, the content of \p *output_len is
 *                      unspecified.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_CCM_BAD_INPUT on failure:
 *                 \p ctx is in an invalid state,
 *                 total input length too long,
 *                 or \p output_size too small.
 */
int mbedtls_ccm_update(mbedtls_ccm_context *ctx,
                       const unsigned char *input, size_t input_len,
                       unsigned char *output, size_t output_size,
                       size_t *output_len);

/**
 * \brief           This function finishes the CCM operation and generates
 *                  the authentication tag.
 *
 *                  It wraps up the CCM stream, and generates the
 *                  tag. The tag can have a maximum length of 16 Bytes.
 *
 * \note            This function is not implemented in Mbed TLS yet.
 *
 * \param ctx       The CCM context. This must have been started with
 *                  mbedtls_ccm_starts() and the lengths of the message and
 *                  additional data must have been declared with
 *                  mbedtls_ccm_set_lengths().
 * \param tag       The buffer for holding the tag. If \p tag_len is greater
 *                  than zero, this must be a writable buffer of at least \p
 *                  tag_len Bytes.
 * \param tag_len   The length of the tag. Must match the tag length passed to
 *                  mbedtls_ccm_set_lengths() function.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_CCM_BAD_INPUT on failure:
 *                  \p ctx is in an invalid state,
 *                  invalid value of \p tag_len,
 *                  the total amount of additional data passed to
 *                  mbedtls_ccm_update_ad() was lower than the total length of
 *                  additional data \c total_ad_len passed to
 *                  mbedtls_ccm_set_lengths(),
 *                  the total amount of input data passed to
 *                  mbedtls_ccm_update() was lower than the plaintext length
 *                  \c plaintext_len passed to mbedtls_ccm_set_lengths().
 */
int mbedtls_ccm_finish(mbedtls_ccm_context *ctx,
                       unsigned char *tag, size_t tag_len);

#if defined(MBEDTLS_SELF_TEST) && defined(MBEDTLS_CCM_GCM_CAN_AES)
/**
 * \brief          The CCM checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_ccm_self_test(int verbose);
#endif /* MBEDTLS_SELF_TEST && MBEDTLS_AES_C */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_CCM_H */

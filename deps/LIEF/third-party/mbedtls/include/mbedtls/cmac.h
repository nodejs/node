/**
 * \file cmac.h
 *
 * \brief This file contains CMAC definitions and functions.
 *
 * The Cipher-based Message Authentication Code (CMAC) Mode for
 * Authentication is defined in <em>RFC-4493: The AES-CMAC Algorithm</em>.
 * It is supported with AES and DES.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CMAC_H
#define MBEDTLS_CMAC_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/cipher.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBEDTLS_AES_BLOCK_SIZE          16
#define MBEDTLS_DES3_BLOCK_SIZE         8

/* We don't support Camellia or ARIA in this module */
#if defined(MBEDTLS_AES_C)
#define MBEDTLS_CMAC_MAX_BLOCK_SIZE      16  /**< The longest block used by CMAC is that of AES. */
#else
#define MBEDTLS_CMAC_MAX_BLOCK_SIZE      8   /**< The longest block used by CMAC is that of 3DES. */
#endif

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
/** The longest block supported by the cipher module.
 *
 * \deprecated
 * For the maximum block size of a cipher supported by the CMAC module,
 * use #MBEDTLS_CMAC_MAX_BLOCK_SIZE.
 * For the maximum block size of a cipher supported by the cipher module,
 * use #MBEDTLS_MAX_BLOCK_LENGTH.
 */
/* Before Mbed TLS 3.5, this was the maximum block size supported by the CMAC
 * module, so it didn't take Camellia or ARIA into account. Since the name
 * of the macro doesn't even convey "CMAC", this was misleading. Now the size
 * is sufficient for any cipher, but the name is defined in cmac.h for
 * backward compatibility. */
#define MBEDTLS_CIPHER_BLKSIZE_MAX MBEDTLS_MAX_BLOCK_LENGTH
#endif /* MBEDTLS_DEPRECATED_REMOVED */

#if !defined(MBEDTLS_CMAC_ALT)

/**
 * The CMAC context structure.
 */
struct mbedtls_cmac_context_t {
    /** The internal state of the CMAC algorithm.  */
    unsigned char       MBEDTLS_PRIVATE(state)[MBEDTLS_CMAC_MAX_BLOCK_SIZE];

    /** Unprocessed data - either data that was not block aligned and is still
     *  pending processing, or the final block. */
    unsigned char       MBEDTLS_PRIVATE(unprocessed_block)[MBEDTLS_CMAC_MAX_BLOCK_SIZE];

    /** The length of data pending processing. */
    size_t              MBEDTLS_PRIVATE(unprocessed_len);
};

#else  /* !MBEDTLS_CMAC_ALT */
#include "cmac_alt.h"
#endif /* !MBEDTLS_CMAC_ALT */

/**
 * \brief               This function starts a new CMAC computation
 *                      by setting the CMAC key, and preparing to authenticate
 *                      the input data.
 *                      It must be called with an initialized cipher context.
 *
 *                      Once this function has completed, data can be supplied
 *                      to the CMAC computation by calling
 *                      mbedtls_cipher_cmac_update().
 *
 *                      To start a CMAC computation using the same key as a previous
 *                      CMAC computation, use mbedtls_cipher_cmac_finish().
 *
 * \note                When the CMAC implementation is supplied by an alternate
 *                      implementation (through #MBEDTLS_CMAC_ALT), some ciphers
 *                      may not be supported by that implementation, and thus
 *                      return an error. Alternate implementations must support
 *                      AES-128 and AES-256, and may support AES-192 and 3DES.
 *
 * \param ctx           The cipher context used for the CMAC operation, initialized
 *                      as one of the following types: MBEDTLS_CIPHER_AES_128_ECB,
 *                      MBEDTLS_CIPHER_AES_192_ECB, MBEDTLS_CIPHER_AES_256_ECB,
 *                      or MBEDTLS_CIPHER_DES_EDE3_ECB.
 * \param key           The CMAC key.
 * \param keybits       The length of the CMAC key in bits.
 *                      Must be supported by the cipher.
 *
 * \return              \c 0 on success.
 * \return              A cipher-specific error code on failure.
 */
int mbedtls_cipher_cmac_starts(mbedtls_cipher_context_t *ctx,
                               const unsigned char *key, size_t keybits);

/**
 * \brief               This function feeds an input buffer into an ongoing CMAC
 *                      computation.
 *
 *                      The CMAC computation must have previously been started
 *                      by calling mbedtls_cipher_cmac_starts() or
 *                      mbedtls_cipher_cmac_reset().
 *
 *                      Call this function as many times as needed to input the
 *                      data to be authenticated.
 *                      Once all of the required data has been input,
 *                      call mbedtls_cipher_cmac_finish() to obtain the result
 *                      of the CMAC operation.
 *
 * \param ctx           The cipher context used for the CMAC operation.
 * \param input         The buffer holding the input data.
 * \param ilen          The length of the input data.
 *
 * \return             \c 0 on success.
 * \return             #MBEDTLS_ERR_MD_BAD_INPUT_DATA
 *                     if parameter verification fails.
 */
int mbedtls_cipher_cmac_update(mbedtls_cipher_context_t *ctx,
                               const unsigned char *input, size_t ilen);

/**
 * \brief               This function finishes an ongoing CMAC operation, and
 *                      writes the result to the output buffer.
 *
 *                      It should be followed either by
 *                      mbedtls_cipher_cmac_reset(), which starts another CMAC
 *                      operation with the same key, or mbedtls_cipher_free(),
 *                      which clears the cipher context.
 *
 * \param ctx           The cipher context used for the CMAC operation.
 * \param output        The output buffer for the CMAC checksum result.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_MD_BAD_INPUT_DATA
 *                      if parameter verification fails.
 */
int mbedtls_cipher_cmac_finish(mbedtls_cipher_context_t *ctx,
                               unsigned char *output);

/**
 * \brief               This function starts a new CMAC operation with the same
 *                      key as the previous one.
 *
 *                      It should be called after finishing the previous CMAC
 *                      operation with mbedtls_cipher_cmac_finish().
 *                      After calling this function,
 *                      call mbedtls_cipher_cmac_update() to supply the new
 *                      CMAC operation with data.
 *
 * \param ctx           The cipher context used for the CMAC operation.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_MD_BAD_INPUT_DATA
 *                      if parameter verification fails.
 */
int mbedtls_cipher_cmac_reset(mbedtls_cipher_context_t *ctx);

/**
 * \brief               This function calculates the full generic CMAC
 *                      on the input buffer with the provided key.
 *
 *                      The function allocates the context, performs the
 *                      calculation, and frees the context.
 *
 *                      The CMAC result is calculated as
 *                      output = generic CMAC(cmac key, input buffer).
 *
 * \note                When the CMAC implementation is supplied by an alternate
 *                      implementation (through #MBEDTLS_CMAC_ALT), some ciphers
 *                      may not be supported by that implementation, and thus
 *                      return an error. Alternate implementations must support
 *                      AES-128 and AES-256, and may support AES-192 and 3DES.
 *
 * \param cipher_info   The cipher information.
 * \param key           The CMAC key.
 * \param keylen        The length of the CMAC key in bits.
 * \param input         The buffer holding the input data.
 * \param ilen          The length of the input data.
 * \param output        The buffer for the generic CMAC result.
 *
 * \return              \c 0 on success.
 * \return              #MBEDTLS_ERR_MD_BAD_INPUT_DATA
 *                      if parameter verification fails.
 */
int mbedtls_cipher_cmac(const mbedtls_cipher_info_t *cipher_info,
                        const unsigned char *key, size_t keylen,
                        const unsigned char *input, size_t ilen,
                        unsigned char *output);

#if defined(MBEDTLS_AES_C)
/**
 * \brief           This function implements the AES-CMAC-PRF-128 pseudorandom
 *                  function, as defined in
 *                  <em>RFC-4615: The Advanced Encryption Standard-Cipher-based
 *                  Message Authentication Code-Pseudo-Random Function-128
 *                  (AES-CMAC-PRF-128) Algorithm for the Internet Key
 *                  Exchange Protocol (IKE).</em>
 *
 * \param key       The key to use.
 * \param key_len   The key length in Bytes.
 * \param input     The buffer holding the input data.
 * \param in_len    The length of the input data in Bytes.
 * \param output    The buffer holding the generated 16 Bytes of
 *                  pseudorandom output.
 *
 * \return          \c 0 on success.
 */
int mbedtls_aes_cmac_prf_128(const unsigned char *key, size_t key_len,
                             const unsigned char *input, size_t in_len,
                             unsigned char output[16]);
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_SELF_TEST) && (defined(MBEDTLS_AES_C) || defined(MBEDTLS_DES_C))
/**
 * \brief          The CMAC checkup routine.
 *
 * \note           In case the CMAC routines are provided by an alternative
 *                 implementation (i.e. #MBEDTLS_CMAC_ALT is defined), the
 *                 checkup routine will succeed even if the implementation does
 *                 not support the less widely used AES-192 or 3DES primitives.
 *                 The self-test requires at least AES-128 and AES-256 to be
 *                 supported by the underlying implementation.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_cmac_self_test(int verbose);
#endif /* MBEDTLS_SELF_TEST && ( MBEDTLS_AES_C || MBEDTLS_DES_C ) */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_CMAC_H */

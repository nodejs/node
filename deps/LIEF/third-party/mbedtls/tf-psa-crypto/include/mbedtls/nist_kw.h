/**
 * \file nist_kw.h
 *
 * \brief This file provides an API for key wrapping (KW) and key wrapping with
 *        padding (KWP) as defined in NIST SP 800-38F.
 *        https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-38F.pdf
 *
 *        Key wrapping specifies a deterministic authenticated-encryption mode
 *        of operation, according to <em>NIST SP 800-38F: Recommendation for
 *        Block Cipher Modes of Operation: Methods for Key Wrapping</em>. Its
 *        purpose is to protect cryptographic keys.
 *
 *        Its equivalent is RFC 3394 for KW, and RFC 5649 for KWP.
 *        https://tools.ietf.org/html/rfc3394
 *        https://tools.ietf.org/html/rfc5649
 *
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_NIST_KW_H
#define MBEDTLS_NIST_KW_H
#include "mbedtls/private_access.h"

#include "tf-psa-crypto/build_info.h"
#include "psa/crypto_types.h"
#include "psa/crypto_values.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MBEDTLS_KW_MODE_KW = 0,
    MBEDTLS_KW_MODE_KWP = 1
} mbedtls_nist_kw_mode_t;

/**
 * \brief                    This function encrypts a buffer using key wrapping.
 *
 * \param key                The key wrapping PSA key ID to use for encryption. The key should have the following attributes:
 *                               - type: #PSA_KEY_TYPE_AES
 *                               - algorithm: #PSA_ALG_ECB_NO_PADDING
 *                               - usage flag: #PSA_KEY_USAGE_ENCRYPT + other flags if required by the application.
 * \param mode               The key wrapping mode to use (#MBEDTLS_KW_MODE_KW or #MBEDTLS_KW_MODE_KWP)
 * \param input              The buffer holding the input data.
 * \param input_length       The length of the input data in Bytes.
 *                           The input uses units of 8 Bytes called semiblocks.
 *                           <ul><li>For KW mode: a multiple of 8 bytes between 16 and 2^57-8 inclusive. </li>
 *                           <li>For KWP mode: any length between 1 and 2^32-1 inclusive.</li></ul>
 * \param[out] output        The buffer holding the output data.
 *                           <ul><li>For KW mode: Must be at least 8 bytes larger than \p in_len.</li>
 *                           <li>For KWP mode: Must be at least 8 bytes larger rounded up to a multiple of
 *                           8 bytes for KWP (15 bytes at most).</li></ul>
 * \param output_size        The capacity of the output buffer. \c 0 on failure.
 * \param[out] output_length On success, the number of bytes written to the output buffer. \c 0 on failure.
 *
 * \return                   \c 0 on success.
 * \return                   #PSA_ERROR_INVALID_ARGUMENT for invalid input length.
 * \return                   Another error code on failure of the underlying cipher.
 */
psa_status_t mbedtls_nist_kw_wrap(mbedtls_svc_key_id_t key,
                                  mbedtls_nist_kw_mode_t mode,
                                  const unsigned char *input, size_t input_length,
                                  unsigned char *output, size_t output_size, size_t *output_length);

/**
 * \brief                    This function decrypts a buffer using key wrapping.
 *
 * \param key                The key wrapping PSA key ID to use for encryption. The key should have the following attributes:
 *                               - type: #PSA_KEY_TYPE_AES
 *                               - algorithm: #PSA_ALG_ECB_NO_PADDING
 *                               - usage flag: #PSA_KEY_USAGE_DECRYPT + other flags if required by the application.
 * \param mode               The key wrapping mode to use (#MBEDTLS_KW_MODE_KW or #MBEDTLS_KW_MODE_KWP)
 * \param input              The buffer holding the input data.
 * \param input_length       The length of the input data in Bytes.
 *                           The input uses units of 8 Bytes called semiblocks.
 *                           The input must be a multiple of semiblocks.
 *                           <ul><li>For KW mode: a multiple of 8 bytes between 24 and 2^57 inclusive. </li>
 *                           <li>For KWP mode: a multiple of 8 bytes between 16 and 2^32 inclusive.</li></ul>
 * \param[out] output        The buffer holding the output data.
 *                           The output buffer's minimal length is 8 bytes shorter than \p in_len.
 * \param output_size    The capacity of the output buffer.
 * \param[out] output_length The number of bytes written to the output buffer. \c 0 on failure.
 *                           For KWP mode, the length could be up to 15 bytes shorter than \p in_len,
 *                           depending on how much padding was added to the data.
 *
 * \return                   \c 0 on success.
 * \return                   #PSA_ERROR_INVALID_ARGUMENT for invalid input length.
 * \return                   #PSA_ERROR_INVALID_SIGNATURE for invalid ciphertext.
 * \return                   Another error code on failure of the underlying cipher.
 */
psa_status_t mbedtls_nist_kw_unwrap(mbedtls_svc_key_id_t key,
                                    mbedtls_nist_kw_mode_t mode,
                                    const unsigned char *input, size_t input_length,
                                    unsigned char *output, size_t output_size,
                                    size_t *output_length);


#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_NIST_KW_H */

/**
 * \file aesce.h
 *
 * \brief Support hardware AES acceleration on Armv8-A processors with
 *        the Armv8-A Cryptographic Extension.
 *
 * \warning These functions are only for internal use by other library
 *          functions; you must not call them directly.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_AESCE_H
#define MBEDTLS_AESCE_H

#include "mbedtls/build_info.h"
#include "common.h"

#include "mbedtls/aes.h"


#if defined(MBEDTLS_AESCE_C) \
    && defined(MBEDTLS_ARCH_IS_ARMV8_A) && defined(MBEDTLS_HAVE_NEON_INTRINSICS) \
    && (defined(MBEDTLS_COMPILER_IS_GCC) || defined(__clang__) || defined(MSC_VER))

/* MBEDTLS_AESCE_HAVE_CODE is defined if we have a suitable target platform, and a
 * potentially suitable compiler (compiler version & flags are not checked when defining
 * this). */
#define MBEDTLS_AESCE_HAVE_CODE

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) && !defined(MBEDTLS_AES_USE_HARDWARE_ONLY)

extern signed char mbedtls_aesce_has_support_result;

/**
 * \brief          Internal function to detect the crypto extension in CPUs.
 *
 * \return         1 if CPU has support for the feature, 0 otherwise
 */
int mbedtls_aesce_has_support_impl(void);

#define MBEDTLS_AESCE_HAS_SUPPORT() (mbedtls_aesce_has_support_result == -1 ? \
                                     mbedtls_aesce_has_support_impl() : \
                                     mbedtls_aesce_has_support_result)

#else /* defined(__linux__) && !defined(MBEDTLS_AES_USE_HARDWARE_ONLY) */

/* If we are not on Linux, we can't detect support so assume that it's supported.
 * Similarly, assume support if MBEDTLS_AES_USE_HARDWARE_ONLY is set.
 */
#define MBEDTLS_AESCE_HAS_SUPPORT() 1

#endif /* defined(__linux__) && !defined(MBEDTLS_AES_USE_HARDWARE_ONLY) */

/**
 * \brief          Internal AES-ECB block encryption and decryption
 *
 * \warning        This assumes that the context specifies either 10, 12 or 14
 *                 rounds and will behave incorrectly if this is not the case.
 *
 * \param ctx      AES context
 * \param mode     MBEDTLS_AES_ENCRYPT or MBEDTLS_AES_DECRYPT
 * \param input    16-byte input block
 * \param output   16-byte output block
 *
 * \return         0 on success (cannot fail)
 */
int mbedtls_aesce_crypt_ecb(mbedtls_aes_context *ctx,
                            int mode,
                            const unsigned char input[16],
                            unsigned char output[16]);

/**
 * \brief          Internal GCM multiplication: c = a * b in GF(2^128)
 *
 * \note           This function is only for internal use by other library
 *                 functions; you must not call it directly.
 *
 * \param c        Result
 * \param a        First operand
 * \param b        Second operand
 *
 * \note           Both operands and result are bit strings interpreted as
 *                 elements of GF(2^128) as per the GCM spec.
 */
void mbedtls_aesce_gcm_mult(unsigned char c[16],
                            const unsigned char a[16],
                            const unsigned char b[16]);


#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
/**
 * \brief           Internal round key inversion. This function computes
 *                  decryption round keys from the encryption round keys.
 *
 * \param invkey    Round keys for the equivalent inverse cipher
 * \param fwdkey    Original round keys (for encryption)
 * \param nr        Number of rounds (that is, number of round keys minus one)
 */
void mbedtls_aesce_inverse_key(unsigned char *invkey,
                               const unsigned char *fwdkey,
                               int nr);
#endif /* !MBEDTLS_BLOCK_CIPHER_NO_DECRYPT */

/**
 * \brief           Internal key expansion for encryption
 *
 * \param rk        Destination buffer where the round keys are written
 * \param key       Encryption key
 * \param bits      Key size in bits (must be 128, 192 or 256)
 *
 * \return          0 if successful, or MBEDTLS_ERR_AES_INVALID_KEY_LENGTH
 */
int mbedtls_aesce_setkey_enc(unsigned char *rk,
                             const unsigned char *key,
                             size_t bits);

#ifdef __cplusplus
}
#endif

#else

#if defined(MBEDTLS_AES_USE_HARDWARE_ONLY) && defined(MBEDTLS_ARCH_IS_ARMV8_A)
#error "AES hardware acceleration not supported on this platform / compiler"
#endif

#endif /* MBEDTLS_AESCE_C && MBEDTLS_ARCH_IS_ARMV8_A && MBEDTLS_HAVE_NEON_INTRINSICS &&
          (MBEDTLS_COMPILER_IS_GCC || __clang__ || MSC_VER) */

#endif /* MBEDTLS_AESCE_H */

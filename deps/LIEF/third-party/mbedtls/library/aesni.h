/**
 * \file aesni.h
 *
 * \brief AES-NI for hardware AES acceleration on some Intel processors
 *
 * \warning These functions are only for internal use by other library
 *          functions; you must not call them directly.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_AESNI_H
#define MBEDTLS_AESNI_H

#include "mbedtls/build_info.h"

#include "mbedtls/aes.h"

#define MBEDTLS_AESNI_AES      0x02000000u
#define MBEDTLS_AESNI_CLMUL    0x00000002u

#if defined(MBEDTLS_AESNI_C) && \
    (defined(MBEDTLS_ARCH_IS_X64) || defined(MBEDTLS_ARCH_IS_X86))

/* Can we do AESNI with intrinsics?
 * (Only implemented with certain compilers, only for certain targets.)
 */
#undef MBEDTLS_AESNI_HAVE_INTRINSICS
#if defined(_MSC_VER) && !defined(__clang__)
/* Visual Studio supports AESNI intrinsics since VS 2008 SP1. We only support
 * VS 2013 and up for other reasons anyway, so no need to check the version. */
#define MBEDTLS_AESNI_HAVE_INTRINSICS
#endif
/* GCC-like compilers: currently, we only support intrinsics if the requisite
 * target flag is enabled when building the library (e.g. `gcc -mpclmul -msse2`
 * or `clang -maes -mpclmul`). */
#if (defined(__GNUC__) || defined(__clang__)) && defined(__AES__) && defined(__PCLMUL__)
#define MBEDTLS_AESNI_HAVE_INTRINSICS
#endif
/* For 32-bit, we only support intrinsics */
#if defined(MBEDTLS_ARCH_IS_X86) && (defined(__GNUC__) || defined(__clang__))
#define MBEDTLS_AESNI_HAVE_INTRINSICS
#endif

/* Choose the implementation of AESNI, if one is available.
 *
 * Favor the intrinsics-based implementation if it's available, for better
 * maintainability.
 * Performance is about the same (see #7380).
 * In the long run, we will likely remove the assembly implementation. */
#if defined(MBEDTLS_AESNI_HAVE_INTRINSICS)
#define MBEDTLS_AESNI_HAVE_CODE 2 // via intrinsics
#elif defined(MBEDTLS_HAVE_ASM) && \
    (defined(__GNUC__) || defined(__clang__)) && defined(MBEDTLS_ARCH_IS_X64)
/* Can we do AESNI with inline assembly?
 * (Only implemented with gas syntax, only for 64-bit.)
 */
#define MBEDTLS_AESNI_HAVE_CODE 1 // via assembly
#else
#error "MBEDTLS_AESNI_C defined, but neither intrinsics nor assembly available"
#endif

#if defined(MBEDTLS_AESNI_HAVE_CODE)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          Internal function to detect the AES-NI feature in CPUs.
 *
 * \note           This function is only for internal use by other library
 *                 functions; you must not call it directly.
 *
 * \param what     The feature to detect
 *                 (MBEDTLS_AESNI_AES or MBEDTLS_AESNI_CLMUL)
 *
 * \return         1 if CPU has support for the feature, 0 otherwise
 */
#if !defined(MBEDTLS_AES_USE_HARDWARE_ONLY)
int mbedtls_aesni_has_support(unsigned int what);
#else
#define mbedtls_aesni_has_support(what) 1
#endif

/**
 * \brief          Internal AES-NI AES-ECB block encryption and decryption
 *
 * \note           This function is only for internal use by other library
 *                 functions; you must not call it directly.
 *
 * \param ctx      AES context
 * \param mode     MBEDTLS_AES_ENCRYPT or MBEDTLS_AES_DECRYPT
 * \param input    16-byte input block
 * \param output   16-byte output block
 *
 * \return         0 on success (cannot fail)
 */
int mbedtls_aesni_crypt_ecb(mbedtls_aes_context *ctx,
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
void mbedtls_aesni_gcm_mult(unsigned char c[16],
                            const unsigned char a[16],
                            const unsigned char b[16]);

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
/**
 * \brief           Internal round key inversion. This function computes
 *                  decryption round keys from the encryption round keys.
 *
 * \note            This function is only for internal use by other library
 *                  functions; you must not call it directly.
 *
 * \param invkey    Round keys for the equivalent inverse cipher
 * \param fwdkey    Original round keys (for encryption)
 * \param nr        Number of rounds (that is, number of round keys minus one)
 */
void mbedtls_aesni_inverse_key(unsigned char *invkey,
                               const unsigned char *fwdkey,
                               int nr);
#endif /* !MBEDTLS_BLOCK_CIPHER_NO_DECRYPT */

/**
 * \brief           Internal key expansion for encryption
 *
 * \note            This function is only for internal use by other library
 *                  functions; you must not call it directly.
 *
 * \param rk        Destination buffer where the round keys are written
 * \param key       Encryption key
 * \param bits      Key size in bits (must be 128, 192 or 256)
 *
 * \return          0 if successful, or MBEDTLS_ERR_AES_INVALID_KEY_LENGTH
 */
int mbedtls_aesni_setkey_enc(unsigned char *rk,
                             const unsigned char *key,
                             size_t bits);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_AESNI_HAVE_CODE */
#endif  /* MBEDTLS_AESNI_C && (MBEDTLS_ARCH_IS_X64 || MBEDTLS_ARCH_IS_X86) */

#endif /* MBEDTLS_AESNI_H */

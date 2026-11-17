/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (C) 2026, Advanced Micro Devices, all rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 *
 * Implements AES-CBC128/192/256 decryption with VAES (AVX-512)
 *
 * CBC encryption is inherently serial (each ciphertext block depends
 * on the previous one), so VAES provides no benefit there -- the
 * encrypt path falls back to the aesni_cbc_encrypt assembly routine.
 *
 * CBC decryption IS parallel: all blocks can be independently decrypted,
 * then XORed with the preceding ciphertext block (or IV for the first).
 * This implementation processes 4x4=16 blocks per iteration using four
 * ZMM registers, falling back to 8, 4, then single-block processing.
 */

#include "internal/deprecated.h"

#include <openssl/opensslconf.h>
#include "internal/cryptlib.h"
#include <openssl/aes.h>
#include "crypto/modes.h"
#include "crypto/aes_platform.h"

#if VAES_CBC_ELIGIBLE

/* Function prototypes */
void ossl_aes_cbc_vaes_decrypt(const unsigned char *in, unsigned char *out,
    size_t len, const void *key,
    unsigned char ivec[16], int enc);
int ossl_aes_cbc_vaes_eligible(void);

#include <openssl/modes.h>

/* Forward declarations -- defined in aesni-x86_64.pl assembly        */
void aesni_cbc_encrypt(const unsigned char *in, unsigned char *out,
    size_t len, const AES_KEY *key,
    unsigned char *ivec, int enc);
void aesni_decrypt(const unsigned char *in, unsigned char *out,
    const AES_KEY *key);

/*
 * Keep the cleanup out of line in assembly. Its opacity to C and LTO
 * optimizers is what prevents the cleanup call and stores from being
 * eliminated as dead. Do not replace it with a compiler-visible C function.
 */
void ossl_aes_cbc_vaes_cleanup(void *key_schedule, size_t num_keys);
int ossl_aes_cbc_vaes_cleanup_eligible(void);

/* Portable compiler abstractions for inlining and ISA target selection */
#define STRINGIFY_IMPL_(a) #a
#define STRINGIFY_(a) STRINGIFY_IMPL_(a)

#ifdef __clang__
#define OPENSSL_TARGET_VAES512                                         \
    _Pragma(STRINGIFY_(clang attribute push(                           \
        __attribute__((target("avx512f,avx512dq,avx512bw,vaes,aes"))), \
        apply_to = function)))
#define OPENSSL_UNTARGET_VAES512 _Pragma("clang attribute pop")
#elif defined(__GNUC__)
#define OPENSSL_TARGET_VAES512  \
    _Pragma("GCC push_options") \
        _Pragma(STRINGIFY_(GCC target("avx512f,avx512dq,avx512bw,vaes,aes")))
#define OPENSSL_UNTARGET_VAES512 _Pragma("GCC pop_options")
#else
/* MSVC: all intrinsics are always available via <immintrin.h>. */
#define OPENSSL_TARGET_VAES512
#define OPENSSL_UNTARGET_VAES512
#endif

#if defined(__GNUC__) || defined(__clang__)
#define OSSL_FUNC_ALWAYS_INLINE static inline __attribute__((always_inline))
#define OSSL_FUNC_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define OSSL_FUNC_ALWAYS_INLINE static __forceinline
#define OSSL_FUNC_NOINLINE __declspec(noinline)
#else
#define OSSL_FUNC_ALWAYS_INLINE static inline
#define OSSL_FUNC_NOINLINE
#endif

#include <immintrin.h>

OPENSSL_TARGET_VAES512

/* ------------------------------------------------------------------ */
/* AES decryption helpers: 1x, 2x, 4x parallel 512-bit blocks         */
/* Each 512-bit register holds 4 independent 128-bit AES blocks.      */
/* always_inline guarantees the compiler keeps keys in ZMM regs.      */
/* ------------------------------------------------------------------ */

#define DEFINE_AES_DECRYPT_FUNCS(ROUNDS)                    \
    OSSL_FUNC_ALWAYS_INLINE                                 \
    void AesDec_4x512_##ROUNDS(                             \
        __m512i *b1, __m512i *b2, __m512i *b3, __m512i *b4, \
        const __m512i *rk)                                  \
    {                                                       \
        *b1 = _mm512_xor_si512(*b1, rk[0]);                 \
        *b2 = _mm512_xor_si512(*b2, rk[0]);                 \
        *b3 = _mm512_xor_si512(*b3, rk[0]);                 \
        *b4 = _mm512_xor_si512(*b4, rk[0]);                 \
        for (int i = 1; i < ROUNDS; i++) {                  \
            *b1 = _mm512_aesdec_epi128(*b1, rk[i]);         \
            *b2 = _mm512_aesdec_epi128(*b2, rk[i]);         \
            *b3 = _mm512_aesdec_epi128(*b3, rk[i]);         \
            *b4 = _mm512_aesdec_epi128(*b4, rk[i]);         \
        }                                                   \
        *b1 = _mm512_aesdeclast_epi128(*b1, rk[ROUNDS]);    \
        *b2 = _mm512_aesdeclast_epi128(*b2, rk[ROUNDS]);    \
        *b3 = _mm512_aesdeclast_epi128(*b3, rk[ROUNDS]);    \
        *b4 = _mm512_aesdeclast_epi128(*b4, rk[ROUNDS]);    \
    }                                                       \
                                                            \
    OSSL_FUNC_ALWAYS_INLINE                                 \
    void AesDec_2x512_##ROUNDS(                             \
        __m512i *b1, __m512i *b2, const __m512i *rk)        \
    {                                                       \
        *b1 = _mm512_xor_si512(*b1, rk[0]);                 \
        *b2 = _mm512_xor_si512(*b2, rk[0]);                 \
        for (int i = 1; i < ROUNDS; i++) {                  \
            *b1 = _mm512_aesdec_epi128(*b1, rk[i]);         \
            *b2 = _mm512_aesdec_epi128(*b2, rk[i]);         \
        }                                                   \
        *b1 = _mm512_aesdeclast_epi128(*b1, rk[ROUNDS]);    \
        *b2 = _mm512_aesdeclast_epi128(*b2, rk[ROUNDS]);    \
    }                                                       \
                                                            \
    OSSL_FUNC_ALWAYS_INLINE                                 \
    void AesDec_1x512_##ROUNDS(                             \
        __m512i *b1, const __m512i *rk)                     \
    {                                                       \
        *b1 = _mm512_xor_si512(*b1, rk[0]);                 \
        for (int i = 1; i < ROUNDS; i++)                    \
            *b1 = _mm512_aesdec_epi128(*b1, rk[i]);         \
        *b1 = _mm512_aesdeclast_epi128(*b1, rk[ROUNDS]);    \
    }

DEFINE_AES_DECRYPT_FUNCS(10) /* AES-128 */
DEFINE_AES_DECRYPT_FUNCS(12) /* AES-192 */
DEFINE_AES_DECRYPT_FUNCS(14) /* AES-256 */

/* ------------------------------------------------------------------ */
/* CBC-mode decryption -- templated per round count                   */
/*                                                                    */
/* Processes as many full blocks as possible:                         */
/*   16 blocks at a time (4 x zmm = 4 x 4 = 16 blocks)                */
/*    8 blocks at a time (2 x zmm)                                    */
/*    4 blocks at a time (1 x zmm)                                    */
/*    1 block at a time for the remaining 0-3 blocks                  */
/*                                                                    */
/* The chaining vector b1 packs [prev_ct[last] | ct[0] | ct[1]        */
/* | ct[2]] so that a single XOR after decryption applies the CBC     */
/* feedback to all four lanes simultaneously.                         */
/* ------------------------------------------------------------------ */

#define DEFINE_CBC_DECRYPT(NR)                                                      \
    OSSL_FUNC_NOINLINE                                                              \
    static void cbc_decrypt_##NR(                                                   \
        const unsigned char *in, unsigned char *out, size_t len,                    \
        const AES_KEY *key, unsigned char *iv)                                      \
    {                                                                               \
        const unsigned char *rk_bytes = (const unsigned char *)key->rd_key;         \
        __m512i rk[NR + 1];                                                         \
        for (int i = 0; i <= NR; i++) {                                             \
            __m128i t = _mm_loadu_si128((const __m128i *)(rk_bytes + i * 16));      \
            rk[i] = _mm512_broadcast_i32x4(t);                                      \
        }                                                                           \
                                                                                    \
        __m512i a1, a2, a3, a4;                                                     \
        __m512i b1, b2, b3, b4;                                                     \
                                                                                    \
        const __m128i *pa = (const __m128i *)in;                                    \
        __m512i *po = (__m512i *)out;                                               \
        size_t blocks = len / AES_BLOCK_SIZE;                                       \
                                                                                    \
        /* Save last ciphertext block for IV update (in-place safe)             */  \
        __m128i saved_iv = _mm_setzero_si128();                                     \
        int has_blocks = (blocks > 0);                                              \
        if (has_blocks)                                                             \
            saved_iv = _mm_loadu_si128(pa + blocks - 1);                            \
                                                                                    \
        if (blocks >= 4) {                                                          \
            /* Build b1 = [IV | ct[0] | ct[1] | ct[2]]                          */  \
            __m512i idx = _mm512_set_epi64(5, 4, 3, 2, 1, 0, 0, 0);                 \
            __m512i ct0;                                                            \
                                                                                    \
            /* CBC C[0]=IV; 0x03 loads one 128-bit block (two 64-bit lanes). */     \
            b1 = _mm512_maskz_loadu_epi64(0x03, iv);                                \
            ct0 = _mm512_loadu_si512(pa);                                           \
            ct0 = _mm512_permutexvar_epi64(idx, ct0);                               \
            b1 = _mm512_mask_blend_epi64(0xFC, b1, ct0);                            \
                                                                                    \
            /* --- 16-block (4 x zmm) main loop ---                             */  \
            while (blocks >= 16) {                                                  \
                __m128i last;                                                       \
                                                                                    \
                a1 = _mm512_loadu_si512(pa);                                        \
                a2 = _mm512_loadu_si512(pa + 4);                                    \
                a3 = _mm512_loadu_si512(pa + 8);                                    \
                a4 = _mm512_loadu_si512(pa + 12);                                   \
                                                                                    \
                b2 = _mm512_loadu_si512(pa + 3);                                    \
                b3 = _mm512_loadu_si512(pa + 7);                                    \
                b4 = _mm512_loadu_si512(pa + 11);                                   \
                                                                                    \
                last = _mm_loadu_si128(pa + 15);                                    \
                                                                                    \
                AesDec_4x512_##NR(&a1, &a2, &a3, &a4, rk);                          \
                                                                                    \
                a1 = _mm512_xor_si512(a1, b1);                                      \
                a2 = _mm512_xor_si512(a2, b2);                                      \
                a3 = _mm512_xor_si512(a3, b3);                                      \
                a4 = _mm512_xor_si512(a4, b4);                                      \
                                                                                    \
                _mm512_storeu_si512(po, a1);                                        \
                _mm512_storeu_si512(po + 1, a2);                                    \
                _mm512_storeu_si512(po + 2, a3);                                    \
                _mm512_storeu_si512(po + 3, a4);                                    \
                                                                                    \
                /* Build next b1 from last ciphertext block                     */  \
                b1 = _mm512_maskz_loadu_epi64(0x03, &last);                         \
                if (blocks > 16) {                                                  \
                    size_t rem = blocks - 16;                                       \
                    /* Load only available lookahead blocks to avoid OOB read. */   \
                    /* One AES block is 16 bytes, it maps to two 64-bit lanes. */   \
                    /* 0x03 (00000011) for 1 block (2 lanes), */                    \
                    /* 0x0F (00001111) for 2 blocks (4 lanes), */                   \
                    /* 0x3F (00111111) for 3 or more blocks (6 lanes). */           \
                    __mmask8 nxmask = (rem >= 3) ? 0x3F : (rem == 2 ? 0x0F : 0x03); \
                    __m512i nx = _mm512_maskz_loadu_epi64(nxmask, pa + 16);         \
                    nx = _mm512_permutexvar_epi64(idx, nx);                         \
                    b1 = _mm512_mask_blend_epi64(0xFC, b1, nx);                     \
                }                                                                   \
                                                                                    \
                pa += 16;                                                           \
                po += 4;                                                            \
                blocks -= 16;                                                       \
            }                                                                       \
                                                                                    \
            /* --- 8-block (2 x zmm) ---                                        */  \
            if (blocks >= 8) {                                                      \
                __m128i last8;                                                      \
                                                                                    \
                a1 = _mm512_loadu_si512(pa);                                        \
                a2 = _mm512_loadu_si512(pa + 4);                                    \
                b2 = _mm512_loadu_si512(pa + 3);                                    \
                last8 = _mm_loadu_si128(pa + 7);                                    \
                                                                                    \
                AesDec_2x512_##NR(&a1, &a2, rk);                                    \
                a1 = _mm512_xor_si512(a1, b1);                                      \
                a2 = _mm512_xor_si512(a2, b2);                                      \
                                                                                    \
                _mm512_storeu_si512(po, a1);                                        \
                _mm512_storeu_si512(po + 1, a2);                                    \
                                                                                    \
                b1 = _mm512_maskz_loadu_epi64(0x03, &last8);                        \
                pa += 8;                                                            \
                po += 2;                                                            \
                blocks -= 8;                                                        \
                                                                                    \
                if (blocks >= 4) {                                                  \
                    __m512i nx = _mm512_loadu_si512(pa);                            \
                    nx = _mm512_permutexvar_epi64(idx, nx);                         \
                    b1 = _mm512_mask_blend_epi64(0xFC, b1, nx);                     \
                }                                                                   \
            }                                                                       \
                                                                                    \
            /* --- 4-block (1 x zmm) ---                                        */  \
            if (blocks >= 4) {                                                      \
                __m128i last4;                                                      \
                                                                                    \
                a1 = _mm512_loadu_si512(pa);                                        \
                last4 = _mm_loadu_si128(pa + 3);                                    \
                                                                                    \
                AesDec_1x512_##NR(&a1, rk);                                         \
                a1 = _mm512_xor_si512(a1, b1);                                      \
                _mm512_storeu_si512(po, a1);                                        \
                                                                                    \
                b1 = _mm512_maskz_loadu_epi64(0x03, &last4);                        \
                pa += 4;                                                            \
                po += 1;                                                            \
                blocks -= 4;                                                        \
            }                                                                       \
                                                                                    \
            /* --- Remaining 1-3 blocks ---                                     */  \
            {                                                                       \
                __m128i *po128 = (__m128i *)po;                                     \
                while (blocks > 0) {                                                \
                    __m128i ct = _mm_loadu_si128(pa);                               \
                    a1 = _mm512_maskz_loadu_epi64(0x03, pa);                        \
                    AesDec_1x512_##NR(&a1, rk);                                     \
                    a1 = _mm512_xor_si512(a1, b1);                                  \
                    _mm512_mask_storeu_epi64(po128, 0x03, a1);                      \
                    b1 = _mm512_maskz_loadu_epi64(0x03, &ct);                       \
                    pa++;                                                           \
                    po128++;                                                        \
                    blocks--;                                                       \
                }                                                                   \
            }                                                                       \
        } else {                                                                    \
            /* Less than 4 blocks -- process individually                       */  \
            __m128i *po128 = (__m128i *)po;                                         \
            b1 = _mm512_maskz_loadu_epi64(0x03, iv);                                \
            while (blocks > 0) {                                                    \
                __m128i ct = _mm_loadu_si128(pa);                                   \
                a1 = _mm512_maskz_loadu_epi64(0x03, pa);                            \
                AesDec_1x512_##NR(&a1, rk);                                         \
                a1 = _mm512_xor_si512(a1, b1);                                      \
                _mm512_mask_storeu_epi64(po128, 0x03, a1);                          \
                b1 = _mm512_maskz_loadu_epi64(0x03, &ct);                           \
                pa++;                                                               \
                po128++;                                                            \
                blocks--;                                                           \
            }                                                                       \
        }                                                                           \
                                                                                    \
        if (has_blocks)                                                             \
            _mm_storeu_si128((__m128i *)iv, saved_iv);                              \
                                                                                    \
        /* Erase the broadcast schedule and the volatile vector register bank. */   \
        ossl_aes_cbc_vaes_cleanup(rk, NR + 1);                                      \
    }

DEFINE_CBC_DECRYPT(10) /* AES-128 */
DEFINE_CBC_DECRYPT(12) /* AES-192 */
DEFINE_CBC_DECRYPT(14) /* AES-256 */

/* ------------------------------------------------------------------ */
/* Public entry point                                                 */
/* ------------------------------------------------------------------ */

void ossl_aes_cbc_vaes_decrypt(const unsigned char *in, unsigned char *out,
    size_t len, const void *key,
    unsigned char ivec[16], int enc)
{
    size_t full_bytes;
    int nr = ((const AES_KEY *)key)->rounds + 1;

    if (len == 0)
        return;

    /* VAES path only optimises decryption; encrypt falls back to asm */
    if (enc) {
        aesni_cbc_encrypt(in, out, len, (const AES_KEY *)key, ivec, enc);
        return;
    }

    full_bytes = (len / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
    if (full_bytes > 0) {
        switch (nr) {
        case 10:
            cbc_decrypt_10(in, out, full_bytes, (const AES_KEY *)key, ivec);
            break;
        case 12:
            cbc_decrypt_12(in, out, full_bytes, (const AES_KEY *)key, ivec);
            break;
        case 14:
            cbc_decrypt_14(in, out, full_bytes, (const AES_KEY *)key, ivec);
            break;
        default: /* invalid key size */
            aesni_cbc_encrypt(in, out, len, (const AES_KEY *)key, ivec, 0);
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* CPU feature check                                                  */
/* ------------------------------------------------------------------ */

int ossl_aes_cbc_vaes_eligible(void)
{
    return ossl_aes_cbc_vaes_cleanup_eligible()
        && (OPENSSL_ia32cap_P[2] & (1 << 16)) /* AVX512F            */
        && (OPENSSL_ia32cap_P[2] & (1 << 17)) /* AVX512DQ           */
        && (OPENSSL_ia32cap_P[2] & (1 << 30)) /* AVX512BW           */
        && (OPENSSL_ia32cap_P[3] & (1 << 9)); /* AVX512VAES         */
}

OPENSSL_UNTARGET_VAES512

#undef OPENSSL_TARGET_VAES512
#undef OPENSSL_UNTARGET_VAES512
#undef STRINGIFY_IMPL_
#undef STRINGIFY_
#undef OSSL_FUNC_ALWAYS_INLINE
#undef OSSL_FUNC_NOINLINE
#endif /* VAES_CBC_ELIGIBLE */

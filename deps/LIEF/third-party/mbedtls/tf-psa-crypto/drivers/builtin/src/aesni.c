/*
 *  AES-NI support functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * [AES-WP] https://www.intel.com/content/www/us/en/developer/articles/tool/intel-advanced-encryption-standard-aes-instructions-set.html
 * [CLMUL-WP] https://www.intel.com/content/www/us/en/develop/download/intel-carry-less-multiplication-instruction-and-its-usage-for-computing-the-gcm-mode.html
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_AESNI_C)

#include "aesni.h"

#include <string.h>

#if defined(MBEDTLS_AESNI_HAVE_CODE)

#if MBEDTLS_AESNI_HAVE_CODE == 2
#if defined(__GNUC__)
#include <cpuid.h>
#elif defined(_MSC_VER)
#include <intrin.h>
#else
#error "`__cpuid` required by MBEDTLS_AESNI_C is not supported by the compiler"
#endif
#include <immintrin.h>
#endif

#if defined(MBEDTLS_ARCH_IS_X86)
#if defined(MBEDTLS_COMPILER_IS_GCC)
#pragma GCC push_options
#pragma GCC target ("pclmul,sse2,aes")
#define MBEDTLS_POP_TARGET_PRAGMA
#elif defined(__clang__) && (__clang_major__ >= 5)
#pragma clang attribute push (__attribute__((target("pclmul,sse2,aes"))), apply_to=function)
#define MBEDTLS_POP_TARGET_PRAGMA
#endif
#endif

#if !defined(MBEDTLS_AES_USE_HARDWARE_ONLY)
/*
 * AES-NI support detection routine
 */
int mbedtls_aesni_has_support(unsigned int what)
{
    /* To avoid a race condition, tell the compiler that the assignment
     * `done = 1` and the assignment to `c` may not be reordered.
     * https://github.com/Mbed-TLS/mbedtls/issues/9840
     *
     * Note that we may also be worried about memory access reordering,
     * but fortunately the x86 memory model is not too wild: stores
     * from the same thread are observed consistently by other threads.
     * (See example 8-1 in Sewell et al., "x86-TSO: A Rigorous and Usable
     * Programmer’s Model for x86 Multiprocessors", CACM, 2010,
     * https://www.cl.cam.ac.uk/~pes20/weakmemory/cacm.pdf)
     */
    static volatile int done = 0;
    static volatile unsigned int c = 0;

    if (!done) {
#if MBEDTLS_AESNI_HAVE_CODE == 2
        static int info[4] = { 0, 0, 0, 0 };
#if defined(_MSC_VER)
        __cpuid(info, 1);
#else
        __cpuid(1, info[0], info[1], info[2], info[3]);
#endif
        c = info[2];
#else /* AESNI using asm */
        asm ("movl  $1, %%eax   \n\t"
             "cpuid             \n\t"
             : "=c" (c)
             :
             : "eax", "ebx", "edx");
#endif /* MBEDTLS_AESNI_HAVE_CODE */
        done = 1;
    }

    return (c & what) != 0;
}
#endif /* !MBEDTLS_AES_USE_HARDWARE_ONLY */

#if MBEDTLS_AESNI_HAVE_CODE == 2

/*
 * AES-NI AES-ECB block en(de)cryption
 */
int mbedtls_aesni_crypt_ecb(mbedtls_aes_context *ctx,
                            int mode,
                            const unsigned char input[16],
                            unsigned char output[16])
{
    const __m128i *rk = (const __m128i *) (ctx->buf + ctx->rk_offset);
    unsigned nr = ctx->nr; // Number of remaining rounds

    // Load round key 0
    __m128i state;
    memcpy(&state, input, 16);
    state = _mm_xor_si128(state, rk[0]);  // state ^= *rk;
    ++rk;
    --nr;

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    if (mode == MBEDTLS_AES_DECRYPT) {
        while (nr != 0) {
            state = _mm_aesdec_si128(state, *rk);
            ++rk;
            --nr;
        }
        state = _mm_aesdeclast_si128(state, *rk);
    } else
#else
    (void) mode;
#endif
    {
        while (nr != 0) {
            state = _mm_aesenc_si128(state, *rk);
            ++rk;
            --nr;
        }
        state = _mm_aesenclast_si128(state, *rk);
    }

    memcpy(output, &state, 16);
    return 0;
}

/*
 * GCM multiplication: c = a times b in GF(2^128)
 * Based on [CLMUL-WP] algorithms 1 (with equation 27) and 5.
 */

static void gcm_clmul(const __m128i aa, const __m128i bb,
                      __m128i *cc, __m128i *dd)
{
    /*
     * Caryless multiplication dd:cc = aa * bb
     * using [CLMUL-WP] algorithm 1 (p. 12).
     */
    *cc = _mm_clmulepi64_si128(aa, bb, 0x00); // a0*b0 = c1:c0
    *dd = _mm_clmulepi64_si128(aa, bb, 0x11); // a1*b1 = d1:d0
    __m128i ee = _mm_clmulepi64_si128(aa, bb, 0x10); // a0*b1 = e1:e0
    __m128i ff = _mm_clmulepi64_si128(aa, bb, 0x01); // a1*b0 = f1:f0
    ff = _mm_xor_si128(ff, ee);                      // e1+f1:e0+f0
    ee = ff;                                         // e1+f1:e0+f0
    ff = _mm_srli_si128(ff, 8);                      // 0:e1+f1
    ee = _mm_slli_si128(ee, 8);                      // e0+f0:0
    *dd = _mm_xor_si128(*dd, ff);                    // d1:d0+e1+f1
    *cc = _mm_xor_si128(*cc, ee);                    // c1+e0+f0:c0
}

static void gcm_shift(__m128i *cc, __m128i *dd)
{
    /* [CMUCL-WP] Algorithm 5 Step 1: shift cc:dd one bit to the left,
     * taking advantage of [CLMUL-WP] eq 27 (p. 18). */
    //                                        // *cc = r1:r0
    //                                        // *dd = r3:r2
    __m128i cc_lo = _mm_slli_epi64(*cc, 1);   // r1<<1:r0<<1
    __m128i dd_lo = _mm_slli_epi64(*dd, 1);   // r3<<1:r2<<1
    __m128i cc_hi = _mm_srli_epi64(*cc, 63);  // r1>>63:r0>>63
    __m128i dd_hi = _mm_srli_epi64(*dd, 63);  // r3>>63:r2>>63
    __m128i xmm5 = _mm_srli_si128(cc_hi, 8);  // 0:r1>>63
    cc_hi = _mm_slli_si128(cc_hi, 8);         // r0>>63:0
    dd_hi = _mm_slli_si128(dd_hi, 8);         // 0:r1>>63

    *cc = _mm_or_si128(cc_lo, cc_hi);         // r1<<1|r0>>63:r0<<1
    *dd = _mm_or_si128(_mm_or_si128(dd_lo, dd_hi), xmm5); // r3<<1|r2>>62:r2<<1|r1>>63
}

static __m128i gcm_reduce(__m128i xx)
{
    //                                            // xx = x1:x0
    /* [CLMUL-WP] Algorithm 5 Step 2 */
    __m128i aa = _mm_slli_epi64(xx, 63);          // x1<<63:x0<<63 = stuff:a
    __m128i bb = _mm_slli_epi64(xx, 62);          // x1<<62:x0<<62 = stuff:b
    __m128i cc = _mm_slli_epi64(xx, 57);          // x1<<57:x0<<57 = stuff:c
    __m128i dd = _mm_slli_si128(_mm_xor_si128(_mm_xor_si128(aa, bb), cc), 8); // a+b+c:0
    return _mm_xor_si128(dd, xx);                 // x1+a+b+c:x0 = d:x0
}

static __m128i gcm_mix(__m128i dx)
{
    /* [CLMUL-WP] Algorithm 5 Steps 3 and 4 */
    __m128i ee = _mm_srli_epi64(dx, 1);           // e1:x0>>1 = e1:e0'
    __m128i ff = _mm_srli_epi64(dx, 2);           // f1:x0>>2 = f1:f0'
    __m128i gg = _mm_srli_epi64(dx, 7);           // g1:x0>>7 = g1:g0'

    // e0'+f0'+g0' is almost e0+f0+g0, except for some missing
    // bits carried from d. Now get those bits back in.
    __m128i eh = _mm_slli_epi64(dx, 63);          // d<<63:stuff
    __m128i fh = _mm_slli_epi64(dx, 62);          // d<<62:stuff
    __m128i gh = _mm_slli_epi64(dx, 57);          // d<<57:stuff
    __m128i hh = _mm_srli_si128(_mm_xor_si128(_mm_xor_si128(eh, fh), gh), 8); // 0:missing bits of d

    return _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(ee, ff), gg), hh), dx);
}

void mbedtls_aesni_gcm_mult(unsigned char c[16],
                            const unsigned char a[16],
                            const unsigned char b[16])
{
    __m128i aa = { 0 }, bb = { 0 }, cc, dd;

    /* The inputs are in big-endian order, so byte-reverse them */
    for (size_t i = 0; i < 16; i++) {
        ((uint8_t *) &aa)[i] = a[15 - i];
        ((uint8_t *) &bb)[i] = b[15 - i];
    }

    gcm_clmul(aa, bb, &cc, &dd);
    gcm_shift(&cc, &dd);
    /*
     * Now reduce modulo the GCM polynomial x^128 + x^7 + x^2 + x + 1
     * using [CLMUL-WP] algorithm 5 (p. 18).
     * Currently dd:cc holds x3:x2:x1:x0 (already shifted).
     */
    __m128i dx = gcm_reduce(cc);
    __m128i xh = gcm_mix(dx);
    cc = _mm_xor_si128(xh, dd); // x3+h1:x2+h0

    /* Now byte-reverse the outputs */
    for (size_t i = 0; i < 16; i++) {
        c[i] = ((uint8_t *) &cc)[15 - i];
    }

    return;
}

/*
 * Compute decryption round keys from encryption round keys
 */
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
void mbedtls_aesni_inverse_key(unsigned char *invkey,
                               const unsigned char *fwdkey, int nr)
{
    __m128i *ik = (__m128i *) invkey;
    const __m128i *fk = (const __m128i *) fwdkey + nr;

    *ik = *fk;
    for (--fk, ++ik; fk > (const __m128i *) fwdkey; --fk, ++ik) {
        *ik = _mm_aesimc_si128(*fk);
    }
    *ik = *fk;
}
#endif

/*
 * Key expansion, 128-bit case
 */
static __m128i aesni_set_rk_128(__m128i state, __m128i xword)
{
    /*
     * Finish generating the next round key.
     *
     * On entry state is r3:r2:r1:r0 and xword is X:stuff:stuff:stuff
     * with X = rot( sub( r3 ) ) ^ RCON (obtained with AESKEYGENASSIST).
     *
     * On exit, xword is r7:r6:r5:r4
     * with r4 = X + r0, r5 = r4 + r1, r6 = r5 + r2, r7 = r6 + r3
     * and this is returned, to be written to the round key buffer.
     */
    xword = _mm_shuffle_epi32(xword, 0xff);   // X:X:X:X
    xword = _mm_xor_si128(xword, state);      // X+r3:X+r2:X+r1:r4
    state = _mm_slli_si128(state, 4);         // r2:r1:r0:0
    xword = _mm_xor_si128(xword, state);      // X+r3+r2:X+r2+r1:r5:r4
    state = _mm_slli_si128(state, 4);         // r1:r0:0:0
    xword = _mm_xor_si128(xword, state);      // X+r3+r2+r1:r6:r5:r4
    state = _mm_slli_si128(state, 4);         // r0:0:0:0
    state = _mm_xor_si128(xword, state);      // r7:r6:r5:r4
    return state;
}

static void aesni_setkey_enc_128(unsigned char *rk_bytes,
                                 const unsigned char *key)
{
    __m128i *rk = (__m128i *) rk_bytes;

    memcpy(&rk[0], key, 16);
    rk[1] = aesni_set_rk_128(rk[0], _mm_aeskeygenassist_si128(rk[0], 0x01));
    rk[2] = aesni_set_rk_128(rk[1], _mm_aeskeygenassist_si128(rk[1], 0x02));
    rk[3] = aesni_set_rk_128(rk[2], _mm_aeskeygenassist_si128(rk[2], 0x04));
    rk[4] = aesni_set_rk_128(rk[3], _mm_aeskeygenassist_si128(rk[3], 0x08));
    rk[5] = aesni_set_rk_128(rk[4], _mm_aeskeygenassist_si128(rk[4], 0x10));
    rk[6] = aesni_set_rk_128(rk[5], _mm_aeskeygenassist_si128(rk[5], 0x20));
    rk[7] = aesni_set_rk_128(rk[6], _mm_aeskeygenassist_si128(rk[6], 0x40));
    rk[8] = aesni_set_rk_128(rk[7], _mm_aeskeygenassist_si128(rk[7], 0x80));
    rk[9] = aesni_set_rk_128(rk[8], _mm_aeskeygenassist_si128(rk[8], 0x1B));
    rk[10] = aesni_set_rk_128(rk[9], _mm_aeskeygenassist_si128(rk[9], 0x36));
}

/*
 * Key expansion, 192-bit case
 */
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static void aesni_set_rk_192(__m128i *state0, __m128i *state1, __m128i xword,
                             unsigned char *rk)
{
    /*
     * Finish generating the next 6 quarter-keys.
     *
     * On entry state0 is r3:r2:r1:r0, state1 is stuff:stuff:r5:r4
     * and xword is stuff:stuff:X:stuff with X = rot( sub( r3 ) ) ^ RCON
     * (obtained with AESKEYGENASSIST).
     *
     * On exit, state0 is r9:r8:r7:r6 and state1 is stuff:stuff:r11:r10
     * and those are written to the round key buffer.
     */
    xword = _mm_shuffle_epi32(xword, 0x55);   // X:X:X:X
    xword = _mm_xor_si128(xword, *state0);    // X+r3:X+r2:X+r1:X+r0
    *state0 = _mm_slli_si128(*state0, 4);     // r2:r1:r0:0
    xword = _mm_xor_si128(xword, *state0);    // X+r3+r2:X+r2+r1:X+r1+r0:X+r0
    *state0 = _mm_slli_si128(*state0, 4);     // r1:r0:0:0
    xword = _mm_xor_si128(xword, *state0);    // X+r3+r2+r1:X+r2+r1+r0:X+r1+r0:X+r0
    *state0 = _mm_slli_si128(*state0, 4);     // r0:0:0:0
    xword = _mm_xor_si128(xword, *state0);    // X+r3+r2+r1+r0:X+r2+r1+r0:X+r1+r0:X+r0
    *state0 = xword;                          // = r9:r8:r7:r6

    xword = _mm_shuffle_epi32(xword, 0xff);   // r9:r9:r9:r9
    xword = _mm_xor_si128(xword, *state1);    // stuff:stuff:r9+r5:r9+r4
    *state1 = _mm_slli_si128(*state1, 4);     // stuff:stuff:r4:0
    xword = _mm_xor_si128(xword, *state1);    // stuff:stuff:r9+r5+r4:r9+r4
    *state1 = xword;                          // = stuff:stuff:r11:r10

    /* Store state0 and the low half of state1 into rk, which is conceptually
     * an array of 24-byte elements. Since 24 is not a multiple of 16,
     * rk is not necessarily aligned so just `*rk = *state0` doesn't work. */
    memcpy(rk, state0, 16);
    memcpy(rk + 16, state1, 8);
}

static void aesni_setkey_enc_192(unsigned char *rk,
                                 const unsigned char *key)
{
    /* First round: use original key */
    memcpy(rk, key, 24);
    /* aes.c guarantees that rk is aligned on a 16-byte boundary. */
    __m128i state0 = ((__m128i *) rk)[0];
    __m128i state1 = _mm_loadl_epi64(((__m128i *) rk) + 1);

    aesni_set_rk_192(&state0, &state1, _mm_aeskeygenassist_si128(state1, 0x01), rk + 24 * 1);
    aesni_set_rk_192(&state0, &state1, _mm_aeskeygenassist_si128(state1, 0x02), rk + 24 * 2);
    aesni_set_rk_192(&state0, &state1, _mm_aeskeygenassist_si128(state1, 0x04), rk + 24 * 3);
    aesni_set_rk_192(&state0, &state1, _mm_aeskeygenassist_si128(state1, 0x08), rk + 24 * 4);
    aesni_set_rk_192(&state0, &state1, _mm_aeskeygenassist_si128(state1, 0x10), rk + 24 * 5);
    aesni_set_rk_192(&state0, &state1, _mm_aeskeygenassist_si128(state1, 0x20), rk + 24 * 6);
    aesni_set_rk_192(&state0, &state1, _mm_aeskeygenassist_si128(state1, 0x40), rk + 24 * 7);
    aesni_set_rk_192(&state0, &state1, _mm_aeskeygenassist_si128(state1, 0x80), rk + 24 * 8);
}
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */

/*
 * Key expansion, 256-bit case
 */
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static void aesni_set_rk_256(__m128i state0, __m128i state1, __m128i xword,
                             __m128i *rk0, __m128i *rk1)
{
    /*
     * Finish generating the next two round keys.
     *
     * On entry state0 is r3:r2:r1:r0, state1 is r7:r6:r5:r4 and
     * xword is X:stuff:stuff:stuff with X = rot( sub( r7 )) ^ RCON
     * (obtained with AESKEYGENASSIST).
     *
     * On exit, *rk0 is r11:r10:r9:r8 and *rk1 is r15:r14:r13:r12
     */
    xword = _mm_shuffle_epi32(xword, 0xff);
    xword = _mm_xor_si128(xword, state0);
    state0 = _mm_slli_si128(state0, 4);
    xword = _mm_xor_si128(xword, state0);
    state0 = _mm_slli_si128(state0, 4);
    xword = _mm_xor_si128(xword, state0);
    state0 = _mm_slli_si128(state0, 4);
    state0 = _mm_xor_si128(state0, xword);
    *rk0 = state0;

    /* Set xword to stuff:Y:stuff:stuff with Y = subword( r11 )
     * and proceed to generate next round key from there */
    xword = _mm_aeskeygenassist_si128(state0, 0x00);
    xword = _mm_shuffle_epi32(xword, 0xaa);
    xword = _mm_xor_si128(xword, state1);
    state1 = _mm_slli_si128(state1, 4);
    xword = _mm_xor_si128(xword, state1);
    state1 = _mm_slli_si128(state1, 4);
    xword = _mm_xor_si128(xword, state1);
    state1 = _mm_slli_si128(state1, 4);
    state1 = _mm_xor_si128(state1, xword);
    *rk1 = state1;
}

static void aesni_setkey_enc_256(unsigned char *rk_bytes,
                                 const unsigned char *key)
{
    __m128i *rk = (__m128i *) rk_bytes;

    memcpy(&rk[0], key, 16);
    memcpy(&rk[1], key + 16, 16);

    /*
     * Main "loop" - Generating one more key than necessary,
     * see definition of mbedtls_aes_context.buf
     */
    aesni_set_rk_256(rk[0], rk[1], _mm_aeskeygenassist_si128(rk[1], 0x01), &rk[2], &rk[3]);
    aesni_set_rk_256(rk[2], rk[3], _mm_aeskeygenassist_si128(rk[3], 0x02), &rk[4], &rk[5]);
    aesni_set_rk_256(rk[4], rk[5], _mm_aeskeygenassist_si128(rk[5], 0x04), &rk[6], &rk[7]);
    aesni_set_rk_256(rk[6], rk[7], _mm_aeskeygenassist_si128(rk[7], 0x08), &rk[8], &rk[9]);
    aesni_set_rk_256(rk[8], rk[9], _mm_aeskeygenassist_si128(rk[9], 0x10), &rk[10], &rk[11]);
    aesni_set_rk_256(rk[10], rk[11], _mm_aeskeygenassist_si128(rk[11], 0x20), &rk[12], &rk[13]);
    aesni_set_rk_256(rk[12], rk[13], _mm_aeskeygenassist_si128(rk[13], 0x40), &rk[14], &rk[15]);
}
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */

#if defined(MBEDTLS_POP_TARGET_PRAGMA)
#if defined(__clang__)
#pragma clang attribute pop
#elif defined(__GNUC__)
#pragma GCC pop_options
#endif
#undef MBEDTLS_POP_TARGET_PRAGMA
#endif

#else /* MBEDTLS_AESNI_HAVE_CODE == 1 */

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#warning \
    "MBEDTLS_AESNI_C is known to cause spurious error reports with some memory sanitizers as they do not understand the assembly code."
#endif
#endif

/*
 * Binutils needs to be at least 2.19 to support AES-NI instructions.
 * Unfortunately, a lot of users have a lower version now (2014-04).
 * Emit bytecode directly in order to support "old" version of gas.
 *
 * Opcodes from the Intel architecture reference manual, vol. 3.
 * We always use registers, so we don't need prefixes for memory operands.
 * Operand macros are in gas order (src, dst) as opposed to Intel order
 * (dst, src) in order to blend better into the surrounding assembly code.
 */
#define AESDEC(regs)      ".byte 0x66,0x0F,0x38,0xDE," regs "\n\t"
#define AESDECLAST(regs)  ".byte 0x66,0x0F,0x38,0xDF," regs "\n\t"
#define AESENC(regs)      ".byte 0x66,0x0F,0x38,0xDC," regs "\n\t"
#define AESENCLAST(regs)  ".byte 0x66,0x0F,0x38,0xDD," regs "\n\t"
#define AESIMC(regs)      ".byte 0x66,0x0F,0x38,0xDB," regs "\n\t"
#define AESKEYGENA(regs, imm)  ".byte 0x66,0x0F,0x3A,0xDF," regs "," imm "\n\t"
#define PCLMULQDQ(regs, imm)   ".byte 0x66,0x0F,0x3A,0x44," regs "," imm "\n\t"

#define xmm0_xmm0   "0xC0"
#define xmm0_xmm1   "0xC8"
#define xmm0_xmm2   "0xD0"
#define xmm0_xmm3   "0xD8"
#define xmm0_xmm4   "0xE0"
#define xmm1_xmm0   "0xC1"
#define xmm1_xmm2   "0xD1"

/*
 * AES-NI AES-ECB block en(de)cryption
 */
int mbedtls_aesni_crypt_ecb(mbedtls_aes_context *ctx,
                            int mode,
                            const unsigned char input[16],
                            unsigned char output[16])
{
    asm ("movdqu    (%3), %%xmm0    \n\t" // load input
         "movdqu    (%1), %%xmm1    \n\t" // load round key 0
         "pxor      %%xmm1, %%xmm0  \n\t" // round 0
         "add       $16, %1         \n\t" // point to next round key
         "subl      $1, %0          \n\t" // normal rounds = nr - 1
         "test      %2, %2          \n\t" // mode?
         "jz        2f              \n\t" // 0 = decrypt

         "1:                        \n\t" // encryption loop
         "movdqu    (%1), %%xmm1    \n\t" // load round key
         AESENC(xmm1_xmm0)                // do round
         "add       $16, %1         \n\t" // point to next round key
         "subl      $1, %0          \n\t" // loop
         "jnz       1b              \n\t"
         "movdqu    (%1), %%xmm1    \n\t" // load round key
         AESENCLAST(xmm1_xmm0)            // last round
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
         "jmp       3f              \n\t"

         "2:                        \n\t" // decryption loop
         "movdqu    (%1), %%xmm1    \n\t"
         AESDEC(xmm1_xmm0)                // do round
         "add       $16, %1         \n\t"
         "subl      $1, %0          \n\t"
         "jnz       2b              \n\t"
         "movdqu    (%1), %%xmm1    \n\t" // load round key
         AESDECLAST(xmm1_xmm0)            // last round
#endif

         "3:                        \n\t"
         "movdqu    %%xmm0, (%4)    \n\t" // export output
         :
         : "r" (ctx->nr), "r" (ctx->buf + ctx->rk_offset), "r" (mode), "r" (input), "r" (output)
         : "memory", "cc", "xmm0", "xmm1", "0", "1");


    return 0;
}

/*
 * GCM multiplication: c = a times b in GF(2^128)
 * Based on [CLMUL-WP] algorithms 1 (with equation 27) and 5.
 */
void mbedtls_aesni_gcm_mult(unsigned char c[16],
                            const unsigned char a[16],
                            const unsigned char b[16])
{
    unsigned char aa[16], bb[16], cc[16];
    size_t i;

    /* The inputs are in big-endian order, so byte-reverse them */
    for (i = 0; i < 16; i++) {
        aa[i] = a[15 - i];
        bb[i] = b[15 - i];
    }

    asm ("movdqu (%0), %%xmm0               \n\t" // a1:a0
         "movdqu (%1), %%xmm1               \n\t" // b1:b0

         /*
          * Caryless multiplication xmm2:xmm1 = xmm0 * xmm1
          * using [CLMUL-WP] algorithm 1 (p. 12).
          */
         "movdqa %%xmm1, %%xmm2             \n\t" // copy of b1:b0
         "movdqa %%xmm1, %%xmm3             \n\t" // same
         "movdqa %%xmm1, %%xmm4             \n\t" // same
         PCLMULQDQ(xmm0_xmm1, "0x00")             // a0*b0 = c1:c0
         PCLMULQDQ(xmm0_xmm2, "0x11")             // a1*b1 = d1:d0
         PCLMULQDQ(xmm0_xmm3, "0x10")             // a0*b1 = e1:e0
         PCLMULQDQ(xmm0_xmm4, "0x01")             // a1*b0 = f1:f0
         "pxor %%xmm3, %%xmm4               \n\t" // e1+f1:e0+f0
         "movdqa %%xmm4, %%xmm3             \n\t" // same
         "psrldq $8, %%xmm4                 \n\t" // 0:e1+f1
         "pslldq $8, %%xmm3                 \n\t" // e0+f0:0
         "pxor %%xmm4, %%xmm2               \n\t" // d1:d0+e1+f1
         "pxor %%xmm3, %%xmm1               \n\t" // c1+e0+f1:c0

         /*
          * Now shift the result one bit to the left,
          * taking advantage of [CLMUL-WP] eq 27 (p. 18)
          */
         "movdqa %%xmm1, %%xmm3             \n\t" // r1:r0
         "movdqa %%xmm2, %%xmm4             \n\t" // r3:r2
         "psllq $1, %%xmm1                  \n\t" // r1<<1:r0<<1
         "psllq $1, %%xmm2                  \n\t" // r3<<1:r2<<1
         "psrlq $63, %%xmm3                 \n\t" // r1>>63:r0>>63
         "psrlq $63, %%xmm4                 \n\t" // r3>>63:r2>>63
         "movdqa %%xmm3, %%xmm5             \n\t" // r1>>63:r0>>63
         "pslldq $8, %%xmm3                 \n\t" // r0>>63:0
         "pslldq $8, %%xmm4                 \n\t" // r2>>63:0
         "psrldq $8, %%xmm5                 \n\t" // 0:r1>>63
         "por %%xmm3, %%xmm1                \n\t" // r1<<1|r0>>63:r0<<1
         "por %%xmm4, %%xmm2                \n\t" // r3<<1|r2>>62:r2<<1
         "por %%xmm5, %%xmm2                \n\t" // r3<<1|r2>>62:r2<<1|r1>>63

         /*
          * Now reduce modulo the GCM polynomial x^128 + x^7 + x^2 + x + 1
          * using [CLMUL-WP] algorithm 5 (p. 18).
          * Currently xmm2:xmm1 holds x3:x2:x1:x0 (already shifted).
          */
         /* Step 2 (1) */
         "movdqa %%xmm1, %%xmm3             \n\t" // x1:x0
         "movdqa %%xmm1, %%xmm4             \n\t" // same
         "movdqa %%xmm1, %%xmm5             \n\t" // same
         "psllq $63, %%xmm3                 \n\t" // x1<<63:x0<<63 = stuff:a
         "psllq $62, %%xmm4                 \n\t" // x1<<62:x0<<62 = stuff:b
         "psllq $57, %%xmm5                 \n\t" // x1<<57:x0<<57 = stuff:c

         /* Step 2 (2) */
         "pxor %%xmm4, %%xmm3               \n\t" // stuff:a+b
         "pxor %%xmm5, %%xmm3               \n\t" // stuff:a+b+c
         "pslldq $8, %%xmm3                 \n\t" // a+b+c:0
         "pxor %%xmm3, %%xmm1               \n\t" // x1+a+b+c:x0 = d:x0

         /* Steps 3 and 4 */
         "movdqa %%xmm1,%%xmm0              \n\t" // d:x0
         "movdqa %%xmm1,%%xmm4              \n\t" // same
         "movdqa %%xmm1,%%xmm5              \n\t" // same
         "psrlq $1, %%xmm0                  \n\t" // e1:x0>>1 = e1:e0'
         "psrlq $2, %%xmm4                  \n\t" // f1:x0>>2 = f1:f0'
         "psrlq $7, %%xmm5                  \n\t" // g1:x0>>7 = g1:g0'
         "pxor %%xmm4, %%xmm0               \n\t" // e1+f1:e0'+f0'
         "pxor %%xmm5, %%xmm0               \n\t" // e1+f1+g1:e0'+f0'+g0'
         // e0'+f0'+g0' is almost e0+f0+g0, ex\tcept for some missing
         // bits carried from d. Now get those\t bits back in.
         "movdqa %%xmm1,%%xmm3              \n\t" // d:x0
         "movdqa %%xmm1,%%xmm4              \n\t" // same
         "movdqa %%xmm1,%%xmm5              \n\t" // same
         "psllq $63, %%xmm3                 \n\t" // d<<63:stuff
         "psllq $62, %%xmm4                 \n\t" // d<<62:stuff
         "psllq $57, %%xmm5                 \n\t" // d<<57:stuff
         "pxor %%xmm4, %%xmm3               \n\t" // d<<63+d<<62:stuff
         "pxor %%xmm5, %%xmm3               \n\t" // missing bits of d:stuff
         "psrldq $8, %%xmm3                 \n\t" // 0:missing bits of d
         "pxor %%xmm3, %%xmm0               \n\t" // e1+f1+g1:e0+f0+g0
         "pxor %%xmm1, %%xmm0               \n\t" // h1:h0
         "pxor %%xmm2, %%xmm0               \n\t" // x3+h1:x2+h0

         "movdqu %%xmm0, (%2)               \n\t" // done
         :
         : "r" (aa), "r" (bb), "r" (cc)
         : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");

    /* Now byte-reverse the outputs */
    for (i = 0; i < 16; i++) {
        c[i] = cc[15 - i];
    }

    return;
}

/*
 * Compute decryption round keys from encryption round keys
 */
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
void mbedtls_aesni_inverse_key(unsigned char *invkey,
                               const unsigned char *fwdkey, int nr)
{
    unsigned char *ik = invkey;
    const unsigned char *fk = fwdkey + 16 * nr;

    memcpy(ik, fk, 16);

    for (fk -= 16, ik += 16; fk > fwdkey; fk -= 16, ik += 16) {
        asm ("movdqu (%0), %%xmm0       \n\t"
             AESIMC(xmm0_xmm0)
             "movdqu %%xmm0, (%1)       \n\t"
             :
             : "r" (fk), "r" (ik)
             : "memory", "xmm0");
    }

    memcpy(ik, fk, 16);
}
#endif

/*
 * Key expansion, 128-bit case
 */
static void aesni_setkey_enc_128(unsigned char *rk,
                                 const unsigned char *key)
{
    asm ("movdqu (%1), %%xmm0               \n\t" // copy the original key
         "movdqu %%xmm0, (%0)               \n\t" // as round key 0
         "jmp 2f                            \n\t" // skip auxiliary routine

         /*
          * Finish generating the next round key.
          *
          * On entry xmm0 is r3:r2:r1:r0 and xmm1 is X:stuff:stuff:stuff
          * with X = rot( sub( r3 ) ) ^ RCON.
          *
          * On exit, xmm0 is r7:r6:r5:r4
          * with r4 = X + r0, r5 = r4 + r1, r6 = r5 + r2, r7 = r6 + r3
          * and those are written to the round key buffer.
          */
         "1:                                \n\t"
         "pshufd $0xff, %%xmm1, %%xmm1      \n\t" // X:X:X:X
         "pxor %%xmm0, %%xmm1               \n\t" // X+r3:X+r2:X+r1:r4
         "pslldq $4, %%xmm0                 \n\t" // r2:r1:r0:0
         "pxor %%xmm0, %%xmm1               \n\t" // X+r3+r2:X+r2+r1:r5:r4
         "pslldq $4, %%xmm0                 \n\t" // etc
         "pxor %%xmm0, %%xmm1               \n\t"
         "pslldq $4, %%xmm0                 \n\t"
         "pxor %%xmm1, %%xmm0               \n\t" // update xmm0 for next time!
         "add $16, %0                       \n\t" // point to next round key
         "movdqu %%xmm0, (%0)               \n\t" // write it
         "ret                               \n\t"

         /* Main "loop" */
         "2:                                \n\t"
         AESKEYGENA(xmm0_xmm1, "0x01")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x02")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x04")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x08")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x10")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x20")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x40")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x80")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x1B")      "call 1b \n\t"
         AESKEYGENA(xmm0_xmm1, "0x36")      "call 1b \n\t"
         :
         : "r" (rk), "r" (key)
         : "memory", "cc", "xmm0", "xmm1", "0");
}

/*
 * Key expansion, 192-bit case
 */
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static void aesni_setkey_enc_192(unsigned char *rk,
                                 const unsigned char *key)
{
    asm ("movdqu (%1), %%xmm0   \n\t" // copy original round key
         "movdqu %%xmm0, (%0)   \n\t"
         "add $16, %0           \n\t"
         "movq 16(%1), %%xmm1   \n\t"
         "movq %%xmm1, (%0)     \n\t"
         "add $8, %0            \n\t"
         "jmp 2f                \n\t" // skip auxiliary routine

         /*
          * Finish generating the next 6 quarter-keys.
          *
          * On entry xmm0 is r3:r2:r1:r0, xmm1 is stuff:stuff:r5:r4
          * and xmm2 is stuff:stuff:X:stuff with X = rot( sub( r3 ) ) ^ RCON.
          *
          * On exit, xmm0 is r9:r8:r7:r6 and xmm1 is stuff:stuff:r11:r10
          * and those are written to the round key buffer.
          */
         "1:                            \n\t"
         "pshufd $0x55, %%xmm2, %%xmm2  \n\t" // X:X:X:X
         "pxor %%xmm0, %%xmm2           \n\t" // X+r3:X+r2:X+r1:r4
         "pslldq $4, %%xmm0             \n\t" // etc
         "pxor %%xmm0, %%xmm2           \n\t"
         "pslldq $4, %%xmm0             \n\t"
         "pxor %%xmm0, %%xmm2           \n\t"
         "pslldq $4, %%xmm0             \n\t"
         "pxor %%xmm2, %%xmm0           \n\t" // update xmm0 = r9:r8:r7:r6
         "movdqu %%xmm0, (%0)           \n\t"
         "add $16, %0                   \n\t"
         "pshufd $0xff, %%xmm0, %%xmm2  \n\t" // r9:r9:r9:r9
         "pxor %%xmm1, %%xmm2           \n\t" // stuff:stuff:r9+r5:r10
         "pslldq $4, %%xmm1             \n\t" // r2:r1:r0:0
         "pxor %%xmm2, %%xmm1           \n\t" // xmm1 = stuff:stuff:r11:r10
         "movq %%xmm1, (%0)             \n\t"
         "add $8, %0                    \n\t"
         "ret                           \n\t"

         "2:                            \n\t"
         AESKEYGENA(xmm1_xmm2, "0x01")  "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x02")  "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x04")  "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x08")  "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x10")  "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x20")  "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x40")  "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x80")  "call 1b \n\t"

         :
         : "r" (rk), "r" (key)
         : "memory", "cc", "xmm0", "xmm1", "xmm2", "0");
}
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */

/*
 * Key expansion, 256-bit case
 */
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static void aesni_setkey_enc_256(unsigned char *rk,
                                 const unsigned char *key)
{
    asm ("movdqu (%1), %%xmm0           \n\t"
         "movdqu %%xmm0, (%0)           \n\t"
         "add $16, %0                   \n\t"
         "movdqu 16(%1), %%xmm1         \n\t"
         "movdqu %%xmm1, (%0)           \n\t"
         "jmp 2f                        \n\t" // skip auxiliary routine

         /*
          * Finish generating the next two round keys.
          *
          * On entry xmm0 is r3:r2:r1:r0, xmm1 is r7:r6:r5:r4 and
          * xmm2 is X:stuff:stuff:stuff with X = rot( sub( r7 )) ^ RCON
          *
          * On exit, xmm0 is r11:r10:r9:r8 and xmm1 is r15:r14:r13:r12
          * and those have been written to the output buffer.
          */
         "1:                                \n\t"
         "pshufd $0xff, %%xmm2, %%xmm2      \n\t"
         "pxor %%xmm0, %%xmm2               \n\t"
         "pslldq $4, %%xmm0                 \n\t"
         "pxor %%xmm0, %%xmm2               \n\t"
         "pslldq $4, %%xmm0                 \n\t"
         "pxor %%xmm0, %%xmm2               \n\t"
         "pslldq $4, %%xmm0                 \n\t"
         "pxor %%xmm2, %%xmm0               \n\t"
         "add $16, %0                       \n\t"
         "movdqu %%xmm0, (%0)               \n\t"

         /* Set xmm2 to stuff:Y:stuff:stuff with Y = subword( r11 )
          * and proceed to generate next round key from there */
         AESKEYGENA(xmm0_xmm2, "0x00")
         "pshufd $0xaa, %%xmm2, %%xmm2      \n\t"
         "pxor %%xmm1, %%xmm2               \n\t"
         "pslldq $4, %%xmm1                 \n\t"
         "pxor %%xmm1, %%xmm2               \n\t"
         "pslldq $4, %%xmm1                 \n\t"
         "pxor %%xmm1, %%xmm2               \n\t"
         "pslldq $4, %%xmm1                 \n\t"
         "pxor %%xmm2, %%xmm1               \n\t"
         "add $16, %0                       \n\t"
         "movdqu %%xmm1, (%0)               \n\t"
         "ret                               \n\t"

         /*
          * Main "loop" - Generating one more key than necessary,
          * see definition of mbedtls_aes_context.buf
          */
         "2:                                \n\t"
         AESKEYGENA(xmm1_xmm2, "0x01")      "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x02")      "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x04")      "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x08")      "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x10")      "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x20")      "call 1b \n\t"
         AESKEYGENA(xmm1_xmm2, "0x40")      "call 1b \n\t"
         :
         : "r" (rk), "r" (key)
         : "memory", "cc", "xmm0", "xmm1", "xmm2", "0");
}
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */

#endif  /* MBEDTLS_AESNI_HAVE_CODE */

/*
 * Key expansion, wrapper
 */
int mbedtls_aesni_setkey_enc(unsigned char *rk,
                             const unsigned char *key,
                             size_t bits)
{
    switch (bits) {
        case 128: aesni_setkey_enc_128(rk, key); break;
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
        case 192: aesni_setkey_enc_192(rk, key); break;
        case 256: aesni_setkey_enc_256(rk, key); break;
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */
        default: return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    return 0;
}

#endif /* MBEDTLS_AESNI_HAVE_CODE */

#endif /* MBEDTLS_AESNI_C */

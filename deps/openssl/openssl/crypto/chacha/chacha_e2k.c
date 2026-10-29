/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>

#include "internal/endian.h"
#include "crypto/chacha.h"
#include "crypto/ctype.h"

typedef uint32_t u32;
typedef uint8_t u8;

#include <stdint.h>
#include <e2kintrin.h>

#if __iset__ >= 5 /* 128-bit SIMD */

/* QUARTERROUND updates a, b, c, d with a ChaCha "quarter" round. */
#define QUARTERROUND(a, b, c, d) (                                                                                  \
    x[a] = __builtin_e2k_qpaddw(x[a], x[b]), x[d] = __builtin_e2k_qpsrcw(__builtin_e2k_qpxor(x[d], x[a]), 32 - 16), \
    x[c] = __builtin_e2k_qpaddw(x[c], x[d]), x[b] = __builtin_e2k_qpsrcw(__builtin_e2k_qpxor(x[b], x[c]), 32 - 12), \
    x[a] = __builtin_e2k_qpaddw(x[a], x[b]), x[d] = __builtin_e2k_qpsrcw(__builtin_e2k_qpxor(x[d], x[a]), 32 - 8),  \
    x[c] = __builtin_e2k_qpaddw(x[c], x[d]), x[b] = __builtin_e2k_qpsrcw(__builtin_e2k_qpxor(x[b], x[c]), 32 - 7))

/* chacha_core performs 20 rounds of ChaCha on the input words in
 * |input| and writes the 64 output bytes to |output|.
 */
static inline __attribute__((__always_inline__)) void chacha20_core_x4(__v2di *output, const __v2di input[16])
{
    __v2di x[16];
    int i;
    memcpy(x, input, sizeof(x));

    for (i = 20; i > 0; i -= 2) {
        QUARTERROUND(0, 4, 8, 12);
        QUARTERROUND(1, 5, 9, 13);
        QUARTERROUND(2, 6, 10, 14);
        QUARTERROUND(3, 7, 11, 15);
        QUARTERROUND(0, 5, 10, 15);
        QUARTERROUND(1, 6, 11, 12);
        QUARTERROUND(2, 7, 8, 13);
        QUARTERROUND(3, 4, 9, 14);
    }

    for (i = 0; i < 16; ++i)
        output[i] = __builtin_e2k_qpaddw(x[i], input[i]);
}

void ChaCha20_ctr32(unsigned char *out, const unsigned char *inp, size_t len,
    const unsigned int key[8], const unsigned int counter[4])
{
    u32 input[16];
    __v2di input_x4[16];
    __v2di buf[16];
    size_t todo, i;

    /* sigma constant "expand 32-byte k" in little-endian encoding */
    input[0] = ((u32)('e')) | ((u32)('x') << 8) | ((u32)('p') << 16) | ((u32)('a') << 24);
    input[1] = ((u32)('n')) | ((u32)('d') << 8) | ((u32)(' ') << 16) | ((u32)('3') << 24);
    input[2] = ((u32)('2')) | ((u32)('-') << 8) | ((u32)('b') << 16) | ((u32)('y') << 24);
    input[3] = ((u32)('t')) | ((u32)('e') << 8) | ((u32)(' ') << 16) | ((u32)('k') << 24);

    input[4] = key[0];
    input[5] = key[1];
    input[6] = key[2];
    input[7] = key[3];
    input[8] = key[4];
    input[9] = key[5];
    input[10] = key[6];
    input[11] = key[7];

    input[12] = counter[0];
    input[13] = counter[1];
    input[14] = counter[2];
    input[15] = counter[3];

    for (i = 0; i < 16; i++) {
        unsigned long long w = input[i] * 0x100000001LL;
        input_x4[i] = __builtin_e2k_qppackdl(w, w);
    }

    input_x4[12] = __builtin_e2k_qpaddw(input_x4[12], (__v2di) { 0x100000000LL, 0x300000002LL });

    while (len > 0) {
        __v2di buf_tran[16];

        chacha20_core_x4(buf, input_x4);

        for (i = 0; i < 16; i += 4) {
            const __v2di f1 = __builtin_e2k_qppackdl(0x1f1e1d1c0f0e0d0cLL, 0x1716151407060504LL);
            const __v2di f0 = __builtin_e2k_qppackdl(0x1b1a19180b0a0908LL, 0x1312111003020100LL);

            const __v2di f3 = __builtin_e2k_qppackdl(0x1f1e1d1c1b1a1918LL, 0x0f0e0d0c0b0a0908LL);
            const __v2di f2 = __builtin_e2k_qppackdl(0x1716151413121110LL, 0x0706050403020100LL);

            __v2di t0 = __builtin_e2k_qppermb(buf[i + 1], buf[i + 0], f0);
            __v2di t1 = __builtin_e2k_qppermb(buf[i + 1], buf[i + 0], f1);
            __v2di t2 = __builtin_e2k_qppermb(buf[i + 3], buf[i + 2], f0);
            __v2di t3 = __builtin_e2k_qppermb(buf[i + 3], buf[i + 2], f1);

            buf_tran[i / 4 + 0] = __builtin_e2k_qppermb(t2, t0, f2);
            buf_tran[i / 4 + 4] = __builtin_e2k_qppermb(t3, t1, f2);
            buf_tran[i / 4 + 8] = __builtin_e2k_qppermb(t2, t0, f3);
            buf_tran[i / 4 + 12] = __builtin_e2k_qppermb(t3, t1, f3);
        }

        todo = sizeof(buf);
        if (__builtin_expect(len < todo, 0)) {
            todo = len & ~(size_t)15;

            for (i = 0; i < todo; i += 16) {
                *(__v2di *)&out[i] = __builtin_e2k_qpxor(*(__v2di *)&inp[i], buf_tran[i / 16]);
            }
            for (; i < len; i++) {
                out[i] = inp[i] ^ ((u8 *)buf_tran)[i];
            }
            return;
        }

        for (i = 0; i < todo; i += 16) {
            *(__v2di *)&out[i] = __builtin_e2k_qpxor(*(__v2di *)&inp[i], buf_tran[i / 16]);
        }

        /*
         * Advance 32-bit counters. Note that as subroutine is so to
         * say nonce-agnostic, this limited counter width doesn't
         * prevent caller from implementing wider counter. It would
         * simply take two calls split on counter overflow...
         */
        input_x4[12] = __builtin_e2k_qpaddw(input_x4[12], (__v2di) { 0x400000004LL, 0x400000004LL });

        out += todo;
        inp += todo;
        len -= todo;
    }
}

#else /* 64-bit SIMD */

/* QUARTERROUND updates a, b, c, d with a ChaCha "quarter" round. */
#define QUARTERROUND(a, b, c, d) (                                                                                                                                          \
    x[a] = __builtin_e2k_paddw(x[a], x[b]), tt = __builtin_e2k_pxord(x[d], x[a]), x[d] = __builtin_e2k_pshufb(tt, tt, 0x0504070601000302ull),                               \
    x[c] = __builtin_e2k_paddw(x[c], x[d]), tt = __builtin_e2k_pxord(x[b], x[c]), x[b] = __builtin_e2k_pord(__builtin_e2k_psllw(tt, 12), __builtin_e2k_psrlw(tt, 32 - 12)), \
    x[a] = __builtin_e2k_paddw(x[a], x[b]), tt = __builtin_e2k_pxord(x[d], x[a]), x[d] = __builtin_e2k_pshufb(tt, tt, 0x0605040702010003ull),                               \
    x[c] = __builtin_e2k_paddw(x[c], x[d]), tt = __builtin_e2k_pxord(x[b], x[c]), x[b] = __builtin_e2k_pord(__builtin_e2k_psllw(tt, 7), __builtin_e2k_psrlw(tt, 32 - 7)))

/* chacha_core performs 20 rounds of ChaCha on the input words in *inp
 * and writes the 64 output bytes to *out .
 */
void ChaCha20_ctr32(unsigned char *out, const unsigned char *inp,
    size_t len, const unsigned int key[8],
    const unsigned int counter[4])
{
    u32 input[16];
    uint64_t input_x2[16];
    uint64_t buf[16];
    size_t todo, i;

    /* sigma constant "expand 32-byte k" in little-endian encoding */
    input[0] = ((u32)('e')) | ((u32)('x') << 8) | ((u32)('p') << 16) | ((u32)('a') << 24);
    input[1] = ((u32)('n')) | ((u32)('d') << 8) | ((u32)(' ') << 16) | ((u32)('3') << 24);
    input[2] = ((u32)('2')) | ((u32)('-') << 8) | ((u32)('b') << 16) | ((u32)('y') << 24);
    input[3] = ((u32)('t')) | ((u32)('e') << 8) | ((u32)(' ') << 16) | ((u32)('k') << 24);

    input[4] = key[0];
    input[5] = key[1];
    input[6] = key[2];
    input[7] = key[3];
    input[8] = key[4];
    input[9] = key[5];
    input[10] = key[6];
    input[11] = key[7];

    input[12] = counter[0];
    input[13] = counter[1];
    input[14] = counter[2];
    input[15] = counter[3];

#pragma unroll(16)
    for (i = 0; i < 16; i++) {
        input_x2[i] = input[i] * 0x100000001ull;
    }

    input_x2[12] = __builtin_e2k_paddw(input_x2[12], 0x100000000ull);

    while (len > 0) {
        uint64_t buf_tran[16];
        uint64_t x[16], tt;
        uint64_t *__restrict__ outw = (uint64_t *)out;

        for (i = 0; i < 16; ++i)
            x[i] = input_x2[i];

        for (i = 20; i > 0; i -= 2) {
            QUARTERROUND(0, 4, 8, 12);
            QUARTERROUND(1, 5, 9, 13);
            QUARTERROUND(2, 6, 10, 14);
            QUARTERROUND(3, 7, 11, 15);
            QUARTERROUND(0, 5, 10, 15);
            QUARTERROUND(1, 6, 11, 12);
            QUARTERROUND(2, 7, 8, 13);
            QUARTERROUND(3, 4, 9, 14);
        }

        for (i = 0; i < 16; ++i)
            buf[i] = __builtin_e2k_paddw(x[i], input_x2[i]);

#pragma unroll(8)
        for (i = 0; i < 16; i += 2) {
            const uint64_t fmtl = 0x0b0a090803020100ull;
            const uint64_t fmtr = 0x0f0e0d0c07060504ull;

            buf_tran[i / 2 + 0] = __builtin_e2k_pshufb(buf[i + 1], buf[i + 0], fmtl);
            buf_tran[i / 2 + 8] = __builtin_e2k_pshufb(buf[i + 1], buf[i + 0], fmtr);
        }

        todo = sizeof(buf);
        if (__builtin_expect(len < todo, 0)) {
            todo = len & ~(size_t)7;

#pragma loop count(16)
            for (i = 0; i < todo; i += 8) {
                *(uint64_t *)&out[i] = __builtin_e2k_pxord(*(uint64_t *)&inp[i], buf_tran[i / 8]);
            }
#pragma loop count(7)
            for (; i < len; i++) {
                out[i] = inp[i] ^ ((u8 *)buf_tran)[i];
            }
            return;
        }

#pragma unroll(16)
        for (i = 0; i < todo; i += 8) {
            *outw++ = __builtin_e2k_pxord(*(uint64_t *)&inp[i], buf_tran[i / 8]);
        }

        /*
         * Advance 32-bit counters. Note that as subroutine is so to
         * say nonce-agnostic, this limited counter width doesn't
         * prevent caller from implementing wider counter. It would
         * simply take two calls split on counter overflow...
         */
        input_x2[12] = __builtin_e2k_paddw(input_x2[12], 0x200000002ull);

        out += todo;
        inp += todo;
        len -= todo;
    }
}
#endif

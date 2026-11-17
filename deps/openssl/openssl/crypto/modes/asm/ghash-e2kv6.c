/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdint.h>
#include <e2kintrin.h>

#include "crypto/modes.h"

static __attribute__((__always_inline__)) inline __v2di reverse_vector(const __v2di in)
{
    __v2di fmt = __builtin_e2k_qppackdl(0x0001020304050607LL, 0x08090a0b0c0d0e0fLL);
    return __builtin_e2k_qppermb(in, in, fmt);
}

static __attribute__((__always_inline__)) inline __v2di gcm_reduce(__v2di B0, __v2di B1)
{
    __v2di X0 = __builtin_e2k_qpsrlw(B1, 31);
    __v2di X1 = __builtin_e2k_qpsllw(B1, 1);
    __v2di X2 = __builtin_e2k_qpsrlw(B0, 31);
    __v2di X3 = __builtin_e2k_qpsllw(B0, 1);

    X3 = X3 | __builtin_e2k_qpshufb(X0, X0, __builtin_e2k_qppackdl(0x8080808080808080LL, 0x808080800f0e0d0cLL))
        | __builtin_e2k_qpshufb(X2, X2, __builtin_e2k_qppackdl(0x0b0a090807060504LL, 0x0302010080808080LL));

    X1 = X1 | __builtin_e2k_qpshufb(X0, X0, __builtin_e2k_qppackdl(0x0b0a090807060504LL, 0x0302010080808080LL));

    X0 = __builtin_e2k_qpsllw(X1, 31) ^ __builtin_e2k_qpsllw(X1, 30) ^ __builtin_e2k_qpsllw(X1, 25);

    X1 ^= __builtin_e2k_qpshufb(X0, X0, __builtin_e2k_qppackdl(0x0302010080808080LL, 0x8080808080808080LL));

    X0 = X1 ^ X3 ^ __builtin_e2k_qpshufb(X0, X0, __builtin_e2k_qppackdl(0x808080800f0e0d0cLL, 0x0b0a090807060504LL));

    X0 ^= __builtin_e2k_qpsrlw(X1, 7) ^ __builtin_e2k_qpsrlw(X1, 2) ^ __builtin_e2k_qpsrlw(X1, 1);

    return X0;
}

static __attribute__((__always_inline__)) inline __v2di gcm_multiply(__v2di H, __v2di x)
{
    uint64_t Hh = H[1], Hl = H[0];
    uint64_t xh = x[1], xl = x[0];

    uint64_t T0h = __builtin_e2k_clmulh(Hh, xh), T0l = __builtin_e2k_clmull(Hh, xh);
    uint64_t T1h = __builtin_e2k_clmulh(Hh, xl), T1l = __builtin_e2k_clmull(Hh, xl);
    uint64_t T2h = __builtin_e2k_clmulh(Hl, xh), T2l = __builtin_e2k_clmull(Hl, xh);
    uint64_t T3h = __builtin_e2k_clmulh(Hl, xl), T3l = __builtin_e2k_clmull(Hl, xl);

    T1h = __builtin_e2k_pxord(T1h, T2h);
    T1l = __builtin_e2k_pxord(T1l, T2l);

    T0l = __builtin_e2k_pxord(T0l, T1h);
    T3h = __builtin_e2k_pxord(T3h, T1l);

    return gcm_reduce(__builtin_e2k_qppackdl(T0h, T0l), __builtin_e2k_qppackdl(T3h, T3l));
}

static __attribute__((__always_inline__)) inline __v2di gcm_multiply_x4(__v2di H1, __v2di H2, __v2di H3, __v2di H4,
    __v2di X1, __v2di X2, __v2di X3, __v2di X4)
{
    /*
     * Multiply with delayed reduction, algorithm by Krzysztof Jankowski
     * and Pierre Laurent of Intel
     */

    const uint64_t loh = (__builtin_e2k_clmulh(H1[0], X1[0]) ^ __builtin_e2k_clmulh(H2[0], X2[0])) ^ (__builtin_e2k_clmulh(H3[0], X3[0]) ^ __builtin_e2k_clmulh(H4[0], X4[0]));
    const uint64_t lol = (__builtin_e2k_clmull(H1[0], X1[0]) ^ __builtin_e2k_clmull(H2[0], X2[0])) ^ (__builtin_e2k_clmull(H3[0], X3[0]) ^ __builtin_e2k_clmull(H4[0], X4[0]));

    const uint64_t hih = (__builtin_e2k_clmulh(H1[1], X1[1]) ^ __builtin_e2k_clmulh(H2[1], X2[1])) ^ (__builtin_e2k_clmulh(H3[1], X3[1]) ^ __builtin_e2k_clmulh(H4[1], X4[1]));
    const uint64_t hil = (__builtin_e2k_clmull(H1[1], X1[1]) ^ __builtin_e2k_clmull(H2[1], X2[1])) ^ (__builtin_e2k_clmull(H3[1], X3[1]) ^ __builtin_e2k_clmull(H4[1], X4[1]));
    uint64_t Th, Tl;

    Th = __builtin_e2k_clmulh(H1[0] ^ H1[1], X1[0] ^ X1[1]);
    Tl = __builtin_e2k_clmull(H1[0] ^ H1[1], X1[0] ^ X1[1]);

    Th ^= __builtin_e2k_clmulh(H2[0] ^ H2[1], X2[0] ^ X2[1]);
    Tl ^= __builtin_e2k_clmull(H2[0] ^ H2[1], X2[0] ^ X2[1]);

    Th ^= __builtin_e2k_clmulh(H3[0] ^ H3[1], X3[0] ^ X3[1]);
    Tl ^= __builtin_e2k_clmull(H3[0] ^ H3[1], X3[0] ^ X3[1]);

    Th ^= __builtin_e2k_clmulh(H4[0] ^ H4[1], X4[0] ^ X4[1]);
    Tl ^= __builtin_e2k_clmull(H4[0] ^ H4[1], X4[0] ^ X4[1]);

    Th ^= loh;
    Tl ^= lol;
    Th ^= hih;
    Tl ^= hil;

    return gcm_reduce(__builtin_e2k_qppackdl(hih, hil ^ Th),
        __builtin_e2k_qppackdl(loh ^ Tl, lol));
}

/*##############################################################################
# void gcm_init_e2kv6_clmul(u128 Htable[16],const uint64_t H[2]);
#
# input:        128-bit H - secret parameter E(K,0^128)
# output:       precomputed table filled with degrees of twisted H;
#               H is twisted to handle reverse bitness of GHASH;
#               only few of 16 slots of Htable[16] are used;
#               data is opaque to outside world (which allows to
#               optimize the code independently);
#
*/
void gcm_init_e2kv6_clmul(u128 Htable[16], const uint64_t H[2])
{
    __v2di *Hp = (__v2di *)Htable;
    __v2di H1 = (__v2di) { H[1], H[0] }; /* H in LE, but need swap hi/lo */
    __v2di H2 = gcm_multiply(H1, H1);
    __v2di H3 = gcm_multiply(H1, H2);
    __v2di H4 = gcm_multiply(H2, H2);

    Hp[0] = H1;
    Hp[1] = H2;
    Hp[2] = H3;
    Hp[3] = H4;
}

/*##############################################################################
# void gcm_gmult_e2kv6_clmul(uint64_t Xi[2],const u128 Htable[16]);
#
# input:        Xi - current hash value;
#               Htable - table precomputed in gcm_init_e2kv6_clmul;
# output:       Xi - next hash value Xi;
*/
void gcm_gmult_e2kv6_clmul(uint64_t Xi[2], const u128 Htable[16])
{
    __v2di *Xp = (__v2di *)Xi;
    __v2di *Hp = (__v2di *)Htable;
    *Xp = reverse_vector(gcm_multiply(Hp[0], reverse_vector(*Xp)));
}

/*##############################################################################
# void gcm_ghash_e2kv6_clmul(uint64_t Xi[2], const u128 Htable[16],
#                            const uint8_t *inp,size_t len);
#
# input:        table precomputed in gcm_init_e2kv6_clmul;
#               current hash value Xi;
#               pointer to input data;
#               length of input data in bytes, but divisible by block size;
# output:       next hash value Xi;
*/
void gcm_ghash_e2kv6_clmul(uint64_t Xi[2], const u128 Htable[16],
    const uint8_t *inp, size_t len)
{
    __v2di *Hp = (__v2di *)Htable;
    __v2di *input = (__v2di *)inp;
    __v2di x;
    size_t i, blocks = (len >> 4);

    x = reverse_vector(*(__v2di *)Xi);

    while (blocks >= 4) {
        __v2di m0 = reverse_vector(input[0]);
        __v2di m1 = reverse_vector(input[1]);
        __v2di m2 = reverse_vector(input[2]);
        __v2di m3 = reverse_vector(input[3]);

        x ^= m0;
        x = gcm_multiply_x4(Hp[0], Hp[1], Hp[2], Hp[3], m3, m2, m1, x);

        input += 4;
        blocks -= 4;
    }

#pragma loop count(3)
    for (i = 0; i < blocks; i++) {
        __v2di m = reverse_vector(input[i]);

        x ^= m;
        x = gcm_multiply(Hp[0], x);
    }

    *(__v2di *)Xi = reverse_vector(x);
}

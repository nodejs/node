/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <e2kintrin.h>

#include "../bn_local.h"

void bn_GF2m_mul_2x2(BN_ULONG *r, const BN_ULONG a1, const BN_ULONG a0,
    const BN_ULONG b1, const BN_ULONG b0)
{
    BN_ULONG m1, m0;

    r[3] = __builtin_e2k_clmulh(a1, b1);
    r[2] = __builtin_e2k_clmull(a1, b1);

    r[1] = __builtin_e2k_clmulh(a0, b0);
    r[0] = __builtin_e2k_clmull(a0, b0);

    m1 = __builtin_e2k_clmulh(a0 ^ a1, b0 ^ b1);
    m0 = __builtin_e2k_clmull(a0 ^ a1, b0 ^ b1);

    r[2] ^= __builtin_e2k_plog(0x96, m1, r[1], r[3]);
    r[1] = __builtin_e2k_plog(0x96, r[3], r[2], __builtin_e2k_plog(0x96, r[0], m1, m0));
}

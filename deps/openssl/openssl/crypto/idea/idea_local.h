/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_LIBCRYPTO_IDEA_IDEA_LOCAL_H)
#define OSSL_LIBCRYPTO_IDEA_IDEA_LOCAL_H

#include "internal/common.h"

#define idea_mul(r, a, b, ul)                                          \
    ul = (unsigned long)a * b;                                         \
    if (ul != 0) {                                                     \
        r = (ul & 0xffff) - (ul >> 16);                                \
        r -= ((r) >> 16);                                              \
    } else {                                                           \
        r = (-(int)a - b + 1); /* assuming a or b is 0 and in range */ \
    }

#define E_IDEA(num)                       \
    x1 &= 0xffff;                         \
    idea_mul(x1, x1, *p, ul);             \
    p++;                                  \
    x2 += *(p++);                         \
    x3 += *(p++);                         \
    x4 &= 0xffff;                         \
    idea_mul(x4, x4, *p, ul);             \
    p++;                                  \
    t0 = (x1 ^ x3) & 0xffff;              \
    idea_mul(t0, t0, *p, ul);             \
    p++;                                  \
    t1 = (t0 + (x2 ^ x4)) & 0xffff;       \
    idea_mul(t1, t1, *p, ul);             \
    p++;                                  \
    t0 += t1;                             \
    x1 ^= t1;                             \
    x4 ^= t0;                             \
    ul = x2 ^ t0; /* do the swap to x3 */ \
    x2 = x3 ^ t1;                         \
    x3 = ul;

#endif /* !defined(OSSL_LIBCRYPTO_IDEA_IDEA_LOCAL_H) */

/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_LIBCRYPTO_RC5_RC5_LOCAL_H)
#define OSSL_LIBCRYPTO_RC5_RC5_LOCAL_H

#include <stdlib.h>
#include "internal/common.h"

#if (defined(OPENSSL_SYS_WIN32) && defined(_MSC_VER))
#define ROTATE_l32(a, n) _lrotl(a, n)
#define ROTATE_r32(a, n) _lrotr(a, n)
#elif defined(__ICC)
#define ROTATE_l32(a, n) _rotl(a, n)
#define ROTATE_r32(a, n) _rotr(a, n)
#elif defined(__GNUC__) && !defined(__STRICT_ANSI__) && !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_NO_INLINE_ASM) && !defined(PEDANTIC)
#if defined(__i386) || defined(__i386__) || defined(__x86_64) || defined(__x86_64__)
#define ROTATE_l32(a, n) ({              \
    register unsigned int ret;           \
    asm("roll %%cl,%0"                   \
        : "=r"(ret)                      \
        : "c"(n), "0"((unsigned int)(a)) \
        : "cc");                         \
    ret;                                 \
})
#define ROTATE_r32(a, n) ({              \
    register unsigned int ret;           \
    asm("rorl %%cl,%0"                   \
        : "=r"(ret)                      \
        : "c"(n), "0"((unsigned int)(a)) \
        : "cc");                         \
    ret;                                 \
})
#endif
#endif
#ifndef ROTATE_l32
#define ROTATE_l32(a, n) (((a) << (n & 0x1f)) | (((a) & 0xffffffff) >> ((32 - n) & 0x1f)))
#endif
#ifndef ROTATE_r32
#define ROTATE_r32(a, n) (((a) << ((32 - n) & 0x1f)) | (((a) & 0xffffffff) >> (n & 0x1f)))
#endif

#define RC5_32_MASK 0xffffffffL

#define RC5_32_P 0xB7E15163L
#define RC5_32_Q 0x9E3779B9L

#define E_RC5_32(a, b, s, n) \
    a ^= b;                  \
    a = ROTATE_l32(a, b);    \
    a += s[n];               \
    a &= RC5_32_MASK;        \
    b ^= a;                  \
    b = ROTATE_l32(b, a);    \
    b += s[n + 1];           \
    b &= RC5_32_MASK;

#define D_RC5_32(a, b, s, n) \
    b -= s[n + 1];           \
    b &= RC5_32_MASK;        \
    b = ROTATE_r32(b, a);    \
    b ^= a;                  \
    a -= s[n];               \
    a &= RC5_32_MASK;        \
    a = ROTATE_r32(a, b);    \
    a ^= b;

#endif /* !defined(OSSL_LIBCRYPTO_RC5_RC5_LOCAL_H) */

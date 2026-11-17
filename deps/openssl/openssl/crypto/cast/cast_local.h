/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#if !defined(OSSL_LIBCRYPTO_CAST_CAST_LOCAL_H)
#define OSSL_LIBCRYPTO_CAST_CAST_LOCAL_H

#include <openssl/cast.h>

#include "internal/common.h"

#ifdef OPENSSL_SYS_WIN32
#include <stdlib.h>
#endif

#if defined(OPENSSL_SYS_WIN32) && defined(_MSC_VER)
#define ROTL(a, n) (_lrotl(a, n))
#else
#define ROTL(a, n) ((((a) << (n)) & 0xffffffffL) | ((a) >> ((32 - (n)) & 31)))
#endif

#define C_M 0x3fc
#define C_0 22L
#define C_1 14L
#define C_2 6L
#define C_3 2L /* left shift */

/* The rotate has an extra 16 added to it to help the x86 asm */
#if defined(CAST_PTR)
#define E_CAST(n, key, L, R, OP1, OP2, OP3)            \
    {                                                  \
        int i;                                         \
        t = (key[n * 2] OP1 R) & 0xffffffffL;          \
        i = key[n * 2 + 1];                            \
        t = ROTL(t, i);                                \
        L ^= (((((*(CAST_LONG *)((unsigned char *)     \
                                     CAST_S_table0     \
                      + ((t >> C_2) & C_M)) OP2        \
                     * (CAST_LONG *)((unsigned char *) \
                                         CAST_S_table1 \
                         + ((t << C_3) & C_M)))        \
                    & 0xffffffffL) OP3                 \
                   * (CAST_LONG *)((unsigned char *)   \
                                       CAST_S_table2   \
                       + ((t >> C_0) & C_M)))          \
                  & 0xffffffffL) OP1                   \
                 * (CAST_LONG *)((unsigned char *)     \
                                     CAST_S_table3     \
                     + ((t >> C_1) & C_M)))            \
            & 0xffffffffL;                             \
    }
#elif defined(CAST_PTR2)
#define E_CAST(n, key, L, R, OP1, OP2, OP3)                                            \
    {                                                                                  \
        int i;                                                                         \
        CAST_LONG u, v, w;                                                             \
        w = (key[n * 2] OP1 R) & 0xffffffffL;                                          \
        i = key[n * 2 + 1];                                                            \
        w = ROTL(w, i);                                                                \
        u = w >> C_2;                                                                  \
        v = w << C_3;                                                                  \
        u &= C_M;                                                                      \
        v &= C_M;                                                                      \
        t = *(CAST_LONG *)((unsigned char *)CAST_S_table0 + u);                        \
        u = w >> C_0;                                                                  \
        t = (t OP2 * (CAST_LONG *)((unsigned char *)CAST_S_table1 + v)) & 0xffffffffL; \
        v = w >> C_1;                                                                  \
        u &= C_M;                                                                      \
        v &= C_M;                                                                      \
        t = (t OP3 * (CAST_LONG *)((unsigned char *)CAST_S_table2 + u) & 0xffffffffL); \
        t = (t OP1 * (CAST_LONG *)((unsigned char *)CAST_S_table3 + v) & 0xffffffffL); \
        L ^= (t & 0xffffffff);                                                         \
    }
#else
#define E_CAST(n, key, L, R, OP1, OP2, OP3)                                           \
    {                                                                                 \
        CAST_LONG a, b, c, d;                                                         \
        t = (key[n * 2] OP1 R) & 0xffffffff;                                          \
        t = ROTL(t, (key[n * 2 + 1]));                                                \
        a = CAST_S_table0[(t >> 8) & 0xff];                                           \
        b = CAST_S_table1[(t) & 0xff];                                                \
        c = CAST_S_table2[(t >> 24) & 0xff];                                          \
        d = CAST_S_table3[(t >> 16) & 0xff];                                          \
        L ^= (((((a OP2 b) & 0xffffffffL) OP3 c) & 0xffffffffL) OP1 d) & 0xffffffffL; \
    }
#endif

extern const CAST_LONG CAST_S_table0[256];
extern const CAST_LONG CAST_S_table1[256];
extern const CAST_LONG CAST_S_table2[256];
extern const CAST_LONG CAST_S_table3[256];
extern const CAST_LONG CAST_S_table4[256];
extern const CAST_LONG CAST_S_table5[256];
extern const CAST_LONG CAST_S_table6[256];
extern const CAST_LONG CAST_S_table7[256];

#endif /* !defined(OSSL_LIBCRYPTO_CAST_CAST_LOCAL_H) */

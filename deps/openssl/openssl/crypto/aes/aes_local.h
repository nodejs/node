/*
 * Copyright 2002-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_CRYPTO_AES_LOCAL_H
#define OSSL_CRYPTO_AES_LOCAL_H

#include <openssl/e_os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_X64))
#define SWAP(x) (_lrotl(x, 8) & 0x00ff00ff | _lrotr(x, 8) & 0xff00ff00)
#define GETU32(p) SWAP(*((uint32_t *)(p)))
#define PUTU32(ct, st)                    \
    {                                     \
        *((uint32_t *)(ct)) = SWAP((st)); \
    }
#else
#define GETU32(pt) (((uint32_t)(pt)[0] << 24) ^ ((uint32_t)(pt)[1] << 16) ^ ((uint32_t)(pt)[2] << 8) ^ ((uint32_t)(pt)[3]))
#define PUTU32(ct, st)                   \
    {                                    \
        (ct)[0] = (uint8_t)((st) >> 24); \
        (ct)[1] = (uint8_t)((st) >> 16); \
        (ct)[2] = (uint8_t)((st) >> 8);  \
        (ct)[3] = (uint8_t)(st);         \
    }
#endif

#define MAXKC (256 / 32)
#define MAXKB (256 / 8)
#define MAXNR 14

/* This controls loop-unrolling in aes_core.c */
#undef FULL_UNROLL

#endif /* !OSSL_CRYPTO_AES_LOCAL_H */

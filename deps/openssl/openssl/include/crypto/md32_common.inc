/*
 * Copyright 1999-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * This is a generic 32 bit "collector" for message digest algorithms.
 * Whenever needed it collects input character stream into chunks of
 * 32 bit values and invokes a block function that performs actual hash
 * calculations.
 *
 * Porting guide.
 *
 * Obligatory macros:
 *
 * DATA_ORDER_IS_BIG_ENDIAN or DATA_ORDER_IS_LITTLE_ENDIAN
 *      this macro defines byte order of input stream.
 * HASH_CBLOCK
 *      size of a unit chunk HASH_BLOCK operates on.
 * HASH_LONG
 *      has to be at least 32 bit wide.
 * HASH_CTX
 *      context structure that at least contains following
 *      members:
 *              typedef struct {
 *                      ...
 *                      HASH_LONG       Nl,Nh;
 *                      either {
 *                      HASH_LONG       data[HASH_LBLOCK];
 *                      unsigned char   data[HASH_CBLOCK];
 *                      };
 *                      unsigned int    num;
 *                      ...
 *                      } HASH_CTX;
 *      data[] vector is expected to be zeroed upon first call to
 *      HASH_UPDATE.
 * HASH_UPDATE
 *      name of "Update" function, implemented here.
 * HASH_TRANSFORM
 *      name of "Transform" function, implemented here.
 * HASH_FINAL
 *      name of "Final" function, implemented here.
 * HASH_BLOCK_DATA_ORDER
 *      name of "block" function capable of treating *unaligned* input
 *      message in original (data) byte order, implemented externally.
 * HASH_MAKE_STRING
 *      macro converting context variables to an ASCII hash string.
 *
 * MD5 example:
 *
 *      #define DATA_ORDER_IS_LITTLE_ENDIAN
 *
 *      #define HASH_LONG               MD5_LONG
 *      #define HASH_CTX                MD5_CTX
 *      #define HASH_CBLOCK             MD5_CBLOCK
 *      #define HASH_UPDATE             MD5_Update
 *      #define HASH_TRANSFORM          MD5_Transform
 *      #define HASH_FINAL              MD5_Final
 *      #define HASH_BLOCK_DATA_ORDER   md5_block_data_order
 */

#ifndef OSSL_CRYPTO_MD32_COMMON_H
#define OSSL_CRYPTO_MD32_COMMON_H
#pragma once

#include <openssl/crypto.h>
/*
 * For ossl_(un)likely
 */
#include <internal/common.h>

#if !defined(DATA_ORDER_IS_BIG_ENDIAN) && !defined(DATA_ORDER_IS_LITTLE_ENDIAN)
#error "DATA_ORDER must be defined!"
#endif

#ifndef HASH_CBLOCK
#error "HASH_CBLOCK must be defined!"
#endif
#ifndef HASH_LONG
#error "HASH_LONG must be defined!"
#endif
#ifndef HASH_CTX
#error "HASH_CTX must be defined!"
#endif

#ifndef HASH_UPDATE
#error "HASH_UPDATE must be defined!"
#endif
#ifndef HASH_TRANSFORM
#error "HASH_TRANSFORM must be defined!"
#endif
#ifndef HASH_FINAL
#error "HASH_FINAL must be defined!"
#endif

#ifndef HASH_BLOCK_DATA_ORDER
#error "HASH_BLOCK_DATA_ORDER must be defined!"
#endif

#define ROTATE(a, n) (((a) << (n)) | (((a) & 0xffffffff) >> (32 - (n))))

#ifndef PEDANTIC
#if defined(__GNUC__) && !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_NO_INLINE_ASM)
#if defined(__riscv_zbb) || defined(__riscv_zbkb)
#if __riscv_xlen == 64
#undef ROTATE
#define ROTATE(x, n) ({ MD32_REG_T ret;            \
                       asm ("roriw %0, %1, %2"        \
                       : "=r"(ret)                    \
                       : "r"(x), "i"(32 - (n))); ret; })
#endif
#if __riscv_xlen == 32
#undef ROTATE
#define ROTATE(x, n) ({ MD32_REG_T ret;            \
                       asm ("rori %0, %1, %2"         \
                       : "=r"(ret)                    \
                       : "r"(x), "i"(32 - (n))); ret; })
#endif
#  elif defined(__e2k__)
#   undef ROTATE
#   define ROTATE(a,n)  ( (__builtin_constant_p(n) && (n) > 16) \
                          ? __builtin_e2k_scrs((a), 32 - (n))   \
                          : __builtin_e2k_scls((a),      (n))   )
#endif
#endif
#endif

#if defined(DATA_ORDER_IS_BIG_ENDIAN)

#define HOST_c2l(c, l) (l = (((unsigned long)(*((c)++))) << 24), \
    l |= (((unsigned long)(*((c)++))) << 16),                    \
    l |= (((unsigned long)(*((c)++))) << 8),                     \
    l |= (((unsigned long)(*((c)++)))))
#define HOST_l2c(l, c) (*((c)++) = (unsigned char)(((l) >> 24) & 0xff), \
    *((c)++) = (unsigned char)(((l) >> 16) & 0xff),                     \
    *((c)++) = (unsigned char)(((l) >> 8) & 0xff),                      \
    *((c)++) = (unsigned char)(((l)) & 0xff),                           \
    l)

#elif defined(DATA_ORDER_IS_LITTLE_ENDIAN)

#define HOST_c2l(c, l) (l = (((unsigned long)(*((c)++)))), \
    l |= (((unsigned long)(*((c)++))) << 8),               \
    l |= (((unsigned long)(*((c)++))) << 16),              \
    l |= (((unsigned long)(*((c)++))) << 24))
#define HOST_l2c(l, c) (*((c)++) = (unsigned char)(((l)) & 0xff), \
    *((c)++) = (unsigned char)(((l) >> 8) & 0xff),                \
    *((c)++) = (unsigned char)(((l) >> 16) & 0xff),               \
    *((c)++) = (unsigned char)(((l) >> 24) & 0xff),               \
    l)

#endif

/*
 * Time for some action :-)
 */

#ifdef HASH_UPDATE_THUNK
int HASH_UPDATE(void *cp, const unsigned char *data_, size_t len);
int HASH_UPDATE(void *cp, const unsigned char *data_, size_t len)
#else
int HASH_UPDATE(HASH_CTX *c, const void *data_, size_t len)
#endif
{
#ifdef HASH_UPDATE_THUNK
    HASH_CTX *c = (HASH_CTX *)cp;
#endif
    const unsigned char *data = data_;
    unsigned char *p;
    HASH_LONG l;
    size_t n;

    if (ossl_unlikely(len == 0))
        return 1;

    l = (c->Nl + (((HASH_LONG)len) << 3)) & 0xffffffffUL;
    if (ossl_unlikely(l < c->Nl)) /* overflow */
        c->Nh++;
    c->Nh += (HASH_LONG)(len >> 29); /* might cause compiler warning on
                                      * 16-bit */
    c->Nl = l;

    n = c->num;
    if (ossl_likely(n != 0)) {
        /* Gets here if we already have buffered input data */
        p = (unsigned char *)c->data;

        if (len >= HASH_CBLOCK || len + n >= HASH_CBLOCK) {
            /*
             * If there is enough input to fill the buffer then fill the
             * buffer and process a single chunk.
             */
            memcpy(p + n, data, HASH_CBLOCK - n);
            HASH_BLOCK_DATA_ORDER(c, p, 1);
            n = HASH_CBLOCK - n;
            data += n;
            len -= n;
            c->num = 0;
            /*
             * We use memset rather than OPENSSL_cleanse() here deliberately.
             * Using OPENSSL_cleanse() here could be a performance issue. It
             * will get properly cleansed on finalisation so this isn't a
             * security problem.
             */
            memset(p, 0, HASH_CBLOCK); /* keep it zeroed */
        } else {
            /* Otherwise just keep filling the buffer */
            memcpy(p + n, data, len);
            c->num += (unsigned int)len;
            return 1;
        }
    }

    n = len / HASH_CBLOCK; /* Get number of input chunks (e.g. multiple of 512 bits for SHA256) */
    if (n > 0) {
        /* Process chunks */
        HASH_BLOCK_DATA_ORDER(c, data, n);
        n *= HASH_CBLOCK;
        data += n;
        len -= n;
    }
    /* Buffer any left over data */
    if (len != 0) {
        p = (unsigned char *)c->data;
        c->num = (unsigned int)len;
        memcpy(p, data, len);
    }
    return 1;
}

void HASH_TRANSFORM(HASH_CTX *c, const unsigned char *data)
{
    HASH_BLOCK_DATA_ORDER(c, data, 1); /* Process a single chunk */
}

int HASH_FINAL(unsigned char *md, HASH_CTX *c)
{
    unsigned char *p = (unsigned char *)c->data;
    size_t n = c->num;

    /*
     * Pad the input by adding a 1 bit + K zero bits + input length (L)
     * as a 64 bit value. K must align the data to a chunk boundary.
     */
    p[n] = 0x80; /* there is always room for one */
    n++;

    if (n > (HASH_CBLOCK - 8)) {
        /*
         * If there is not enough room in the buffer to add L, then fill the
         * current buffer with zeros, and process the chunk
         */
        memset(p + n, 0, HASH_CBLOCK - n);
        n = 0;
        HASH_BLOCK_DATA_ORDER(c, p, 1);
    }
    /* Add zero padding - but leave enough room for L */
    memset(p + n, 0, HASH_CBLOCK - 8 - n);

    /* Add the 64 bit L value to the end of the buffer */
    p += HASH_CBLOCK - 8;
#if defined(DATA_ORDER_IS_BIG_ENDIAN)
    (void)HOST_l2c(c->Nh, p);
    (void)HOST_l2c(c->Nl, p);
#elif defined(DATA_ORDER_IS_LITTLE_ENDIAN)
    (void)HOST_l2c(c->Nl, p);
    (void)HOST_l2c(c->Nh, p);
#endif
    p -= HASH_CBLOCK;
    /* Process the final padded chunk */
    HASH_BLOCK_DATA_ORDER(c, p, 1);
    c->num = 0;
    OPENSSL_cleanse(p, HASH_CBLOCK);

#ifndef HASH_MAKE_STRING
#error "HASH_MAKE_STRING must be defined!"
#else
    HASH_MAKE_STRING(c, md);
#endif

    return 1;
}

#ifndef MD32_REG_T
#if defined(__alpha) || defined(__sparcv9) || defined(__mips)
#define MD32_REG_T long
/*
 * This comment was originally written for MD5, which is why it
 * discusses A-D. But it basically applies to all 32-bit digests,
 * which is why it was moved to common header file.
 *
 * In case you wonder why A-D are declared as long and not
 * as MD5_LONG. Doing so results in slight performance
 * boost on LP64 architectures. The catch is we don't
 * really care if 32 MSBs of a 64-bit register get polluted
 * with eventual overflows as we *save* only 32 LSBs in
 * *either* case. Now declaring 'em long excuses the compiler
 * from keeping 32 MSBs zeroed resulting in 13% performance
 * improvement under SPARC Solaris7/64 and 5% under AlphaLinux.
 * Well, to be honest it should say that this *prevents*
 * performance degradation.
 */
#else
/*
 * Above is not absolute and there are LP64 compilers that
 * generate better code if MD32_REG_T is defined int. The above
 * pre-processor condition reflects the circumstances under which
 * the conclusion was made and is subject to further extension.
 */
#define MD32_REG_T int
#endif
#endif

#endif

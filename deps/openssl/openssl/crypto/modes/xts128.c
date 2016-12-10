/* ====================================================================
 * Copyright (c) 2011 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

#include <openssl/crypto.h>
#include "modes_lcl.h"
#include <string.h>

#ifndef MODES_DEBUG
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif
#include <assert.h>

int CRYPTO_xts128_encrypt(const XTS128_CONTEXT *ctx,
                          const unsigned char iv[16],
                          const unsigned char *inp, unsigned char *out,
                          size_t len, int enc)
{
    const union {
        long one;
        char little;
    } is_endian = {
        1
    };
    union {
        u64 u[2];
        u32 d[4];
        u8 c[16];
    } tweak, scratch;
    unsigned int i;

    if (len < 16)
        return -1;

    memcpy(tweak.c, iv, 16);

    (*ctx->block2) (tweak.c, tweak.c, ctx->key2);

    if (!enc && (len % 16))
        len -= 16;

    while (len >= 16) {
#if defined(STRICT_ALIGNMENT)
        memcpy(scratch.c, inp, 16);
        scratch.u[0] ^= tweak.u[0];
        scratch.u[1] ^= tweak.u[1];
#else
        scratch.u[0] = ((u64 *)inp)[0] ^ tweak.u[0];
        scratch.u[1] = ((u64 *)inp)[1] ^ tweak.u[1];
#endif
        (*ctx->block1) (scratch.c, scratch.c, ctx->key1);
#if defined(STRICT_ALIGNMENT)
        scratch.u[0] ^= tweak.u[0];
        scratch.u[1] ^= tweak.u[1];
        memcpy(out, scratch.c, 16);
#else
        ((u64 *)out)[0] = scratch.u[0] ^= tweak.u[0];
        ((u64 *)out)[1] = scratch.u[1] ^= tweak.u[1];
#endif
        inp += 16;
        out += 16;
        len -= 16;

        if (len == 0)
            return 0;

        if (is_endian.little) {
            unsigned int carry, res;

            res = 0x87 & (((int)tweak.d[3]) >> 31);
            carry = (unsigned int)(tweak.u[0] >> 63);
            tweak.u[0] = (tweak.u[0] << 1) ^ res;
            tweak.u[1] = (tweak.u[1] << 1) | carry;
        } else {
            size_t c;

            for (c = 0, i = 0; i < 16; ++i) {
                /*
                 * + substitutes for |, because c is 1 bit
                 */
                c += ((size_t)tweak.c[i]) << 1;
                tweak.c[i] = (u8)c;
                c = c >> 8;
            }
            tweak.c[0] ^= (u8)(0x87 & (0 - c));
        }
    }
    if (enc) {
        for (i = 0; i < len; ++i) {
            u8 c = inp[i];
            out[i] = scratch.c[i];
            scratch.c[i] = c;
        }
        scratch.u[0] ^= tweak.u[0];
        scratch.u[1] ^= tweak.u[1];
        (*ctx->block1) (scratch.c, scratch.c, ctx->key1);
        scratch.u[0] ^= tweak.u[0];
        scratch.u[1] ^= tweak.u[1];
        memcpy(out - 16, scratch.c, 16);
    } else {
        union {
            u64 u[2];
            u8 c[16];
        } tweak1;

        if (is_endian.little) {
            unsigned int carry, res;

            res = 0x87 & (((int)tweak.d[3]) >> 31);
            carry = (unsigned int)(tweak.u[0] >> 63);
            tweak1.u[0] = (tweak.u[0] << 1) ^ res;
            tweak1.u[1] = (tweak.u[1] << 1) | carry;
        } else {
            size_t c;

            for (c = 0, i = 0; i < 16; ++i) {
                /*
                 * + substitutes for |, because c is 1 bit
                 */
                c += ((size_t)tweak.c[i]) << 1;
                tweak1.c[i] = (u8)c;
                c = c >> 8;
            }
            tweak1.c[0] ^= (u8)(0x87 & (0 - c));
        }
#if defined(STRICT_ALIGNMENT)
        memcpy(scratch.c, inp, 16);
        scratch.u[0] ^= tweak1.u[0];
        scratch.u[1] ^= tweak1.u[1];
#else
        scratch.u[0] = ((u64 *)inp)[0] ^ tweak1.u[0];
        scratch.u[1] = ((u64 *)inp)[1] ^ tweak1.u[1];
#endif
        (*ctx->block1) (scratch.c, scratch.c, ctx->key1);
        scratch.u[0] ^= tweak1.u[0];
        scratch.u[1] ^= tweak1.u[1];

        for (i = 0; i < len; ++i) {
            u8 c = inp[16 + i];
            out[16 + i] = scratch.c[i];
            scratch.c[i] = c;
        }
        scratch.u[0] ^= tweak.u[0];
        scratch.u[1] ^= tweak.u[1];
        (*ctx->block1) (scratch.c, scratch.c, ctx->key1);
#if defined(STRICT_ALIGNMENT)
        scratch.u[0] ^= tweak.u[0];
        scratch.u[1] ^= tweak.u[1];
        memcpy(out, scratch.c, 16);
#else
        ((u64 *)out)[0] = scratch.u[0] ^ tweak.u[0];
        ((u64 *)out)[1] = scratch.u[1] ^ tweak.u[1];
#endif
    }

    return 0;
}

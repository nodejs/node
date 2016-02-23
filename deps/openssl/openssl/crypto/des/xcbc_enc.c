/* crypto/des/xcbc_enc.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "des_locl.h"

/* RSA's DESX */

#if 0                           /* broken code, preserved just in case anyone
                                 * specifically looks for this */
static const unsigned char desx_white_in2out[256] = {
    0xBD, 0x56, 0xEA, 0xF2, 0xA2, 0xF1, 0xAC, 0x2A, 0xB0, 0x93, 0xD1, 0x9C,
    0x1B, 0x33, 0xFD, 0xD0,
    0x30, 0x04, 0xB6, 0xDC, 0x7D, 0xDF, 0x32, 0x4B, 0xF7, 0xCB, 0x45, 0x9B,
    0x31, 0xBB, 0x21, 0x5A,
    0x41, 0x9F, 0xE1, 0xD9, 0x4A, 0x4D, 0x9E, 0xDA, 0xA0, 0x68, 0x2C, 0xC3,
    0x27, 0x5F, 0x80, 0x36,
    0x3E, 0xEE, 0xFB, 0x95, 0x1A, 0xFE, 0xCE, 0xA8, 0x34, 0xA9, 0x13, 0xF0,
    0xA6, 0x3F, 0xD8, 0x0C,
    0x78, 0x24, 0xAF, 0x23, 0x52, 0xC1, 0x67, 0x17, 0xF5, 0x66, 0x90, 0xE7,
    0xE8, 0x07, 0xB8, 0x60,
    0x48, 0xE6, 0x1E, 0x53, 0xF3, 0x92, 0xA4, 0x72, 0x8C, 0x08, 0x15, 0x6E,
    0x86, 0x00, 0x84, 0xFA,
    0xF4, 0x7F, 0x8A, 0x42, 0x19, 0xF6, 0xDB, 0xCD, 0x14, 0x8D, 0x50, 0x12,
    0xBA, 0x3C, 0x06, 0x4E,
    0xEC, 0xB3, 0x35, 0x11, 0xA1, 0x88, 0x8E, 0x2B, 0x94, 0x99, 0xB7, 0x71,
    0x74, 0xD3, 0xE4, 0xBF,
    0x3A, 0xDE, 0x96, 0x0E, 0xBC, 0x0A, 0xED, 0x77, 0xFC, 0x37, 0x6B, 0x03,
    0x79, 0x89, 0x62, 0xC6,
    0xD7, 0xC0, 0xD2, 0x7C, 0x6A, 0x8B, 0x22, 0xA3, 0x5B, 0x05, 0x5D, 0x02,
    0x75, 0xD5, 0x61, 0xE3,
    0x18, 0x8F, 0x55, 0x51, 0xAD, 0x1F, 0x0B, 0x5E, 0x85, 0xE5, 0xC2, 0x57,
    0x63, 0xCA, 0x3D, 0x6C,
    0xB4, 0xC5, 0xCC, 0x70, 0xB2, 0x91, 0x59, 0x0D, 0x47, 0x20, 0xC8, 0x4F,
    0x58, 0xE0, 0x01, 0xE2,
    0x16, 0x38, 0xC4, 0x6F, 0x3B, 0x0F, 0x65, 0x46, 0xBE, 0x7E, 0x2D, 0x7B,
    0x82, 0xF9, 0x40, 0xB5,
    0x1D, 0x73, 0xF8, 0xEB, 0x26, 0xC7, 0x87, 0x97, 0x25, 0x54, 0xB1, 0x28,
    0xAA, 0x98, 0x9D, 0xA5,
    0x64, 0x6D, 0x7A, 0xD4, 0x10, 0x81, 0x44, 0xEF, 0x49, 0xD6, 0xAE, 0x2E,
    0xDD, 0x76, 0x5C, 0x2F,
    0xA7, 0x1C, 0xC9, 0x09, 0x69, 0x9A, 0x83, 0xCF, 0x29, 0x39, 0xB9, 0xE9,
    0x4C, 0xFF, 0x43, 0xAB,
};

void DES_xwhite_in2out(const_DES_cblock *des_key, const_DES_cblock *in_white,
                       DES_cblock *out_white)
{
    int out0, out1;
    int i;
    const unsigned char *key = &(*des_key)[0];
    const unsigned char *in = &(*in_white)[0];
    unsigned char *out = &(*out_white)[0];

    out[0] = out[1] = out[2] = out[3] = out[4] = out[5] = out[6] = out[7] = 0;
    out0 = out1 = 0;
    for (i = 0; i < 8; i++) {
        out[i] = key[i] ^ desx_white_in2out[out0 ^ out1];
        out0 = out1;
        out1 = (int)out[i & 0x07];
    }

    out0 = out[0];
    out1 = out[i];              /* BUG: out-of-bounds read */
    for (i = 0; i < 8; i++) {
        out[i] = in[i] ^ desx_white_in2out[out0 ^ out1];
        out0 = out1;
        out1 = (int)out[i & 0x07];
    }
}
#endif

void DES_xcbc_encrypt(const unsigned char *in, unsigned char *out,
                      long length, DES_key_schedule *schedule,
                      DES_cblock *ivec, const_DES_cblock *inw,
                      const_DES_cblock *outw, int enc)
{
    register DES_LONG tin0, tin1;
    register DES_LONG tout0, tout1, xor0, xor1;
    register DES_LONG inW0, inW1, outW0, outW1;
    register const unsigned char *in2;
    register long l = length;
    DES_LONG tin[2];
    unsigned char *iv;

    in2 = &(*inw)[0];
    c2l(in2, inW0);
    c2l(in2, inW1);
    in2 = &(*outw)[0];
    c2l(in2, outW0);
    c2l(in2, outW1);

    iv = &(*ivec)[0];

    if (enc) {
        c2l(iv, tout0);
        c2l(iv, tout1);
        for (l -= 8; l >= 0; l -= 8) {
            c2l(in, tin0);
            c2l(in, tin1);
            tin0 ^= tout0 ^ inW0;
            tin[0] = tin0;
            tin1 ^= tout1 ^ inW1;
            tin[1] = tin1;
            DES_encrypt1(tin, schedule, DES_ENCRYPT);
            tout0 = tin[0] ^ outW0;
            l2c(tout0, out);
            tout1 = tin[1] ^ outW1;
            l2c(tout1, out);
        }
        if (l != -8) {
            c2ln(in, tin0, tin1, l + 8);
            tin0 ^= tout0 ^ inW0;
            tin[0] = tin0;
            tin1 ^= tout1 ^ inW1;
            tin[1] = tin1;
            DES_encrypt1(tin, schedule, DES_ENCRYPT);
            tout0 = tin[0] ^ outW0;
            l2c(tout0, out);
            tout1 = tin[1] ^ outW1;
            l2c(tout1, out);
        }
        iv = &(*ivec)[0];
        l2c(tout0, iv);
        l2c(tout1, iv);
    } else {
        c2l(iv, xor0);
        c2l(iv, xor1);
        for (l -= 8; l > 0; l -= 8) {
            c2l(in, tin0);
            tin[0] = tin0 ^ outW0;
            c2l(in, tin1);
            tin[1] = tin1 ^ outW1;
            DES_encrypt1(tin, schedule, DES_DECRYPT);
            tout0 = tin[0] ^ xor0 ^ inW0;
            tout1 = tin[1] ^ xor1 ^ inW1;
            l2c(tout0, out);
            l2c(tout1, out);
            xor0 = tin0;
            xor1 = tin1;
        }
        if (l != -8) {
            c2l(in, tin0);
            tin[0] = tin0 ^ outW0;
            c2l(in, tin1);
            tin[1] = tin1 ^ outW1;
            DES_encrypt1(tin, schedule, DES_DECRYPT);
            tout0 = tin[0] ^ xor0 ^ inW0;
            tout1 = tin[1] ^ xor1 ^ inW1;
            l2cn(tout0, tout1, out, l + 8);
            xor0 = tin0;
            xor1 = tin1;
        }

        iv = &(*ivec)[0];
        l2c(xor0, iv);
        l2c(xor1, iv);
    }
    tin0 = tin1 = tout0 = tout1 = xor0 = xor1 = 0;
    inW0 = inW1 = outW0 = outW1 = 0;
    tin[0] = tin[1] = 0;
}

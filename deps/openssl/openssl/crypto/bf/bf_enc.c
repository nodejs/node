/* crypto/bf/bf_enc.c */
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

#include <openssl/blowfish.h>
#include "bf_locl.h"

/*
 * Blowfish as implemented from 'Blowfish: Springer-Verlag paper' (From
 * LECTURE NOTES IN COMPUTER SCIENCE 809, FAST SOFTWARE ENCRYPTION, CAMBRIDGE
 * SECURITY WORKSHOP, CAMBRIDGE, U.K., DECEMBER 9-11, 1993)
 */

#if (BF_ROUNDS != 16) && (BF_ROUNDS != 20)
# error If you set BF_ROUNDS to some value other than 16 or 20, you will have \
to modify the code.
#endif

void BF_encrypt(BF_LONG *data, const BF_KEY *key)
{
#ifndef BF_PTR2
    register BF_LONG l, r;
    register const BF_LONG *p, *s;

    p = key->P;
    s = &(key->S[0]);
    l = data[0];
    r = data[1];

    l ^= p[0];
    BF_ENC(r, l, s, p[1]);
    BF_ENC(l, r, s, p[2]);
    BF_ENC(r, l, s, p[3]);
    BF_ENC(l, r, s, p[4]);
    BF_ENC(r, l, s, p[5]);
    BF_ENC(l, r, s, p[6]);
    BF_ENC(r, l, s, p[7]);
    BF_ENC(l, r, s, p[8]);
    BF_ENC(r, l, s, p[9]);
    BF_ENC(l, r, s, p[10]);
    BF_ENC(r, l, s, p[11]);
    BF_ENC(l, r, s, p[12]);
    BF_ENC(r, l, s, p[13]);
    BF_ENC(l, r, s, p[14]);
    BF_ENC(r, l, s, p[15]);
    BF_ENC(l, r, s, p[16]);
# if BF_ROUNDS == 20
    BF_ENC(r, l, s, p[17]);
    BF_ENC(l, r, s, p[18]);
    BF_ENC(r, l, s, p[19]);
    BF_ENC(l, r, s, p[20]);
# endif
    r ^= p[BF_ROUNDS + 1];

    data[1] = l & 0xffffffffL;
    data[0] = r & 0xffffffffL;
#else
    register BF_LONG l, r, t, *k;

    l = data[0];
    r = data[1];
    k = (BF_LONG *)key;

    l ^= k[0];
    BF_ENC(r, l, k, 1);
    BF_ENC(l, r, k, 2);
    BF_ENC(r, l, k, 3);
    BF_ENC(l, r, k, 4);
    BF_ENC(r, l, k, 5);
    BF_ENC(l, r, k, 6);
    BF_ENC(r, l, k, 7);
    BF_ENC(l, r, k, 8);
    BF_ENC(r, l, k, 9);
    BF_ENC(l, r, k, 10);
    BF_ENC(r, l, k, 11);
    BF_ENC(l, r, k, 12);
    BF_ENC(r, l, k, 13);
    BF_ENC(l, r, k, 14);
    BF_ENC(r, l, k, 15);
    BF_ENC(l, r, k, 16);
# if BF_ROUNDS == 20
    BF_ENC(r, l, k, 17);
    BF_ENC(l, r, k, 18);
    BF_ENC(r, l, k, 19);
    BF_ENC(l, r, k, 20);
# endif
    r ^= k[BF_ROUNDS + 1];

    data[1] = l & 0xffffffffL;
    data[0] = r & 0xffffffffL;
#endif
}

#ifndef BF_DEFAULT_OPTIONS

void BF_decrypt(BF_LONG *data, const BF_KEY *key)
{
# ifndef BF_PTR2
    register BF_LONG l, r;
    register const BF_LONG *p, *s;

    p = key->P;
    s = &(key->S[0]);
    l = data[0];
    r = data[1];

    l ^= p[BF_ROUNDS + 1];
#  if BF_ROUNDS == 20
    BF_ENC(r, l, s, p[20]);
    BF_ENC(l, r, s, p[19]);
    BF_ENC(r, l, s, p[18]);
    BF_ENC(l, r, s, p[17]);
#  endif
    BF_ENC(r, l, s, p[16]);
    BF_ENC(l, r, s, p[15]);
    BF_ENC(r, l, s, p[14]);
    BF_ENC(l, r, s, p[13]);
    BF_ENC(r, l, s, p[12]);
    BF_ENC(l, r, s, p[11]);
    BF_ENC(r, l, s, p[10]);
    BF_ENC(l, r, s, p[9]);
    BF_ENC(r, l, s, p[8]);
    BF_ENC(l, r, s, p[7]);
    BF_ENC(r, l, s, p[6]);
    BF_ENC(l, r, s, p[5]);
    BF_ENC(r, l, s, p[4]);
    BF_ENC(l, r, s, p[3]);
    BF_ENC(r, l, s, p[2]);
    BF_ENC(l, r, s, p[1]);
    r ^= p[0];

    data[1] = l & 0xffffffffL;
    data[0] = r & 0xffffffffL;
# else
    register BF_LONG l, r, t, *k;

    l = data[0];
    r = data[1];
    k = (BF_LONG *)key;

    l ^= k[BF_ROUNDS + 1];
#  if BF_ROUNDS == 20
    BF_ENC(r, l, k, 20);
    BF_ENC(l, r, k, 19);
    BF_ENC(r, l, k, 18);
    BF_ENC(l, r, k, 17);
#  endif
    BF_ENC(r, l, k, 16);
    BF_ENC(l, r, k, 15);
    BF_ENC(r, l, k, 14);
    BF_ENC(l, r, k, 13);
    BF_ENC(r, l, k, 12);
    BF_ENC(l, r, k, 11);
    BF_ENC(r, l, k, 10);
    BF_ENC(l, r, k, 9);
    BF_ENC(r, l, k, 8);
    BF_ENC(l, r, k, 7);
    BF_ENC(r, l, k, 6);
    BF_ENC(l, r, k, 5);
    BF_ENC(r, l, k, 4);
    BF_ENC(l, r, k, 3);
    BF_ENC(r, l, k, 2);
    BF_ENC(l, r, k, 1);
    r ^= k[0];

    data[1] = l & 0xffffffffL;
    data[0] = r & 0xffffffffL;
# endif
}

void BF_cbc_encrypt(const unsigned char *in, unsigned char *out, long length,
                    const BF_KEY *schedule, unsigned char *ivec, int encrypt)
{
    register BF_LONG tin0, tin1;
    register BF_LONG tout0, tout1, xor0, xor1;
    register long l = length;
    BF_LONG tin[2];

    if (encrypt) {
        n2l(ivec, tout0);
        n2l(ivec, tout1);
        ivec -= 8;
        for (l -= 8; l >= 0; l -= 8) {
            n2l(in, tin0);
            n2l(in, tin1);
            tin0 ^= tout0;
            tin1 ^= tout1;
            tin[0] = tin0;
            tin[1] = tin1;
            BF_encrypt(tin, schedule);
            tout0 = tin[0];
            tout1 = tin[1];
            l2n(tout0, out);
            l2n(tout1, out);
        }
        if (l != -8) {
            n2ln(in, tin0, tin1, l + 8);
            tin0 ^= tout0;
            tin1 ^= tout1;
            tin[0] = tin0;
            tin[1] = tin1;
            BF_encrypt(tin, schedule);
            tout0 = tin[0];
            tout1 = tin[1];
            l2n(tout0, out);
            l2n(tout1, out);
        }
        l2n(tout0, ivec);
        l2n(tout1, ivec);
    } else {
        n2l(ivec, xor0);
        n2l(ivec, xor1);
        ivec -= 8;
        for (l -= 8; l >= 0; l -= 8) {
            n2l(in, tin0);
            n2l(in, tin1);
            tin[0] = tin0;
            tin[1] = tin1;
            BF_decrypt(tin, schedule);
            tout0 = tin[0] ^ xor0;
            tout1 = tin[1] ^ xor1;
            l2n(tout0, out);
            l2n(tout1, out);
            xor0 = tin0;
            xor1 = tin1;
        }
        if (l != -8) {
            n2l(in, tin0);
            n2l(in, tin1);
            tin[0] = tin0;
            tin[1] = tin1;
            BF_decrypt(tin, schedule);
            tout0 = tin[0] ^ xor0;
            tout1 = tin[1] ^ xor1;
            l2nn(tout0, tout1, out, l + 8);
            xor0 = tin0;
            xor1 = tin1;
        }
        l2n(xor0, ivec);
        l2n(xor1, ivec);
    }
    tin0 = tin1 = tout0 = tout1 = xor0 = xor1 = 0;
    tin[0] = tin[1] = 0;
}

#endif

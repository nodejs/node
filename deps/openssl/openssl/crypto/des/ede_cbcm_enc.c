/* ede_cbcm_enc.c */
/*
 * Written by Ben Laurie <ben@algroup.co.uk> for the OpenSSL project 13 Feb
 * 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/*
 *
 * This is an implementation of Triple DES Cipher Block Chaining with Output
 * Feedback Masking, by Coppersmith, Johnson and Matyas, (IBM and Certicom).
 *
 * Note that there is a known attack on this by Biham and Knudsen but it
 * takes a lot of work:
 *
 * http://www.cs.technion.ac.il/users/wwwb/cgi-bin/tr-get.cgi/1998/CS/CS0928.ps.gz
 *
 */

#include <openssl/opensslconf.h> /* To see if OPENSSL_NO_DESCBCM is defined */

#ifndef OPENSSL_NO_DESCBCM
# include "des_locl.h"

void DES_ede3_cbcm_encrypt(const unsigned char *in, unsigned char *out,
                           long length, DES_key_schedule *ks1,
                           DES_key_schedule *ks2, DES_key_schedule *ks3,
                           DES_cblock *ivec1, DES_cblock *ivec2, int enc)
{
    register DES_LONG tin0, tin1;
    register DES_LONG tout0, tout1, xor0, xor1, m0, m1;
    register long l = length;
    DES_LONG tin[2];
    unsigned char *iv1, *iv2;

    iv1 = &(*ivec1)[0];
    iv2 = &(*ivec2)[0];

    if (enc) {
        c2l(iv1, m0);
        c2l(iv1, m1);
        c2l(iv2, tout0);
        c2l(iv2, tout1);
        for (l -= 8; l >= -7; l -= 8) {
            tin[0] = m0;
            tin[1] = m1;
            DES_encrypt1(tin, ks3, 1);
            m0 = tin[0];
            m1 = tin[1];

            if (l < 0) {
                c2ln(in, tin0, tin1, l + 8);
            } else {
                c2l(in, tin0);
                c2l(in, tin1);
            }
            tin0 ^= tout0;
            tin1 ^= tout1;

            tin[0] = tin0;
            tin[1] = tin1;
            DES_encrypt1(tin, ks1, 1);
            tin[0] ^= m0;
            tin[1] ^= m1;
            DES_encrypt1(tin, ks2, 0);
            tin[0] ^= m0;
            tin[1] ^= m1;
            DES_encrypt1(tin, ks1, 1);
            tout0 = tin[0];
            tout1 = tin[1];

            l2c(tout0, out);
            l2c(tout1, out);
        }
        iv1 = &(*ivec1)[0];
        l2c(m0, iv1);
        l2c(m1, iv1);

        iv2 = &(*ivec2)[0];
        l2c(tout0, iv2);
        l2c(tout1, iv2);
    } else {
        register DES_LONG t0, t1;

        c2l(iv1, m0);
        c2l(iv1, m1);
        c2l(iv2, xor0);
        c2l(iv2, xor1);
        for (l -= 8; l >= -7; l -= 8) {
            tin[0] = m0;
            tin[1] = m1;
            DES_encrypt1(tin, ks3, 1);
            m0 = tin[0];
            m1 = tin[1];

            c2l(in, tin0);
            c2l(in, tin1);

            t0 = tin0;
            t1 = tin1;

            tin[0] = tin0;
            tin[1] = tin1;
            DES_encrypt1(tin, ks1, 0);
            tin[0] ^= m0;
            tin[1] ^= m1;
            DES_encrypt1(tin, ks2, 1);
            tin[0] ^= m0;
            tin[1] ^= m1;
            DES_encrypt1(tin, ks1, 0);
            tout0 = tin[0];
            tout1 = tin[1];

            tout0 ^= xor0;
            tout1 ^= xor1;
            if (l < 0) {
                l2cn(tout0, tout1, out, l + 8);
            } else {
                l2c(tout0, out);
                l2c(tout1, out);
            }
            xor0 = t0;
            xor1 = t1;
        }

        iv1 = &(*ivec1)[0];
        l2c(m0, iv1);
        l2c(m1, iv1);

        iv2 = &(*ivec2)[0];
        l2c(xor0, iv2);
        l2c(xor1, iv2);
    }
    tin0 = tin1 = tout0 = tout1 = xor0 = xor1 = 0;
    tin[0] = tin[1] = 0;
}
#endif

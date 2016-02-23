/* crypto/des/cbc_cksm.c */
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

DES_LONG DES_cbc_cksum(const unsigned char *in, DES_cblock *output,
                       long length, DES_key_schedule *schedule,
                       const_DES_cblock *ivec)
{
    register DES_LONG tout0, tout1, tin0, tin1;
    register long l = length;
    DES_LONG tin[2];
    unsigned char *out = &(*output)[0];
    const unsigned char *iv = &(*ivec)[0];

    c2l(iv, tout0);
    c2l(iv, tout1);
    for (; l > 0; l -= 8) {
        if (l >= 8) {
            c2l(in, tin0);
            c2l(in, tin1);
        } else
            c2ln(in, tin0, tin1, l);

        tin0 ^= tout0;
        tin[0] = tin0;
        tin1 ^= tout1;
        tin[1] = tin1;
        DES_encrypt1((DES_LONG *)tin, schedule, DES_ENCRYPT);
        /* fix 15/10/91 eay - thanks to keithr@sco.COM */
        tout0 = tin[0];
        tout1 = tin[1];
    }
    if (out != NULL) {
        l2c(tout0, out);
        l2c(tout1, out);
    }
    tout0 = tin0 = tin1 = tin[0] = tin[1] = 0;
    /*
     * Transform the data in tout1 so that it will match the return value
     * that the MIT Kerberos mit_des_cbc_cksum API returns.
     */
    tout1 = ((tout1 >> 24L) & 0x000000FF)
        | ((tout1 >> 8L) & 0x0000FF00)
        | ((tout1 << 8L) & 0x00FF0000)
        | ((tout1 << 24L) & 0xFF000000);
    return (tout1);
}

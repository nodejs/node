/* crypto/des/rpc_enc.c */
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

#include "rpc_des.h"
#include "des_locl.h"
#include "des_ver.h"

int _des_crypt(char *buf, int len, struct desparams *desp);
int _des_crypt(char *buf, int len, struct desparams *desp)
{
    DES_key_schedule ks;
    int enc;

    DES_set_key_unchecked(&desp->des_key, &ks);
    enc = (desp->des_dir == ENCRYPT) ? DES_ENCRYPT : DES_DECRYPT;

    if (desp->des_mode == CBC)
        DES_ecb_encrypt((const_DES_cblock *)desp->UDES.UDES_buf,
                        (DES_cblock *)desp->UDES.UDES_buf, &ks, enc);
    else {
        DES_ncbc_encrypt(desp->UDES.UDES_buf, desp->UDES.UDES_buf,
                         len, &ks, &desp->des_ivec, enc);
#ifdef undef
        /*
         * len will always be %8 if called from common_crypt in secure_rpc.
         * Libdes's cbc encrypt does not copy back the iv, so we have to do
         * it here.
         */
        /* It does now :-) eay 20/09/95 */

        a = (char *)&(desp->UDES.UDES_buf[len - 8]);
        b = (char *)&(desp->des_ivec[0]);

        *(a++) = *(b++);
        *(a++) = *(b++);
        *(a++) = *(b++);
        *(a++) = *(b++);
        *(a++) = *(b++);
        *(a++) = *(b++);
        *(a++) = *(b++);
        *(a++) = *(b++);
#endif
    }
    return (1);
}

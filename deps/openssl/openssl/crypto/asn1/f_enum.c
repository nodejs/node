/* crypto/asn1/f_enum.c */
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

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/buffer.h>
#include <openssl/asn1.h>

/* Based on a_int.c: equivalent ENUMERATED functions */

int i2a_ASN1_ENUMERATED(BIO *bp, ASN1_ENUMERATED *a)
{
    int i, n = 0;
    static const char *h = "0123456789ABCDEF";
    char buf[2];

    if (a == NULL)
        return (0);

    if (a->length == 0) {
        if (BIO_write(bp, "00", 2) != 2)
            goto err;
        n = 2;
    } else {
        for (i = 0; i < a->length; i++) {
            if ((i != 0) && (i % 35 == 0)) {
                if (BIO_write(bp, "\\\n", 2) != 2)
                    goto err;
                n += 2;
            }
            buf[0] = h[((unsigned char)a->data[i] >> 4) & 0x0f];
            buf[1] = h[((unsigned char)a->data[i]) & 0x0f];
            if (BIO_write(bp, buf, 2) != 2)
                goto err;
            n += 2;
        }
    }
    return (n);
 err:
    return (-1);
}

int a2i_ASN1_ENUMERATED(BIO *bp, ASN1_ENUMERATED *bs, char *buf, int size)
{
    int ret = 0;
    int i, j, k, m, n, again, bufsize;
    unsigned char *s = NULL, *sp;
    unsigned char *bufp;
    int num = 0, slen = 0, first = 1;

    bs->type = V_ASN1_ENUMERATED;

    bufsize = BIO_gets(bp, buf, size);
    for (;;) {
        if (bufsize < 1)
            goto err_sl;
        i = bufsize;
        if (buf[i - 1] == '\n')
            buf[--i] = '\0';
        if (i == 0)
            goto err_sl;
        if (buf[i - 1] == '\r')
            buf[--i] = '\0';
        if (i == 0)
            goto err_sl;
        again = (buf[i - 1] == '\\');

        for (j = 0; j < i; j++) {
            if (!(((buf[j] >= '0') && (buf[j] <= '9')) ||
                  ((buf[j] >= 'a') && (buf[j] <= 'f')) ||
                  ((buf[j] >= 'A') && (buf[j] <= 'F')))) {
                i = j;
                break;
            }
        }
        buf[i] = '\0';
        /*
         * We have now cleared all the crap off the end of the line
         */
        if (i < 2)
            goto err_sl;

        bufp = (unsigned char *)buf;
        if (first) {
            first = 0;
            if ((bufp[0] == '0') && (buf[1] == '0')) {
                bufp += 2;
                i -= 2;
            }
        }
        k = 0;
        i -= again;
        if (i % 2 != 0) {
            ASN1err(ASN1_F_A2I_ASN1_ENUMERATED, ASN1_R_ODD_NUMBER_OF_CHARS);
            goto err;
        }
        i /= 2;
        if (num + i > slen) {
            if (s == NULL)
                sp = (unsigned char *)OPENSSL_malloc((unsigned int)num +
                                                     i * 2);
            else
                sp = (unsigned char *)OPENSSL_realloc(s,
                                                      (unsigned int)num +
                                                      i * 2);
            if (sp == NULL) {
                ASN1err(ASN1_F_A2I_ASN1_ENUMERATED, ERR_R_MALLOC_FAILURE);
                goto err;
            }
            s = sp;
            slen = num + i * 2;
        }
        for (j = 0; j < i; j++, k += 2) {
            for (n = 0; n < 2; n++) {
                m = bufp[k + n];
                if ((m >= '0') && (m <= '9'))
                    m -= '0';
                else if ((m >= 'a') && (m <= 'f'))
                    m = m - 'a' + 10;
                else if ((m >= 'A') && (m <= 'F'))
                    m = m - 'A' + 10;
                else {
                    ASN1err(ASN1_F_A2I_ASN1_ENUMERATED,
                            ASN1_R_NON_HEX_CHARACTERS);
                    goto err;
                }
                s[num + j] <<= 4;
                s[num + j] |= m;
            }
        }
        num += i;
        if (again)
            bufsize = BIO_gets(bp, buf, size);
        else
            break;
    }
    bs->length = num;
    bs->data = s;
    ret = 1;
 err:
    if (0) {
 err_sl:
        ASN1err(ASN1_F_A2I_ASN1_ENUMERATED, ASN1_R_SHORT_LINE);
    }
    if (ret != 1)
        OPENSSL_free(s);
    return (ret);
}

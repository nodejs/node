/* crypto/asn1/a_enum.c */
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
#include <openssl/asn1.h>
#include <openssl/bn.h>

/*
 * Code for ENUMERATED type: identical to INTEGER apart from a different tag.
 * for comments on encoding see a_int.c
 */

int ASN1_ENUMERATED_set(ASN1_ENUMERATED *a, long v)
{
    int j, k;
    unsigned int i;
    unsigned char buf[sizeof(long) + 1];
    long d;

    a->type = V_ASN1_ENUMERATED;
    if (a->length < (int)(sizeof(long) + 1)) {
        if (a->data != NULL)
            OPENSSL_free(a->data);
        if ((a->data =
             (unsigned char *)OPENSSL_malloc(sizeof(long) + 1)) != NULL)
            memset((char *)a->data, 0, sizeof(long) + 1);
    }
    if (a->data == NULL) {
        ASN1err(ASN1_F_ASN1_ENUMERATED_SET, ERR_R_MALLOC_FAILURE);
        return (0);
    }
    d = v;
    if (d < 0) {
        d = -d;
        a->type = V_ASN1_NEG_ENUMERATED;
    }

    for (i = 0; i < sizeof(long); i++) {
        if (d == 0)
            break;
        buf[i] = (int)d & 0xff;
        d >>= 8;
    }
    j = 0;
    for (k = i - 1; k >= 0; k--)
        a->data[j++] = buf[k];
    a->length = j;
    return (1);
}

long ASN1_ENUMERATED_get(ASN1_ENUMERATED *a)
{
    int neg = 0, i;
    long r = 0;

    if (a == NULL)
        return (0L);
    i = a->type;
    if (i == V_ASN1_NEG_ENUMERATED)
        neg = 1;
    else if (i != V_ASN1_ENUMERATED)
        return -1;

    if (a->length > (int)sizeof(long)) {
        /* hmm... a bit ugly */
        return (0xffffffffL);
    }
    if (a->data == NULL)
        return 0;

    for (i = 0; i < a->length; i++) {
        r <<= 8;
        r |= (unsigned char)a->data[i];
    }
    if (neg)
        r = -r;
    return (r);
}

ASN1_ENUMERATED *BN_to_ASN1_ENUMERATED(BIGNUM *bn, ASN1_ENUMERATED *ai)
{
    ASN1_ENUMERATED *ret;
    int len, j;

    if (ai == NULL)
        ret = M_ASN1_ENUMERATED_new();
    else
        ret = ai;
    if (ret == NULL) {
        ASN1err(ASN1_F_BN_TO_ASN1_ENUMERATED, ERR_R_NESTED_ASN1_ERROR);
        goto err;
    }
    if (BN_is_negative(bn))
        ret->type = V_ASN1_NEG_ENUMERATED;
    else
        ret->type = V_ASN1_ENUMERATED;
    j = BN_num_bits(bn);
    len = ((j == 0) ? 0 : ((j / 8) + 1));
    if (ret->length < len + 4) {
        unsigned char *new_data = OPENSSL_realloc(ret->data, len + 4);
        if (!new_data) {
            ASN1err(ASN1_F_BN_TO_ASN1_ENUMERATED, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        ret->data = new_data;
    }

    ret->length = BN_bn2bin(bn, ret->data);
    return (ret);
 err:
    if (ret != ai)
        M_ASN1_ENUMERATED_free(ret);
    return (NULL);
}

BIGNUM *ASN1_ENUMERATED_to_BN(ASN1_ENUMERATED *ai, BIGNUM *bn)
{
    BIGNUM *ret;

    if ((ret = BN_bin2bn(ai->data, ai->length, bn)) == NULL)
        ASN1err(ASN1_F_ASN1_ENUMERATED_TO_BN, ASN1_R_BN_LIB);
    else if (ai->type == V_ASN1_NEG_ENUMERATED)
        BN_set_negative(ret, 1);
    return (ret);
}

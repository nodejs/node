/* dh_asn1.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2000.
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
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

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/objects.h>
#include <openssl/asn1t.h>

/* Override the default free and new methods */
static int dh_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
                 void *exarg)
{
    if (operation == ASN1_OP_NEW_PRE) {
        *pval = (ASN1_VALUE *)DH_new();
        if (*pval)
            return 2;
        return 0;
    } else if (operation == ASN1_OP_FREE_PRE) {
        DH_free((DH *)*pval);
        *pval = NULL;
        return 2;
    }
    return 1;
}

ASN1_SEQUENCE_cb(DHparams, dh_cb) = {
        ASN1_SIMPLE(DH, p, BIGNUM),
        ASN1_SIMPLE(DH, g, BIGNUM),
        ASN1_OPT(DH, length, ZLONG),
} ASN1_SEQUENCE_END_cb(DH, DHparams)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DH, DHparams, DHparams)

/*
 * Internal only structures for handling X9.42 DH: this gets translated to or
 * from a DH structure straight away.
 */

typedef struct {
    ASN1_BIT_STRING *seed;
    BIGNUM *counter;
} int_dhvparams;

typedef struct {
    BIGNUM *p;
    BIGNUM *q;
    BIGNUM *g;
    BIGNUM *j;
    int_dhvparams *vparams;
} int_dhx942_dh;

ASN1_SEQUENCE(DHvparams) = {
        ASN1_SIMPLE(int_dhvparams, seed, ASN1_BIT_STRING),
        ASN1_SIMPLE(int_dhvparams, counter, BIGNUM)
} ASN1_SEQUENCE_END_name(int_dhvparams, DHvparams)

ASN1_SEQUENCE(DHxparams) = {
        ASN1_SIMPLE(int_dhx942_dh, p, BIGNUM),
        ASN1_SIMPLE(int_dhx942_dh, g, BIGNUM),
        ASN1_SIMPLE(int_dhx942_dh, q, BIGNUM),
        ASN1_OPT(int_dhx942_dh, j, BIGNUM),
        ASN1_OPT(int_dhx942_dh, vparams, DHvparams),
} ASN1_SEQUENCE_END_name(int_dhx942_dh, DHxparams)

int_dhx942_dh *d2i_int_dhx(int_dhx942_dh **a,
                           const unsigned char **pp, long length);
int i2d_int_dhx(const int_dhx942_dh *a, unsigned char **pp);

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(int_dhx942_dh, DHxparams, int_dhx)

/* Application leve function: read in X9.42 DH parameters into DH structure */

DH *d2i_DHxparams(DH **a, const unsigned char **pp, long length)
{
    int_dhx942_dh *dhx = NULL;
    DH *dh = NULL;
    dh = DH_new();
    if (!dh)
        return NULL;
    dhx = d2i_int_dhx(NULL, pp, length);
    if (!dhx) {
        DH_free(dh);
        return NULL;
    }

    if (a) {
        if (*a)
            DH_free(*a);
        *a = dh;
    }

    dh->p = dhx->p;
    dh->q = dhx->q;
    dh->g = dhx->g;
    dh->j = dhx->j;

    if (dhx->vparams) {
        dh->seed = dhx->vparams->seed->data;
        dh->seedlen = dhx->vparams->seed->length;
        dh->counter = dhx->vparams->counter;
        dhx->vparams->seed->data = NULL;
        ASN1_BIT_STRING_free(dhx->vparams->seed);
        OPENSSL_free(dhx->vparams);
        dhx->vparams = NULL;
    }

    OPENSSL_free(dhx);
    return dh;
}

int i2d_DHxparams(const DH *dh, unsigned char **pp)
{
    int_dhx942_dh dhx;
    int_dhvparams dhv;
    ASN1_BIT_STRING bs;
    dhx.p = dh->p;
    dhx.g = dh->g;
    dhx.q = dh->q;
    dhx.j = dh->j;
    if (dh->counter && dh->seed && dh->seedlen > 0) {
        bs.flags = ASN1_STRING_FLAG_BITS_LEFT;
        bs.data = dh->seed;
        bs.length = dh->seedlen;
        dhv.seed = &bs;
        dhv.counter = dh->counter;
        dhx.vparams = &dhv;
    } else
        dhx.vparams = NULL;

    return i2d_int_dhx(&dhx, pp);
}

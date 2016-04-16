/* p5_pbe.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
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

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/rand.h>

/* PKCS#5 password based encryption structure */

ASN1_SEQUENCE(PBEPARAM) = {
        ASN1_SIMPLE(PBEPARAM, salt, ASN1_OCTET_STRING),
        ASN1_SIMPLE(PBEPARAM, iter, ASN1_INTEGER)
} ASN1_SEQUENCE_END(PBEPARAM)

IMPLEMENT_ASN1_FUNCTIONS(PBEPARAM)

/* Set an algorithm identifier for a PKCS#5 PBE algorithm */

int PKCS5_pbe_set0_algor(X509_ALGOR *algor, int alg, int iter,
                         const unsigned char *salt, int saltlen)
{
    PBEPARAM *pbe = NULL;
    ASN1_STRING *pbe_str = NULL;
    unsigned char *sstr;

    pbe = PBEPARAM_new();
    if (!pbe) {
        ASN1err(ASN1_F_PKCS5_PBE_SET0_ALGOR, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    if (iter <= 0)
        iter = PKCS5_DEFAULT_ITER;
    if (!ASN1_INTEGER_set(pbe->iter, iter)) {
        ASN1err(ASN1_F_PKCS5_PBE_SET0_ALGOR, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    if (!saltlen)
        saltlen = PKCS5_SALT_LEN;
    if (!ASN1_STRING_set(pbe->salt, NULL, saltlen)) {
        ASN1err(ASN1_F_PKCS5_PBE_SET0_ALGOR, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    sstr = ASN1_STRING_data(pbe->salt);
    if (salt)
        memcpy(sstr, salt, saltlen);
    else if (RAND_pseudo_bytes(sstr, saltlen) < 0)
        goto err;

    if (!ASN1_item_pack(pbe, ASN1_ITEM_rptr(PBEPARAM), &pbe_str)) {
        ASN1err(ASN1_F_PKCS5_PBE_SET0_ALGOR, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    PBEPARAM_free(pbe);
    pbe = NULL;

    if (X509_ALGOR_set0(algor, OBJ_nid2obj(alg), V_ASN1_SEQUENCE, pbe_str))
        return 1;

 err:
    if (pbe != NULL)
        PBEPARAM_free(pbe);
    if (pbe_str != NULL)
        ASN1_STRING_free(pbe_str);
    return 0;
}

/* Return an algorithm identifier for a PKCS#5 PBE algorithm */

X509_ALGOR *PKCS5_pbe_set(int alg, int iter,
                          const unsigned char *salt, int saltlen)
{
    X509_ALGOR *ret;
    ret = X509_ALGOR_new();
    if (!ret) {
        ASN1err(ASN1_F_PKCS5_PBE_SET, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    if (PKCS5_pbe_set0_algor(ret, alg, iter, salt, saltlen))
        return ret;

    X509_ALGOR_free(ret);
    return NULL;
}

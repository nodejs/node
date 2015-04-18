/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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
#include <openssl/x509.h>
#include <openssl/asn1.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include "asn1_locl.h"

static void int_dh_free(EVP_PKEY *pkey)
{
    DH_free(pkey->pkey.dh);
}

static int dh_pub_decode(EVP_PKEY *pkey, X509_PUBKEY *pubkey)
{
    const unsigned char *p, *pm;
    int pklen, pmlen;
    int ptype;
    void *pval;
    ASN1_STRING *pstr;
    X509_ALGOR *palg;
    ASN1_INTEGER *public_key = NULL;

    DH *dh = NULL;

    if (!X509_PUBKEY_get0_param(NULL, &p, &pklen, &palg, pubkey))
        return 0;
    X509_ALGOR_get0(NULL, &ptype, &pval, palg);

    if (ptype != V_ASN1_SEQUENCE) {
        DHerr(DH_F_DH_PUB_DECODE, DH_R_PARAMETER_ENCODING_ERROR);
        goto err;
    }

    pstr = pval;
    pm = pstr->data;
    pmlen = pstr->length;

    if (!(dh = d2i_DHparams(NULL, &pm, pmlen))) {
        DHerr(DH_F_DH_PUB_DECODE, DH_R_DECODE_ERROR);
        goto err;
    }

    if (!(public_key = d2i_ASN1_INTEGER(NULL, &p, pklen))) {
        DHerr(DH_F_DH_PUB_DECODE, DH_R_DECODE_ERROR);
        goto err;
    }

    /* We have parameters now set public key */
    if (!(dh->pub_key = ASN1_INTEGER_to_BN(public_key, NULL))) {
        DHerr(DH_F_DH_PUB_DECODE, DH_R_BN_DECODE_ERROR);
        goto err;
    }

    ASN1_INTEGER_free(public_key);
    EVP_PKEY_assign_DH(pkey, dh);
    return 1;

 err:
    if (public_key)
        ASN1_INTEGER_free(public_key);
    if (dh)
        DH_free(dh);
    return 0;

}

static int dh_pub_encode(X509_PUBKEY *pk, const EVP_PKEY *pkey)
{
    DH *dh;
    int ptype;
    unsigned char *penc = NULL;
    int penclen;
    ASN1_STRING *str;
    ASN1_INTEGER *pub_key = NULL;

    dh = pkey->pkey.dh;

    str = ASN1_STRING_new();
    if(!str) {
        DHerr(DH_F_DH_PUB_ENCODE, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    str->length = i2d_DHparams(dh, &str->data);
    if (str->length <= 0) {
        DHerr(DH_F_DH_PUB_ENCODE, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    ptype = V_ASN1_SEQUENCE;

    pub_key = BN_to_ASN1_INTEGER(dh->pub_key, NULL);
    if (!pub_key)
        goto err;

    penclen = i2d_ASN1_INTEGER(pub_key, &penc);

    ASN1_INTEGER_free(pub_key);

    if (penclen <= 0) {
        DHerr(DH_F_DH_PUB_ENCODE, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (X509_PUBKEY_set0_param(pk, OBJ_nid2obj(EVP_PKEY_DH),
                               ptype, str, penc, penclen))
        return 1;

 err:
    if (penc)
        OPENSSL_free(penc);
    if (str)
        ASN1_STRING_free(str);

    return 0;
}

/*
 * PKCS#8 DH is defined in PKCS#11 of all places. It is similar to DH in that
 * the AlgorithmIdentifier contains the paramaters, the private key is
 * explcitly included and the pubkey must be recalculated.
 */

static int dh_priv_decode(EVP_PKEY *pkey, PKCS8_PRIV_KEY_INFO *p8)
{
    const unsigned char *p, *pm;
    int pklen, pmlen;
    int ptype;
    void *pval;
    ASN1_STRING *pstr;
    X509_ALGOR *palg;
    ASN1_INTEGER *privkey = NULL;

    DH *dh = NULL;

    if (!PKCS8_pkey_get0(NULL, &p, &pklen, &palg, p8))
        return 0;

    X509_ALGOR_get0(NULL, &ptype, &pval, palg);

    if (ptype != V_ASN1_SEQUENCE)
        goto decerr;

    if (!(privkey = d2i_ASN1_INTEGER(NULL, &p, pklen)))
        goto decerr;

    pstr = pval;
    pm = pstr->data;
    pmlen = pstr->length;
    if (!(dh = d2i_DHparams(NULL, &pm, pmlen)))
        goto decerr;
    /* We have parameters now set private key */
    if (!(dh->priv_key = ASN1_INTEGER_to_BN(privkey, NULL))) {
        DHerr(DH_F_DH_PRIV_DECODE, DH_R_BN_ERROR);
        goto dherr;
    }
    /* Calculate public key */
    if (!DH_generate_key(dh))
        goto dherr;

    EVP_PKEY_assign_DH(pkey, dh);

    ASN1_STRING_clear_free(privkey);

    return 1;

 decerr:
    DHerr(DH_F_DH_PRIV_DECODE, EVP_R_DECODE_ERROR);
 dherr:
    DH_free(dh);
    ASN1_STRING_clear_free(privkey);
    return 0;
}

static int dh_priv_encode(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pkey)
{
    ASN1_STRING *params = NULL;
    ASN1_INTEGER *prkey = NULL;
    unsigned char *dp = NULL;
    int dplen;

    params = ASN1_STRING_new();

    if (!params) {
        DHerr(DH_F_DH_PRIV_ENCODE, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    params->length = i2d_DHparams(pkey->pkey.dh, &params->data);
    if (params->length <= 0) {
        DHerr(DH_F_DH_PRIV_ENCODE, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    params->type = V_ASN1_SEQUENCE;

    /* Get private key into integer */
    prkey = BN_to_ASN1_INTEGER(pkey->pkey.dh->priv_key, NULL);

    if (!prkey) {
        DHerr(DH_F_DH_PRIV_ENCODE, DH_R_BN_ERROR);
        goto err;
    }

    dplen = i2d_ASN1_INTEGER(prkey, &dp);

    ASN1_STRING_clear_free(prkey);
    prkey = NULL;

    if (!PKCS8_pkey_set0(p8, OBJ_nid2obj(NID_dhKeyAgreement), 0,
                         V_ASN1_SEQUENCE, params, dp, dplen))
        goto err;

    return 1;

 err:
    if (dp != NULL)
        OPENSSL_free(dp);
    if (params != NULL)
        ASN1_STRING_free(params);
    if (prkey != NULL)
        ASN1_STRING_clear_free(prkey);
    return 0;
}

static void update_buflen(const BIGNUM *b, size_t *pbuflen)
{
    size_t i;
    if (!b)
        return;
    if (*pbuflen < (i = (size_t)BN_num_bytes(b)))
        *pbuflen = i;
}

static int dh_param_decode(EVP_PKEY *pkey,
                           const unsigned char **pder, int derlen)
{
    DH *dh;
    if (!(dh = d2i_DHparams(NULL, pder, derlen))) {
        DHerr(DH_F_DH_PARAM_DECODE, ERR_R_DH_LIB);
        return 0;
    }
    EVP_PKEY_assign_DH(pkey, dh);
    return 1;
}

static int dh_param_encode(const EVP_PKEY *pkey, unsigned char **pder)
{
    return i2d_DHparams(pkey->pkey.dh, pder);
}

static int do_dh_print(BIO *bp, const DH *x, int indent,
                       ASN1_PCTX *ctx, int ptype)
{
    unsigned char *m = NULL;
    int reason = ERR_R_BUF_LIB, ret = 0;
    size_t buf_len = 0;

    const char *ktype = NULL;

    BIGNUM *priv_key, *pub_key;

    if (ptype == 2)
        priv_key = x->priv_key;
    else
        priv_key = NULL;

    if (ptype > 0)
        pub_key = x->pub_key;
    else
        pub_key = NULL;

    update_buflen(x->p, &buf_len);

    if (buf_len == 0) {
        reason = ERR_R_PASSED_NULL_PARAMETER;
        goto err;
    }

    update_buflen(x->g, &buf_len);
    update_buflen(pub_key, &buf_len);
    update_buflen(priv_key, &buf_len);

    if (ptype == 2)
        ktype = "PKCS#3 DH Private-Key";
    else if (ptype == 1)
        ktype = "PKCS#3 DH Public-Key";
    else
        ktype = "PKCS#3 DH Parameters";

    m = OPENSSL_malloc(buf_len + 10);
    if (m == NULL) {
        reason = ERR_R_MALLOC_FAILURE;
        goto err;
    }

    BIO_indent(bp, indent, 128);
    if (BIO_printf(bp, "%s: (%d bit)\n", ktype, BN_num_bits(x->p)) <= 0)
        goto err;
    indent += 4;

    if (!ASN1_bn_print(bp, "private-key:", priv_key, m, indent))
        goto err;
    if (!ASN1_bn_print(bp, "public-key:", pub_key, m, indent))
        goto err;

    if (!ASN1_bn_print(bp, "prime:", x->p, m, indent))
        goto err;
    if (!ASN1_bn_print(bp, "generator:", x->g, m, indent))
        goto err;
    if (x->length != 0) {
        BIO_indent(bp, indent, 128);
        if (BIO_printf(bp, "recommended-private-length: %d bits\n",
                       (int)x->length) <= 0)
            goto err;
    }

    ret = 1;
    if (0) {
 err:
        DHerr(DH_F_DO_DH_PRINT, reason);
    }
    if (m != NULL)
        OPENSSL_free(m);
    return (ret);
}

static int int_dh_size(const EVP_PKEY *pkey)
{
    return (DH_size(pkey->pkey.dh));
}

static int dh_bits(const EVP_PKEY *pkey)
{
    return BN_num_bits(pkey->pkey.dh->p);
}

static int dh_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b)
{
    if (BN_cmp(a->pkey.dh->p, b->pkey.dh->p) ||
        BN_cmp(a->pkey.dh->g, b->pkey.dh->g))
        return 0;
    else
        return 1;
}

static int dh_copy_parameters(EVP_PKEY *to, const EVP_PKEY *from)
{
    BIGNUM *a;

    if ((a = BN_dup(from->pkey.dh->p)) == NULL)
        return 0;
    if (to->pkey.dh->p != NULL)
        BN_free(to->pkey.dh->p);
    to->pkey.dh->p = a;

    if ((a = BN_dup(from->pkey.dh->g)) == NULL)
        return 0;
    if (to->pkey.dh->g != NULL)
        BN_free(to->pkey.dh->g);
    to->pkey.dh->g = a;

    return 1;
}

static int dh_missing_parameters(const EVP_PKEY *a)
{
    if (!a->pkey.dh->p || !a->pkey.dh->g)
        return 1;
    return 0;
}

static int dh_pub_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
{
    if (dh_cmp_parameters(a, b) == 0)
        return 0;
    if (BN_cmp(b->pkey.dh->pub_key, a->pkey.dh->pub_key) != 0)
        return 0;
    else
        return 1;
}

static int dh_param_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                          ASN1_PCTX *ctx)
{
    return do_dh_print(bp, pkey->pkey.dh, indent, ctx, 0);
}

static int dh_public_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                           ASN1_PCTX *ctx)
{
    return do_dh_print(bp, pkey->pkey.dh, indent, ctx, 1);
}

static int dh_private_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                            ASN1_PCTX *ctx)
{
    return do_dh_print(bp, pkey->pkey.dh, indent, ctx, 2);
}

int DHparams_print(BIO *bp, const DH *x)
{
    return do_dh_print(bp, x, 4, NULL, 0);
}

const EVP_PKEY_ASN1_METHOD dh_asn1_meth = {
    EVP_PKEY_DH,
    EVP_PKEY_DH,
    0,

    "DH",
    "OpenSSL PKCS#3 DH method",

    dh_pub_decode,
    dh_pub_encode,
    dh_pub_cmp,
    dh_public_print,

    dh_priv_decode,
    dh_priv_encode,
    dh_private_print,

    int_dh_size,
    dh_bits,

    dh_param_decode,
    dh_param_encode,
    dh_missing_parameters,
    dh_copy_parameters,
    dh_cmp_parameters,
    dh_param_print,
    0,

    int_dh_free,
    0
};

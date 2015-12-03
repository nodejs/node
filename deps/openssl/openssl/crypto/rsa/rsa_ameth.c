/* crypto/rsa/rsa_ameth.c */
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
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#ifndef OPENSSL_NO_CMS
# include <openssl/cms.h>
#endif
#include "asn1_locl.h"

static int rsa_pub_encode(X509_PUBKEY *pk, const EVP_PKEY *pkey)
{
    unsigned char *penc = NULL;
    int penclen;
    penclen = i2d_RSAPublicKey(pkey->pkey.rsa, &penc);
    if (penclen <= 0)
        return 0;
    if (X509_PUBKEY_set0_param(pk, OBJ_nid2obj(EVP_PKEY_RSA),
                               V_ASN1_NULL, NULL, penc, penclen))
        return 1;

    OPENSSL_free(penc);
    return 0;
}

static int rsa_pub_decode(EVP_PKEY *pkey, X509_PUBKEY *pubkey)
{
    const unsigned char *p;
    int pklen;
    RSA *rsa = NULL;
    if (!X509_PUBKEY_get0_param(NULL, &p, &pklen, NULL, pubkey))
        return 0;
    if (!(rsa = d2i_RSAPublicKey(NULL, &p, pklen))) {
        RSAerr(RSA_F_RSA_PUB_DECODE, ERR_R_RSA_LIB);
        return 0;
    }
    EVP_PKEY_assign_RSA(pkey, rsa);
    return 1;
}

static int rsa_pub_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
{
    if (BN_cmp(b->pkey.rsa->n, a->pkey.rsa->n) != 0
        || BN_cmp(b->pkey.rsa->e, a->pkey.rsa->e) != 0)
        return 0;
    return 1;
}

static int old_rsa_priv_decode(EVP_PKEY *pkey,
                               const unsigned char **pder, int derlen)
{
    RSA *rsa;
    if (!(rsa = d2i_RSAPrivateKey(NULL, pder, derlen))) {
        RSAerr(RSA_F_OLD_RSA_PRIV_DECODE, ERR_R_RSA_LIB);
        return 0;
    }
    EVP_PKEY_assign_RSA(pkey, rsa);
    return 1;
}

static int old_rsa_priv_encode(const EVP_PKEY *pkey, unsigned char **pder)
{
    return i2d_RSAPrivateKey(pkey->pkey.rsa, pder);
}

static int rsa_priv_encode(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pkey)
{
    unsigned char *rk = NULL;
    int rklen;
    rklen = i2d_RSAPrivateKey(pkey->pkey.rsa, &rk);

    if (rklen <= 0) {
        RSAerr(RSA_F_RSA_PRIV_ENCODE, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    if (!PKCS8_pkey_set0(p8, OBJ_nid2obj(NID_rsaEncryption), 0,
                         V_ASN1_NULL, NULL, rk, rklen)) {
        RSAerr(RSA_F_RSA_PRIV_ENCODE, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    return 1;
}

static int rsa_priv_decode(EVP_PKEY *pkey, PKCS8_PRIV_KEY_INFO *p8)
{
    const unsigned char *p;
    int pklen;
    if (!PKCS8_pkey_get0(NULL, &p, &pklen, NULL, p8))
        return 0;
    return old_rsa_priv_decode(pkey, &p, pklen);
}

static int int_rsa_size(const EVP_PKEY *pkey)
{
    return RSA_size(pkey->pkey.rsa);
}

static int rsa_bits(const EVP_PKEY *pkey)
{
    return BN_num_bits(pkey->pkey.rsa->n);
}

static void int_rsa_free(EVP_PKEY *pkey)
{
    RSA_free(pkey->pkey.rsa);
}

static void update_buflen(const BIGNUM *b, size_t *pbuflen)
{
    size_t i;
    if (!b)
        return;
    if (*pbuflen < (i = (size_t)BN_num_bytes(b)))
        *pbuflen = i;
}

static int do_rsa_print(BIO *bp, const RSA *x, int off, int priv)
{
    char *str;
    const char *s;
    unsigned char *m = NULL;
    int ret = 0, mod_len = 0;
    size_t buf_len = 0;

    update_buflen(x->n, &buf_len);
    update_buflen(x->e, &buf_len);

    if (priv) {
        update_buflen(x->d, &buf_len);
        update_buflen(x->p, &buf_len);
        update_buflen(x->q, &buf_len);
        update_buflen(x->dmp1, &buf_len);
        update_buflen(x->dmq1, &buf_len);
        update_buflen(x->iqmp, &buf_len);
    }

    m = (unsigned char *)OPENSSL_malloc(buf_len + 10);
    if (m == NULL) {
        RSAerr(RSA_F_DO_RSA_PRINT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (x->n != NULL)
        mod_len = BN_num_bits(x->n);

    if (!BIO_indent(bp, off, 128))
        goto err;

    if (priv && x->d) {
        if (BIO_printf(bp, "Private-Key: (%d bit)\n", mod_len)
            <= 0)
            goto err;
        str = "modulus:";
        s = "publicExponent:";
    } else {
        if (BIO_printf(bp, "Public-Key: (%d bit)\n", mod_len)
            <= 0)
            goto err;
        str = "Modulus:";
        s = "Exponent:";
    }
    if (!ASN1_bn_print(bp, str, x->n, m, off))
        goto err;
    if (!ASN1_bn_print(bp, s, x->e, m, off))
        goto err;
    if (priv) {
        if (!ASN1_bn_print(bp, "privateExponent:", x->d, m, off))
            goto err;
        if (!ASN1_bn_print(bp, "prime1:", x->p, m, off))
            goto err;
        if (!ASN1_bn_print(bp, "prime2:", x->q, m, off))
            goto err;
        if (!ASN1_bn_print(bp, "exponent1:", x->dmp1, m, off))
            goto err;
        if (!ASN1_bn_print(bp, "exponent2:", x->dmq1, m, off))
            goto err;
        if (!ASN1_bn_print(bp, "coefficient:", x->iqmp, m, off))
            goto err;
    }
    ret = 1;
 err:
    if (m != NULL)
        OPENSSL_free(m);
    return (ret);
}

static int rsa_pub_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                         ASN1_PCTX *ctx)
{
    return do_rsa_print(bp, pkey->pkey.rsa, indent, 0);
}

static int rsa_priv_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                          ASN1_PCTX *ctx)
{
    return do_rsa_print(bp, pkey->pkey.rsa, indent, 1);
}

static RSA_PSS_PARAMS *rsa_pss_decode(const X509_ALGOR *alg,
                                      X509_ALGOR **pmaskHash)
{
    const unsigned char *p;
    int plen;
    RSA_PSS_PARAMS *pss;

    *pmaskHash = NULL;

    if (!alg->parameter || alg->parameter->type != V_ASN1_SEQUENCE)
        return NULL;
    p = alg->parameter->value.sequence->data;
    plen = alg->parameter->value.sequence->length;
    pss = d2i_RSA_PSS_PARAMS(NULL, &p, plen);

    if (!pss)
        return NULL;

    if (pss->maskGenAlgorithm) {
        ASN1_TYPE *param = pss->maskGenAlgorithm->parameter;
        if (OBJ_obj2nid(pss->maskGenAlgorithm->algorithm) == NID_mgf1
            && param && param->type == V_ASN1_SEQUENCE) {
            p = param->value.sequence->data;
            plen = param->value.sequence->length;
            *pmaskHash = d2i_X509_ALGOR(NULL, &p, plen);
        }
    }

    return pss;
}

static int rsa_pss_param_print(BIO *bp, RSA_PSS_PARAMS *pss,
                               X509_ALGOR *maskHash, int indent)
{
    int rv = 0;
    if (!pss) {
        if (BIO_puts(bp, " (INVALID PSS PARAMETERS)\n") <= 0)
            return 0;
        return 1;
    }
    if (BIO_puts(bp, "\n") <= 0)
        goto err;
    if (!BIO_indent(bp, indent, 128))
        goto err;
    if (BIO_puts(bp, "Hash Algorithm: ") <= 0)
        goto err;

    if (pss->hashAlgorithm) {
        if (i2a_ASN1_OBJECT(bp, pss->hashAlgorithm->algorithm) <= 0)
            goto err;
    } else if (BIO_puts(bp, "sha1 (default)") <= 0)
        goto err;

    if (BIO_puts(bp, "\n") <= 0)
        goto err;

    if (!BIO_indent(bp, indent, 128))
        goto err;

    if (BIO_puts(bp, "Mask Algorithm: ") <= 0)
        goto err;
    if (pss->maskGenAlgorithm) {
        if (i2a_ASN1_OBJECT(bp, pss->maskGenAlgorithm->algorithm) <= 0)
            goto err;
        if (BIO_puts(bp, " with ") <= 0)
            goto err;
        if (maskHash) {
            if (i2a_ASN1_OBJECT(bp, maskHash->algorithm) <= 0)
                goto err;
        } else if (BIO_puts(bp, "INVALID") <= 0)
            goto err;
    } else if (BIO_puts(bp, "mgf1 with sha1 (default)") <= 0)
        goto err;
    BIO_puts(bp, "\n");

    if (!BIO_indent(bp, indent, 128))
        goto err;
    if (BIO_puts(bp, "Salt Length: 0x") <= 0)
        goto err;
    if (pss->saltLength) {
        if (i2a_ASN1_INTEGER(bp, pss->saltLength) <= 0)
            goto err;
    } else if (BIO_puts(bp, "14 (default)") <= 0)
        goto err;
    BIO_puts(bp, "\n");

    if (!BIO_indent(bp, indent, 128))
        goto err;
    if (BIO_puts(bp, "Trailer Field: 0x") <= 0)
        goto err;
    if (pss->trailerField) {
        if (i2a_ASN1_INTEGER(bp, pss->trailerField) <= 0)
            goto err;
    } else if (BIO_puts(bp, "BC (default)") <= 0)
        goto err;
    BIO_puts(bp, "\n");

    rv = 1;

 err:
    return rv;

}

static int rsa_sig_print(BIO *bp, const X509_ALGOR *sigalg,
                         const ASN1_STRING *sig, int indent, ASN1_PCTX *pctx)
{
    if (OBJ_obj2nid(sigalg->algorithm) == NID_rsassaPss) {
        int rv;
        RSA_PSS_PARAMS *pss;
        X509_ALGOR *maskHash;
        pss = rsa_pss_decode(sigalg, &maskHash);
        rv = rsa_pss_param_print(bp, pss, maskHash, indent);
        if (pss)
            RSA_PSS_PARAMS_free(pss);
        if (maskHash)
            X509_ALGOR_free(maskHash);
        if (!rv)
            return 0;
    } else if (!sig && BIO_puts(bp, "\n") <= 0)
        return 0;
    if (sig)
        return X509_signature_dump(bp, sig, indent);
    return 1;
}

static int rsa_pkey_ctrl(EVP_PKEY *pkey, int op, long arg1, void *arg2)
{
    X509_ALGOR *alg = NULL;
    switch (op) {

    case ASN1_PKEY_CTRL_PKCS7_SIGN:
        if (arg1 == 0)
            PKCS7_SIGNER_INFO_get0_algs(arg2, NULL, NULL, &alg);
        break;

    case ASN1_PKEY_CTRL_PKCS7_ENCRYPT:
        if (arg1 == 0)
            PKCS7_RECIP_INFO_get0_alg(arg2, &alg);
        break;
#ifndef OPENSSL_NO_CMS
    case ASN1_PKEY_CTRL_CMS_SIGN:
        if (arg1 == 0)
            CMS_SignerInfo_get0_algs(arg2, NULL, NULL, NULL, &alg);
        break;

    case ASN1_PKEY_CTRL_CMS_ENVELOPE:
        if (arg1 == 0)
            CMS_RecipientInfo_ktri_get0_algs(arg2, NULL, NULL, &alg);
        break;
#endif

    case ASN1_PKEY_CTRL_DEFAULT_MD_NID:
        *(int *)arg2 = NID_sha1;
        return 1;

    default:
        return -2;

    }

    if (alg)
        X509_ALGOR_set0(alg, OBJ_nid2obj(NID_rsaEncryption), V_ASN1_NULL, 0);

    return 1;

}

/*
 * Customised RSA item verification routine. This is called when a signature
 * is encountered requiring special handling. We currently only handle PSS.
 */

static int rsa_item_verify(EVP_MD_CTX *ctx, const ASN1_ITEM *it, void *asn,
                           X509_ALGOR *sigalg, ASN1_BIT_STRING *sig,
                           EVP_PKEY *pkey)
{
    int rv = -1;
    int saltlen;
    const EVP_MD *mgf1md = NULL, *md = NULL;
    RSA_PSS_PARAMS *pss;
    X509_ALGOR *maskHash;
    EVP_PKEY_CTX *pkctx;
    /* Sanity check: make sure it is PSS */
    if (OBJ_obj2nid(sigalg->algorithm) != NID_rsassaPss) {
        RSAerr(RSA_F_RSA_ITEM_VERIFY, RSA_R_UNSUPPORTED_SIGNATURE_TYPE);
        return -1;
    }
    /* Decode PSS parameters */
    pss = rsa_pss_decode(sigalg, &maskHash);

    if (pss == NULL) {
        RSAerr(RSA_F_RSA_ITEM_VERIFY, RSA_R_INVALID_PSS_PARAMETERS);
        goto err;
    }
    /* Check mask and lookup mask hash algorithm */
    if (pss->maskGenAlgorithm) {
        if (OBJ_obj2nid(pss->maskGenAlgorithm->algorithm) != NID_mgf1) {
            RSAerr(RSA_F_RSA_ITEM_VERIFY, RSA_R_UNSUPPORTED_MASK_ALGORITHM);
            goto err;
        }
        if (!maskHash) {
            RSAerr(RSA_F_RSA_ITEM_VERIFY, RSA_R_UNSUPPORTED_MASK_PARAMETER);
            goto err;
        }
        mgf1md = EVP_get_digestbyobj(maskHash->algorithm);
        if (mgf1md == NULL) {
            RSAerr(RSA_F_RSA_ITEM_VERIFY, RSA_R_UNKNOWN_MASK_DIGEST);
            goto err;
        }
    } else
        mgf1md = EVP_sha1();

    if (pss->hashAlgorithm) {
        md = EVP_get_digestbyobj(pss->hashAlgorithm->algorithm);
        if (md == NULL) {
            RSAerr(RSA_F_RSA_ITEM_VERIFY, RSA_R_UNKNOWN_PSS_DIGEST);
            goto err;
        }
    } else
        md = EVP_sha1();

    if (pss->saltLength) {
        saltlen = ASN1_INTEGER_get(pss->saltLength);

        /*
         * Could perform more salt length sanity checks but the main RSA
         * routines will trap other invalid values anyway.
         */
        if (saltlen < 0) {
            RSAerr(RSA_F_RSA_ITEM_VERIFY, RSA_R_INVALID_SALT_LENGTH);
            goto err;
        }
    } else
        saltlen = 20;

    /*
     * low-level routines support only trailer field 0xbc (value 1) and
     * PKCS#1 says we should reject any other value anyway.
     */
    if (pss->trailerField && ASN1_INTEGER_get(pss->trailerField) != 1) {
        RSAerr(RSA_F_RSA_ITEM_VERIFY, RSA_R_INVALID_TRAILER);
        goto err;
    }

    /* We have all parameters now set up context */

    if (!EVP_DigestVerifyInit(ctx, &pkctx, md, NULL, pkey))
        goto err;

    if (EVP_PKEY_CTX_set_rsa_padding(pkctx, RSA_PKCS1_PSS_PADDING) <= 0)
        goto err;

    if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pkctx, saltlen) <= 0)
        goto err;

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(pkctx, mgf1md) <= 0)
        goto err;
    /* Carry on */
    rv = 2;

 err:
    RSA_PSS_PARAMS_free(pss);
    if (maskHash)
        X509_ALGOR_free(maskHash);
    return rv;
}

static int rsa_item_sign(EVP_MD_CTX *ctx, const ASN1_ITEM *it, void *asn,
                         X509_ALGOR *alg1, X509_ALGOR *alg2,
                         ASN1_BIT_STRING *sig)
{
    int pad_mode;
    EVP_PKEY_CTX *pkctx = ctx->pctx;
    if (EVP_PKEY_CTX_get_rsa_padding(pkctx, &pad_mode) <= 0)
        return 0;
    if (pad_mode == RSA_PKCS1_PADDING)
        return 2;
    if (pad_mode == RSA_PKCS1_PSS_PADDING) {
        const EVP_MD *sigmd, *mgf1md;
        RSA_PSS_PARAMS *pss = NULL;
        X509_ALGOR *mgf1alg = NULL;
        ASN1_STRING *os1 = NULL, *os2 = NULL;
        EVP_PKEY *pk = EVP_PKEY_CTX_get0_pkey(pkctx);
        int saltlen, rv = 0;
        sigmd = EVP_MD_CTX_md(ctx);
        if (EVP_PKEY_CTX_get_rsa_mgf1_md(pkctx, &mgf1md) <= 0)
            goto err;
        if (!EVP_PKEY_CTX_get_rsa_pss_saltlen(pkctx, &saltlen))
            goto err;
        if (saltlen == -1)
            saltlen = EVP_MD_size(sigmd);
        else if (saltlen == -2) {
            saltlen = EVP_PKEY_size(pk) - EVP_MD_size(sigmd) - 2;
            if (((EVP_PKEY_bits(pk) - 1) & 0x7) == 0)
                saltlen--;
        }
        pss = RSA_PSS_PARAMS_new();
        if (!pss)
            goto err;
        if (saltlen != 20) {
            pss->saltLength = ASN1_INTEGER_new();
            if (!pss->saltLength)
                goto err;
            if (!ASN1_INTEGER_set(pss->saltLength, saltlen))
                goto err;
        }
        if (EVP_MD_type(sigmd) != NID_sha1) {
            pss->hashAlgorithm = X509_ALGOR_new();
            if (!pss->hashAlgorithm)
                goto err;
            X509_ALGOR_set_md(pss->hashAlgorithm, sigmd);
        }
        if (EVP_MD_type(mgf1md) != NID_sha1) {
            ASN1_STRING *stmp = NULL;
            /* need to embed algorithm ID inside another */
            mgf1alg = X509_ALGOR_new();
            X509_ALGOR_set_md(mgf1alg, mgf1md);
            if (!ASN1_item_pack(mgf1alg, ASN1_ITEM_rptr(X509_ALGOR), &stmp))
                 goto err;
            pss->maskGenAlgorithm = X509_ALGOR_new();
            if (!pss->maskGenAlgorithm)
                goto err;
            X509_ALGOR_set0(pss->maskGenAlgorithm,
                            OBJ_nid2obj(NID_mgf1), V_ASN1_SEQUENCE, stmp);
        }
        /* Finally create string with pss parameter encoding. */
        if (!ASN1_item_pack(pss, ASN1_ITEM_rptr(RSA_PSS_PARAMS), &os1))
             goto err;
        if (alg2) {
            os2 = ASN1_STRING_dup(os1);
            if (!os2)
                goto err;
            X509_ALGOR_set0(alg2, OBJ_nid2obj(NID_rsassaPss),
                            V_ASN1_SEQUENCE, os2);
        }
        X509_ALGOR_set0(alg1, OBJ_nid2obj(NID_rsassaPss),
                        V_ASN1_SEQUENCE, os1);
        os1 = os2 = NULL;
        rv = 3;
 err:
        if (mgf1alg)
            X509_ALGOR_free(mgf1alg);
        if (pss)
            RSA_PSS_PARAMS_free(pss);
        if (os1)
            ASN1_STRING_free(os1);
        return rv;

    }
    return 2;
}

const EVP_PKEY_ASN1_METHOD rsa_asn1_meths[] = {
    {
     EVP_PKEY_RSA,
     EVP_PKEY_RSA,
     ASN1_PKEY_SIGPARAM_NULL,

     "RSA",
     "OpenSSL RSA method",

     rsa_pub_decode,
     rsa_pub_encode,
     rsa_pub_cmp,
     rsa_pub_print,

     rsa_priv_decode,
     rsa_priv_encode,
     rsa_priv_print,

     int_rsa_size,
     rsa_bits,

     0, 0, 0, 0, 0, 0,

     rsa_sig_print,
     int_rsa_free,
     rsa_pkey_ctrl,
     old_rsa_priv_decode,
     old_rsa_priv_encode,
     rsa_item_verify,
     rsa_item_sign},

    {
     EVP_PKEY_RSA2,
     EVP_PKEY_RSA,
     ASN1_PKEY_ALIAS}
};

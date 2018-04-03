/* crypto/rsa/rsa_pmeth.c */
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
#include <openssl/evp.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_NO_CMS
# include <openssl/cms.h>
#endif
#ifdef OPENSSL_FIPS
# include <openssl/fips.h>
#endif
#include "evp_locl.h"
#include "rsa_locl.h"

/* RSA pkey context structure */

typedef struct {
    /* Key gen parameters */
    int nbits;
    BIGNUM *pub_exp;
    /* Keygen callback info */
    int gentmp[2];
    /* RSA padding mode */
    int pad_mode;
    /* message digest */
    const EVP_MD *md;
    /* message digest for MGF1 */
    const EVP_MD *mgf1md;
    /* PSS salt length */
    int saltlen;
    /* Temp buffer */
    unsigned char *tbuf;
    /* OAEP label */
    unsigned char *oaep_label;
    size_t oaep_labellen;
} RSA_PKEY_CTX;

static int pkey_rsa_init(EVP_PKEY_CTX *ctx)
{
    RSA_PKEY_CTX *rctx;
    rctx = OPENSSL_malloc(sizeof(RSA_PKEY_CTX));
    if (!rctx)
        return 0;
    rctx->nbits = 1024;
    rctx->pub_exp = NULL;
    rctx->pad_mode = RSA_PKCS1_PADDING;
    rctx->md = NULL;
    rctx->mgf1md = NULL;
    rctx->tbuf = NULL;

    rctx->saltlen = -2;

    rctx->oaep_label = NULL;
    rctx->oaep_labellen = 0;

    ctx->data = rctx;
    ctx->keygen_info = rctx->gentmp;
    ctx->keygen_info_count = 2;

    return 1;
}

static int pkey_rsa_copy(EVP_PKEY_CTX *dst, EVP_PKEY_CTX *src)
{
    RSA_PKEY_CTX *dctx, *sctx;
    if (!pkey_rsa_init(dst))
        return 0;
    sctx = src->data;
    dctx = dst->data;
    dctx->nbits = sctx->nbits;
    if (sctx->pub_exp) {
        dctx->pub_exp = BN_dup(sctx->pub_exp);
        if (!dctx->pub_exp)
            return 0;
    }
    dctx->pad_mode = sctx->pad_mode;
    dctx->md = sctx->md;
    dctx->mgf1md = sctx->mgf1md;
    if (sctx->oaep_label) {
        if (dctx->oaep_label)
            OPENSSL_free(dctx->oaep_label);
        dctx->oaep_label = BUF_memdup(sctx->oaep_label, sctx->oaep_labellen);
        if (!dctx->oaep_label)
            return 0;
        dctx->oaep_labellen = sctx->oaep_labellen;
    }
    return 1;
}

static int setup_tbuf(RSA_PKEY_CTX *ctx, EVP_PKEY_CTX *pk)
{
    if (ctx->tbuf)
        return 1;
    ctx->tbuf = OPENSSL_malloc(EVP_PKEY_size(pk->pkey));
    if (!ctx->tbuf)
        return 0;
    return 1;
}

static void pkey_rsa_cleanup(EVP_PKEY_CTX *ctx)
{
    RSA_PKEY_CTX *rctx = ctx->data;
    if (rctx) {
        if (rctx->pub_exp)
            BN_free(rctx->pub_exp);
        if (rctx->tbuf)
            OPENSSL_free(rctx->tbuf);
        if (rctx->oaep_label)
            OPENSSL_free(rctx->oaep_label);
        OPENSSL_free(rctx);
    }
}

#ifdef OPENSSL_FIPS
/*
 * FIP checker. Return value indicates status of context parameters: 1 :
 * redirect to FIPS. 0 : don't redirect to FIPS. -1 : illegal operation in
 * FIPS mode.
 */

static int pkey_fips_check_rsa(const RSA *rsa, const EVP_MD **pmd,
                               const EVP_MD **pmgf1md)
{
    int rv = -1;

    if (!FIPS_mode())
        return 0;
    if (rsa->flags & RSA_FLAG_NON_FIPS_ALLOW)
        rv = 0;
    if (!(rsa->meth->flags & RSA_FLAG_FIPS_METHOD) && rv)
        return -1;
    if (*pmd != NULL) {
        *pmd = FIPS_get_digestbynid(EVP_MD_type(*pmd));
        if (*pmd == NULL || !((*pmd)->flags & EVP_MD_FLAG_FIPS))
            return rv;
    }
    if (*pmgf1md != NULL) {
        *pmgf1md = FIPS_get_digestbynid(EVP_MD_type(*pmgf1md));
        if (*pmgf1md == NULL || !((*pmgf1md)->flags & EVP_MD_FLAG_FIPS))
            return rv;
    }
    return 1;
}
#endif

static int pkey_rsa_sign(EVP_PKEY_CTX *ctx, unsigned char *sig,
                         size_t *siglen, const unsigned char *tbs,
                         size_t tbslen)
{
    int ret;
    RSA_PKEY_CTX *rctx = ctx->data;
    RSA *rsa = ctx->pkey->pkey.rsa;
    const EVP_MD *md = rctx->md;
    const EVP_MD *mgf1md = rctx->mgf1md;

#ifdef OPENSSL_FIPS
    ret = pkey_fips_check_rsa(rsa, &md, &mgf1md);
    if (ret < 0) {
        RSAerr(RSA_F_PKEY_RSA_SIGN, RSA_R_OPERATION_NOT_ALLOWED_IN_FIPS_MODE);
        return -1;
    }
#endif

    if (md != NULL) {
        if (tbslen != (size_t)EVP_MD_size(md)) {
            RSAerr(RSA_F_PKEY_RSA_SIGN, RSA_R_INVALID_DIGEST_LENGTH);
            return -1;
        }
#ifdef OPENSSL_FIPS
        if (ret > 0) {
            unsigned int slen;
            ret = FIPS_rsa_sign_digest(rsa, tbs, tbslen, md, rctx->pad_mode,
                                       rctx->saltlen, mgf1md, sig, &slen);
            if (ret > 0)
                *siglen = slen;
            else
                *siglen = 0;
            return ret;
        }
#endif

        if (EVP_MD_type(md) == NID_mdc2) {
            unsigned int sltmp;
            if (rctx->pad_mode != RSA_PKCS1_PADDING)
                return -1;
            ret = RSA_sign_ASN1_OCTET_STRING(NID_mdc2, tbs, tbslen, sig, &sltmp,
                                             rsa);

            if (ret <= 0)
                return ret;
            ret = sltmp;
        } else if (rctx->pad_mode == RSA_X931_PADDING) {
            if ((size_t)EVP_PKEY_size(ctx->pkey) < tbslen + 1) {
                RSAerr(RSA_F_PKEY_RSA_SIGN, RSA_R_KEY_SIZE_TOO_SMALL);
                return -1;
            }
            if (!setup_tbuf(rctx, ctx)) {
                RSAerr(RSA_F_PKEY_RSA_SIGN, ERR_R_MALLOC_FAILURE);
                return -1;
            }
            memcpy(rctx->tbuf, tbs, tbslen);
            rctx->tbuf[tbslen] = RSA_X931_hash_id(EVP_MD_type(md));
            ret = RSA_private_encrypt(tbslen + 1, rctx->tbuf,
                                      sig, rsa, RSA_X931_PADDING);
        } else if (rctx->pad_mode == RSA_PKCS1_PADDING) {
            unsigned int sltmp;
            ret = RSA_sign(EVP_MD_type(md), tbs, tbslen, sig, &sltmp, rsa);
            if (ret <= 0)
                return ret;
            ret = sltmp;
        } else if (rctx->pad_mode == RSA_PKCS1_PSS_PADDING) {
            if (!setup_tbuf(rctx, ctx))
                return -1;
            if (!RSA_padding_add_PKCS1_PSS_mgf1(rsa, rctx->tbuf, tbs,
                                                md, mgf1md, rctx->saltlen))
                return -1;
            ret = RSA_private_encrypt(RSA_size(rsa), rctx->tbuf,
                                      sig, rsa, RSA_NO_PADDING);
        } else
            return -1;
    } else
        ret = RSA_private_encrypt(tbslen, tbs, sig, ctx->pkey->pkey.rsa,
                                  rctx->pad_mode);
    if (ret < 0)
        return ret;
    *siglen = ret;
    return 1;
}

static int pkey_rsa_verifyrecover(EVP_PKEY_CTX *ctx,
                                  unsigned char *rout, size_t *routlen,
                                  const unsigned char *sig, size_t siglen)
{
    int ret;
    RSA_PKEY_CTX *rctx = ctx->data;

    if (rctx->md) {
        if (rctx->pad_mode == RSA_X931_PADDING) {
            if (!setup_tbuf(rctx, ctx))
                return -1;
            ret = RSA_public_decrypt(siglen, sig,
                                     rctx->tbuf, ctx->pkey->pkey.rsa,
                                     RSA_X931_PADDING);
            if (ret < 1)
                return 0;
            ret--;
            if (rctx->tbuf[ret] != RSA_X931_hash_id(EVP_MD_type(rctx->md))) {
                RSAerr(RSA_F_PKEY_RSA_VERIFYRECOVER,
                       RSA_R_ALGORITHM_MISMATCH);
                return 0;
            }
            if (ret != EVP_MD_size(rctx->md)) {
                RSAerr(RSA_F_PKEY_RSA_VERIFYRECOVER,
                       RSA_R_INVALID_DIGEST_LENGTH);
                return 0;
            }
            if (rout)
                memcpy(rout, rctx->tbuf, ret);
        } else if (rctx->pad_mode == RSA_PKCS1_PADDING) {
            size_t sltmp;
            ret = int_rsa_verify(EVP_MD_type(rctx->md),
                                 NULL, 0, rout, &sltmp,
                                 sig, siglen, ctx->pkey->pkey.rsa);
            if (ret <= 0)
                return 0;
            ret = sltmp;
        } else
            return -1;
    } else
        ret = RSA_public_decrypt(siglen, sig, rout, ctx->pkey->pkey.rsa,
                                 rctx->pad_mode);
    if (ret < 0)
        return ret;
    *routlen = ret;
    return 1;
}

static int pkey_rsa_verify(EVP_PKEY_CTX *ctx,
                           const unsigned char *sig, size_t siglen,
                           const unsigned char *tbs, size_t tbslen)
{
    RSA_PKEY_CTX *rctx = ctx->data;
    RSA *rsa = ctx->pkey->pkey.rsa;
    const EVP_MD *md = rctx->md;
    const EVP_MD *mgf1md = rctx->mgf1md;
    size_t rslen;

#ifdef OPENSSL_FIPS
    int rv = pkey_fips_check_rsa(rsa, &md, &mgf1md);

    if (rv < 0) {
        RSAerr(RSA_F_PKEY_RSA_VERIFY,
               RSA_R_OPERATION_NOT_ALLOWED_IN_FIPS_MODE);
        return -1;
    }
#endif
    if (md != NULL) {
#ifdef OPENSSL_FIPS
        if (rv > 0) {
            return FIPS_rsa_verify_digest(rsa, tbs, tbslen, md, rctx->pad_mode,
                                          rctx->saltlen, mgf1md, sig, siglen);

        }
#endif
        if (rctx->pad_mode == RSA_PKCS1_PADDING)
            return RSA_verify(EVP_MD_type(md), tbs, tbslen,
                              sig, siglen, rsa);
        if (tbslen != (size_t)EVP_MD_size(md)) {
            RSAerr(RSA_F_PKEY_RSA_VERIFY, RSA_R_INVALID_DIGEST_LENGTH);
            return -1;
        }
        if (rctx->pad_mode == RSA_X931_PADDING) {
            if (pkey_rsa_verifyrecover(ctx, NULL, &rslen, sig, siglen) <= 0)
                return 0;
        } else if (rctx->pad_mode == RSA_PKCS1_PSS_PADDING) {
            int ret;
            if (!setup_tbuf(rctx, ctx))
                return -1;
            ret = RSA_public_decrypt(siglen, sig, rctx->tbuf,
                                     rsa, RSA_NO_PADDING);
            if (ret <= 0)
                return 0;
            ret = RSA_verify_PKCS1_PSS_mgf1(rsa, tbs, md, mgf1md,
                                            rctx->tbuf, rctx->saltlen);
            if (ret <= 0)
                return 0;
            return 1;
        } else
            return -1;
    } else {
        if (!setup_tbuf(rctx, ctx))
            return -1;
        rslen = RSA_public_decrypt(siglen, sig, rctx->tbuf,
                                   rsa, rctx->pad_mode);
        if (rslen == 0)
            return 0;
    }

    if ((rslen != tbslen) || memcmp(tbs, rctx->tbuf, rslen))
        return 0;

    return 1;

}

static int pkey_rsa_encrypt(EVP_PKEY_CTX *ctx,
                            unsigned char *out, size_t *outlen,
                            const unsigned char *in, size_t inlen)
{
    int ret;
    RSA_PKEY_CTX *rctx = ctx->data;
    if (rctx->pad_mode == RSA_PKCS1_OAEP_PADDING) {
        int klen = RSA_size(ctx->pkey->pkey.rsa);
        if (!setup_tbuf(rctx, ctx))
            return -1;
        if (!RSA_padding_add_PKCS1_OAEP_mgf1(rctx->tbuf, klen,
                                             in, inlen,
                                             rctx->oaep_label,
                                             rctx->oaep_labellen,
                                             rctx->md, rctx->mgf1md))
            return -1;
        ret = RSA_public_encrypt(klen, rctx->tbuf, out,
                                 ctx->pkey->pkey.rsa, RSA_NO_PADDING);
    } else
        ret = RSA_public_encrypt(inlen, in, out, ctx->pkey->pkey.rsa,
                                 rctx->pad_mode);
    if (ret < 0)
        return ret;
    *outlen = ret;
    return 1;
}

static int pkey_rsa_decrypt(EVP_PKEY_CTX *ctx,
                            unsigned char *out, size_t *outlen,
                            const unsigned char *in, size_t inlen)
{
    int ret;
    RSA_PKEY_CTX *rctx = ctx->data;
    if (rctx->pad_mode == RSA_PKCS1_OAEP_PADDING) {
        if (!setup_tbuf(rctx, ctx))
            return -1;
        ret = RSA_private_decrypt(inlen, in, rctx->tbuf,
                                  ctx->pkey->pkey.rsa, RSA_NO_PADDING);
        if (ret <= 0)
            return ret;
        ret = RSA_padding_check_PKCS1_OAEP_mgf1(out, ret, rctx->tbuf,
                                                ret, ret,
                                                rctx->oaep_label,
                                                rctx->oaep_labellen,
                                                rctx->md, rctx->mgf1md);
    } else
        ret = RSA_private_decrypt(inlen, in, out, ctx->pkey->pkey.rsa,
                                  rctx->pad_mode);
    if (ret < 0)
        return ret;
    *outlen = ret;
    return 1;
}

static int check_padding_md(const EVP_MD *md, int padding)
{
    if (!md)
        return 1;

    if (padding == RSA_NO_PADDING) {
        RSAerr(RSA_F_CHECK_PADDING_MD, RSA_R_INVALID_PADDING_MODE);
        return 0;
    }

    if (padding == RSA_X931_PADDING) {
        if (RSA_X931_hash_id(EVP_MD_type(md)) == -1) {
            RSAerr(RSA_F_CHECK_PADDING_MD, RSA_R_INVALID_X931_DIGEST);
            return 0;
        }
        return 1;
    }

    return 1;
}

static int pkey_rsa_ctrl(EVP_PKEY_CTX *ctx, int type, int p1, void *p2)
{
    RSA_PKEY_CTX *rctx = ctx->data;
    switch (type) {
    case EVP_PKEY_CTRL_RSA_PADDING:
        if ((p1 >= RSA_PKCS1_PADDING) && (p1 <= RSA_PKCS1_PSS_PADDING)) {
            if (!check_padding_md(rctx->md, p1))
                return 0;
            if (p1 == RSA_PKCS1_PSS_PADDING) {
                if (!(ctx->operation &
                      (EVP_PKEY_OP_SIGN | EVP_PKEY_OP_VERIFY)))
                    goto bad_pad;
                if (!rctx->md)
                    rctx->md = EVP_sha1();
            }
            if (p1 == RSA_PKCS1_OAEP_PADDING) {
                if (!(ctx->operation & EVP_PKEY_OP_TYPE_CRYPT))
                    goto bad_pad;
                if (!rctx->md)
                    rctx->md = EVP_sha1();
            }
            rctx->pad_mode = p1;
            return 1;
        }
 bad_pad:
        RSAerr(RSA_F_PKEY_RSA_CTRL,
               RSA_R_ILLEGAL_OR_UNSUPPORTED_PADDING_MODE);
        return -2;

    case EVP_PKEY_CTRL_GET_RSA_PADDING:
        *(int *)p2 = rctx->pad_mode;
        return 1;

    case EVP_PKEY_CTRL_RSA_PSS_SALTLEN:
    case EVP_PKEY_CTRL_GET_RSA_PSS_SALTLEN:
        if (rctx->pad_mode != RSA_PKCS1_PSS_PADDING) {
            RSAerr(RSA_F_PKEY_RSA_CTRL, RSA_R_INVALID_PSS_SALTLEN);
            return -2;
        }
        if (type == EVP_PKEY_CTRL_GET_RSA_PSS_SALTLEN)
            *(int *)p2 = rctx->saltlen;
        else {
            if (p1 < -2)
                return -2;
            rctx->saltlen = p1;
        }
        return 1;

    case EVP_PKEY_CTRL_RSA_KEYGEN_BITS:
        if (p1 < 256) {
            RSAerr(RSA_F_PKEY_RSA_CTRL, RSA_R_INVALID_KEYBITS);
            return -2;
        }
        rctx->nbits = p1;
        return 1;

    case EVP_PKEY_CTRL_RSA_KEYGEN_PUBEXP:
        if (p2 == NULL || !BN_is_odd((BIGNUM *)p2) || BN_is_one((BIGNUM *)p2)) {
            RSAerr(RSA_F_PKEY_RSA_CTRL, RSA_R_BAD_E_VALUE);
            return -2;
        }
        BN_free(rctx->pub_exp);
        rctx->pub_exp = p2;
        return 1;

    case EVP_PKEY_CTRL_RSA_OAEP_MD:
    case EVP_PKEY_CTRL_GET_RSA_OAEP_MD:
        if (rctx->pad_mode != RSA_PKCS1_OAEP_PADDING) {
            RSAerr(RSA_F_PKEY_RSA_CTRL, RSA_R_INVALID_PADDING_MODE);
            return -2;
        }
        if (type == EVP_PKEY_CTRL_GET_RSA_OAEP_MD)
            *(const EVP_MD **)p2 = rctx->md;
        else
            rctx->md = p2;
        return 1;

    case EVP_PKEY_CTRL_MD:
        if (!check_padding_md(p2, rctx->pad_mode))
            return 0;
        rctx->md = p2;
        return 1;

    case EVP_PKEY_CTRL_GET_MD:
        *(const EVP_MD **)p2 = rctx->md;
        return 1;

    case EVP_PKEY_CTRL_RSA_MGF1_MD:
    case EVP_PKEY_CTRL_GET_RSA_MGF1_MD:
        if (rctx->pad_mode != RSA_PKCS1_PSS_PADDING
            && rctx->pad_mode != RSA_PKCS1_OAEP_PADDING) {
            RSAerr(RSA_F_PKEY_RSA_CTRL, RSA_R_INVALID_MGF1_MD);
            return -2;
        }
        if (type == EVP_PKEY_CTRL_GET_RSA_MGF1_MD) {
            if (rctx->mgf1md)
                *(const EVP_MD **)p2 = rctx->mgf1md;
            else
                *(const EVP_MD **)p2 = rctx->md;
        } else
            rctx->mgf1md = p2;
        return 1;

    case EVP_PKEY_CTRL_RSA_OAEP_LABEL:
        if (rctx->pad_mode != RSA_PKCS1_OAEP_PADDING) {
            RSAerr(RSA_F_PKEY_RSA_CTRL, RSA_R_INVALID_PADDING_MODE);
            return -2;
        }
        if (rctx->oaep_label)
            OPENSSL_free(rctx->oaep_label);
        if (p2 && p1 > 0) {
            rctx->oaep_label = p2;
            rctx->oaep_labellen = p1;
        } else {
            rctx->oaep_label = NULL;
            rctx->oaep_labellen = 0;
        }
        return 1;

    case EVP_PKEY_CTRL_GET_RSA_OAEP_LABEL:
        if (rctx->pad_mode != RSA_PKCS1_OAEP_PADDING) {
            RSAerr(RSA_F_PKEY_RSA_CTRL, RSA_R_INVALID_PADDING_MODE);
            return -2;
        }
        *(unsigned char **)p2 = rctx->oaep_label;
        return rctx->oaep_labellen;

    case EVP_PKEY_CTRL_DIGESTINIT:
    case EVP_PKEY_CTRL_PKCS7_ENCRYPT:
    case EVP_PKEY_CTRL_PKCS7_DECRYPT:
    case EVP_PKEY_CTRL_PKCS7_SIGN:
        return 1;
#ifndef OPENSSL_NO_CMS
    case EVP_PKEY_CTRL_CMS_DECRYPT:
    case EVP_PKEY_CTRL_CMS_ENCRYPT:
    case EVP_PKEY_CTRL_CMS_SIGN:
        return 1;
#endif
    case EVP_PKEY_CTRL_PEER_KEY:
        RSAerr(RSA_F_PKEY_RSA_CTRL,
               RSA_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
        return -2;

    default:
        return -2;

    }
}

static int pkey_rsa_ctrl_str(EVP_PKEY_CTX *ctx,
                             const char *type, const char *value)
{
    if (!value) {
        RSAerr(RSA_F_PKEY_RSA_CTRL_STR, RSA_R_VALUE_MISSING);
        return 0;
    }
    if (!strcmp(type, "rsa_padding_mode")) {
        int pm;
        if (!strcmp(value, "pkcs1"))
            pm = RSA_PKCS1_PADDING;
        else if (!strcmp(value, "sslv23"))
            pm = RSA_SSLV23_PADDING;
        else if (!strcmp(value, "none"))
            pm = RSA_NO_PADDING;
        else if (!strcmp(value, "oeap"))
            pm = RSA_PKCS1_OAEP_PADDING;
        else if (!strcmp(value, "oaep"))
            pm = RSA_PKCS1_OAEP_PADDING;
        else if (!strcmp(value, "x931"))
            pm = RSA_X931_PADDING;
        else if (!strcmp(value, "pss"))
            pm = RSA_PKCS1_PSS_PADDING;
        else {
            RSAerr(RSA_F_PKEY_RSA_CTRL_STR, RSA_R_UNKNOWN_PADDING_TYPE);
            return -2;
        }
        return EVP_PKEY_CTX_set_rsa_padding(ctx, pm);
    }

    if (!strcmp(type, "rsa_pss_saltlen")) {
        int saltlen;
        saltlen = atoi(value);
        return EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, saltlen);
    }

    if (!strcmp(type, "rsa_keygen_bits")) {
        int nbits;
        nbits = atoi(value);
        return EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, nbits);
    }

    if (!strcmp(type, "rsa_keygen_pubexp")) {
        int ret;
        BIGNUM *pubexp = NULL;
        if (!BN_asc2bn(&pubexp, value))
            return 0;
        ret = EVP_PKEY_CTX_set_rsa_keygen_pubexp(ctx, pubexp);
        if (ret <= 0)
            BN_free(pubexp);
        return ret;
    }

    if (!strcmp(type, "rsa_mgf1_md")) {
        const EVP_MD *md;
        if (!(md = EVP_get_digestbyname(value))) {
            RSAerr(RSA_F_PKEY_RSA_CTRL_STR, RSA_R_INVALID_DIGEST);
            return 0;
        }
        return EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, md);
    }

    if (!strcmp(type, "rsa_oaep_md")) {
        const EVP_MD *md;
        if (!(md = EVP_get_digestbyname(value))) {
            RSAerr(RSA_F_PKEY_RSA_CTRL_STR, RSA_R_INVALID_DIGEST);
            return 0;
        }
        return EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md);
    }
    if (!strcmp(type, "rsa_oaep_label")) {
        unsigned char *lab;
        long lablen;
        int ret;
        lab = string_to_hex(value, &lablen);
        if (!lab)
            return 0;
        ret = EVP_PKEY_CTX_set0_rsa_oaep_label(ctx, lab, lablen);
        if (ret <= 0)
            OPENSSL_free(lab);
        return ret;
    }

    return -2;
}

static int pkey_rsa_keygen(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey)
{
    RSA *rsa = NULL;
    RSA_PKEY_CTX *rctx = ctx->data;
    BN_GENCB *pcb, cb;
    int ret;
    if (!rctx->pub_exp) {
        rctx->pub_exp = BN_new();
        if (!rctx->pub_exp || !BN_set_word(rctx->pub_exp, RSA_F4))
            return 0;
    }
    rsa = RSA_new();
    if (!rsa)
        return 0;
    if (ctx->pkey_gencb) {
        pcb = &cb;
        evp_pkey_set_cb_translate(pcb, ctx);
    } else
        pcb = NULL;
    ret = RSA_generate_key_ex(rsa, rctx->nbits, rctx->pub_exp, pcb);
    if (ret > 0)
        EVP_PKEY_assign_RSA(pkey, rsa);
    else
        RSA_free(rsa);
    return ret;
}

const EVP_PKEY_METHOD rsa_pkey_meth = {
    EVP_PKEY_RSA,
    EVP_PKEY_FLAG_AUTOARGLEN,
    pkey_rsa_init,
    pkey_rsa_copy,
    pkey_rsa_cleanup,

    0, 0,

    0,
    pkey_rsa_keygen,

    0,
    pkey_rsa_sign,

    0,
    pkey_rsa_verify,

    0,
    pkey_rsa_verifyrecover,

    0, 0, 0, 0,

    0,
    pkey_rsa_encrypt,

    0,
    pkey_rsa_decrypt,

    0, 0,

    pkey_rsa_ctrl,
    pkey_rsa_ctrl_str
};

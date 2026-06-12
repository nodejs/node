/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/cms.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/x509.h>
#include "cms_local.h"
#include "crypto/evp.h"
#include "internal/sizes.h"

/* KEM Recipient Info (KEMRI) routines */

int ossl_cms_RecipientInfo_kemri_get0_alg(CMS_RecipientInfo *ri,
                                          uint32_t **pkekLength,
                                          X509_ALGOR **pwrap)
{
    if (ri->type != CMS_RECIPINFO_KEM) {
        ERR_raise(ERR_LIB_CMS, CMS_R_NOT_KEM);
        return 0;
    }
    if (pkekLength)
        *pkekLength = &ri->d.ori->d.kemri->kekLength;
    if (pwrap)
        *pwrap = ri->d.ori->d.kemri->wrap;
    return 1;
}

int CMS_RecipientInfo_kemri_cert_cmp(CMS_RecipientInfo *ri, X509 *cert)
{
    if (ri->type != CMS_RECIPINFO_KEM) {
        ERR_raise(ERR_LIB_CMS, CMS_R_NOT_KEM);
        return -2;
    }
    return ossl_cms_SignerIdentifier_cert_cmp(ri->d.ori->d.kemri->rid, cert);
}

int CMS_RecipientInfo_kemri_set0_pkey(CMS_RecipientInfo *ri, EVP_PKEY *pk)
{
    EVP_PKEY_CTX *pctx = NULL;
    CMS_KEMRecipientInfo *kemri;

    if (ri->type != CMS_RECIPINFO_KEM) {
        ERR_raise(ERR_LIB_CMS, CMS_R_NOT_KEM);
        return 0;
    }

    kemri = ri->d.ori->d.kemri;

    EVP_PKEY_CTX_free(kemri->pctx);
    kemri->pctx = NULL;

    if (pk != NULL) {
        pctx = EVP_PKEY_CTX_new_from_pkey(ossl_cms_ctx_get0_libctx(kemri->cms_ctx), pk,
                                          ossl_cms_ctx_get0_propq(kemri->cms_ctx));
        if (pctx == NULL || EVP_PKEY_decapsulate_init(pctx, NULL) <= 0)
            goto err;

        kemri->pctx = pctx;
    }

    return 1;
err:
    EVP_PKEY_CTX_free(pctx);
    return 0;
}

/* Initialise a kemri based on passed certificate and key */

int ossl_cms_RecipientInfo_kemri_init(CMS_RecipientInfo *ri, X509 *recip,
                                      EVP_PKEY *recipPubKey, unsigned int flags,
                                      const CMS_CTX *ctx)
{
    CMS_OtherRecipientInfo *ori;
    CMS_KEMRecipientInfo *kemri;
    int idtype;
    X509_PUBKEY *x_pubkey;
    X509_ALGOR *x_alg;

    ri->d.ori = M_ASN1_new_of(CMS_OtherRecipientInfo);
    if (ri->d.ori == NULL)
        return 0;
    ri->encoded_type = CMS_RECIPINFO_OTHER;
    ri->type = CMS_RECIPINFO_KEM;

    ori = ri->d.ori;
    ori->oriType = OBJ_nid2obj(NID_id_smime_ori_kem);
    if (ori->oriType == NULL)
        return 0;
    ori->d.kemri = M_ASN1_new_of(CMS_KEMRecipientInfo);
    if (ori->d.kemri == NULL)
        return 0;

    kemri = ori->d.kemri;
    kemri->version = 0;
    kemri->cms_ctx = ctx;

    /*
     * Not a typo: RecipientIdentifier and SignerIdentifier are the same
     * structure.
     */

    idtype = (flags & CMS_USE_KEYID) ? CMS_RECIPINFO_KEYIDENTIFIER : CMS_RECIPINFO_ISSUER_SERIAL;
    if (!ossl_cms_set1_SignerIdentifier(kemri->rid, recip, idtype, ctx))
        return 0;

    x_pubkey = X509_get_X509_PUBKEY(recip);
    if (x_pubkey == NULL)
        return 0;
    if (!X509_PUBKEY_get0_param(NULL, NULL, NULL, &x_alg, x_pubkey))
        return 0;
    if (!X509_ALGOR_copy(kemri->kem, x_alg))
        return 0;

    kemri->pctx = EVP_PKEY_CTX_new_from_pkey(ossl_cms_ctx_get0_libctx(ctx),
                                             recipPubKey,
                                             ossl_cms_ctx_get0_propq(ctx));
    if (kemri->pctx == NULL)
        return 0;
    if (EVP_PKEY_encapsulate_init(kemri->pctx, NULL) <= 0)
        return 0;

    return 1;
}

EVP_CIPHER_CTX *CMS_RecipientInfo_kemri_get0_ctx(CMS_RecipientInfo *ri)
{
    if (ri->type == CMS_RECIPINFO_KEM)
        return ri->d.ori->d.kemri->ctx;
    return NULL;
}

X509_ALGOR *CMS_RecipientInfo_kemri_get0_kdf_alg(CMS_RecipientInfo *ri)
{
    if (ri->type == CMS_RECIPINFO_KEM)
        return ri->d.ori->d.kemri->kdf;
    return NULL;
}

int CMS_RecipientInfo_kemri_set_ukm(CMS_RecipientInfo *ri,
                                    const unsigned char *ukm,
                                    int ukmLength)
{
    CMS_KEMRecipientInfo *kemri;
    ASN1_OCTET_STRING *ukm_str;

    if (ri->type != CMS_RECIPINFO_KEM) {
        ERR_raise(ERR_LIB_CMS, CMS_R_NOT_KEM);
        return 0;
    }

    if (ukm == NULL && ukmLength != 0) {
        ERR_raise(ERR_LIB_CMS, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }

    kemri = ri->d.ori->d.kemri;

    ukm_str = ASN1_OCTET_STRING_new();
    if (ukm_str == NULL)
        return 0;
    if (!ASN1_OCTET_STRING_set(ukm_str, ukm, ukmLength)) {
        ASN1_OCTET_STRING_free(ukm_str);
        return 0;
    }
    ASN1_OCTET_STRING_free(kemri->ukm);
    kemri->ukm = ukm_str;
    return 1;
}

static EVP_KDF_CTX *create_kdf_ctx(CMS_KEMRecipientInfo *kemri)
{
    const ASN1_OBJECT *kdf_oid;
    int ptype;
    char kdf_alg[OSSL_MAX_NAME_SIZE];
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;

    /*
     * KDFs with algorithm identifier parameters are not supported yet. To
     * support this, EVP_KDF_CTX_set_algor_params from
     * `doc/designs/passing-algorithmidentifier-parameters.md` needs to be
     * implemented.
     */
    X509_ALGOR_get0(&kdf_oid, &ptype, NULL, kemri->kdf);
    if (ptype != V_ASN1_UNDEF && ptype != V_ASN1_NULL) {
        ERR_raise(ERR_LIB_CMS, CMS_R_UNSUPPORTED_KDF_ALGORITHM);
        goto err;
    }
    if (OBJ_obj2txt(kdf_alg, sizeof(kdf_alg), kdf_oid, 1) < 0)
        goto err;

    kdf = EVP_KDF_fetch(ossl_cms_ctx_get0_libctx(kemri->cms_ctx), kdf_alg,
                        ossl_cms_ctx_get0_propq(kemri->cms_ctx));
    if (kdf == NULL)
        goto err;

    kctx = EVP_KDF_CTX_new(kdf);
err:
    EVP_KDF_free(kdf);
    return kctx;
}

static int kdf_derive(unsigned char *kek, size_t keklen,
                      const unsigned char *ss, size_t sslen,
                      CMS_KEMRecipientInfo *kemri)
{
    EVP_KDF_CTX *kctx = NULL;
    OSSL_PARAM params[3];
    unsigned char *infoder = NULL;
    int infolen = 0;
    int rv = 0;

    infolen = CMS_CMSORIforKEMOtherInfo_encode(&infoder, kemri->wrap, kemri->ukm,
                                               kemri->kekLength);
    if (infolen <= 0)
        goto err;

    kctx = create_kdf_ctx(kemri);
    if (kctx == NULL)
        goto err;

    params[0] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  (unsigned char *)ss, sslen);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  (char *)infoder, infolen);
    params[2] = OSSL_PARAM_construct_end();

    if (EVP_KDF_derive(kctx, kek, keklen, params) <= 0)
        goto err;

    rv = 1;
err:
    OPENSSL_free(infoder);
    EVP_KDF_CTX_free(kctx);

    return rv;
}

/*
 * Derive KEK and decrypt/encrypt with it to produce either the original CEK
 * or the encrypted CEK.
 */

static int cms_kek_cipher(unsigned char **pout, size_t *poutlen,
                          const unsigned char *ss, size_t sslen,
                          const unsigned char *in, size_t inlen,
                          CMS_KEMRecipientInfo *kemri, int enc)
{
    /* Key encryption key */
    unsigned char kek[EVP_MAX_KEY_LENGTH];
    size_t keklen = kemri->kekLength;
    unsigned char *out = NULL;
    int outlen = 0;
    int rv = 0;

    if (keklen > sizeof(kek)) {
        ERR_raise(ERR_LIB_CMS, CMS_R_INVALID_KEY_LENGTH);
        return 0;
    }

    if (!kdf_derive(kek, keklen, ss, sslen, kemri))
        goto err;

    /* Set KEK in context */
    if (!EVP_CipherInit_ex(kemri->ctx, NULL, NULL, kek, NULL, enc))
        goto err;
    /* obtain output length of ciphered key */
    if (!EVP_CipherUpdate(kemri->ctx, NULL, &outlen, in, (int)inlen))
        goto err;
    out = OPENSSL_malloc(outlen);
    if (out == NULL)
        goto err;
    if (!EVP_CipherUpdate(kemri->ctx, out, &outlen, in, (int)inlen))
        goto err;
    *pout = out;
    out = NULL;
    *poutlen = (size_t)outlen;

    rv = 1;
err:
    OPENSSL_free(out);
    OPENSSL_cleanse(kek, sizeof(kek));
    EVP_CIPHER_CTX_reset(kemri->ctx);
    EVP_PKEY_CTX_free(kemri->pctx);
    kemri->pctx = NULL;
    return rv;
}

/* Encrypt content key in KEM recipient info */

int ossl_cms_RecipientInfo_kemri_encrypt(const CMS_ContentInfo *cms,
                                         CMS_RecipientInfo *ri)
{
    CMS_KEMRecipientInfo *kemri;
    CMS_EncryptedContentInfo *ec;
    unsigned char *kem_ct = NULL;
    size_t kem_ct_len;
    unsigned char *kem_secret = NULL;
    size_t kem_secret_len = 0;
    unsigned char *enckey;
    size_t enckeylen;
    int rv = 0;

    if (ri->type != CMS_RECIPINFO_KEM) {
        ERR_raise(ERR_LIB_CMS, CMS_R_NOT_KEM);
        return 0;
    }

    kemri = ri->d.ori->d.kemri;

    ec = ossl_cms_get0_env_enc_content(cms);
    /* Initialise wrap algorithm parameters */
    if (!ossl_cms_RecipientInfo_wrap_init(ri, ec->cipher))
        return 0;

    /* Initialise KDF algorithm */
    if (!ossl_cms_env_asn1_ctrl(ri, 0))
        return 0;

    if (EVP_PKEY_encapsulate(kemri->pctx, NULL, &kem_ct_len, NULL, &kem_secret_len) <= 0)
        return 0;
    kem_ct = OPENSSL_malloc(kem_ct_len);
    kem_secret = OPENSSL_malloc(kem_secret_len);
    if (kem_ct == NULL || kem_secret == NULL)
        goto err;

    if (EVP_PKEY_encapsulate(kemri->pctx, kem_ct, &kem_ct_len, kem_secret, &kem_secret_len) <= 0)
        goto err;

    ASN1_STRING_set0(kemri->kemct, kem_ct, (int)kem_ct_len);
    kem_ct = NULL;

    if (!cms_kek_cipher(&enckey, &enckeylen, kem_secret, kem_secret_len, ec->key, ec->keylen,
                        kemri, 1))
        goto err;
    ASN1_STRING_set0(kemri->encryptedKey, enckey, (int)enckeylen);

    rv = 1;
err:
    OPENSSL_free(kem_ct);
    OPENSSL_clear_free(kem_secret, kem_secret_len);
    return rv;
}

int ossl_cms_RecipientInfo_kemri_decrypt(const CMS_ContentInfo *cms,
                                         CMS_RecipientInfo *ri)
{
    CMS_KEMRecipientInfo *kemri;
    CMS_EncryptedContentInfo *ec;
    const unsigned char *kem_ct = NULL;
    size_t kem_ct_len;
    unsigned char *kem_secret = NULL;
    size_t kem_secret_len = 0;
    unsigned char *enckey = NULL;
    size_t enckeylen;
    unsigned char *cek = NULL;
    size_t ceklen;
    int ret = 0;

    if (ri->type != CMS_RECIPINFO_KEM) {
        ERR_raise(ERR_LIB_CMS, CMS_R_NOT_KEM);
        return 0;
    }

    kemri = ri->d.ori->d.kemri;

    ec = ossl_cms_get0_env_enc_content(cms);

    if (kemri->pctx == NULL) {
        ERR_raise(ERR_LIB_CMS, CMS_R_NO_PRIVATE_KEY);
        return 0;
    }

    /* Setup all parameters to derive KEK */
    if (!ossl_cms_env_asn1_ctrl(ri, 1))
        goto err;

    kem_ct = ASN1_STRING_get0_data(kemri->kemct);
    kem_ct_len = ASN1_STRING_length(kemri->kemct);

    if (EVP_PKEY_decapsulate(kemri->pctx, NULL, &kem_secret_len, kem_ct, kem_ct_len) <= 0)
        return 0;
    kem_secret = OPENSSL_malloc(kem_secret_len);
    if (kem_secret == NULL)
        goto err;

    if (EVP_PKEY_decapsulate(kemri->pctx, kem_secret, &kem_secret_len, kem_ct, kem_ct_len) <= 0)
        goto err;

    /* Attempt to decrypt CEK */
    enckeylen = kemri->encryptedKey->length;
    enckey = kemri->encryptedKey->data;
    if (!cms_kek_cipher(&cek, &ceklen, kem_secret, kem_secret_len, enckey, enckeylen, kemri, 0))
        goto err;
    ec = ossl_cms_get0_env_enc_content(cms);
    OPENSSL_clear_free(ec->key, ec->keylen);
    ec->key = cek;
    ec->keylen = ceklen;

    ret = 1;
err:
    OPENSSL_clear_free(kem_secret, kem_secret_len);
    return ret;
}

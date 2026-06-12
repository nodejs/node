/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <assert.h>
#include <limits.h>
#include <openssl/cms.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/decoder.h>
#include "internal/sizes.h"
#include "crypto/asn1.h"
#include "crypto/evp.h"
#include "cms_local.h"

static int kem_cms_decrypt(CMS_RecipientInfo *ri)
{
    uint32_t *kekLength;
    X509_ALGOR *wrap;
    EVP_PKEY_CTX *pctx;
    EVP_CIPHER_CTX *kekctx;
    uint32_t cipher_length;
    char name[OSSL_MAX_NAME_SIZE];
    EVP_CIPHER *kekcipher = NULL;
    int rv = 0;

    if (!ossl_cms_RecipientInfo_kemri_get0_alg(ri, &kekLength, &wrap))
        goto err;

    pctx = CMS_RecipientInfo_get0_pkey_ctx(ri);
    if (pctx == NULL)
        goto err;

    kekctx = CMS_RecipientInfo_kemri_get0_ctx(ri);
    if (kekctx == NULL)
        goto err;

    OBJ_obj2txt(name, sizeof(name), wrap->algorithm, 0);
    kekcipher = EVP_CIPHER_fetch(pctx->libctx, name, pctx->propquery);
    if (kekcipher == NULL || EVP_CIPHER_get_mode(kekcipher) != EVP_CIPH_WRAP_MODE)
        goto err;
    if (!EVP_EncryptInit_ex(kekctx, kekcipher, NULL, NULL, NULL))
        goto err;
    if (EVP_CIPHER_asn1_to_param(kekctx, wrap->parameter) <= 0)
        goto err;

    cipher_length = EVP_CIPHER_CTX_get_key_length(kekctx);
    if (cipher_length != *kekLength) {
        ERR_raise(ERR_LIB_CMS, CMS_R_INVALID_KEY_LENGTH);
        goto err;
    }

    rv = 1;
err:
    EVP_CIPHER_free(kekcipher);
    return rv;
}

static int kem_cms_encrypt(CMS_RecipientInfo *ri)
{
    uint32_t *kekLength;
    X509_ALGOR *wrap;
    X509_ALGOR *kdf;
    EVP_PKEY_CTX *pctx;
    EVP_PKEY *pkey;
    int security_bits;
    const ASN1_OBJECT *kdf_obj = NULL;
    unsigned char kemri_x509_algor[OSSL_MAX_ALGORITHM_ID_SIZE];
    OSSL_PARAM params[2];
    X509_ALGOR *x509_algor = NULL;
    EVP_CIPHER_CTX *kekctx;
    int wrap_nid;
    int rv = 0;

    if (!ossl_cms_RecipientInfo_kemri_get0_alg(ri, &kekLength, &wrap))
        goto err;

    kdf = CMS_RecipientInfo_kemri_get0_kdf_alg(ri);
    if (kdf == NULL)
        goto err;

    pctx = CMS_RecipientInfo_get0_pkey_ctx(ri);
    if (pctx == NULL)
        goto err;

    pkey = EVP_PKEY_CTX_get0_pkey(pctx);
    if (pkey == NULL)
        goto err;

    security_bits = EVP_PKEY_get_security_bits(pkey);
    if (security_bits == 0)
        goto err;

    X509_ALGOR_get0(&kdf_obj, NULL, NULL, kdf);
    if (kdf_obj == NULL || OBJ_obj2nid(kdf_obj) == NID_undef) {
        /*
         * If the KDF OID hasn't already been set, then query the provider
         * for a default KDF.
         */
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_CMS_KEMRI_KDF_ALGORITHM,
                                                      kemri_x509_algor, sizeof(kemri_x509_algor));
        params[1] = OSSL_PARAM_construct_end();
        if (!EVP_PKEY_get_params(pkey, params))
            goto err;
        if (OSSL_PARAM_modified(&params[0])) {
            const unsigned char *p = kemri_x509_algor;

            x509_algor = d2i_X509_ALGOR(NULL, &p, (long)params[0].return_size);
            if (x509_algor == NULL)
                goto err;
            if (!X509_ALGOR_copy(kdf, x509_algor))
                goto err;
        } else {
            if (!X509_ALGOR_set0(kdf, OBJ_nid2obj(NID_HKDF_SHA256), V_ASN1_UNDEF, NULL))
                return 0;
        }
    }

    /* Get wrap NID */
    kekctx = CMS_RecipientInfo_kemri_get0_ctx(ri);
    if (kekctx == NULL)
        goto err;
    *kekLength = EVP_CIPHER_CTX_get_key_length(kekctx);
    wrap_nid = EVP_CIPHER_CTX_get_type(kekctx);

    /* Package wrap algorithm in an AlgorithmIdentifier */
    ASN1_OBJECT_free(wrap->algorithm);
    ASN1_TYPE_free(wrap->parameter);
    wrap->algorithm = OBJ_nid2obj(wrap_nid);
    wrap->parameter = ASN1_TYPE_new();
    if (wrap->parameter == NULL)
        goto err;
    if (EVP_CIPHER_param_to_asn1(kekctx, wrap->parameter) <= 0) {
        ASN1_TYPE_free(wrap->parameter);
        wrap->parameter = NULL;
        goto err;
    }
    if (ASN1_TYPE_get(wrap->parameter) == NID_undef) {
        ASN1_TYPE_free(wrap->parameter);
        wrap->parameter = NULL;
    }

    rv = 1;
err:
    X509_ALGOR_free(x509_algor);
    return rv;
}

int ossl_cms_kem_envelope(CMS_RecipientInfo *ri, int decrypt)
{
    assert(decrypt == 0 || decrypt == 1);

    if (decrypt == 1)
        return kem_cms_decrypt(ri);

    if (decrypt == 0)
        return kem_cms_encrypt(ri);

    ERR_raise(ERR_LIB_CMS, CMS_R_NOT_SUPPORTED_FOR_THIS_KEY_TYPE);
    return 0;
}

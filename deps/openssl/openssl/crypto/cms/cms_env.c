/* crypto/cms/cms_env.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
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
 */

#include "cryptlib.h"
#include <openssl/asn1t.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/cms.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include "cms_lcl.h"
#include "asn1_locl.h"

/* CMS EnvelopedData Utilities */

DECLARE_ASN1_ITEM(CMS_EnvelopedData)
DECLARE_ASN1_ITEM(CMS_KeyTransRecipientInfo)
DECLARE_ASN1_ITEM(CMS_KEKRecipientInfo)
DECLARE_ASN1_ITEM(CMS_OtherKeyAttribute)

DECLARE_STACK_OF(CMS_RecipientInfo)

CMS_EnvelopedData *cms_get0_enveloped(CMS_ContentInfo *cms)
{
    if (OBJ_obj2nid(cms->contentType) != NID_pkcs7_enveloped) {
        CMSerr(CMS_F_CMS_GET0_ENVELOPED,
               CMS_R_CONTENT_TYPE_NOT_ENVELOPED_DATA);
        return NULL;
    }
    return cms->d.envelopedData;
}

static CMS_EnvelopedData *cms_enveloped_data_init(CMS_ContentInfo *cms)
{
    if (cms->d.other == NULL) {
        cms->d.envelopedData = M_ASN1_new_of(CMS_EnvelopedData);
        if (!cms->d.envelopedData) {
            CMSerr(CMS_F_CMS_ENVELOPED_DATA_INIT, ERR_R_MALLOC_FAILURE);
            return NULL;
        }
        cms->d.envelopedData->version = 0;
        cms->d.envelopedData->encryptedContentInfo->contentType =
            OBJ_nid2obj(NID_pkcs7_data);
        ASN1_OBJECT_free(cms->contentType);
        cms->contentType = OBJ_nid2obj(NID_pkcs7_enveloped);
        return cms->d.envelopedData;
    }
    return cms_get0_enveloped(cms);
}

STACK_OF(CMS_RecipientInfo) *CMS_get0_RecipientInfos(CMS_ContentInfo *cms)
{
    CMS_EnvelopedData *env;
    env = cms_get0_enveloped(cms);
    if (!env)
        return NULL;
    return env->recipientInfos;
}

int CMS_RecipientInfo_type(CMS_RecipientInfo *ri)
{
    return ri->type;
}

CMS_ContentInfo *CMS_EnvelopedData_create(const EVP_CIPHER *cipher)
{
    CMS_ContentInfo *cms;
    CMS_EnvelopedData *env;
    cms = CMS_ContentInfo_new();
    if (!cms)
        goto merr;
    env = cms_enveloped_data_init(cms);
    if (!env)
        goto merr;
    if (!cms_EncryptedContent_init(env->encryptedContentInfo,
                                   cipher, NULL, 0))
        goto merr;
    return cms;
 merr:
    if (cms)
        CMS_ContentInfo_free(cms);
    CMSerr(CMS_F_CMS_ENVELOPEDDATA_CREATE, ERR_R_MALLOC_FAILURE);
    return NULL;
}

/* Key Transport Recipient Info (KTRI) routines */

/*
 * Add a recipient certificate. For now only handle key transport. If we ever
 * handle key agreement will need updating.
 */

CMS_RecipientInfo *CMS_add1_recipient_cert(CMS_ContentInfo *cms,
                                           X509 *recip, unsigned int flags)
{
    CMS_RecipientInfo *ri = NULL;
    CMS_KeyTransRecipientInfo *ktri;
    CMS_EnvelopedData *env;
    EVP_PKEY *pk = NULL;
    int i, type;
    env = cms_get0_enveloped(cms);
    if (!env)
        goto err;

    /* Initialize recipient info */
    ri = M_ASN1_new_of(CMS_RecipientInfo);
    if (!ri)
        goto merr;

    /* Initialize and add key transport recipient info */

    ri->d.ktri = M_ASN1_new_of(CMS_KeyTransRecipientInfo);
    if (!ri->d.ktri)
        goto merr;
    ri->type = CMS_RECIPINFO_TRANS;

    ktri = ri->d.ktri;

    X509_check_purpose(recip, -1, -1);
    pk = X509_get_pubkey(recip);
    if (!pk) {
        CMSerr(CMS_F_CMS_ADD1_RECIPIENT_CERT, CMS_R_ERROR_GETTING_PUBLIC_KEY);
        goto err;
    }
    CRYPTO_add(&recip->references, 1, CRYPTO_LOCK_X509);
    ktri->pkey = pk;
    ktri->recip = recip;

    if (flags & CMS_USE_KEYID) {
        ktri->version = 2;
        if (env->version < 2)
            env->version = 2;
        type = CMS_RECIPINFO_KEYIDENTIFIER;
    } else {
        ktri->version = 0;
        type = CMS_RECIPINFO_ISSUER_SERIAL;
    }

    /*
     * Not a typo: RecipientIdentifier and SignerIdentifier are the same
     * structure.
     */

    if (!cms_set1_SignerIdentifier(ktri->rid, recip, type))
        goto err;

    if (pk->ameth && pk->ameth->pkey_ctrl) {
        i = pk->ameth->pkey_ctrl(pk, ASN1_PKEY_CTRL_CMS_ENVELOPE, 0, ri);
        if (i == -2) {
            CMSerr(CMS_F_CMS_ADD1_RECIPIENT_CERT,
                   CMS_R_NOT_SUPPORTED_FOR_THIS_KEY_TYPE);
            goto err;
        }
        if (i <= 0) {
            CMSerr(CMS_F_CMS_ADD1_RECIPIENT_CERT, CMS_R_CTRL_FAILURE);
            goto err;
        }
    }

    if (!sk_CMS_RecipientInfo_push(env->recipientInfos, ri))
        goto merr;

    return ri;

 merr:
    CMSerr(CMS_F_CMS_ADD1_RECIPIENT_CERT, ERR_R_MALLOC_FAILURE);
 err:
    if (ri)
        M_ASN1_free_of(ri, CMS_RecipientInfo);
    return NULL;

}

int CMS_RecipientInfo_ktri_get0_algs(CMS_RecipientInfo *ri,
                                     EVP_PKEY **pk, X509 **recip,
                                     X509_ALGOR **palg)
{
    CMS_KeyTransRecipientInfo *ktri;
    if (ri->type != CMS_RECIPINFO_TRANS) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_GET0_ALGS,
               CMS_R_NOT_KEY_TRANSPORT);
        return 0;
    }

    ktri = ri->d.ktri;

    if (pk)
        *pk = ktri->pkey;
    if (recip)
        *recip = ktri->recip;
    if (palg)
        *palg = ktri->keyEncryptionAlgorithm;
    return 1;
}

int CMS_RecipientInfo_ktri_get0_signer_id(CMS_RecipientInfo *ri,
                                          ASN1_OCTET_STRING **keyid,
                                          X509_NAME **issuer,
                                          ASN1_INTEGER **sno)
{
    CMS_KeyTransRecipientInfo *ktri;
    if (ri->type != CMS_RECIPINFO_TRANS) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_GET0_SIGNER_ID,
               CMS_R_NOT_KEY_TRANSPORT);
        return 0;
    }
    ktri = ri->d.ktri;

    return cms_SignerIdentifier_get0_signer_id(ktri->rid, keyid, issuer, sno);
}

int CMS_RecipientInfo_ktri_cert_cmp(CMS_RecipientInfo *ri, X509 *cert)
{
    if (ri->type != CMS_RECIPINFO_TRANS) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_CERT_CMP,
               CMS_R_NOT_KEY_TRANSPORT);
        return -2;
    }
    return cms_SignerIdentifier_cert_cmp(ri->d.ktri->rid, cert);
}

int CMS_RecipientInfo_set0_pkey(CMS_RecipientInfo *ri, EVP_PKEY *pkey)
{
    if (ri->type != CMS_RECIPINFO_TRANS) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_SET0_PKEY, CMS_R_NOT_KEY_TRANSPORT);
        return 0;
    }
    ri->d.ktri->pkey = pkey;
    return 1;
}

/* Encrypt content key in key transport recipient info */

static int cms_RecipientInfo_ktri_encrypt(CMS_ContentInfo *cms,
                                          CMS_RecipientInfo *ri)
{
    CMS_KeyTransRecipientInfo *ktri;
    CMS_EncryptedContentInfo *ec;
    EVP_PKEY_CTX *pctx = NULL;
    unsigned char *ek = NULL;
    size_t eklen;

    int ret = 0;

    if (ri->type != CMS_RECIPINFO_TRANS) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_ENCRYPT, CMS_R_NOT_KEY_TRANSPORT);
        return 0;
    }
    ktri = ri->d.ktri;
    ec = cms->d.envelopedData->encryptedContentInfo;

    pctx = EVP_PKEY_CTX_new(ktri->pkey, NULL);
    if (!pctx)
        return 0;

    if (EVP_PKEY_encrypt_init(pctx) <= 0)
        goto err;

    if (EVP_PKEY_CTX_ctrl(pctx, -1, EVP_PKEY_OP_ENCRYPT,
                          EVP_PKEY_CTRL_CMS_ENCRYPT, 0, ri) <= 0) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_ENCRYPT, CMS_R_CTRL_ERROR);
        goto err;
    }

    if (EVP_PKEY_encrypt(pctx, NULL, &eklen, ec->key, ec->keylen) <= 0)
        goto err;

    ek = OPENSSL_malloc(eklen);

    if (ek == NULL) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (EVP_PKEY_encrypt(pctx, ek, &eklen, ec->key, ec->keylen) <= 0)
        goto err;

    ASN1_STRING_set0(ktri->encryptedKey, ek, eklen);
    ek = NULL;

    ret = 1;

 err:
    if (pctx)
        EVP_PKEY_CTX_free(pctx);
    if (ek)
        OPENSSL_free(ek);
    return ret;

}

/* Decrypt content key from KTRI */

static int cms_RecipientInfo_ktri_decrypt(CMS_ContentInfo *cms,
                                          CMS_RecipientInfo *ri)
{
    CMS_KeyTransRecipientInfo *ktri = ri->d.ktri;
    EVP_PKEY_CTX *pctx = NULL;
    unsigned char *ek = NULL;
    size_t eklen;
    int ret = 0;
    CMS_EncryptedContentInfo *ec;
    ec = cms->d.envelopedData->encryptedContentInfo;

    if (ktri->pkey == NULL) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_DECRYPT, CMS_R_NO_PRIVATE_KEY);
        return 0;
    }

    pctx = EVP_PKEY_CTX_new(ktri->pkey, NULL);
    if (!pctx)
        return 0;

    if (EVP_PKEY_decrypt_init(pctx) <= 0)
        goto err;

    if (EVP_PKEY_CTX_ctrl(pctx, -1, EVP_PKEY_OP_DECRYPT,
                          EVP_PKEY_CTRL_CMS_DECRYPT, 0, ri) <= 0) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_DECRYPT, CMS_R_CTRL_ERROR);
        goto err;
    }

    if (EVP_PKEY_decrypt(pctx, NULL, &eklen,
                         ktri->encryptedKey->data,
                         ktri->encryptedKey->length) <= 0)
        goto err;

    ek = OPENSSL_malloc(eklen);

    if (ek == NULL) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_DECRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (EVP_PKEY_decrypt(pctx, ek, &eklen,
                         ktri->encryptedKey->data,
                         ktri->encryptedKey->length) <= 0) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KTRI_DECRYPT, CMS_R_CMS_LIB);
        goto err;
    }

    ret = 1;

    if (ec->key) {
        OPENSSL_cleanse(ec->key, ec->keylen);
        OPENSSL_free(ec->key);
    }

    ec->key = ek;
    ec->keylen = eklen;

 err:
    if (pctx)
        EVP_PKEY_CTX_free(pctx);
    if (!ret && ek)
        OPENSSL_free(ek);

    return ret;
}

/* Key Encrypted Key (KEK) RecipientInfo routines */

int CMS_RecipientInfo_kekri_id_cmp(CMS_RecipientInfo *ri,
                                   const unsigned char *id, size_t idlen)
{
    ASN1_OCTET_STRING tmp_os;
    CMS_KEKRecipientInfo *kekri;
    if (ri->type != CMS_RECIPINFO_KEK) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_ID_CMP, CMS_R_NOT_KEK);
        return -2;
    }
    kekri = ri->d.kekri;
    tmp_os.type = V_ASN1_OCTET_STRING;
    tmp_os.flags = 0;
    tmp_os.data = (unsigned char *)id;
    tmp_os.length = (int)idlen;
    return ASN1_OCTET_STRING_cmp(&tmp_os, kekri->kekid->keyIdentifier);
}

/* For now hard code AES key wrap info */

static size_t aes_wrap_keylen(int nid)
{
    switch (nid) {
    case NID_id_aes128_wrap:
        return 16;

    case NID_id_aes192_wrap:
        return 24;

    case NID_id_aes256_wrap:
        return 32;

    default:
        return 0;
    }
}

CMS_RecipientInfo *CMS_add0_recipient_key(CMS_ContentInfo *cms, int nid,
                                          unsigned char *key, size_t keylen,
                                          unsigned char *id, size_t idlen,
                                          ASN1_GENERALIZEDTIME *date,
                                          ASN1_OBJECT *otherTypeId,
                                          ASN1_TYPE *otherType)
{
    CMS_RecipientInfo *ri = NULL;
    CMS_EnvelopedData *env;
    CMS_KEKRecipientInfo *kekri;
    env = cms_get0_enveloped(cms);
    if (!env)
        goto err;

    if (nid == NID_undef) {
        switch (keylen) {
        case 16:
            nid = NID_id_aes128_wrap;
            break;

        case 24:
            nid = NID_id_aes192_wrap;
            break;

        case 32:
            nid = NID_id_aes256_wrap;
            break;

        default:
            CMSerr(CMS_F_CMS_ADD0_RECIPIENT_KEY, CMS_R_INVALID_KEY_LENGTH);
            goto err;
        }

    } else {

        size_t exp_keylen = aes_wrap_keylen(nid);

        if (!exp_keylen) {
            CMSerr(CMS_F_CMS_ADD0_RECIPIENT_KEY,
                   CMS_R_UNSUPPORTED_KEK_ALGORITHM);
            goto err;
        }

        if (keylen != exp_keylen) {
            CMSerr(CMS_F_CMS_ADD0_RECIPIENT_KEY, CMS_R_INVALID_KEY_LENGTH);
            goto err;
        }

    }

    /* Initialize recipient info */
    ri = M_ASN1_new_of(CMS_RecipientInfo);
    if (!ri)
        goto merr;

    ri->d.kekri = M_ASN1_new_of(CMS_KEKRecipientInfo);
    if (!ri->d.kekri)
        goto merr;
    ri->type = CMS_RECIPINFO_KEK;

    kekri = ri->d.kekri;

    if (otherTypeId) {
        kekri->kekid->other = M_ASN1_new_of(CMS_OtherKeyAttribute);
        if (kekri->kekid->other == NULL)
            goto merr;
    }

    if (!sk_CMS_RecipientInfo_push(env->recipientInfos, ri))
        goto merr;

    /* After this point no calls can fail */

    kekri->version = 4;

    kekri->key = key;
    kekri->keylen = keylen;

    ASN1_STRING_set0(kekri->kekid->keyIdentifier, id, idlen);

    kekri->kekid->date = date;

    if (kekri->kekid->other) {
        kekri->kekid->other->keyAttrId = otherTypeId;
        kekri->kekid->other->keyAttr = otherType;
    }

    X509_ALGOR_set0(kekri->keyEncryptionAlgorithm,
                    OBJ_nid2obj(nid), V_ASN1_UNDEF, NULL);

    return ri;

 merr:
    CMSerr(CMS_F_CMS_ADD0_RECIPIENT_KEY, ERR_R_MALLOC_FAILURE);
 err:
    if (ri)
        M_ASN1_free_of(ri, CMS_RecipientInfo);
    return NULL;

}

int CMS_RecipientInfo_kekri_get0_id(CMS_RecipientInfo *ri,
                                    X509_ALGOR **palg,
                                    ASN1_OCTET_STRING **pid,
                                    ASN1_GENERALIZEDTIME **pdate,
                                    ASN1_OBJECT **potherid,
                                    ASN1_TYPE **pothertype)
{
    CMS_KEKIdentifier *rkid;
    if (ri->type != CMS_RECIPINFO_KEK) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_GET0_ID, CMS_R_NOT_KEK);
        return 0;
    }
    rkid = ri->d.kekri->kekid;
    if (palg)
        *palg = ri->d.kekri->keyEncryptionAlgorithm;
    if (pid)
        *pid = rkid->keyIdentifier;
    if (pdate)
        *pdate = rkid->date;
    if (potherid) {
        if (rkid->other)
            *potherid = rkid->other->keyAttrId;
        else
            *potherid = NULL;
    }
    if (pothertype) {
        if (rkid->other)
            *pothertype = rkid->other->keyAttr;
        else
            *pothertype = NULL;
    }
    return 1;
}

int CMS_RecipientInfo_set0_key(CMS_RecipientInfo *ri,
                               unsigned char *key, size_t keylen)
{
    CMS_KEKRecipientInfo *kekri;
    if (ri->type != CMS_RECIPINFO_KEK) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_SET0_KEY, CMS_R_NOT_KEK);
        return 0;
    }

    kekri = ri->d.kekri;
    kekri->key = key;
    kekri->keylen = keylen;
    return 1;
}

/* Encrypt content key in KEK recipient info */

static int cms_RecipientInfo_kekri_encrypt(CMS_ContentInfo *cms,
                                           CMS_RecipientInfo *ri)
{
    CMS_EncryptedContentInfo *ec;
    CMS_KEKRecipientInfo *kekri;
    AES_KEY actx;
    unsigned char *wkey = NULL;
    int wkeylen;
    int r = 0;

    ec = cms->d.envelopedData->encryptedContentInfo;

    kekri = ri->d.kekri;

    if (!kekri->key) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_ENCRYPT, CMS_R_NO_KEY);
        return 0;
    }

    if (AES_set_encrypt_key(kekri->key, kekri->keylen << 3, &actx)) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_ENCRYPT,
               CMS_R_ERROR_SETTING_KEY);
        goto err;
    }

    wkey = OPENSSL_malloc(ec->keylen + 8);

    if (!wkey) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    wkeylen = AES_wrap_key(&actx, NULL, wkey, ec->key, ec->keylen);

    if (wkeylen <= 0) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_ENCRYPT, CMS_R_WRAP_ERROR);
        goto err;
    }

    ASN1_STRING_set0(kekri->encryptedKey, wkey, wkeylen);

    r = 1;

 err:

    if (!r && wkey)
        OPENSSL_free(wkey);
    OPENSSL_cleanse(&actx, sizeof(actx));

    return r;

}

/* Decrypt content key in KEK recipient info */

static int cms_RecipientInfo_kekri_decrypt(CMS_ContentInfo *cms,
                                           CMS_RecipientInfo *ri)
{
    CMS_EncryptedContentInfo *ec;
    CMS_KEKRecipientInfo *kekri;
    AES_KEY actx;
    unsigned char *ukey = NULL;
    int ukeylen;
    int r = 0, wrap_nid;

    ec = cms->d.envelopedData->encryptedContentInfo;

    kekri = ri->d.kekri;

    if (!kekri->key) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_DECRYPT, CMS_R_NO_KEY);
        return 0;
    }

    wrap_nid = OBJ_obj2nid(kekri->keyEncryptionAlgorithm->algorithm);
    if (aes_wrap_keylen(wrap_nid) != kekri->keylen) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_DECRYPT,
               CMS_R_INVALID_KEY_LENGTH);
        return 0;
    }

    /* If encrypted key length is invalid don't bother */

    if (kekri->encryptedKey->length < 16) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_DECRYPT,
               CMS_R_INVALID_ENCRYPTED_KEY_LENGTH);
        goto err;
    }

    if (AES_set_decrypt_key(kekri->key, kekri->keylen << 3, &actx)) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_DECRYPT,
               CMS_R_ERROR_SETTING_KEY);
        goto err;
    }

    ukey = OPENSSL_malloc(kekri->encryptedKey->length - 8);

    if (!ukey) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_DECRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    ukeylen = AES_unwrap_key(&actx, NULL, ukey,
                             kekri->encryptedKey->data,
                             kekri->encryptedKey->length);

    if (ukeylen <= 0) {
        CMSerr(CMS_F_CMS_RECIPIENTINFO_KEKRI_DECRYPT, CMS_R_UNWRAP_ERROR);
        goto err;
    }

    ec->key = ukey;
    ec->keylen = ukeylen;

    r = 1;

 err:

    if (!r && ukey)
        OPENSSL_free(ukey);
    OPENSSL_cleanse(&actx, sizeof(actx));

    return r;

}

int CMS_RecipientInfo_decrypt(CMS_ContentInfo *cms, CMS_RecipientInfo *ri)
{
    switch (ri->type) {
    case CMS_RECIPINFO_TRANS:
        return cms_RecipientInfo_ktri_decrypt(cms, ri);

    case CMS_RECIPINFO_KEK:
        return cms_RecipientInfo_kekri_decrypt(cms, ri);

    case CMS_RECIPINFO_PASS:
        return cms_RecipientInfo_pwri_crypt(cms, ri, 0);

    default:
        CMSerr(CMS_F_CMS_RECIPIENTINFO_DECRYPT,
               CMS_R_UNSUPPORTED_RECPIENTINFO_TYPE);
        return 0;
    }
}

BIO *cms_EnvelopedData_init_bio(CMS_ContentInfo *cms)
{
    CMS_EncryptedContentInfo *ec;
    STACK_OF(CMS_RecipientInfo) *rinfos;
    CMS_RecipientInfo *ri;
    int i, r, ok = 0;
    BIO *ret;

    /* Get BIO first to set up key */

    ec = cms->d.envelopedData->encryptedContentInfo;
    ret = cms_EncryptedContent_init_bio(ec);

    /* If error or no cipher end of processing */

    if (!ret || !ec->cipher)
        return ret;

    /* Now encrypt content key according to each RecipientInfo type */

    rinfos = cms->d.envelopedData->recipientInfos;

    for (i = 0; i < sk_CMS_RecipientInfo_num(rinfos); i++) {
        ri = sk_CMS_RecipientInfo_value(rinfos, i);

        switch (ri->type) {
        case CMS_RECIPINFO_TRANS:
            r = cms_RecipientInfo_ktri_encrypt(cms, ri);
            break;

        case CMS_RECIPINFO_KEK:
            r = cms_RecipientInfo_kekri_encrypt(cms, ri);
            break;

        case CMS_RECIPINFO_PASS:
            r = cms_RecipientInfo_pwri_crypt(cms, ri, 1);
            break;

        default:
            CMSerr(CMS_F_CMS_ENVELOPEDDATA_INIT_BIO,
                   CMS_R_UNSUPPORTED_RECIPIENT_TYPE);
            goto err;
        }

        if (r <= 0) {
            CMSerr(CMS_F_CMS_ENVELOPEDDATA_INIT_BIO,
                   CMS_R_ERROR_SETTING_RECIPIENTINFO);
            goto err;
        }
    }

    ok = 1;

 err:
    ec->cipher = NULL;
    if (ec->key) {
        OPENSSL_cleanse(ec->key, ec->keylen);
        OPENSSL_free(ec->key);
        ec->key = NULL;
        ec->keylen = 0;
    }
    if (ok)
        return ret;
    BIO_free(ret);
    return NULL;

}

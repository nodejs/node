/*
 * Copyright 2006-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * ECDSA low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <stdio.h>
#include <openssl/x509.h>
#include <openssl/ec.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/rand.h>
#include "internal/cryptlib.h"
#include "internal/provider.h"
#include "crypto/asn1.h"
#include "crypto/evp.h"
#include "crypto/ecx.h"
#include "ec_local.h"
#include "curve448/curve448_local.h"
#include "ecx_backend.h"

static int ecx_pub_encode(X509_PUBKEY *pk, const EVP_PKEY *pkey)
{
    const ECX_KEY *ecxkey = pkey->pkey.ecx;
    unsigned char *penc;

    if (ecxkey == NULL) {
        ERR_raise(ERR_LIB_EC, EC_R_INVALID_KEY);
        return 0;
    }

    penc = OPENSSL_memdup(ecxkey->pubkey, KEYLEN(pkey));
    if (penc == NULL)
        return 0;

    if (!X509_PUBKEY_set0_param(pk, OBJ_nid2obj(pkey->ameth->pkey_id),
            V_ASN1_UNDEF, NULL, penc, KEYLEN(pkey))) {
        OPENSSL_free(penc);
        ERR_raise(ERR_LIB_EC, ERR_R_X509_LIB);
        return 0;
    }
    return 1;
}

static int ecx_pub_decode(EVP_PKEY *pkey, const X509_PUBKEY *pubkey)
{
    const unsigned char *p;
    int pklen;
    X509_ALGOR *palg;
    ECX_KEY *ecx;
    int ret = 0;

    if (!X509_PUBKEY_get0_param(NULL, &p, &pklen, &palg, pubkey))
        return 0;
    ecx = ossl_ecx_key_op(palg, p, pklen, pkey->ameth->pkey_id,
        KEY_OP_PUBLIC, NULL, NULL);
    if (ecx != NULL) {
        ret = 1;
        EVP_PKEY_assign(pkey, pkey->ameth->pkey_id, ecx);
    }
    return ret;
}

static int ecx_pub_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
{
    const ECX_KEY *akey = a->pkey.ecx;
    const ECX_KEY *bkey = b->pkey.ecx;

    if (akey == NULL || bkey == NULL)
        return -2;

    return CRYPTO_memcmp(akey->pubkey, bkey->pubkey, KEYLEN(a)) == 0;
}

static int ecx_priv_decode_ex(EVP_PKEY *pkey, const PKCS8_PRIV_KEY_INFO *p8,
    OSSL_LIB_CTX *libctx, const char *propq)
{
    int ret = 0;
    ECX_KEY *ecx = ossl_ecx_key_from_pkcs8(p8, libctx, propq);

    if (ecx != NULL) {
        ret = 1;
        EVP_PKEY_assign(pkey, pkey->ameth->pkey_id, ecx);
    }

    return ret;
}

static int ecx_priv_encode(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pkey)
{
    const ECX_KEY *ecxkey = pkey->pkey.ecx;
    ASN1_OCTET_STRING oct;
    unsigned char *penc = NULL;
    int penclen;

    if (ecxkey == NULL || ecxkey->privkey == NULL) {
        ERR_raise(ERR_LIB_EC, EC_R_INVALID_PRIVATE_KEY);
        return 0;
    }

    oct.data = ecxkey->privkey;
    oct.length = KEYLEN(pkey);
    oct.flags = 0;

    penclen = i2d_ASN1_OCTET_STRING(&oct, &penc);
    if (penclen < 0) {
        ERR_raise(ERR_LIB_EC, ERR_R_ASN1_LIB);
        return 0;
    }

    if (!PKCS8_pkey_set0(p8, OBJ_nid2obj(pkey->ameth->pkey_id), 0,
            V_ASN1_UNDEF, NULL, penc, penclen)) {
        OPENSSL_clear_free(penc, penclen);
        ERR_raise(ERR_LIB_EC, ERR_R_ASN1_LIB);
        return 0;
    }

    return 1;
}

static int ecx_size(const EVP_PKEY *pkey)
{
    return KEYLEN(pkey);
}

static int ecx_bits(const EVP_PKEY *pkey)
{
    if (IS25519(pkey->ameth->pkey_id)) {
        return X25519_BITS;
    } else if (ISX448(pkey->ameth->pkey_id)) {
        return X448_BITS;
    } else {
        return ED448_BITS;
    }
}

static int ecx_security_bits(const EVP_PKEY *pkey)
{
    if (IS25519(pkey->ameth->pkey_id)) {
        return X25519_SECURITY_BITS;
    } else {
        return X448_SECURITY_BITS;
    }
}

static void ecx_free(EVP_PKEY *pkey)
{
    ossl_ecx_key_free(pkey->pkey.ecx);
}

/* "parameters" are always equal */
static int ecx_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b)
{
    return 1;
}

static int ecx_key_print(BIO *bp, const EVP_PKEY *pkey, int indent,
    ASN1_PCTX *ctx, ecx_key_op_t op)
{
    const ECX_KEY *ecxkey = pkey->pkey.ecx;
    const char *nm = OBJ_nid2ln(pkey->ameth->pkey_id);

    if (op == KEY_OP_PRIVATE) {
        if (ecxkey == NULL || ecxkey->privkey == NULL) {
            if (BIO_printf(bp, "%*s<INVALID PRIVATE KEY>\n", indent, "") <= 0)
                return 0;
            return 1;
        }
        if (BIO_printf(bp, "%*s%s Private-Key:\n", indent, "", nm) <= 0)
            return 0;
        if (BIO_printf(bp, "%*spriv:\n", indent, "") <= 0)
            return 0;
        if (ASN1_buf_print(bp, ecxkey->privkey, KEYLEN(pkey),
                indent + 4)
            == 0)
            return 0;
    } else {
        if (ecxkey == NULL) {
            if (BIO_printf(bp, "%*s<INVALID PUBLIC KEY>\n", indent, "") <= 0)
                return 0;
            return 1;
        }
        if (BIO_printf(bp, "%*s%s Public-Key:\n", indent, "", nm) <= 0)
            return 0;
    }
    if (BIO_printf(bp, "%*spub:\n", indent, "") <= 0)
        return 0;

    if (ASN1_buf_print(bp, ecxkey->pubkey, KEYLEN(pkey),
            indent + 4)
        == 0)
        return 0;
    return 1;
}

static int ecx_priv_print(BIO *bp, const EVP_PKEY *pkey, int indent,
    ASN1_PCTX *ctx)
{
    return ecx_key_print(bp, pkey, indent, ctx, KEY_OP_PRIVATE);
}

static int ecx_pub_print(BIO *bp, const EVP_PKEY *pkey, int indent,
    ASN1_PCTX *ctx)
{
    return ecx_key_print(bp, pkey, indent, ctx, KEY_OP_PUBLIC);
}

static int ecx_ctrl(EVP_PKEY *pkey, int op, long arg1, void *arg2)
{
    switch (op) {

    case ASN1_PKEY_CTRL_SET1_TLS_ENCPT: {
        ECX_KEY *ecx = ossl_ecx_key_op(NULL, arg2, arg1, pkey->ameth->pkey_id,
            KEY_OP_PUBLIC, NULL, NULL);

        if (ecx != NULL) {
            EVP_PKEY_assign(pkey, pkey->ameth->pkey_id, ecx);
            return 1;
        }
        return 0;
    }
    case ASN1_PKEY_CTRL_GET1_TLS_ENCPT:
        if (pkey->pkey.ecx != NULL) {
            unsigned char **ppt = arg2;

            *ppt = OPENSSL_memdup(pkey->pkey.ecx->pubkey, KEYLEN(pkey));
            if (*ppt != NULL)
                return KEYLEN(pkey);
        }
        return 0;

    default:
        return -2;
    }
}

static int ecd_ctrl(EVP_PKEY *pkey, int op, long arg1, void *arg2)
{
    switch (op) {
    case ASN1_PKEY_CTRL_DEFAULT_MD_NID:
        /* We currently only support Pure EdDSA which takes no digest */
        *(int *)arg2 = NID_undef;
        return 2;

    default:
        return -2;
    }
}

static int ecx_set_priv_key(EVP_PKEY *pkey, const unsigned char *priv,
    size_t len)
{
    OSSL_LIB_CTX *libctx = NULL;
    ECX_KEY *ecx = NULL;

    if (pkey->keymgmt != NULL)
        libctx = ossl_provider_libctx(EVP_KEYMGMT_get0_provider(pkey->keymgmt));

    ecx = ossl_ecx_key_op(NULL, priv, (int)len, pkey->ameth->pkey_id,
        KEY_OP_PRIVATE, libctx, NULL);

    if (ecx != NULL) {
        EVP_PKEY_assign(pkey, pkey->ameth->pkey_id, ecx);
        return 1;
    }
    return 0;
}

static int ecx_set_pub_key(EVP_PKEY *pkey, const unsigned char *pub, size_t len)
{
    OSSL_LIB_CTX *libctx = NULL;
    ECX_KEY *ecx = NULL;

    if (pkey->keymgmt != NULL)
        libctx = ossl_provider_libctx(EVP_KEYMGMT_get0_provider(pkey->keymgmt));

    ecx = ossl_ecx_key_op(NULL, pub, (int)len, pkey->ameth->pkey_id,
        KEY_OP_PUBLIC, libctx, NULL);

    if (ecx != NULL) {
        EVP_PKEY_assign(pkey, pkey->ameth->pkey_id, ecx);
        return 1;
    }
    return 0;
}

static int ecx_get_priv_key(const EVP_PKEY *pkey, unsigned char *priv,
    size_t *len)
{
    const ECX_KEY *key = pkey->pkey.ecx;

    if (priv == NULL) {
        *len = KEYLENID(pkey->ameth->pkey_id);
        return 1;
    }

    if (key == NULL
        || key->privkey == NULL
        || *len < (size_t)KEYLENID(pkey->ameth->pkey_id))
        return 0;

    *len = KEYLENID(pkey->ameth->pkey_id);
    memcpy(priv, key->privkey, *len);

    return 1;
}

static int ecx_get_pub_key(const EVP_PKEY *pkey, unsigned char *pub,
    size_t *len)
{
    const ECX_KEY *key = pkey->pkey.ecx;

    if (pub == NULL) {
        *len = KEYLENID(pkey->ameth->pkey_id);
        return 1;
    }

    if (key == NULL
        || *len < (size_t)KEYLENID(pkey->ameth->pkey_id))
        return 0;

    *len = KEYLENID(pkey->ameth->pkey_id);
    memcpy(pub, key->pubkey, *len);

    return 1;
}

static size_t ecx_pkey_dirty_cnt(const EVP_PKEY *pkey)
{
    /*
     * We provide no mechanism to "update" an ECX key once it has been set,
     * therefore we do not have to maintain a dirty count.
     */
    return 1;
}

static int ecx_pkey_export_to(const EVP_PKEY *from, void *to_keydata,
    OSSL_FUNC_keymgmt_import_fn *importer,
    OSSL_LIB_CTX *libctx, const char *propq)
{
    const ECX_KEY *key = from->pkey.ecx;
    OSSL_PARAM_BLD *tmpl = OSSL_PARAM_BLD_new();
    OSSL_PARAM *params = NULL;
    int selection = 0;
    int rv = 0;

    if (tmpl == NULL)
        return 0;

    /* A key must at least have a public part */
    if (!OSSL_PARAM_BLD_push_octet_string(tmpl, OSSL_PKEY_PARAM_PUB_KEY,
            key->pubkey, key->keylen))
        goto err;
    selection |= OSSL_KEYMGMT_SELECT_PUBLIC_KEY;

    if (key->privkey != NULL) {
        if (!OSSL_PARAM_BLD_push_octet_string(tmpl,
                OSSL_PKEY_PARAM_PRIV_KEY,
                key->privkey, key->keylen))
            goto err;
        selection |= OSSL_KEYMGMT_SELECT_PRIVATE_KEY;
    }

    params = OSSL_PARAM_BLD_to_param(tmpl);

    /* We export, the provider imports */
    rv = importer(to_keydata, selection, params);

err:
    OSSL_PARAM_BLD_free(tmpl);
    OSSL_PARAM_free(params);
    return rv;
}

static int ecx_generic_import_from(const OSSL_PARAM params[], void *vpctx,
    int keytype)
{
    EVP_PKEY_CTX *pctx = vpctx;
    EVP_PKEY *pkey = EVP_PKEY_CTX_get0_pkey(pctx);
    ECX_KEY *ecx = ossl_ecx_key_new(pctx->libctx, KEYNID2TYPE(keytype), 0,
        pctx->propquery);
    const OSSL_PARAM *pub = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PUB_KEY);
    const OSSL_PARAM *priv = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PRIV_KEY);

    if (ecx == NULL) {
        ERR_raise(ERR_LIB_DH, ERR_R_EC_LIB);
        return 0;
    }

    if (!ossl_ecx_key_fromdata(ecx, pub, priv, 1)
        || !EVP_PKEY_assign(pkey, keytype, ecx)) {
        ossl_ecx_key_free(ecx);
        return 0;
    }
    return 1;
}

static int ecx_pkey_copy(EVP_PKEY *to, EVP_PKEY *from)
{
    ECX_KEY *ecx = from->pkey.ecx, *dupkey = NULL;
    int ret;

    if (ecx != NULL) {
        dupkey = ossl_ecx_key_dup(ecx, OSSL_KEYMGMT_SELECT_ALL);
        if (dupkey == NULL)
            return 0;
    }

    ret = EVP_PKEY_assign(to, from->type, dupkey);
    if (!ret)
        ossl_ecx_key_free(dupkey);
    return ret;
}

static int x25519_import_from(const OSSL_PARAM params[], void *vpctx)
{
    return ecx_generic_import_from(params, vpctx, EVP_PKEY_X25519);
}

const EVP_PKEY_ASN1_METHOD ossl_ecx25519_asn1_meth = {
    EVP_PKEY_X25519,
    EVP_PKEY_X25519,
    0,
    "X25519",
    "OpenSSL X25519 algorithm",

    ecx_pub_decode,
    ecx_pub_encode,
    ecx_pub_cmp,
    ecx_pub_print,

    NULL,
    ecx_priv_encode,
    ecx_priv_print,

    ecx_size,
    ecx_bits,
    ecx_security_bits,

    0, 0, 0, 0,
    ecx_cmp_parameters,
    0, 0,

    ecx_free,
    ecx_ctrl,
    NULL,
    NULL,

    NULL,
    NULL,
    NULL,

    NULL,
    NULL,
    NULL,

    ecx_set_priv_key,
    ecx_set_pub_key,
    ecx_get_priv_key,
    ecx_get_pub_key,
    ecx_pkey_dirty_cnt,
    ecx_pkey_export_to,
    x25519_import_from,
    ecx_pkey_copy,

    ecx_priv_decode_ex
};

static int x448_import_from(const OSSL_PARAM params[], void *vpctx)
{
    return ecx_generic_import_from(params, vpctx, EVP_PKEY_X448);
}

const EVP_PKEY_ASN1_METHOD ossl_ecx448_asn1_meth = {
    EVP_PKEY_X448,
    EVP_PKEY_X448,
    0,
    "X448",
    "OpenSSL X448 algorithm",

    ecx_pub_decode,
    ecx_pub_encode,
    ecx_pub_cmp,
    ecx_pub_print,

    NULL,
    ecx_priv_encode,
    ecx_priv_print,

    ecx_size,
    ecx_bits,
    ecx_security_bits,

    0, 0, 0, 0,
    ecx_cmp_parameters,
    0, 0,

    ecx_free,
    ecx_ctrl,
    NULL,
    NULL,

    NULL,
    NULL,
    NULL,

    NULL,
    NULL,
    NULL,

    ecx_set_priv_key,
    ecx_set_pub_key,
    ecx_get_priv_key,
    ecx_get_pub_key,
    ecx_pkey_dirty_cnt,
    ecx_pkey_export_to,
    x448_import_from,
    ecx_pkey_copy,

    ecx_priv_decode_ex
};

static int ecd_size25519(const EVP_PKEY *pkey)
{
    return ED25519_SIGSIZE;
}

static int ecd_size448(const EVP_PKEY *pkey)
{
    return ED448_SIGSIZE;
}

static int ecd_item_verify(EVP_MD_CTX *ctx, const ASN1_ITEM *it,
    const void *asn, const X509_ALGOR *sigalg,
    const ASN1_BIT_STRING *str, EVP_PKEY *pkey)
{
    const ASN1_OBJECT *obj;
    int ptype;
    int nid;

    /* Sanity check: make sure it is ED25519/ED448 with absent parameters */
    X509_ALGOR_get0(&obj, &ptype, NULL, sigalg);
    nid = OBJ_obj2nid(obj);
    if ((nid != NID_ED25519 && nid != NID_ED448) || ptype != V_ASN1_UNDEF) {
        ERR_raise(ERR_LIB_EC, EC_R_INVALID_ENCODING);
        return 0;
    }

    if (!EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey))
        return 0;

    return 2;
}

static int ecd_item_sign(X509_ALGOR *alg1, X509_ALGOR *alg2, int nid)
{
    /* Note that X509_ALGOR_set0(..., ..., V_ASN1_UNDEF, ...) cannot fail */
    /* Set algorithms identifiers */
    (void)X509_ALGOR_set0(alg1, OBJ_nid2obj(nid), V_ASN1_UNDEF, NULL);
    if (alg2 != NULL)
        (void)X509_ALGOR_set0(alg2, OBJ_nid2obj(nid), V_ASN1_UNDEF, NULL);
    /* Algorithm identifiers set: carry on as normal */
    return 3;
}

static int ecd_item_sign25519(EVP_MD_CTX *ctx, const ASN1_ITEM *it,
    const void *asn,
    X509_ALGOR *alg1, X509_ALGOR *alg2,
    ASN1_BIT_STRING *str)
{
    return ecd_item_sign(alg1, alg2, NID_ED25519);
}

static int ecd_sig_info_set25519(X509_SIG_INFO *siginf, const X509_ALGOR *alg,
    const ASN1_STRING *sig)
{
    X509_SIG_INFO_set(siginf, NID_undef, NID_ED25519, X25519_SECURITY_BITS,
        X509_SIG_INFO_TLS);
    return 1;
}

static int ecd_item_sign448(EVP_MD_CTX *ctx, const ASN1_ITEM *it,
    const void *asn,
    X509_ALGOR *alg1, X509_ALGOR *alg2,
    ASN1_BIT_STRING *str)
{
    return ecd_item_sign(alg1, alg2, NID_ED448);
}

static int ecd_sig_info_set448(X509_SIG_INFO *siginf, const X509_ALGOR *alg,
    const ASN1_STRING *sig)
{
    X509_SIG_INFO_set(siginf, NID_undef, NID_ED448, X448_SECURITY_BITS,
        X509_SIG_INFO_TLS);
    return 1;
}

static int ed25519_import_from(const OSSL_PARAM params[], void *vpctx)
{
    return ecx_generic_import_from(params, vpctx, EVP_PKEY_ED25519);
}

const EVP_PKEY_ASN1_METHOD ossl_ed25519_asn1_meth = {
    EVP_PKEY_ED25519,
    EVP_PKEY_ED25519,
    0,
    "ED25519",
    "OpenSSL ED25519 algorithm",

    ecx_pub_decode,
    ecx_pub_encode,
    ecx_pub_cmp,
    ecx_pub_print,

    NULL,
    ecx_priv_encode,
    ecx_priv_print,

    ecd_size25519,
    ecx_bits,
    ecx_security_bits,

    0, 0, 0, 0,
    ecx_cmp_parameters,
    0, 0,

    ecx_free,
    ecd_ctrl,
    NULL,
    NULL,
    ecd_item_verify,
    ecd_item_sign25519,
    ecd_sig_info_set25519,

    NULL,
    NULL,
    NULL,

    ecx_set_priv_key,
    ecx_set_pub_key,
    ecx_get_priv_key,
    ecx_get_pub_key,
    ecx_pkey_dirty_cnt,
    ecx_pkey_export_to,
    ed25519_import_from,
    ecx_pkey_copy,

    ecx_priv_decode_ex
};

static int ed448_import_from(const OSSL_PARAM params[], void *vpctx)
{
    return ecx_generic_import_from(params, vpctx, EVP_PKEY_ED448);
}

const EVP_PKEY_ASN1_METHOD ossl_ed448_asn1_meth = {
    EVP_PKEY_ED448,
    EVP_PKEY_ED448,
    0,
    "ED448",
    "OpenSSL ED448 algorithm",

    ecx_pub_decode,
    ecx_pub_encode,
    ecx_pub_cmp,
    ecx_pub_print,

    NULL,
    ecx_priv_encode,
    ecx_priv_print,

    ecd_size448,
    ecx_bits,
    ecx_security_bits,

    0, 0, 0, 0,
    ecx_cmp_parameters,
    0, 0,

    ecx_free,
    ecd_ctrl,
    NULL,
    NULL,
    ecd_item_verify,
    ecd_item_sign448,
    ecd_sig_info_set448,

    NULL,
    NULL,
    NULL,

    ecx_set_priv_key,
    ecx_set_pub_key,
    ecx_get_priv_key,
    ecx_get_pub_key,
    ecx_pkey_dirty_cnt,
    ecx_pkey_export_to,
    ed448_import_from,
    ecx_pkey_copy,

    ecx_priv_decode_ex
};

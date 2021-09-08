/*
 * Copyright 1995-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * EVP _meth_ APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <stdio.h>
#include <string.h>
#include "e_os.h" /* strcasecmp */
#include "internal/cryptlib.h"
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <openssl/rsa.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include "crypto/evp.h"
#include "internal/provider.h"
#include "evp_local.h"

#if !defined(FIPS_MODULE)
# include "crypto/asn1.h"

int EVP_CIPHER_param_to_asn1(EVP_CIPHER_CTX *c, ASN1_TYPE *type)
{
    return evp_cipher_param_to_asn1_ex(c, type, NULL);
}

int EVP_CIPHER_asn1_to_param(EVP_CIPHER_CTX *c, ASN1_TYPE *type)
{
    return evp_cipher_asn1_to_param_ex(c, type, NULL);
}

int EVP_CIPHER_get_asn1_iv(EVP_CIPHER_CTX *ctx, ASN1_TYPE *type)
{
    int i = 0;
    unsigned int l;

    if (type != NULL) {
        unsigned char iv[EVP_MAX_IV_LENGTH];

        l = EVP_CIPHER_CTX_get_iv_length(ctx);
        if (!ossl_assert(l <= sizeof(iv)))
            return -1;
        i = ASN1_TYPE_get_octetstring(type, iv, l);
        if (i != (int)l)
            return -1;

        if (!EVP_CipherInit_ex(ctx, NULL, NULL, NULL, iv, -1))
            return -1;
    }
    return i;
}

int EVP_CIPHER_set_asn1_iv(EVP_CIPHER_CTX *c, ASN1_TYPE *type)
{
    int i = 0;
    unsigned int j;
    unsigned char *oiv = NULL;

    if (type != NULL) {
        oiv = (unsigned char *)EVP_CIPHER_CTX_original_iv(c);
        j = EVP_CIPHER_CTX_get_iv_length(c);
        OPENSSL_assert(j <= sizeof(c->iv));
        i = ASN1_TYPE_set_octetstring(type, oiv, j);
    }
    return i;
}

int evp_cipher_param_to_asn1_ex(EVP_CIPHER_CTX *c, ASN1_TYPE *type,
                                evp_cipher_aead_asn1_params *asn1_params)
{
    int ret = -1;                /* Assume the worst */
    const EVP_CIPHER *cipher = c->cipher;

    /*
     * For legacy implementations, we detect custom AlgorithmIdentifier
     * parameter handling by checking if the function pointer
     * cipher->set_asn1_parameters is set.  We know that this pointer
     * is NULL for provided implementations.
     *
     * Otherwise, for any implementation, we check the flag
     * EVP_CIPH_FLAG_CUSTOM_ASN1.  If it isn't set, we apply
     * default AI parameter extraction.
     *
     * Otherwise, for provided implementations, we convert |type| to
     * a DER encoded blob and pass to the implementation in OSSL_PARAM
     * form.
     *
     * If none of the above applies, this operation is unsupported.
     */
    if (cipher->set_asn1_parameters != NULL) {
        ret = cipher->set_asn1_parameters(c, type);
    } else if ((EVP_CIPHER_get_flags(cipher) & EVP_CIPH_FLAG_CUSTOM_ASN1) == 0) {
        switch (EVP_CIPHER_get_mode(cipher)) {
        case EVP_CIPH_WRAP_MODE:
            if (EVP_CIPHER_is_a(cipher, SN_id_smime_alg_CMS3DESwrap))
                ASN1_TYPE_set(type, V_ASN1_NULL, NULL);
            ret = 1;
            break;

        case EVP_CIPH_GCM_MODE:
            ret = evp_cipher_set_asn1_aead_params(c, type, asn1_params);
            break;

        case EVP_CIPH_CCM_MODE:
        case EVP_CIPH_XTS_MODE:
        case EVP_CIPH_OCB_MODE:
            ret = -2;
            break;

        default:
            ret = EVP_CIPHER_set_asn1_iv(c, type);
        }
    } else if (cipher->prov != NULL) {
        OSSL_PARAM params[3], *p = params;
        unsigned char *der = NULL, *derp;

        /*
         * We make two passes, the first to get the appropriate buffer size,
         * and the second to get the actual value.
         */
        *p++ = OSSL_PARAM_construct_octet_string(
                       OSSL_CIPHER_PARAM_ALGORITHM_ID_PARAMS,
                       NULL, 0);
        *p = OSSL_PARAM_construct_end();

        if (!EVP_CIPHER_CTX_get_params(c, params))
            goto err;

        /* ... but, we should get a return size too! */
        if (OSSL_PARAM_modified(params)
            && params[0].return_size != 0
            && (der = OPENSSL_malloc(params[0].return_size)) != NULL) {
            params[0].data = der;
            params[0].data_size = params[0].return_size;
            OSSL_PARAM_set_all_unmodified(params);
            derp = der;
            if (EVP_CIPHER_CTX_get_params(c, params)
                && OSSL_PARAM_modified(params)
                && d2i_ASN1_TYPE(&type, (const unsigned char **)&derp,
                                 params[0].return_size) != NULL) {
                ret = 1;
            }
            OPENSSL_free(der);
        }
    } else {
        ret = -2;
    }

 err:
    if (ret == -2)
        ERR_raise(ERR_LIB_EVP, EVP_R_UNSUPPORTED_CIPHER);
    else if (ret <= 0)
        ERR_raise(ERR_LIB_EVP, EVP_R_CIPHER_PARAMETER_ERROR);
    if (ret < -1)
        ret = -1;
    return ret;
}

int evp_cipher_asn1_to_param_ex(EVP_CIPHER_CTX *c, ASN1_TYPE *type,
                                evp_cipher_aead_asn1_params *asn1_params)
{
    int ret = -1;                /* Assume the worst */
    const EVP_CIPHER *cipher = c->cipher;

    /*
     * For legacy implementations, we detect custom AlgorithmIdentifier
     * parameter handling by checking if there the function pointer
     * cipher->get_asn1_parameters is set.  We know that this pointer
     * is NULL for provided implementations.
     *
     * Otherwise, for any implementation, we check the flag
     * EVP_CIPH_FLAG_CUSTOM_ASN1.  If it isn't set, we apply
     * default AI parameter creation.
     *
     * Otherwise, for provided implementations, we get the AI parameter
     * in DER encoded form from the implementation by requesting the
     * appropriate OSSL_PARAM and converting the result to a ASN1_TYPE.
     *
     * If none of the above applies, this operation is unsupported.
     */
    if (cipher->get_asn1_parameters != NULL) {
        ret = cipher->get_asn1_parameters(c, type);
    } else if ((EVP_CIPHER_get_flags(cipher) & EVP_CIPH_FLAG_CUSTOM_ASN1) == 0) {
        switch (EVP_CIPHER_get_mode(cipher)) {
        case EVP_CIPH_WRAP_MODE:
            ret = 1;
            break;

        case EVP_CIPH_GCM_MODE:
            ret = evp_cipher_get_asn1_aead_params(c, type, asn1_params);
            break;

        case EVP_CIPH_CCM_MODE:
        case EVP_CIPH_XTS_MODE:
        case EVP_CIPH_OCB_MODE:
            ret = -2;
            break;

        default:
            ret = EVP_CIPHER_get_asn1_iv(c, type);
        }
    } else if (cipher->prov != NULL) {
        OSSL_PARAM params[3], *p = params;
        unsigned char *der = NULL;
        int derl = -1;

        if ((derl = i2d_ASN1_TYPE(type, &der)) >= 0) {
            *p++ =
                OSSL_PARAM_construct_octet_string(
                        OSSL_CIPHER_PARAM_ALGORITHM_ID_PARAMS,
                        der, (size_t)derl);
            *p = OSSL_PARAM_construct_end();
            if (EVP_CIPHER_CTX_set_params(c, params))
                ret = 1;
            OPENSSL_free(der);
        }
    } else {
        ret = -2;
    }

    if (ret == -2)
        ERR_raise(ERR_LIB_EVP, EVP_R_UNSUPPORTED_CIPHER);
    else if (ret <= 0)
        ERR_raise(ERR_LIB_EVP, EVP_R_CIPHER_PARAMETER_ERROR);
    if (ret < -1)
        ret = -1;
    return ret;
}

int evp_cipher_get_asn1_aead_params(EVP_CIPHER_CTX *c, ASN1_TYPE *type,
                                    evp_cipher_aead_asn1_params *asn1_params)
{
    int i = 0;
    long tl;
    unsigned char iv[EVP_MAX_IV_LENGTH];

    if (type == NULL || asn1_params == NULL)
        return 0;

    i = ossl_asn1_type_get_octetstring_int(type, &tl, NULL, EVP_MAX_IV_LENGTH);
    if (i <= 0)
        return -1;
    ossl_asn1_type_get_octetstring_int(type, &tl, iv, i);

    memcpy(asn1_params->iv, iv, i);
    asn1_params->iv_len = i;

    return i;
}

int evp_cipher_set_asn1_aead_params(EVP_CIPHER_CTX *c, ASN1_TYPE *type,
                                    evp_cipher_aead_asn1_params *asn1_params)
{
    if (type == NULL || asn1_params == NULL)
        return 0;

    return ossl_asn1_type_set_octetstring_int(type, asn1_params->tag_len,
                                              asn1_params->iv,
                                              asn1_params->iv_len);
}
#endif /* !defined(FIPS_MODULE) */

/* Convert the various cipher NIDs and dummies to a proper OID NID */
int EVP_CIPHER_get_type(const EVP_CIPHER *cipher)
{
    int nid;
    nid = EVP_CIPHER_get_nid(cipher);

    switch (nid) {

    case NID_rc2_cbc:
    case NID_rc2_64_cbc:
    case NID_rc2_40_cbc:

        return NID_rc2_cbc;

    case NID_rc4:
    case NID_rc4_40:

        return NID_rc4;

    case NID_aes_128_cfb128:
    case NID_aes_128_cfb8:
    case NID_aes_128_cfb1:

        return NID_aes_128_cfb128;

    case NID_aes_192_cfb128:
    case NID_aes_192_cfb8:
    case NID_aes_192_cfb1:

        return NID_aes_192_cfb128;

    case NID_aes_256_cfb128:
    case NID_aes_256_cfb8:
    case NID_aes_256_cfb1:

        return NID_aes_256_cfb128;

    case NID_des_cfb64:
    case NID_des_cfb8:
    case NID_des_cfb1:

        return NID_des_cfb64;

    case NID_des_ede3_cfb64:
    case NID_des_ede3_cfb8:
    case NID_des_ede3_cfb1:

        return NID_des_cfb64;

    default:
#ifdef FIPS_MODULE
        return NID_undef;
#else
        {
            /* Check it has an OID and it is valid */
            ASN1_OBJECT *otmp = OBJ_nid2obj(nid);

            if (OBJ_get0_data(otmp) == NULL)
                nid = NID_undef;
            ASN1_OBJECT_free(otmp);
            return nid;
        }
#endif
    }
}

int evp_cipher_cache_constants(EVP_CIPHER *cipher)
{
    int ok, aead = 0, custom_iv = 0, cts = 0, multiblock = 0, randkey = 0;
    size_t ivlen = 0;
    size_t blksz = 0;
    size_t keylen = 0;
    unsigned int mode = 0;
    OSSL_PARAM params[10];

    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_BLOCK_SIZE, &blksz);
    params[1] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &ivlen);
    params[2] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN, &keylen);
    params[3] = OSSL_PARAM_construct_uint(OSSL_CIPHER_PARAM_MODE, &mode);
    params[4] = OSSL_PARAM_construct_int(OSSL_CIPHER_PARAM_AEAD, &aead);
    params[5] = OSSL_PARAM_construct_int(OSSL_CIPHER_PARAM_CUSTOM_IV,
                                         &custom_iv);
    params[6] = OSSL_PARAM_construct_int(OSSL_CIPHER_PARAM_CTS, &cts);
    params[7] = OSSL_PARAM_construct_int(OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK,
                                         &multiblock);
    params[8] = OSSL_PARAM_construct_int(OSSL_CIPHER_PARAM_HAS_RAND_KEY,
                                         &randkey);
    params[9] = OSSL_PARAM_construct_end();
    ok = evp_do_ciph_getparams(cipher, params) > 0;
    if (ok) {
        cipher->block_size = blksz;
        cipher->iv_len = ivlen;
        cipher->key_len = keylen;
        cipher->flags = mode;
        if (aead)
            cipher->flags |= EVP_CIPH_FLAG_AEAD_CIPHER;
        if (custom_iv)
            cipher->flags |= EVP_CIPH_CUSTOM_IV;
        if (cts)
            cipher->flags |= EVP_CIPH_FLAG_CTS;
        if (multiblock)
            cipher->flags |= EVP_CIPH_FLAG_TLS1_1_MULTIBLOCK;
        if (cipher->ccipher != NULL)
            cipher->flags |= EVP_CIPH_FLAG_CUSTOM_CIPHER;
        if (randkey)
            cipher->flags |= EVP_CIPH_RAND_KEY;
        if (OSSL_PARAM_locate_const(EVP_CIPHER_gettable_ctx_params(cipher),
                                    OSSL_CIPHER_PARAM_ALGORITHM_ID_PARAMS))
            cipher->flags |= EVP_CIPH_FLAG_CUSTOM_ASN1;
    }
    return ok;
}

int EVP_CIPHER_get_block_size(const EVP_CIPHER *cipher)
{
    return cipher->block_size;
}

int EVP_CIPHER_CTX_get_block_size(const EVP_CIPHER_CTX *ctx)
{
    return EVP_CIPHER_get_block_size(ctx->cipher);
}

int EVP_CIPHER_impl_ctx_size(const EVP_CIPHER *e)
{
    return e->ctx_size;
}

int EVP_Cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
               const unsigned char *in, unsigned int inl)
{
    if (ctx->cipher->prov != NULL) {
        /*
         * If the provided implementation has a ccipher function, we use it,
         * and translate its return value like this: 0 => -1, 1 => outlen
         *
         * Otherwise, we call the cupdate function if in != NULL, or cfinal
         * if in == NULL.  Regardless of which, we return what we got.
         */
        int ret = -1;
        size_t outl = 0;
        size_t blocksize = EVP_CIPHER_CTX_get_block_size(ctx);

        if (ctx->cipher->ccipher != NULL)
            ret =  ctx->cipher->ccipher(ctx->algctx, out, &outl,
                                        inl + (blocksize == 1 ? 0 : blocksize),
                                        in, (size_t)inl)
                ? (int)outl : -1;
        else if (in != NULL)
            ret = ctx->cipher->cupdate(ctx->algctx, out, &outl,
                                       inl + (blocksize == 1 ? 0 : blocksize),
                                       in, (size_t)inl);
        else
            ret = ctx->cipher->cfinal(ctx->algctx, out, &outl,
                                      blocksize == 1 ? 0 : blocksize);

        return ret;
    }

    return ctx->cipher->do_cipher(ctx, out, in, inl);
}

#ifndef OPENSSL_NO_DEPRECATED_3_0
const EVP_CIPHER *EVP_CIPHER_CTX_cipher(const EVP_CIPHER_CTX *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->cipher;
}
#endif

const EVP_CIPHER *EVP_CIPHER_CTX_get0_cipher(const EVP_CIPHER_CTX *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->cipher;
}

EVP_CIPHER *EVP_CIPHER_CTX_get1_cipher(EVP_CIPHER_CTX *ctx)
{
    EVP_CIPHER *cipher;

    if (ctx == NULL)
        return NULL;
    cipher = (EVP_CIPHER *)ctx->cipher;
    if (!EVP_CIPHER_up_ref(cipher))
        return NULL;
    return cipher;
}

int EVP_CIPHER_CTX_is_encrypting(const EVP_CIPHER_CTX *ctx)
{
    return ctx->encrypt;
}

unsigned long EVP_CIPHER_get_flags(const EVP_CIPHER *cipher)
{
    return cipher->flags;
}

void *EVP_CIPHER_CTX_get_app_data(const EVP_CIPHER_CTX *ctx)
{
    return ctx->app_data;
}

void EVP_CIPHER_CTX_set_app_data(EVP_CIPHER_CTX *ctx, void *data)
{
    ctx->app_data = data;
}

void *EVP_CIPHER_CTX_get_cipher_data(const EVP_CIPHER_CTX *ctx)
{
    return ctx->cipher_data;
}

void *EVP_CIPHER_CTX_set_cipher_data(EVP_CIPHER_CTX *ctx, void *cipher_data)
{
    void *old_cipher_data;

    old_cipher_data = ctx->cipher_data;
    ctx->cipher_data = cipher_data;

    return old_cipher_data;
}

int EVP_CIPHER_get_iv_length(const EVP_CIPHER *cipher)
{
    return cipher->iv_len;
}

int EVP_CIPHER_CTX_get_iv_length(const EVP_CIPHER_CTX *ctx)
{
    int rv, len = EVP_CIPHER_get_iv_length(ctx->cipher);
    size_t v = len;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &v);
    rv = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);
    if (rv == EVP_CTRL_RET_UNSUPPORTED)
        goto legacy;
    return rv != 0 ? (int)v : -1;
    /* Code below to be removed when legacy support is dropped. */
legacy:
    if ((EVP_CIPHER_get_flags(ctx->cipher) & EVP_CIPH_CUSTOM_IV_LENGTH) != 0) {
        rv = EVP_CIPHER_CTX_ctrl((EVP_CIPHER_CTX *)ctx, EVP_CTRL_GET_IVLEN,
                                 0, &len);
        return (rv == 1) ? len : -1;
    }
    return len;
}

int EVP_CIPHER_CTX_get_tag_length(const EVP_CIPHER_CTX *ctx)
{
    int ret;
    size_t v = 0;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_TAGLEN, &v);
    ret = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);
    return ret == 1 ? (int)v : 0;
}

#ifndef OPENSSL_NO_DEPRECATED_3_0
const unsigned char *EVP_CIPHER_CTX_original_iv(const EVP_CIPHER_CTX *ctx)
{
    int ok;
    const unsigned char *v = ctx->oiv;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] =
        OSSL_PARAM_construct_octet_ptr(OSSL_CIPHER_PARAM_IV,
                                       (void **)&v, sizeof(ctx->oiv));
    ok = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);

    return ok != 0 ? v : NULL;
}

/*
 * OSSL_PARAM_OCTET_PTR gets us the pointer to the running IV in the provider
 */
const unsigned char *EVP_CIPHER_CTX_iv(const EVP_CIPHER_CTX *ctx)
{
    int ok;
    const unsigned char *v = ctx->iv;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] =
        OSSL_PARAM_construct_octet_ptr(OSSL_CIPHER_PARAM_UPDATED_IV,
                                       (void **)&v, sizeof(ctx->iv));
    ok = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);

    return ok != 0 ? v : NULL;
}

unsigned char *EVP_CIPHER_CTX_iv_noconst(EVP_CIPHER_CTX *ctx)
{
    int ok;
    unsigned char *v = ctx->iv;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] =
        OSSL_PARAM_construct_octet_ptr(OSSL_CIPHER_PARAM_UPDATED_IV,
                                       (void **)&v, sizeof(ctx->iv));
    ok = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);

    return ok != 0 ? v : NULL;
}
#endif /* OPENSSL_NO_DEPRECATED_3_0_0 */

int EVP_CIPHER_CTX_get_updated_iv(EVP_CIPHER_CTX *ctx, void *buf, size_t len)
{
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] =
        OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_UPDATED_IV, buf, len);
    return evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);
}

int EVP_CIPHER_CTX_get_original_iv(EVP_CIPHER_CTX *ctx, void *buf, size_t len)
{
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] =
        OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_IV, buf, len);
    return evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);
}

unsigned char *EVP_CIPHER_CTX_buf_noconst(EVP_CIPHER_CTX *ctx)
{
    return ctx->buf;
}

int EVP_CIPHER_CTX_get_num(const EVP_CIPHER_CTX *ctx)
{
    int ok;
    unsigned int v = (unsigned int)ctx->num;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] = OSSL_PARAM_construct_uint(OSSL_CIPHER_PARAM_NUM, &v);
    ok = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);

    return ok != 0 ? (int)v : EVP_CTRL_RET_UNSUPPORTED;
}

int EVP_CIPHER_CTX_set_num(EVP_CIPHER_CTX *ctx, int num)
{
    int ok;
    unsigned int n = (unsigned int)num;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] = OSSL_PARAM_construct_uint(OSSL_CIPHER_PARAM_NUM, &n);
    ok = evp_do_ciph_ctx_setparams(ctx->cipher, ctx->algctx, params);

    if (ok != 0)
        ctx->num = (int)n;
    return ok != 0;
}

int EVP_CIPHER_get_key_length(const EVP_CIPHER *cipher)
{
    return cipher->key_len;
}

int EVP_CIPHER_CTX_get_key_length(const EVP_CIPHER_CTX *ctx)
{
    int ok;
    size_t v = ctx->key_len;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN, &v);
    ok = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);

    return ok != 0 ? (int)v : EVP_CTRL_RET_UNSUPPORTED;
}

int EVP_CIPHER_get_nid(const EVP_CIPHER *cipher)
{
    return cipher->nid;
}

int EVP_CIPHER_CTX_get_nid(const EVP_CIPHER_CTX *ctx)
{
    return ctx->cipher->nid;
}

int EVP_CIPHER_is_a(const EVP_CIPHER *cipher, const char *name)
{
    if (cipher->prov != NULL)
        return evp_is_a(cipher->prov, cipher->name_id, NULL, name);
    return evp_is_a(NULL, 0, EVP_CIPHER_get0_name(cipher), name);
}

int evp_cipher_get_number(const EVP_CIPHER *cipher)
{
    return cipher->name_id;
}

const char *EVP_CIPHER_get0_name(const EVP_CIPHER *cipher)
{
    if (cipher->type_name != NULL)
        return cipher->type_name;
#ifndef FIPS_MODULE
    return OBJ_nid2sn(EVP_CIPHER_get_nid(cipher));
#else
    return NULL;
#endif
}

const char *EVP_CIPHER_get0_description(const EVP_CIPHER *cipher)
{
    if (cipher->description != NULL)
        return cipher->description;
#ifndef FIPS_MODULE
    return OBJ_nid2ln(EVP_CIPHER_get_nid(cipher));
#else
    return NULL;
#endif
}

int EVP_CIPHER_names_do_all(const EVP_CIPHER *cipher,
                            void (*fn)(const char *name, void *data),
                            void *data)
{
    if (cipher->prov != NULL)
        return evp_names_do_all(cipher->prov, cipher->name_id, fn, data);

    return 1;
}

const OSSL_PROVIDER *EVP_CIPHER_get0_provider(const EVP_CIPHER *cipher)
{
    return cipher->prov;
}

int EVP_CIPHER_get_mode(const EVP_CIPHER *cipher)
{
    return EVP_CIPHER_get_flags(cipher) & EVP_CIPH_MODE;
}

int EVP_MD_is_a(const EVP_MD *md, const char *name)
{
    if (md->prov != NULL)
        return evp_is_a(md->prov, md->name_id, NULL, name);
    return evp_is_a(NULL, 0, EVP_MD_get0_name(md), name);
}

int evp_md_get_number(const EVP_MD *md)
{
    return md->name_id;
}

const char *EVP_MD_get0_description(const EVP_MD *md)
{
    if (md->description != NULL)
        return md->description;
#ifndef FIPS_MODULE
    return OBJ_nid2ln(EVP_MD_nid(md));
#else
    return NULL;
#endif
}

const char *EVP_MD_get0_name(const EVP_MD *md)
{
    if (md == NULL)
        return NULL;
    if (md->type_name != NULL)
        return md->type_name;
#ifndef FIPS_MODULE
    return OBJ_nid2sn(EVP_MD_nid(md));
#else
    return NULL;
#endif
}

int EVP_MD_names_do_all(const EVP_MD *md,
                        void (*fn)(const char *name, void *data),
                        void *data)
{
    if (md->prov != NULL)
        return evp_names_do_all(md->prov, md->name_id, fn, data);

    return 1;
}

const OSSL_PROVIDER *EVP_MD_get0_provider(const EVP_MD *md)
{
    return md->prov;
}

int EVP_MD_get_type(const EVP_MD *md)
{
    return md->type;
}

int EVP_MD_get_pkey_type(const EVP_MD *md)
{
    return md->pkey_type;
}

int EVP_MD_get_block_size(const EVP_MD *md)
{
    if (md == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_MESSAGE_DIGEST_IS_NULL);
        return -1;
    }
    return md->block_size;
}

int EVP_MD_get_size(const EVP_MD *md)
{
    if (md == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_MESSAGE_DIGEST_IS_NULL);
        return -1;
    }
    return md->md_size;
}

unsigned long EVP_MD_get_flags(const EVP_MD *md)
{
    return md->flags;
}

EVP_MD *EVP_MD_meth_new(int md_type, int pkey_type)
{
    EVP_MD *md = evp_md_new();

    if (md != NULL) {
        md->type = md_type;
        md->pkey_type = pkey_type;
        md->origin = EVP_ORIG_METH;
    }
    return md;
}

EVP_MD *EVP_MD_meth_dup(const EVP_MD *md)
{
    EVP_MD *to = NULL;

    /*
     * Non-legacy EVP_MDs can't be duplicated like this.
     * Use EVP_MD_up_ref() instead.
     */
    if (md->prov != NULL)
        return NULL;

    if ((to = EVP_MD_meth_new(md->type, md->pkey_type)) != NULL) {
        CRYPTO_RWLOCK *lock = to->lock;

        memcpy(to, md, sizeof(*to));
        to->lock = lock;
        to->origin = EVP_ORIG_METH;
    }
    return to;
}

void evp_md_free_int(EVP_MD *md)
{
    OPENSSL_free(md->type_name);
    ossl_provider_free(md->prov);
    CRYPTO_THREAD_lock_free(md->lock);
    OPENSSL_free(md);
}

void EVP_MD_meth_free(EVP_MD *md)
{
    if (md == NULL || md->origin != EVP_ORIG_METH)
       return;

    evp_md_free_int(md);
}

int EVP_MD_meth_set_input_blocksize(EVP_MD *md, int blocksize)
{
    if (md->block_size != 0)
        return 0;

    md->block_size = blocksize;
    return 1;
}
int EVP_MD_meth_set_result_size(EVP_MD *md, int resultsize)
{
    if (md->md_size != 0)
        return 0;

    md->md_size = resultsize;
    return 1;
}
int EVP_MD_meth_set_app_datasize(EVP_MD *md, int datasize)
{
    if (md->ctx_size != 0)
        return 0;

    md->ctx_size = datasize;
    return 1;
}
int EVP_MD_meth_set_flags(EVP_MD *md, unsigned long flags)
{
    if (md->flags != 0)
        return 0;

    md->flags = flags;
    return 1;
}
int EVP_MD_meth_set_init(EVP_MD *md, int (*init)(EVP_MD_CTX *ctx))
{
    if (md->init != NULL)
        return 0;

    md->init = init;
    return 1;
}
int EVP_MD_meth_set_update(EVP_MD *md, int (*update)(EVP_MD_CTX *ctx,
                                                     const void *data,
                                                     size_t count))
{
    if (md->update != NULL)
        return 0;

    md->update = update;
    return 1;
}
int EVP_MD_meth_set_final(EVP_MD *md, int (*final)(EVP_MD_CTX *ctx,
                                                   unsigned char *md))
{
    if (md->final != NULL)
        return 0;

    md->final = final;
    return 1;
}
int EVP_MD_meth_set_copy(EVP_MD *md, int (*copy)(EVP_MD_CTX *to,
                                                 const EVP_MD_CTX *from))
{
    if (md->copy != NULL)
        return 0;

    md->copy = copy;
    return 1;
}
int EVP_MD_meth_set_cleanup(EVP_MD *md, int (*cleanup)(EVP_MD_CTX *ctx))
{
    if (md->cleanup != NULL)
        return 0;

    md->cleanup = cleanup;
    return 1;
}
int EVP_MD_meth_set_ctrl(EVP_MD *md, int (*ctrl)(EVP_MD_CTX *ctx, int cmd,
                                                 int p1, void *p2))
{
    if (md->md_ctrl != NULL)
        return 0;

    md->md_ctrl = ctrl;
    return 1;
}

int EVP_MD_meth_get_input_blocksize(const EVP_MD *md)
{
    return md->block_size;
}
int EVP_MD_meth_get_result_size(const EVP_MD *md)
{
    return md->md_size;
}
int EVP_MD_meth_get_app_datasize(const EVP_MD *md)
{
    return md->ctx_size;
}
unsigned long EVP_MD_meth_get_flags(const EVP_MD *md)
{
    return md->flags;
}
int (*EVP_MD_meth_get_init(const EVP_MD *md))(EVP_MD_CTX *ctx)
{
    return md->init;
}
int (*EVP_MD_meth_get_update(const EVP_MD *md))(EVP_MD_CTX *ctx,
                                                const void *data,
                                                size_t count)
{
    return md->update;
}
int (*EVP_MD_meth_get_final(const EVP_MD *md))(EVP_MD_CTX *ctx,
                                               unsigned char *md)
{
    return md->final;
}
int (*EVP_MD_meth_get_copy(const EVP_MD *md))(EVP_MD_CTX *to,
                                              const EVP_MD_CTX *from)
{
    return md->copy;
}
int (*EVP_MD_meth_get_cleanup(const EVP_MD *md))(EVP_MD_CTX *ctx)
{
    return md->cleanup;
}
int (*EVP_MD_meth_get_ctrl(const EVP_MD *md))(EVP_MD_CTX *ctx, int cmd,
                                              int p1, void *p2)
{
    return md->md_ctrl;
}

#ifndef OPENSSL_NO_DEPRECATED_3_0
const EVP_MD *EVP_MD_CTX_md(const EVP_MD_CTX *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->reqdigest;
}
#endif

const EVP_MD *EVP_MD_CTX_get0_md(const EVP_MD_CTX *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->reqdigest;
}

EVP_MD *EVP_MD_CTX_get1_md(EVP_MD_CTX *ctx)
{
    EVP_MD *md;

    if (ctx == NULL)
        return NULL;
    md = (EVP_MD *)ctx->reqdigest;
    if (!EVP_MD_up_ref(md))
        return NULL;
    return md;
}

EVP_PKEY_CTX *EVP_MD_CTX_get_pkey_ctx(const EVP_MD_CTX *ctx)
{
    return ctx->pctx;
}

#if !defined(FIPS_MODULE)
void EVP_MD_CTX_set_pkey_ctx(EVP_MD_CTX *ctx, EVP_PKEY_CTX *pctx)
{
    /*
     * it's reasonable to set NULL pctx (a.k.a clear the ctx->pctx), so
     * we have to deal with the cleanup job here.
     */
    if (!EVP_MD_CTX_test_flags(ctx, EVP_MD_CTX_FLAG_KEEP_PKEY_CTX))
        EVP_PKEY_CTX_free(ctx->pctx);

    ctx->pctx = pctx;

    if (pctx != NULL) {
        /* make sure pctx is not freed when destroying EVP_MD_CTX */
        EVP_MD_CTX_set_flags(ctx, EVP_MD_CTX_FLAG_KEEP_PKEY_CTX);
    } else {
        EVP_MD_CTX_clear_flags(ctx, EVP_MD_CTX_FLAG_KEEP_PKEY_CTX);
    }
}
#endif /* !defined(FIPS_MODULE) */

void *EVP_MD_CTX_get0_md_data(const EVP_MD_CTX *ctx)
{
    return ctx->md_data;
}

int (*EVP_MD_CTX_update_fn(EVP_MD_CTX *ctx))(EVP_MD_CTX *ctx,
                                             const void *data, size_t count)
{
    return ctx->update;
}

void EVP_MD_CTX_set_update_fn(EVP_MD_CTX *ctx,
                              int (*update) (EVP_MD_CTX *ctx,
                                             const void *data, size_t count))
{
    ctx->update = update;
}

void EVP_MD_CTX_set_flags(EVP_MD_CTX *ctx, int flags)
{
    ctx->flags |= flags;
}

void EVP_MD_CTX_clear_flags(EVP_MD_CTX *ctx, int flags)
{
    ctx->flags &= ~flags;
}

int EVP_MD_CTX_test_flags(const EVP_MD_CTX *ctx, int flags)
{
    return (ctx->flags & flags);
}

static int evp_cipher_ctx_enable_use_bits(EVP_CIPHER_CTX *ctx,
                                          unsigned int enable)
{
    OSSL_PARAM params[] = { OSSL_PARAM_END, OSSL_PARAM_END };

    params[0] = OSSL_PARAM_construct_uint(OSSL_CIPHER_PARAM_USE_BITS, &enable);
    return EVP_CIPHER_CTX_set_params(ctx, params);
}

void EVP_CIPHER_CTX_set_flags(EVP_CIPHER_CTX *ctx, int flags)
{
    int oldflags = ctx->flags;

    ctx->flags |= flags;
    if (((oldflags ^ ctx->flags) & EVP_CIPH_FLAG_LENGTH_BITS) != 0)
        evp_cipher_ctx_enable_use_bits(ctx, 1);
}

void EVP_CIPHER_CTX_clear_flags(EVP_CIPHER_CTX *ctx, int flags)
{
    int oldflags = ctx->flags;

    ctx->flags &= ~flags;
    if (((oldflags ^ ctx->flags) & EVP_CIPH_FLAG_LENGTH_BITS) != 0)
        evp_cipher_ctx_enable_use_bits(ctx, 0);
}

int EVP_CIPHER_CTX_test_flags(const EVP_CIPHER_CTX *ctx, int flags)
{
    return (ctx->flags & flags);
}

int EVP_PKEY_CTX_set_group_name(EVP_PKEY_CTX *ctx, const char *name)
{
    OSSL_PARAM params[] = { OSSL_PARAM_END, OSSL_PARAM_END };

    if (ctx == NULL || !EVP_PKEY_CTX_IS_GEN_OP(ctx)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        /* Uses the same return values as EVP_PKEY_CTX_ctrl */
        return -2;
    }

    if (name == NULL)
        return -1;

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 (char *)name, 0);
    return EVP_PKEY_CTX_set_params(ctx, params);
}

int EVP_PKEY_CTX_get_group_name(EVP_PKEY_CTX *ctx, char *name, size_t namelen)
{
    OSSL_PARAM params[] = { OSSL_PARAM_END, OSSL_PARAM_END };
    OSSL_PARAM *p = params;

    if (ctx == NULL || !EVP_PKEY_CTX_IS_GEN_OP(ctx)) {
        /* There is no legacy support for this */
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        /* Uses the same return values as EVP_PKEY_CTX_ctrl */
        return -2;
    }

    if (name == NULL)
        return -1;

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                            name, namelen);
    if (!EVP_PKEY_CTX_get_params(ctx, params))
        return -1;
    return 1;
}

/*
 * evp_pkey_keygen() abstracts from the explicit use of B<EVP_PKEY_CTX>
 * while providing a generic way of generating a new asymmetric key pair
 * of algorithm type I<name> (e.g., C<RSA> or C<EC>).
 * The library context I<libctx> and property query I<propq>
 * are used when fetching algorithms from providers.
 * The I<params> specify algorithm-specific parameters
 * such as the RSA modulus size or the name of an EC curve.
 */
static EVP_PKEY *evp_pkey_keygen(OSSL_LIB_CTX *libctx, const char *name,
                                 const char *propq, const OSSL_PARAM *params)
{
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(libctx, name, propq);

    if (ctx != NULL
            && EVP_PKEY_keygen_init(ctx) > 0
            && EVP_PKEY_CTX_set_params(ctx, params))
        (void)EVP_PKEY_generate(ctx, &pkey);

    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

EVP_PKEY *EVP_PKEY_Q_keygen(OSSL_LIB_CTX *libctx, const char *propq,
                            const char *type, ...)
{
    va_list args;
    size_t bits;
    char *name;
    OSSL_PARAM params[] = { OSSL_PARAM_END, OSSL_PARAM_END };
    EVP_PKEY *ret = NULL;

    va_start(args, type);

    if (strcasecmp(type, "RSA") == 0) {
        bits = va_arg(args, size_t);
        params[0] = OSSL_PARAM_construct_size_t(OSSL_PKEY_PARAM_RSA_BITS, &bits);
    } else if (strcasecmp(type, "EC") == 0) {
        name = va_arg(args, char *);
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                     name, 0);
    } else if (strcasecmp(type, "ED25519") != 0
               && strcasecmp(type, "X25519") != 0
               && strcasecmp(type, "ED448") != 0
               && strcasecmp(type, "X448") != 0) {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_INVALID_ARGUMENT);
        goto end;
    }
    ret = evp_pkey_keygen(libctx, type, propq, params);

 end:
    va_end(args);
    return ret;
}

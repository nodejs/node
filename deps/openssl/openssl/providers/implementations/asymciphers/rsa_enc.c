/*
 * Copyright 2019-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * RSA low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/rsa.h>
#include <openssl/params.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
/* Just for SSL_MAX_MASTER_KEY_LENGTH */
#include <openssl/prov_ssl.h>
#include "internal/constant_time.h"
#include "internal/cryptlib.h"
#include "internal/sizes.h"
#include "crypto/rsa.h"
#include "prov/provider_ctx.h"
#include "prov/implementations.h"
#include "prov/providercommon.h"
#include "prov/securitycheck.h"
#include <stdlib.h>
#include "providers/implementations/asymciphers/rsa_enc.inc"

static OSSL_FUNC_asym_cipher_newctx_fn rsa_newctx;
static OSSL_FUNC_asym_cipher_encrypt_init_fn rsa_encrypt_init;
static OSSL_FUNC_asym_cipher_encrypt_fn rsa_encrypt;
static OSSL_FUNC_asym_cipher_decrypt_init_fn rsa_decrypt_init;
static OSSL_FUNC_asym_cipher_decrypt_fn rsa_decrypt;
static OSSL_FUNC_asym_cipher_freectx_fn rsa_freectx;
static OSSL_FUNC_asym_cipher_dupctx_fn rsa_dupctx;
static OSSL_FUNC_asym_cipher_get_ctx_params_fn rsa_get_ctx_params;
static OSSL_FUNC_asym_cipher_gettable_ctx_params_fn rsa_gettable_ctx_params;
static OSSL_FUNC_asym_cipher_set_ctx_params_fn rsa_set_ctx_params;
static OSSL_FUNC_asym_cipher_settable_ctx_params_fn rsa_settable_ctx_params;

static OSSL_ITEM padding_item[] = {
    { RSA_PKCS1_PADDING,        OSSL_PKEY_RSA_PAD_MODE_PKCSV15 },
    { RSA_NO_PADDING,           OSSL_PKEY_RSA_PAD_MODE_NONE },
    { RSA_PKCS1_OAEP_PADDING,   OSSL_PKEY_RSA_PAD_MODE_OAEP }, /* Correct spelling first */
    { RSA_PKCS1_OAEP_PADDING,   "oeap"   },
    { 0,                        NULL     }
};

/*
 * What's passed as an actual key is defined by the KEYMGMT interface.
 * We happen to know that our KEYMGMT simply passes RSA structures, so
 * we use that here too.
 */

typedef struct {
    OSSL_LIB_CTX *libctx;
    RSA *rsa;
    int pad_mode;
    int operation;
    /* OAEP message digest */
    EVP_MD *oaep_md;
    /* message digest for MGF1 */
    EVP_MD *mgf1_md;
    /* OAEP label */
    unsigned char *oaep_label;
    size_t oaep_labellen;
    /* TLS padding */
    unsigned int client_version;
    unsigned int alt_version;
    /* PKCS#1 v1.5 decryption mode */
    unsigned int implicit_rejection;
    OSSL_FIPS_IND_DECLARE
} PROV_RSA_CTX;

static void *rsa_newctx(void *provctx)
{
    PROV_RSA_CTX *prsactx;

    if (!ossl_prov_is_running())
        return NULL;
    prsactx = OPENSSL_zalloc(sizeof(PROV_RSA_CTX));
    if (prsactx == NULL)
        return NULL;
    prsactx->libctx = PROV_LIBCTX_OF(provctx);
    OSSL_FIPS_IND_INIT(prsactx)

    return prsactx;
}

static int rsa_init(void *vprsactx, void *vrsa, const OSSL_PARAM params[],
                    int operation, const char *desc)
{
    PROV_RSA_CTX *prsactx = (PROV_RSA_CTX *)vprsactx;
    int protect = 0;

    if (!ossl_prov_is_running() || prsactx == NULL || vrsa == NULL)
        return 0;

    if (!ossl_rsa_key_op_get_protect(vrsa, operation, &protect))
        return 0;
    if (!RSA_up_ref(vrsa))
        return 0;
    RSA_free(prsactx->rsa);
    prsactx->rsa = vrsa;
    prsactx->operation = operation;
    prsactx->implicit_rejection = 1;

    switch (RSA_test_flags(prsactx->rsa, RSA_FLAG_TYPE_MASK)) {
    case RSA_FLAG_TYPE_RSA:
        prsactx->pad_mode = RSA_PKCS1_PADDING;
        break;
    default:
        /* This should not happen due to the check above */
        ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    OSSL_FIPS_IND_SET_APPROVED(prsactx)
    if (!rsa_set_ctx_params(prsactx, params))
        return 0;
#ifdef FIPS_MODULE
    if (!ossl_fips_ind_rsa_key_check(OSSL_FIPS_IND_GET(prsactx),
                                     OSSL_FIPS_IND_SETTABLE0, prsactx->libctx,
                                     prsactx->rsa, desc, protect))
        return 0;
#endif
    return 1;
}

static int rsa_encrypt_init(void *vprsactx, void *vrsa,
                            const OSSL_PARAM params[])
{
    return rsa_init(vprsactx, vrsa, params, EVP_PKEY_OP_ENCRYPT,
                    "RSA Encrypt Init");
}

static int rsa_decrypt_init(void *vprsactx, void *vrsa,
                            const OSSL_PARAM params[])
{
    return rsa_init(vprsactx, vrsa, params, EVP_PKEY_OP_DECRYPT,
                    "RSA Decrypt Init");
}

static int rsa_encrypt(void *vprsactx, unsigned char *out, size_t *outlen,
                       size_t outsize, const unsigned char *in, size_t inlen)
{
    PROV_RSA_CTX *prsactx = (PROV_RSA_CTX *)vprsactx;
    size_t len = RSA_size(prsactx->rsa);
    int ret;

    if (!ossl_prov_is_running())
        return 0;

#ifdef FIPS_MODULE
    if ((prsactx->pad_mode == RSA_PKCS1_PADDING
         || prsactx->pad_mode == RSA_PKCS1_WITH_TLS_PADDING)
        && !OSSL_FIPS_IND_ON_UNAPPROVED(prsactx, OSSL_FIPS_IND_SETTABLE1,
                                        prsactx->libctx, "RSA Encrypt",
                                        "PKCS#1 v1.5 padding",
                                        ossl_fips_config_rsa_pkcs15_padding_disabled)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_PADDING_MODE);
        return 0;
    }
#endif

    if (len == 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY);
        return 0;
    }

    if (out == NULL) {
        *outlen = len;
        return 1;
    }

    if (outsize < len) {
        ERR_raise(ERR_LIB_PROV, PROV_R_OUTPUT_BUFFER_TOO_SMALL);
        return 0;
    }

    if (prsactx->pad_mode == RSA_PKCS1_OAEP_PADDING) {
        int rsasize = RSA_size(prsactx->rsa);
        unsigned char *tbuf;

        if ((tbuf = OPENSSL_malloc(rsasize)) == NULL)
            return 0;
        if (prsactx->oaep_md == NULL) {
            prsactx->oaep_md = EVP_MD_fetch(prsactx->libctx, "SHA-1", NULL);
            if (prsactx->oaep_md == NULL) {
                OPENSSL_free(tbuf);
                ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
        ret =
            ossl_rsa_padding_add_PKCS1_OAEP_mgf1_ex(prsactx->libctx, tbuf,
                                                    rsasize, in, (int)inlen,
                                                    prsactx->oaep_label,
                                                    (int)prsactx->oaep_labellen,
                                                    prsactx->oaep_md,
                                                    prsactx->mgf1_md);

        if (!ret) {
            OPENSSL_free(tbuf);
            return 0;
        }
        ret = RSA_public_encrypt(rsasize, tbuf, out, prsactx->rsa,
                                 RSA_NO_PADDING);
        OPENSSL_free(tbuf);
    } else {
        ret = RSA_public_encrypt((int)inlen, in, out, prsactx->rsa,
                                 prsactx->pad_mode);
    }
    /* A ret value of 0 is not an error */
    if (ret < 0)
        return ret;
    *outlen = ret;
    return 1;
}

static int rsa_decrypt(void *vprsactx, unsigned char *out, size_t *outlen,
                       size_t outsize, const unsigned char *in, size_t inlen)
{
    PROV_RSA_CTX *prsactx = (PROV_RSA_CTX *)vprsactx;
    int ret;
    int pad_mode;
    size_t len = RSA_size(prsactx->rsa);

    if (!ossl_prov_is_running())
        return 0;

    if (prsactx->pad_mode == RSA_PKCS1_WITH_TLS_PADDING) {
        if (out == NULL) {
            *outlen = SSL_MAX_MASTER_KEY_LENGTH;
            return 1;
        }
        if (outsize < SSL_MAX_MASTER_KEY_LENGTH) {
            ERR_raise(ERR_LIB_PROV, PROV_R_BAD_LENGTH);
            return 0;
        }
    } else {
        if (out == NULL) {
            if (len == 0) {
                ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY);
                return 0;
            }
            *outlen = len;
            return 1;
        }

        if (outsize < len) {
            ERR_raise(ERR_LIB_PROV, PROV_R_BAD_LENGTH);
            return 0;
        }
    }

    if (prsactx->pad_mode == RSA_PKCS1_OAEP_PADDING
            || prsactx->pad_mode == RSA_PKCS1_WITH_TLS_PADDING) {
        unsigned char *tbuf;

        if ((tbuf = OPENSSL_malloc(len)) == NULL)
            return 0;
        ret = RSA_private_decrypt((int)inlen, in, tbuf, prsactx->rsa,
                                  RSA_NO_PADDING);
        /*
         * With no padding then, on success ret should be len, otherwise an
         * error occurred (non-constant time)
         */
        if (ret != (int)len) {
            OPENSSL_free(tbuf);
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_DECRYPT);
            return 0;
        }
        if (prsactx->pad_mode == RSA_PKCS1_OAEP_PADDING) {
            if (prsactx->oaep_md == NULL) {
                prsactx->oaep_md = EVP_MD_fetch(prsactx->libctx, "SHA-1", NULL);
                if (prsactx->oaep_md == NULL) {
                    OPENSSL_free(tbuf);
                    ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
                    return 0;
                }
            }
            ret = RSA_padding_check_PKCS1_OAEP_mgf1(out, (int)outsize, tbuf,
                                                    (int)len, (int)len,
                                                    prsactx->oaep_label,
                                                    (int)prsactx->oaep_labellen,
                                                    prsactx->oaep_md,
                                                    prsactx->mgf1_md);
        } else {
            /* RSA_PKCS1_WITH_TLS_PADDING */
            if (prsactx->client_version <= 0) {
                ERR_raise(ERR_LIB_PROV, PROV_R_BAD_TLS_CLIENT_VERSION);
                OPENSSL_free(tbuf);
                return 0;
            }
            ret = ossl_rsa_padding_check_PKCS1_type_2_TLS(
                        prsactx->libctx, out, outsize, tbuf, len,
                        prsactx->client_version, prsactx->alt_version);
        }
        OPENSSL_free(tbuf);
    } else {
        if ((prsactx->implicit_rejection == 0) &&
                (prsactx->pad_mode == RSA_PKCS1_PADDING))
            pad_mode = RSA_PKCS1_NO_IMPLICIT_REJECT_PADDING;
        else
            pad_mode = prsactx->pad_mode;
        ret = RSA_private_decrypt((int)inlen, in, out, prsactx->rsa, pad_mode);
    }
    *outlen = constant_time_select_s(constant_time_msb_s(ret), *outlen, ret);
    ret = constant_time_select_int(constant_time_msb(ret), 0, 1);
    return ret;
}

static void rsa_freectx(void *vprsactx)
{
    PROV_RSA_CTX *prsactx = (PROV_RSA_CTX *)vprsactx;

    RSA_free(prsactx->rsa);

    EVP_MD_free(prsactx->oaep_md);
    EVP_MD_free(prsactx->mgf1_md);
    OPENSSL_free(prsactx->oaep_label);

    OPENSSL_free(prsactx);
}

static void *rsa_dupctx(void *vprsactx)
{
    PROV_RSA_CTX *srcctx = (PROV_RSA_CTX *)vprsactx;
    PROV_RSA_CTX *dstctx;

    if (!ossl_prov_is_running())
        return NULL;

    dstctx = OPENSSL_zalloc(sizeof(*srcctx));
    if (dstctx == NULL)
        return NULL;

    *dstctx = *srcctx;
    if (dstctx->rsa != NULL && !RSA_up_ref(dstctx->rsa)) {
        OPENSSL_free(dstctx);
        return NULL;
    }

    if (dstctx->oaep_md != NULL && !EVP_MD_up_ref(dstctx->oaep_md)) {
        RSA_free(dstctx->rsa);
        OPENSSL_free(dstctx);
        return NULL;
    }

    if (dstctx->mgf1_md != NULL && !EVP_MD_up_ref(dstctx->mgf1_md)) {
        RSA_free(dstctx->rsa);
        EVP_MD_free(dstctx->oaep_md);
        OPENSSL_free(dstctx);
        return NULL;
    }

    return dstctx;
}

static int rsa_get_ctx_params(void *vprsactx, OSSL_PARAM *params)
{
    PROV_RSA_CTX *prsactx = (PROV_RSA_CTX *)vprsactx;
    struct rsa_get_ctx_params_st p;

    if (prsactx == NULL || !rsa_get_ctx_params_decoder(params, &p))
        return 0;

    if (p.pad != NULL) {
        if (p.pad->data_type != OSSL_PARAM_UTF8_STRING) {
            /* Support for legacy pad mode number */
            if (!OSSL_PARAM_set_int(p.pad, prsactx->pad_mode))
                return 0;
        } else {
            int i;
            const char *word = NULL;

            for (i = 0; padding_item[i].id != 0; i++) {
                if (prsactx->pad_mode == (int)padding_item[i].id) {
                    word = padding_item[i].ptr;
                    break;
                }
            }

            if (word != NULL) {
                if (!OSSL_PARAM_set_utf8_string(p.pad, word))
                    return 0;
            } else {
                ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
            }
        }
    }

    if (p.oaep != NULL && !OSSL_PARAM_set_utf8_string(p.oaep, prsactx->oaep_md == NULL
                                                              ? ""
                                                              : EVP_MD_get0_name(prsactx->oaep_md)))
        return 0;

    if (p.mgf1 != NULL) {
        EVP_MD *mgf1_md = prsactx->mgf1_md == NULL ? prsactx->oaep_md
                                                   : prsactx->mgf1_md;

        if (!OSSL_PARAM_set_utf8_string(p.mgf1, mgf1_md == NULL
                                                ? ""
                                                : EVP_MD_get0_name(mgf1_md)))
        return 0;
    }

    if (p.label != NULL
            && !OSSL_PARAM_set_octet_ptr(p.label, prsactx->oaep_label,
                                         prsactx->oaep_labellen))
        return 0;

    if (p.tlsver != NULL
            && !OSSL_PARAM_set_uint(p.tlsver, prsactx->client_version))
        return 0;

    if (p.negver != NULL
            && !OSSL_PARAM_set_uint(p.negver, prsactx->alt_version))
        return 0;

    if (p.imrej != NULL
            && !OSSL_PARAM_set_uint(p.imrej, prsactx->implicit_rejection))
        return 0;

    if (!OSSL_FIPS_IND_GET_CTX_FROM_PARAM(prsactx, p.ind))
        return 0;

    return 1;
}

static const OSSL_PARAM *rsa_gettable_ctx_params(ossl_unused void *vprsactx,
                                                 ossl_unused void *provctx)
{
    return rsa_get_ctx_params_list;
}

static int rsa_set_ctx_params(void *vprsactx, const OSSL_PARAM params[])
{
    PROV_RSA_CTX *prsactx = (PROV_RSA_CTX *)vprsactx;
    struct rsa_set_ctx_params_st p;
    char mdname[OSSL_MAX_NAME_SIZE];
    char mdprops[OSSL_MAX_PROPQUERY_SIZE] = { '\0' };
    char *str = NULL;

    if (prsactx == NULL || !rsa_set_ctx_params_decoder(params, &p))
        return 0;

    if (!OSSL_FIPS_IND_SET_CTX_FROM_PARAM(prsactx, OSSL_FIPS_IND_SETTABLE0, p.ind_k))
        return 0;
    if (!OSSL_FIPS_IND_SET_CTX_FROM_PARAM(prsactx, OSSL_FIPS_IND_SETTABLE1, p.ind_pad))
        return 0;

    if (p.oaep != NULL) {
        str = mdname;
        if (!OSSL_PARAM_get_utf8_string(p.oaep, &str, sizeof(mdname)))
            return 0;

        if (p.oaep_pq != NULL) {
            str = mdprops;
            if (!OSSL_PARAM_get_utf8_string(p.oaep_pq, &str, sizeof(mdprops)))
                return 0;
        }

        EVP_MD_free(prsactx->oaep_md);
        prsactx->oaep_md = EVP_MD_fetch(prsactx->libctx, mdname, mdprops);

        if (prsactx->oaep_md == NULL)
            return 0;
    }

    if (p.pad != NULL) {
        int pad_mode = 0;

        if (p.pad->data_type != OSSL_PARAM_UTF8_STRING) {
            /* Support for legacy pad mode as a number */
            if (!OSSL_PARAM_get_int(p.pad, &pad_mode))
                return 0;
        } else {
            int i;

            if (p.pad->data == NULL)
                return 0;

            for (i = 0; padding_item[i].id != 0; i++) {
                if (strcmp(p.pad->data, padding_item[i].ptr) == 0) {
                    pad_mode = padding_item[i].id;
                    break;
                }
            }
        }

        /*
         * PSS padding is for signatures only so is not compatible with
         * asymmetric cipher use.
         */
        if (pad_mode == RSA_PKCS1_PSS_PADDING)
            return 0;
        if (pad_mode == RSA_PKCS1_OAEP_PADDING && prsactx->oaep_md == NULL) {
            prsactx->oaep_md = EVP_MD_fetch(prsactx->libctx, "SHA1", mdprops);
            if (prsactx->oaep_md == NULL)
                return 0;
        }
        prsactx->pad_mode = pad_mode;
    }

    if (p.mgf1 != NULL) {
        str = mdname;
        if (!OSSL_PARAM_get_utf8_string(p.mgf1, &str, sizeof(mdname)))
            return 0;

        if (p.mgf1_pq != NULL) {
            str = mdprops;
            if (!OSSL_PARAM_get_utf8_string(p.mgf1_pq, &str, sizeof(mdprops)))
                return 0;
        } else {
            str = NULL;
        }

        EVP_MD_free(prsactx->mgf1_md);
        prsactx->mgf1_md = EVP_MD_fetch(prsactx->libctx, mdname, str);

        if (prsactx->mgf1_md == NULL)
            return 0;
    }

    if (p.label != NULL) {
        void *tmp_label = NULL;
        size_t tmp_labellen;

        if (!OSSL_PARAM_get_octet_string(p.label, &tmp_label, 0, &tmp_labellen))
            return 0;
        OPENSSL_free(prsactx->oaep_label);
        prsactx->oaep_label = (unsigned char *)tmp_label;
        prsactx->oaep_labellen = tmp_labellen;
    }

    if (p.tlsver != NULL) {
        unsigned int client_version;

        if (!OSSL_PARAM_get_uint(p.tlsver, &client_version))
            return 0;
        prsactx->client_version = client_version;
    }

    if (p.negver != NULL) {
        unsigned int alt_version;

        if (!OSSL_PARAM_get_uint(p.negver, &alt_version))
            return 0;
        prsactx->alt_version = alt_version;
    }

    if (p.imrej != NULL) {
        unsigned int implicit_rejection;

        if (!OSSL_PARAM_get_uint(p.imrej, &implicit_rejection))
            return 0;
        prsactx->implicit_rejection = implicit_rejection;
    }
    return 1;
}

static const OSSL_PARAM *rsa_settable_ctx_params(ossl_unused void *vprsactx,
                                                 ossl_unused void *provctx)
{
    return rsa_set_ctx_params_list;
}

const OSSL_DISPATCH ossl_rsa_asym_cipher_functions[] = {
    { OSSL_FUNC_ASYM_CIPHER_NEWCTX, (void (*)(void))rsa_newctx },
    { OSSL_FUNC_ASYM_CIPHER_ENCRYPT_INIT, (void (*)(void))rsa_encrypt_init },
    { OSSL_FUNC_ASYM_CIPHER_ENCRYPT, (void (*)(void))rsa_encrypt },
    { OSSL_FUNC_ASYM_CIPHER_DECRYPT_INIT, (void (*)(void))rsa_decrypt_init },
    { OSSL_FUNC_ASYM_CIPHER_DECRYPT, (void (*)(void))rsa_decrypt },
    { OSSL_FUNC_ASYM_CIPHER_FREECTX, (void (*)(void))rsa_freectx },
    { OSSL_FUNC_ASYM_CIPHER_DUPCTX, (void (*)(void))rsa_dupctx },
    { OSSL_FUNC_ASYM_CIPHER_GET_CTX_PARAMS,
      (void (*)(void))rsa_get_ctx_params },
    { OSSL_FUNC_ASYM_CIPHER_GETTABLE_CTX_PARAMS,
      (void (*)(void))rsa_gettable_ctx_params },
    { OSSL_FUNC_ASYM_CIPHER_SET_CTX_PARAMS,
      (void (*)(void))rsa_set_ctx_params },
    { OSSL_FUNC_ASYM_CIPHER_SETTABLE_CTX_PARAMS,
      (void (*)(void))rsa_settable_ctx_params },
    OSSL_DISPATCH_END
};

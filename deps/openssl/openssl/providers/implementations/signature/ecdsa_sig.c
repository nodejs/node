/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
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

#include <string.h> /* memcpy */
#include <openssl/crypto.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/dsa.h>
#include <openssl/params.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include "internal/nelem.h"
#include "internal/sizes.h"
#include "internal/cryptlib.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/provider_ctx.h"
#include "prov/securitycheck.h"
#include "crypto/ec.h"
#include "prov/der_ec.h"

static OSSL_FUNC_signature_newctx_fn ecdsa_newctx;
static OSSL_FUNC_signature_sign_init_fn ecdsa_sign_init;
static OSSL_FUNC_signature_verify_init_fn ecdsa_verify_init;
static OSSL_FUNC_signature_sign_fn ecdsa_sign;
static OSSL_FUNC_signature_verify_fn ecdsa_verify;
static OSSL_FUNC_signature_digest_sign_init_fn ecdsa_digest_sign_init;
static OSSL_FUNC_signature_digest_sign_update_fn ecdsa_digest_signverify_update;
static OSSL_FUNC_signature_digest_sign_final_fn ecdsa_digest_sign_final;
static OSSL_FUNC_signature_digest_verify_init_fn ecdsa_digest_verify_init;
static OSSL_FUNC_signature_digest_verify_update_fn ecdsa_digest_signverify_update;
static OSSL_FUNC_signature_digest_verify_final_fn ecdsa_digest_verify_final;
static OSSL_FUNC_signature_freectx_fn ecdsa_freectx;
static OSSL_FUNC_signature_dupctx_fn ecdsa_dupctx;
static OSSL_FUNC_signature_get_ctx_params_fn ecdsa_get_ctx_params;
static OSSL_FUNC_signature_gettable_ctx_params_fn ecdsa_gettable_ctx_params;
static OSSL_FUNC_signature_set_ctx_params_fn ecdsa_set_ctx_params;
static OSSL_FUNC_signature_settable_ctx_params_fn ecdsa_settable_ctx_params;
static OSSL_FUNC_signature_get_ctx_md_params_fn ecdsa_get_ctx_md_params;
static OSSL_FUNC_signature_gettable_ctx_md_params_fn ecdsa_gettable_ctx_md_params;
static OSSL_FUNC_signature_set_ctx_md_params_fn ecdsa_set_ctx_md_params;
static OSSL_FUNC_signature_settable_ctx_md_params_fn ecdsa_settable_ctx_md_params;

/*
 * What's passed as an actual key is defined by the KEYMGMT interface.
 * We happen to know that our KEYMGMT simply passes DSA structures, so
 * we use that here too.
 */

typedef struct {
    OSSL_LIB_CTX *libctx;
    char *propq;
    EC_KEY *ec;
    char mdname[OSSL_MAX_NAME_SIZE];

    /*
     * Flag to determine if the hash function can be changed (1) or not (0)
     * Because it's dangerous to change during a DigestSign or DigestVerify
     * operation, this flag is cleared by their Init function, and set again
     * by their Final function.
     */
    unsigned int flag_allow_md : 1;

    /* The Algorithm Identifier of the combined signature algorithm */
    unsigned char aid_buf[OSSL_MAX_ALGORITHM_ID_SIZE];
    unsigned char *aid;
    size_t  aid_len;
    size_t mdsize;
    int operation;

    EVP_MD *md;
    EVP_MD_CTX *mdctx;
    /*
     * Internally used to cache the results of calling the EC group
     * sign_setup() methods which are then passed to the sign operation.
     * This is used by CAVS failure tests to terminate a loop if the signature
     * is not valid.
     * This could of also been done with a simple flag.
     */
    BIGNUM *kinv;
    BIGNUM *r;
#if !defined(OPENSSL_NO_ACVP_TESTS)
    /*
     * This indicates that KAT (CAVS) test is running. Externally an app will
     * override the random callback such that the generated private key and k
     * are known.
     * Normal operation will loop to choose a new k if the signature is not
     * valid - but for this mode of operation it forces a failure instead.
     */
    unsigned int kattest;
#endif
} PROV_ECDSA_CTX;

static void *ecdsa_newctx(void *provctx, const char *propq)
{
    PROV_ECDSA_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = OPENSSL_zalloc(sizeof(PROV_ECDSA_CTX));
    if (ctx == NULL)
        return NULL;

    ctx->flag_allow_md = 1;
    ctx->libctx = PROV_LIBCTX_OF(provctx);
    if (propq != NULL && (ctx->propq = OPENSSL_strdup(propq)) == NULL) {
        OPENSSL_free(ctx);
        ctx = NULL;
        ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
    }
    return ctx;
}

static int ecdsa_signverify_init(void *vctx, void *ec,
                                 const OSSL_PARAM params[], int operation)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (!ossl_prov_is_running()
            || ctx == NULL)
        return 0;

    if (ec == NULL && ctx->ec == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_NO_KEY_SET);
        return 0;
    }

    if (ec != NULL) {
        if (!ossl_ec_check_key(ctx->libctx, ec, operation == EVP_PKEY_OP_SIGN))
            return 0;
        if (!EC_KEY_up_ref(ec))
            return 0;
        EC_KEY_free(ctx->ec);
        ctx->ec = ec;
    }

    ctx->operation = operation;

    if (!ecdsa_set_ctx_params(ctx, params))
        return 0;

    return 1;
}

static int ecdsa_sign_init(void *vctx, void *ec, const OSSL_PARAM params[])
{
    return ecdsa_signverify_init(vctx, ec, params, EVP_PKEY_OP_SIGN);
}

static int ecdsa_verify_init(void *vctx, void *ec, const OSSL_PARAM params[])
{
    return ecdsa_signverify_init(vctx, ec, params, EVP_PKEY_OP_VERIFY);
}

static int ecdsa_sign(void *vctx, unsigned char *sig, size_t *siglen,
                      size_t sigsize, const unsigned char *tbs, size_t tbslen)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;
    int ret;
    unsigned int sltmp;
    size_t ecsize = ECDSA_size(ctx->ec);

    if (!ossl_prov_is_running())
        return 0;

    if (sig == NULL) {
        *siglen = ecsize;
        return 1;
    }

#if !defined(OPENSSL_NO_ACVP_TESTS)
    if (ctx->kattest && !ECDSA_sign_setup(ctx->ec, NULL, &ctx->kinv, &ctx->r))
        return 0;
#endif

    if (sigsize < (size_t)ecsize)
        return 0;

    if (ctx->mdsize != 0 && tbslen != ctx->mdsize)
        return 0;

    ret = ECDSA_sign_ex(0, tbs, tbslen, sig, &sltmp, ctx->kinv, ctx->r, ctx->ec);
    if (ret <= 0)
        return 0;

    *siglen = sltmp;
    return 1;
}

static int ecdsa_verify(void *vctx, const unsigned char *sig, size_t siglen,
                        const unsigned char *tbs, size_t tbslen)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (!ossl_prov_is_running() || (ctx->mdsize != 0 && tbslen != ctx->mdsize))
        return 0;

    return ECDSA_verify(0, tbs, tbslen, sig, siglen, ctx->ec);
}

static int ecdsa_setup_md(PROV_ECDSA_CTX *ctx, const char *mdname,
                          const char *mdprops)
{
    EVP_MD *md = NULL;
    size_t mdname_len;
    int md_nid, sha1_allowed;
    WPACKET pkt;

    if (mdname == NULL)
        return 1;

    mdname_len = strlen(mdname);
    if (mdname_len >= sizeof(ctx->mdname)) {
        ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_DIGEST,
                       "%s exceeds name buffer length", mdname);
        return 0;
    }
    if (mdprops == NULL)
        mdprops = ctx->propq;
    md = EVP_MD_fetch(ctx->libctx, mdname, mdprops);
    if (md == NULL) {
        ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_DIGEST,
                       "%s could not be fetched", mdname);
        return 0;
    }
    sha1_allowed = (ctx->operation != EVP_PKEY_OP_SIGN);
    md_nid = ossl_digest_get_approved_nid_with_sha1(ctx->libctx, md,
                                                    sha1_allowed);
    if (md_nid < 0) {
        ERR_raise_data(ERR_LIB_PROV, PROV_R_DIGEST_NOT_ALLOWED,
                       "digest=%s", mdname);
        EVP_MD_free(md);
        return 0;
    }

    if (!ctx->flag_allow_md) {
        if (ctx->mdname[0] != '\0' && !EVP_MD_is_a(md, ctx->mdname)) {
            ERR_raise_data(ERR_LIB_PROV, PROV_R_DIGEST_NOT_ALLOWED,
                           "digest %s != %s", mdname, ctx->mdname);
            EVP_MD_free(md);
            return 0;
        }
        EVP_MD_free(md);
        return 1;
    }

    EVP_MD_CTX_free(ctx->mdctx);
    EVP_MD_free(ctx->md);

    ctx->aid_len = 0;
    if (WPACKET_init_der(&pkt, ctx->aid_buf, sizeof(ctx->aid_buf))
        && ossl_DER_w_algorithmIdentifier_ECDSA_with_MD(&pkt, -1, ctx->ec,
                                                        md_nid)
        && WPACKET_finish(&pkt)) {
        WPACKET_get_total_written(&pkt, &ctx->aid_len);
        ctx->aid = WPACKET_get_curr(&pkt);
    }
    WPACKET_cleanup(&pkt);
    ctx->mdctx = NULL;
    ctx->md = md;
    ctx->mdsize = EVP_MD_get_size(ctx->md);
    OPENSSL_strlcpy(ctx->mdname, mdname, sizeof(ctx->mdname));

    return 1;
}

static int ecdsa_digest_signverify_init(void *vctx, const char *mdname,
                                        void *ec, const OSSL_PARAM params[],
                                        int operation)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (!ossl_prov_is_running())
        return 0;

    if (!ecdsa_signverify_init(vctx, ec, params, operation)
        || !ecdsa_setup_md(ctx, mdname, NULL))
        return 0;

    ctx->flag_allow_md = 0;

    if (ctx->mdctx == NULL) {
        ctx->mdctx = EVP_MD_CTX_new();
        if (ctx->mdctx == NULL)
            goto error;
    }

    if (!EVP_DigestInit_ex2(ctx->mdctx, ctx->md, params))
        goto error;
    return 1;
error:
    EVP_MD_CTX_free(ctx->mdctx);
    ctx->mdctx = NULL;
    return 0;
}

static int ecdsa_digest_sign_init(void *vctx, const char *mdname, void *ec,
                                  const OSSL_PARAM params[])
{
    return ecdsa_digest_signverify_init(vctx, mdname, ec, params,
                                        EVP_PKEY_OP_SIGN);
}

static int ecdsa_digest_verify_init(void *vctx, const char *mdname, void *ec,
                                    const OSSL_PARAM params[])
{
    return ecdsa_digest_signverify_init(vctx, mdname, ec, params,
                                        EVP_PKEY_OP_VERIFY);
}

int ecdsa_digest_signverify_update(void *vctx, const unsigned char *data,
                                   size_t datalen)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (ctx == NULL || ctx->mdctx == NULL)
        return 0;

    return EVP_DigestUpdate(ctx->mdctx, data, datalen);
}

int ecdsa_digest_sign_final(void *vctx, unsigned char *sig, size_t *siglen,
                            size_t sigsize)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int dlen = 0;

    if (!ossl_prov_is_running() || ctx == NULL || ctx->mdctx == NULL)
        return 0;

    /*
     * If sig is NULL then we're just finding out the sig size. Other fields
     * are ignored. Defer to ecdsa_sign.
     */
    if (sig != NULL
        && !EVP_DigestFinal_ex(ctx->mdctx, digest, &dlen))
        return 0;
    ctx->flag_allow_md = 1;
    return ecdsa_sign(vctx, sig, siglen, sigsize, digest, (size_t)dlen);
}

int ecdsa_digest_verify_final(void *vctx, const unsigned char *sig,
                              size_t siglen)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int dlen = 0;

    if (!ossl_prov_is_running() || ctx == NULL || ctx->mdctx == NULL)
        return 0;

    if (!EVP_DigestFinal_ex(ctx->mdctx, digest, &dlen))
        return 0;
    ctx->flag_allow_md = 1;
    return ecdsa_verify(ctx, sig, siglen, digest, (size_t)dlen);
}

static void ecdsa_freectx(void *vctx)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    OPENSSL_free(ctx->propq);
    EVP_MD_CTX_free(ctx->mdctx);
    EVP_MD_free(ctx->md);
    ctx->propq = NULL;
    ctx->mdctx = NULL;
    ctx->md = NULL;
    ctx->mdsize = 0;
    EC_KEY_free(ctx->ec);
    BN_clear_free(ctx->kinv);
    BN_clear_free(ctx->r);
    OPENSSL_free(ctx);
}

static void *ecdsa_dupctx(void *vctx)
{
    PROV_ECDSA_CTX *srcctx = (PROV_ECDSA_CTX *)vctx;
    PROV_ECDSA_CTX *dstctx;

    if (!ossl_prov_is_running())
        return NULL;

    dstctx = OPENSSL_zalloc(sizeof(*srcctx));
    if (dstctx == NULL)
        return NULL;

    *dstctx = *srcctx;
    dstctx->ec = NULL;
    dstctx->md = NULL;
    dstctx->mdctx = NULL;
    dstctx->propq = NULL;

    if (srcctx->ec != NULL && !EC_KEY_up_ref(srcctx->ec))
        goto err;
    /* Test KATS should not need to be supported */
    if (srcctx->kinv != NULL || srcctx->r != NULL)
        goto err;
    dstctx->ec = srcctx->ec;

    if (srcctx->md != NULL && !EVP_MD_up_ref(srcctx->md))
        goto err;
    dstctx->md = srcctx->md;

    if (srcctx->mdctx != NULL) {
        dstctx->mdctx = EVP_MD_CTX_new();
        if (dstctx->mdctx == NULL
                || !EVP_MD_CTX_copy_ex(dstctx->mdctx, srcctx->mdctx))
            goto err;
    }

    if (srcctx->propq != NULL) {
        dstctx->propq = OPENSSL_strdup(srcctx->propq);
        if (dstctx->propq == NULL)
            goto err;
    }

    return dstctx;
 err:
    ecdsa_freectx(dstctx);
    return NULL;
}

static int ecdsa_get_ctx_params(void *vctx, OSSL_PARAM *params)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;
    OSSL_PARAM *p;

    if (ctx == NULL)
        return 0;

    p = OSSL_PARAM_locate(params, OSSL_SIGNATURE_PARAM_ALGORITHM_ID);
    if (p != NULL && !OSSL_PARAM_set_octet_string(p, ctx->aid, ctx->aid_len))
        return 0;

    p = OSSL_PARAM_locate(params, OSSL_SIGNATURE_PARAM_DIGEST_SIZE);
    if (p != NULL && !OSSL_PARAM_set_size_t(p, ctx->mdsize))
        return 0;

    p = OSSL_PARAM_locate(params, OSSL_SIGNATURE_PARAM_DIGEST);
    if (p != NULL && !OSSL_PARAM_set_utf8_string(p, ctx->md == NULL
                                                    ? ctx->mdname
                                                    : EVP_MD_get0_name(ctx->md)))
        return 0;

    return 1;
}

static const OSSL_PARAM known_gettable_ctx_params[] = {
    OSSL_PARAM_octet_string(OSSL_SIGNATURE_PARAM_ALGORITHM_ID, NULL, 0),
    OSSL_PARAM_size_t(OSSL_SIGNATURE_PARAM_DIGEST_SIZE, NULL),
    OSSL_PARAM_utf8_string(OSSL_SIGNATURE_PARAM_DIGEST, NULL, 0),
    OSSL_PARAM_END
};

static const OSSL_PARAM *ecdsa_gettable_ctx_params(ossl_unused void *vctx,
                                                   ossl_unused void *provctx)
{
    return known_gettable_ctx_params;
}

static int ecdsa_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;
    const OSSL_PARAM *p;
    size_t mdsize = 0;

    if (ctx == NULL)
        return 0;
    if (params == NULL)
        return 1;

#if !defined(OPENSSL_NO_ACVP_TESTS)
    p = OSSL_PARAM_locate_const(params, OSSL_SIGNATURE_PARAM_KAT);
    if (p != NULL && !OSSL_PARAM_get_uint(p, &ctx->kattest))
        return 0;
#endif

    p = OSSL_PARAM_locate_const(params, OSSL_SIGNATURE_PARAM_DIGEST);
    if (p != NULL) {
        char mdname[OSSL_MAX_NAME_SIZE] = "", *pmdname = mdname;
        char mdprops[OSSL_MAX_PROPQUERY_SIZE] = "", *pmdprops = mdprops;
        const OSSL_PARAM *propsp =
            OSSL_PARAM_locate_const(params,
                                    OSSL_SIGNATURE_PARAM_PROPERTIES);

        if (!OSSL_PARAM_get_utf8_string(p, &pmdname, sizeof(mdname)))
            return 0;
        if (propsp != NULL
            && !OSSL_PARAM_get_utf8_string(propsp, &pmdprops, sizeof(mdprops)))
            return 0;
        if (!ecdsa_setup_md(ctx, mdname, mdprops))
            return 0;
    }

    p = OSSL_PARAM_locate_const(params, OSSL_SIGNATURE_PARAM_DIGEST_SIZE);
    if (p != NULL) {
        if (!OSSL_PARAM_get_size_t(p, &mdsize)
            || (!ctx->flag_allow_md && mdsize != ctx->mdsize))
            return 0;
        ctx->mdsize = mdsize;
    }

    return 1;
}

static const OSSL_PARAM settable_ctx_params[] = {
    OSSL_PARAM_utf8_string(OSSL_SIGNATURE_PARAM_DIGEST, NULL, 0),
    OSSL_PARAM_size_t(OSSL_SIGNATURE_PARAM_DIGEST_SIZE, NULL),
    OSSL_PARAM_utf8_string(OSSL_SIGNATURE_PARAM_PROPERTIES, NULL, 0),
    OSSL_PARAM_uint(OSSL_SIGNATURE_PARAM_KAT, NULL),
    OSSL_PARAM_END
};

static const OSSL_PARAM settable_ctx_params_no_digest[] = {
    OSSL_PARAM_uint(OSSL_SIGNATURE_PARAM_KAT, NULL),
    OSSL_PARAM_END
};

static const OSSL_PARAM *ecdsa_settable_ctx_params(void *vctx,
                                                   ossl_unused void *provctx)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (ctx != NULL && !ctx->flag_allow_md)
        return settable_ctx_params_no_digest;
    return settable_ctx_params;
}

static int ecdsa_get_ctx_md_params(void *vctx, OSSL_PARAM *params)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (ctx->mdctx == NULL)
        return 0;

    return EVP_MD_CTX_get_params(ctx->mdctx, params);
}

static const OSSL_PARAM *ecdsa_gettable_ctx_md_params(void *vctx)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (ctx->md == NULL)
        return 0;

    return EVP_MD_gettable_ctx_params(ctx->md);
}

static int ecdsa_set_ctx_md_params(void *vctx, const OSSL_PARAM params[])
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (ctx->mdctx == NULL)
        return 0;

    return EVP_MD_CTX_set_params(ctx->mdctx, params);
}

static const OSSL_PARAM *ecdsa_settable_ctx_md_params(void *vctx)
{
    PROV_ECDSA_CTX *ctx = (PROV_ECDSA_CTX *)vctx;

    if (ctx->md == NULL)
        return 0;

    return EVP_MD_settable_ctx_params(ctx->md);
}

const OSSL_DISPATCH ossl_ecdsa_signature_functions[] = {
    { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))ecdsa_newctx },
    { OSSL_FUNC_SIGNATURE_SIGN_INIT, (void (*)(void))ecdsa_sign_init },
    { OSSL_FUNC_SIGNATURE_SIGN, (void (*)(void))ecdsa_sign },
    { OSSL_FUNC_SIGNATURE_VERIFY_INIT, (void (*)(void))ecdsa_verify_init },
    { OSSL_FUNC_SIGNATURE_VERIFY, (void (*)(void))ecdsa_verify },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN_INIT,
      (void (*)(void))ecdsa_digest_sign_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN_UPDATE,
      (void (*)(void))ecdsa_digest_signverify_update },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN_FINAL,
      (void (*)(void))ecdsa_digest_sign_final },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_INIT,
      (void (*)(void))ecdsa_digest_verify_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_UPDATE,
      (void (*)(void))ecdsa_digest_signverify_update },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_FINAL,
      (void (*)(void))ecdsa_digest_verify_final },
    { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))ecdsa_freectx },
    { OSSL_FUNC_SIGNATURE_DUPCTX, (void (*)(void))ecdsa_dupctx },
    { OSSL_FUNC_SIGNATURE_GET_CTX_PARAMS, (void (*)(void))ecdsa_get_ctx_params },
    { OSSL_FUNC_SIGNATURE_GETTABLE_CTX_PARAMS,
      (void (*)(void))ecdsa_gettable_ctx_params },
    { OSSL_FUNC_SIGNATURE_SET_CTX_PARAMS, (void (*)(void))ecdsa_set_ctx_params },
    { OSSL_FUNC_SIGNATURE_SETTABLE_CTX_PARAMS,
      (void (*)(void))ecdsa_settable_ctx_params },
    { OSSL_FUNC_SIGNATURE_GET_CTX_MD_PARAMS,
      (void (*)(void))ecdsa_get_ctx_md_params },
    { OSSL_FUNC_SIGNATURE_GETTABLE_CTX_MD_PARAMS,
      (void (*)(void))ecdsa_gettable_ctx_md_params },
    { OSSL_FUNC_SIGNATURE_SET_CTX_MD_PARAMS,
      (void (*)(void))ecdsa_set_ctx_md_params },
    { OSSL_FUNC_SIGNATURE_SETTABLE_CTX_MD_PARAMS,
      (void (*)(void))ecdsa_settable_ctx_md_params },
    { 0, NULL }
};

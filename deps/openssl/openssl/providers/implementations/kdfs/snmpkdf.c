/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/sha.h>
#include <openssl/core_names.h>
#include <openssl/proverr.h>
#include "internal/cryptlib.h"
#include "internal/fips.h"
#include "prov/provider_ctx.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/provider_util.h"
#include "providers/implementations/kdfs/snmpkdf.inc"

#define KDF_SNMP_PASSWORD_HASH_AMOUNT (1024 * 1024)
#define KDF_SNMP_MIN_PASSWORD_LEN 8

/* See RFC 3414, Appendix A.2.2 */
/* See NIST SP800-135 Section 6.8 */
static OSSL_FUNC_kdf_newctx_fn kdf_snmpkdf_new;
static OSSL_FUNC_kdf_dupctx_fn kdf_snmpkdf_dup;
static OSSL_FUNC_kdf_freectx_fn kdf_snmpkdf_free;
static OSSL_FUNC_kdf_reset_fn kdf_snmpkdf_reset;
static OSSL_FUNC_kdf_derive_fn kdf_snmpkdf_derive;
static OSSL_FUNC_kdf_settable_ctx_params_fn kdf_snmpkdf_settable_ctx_params;
static OSSL_FUNC_kdf_set_ctx_params_fn kdf_snmpkdf_set_ctx_params;
static OSSL_FUNC_kdf_gettable_ctx_params_fn kdf_snmpkdf_gettable_ctx_params;
static OSSL_FUNC_kdf_get_ctx_params_fn kdf_snmpkdf_get_ctx_params;

static int SNMPKDF(const EVP_MD *evp_md,
    const unsigned char *eid, size_t eid_len,
    unsigned char *password, size_t password_len,
    unsigned char *key, size_t keylen);

typedef struct {
    /* Warning: Any changes to this structure may require you to update kdf_snmpkdf_dup */
    void *provctx;
    PROV_DIGEST digest;
    unsigned char *eid;
    size_t eid_len;
    unsigned char *password;
    size_t password_len;
} KDF_SNMPKDF;

static void *kdf_snmpkdf_new(void *provctx)
{
    KDF_SNMPKDF *ctx;

    if (!ossl_prov_is_running())
        return NULL;

#ifdef FIPS_MODULE
    if (!ossl_deferred_self_test(PROV_LIBCTX_OF(provctx),
            ST_ID_KDF_SNMPKDF))
        return NULL;
#endif

    if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) != NULL)
        ctx->provctx = provctx;
    return ctx;
}

static void *kdf_snmpkdf_dup(void *vctx)
{
    const KDF_SNMPKDF *src = (const KDF_SNMPKDF *)vctx;
    KDF_SNMPKDF *dest;

    dest = kdf_snmpkdf_new(src->provctx);
    if (dest != NULL) {
        if (!ossl_prov_memdup(src->eid, src->eid_len,
                &dest->eid, &dest->eid_len)
            || !ossl_prov_memdup(src->password, src->password_len,
                &dest->password, &dest->password_len)
            || !ossl_prov_digest_copy(&dest->digest, &src->digest))
            goto err;
    }
    return dest;

err:
    kdf_snmpkdf_free(dest);
    return NULL;
}

static void kdf_snmpkdf_free(void *vctx)
{
    KDF_SNMPKDF *ctx = (KDF_SNMPKDF *)vctx;

    if (ctx != NULL) {
        kdf_snmpkdf_reset(ctx);
        OPENSSL_free(ctx);
    }
}

static void kdf_snmpkdf_reset(void *vctx)
{
    KDF_SNMPKDF *ctx = (KDF_SNMPKDF *)vctx;
    void *provctx = ctx->provctx;

    ossl_prov_digest_reset(&ctx->digest);
    OPENSSL_clear_free(ctx->eid, ctx->eid_len);
    OPENSSL_clear_free(ctx->password, ctx->password_len);
    memset(ctx, 0, sizeof(*ctx));
    ctx->provctx = provctx;
}

static int snmpkdf_set_membuf(unsigned char **dst, size_t *dst_len,
    const OSSL_PARAM *p)
{
    OPENSSL_clear_free(*dst, *dst_len);
    *dst = NULL;
    *dst_len = 0;
    return OSSL_PARAM_get_octet_string(p, (void **)dst, 0, dst_len);
}

static int kdf_snmpkdf_derive(void *vctx, unsigned char *key, size_t keylen,
    const OSSL_PARAM params[])
{
    KDF_SNMPKDF *ctx = (KDF_SNMPKDF *)vctx;
    const EVP_MD *md;

    if (!ossl_prov_is_running() || !kdf_snmpkdf_set_ctx_params(ctx, params))
        return 0;

    md = ossl_prov_digest_md(&ctx->digest);
    if (md == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
        return 0;
    }
    if (ctx->eid == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_EID);
        return 0;
    }
    if (ctx->password == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_PASS);
        return 0;
    }

    return SNMPKDF(md, ctx->eid, ctx->eid_len,
        ctx->password, ctx->password_len,
        key, keylen);
}

static int kdf_snmpkdf_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    struct snmp_set_ctx_params_st p;
    KDF_SNMPKDF *ctx = vctx;
    OSSL_LIB_CTX *libctx;
#ifdef FIPS_MODULE
    const EVP_MD *md = NULL;
#endif

    if (params == NULL)
        return 1;

    if (ctx == NULL || !snmp_set_ctx_params_decoder(params, &p))
        return 0;

    libctx = PROV_LIBCTX_OF(ctx->provctx);
    if (p.digest != NULL) {
        if (!ossl_prov_digest_load(&ctx->digest, p.digest, p.propq, libctx))
            return 0;
#ifdef FIPS_MODULE
        md = ossl_prov_digest_md(&ctx->digest);
        if (!EVP_MD_is_a(md, SN_sha1)
            && !EVP_MD_is_a(md, SN_sha224)
            && !EVP_MD_is_a(md, SN_sha256)
            && !EVP_MD_is_a(md, SN_sha384)
            && !EVP_MD_is_a(md, SN_sha512))
            return 0;
#endif
    }

    if (p.pw != NULL) {
        if (!snmpkdf_set_membuf(&ctx->password, &ctx->password_len, p.pw))
            return 0;
        if ((ctx->password_len > KDF_SNMP_PASSWORD_HASH_AMOUNT) || (ctx->password_len < KDF_SNMP_MIN_PASSWORD_LEN))
            return 0;
    }

    if (p.eid != NULL && !snmpkdf_set_membuf(&ctx->eid, &ctx->eid_len, p.eid))
        return 0;

    return 1;
}

static const OSSL_PARAM *kdf_snmpkdf_settable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *p_ctx)
{
    return snmp_set_ctx_params_list;
}

static size_t kdf_snmpkdf_size(KDF_SNMPKDF *ctx)
{
    int len;
    const EVP_MD *md = NULL;

    md = ossl_prov_digest_md(&ctx->digest);
    if (md == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
        return 0;
    }
    len = EVP_MD_get_size(md);
    return (len <= 0) ? 0 : (size_t)len;
}

static int kdf_snmpkdf_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    struct snmp_get_ctx_params_st p;
    KDF_SNMPKDF *ctx = vctx;

    if (ctx == NULL || !snmp_get_ctx_params_decoder(params, &p))
        return 0;

    if (p.size != NULL) {
        size_t sz = kdf_snmpkdf_size(ctx);

        if (sz == 0)
            return 0;
        if (!OSSL_PARAM_set_size_t(p.size, sz))
            return 0;
    }
    return 1;
}

static const OSSL_PARAM *kdf_snmpkdf_gettable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *p_ctx)
{
    return snmp_get_ctx_params_list;
}

const OSSL_DISPATCH ossl_kdf_snmpkdf_functions[] = {
    { OSSL_FUNC_KDF_NEWCTX, (void (*)(void))kdf_snmpkdf_new },
    { OSSL_FUNC_KDF_DUPCTX, (void (*)(void))kdf_snmpkdf_dup },
    { OSSL_FUNC_KDF_FREECTX, (void (*)(void))kdf_snmpkdf_free },
    { OSSL_FUNC_KDF_RESET, (void (*)(void))kdf_snmpkdf_reset },
    { OSSL_FUNC_KDF_DERIVE, (void (*)(void))kdf_snmpkdf_derive },
    { OSSL_FUNC_KDF_SETTABLE_CTX_PARAMS,
        (void (*)(void))kdf_snmpkdf_settable_ctx_params },
    { OSSL_FUNC_KDF_SET_CTX_PARAMS, (void (*)(void))kdf_snmpkdf_set_ctx_params },
    { OSSL_FUNC_KDF_GETTABLE_CTX_PARAMS,
        (void (*)(void))kdf_snmpkdf_gettable_ctx_params },
    { OSSL_FUNC_KDF_GET_CTX_PARAMS, (void (*)(void))kdf_snmpkdf_get_ctx_params },
    { 0, NULL }
};

/*
 * SNMPKDF - In compliance with SP800-135 and RFC7860, calculate
 *           a master key using the engine ID and password.
 *
 * Denote engineLength and passwordlen to be the lengths (in bytes) of an
 * snmpEngineID and a password, respectively.
 *
 * Let N = (1024*1024)/passwordlen
 *
 * Expanded_password = the leftmost 1048576 bytes of the string of N
 * repetitions of the password.
 *
 * Derived_password = SHA-1 (Expanded_password). The Derived_password
 * is the output of hashing the Expanded_password by SHA-1.
 *
 * Let Shared_key to be the key that the user shares with the authoritative
 * SNMP engine with ID snmpEngineID. The Shared_key is generated as follows:
 *
 * Shared_key = SHA-1(Derived_password || snmpEngineID || Derived_password).
 *
 * Input:
 *     e_id -         engine ID(eid)
 *     e_len -        engineID length
 *     password -     password
 *     password_len - password length
 *     okey -         pointer to key output, FIPS testing limited to SHA-1.
 *     keylen -       key length
 * Output:
 *     okey   - filled with derived key
 *     return - 1 on pass, 0 fail
 */
static int SNMPKDF(const EVP_MD *evp_md,
    const unsigned char *e_id, size_t e_len,
    unsigned char *password, size_t password_len,
    unsigned char *okey, size_t keylen)
{
    EVP_MD_CTX *md = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE];
    size_t mdsize = 0, len = 0;
    unsigned int md_len = 0;
    int ret = 0;
    int value = 0;

    /* Limited to SHA-1 and SHA-2 hashes presently */
    if (okey == NULL || keylen == 0)
        return 0;

    md = EVP_MD_CTX_new();
    if (md == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
        goto err;
    }

    value = EVP_MD_get_size(evp_md);
    if (value <= 0 || (size_t)value > keylen)
        goto err;
    mdsize = (size_t)value;

    if (!EVP_DigestInit_ex(md, evp_md, NULL))
        goto err;

    for (len = 0; len < KDF_SNMP_PASSWORD_HASH_AMOUNT - password_len; len += password_len) {
        if (!EVP_DigestUpdate(md, password, password_len))
            goto err;
    }

    if (!EVP_DigestUpdate(md, password, KDF_SNMP_PASSWORD_HASH_AMOUNT - len)
        || !EVP_DigestFinal_ex(md, digest, &md_len)
        || !EVP_DigestInit_ex(md, evp_md, NULL)
        || !EVP_DigestUpdate(md, digest, mdsize)
        || !EVP_DigestUpdate(md, e_id, e_len)
        || !EVP_DigestUpdate(md, digest, mdsize)
        || !EVP_DigestFinal_ex(md, digest, &md_len))
        goto err;

    memcpy(okey, digest, (keylen < md_len) ? keylen : md_len);

    ret = 1;

err:
    EVP_MD_CTX_free(md);
    OPENSSL_cleanse(digest, EVP_MAX_MD_SIZE);
    return ret;
}

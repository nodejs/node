/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/params.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include "internal/nelem.h"
#include "internal/sizes.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/provider_ctx.h"
#include "prov/der_ecx.h"
#include "crypto/ecx.h"

#ifdef S390X_EC_ASM
# include "s390x_arch.h"

# define S390X_CAN_SIGN(edtype)                                                \
((OPENSSL_s390xcap_P.pcc[1] & S390X_CAPBIT(S390X_SCALAR_MULTIPLY_##edtype))    \
&& (OPENSSL_s390xcap_P.kdsa[0] & S390X_CAPBIT(S390X_EDDSA_SIGN_##edtype))      \
&& (OPENSSL_s390xcap_P.kdsa[0] & S390X_CAPBIT(S390X_EDDSA_VERIFY_##edtype)))

static int s390x_ed25519_digestsign(const ECX_KEY *edkey, unsigned char *sig,
                                    const unsigned char *tbs, size_t tbslen);
static int s390x_ed448_digestsign(const ECX_KEY *edkey, unsigned char *sig,
                                  const unsigned char *tbs, size_t tbslen);
static int s390x_ed25519_digestverify(const ECX_KEY *edkey,
                                      const unsigned char *sig,
                                      const unsigned char *tbs, size_t tbslen);
static int s390x_ed448_digestverify(const ECX_KEY *edkey,
                                    const unsigned char *sig,
                                    const unsigned char *tbs, size_t tbslen);

#endif /* S390X_EC_ASM */

static OSSL_FUNC_signature_newctx_fn eddsa_newctx;
static OSSL_FUNC_signature_digest_sign_init_fn eddsa_digest_signverify_init;
static OSSL_FUNC_signature_digest_sign_fn ed25519_digest_sign;
static OSSL_FUNC_signature_digest_sign_fn ed448_digest_sign;
static OSSL_FUNC_signature_digest_verify_fn ed25519_digest_verify;
static OSSL_FUNC_signature_digest_verify_fn ed448_digest_verify;
static OSSL_FUNC_signature_freectx_fn eddsa_freectx;
static OSSL_FUNC_signature_dupctx_fn eddsa_dupctx;
static OSSL_FUNC_signature_get_ctx_params_fn eddsa_get_ctx_params;
static OSSL_FUNC_signature_gettable_ctx_params_fn eddsa_gettable_ctx_params;

typedef struct {
    OSSL_LIB_CTX *libctx;
    ECX_KEY *key;

    /* The Algorithm Identifier of the signature algorithm */
    unsigned char aid_buf[OSSL_MAX_ALGORITHM_ID_SIZE];
    unsigned char *aid;
    size_t  aid_len;
} PROV_EDDSA_CTX;

static void *eddsa_newctx(void *provctx, const char *propq_unused)
{
    PROV_EDDSA_CTX *peddsactx;

    if (!ossl_prov_is_running())
        return NULL;

    peddsactx = OPENSSL_zalloc(sizeof(PROV_EDDSA_CTX));
    if (peddsactx == NULL) {
        ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    peddsactx->libctx = PROV_LIBCTX_OF(provctx);

    return peddsactx;
}

static int eddsa_digest_signverify_init(void *vpeddsactx, const char *mdname,
                                        void *vedkey,
                                        ossl_unused const OSSL_PARAM params[])
{
    PROV_EDDSA_CTX *peddsactx = (PROV_EDDSA_CTX *)vpeddsactx;
    ECX_KEY *edkey = (ECX_KEY *)vedkey;
    WPACKET pkt;
    int ret;

    if (!ossl_prov_is_running())
        return 0;

    if (mdname != NULL && mdname[0] != '\0') {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_DIGEST);
        return 0;
    }

    if (edkey == NULL) {
        if (peddsactx->key != NULL)
            /* there is nothing to do on reinit */
            return 1;
        ERR_raise(ERR_LIB_PROV, PROV_R_NO_KEY_SET);
        return 0;
    }

    if (!ossl_ecx_key_up_ref(edkey)) {
        ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    /*
     * We do not care about DER writing errors.
     * All it really means is that for some reason, there's no
     * AlgorithmIdentifier to be had, but the operation itself is
     * still valid, just as long as it's not used to construct
     * anything that needs an AlgorithmIdentifier.
     */
    peddsactx->aid_len = 0;
    ret = WPACKET_init_der(&pkt, peddsactx->aid_buf, sizeof(peddsactx->aid_buf));
    switch (edkey->type) {
    case ECX_KEY_TYPE_ED25519:
        ret = ret && ossl_DER_w_algorithmIdentifier_ED25519(&pkt, -1, edkey);
        break;
    case ECX_KEY_TYPE_ED448:
        ret = ret && ossl_DER_w_algorithmIdentifier_ED448(&pkt, -1, edkey);
        break;
    default:
        /* Should never happen */
        ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
        ossl_ecx_key_free(edkey);
        return 0;
    }
    if (ret && WPACKET_finish(&pkt)) {
        WPACKET_get_total_written(&pkt, &peddsactx->aid_len);
        peddsactx->aid = WPACKET_get_curr(&pkt);
    }
    WPACKET_cleanup(&pkt);

    peddsactx->key = edkey;

    return 1;
}

int ed25519_digest_sign(void *vpeddsactx, unsigned char *sigret,
                        size_t *siglen, size_t sigsize,
                        const unsigned char *tbs, size_t tbslen)
{
    PROV_EDDSA_CTX *peddsactx = (PROV_EDDSA_CTX *)vpeddsactx;
    const ECX_KEY *edkey = peddsactx->key;

    if (!ossl_prov_is_running())
        return 0;

    if (sigret == NULL) {
        *siglen = ED25519_SIGSIZE;
        return 1;
    }
    if (sigsize < ED25519_SIGSIZE) {
        ERR_raise(ERR_LIB_PROV, PROV_R_OUTPUT_BUFFER_TOO_SMALL);
        return 0;
    }
#ifdef S390X_EC_ASM
    if (S390X_CAN_SIGN(ED25519))
        return s390x_ed25519_digestsign(edkey, sigret, tbs, tbslen);
#endif /* S390X_EC_ASM */
    if (ossl_ed25519_sign(sigret, tbs, tbslen, edkey->pubkey, edkey->privkey,
                          peddsactx->libctx, NULL) == 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SIGN);
        return 0;
    }
    *siglen = ED25519_SIGSIZE;
    return 1;
}

int ed448_digest_sign(void *vpeddsactx, unsigned char *sigret,
                      size_t *siglen, size_t sigsize,
                      const unsigned char *tbs, size_t tbslen)
{
    PROV_EDDSA_CTX *peddsactx = (PROV_EDDSA_CTX *)vpeddsactx;
    const ECX_KEY *edkey = peddsactx->key;

    if (!ossl_prov_is_running())
        return 0;

    if (sigret == NULL) {
        *siglen = ED448_SIGSIZE;
        return 1;
    }
    if (sigsize < ED448_SIGSIZE) {
        ERR_raise(ERR_LIB_PROV, PROV_R_OUTPUT_BUFFER_TOO_SMALL);
        return 0;
    }
#ifdef S390X_EC_ASM
    if (S390X_CAN_SIGN(ED448))
        return s390x_ed448_digestsign(edkey, sigret, tbs, tbslen);
#endif /* S390X_EC_ASM */
    if (ossl_ed448_sign(peddsactx->libctx, sigret, tbs, tbslen, edkey->pubkey,
                        edkey->privkey, NULL, 0, edkey->propq) == 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SIGN);
        return 0;
    }
    *siglen = ED448_SIGSIZE;
    return 1;
}

int ed25519_digest_verify(void *vpeddsactx, const unsigned char *sig,
                          size_t siglen, const unsigned char *tbs,
                          size_t tbslen)
{
    PROV_EDDSA_CTX *peddsactx = (PROV_EDDSA_CTX *)vpeddsactx;
    const ECX_KEY *edkey = peddsactx->key;

    if (!ossl_prov_is_running() || siglen != ED25519_SIGSIZE)
        return 0;

#ifdef S390X_EC_ASM
    if (S390X_CAN_SIGN(ED25519))
        return s390x_ed25519_digestverify(edkey, sig, tbs, tbslen);
#endif /* S390X_EC_ASM */

    return ossl_ed25519_verify(tbs, tbslen, sig, edkey->pubkey,
                               peddsactx->libctx, edkey->propq);
}

int ed448_digest_verify(void *vpeddsactx, const unsigned char *sig,
                        size_t siglen, const unsigned char *tbs,
                        size_t tbslen)
{
    PROV_EDDSA_CTX *peddsactx = (PROV_EDDSA_CTX *)vpeddsactx;
    const ECX_KEY *edkey = peddsactx->key;

    if (!ossl_prov_is_running() || siglen != ED448_SIGSIZE)
        return 0;

#ifdef S390X_EC_ASM
    if (S390X_CAN_SIGN(ED448))
        return s390x_ed448_digestverify(edkey, sig, tbs, tbslen);
#endif /* S390X_EC_ASM */

    return ossl_ed448_verify(peddsactx->libctx, tbs, tbslen, sig, edkey->pubkey,
                             NULL, 0, edkey->propq);
}

static void eddsa_freectx(void *vpeddsactx)
{
    PROV_EDDSA_CTX *peddsactx = (PROV_EDDSA_CTX *)vpeddsactx;

    ossl_ecx_key_free(peddsactx->key);

    OPENSSL_free(peddsactx);
}

static void *eddsa_dupctx(void *vpeddsactx)
{
    PROV_EDDSA_CTX *srcctx = (PROV_EDDSA_CTX *)vpeddsactx;
    PROV_EDDSA_CTX *dstctx;

    if (!ossl_prov_is_running())
        return NULL;

    dstctx = OPENSSL_zalloc(sizeof(*srcctx));
    if (dstctx == NULL)
        return NULL;

    *dstctx = *srcctx;
    dstctx->key = NULL;

    if (srcctx->key != NULL && !ossl_ecx_key_up_ref(srcctx->key)) {
        ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    dstctx->key = srcctx->key;

    return dstctx;
 err:
    eddsa_freectx(dstctx);
    return NULL;
}

static int eddsa_get_ctx_params(void *vpeddsactx, OSSL_PARAM *params)
{
    PROV_EDDSA_CTX *peddsactx = (PROV_EDDSA_CTX *)vpeddsactx;
    OSSL_PARAM *p;

    if (peddsactx == NULL)
        return 0;

    p = OSSL_PARAM_locate(params, OSSL_SIGNATURE_PARAM_ALGORITHM_ID);
    if (p != NULL && !OSSL_PARAM_set_octet_string(p, peddsactx->aid,
                                                  peddsactx->aid_len))
        return 0;

    return 1;
}

static const OSSL_PARAM known_gettable_ctx_params[] = {
    OSSL_PARAM_octet_string(OSSL_SIGNATURE_PARAM_ALGORITHM_ID, NULL, 0),
    OSSL_PARAM_END
};

static const OSSL_PARAM *eddsa_gettable_ctx_params(ossl_unused void *vpeddsactx,
                                                   ossl_unused void *provctx)
{
    return known_gettable_ctx_params;
}

const OSSL_DISPATCH ossl_ed25519_signature_functions[] = {
    { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))eddsa_newctx },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN_INIT,
      (void (*)(void))eddsa_digest_signverify_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN,
      (void (*)(void))ed25519_digest_sign },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_INIT,
      (void (*)(void))eddsa_digest_signverify_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY,
      (void (*)(void))ed25519_digest_verify },
    { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))eddsa_freectx },
    { OSSL_FUNC_SIGNATURE_DUPCTX, (void (*)(void))eddsa_dupctx },
    { OSSL_FUNC_SIGNATURE_GET_CTX_PARAMS, (void (*)(void))eddsa_get_ctx_params },
    { OSSL_FUNC_SIGNATURE_GETTABLE_CTX_PARAMS,
      (void (*)(void))eddsa_gettable_ctx_params },
    { 0, NULL }
};

const OSSL_DISPATCH ossl_ed448_signature_functions[] = {
    { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))eddsa_newctx },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN_INIT,
      (void (*)(void))eddsa_digest_signverify_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN,
      (void (*)(void))ed448_digest_sign },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_INIT,
      (void (*)(void))eddsa_digest_signverify_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY,
      (void (*)(void))ed448_digest_verify },
    { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))eddsa_freectx },
    { OSSL_FUNC_SIGNATURE_DUPCTX, (void (*)(void))eddsa_dupctx },
    { OSSL_FUNC_SIGNATURE_GET_CTX_PARAMS, (void (*)(void))eddsa_get_ctx_params },
    { OSSL_FUNC_SIGNATURE_GETTABLE_CTX_PARAMS,
      (void (*)(void))eddsa_gettable_ctx_params },
    { 0, NULL }
};

#ifdef S390X_EC_ASM

static int s390x_ed25519_digestsign(const ECX_KEY *edkey, unsigned char *sig,
                                    const unsigned char *tbs, size_t tbslen)
{
    int rc;
    union {
        struct {
            unsigned char sig[64];
            unsigned char priv[32];
        } ed25519;
        unsigned long long buff[512];
    } param;

    memset(&param, 0, sizeof(param));
    memcpy(param.ed25519.priv, edkey->privkey, sizeof(param.ed25519.priv));

    rc = s390x_kdsa(S390X_EDDSA_SIGN_ED25519, &param.ed25519, tbs, tbslen);
    OPENSSL_cleanse(param.ed25519.priv, sizeof(param.ed25519.priv));
    if (rc != 0)
        return 0;

    s390x_flip_endian32(sig, param.ed25519.sig);
    s390x_flip_endian32(sig + 32, param.ed25519.sig + 32);
    return 1;
}

static int s390x_ed448_digestsign(const ECX_KEY *edkey, unsigned char *sig,
                                  const unsigned char *tbs, size_t tbslen)
{
    int rc;
    union {
        struct {
            unsigned char sig[128];
            unsigned char priv[64];
        } ed448;
        unsigned long long buff[512];
    } param;

    memset(&param, 0, sizeof(param));
    memcpy(param.ed448.priv + 64 - 57, edkey->privkey, 57);

    rc = s390x_kdsa(S390X_EDDSA_SIGN_ED448, &param.ed448, tbs, tbslen);
    OPENSSL_cleanse(param.ed448.priv, sizeof(param.ed448.priv));
    if (rc != 0)
        return 0;

    s390x_flip_endian64(param.ed448.sig, param.ed448.sig);
    s390x_flip_endian64(param.ed448.sig + 64, param.ed448.sig + 64);
    memcpy(sig, param.ed448.sig, 57);
    memcpy(sig + 57, param.ed448.sig + 64, 57);
    return 1;
}

static int s390x_ed25519_digestverify(const ECX_KEY *edkey,
                                      const unsigned char *sig,
                                      const unsigned char *tbs, size_t tbslen)
{
    union {
        struct {
            unsigned char sig[64];
            unsigned char pub[32];
        } ed25519;
        unsigned long long buff[512];
    } param;

    memset(&param, 0, sizeof(param));
    s390x_flip_endian32(param.ed25519.sig, sig);
    s390x_flip_endian32(param.ed25519.sig + 32, sig + 32);
    s390x_flip_endian32(param.ed25519.pub, edkey->pubkey);

    return s390x_kdsa(S390X_EDDSA_VERIFY_ED25519,
                      &param.ed25519, tbs, tbslen) == 0 ? 1 : 0;
}

static int s390x_ed448_digestverify(const ECX_KEY *edkey,
                                    const unsigned char *sig,
                                    const unsigned char *tbs,
                                    size_t tbslen)
{
    union {
        struct {
            unsigned char sig[128];
            unsigned char pub[64];
        } ed448;
        unsigned long long buff[512];
    } param;

    memset(&param, 0, sizeof(param));
    memcpy(param.ed448.sig, sig, 57);
    s390x_flip_endian64(param.ed448.sig, param.ed448.sig);
    memcpy(param.ed448.sig + 64, sig + 57, 57);
    s390x_flip_endian64(param.ed448.sig + 64, param.ed448.sig + 64);
    memcpy(param.ed448.pub, edkey->pubkey, 57);
    s390x_flip_endian64(param.ed448.pub, param.ed448.pub);

    return s390x_kdsa(S390X_EDDSA_VERIFY_ED448,
                      &param.ed448, tbs, tbslen) == 0 ? 1 : 0;
}

#endif /* S390X_EC_ASM */

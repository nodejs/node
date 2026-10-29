/*
 * Copyright 2020-2026 The OpenSSL Project Authors. All Rights Reserved.
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

#include <string.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#ifndef FIPS_MODULE
#include <openssl/x509.h>
#include "crypto/asn1.h"
#endif
#include "internal/sizes.h"
#include "internal/param_build_set.h"
#include "crypto/rsa.h"
#include "crypto/rsa_params.h"
#include "rsa_local.h"

/*
 * The intention with the "backend" source file is to offer backend functions
 * for legacy backends (EVP_PKEY_ASN1_METHOD) and provider implementations
 * alike.
 */

DEFINE_STACK_OF(BIGNUM)

static int collect_numbers(STACK_OF(BIGNUM) *numbers,
    OSSL_PARAM *const params[], size_t num_params)
{
    size_t i;

    if (numbers == NULL)
        return 0;

    for (i = 0; i < num_params; i++) {
        if (params[i] != NULL) {
            BIGNUM *tmp = NULL;

            if (!OSSL_PARAM_get_BN(params[i], &tmp))
                return 0;
            if (sk_BIGNUM_push(numbers, tmp) == 0) {
                BN_clear_free(tmp);
                return 0;
            }
        }
    }

    return 1;
}

int ossl_rsa_fromdata_parsed(RSA *rsa, const RSA_PARAMS *p,
    int include_private)
{
    BIGNUM *n = NULL, *e = NULL, *d = NULL;
    STACK_OF(BIGNUM) *factors = NULL, *exps = NULL, *coeffs = NULL;
    int is_private = 0;
    int derive_from_pq = 0;
    BN_CTX *ctx = NULL;

    if (rsa == NULL || p == NULL)
        return 0;

    if ((p->n == NULL || !OSSL_PARAM_get_BN(p->n, &n))
        || (p->e == NULL || !OSSL_PARAM_get_BN(p->e, &e))) {
        ERR_raise(ERR_LIB_RSA, ERR_R_PASSED_NULL_PARAMETER);
        goto err;
    }

    if (include_private) {
        if ((p->derive != NULL)
            && !OSSL_PARAM_get_int(p->derive, &derive_from_pq))
            goto err;

        if (p->d != NULL && !OSSL_PARAM_get_BN(p->d, &d)) {
            ERR_raise(ERR_LIB_RSA, ERR_R_PASSED_NULL_PARAMETER);
            goto err;
        }

        if (derive_from_pq) {
            ctx = BN_CTX_new_ex(rsa->libctx);
            if (ctx == NULL)
                goto err;

            /* we need at minimum p, q */
            if (p->mp.factors[0] == NULL || p->mp.factors[1] == NULL) {
                ERR_raise(ERR_LIB_RSA, ERR_R_PASSED_NULL_PARAMETER);
                goto err;
            }
        }
    }

    is_private = (d != NULL);

    if (!RSA_set0_key(rsa, n, e, d))
        goto err;
    n = e = d = NULL;

    if (is_private) {
        if (!collect_numbers(factors = sk_BIGNUM_new_null(), p->mp.factors,
                OSSL_NELEM(p->mp.factors))
            || !collect_numbers(exps = sk_BIGNUM_new_null(), p->mp.exps,
                OSSL_NELEM(p->mp.exps))
            || !collect_numbers(coeffs = sk_BIGNUM_new_null(), p->mp.coeffs,
                OSSL_NELEM(p->mp.coeffs)))
            goto err;

        if (derive_from_pq && sk_BIGNUM_num(exps) == 0
            && sk_BIGNUM_num(coeffs) == 0) {
            /*
             * If we want to use crt to derive our exponents/coefficients, we
             * need to have at least 2 factors
             */
            if (sk_BIGNUM_num(factors) < 2) {
                ERR_raise(ERR_LIB_RSA, ERR_R_PASSED_NULL_PARAMETER);
                goto err;
            }

            /*
             * if we have more than two factors, n and d must also have
             * been provided
             */
            if (sk_BIGNUM_num(factors) > 2
                && (p->n == NULL || p->d == NULL)) {
                ERR_raise(ERR_LIB_RSA, ERR_R_PASSED_NULL_PARAMETER);
                goto err;
            }

            /* build our exponents and coefficients here */
            if (sk_BIGNUM_num(factors) == 2) {
                /* for 2 factors we can use the sp800 functions to do this */
                if (!RSA_set0_factors(rsa, sk_BIGNUM_value(factors, 0),
                        sk_BIGNUM_value(factors, 1))) {
                    ERR_raise(ERR_LIB_RSA, ERR_R_INTERNAL_ERROR);
                    goto err;
                }
                /*
                 * once consumed by RSA_set0_factors, pop those off the stack
                 * so we don't free them below
                 */
                sk_BIGNUM_pop(factors);
                sk_BIGNUM_pop(factors);

                /*
                 * Note: Because we only have 2 factors here, there will be no
                 * additional pinfo fields to hold additional factors, and
                 * since we set our key and 2 factors above we can skip
                 * the call to ossl_rsa_set0_all_params
                 */
                if (!ossl_rsa_sp800_56b_derive_params_from_pq(rsa,
                        RSA_bits(rsa),
                        NULL, ctx)) {
                    ERR_raise(ERR_LIB_RSA, ERR_R_INTERNAL_ERROR);
                    goto err;
                }
            } else {
#ifndef FIPS_MODULE
                /*
                 * in the multiprime case we have to generate exps/coeffs here
                 * for each additional prime
                 */
                if (!ossl_rsa_multiprime_derive(rsa, RSA_bits(rsa),
                        sk_BIGNUM_num(factors),
                        rsa->e, factors, exps,
                        coeffs)) {
                    ERR_raise(ERR_LIB_RSA, ERR_R_INTERNAL_ERROR);
                    goto err;
                }

                /*
                 * Now we should have all our factors, exponents and
                 * coefficients
                 */
                if (!ossl_rsa_set0_all_params(rsa, factors, exps, coeffs)) {
                    ERR_raise(ERR_LIB_RSA, ERR_R_INTERNAL_ERROR);
                    goto err;
                }

#else
                /* multiprime case is disallowed in FIPS mode, raise an error */
                ERR_raise(ERR_LIB_RSA, ERR_R_UNSUPPORTED);
                goto err;
#endif
            }

        } else {
            /*
             * It's ok if this private key just has n, e and d
             * but only if we're not using derive_from_pq
             */
            if (sk_BIGNUM_num(factors) != 0
                && !ossl_rsa_set0_all_params(rsa, factors, exps, coeffs))
                goto err;
        }
        /* sanity check to ensure we used everything in our stacks */
        if (sk_BIGNUM_num(factors) != 0
            || sk_BIGNUM_num(exps) != 0
            || sk_BIGNUM_num(coeffs) != 0) {
            ERR_raise_data(ERR_LIB_RSA, ERR_R_INTERNAL_ERROR,
                "There are %d, %d, %d elements left on our factors, exps, coeffs stacks\n",
                sk_BIGNUM_num(factors), sk_BIGNUM_num(exps),
                sk_BIGNUM_num(coeffs));
            goto err;
        }
    }

    if (!ossl_rsa_check_factors(rsa)) {
        ERR_raise_data(ERR_LIB_RSA, RSA_R_INVALID_KEYPAIR,
            "RSA factors/exponents are too big for n-modulus\n");
        goto err;
    }

    sk_BIGNUM_free(factors);
    sk_BIGNUM_free(exps);
    sk_BIGNUM_free(coeffs);
    BN_CTX_free(ctx);
    return 1;

err:
    BN_free(n);
    BN_free(e);
    BN_free(d);
    sk_BIGNUM_pop_free(factors, BN_clear_free);
    sk_BIGNUM_pop_free(exps, BN_clear_free);
    sk_BIGNUM_pop_free(coeffs, BN_clear_free);
    BN_CTX_free(ctx);
    return 0;
}

int ossl_rsa_fromdata(RSA *rsa, const OSSL_PARAM params[],
    int include_private)
{
    RSA_PARAMS p;

    if (!rsa_key_fromdata_decoder(params, &p))
        return 0;
    return ossl_rsa_fromdata_parsed(rsa, &p, include_private);
}

DEFINE_SPECIAL_STACK_OF_CONST(BIGNUM_const, BIGNUM)

static int rsa_set_multi_key_bn(OSSL_PARAM_BLD *bld,
    OSSL_PARAM *const params[], size_t num_params, const char *names[],
    STACK_OF(BIGNUM_const) *numbers)
{
    int i, num_numbers = sk_BIGNUM_const_num(numbers);

    for (i = 0; i < num_numbers && (size_t)i < num_params
        && names[i] != NULL;
        i++) {
        const BIGNUM *bn = sk_BIGNUM_const_value(numbers, i);
        OSSL_PARAM *p = bld == NULL ? params[i] : NULL;

        if (bn != NULL && !ossl_param_build_set_bn(bld, p, names[i], bn))
            return 0;
    }
    return 1;
}

int ossl_rsa_todata_parsed(RSA *rsa, OSSL_PARAM_BLD *bld,
    const RSA_PARAMS *p, int include_private)
{
    int ret = 0;
    const BIGNUM *rsa_d = NULL, *rsa_n = NULL, *rsa_e = NULL;
    STACK_OF(BIGNUM_const) *factors = sk_BIGNUM_const_new_null();
    STACK_OF(BIGNUM_const) *exps = sk_BIGNUM_const_new_null();
    STACK_OF(BIGNUM_const) *coeffs = sk_BIGNUM_const_new_null();

    if (rsa == NULL || (bld == NULL && p == NULL)
        || factors == NULL || exps == NULL || coeffs == NULL)
        goto err;

    RSA_get0_key(rsa, &rsa_n, &rsa_e, &rsa_d);
    ossl_rsa_get0_all_params(rsa, factors, exps, coeffs);

    if (!ossl_param_build_set_bn(bld, p == NULL ? NULL : p->n,
            OSSL_PKEY_PARAM_RSA_N, rsa_n)
        || !ossl_param_build_set_bn(bld, p == NULL ? NULL : p->e,
            OSSL_PKEY_PARAM_RSA_E, rsa_e))
        goto err;

    /* Check private key data integrity */
    if (include_private && rsa_d != NULL) {

        if (!ossl_param_build_set_bn(bld, p == NULL ? NULL : p->d,
                OSSL_PKEY_PARAM_RSA_D,
                rsa_d)
            || !rsa_set_multi_key_bn(bld,
                p == NULL ? NULL : p->mp.factors,
                OSSL_RSA_PARAM_MAX_PRIMES, ossl_rsa_mp_factor_names, factors)
            || !rsa_set_multi_key_bn(bld,
                p == NULL ? NULL : p->mp.exps,
                OSSL_RSA_PARAM_MAX_PRIMES, ossl_rsa_mp_exp_names, exps)
            || !rsa_set_multi_key_bn(bld,
                p == NULL ? NULL : p->mp.coeffs,
                OSSL_RSA_PARAM_MAX_PRIMES - 1,
                ossl_rsa_mp_coeff_names, coeffs))
            goto err;
    }

#if defined(FIPS_MODULE) && !defined(OPENSSL_NO_ACVP_TESTS)
    /* The acvp test results are not meant for export so check for bld == NULL */
    if (bld == NULL)
        ossl_rsa_acvp_test_get_params_parsed(rsa, p);
#endif
    ret = 1;
err:
    sk_BIGNUM_const_free(factors);
    sk_BIGNUM_const_free(exps);
    sk_BIGNUM_const_free(coeffs);
    return ret;
}

int ossl_rsa_todata(RSA *rsa, OSSL_PARAM_BLD *bld, OSSL_PARAM params[],
    int include_private)
{
    RSA_PARAMS p;

    if (params != NULL) {
        if (!rsa_key_todata_decoder(params, &p))
            return 0;
        return ossl_rsa_todata_parsed(rsa, bld, &p, include_private);
    }
    return ossl_rsa_todata_parsed(rsa, bld, NULL, include_private);
}

int ossl_rsa_pss_params_30_todata_parsed(const RSA_PSS_PARAMS_30 *pss,
    OSSL_PARAM_BLD *bld, const RSA_PARAMS *p)
{
    if (bld == NULL && p == NULL)
        return 0;

    if (!ossl_rsa_pss_params_30_is_unrestricted(pss)) {
        int hashalg_nid = ossl_rsa_pss_params_30_hashalg(pss);
        int maskgenalg_nid = ossl_rsa_pss_params_30_maskgenalg(pss);
        int maskgenhashalg_nid = ossl_rsa_pss_params_30_maskgenhashalg(pss);
        int saltlen = ossl_rsa_pss_params_30_saltlen(pss);
        int default_hashalg_nid = ossl_rsa_pss_params_30_hashalg(NULL);
        int default_maskgenalg_nid = ossl_rsa_pss_params_30_maskgenalg(NULL);
        int default_maskgenhashalg_nid = ossl_rsa_pss_params_30_maskgenhashalg(NULL);
        const char *mdname = (hashalg_nid == default_hashalg_nid
                ? NULL
                : ossl_rsa_oaeppss_nid2name(hashalg_nid));
        const char *mgfname = (maskgenalg_nid == default_maskgenalg_nid
                ? NULL
                : ossl_rsa_oaeppss_nid2name(maskgenalg_nid));
        const char *mgf1mdname = (maskgenhashalg_nid == default_maskgenhashalg_nid
                ? NULL
                : ossl_rsa_oaeppss_nid2name(maskgenhashalg_nid));
        const char *key_md = OSSL_PKEY_PARAM_RSA_DIGEST;
        const char *key_mgf = OSSL_PKEY_PARAM_RSA_MASKGENFUNC;
        const char *key_mgf1_md = OSSL_PKEY_PARAM_RSA_MGF1_DIGEST;
        const char *key_saltlen = OSSL_PKEY_PARAM_RSA_PSS_SALTLEN;

        /*
         * To ensure that the key isn't seen as unrestricted by the recipient,
         * we make sure that at least one PSS-related parameter is passed, even
         * if it has a default value; saltlen.
         */
        if ((mdname != NULL
                && !ossl_param_build_set_utf8_string(bld,
                    p == NULL ? NULL : p->digest, key_md, mdname))
            || (mgfname != NULL
                && !ossl_param_build_set_utf8_string(bld,
                    p == NULL ? NULL : p->maskgenfunc, key_mgf, mgfname))
            || (mgf1mdname != NULL
                && !ossl_param_build_set_utf8_string(bld,
                    p == NULL ? NULL : p->mgf1_digest,
                    key_mgf1_md, mgf1mdname))
            || (!ossl_param_build_set_int(bld,
                p == NULL ? NULL : p->pss_saltlen,
                key_saltlen, saltlen)))
            return 0;
    }
    return 1;
}

int ossl_rsa_pss_params_30_todata(const RSA_PSS_PARAMS_30 *pss,
    OSSL_PARAM_BLD *bld, OSSL_PARAM params[])
{
    RSA_PARAMS p;

    if (params != NULL) {
        if (!rsa_pss_todata_decoder(params, &p))
            return 0;
        return ossl_rsa_pss_params_30_todata_parsed(pss, bld, &p);
    }
    return ossl_rsa_pss_params_30_todata_parsed(pss, bld, NULL);
}

int ossl_rsa_pss_params_30_fromdata_parsed(RSA_PSS_PARAMS_30 *pss_params,
    int *defaults_set, const RSA_PARAMS *p, OSSL_LIB_CTX *libctx)
{
    const char *propq = NULL;
    EVP_MD *md = NULL, *mgf1md = NULL;
    int saltlen;
    int ret = 0;

    if (pss_params == NULL || defaults_set == NULL || p == NULL)
        return 0;

    if (p->digest_props != NULL) {
        if (p->digest_props->data_type == OSSL_PARAM_UTF8_STRING)
            propq = p->digest_props->data;
        else if (!OSSL_PARAM_get_utf8_ptr(p->digest_props, &propq))
            return 0;
    }
    /*
     * If we get any of the parameters, we know we have at least some
     * restrictions, so we start by setting default values, and let each
     * parameter override their specific restriction data.
     */
    if (!*defaults_set
        && (p->digest != NULL || p->maskgenfunc != NULL || p->mgf1_digest != NULL
            || p->pss_saltlen != NULL)) {
        if (!ossl_rsa_pss_params_30_set_defaults(pss_params))
            return 0;
        *defaults_set = 1;
    }

    if (p->maskgenfunc != NULL) {
        int default_maskgenalg_nid = ossl_rsa_pss_params_30_maskgenalg(NULL);
        const char *mgfname = NULL;

        if (p->maskgenfunc->data_type == OSSL_PARAM_UTF8_STRING)
            mgfname = p->maskgenfunc->data;
        else if (!OSSL_PARAM_get_utf8_ptr(p->maskgenfunc, &mgfname))
            return 0;

        if (mgfname == NULL
            || OPENSSL_strcasecmp(mgfname,
                   ossl_rsa_mgf_nid2name(default_maskgenalg_nid))
                != 0)
            return 0;
    }

    /*
     * We're only interested in the NIDs that correspond to the MDs, so the
     * exact propquery is unimportant in the EVP_MD_fetch() calls below.
     */

    if (p->digest != NULL) {
        const char *mdname = NULL;

        if (p->digest->data_type == OSSL_PARAM_UTF8_STRING)
            mdname = p->digest->data;
        else if (!OSSL_PARAM_get_utf8_ptr(p->digest, &mdname))
            goto err;

        if ((md = EVP_MD_fetch(libctx, mdname, propq)) == NULL
            || !ossl_rsa_pss_params_30_set_hashalg(pss_params,
                ossl_rsa_oaeppss_md2nid(md)))
            goto err;
    }

    if (p->mgf1_digest != NULL) {
        const char *mgf1mdname = NULL;

        if (p->mgf1_digest->data_type == OSSL_PARAM_UTF8_STRING)
            mgf1mdname = p->mgf1_digest->data;
        else if (!OSSL_PARAM_get_utf8_ptr(p->mgf1_digest, &mgf1mdname))
            goto err;

        if ((mgf1md = EVP_MD_fetch(libctx, mgf1mdname, propq)) == NULL
            || !ossl_rsa_pss_params_30_set_maskgenhashalg(
                pss_params, ossl_rsa_oaeppss_md2nid(mgf1md)))
            goto err;
    }

    if (p->pss_saltlen != NULL) {
        if (!OSSL_PARAM_get_int(p->pss_saltlen, &saltlen)
            || !ossl_rsa_pss_params_30_set_saltlen(pss_params, saltlen))
            goto err;
    }

    ret = 1;

err:
    EVP_MD_free(md);
    EVP_MD_free(mgf1md);
    return ret;
}

int ossl_rsa_pss_params_30_fromdata(RSA_PSS_PARAMS_30 *pss_params,
    int *defaults_set, const OSSL_PARAM params[], OSSL_LIB_CTX *libctx)
{
    RSA_PARAMS p;

    if (!rsa_pss_fromdata_decoder(params, &p))
        return 0;
    return ossl_rsa_pss_params_30_fromdata_parsed(pss_params, defaults_set,
        &p, libctx);
}

int ossl_rsa_is_foreign(const RSA *rsa)
{
#ifndef FIPS_MODULE
    if (RSA_get_method(rsa) != RSA_PKCS1_OpenSSL())
        return 1;
#endif
    return 0;
}

static ossl_inline int rsa_bn_dup_check(BIGNUM **out, const BIGNUM *f)
{
    if (f != NULL && (*out = BN_dup(f)) == NULL)
        return 0;
    return 1;
}

RSA *ossl_rsa_dup(const RSA *rsa, int selection)
{
    RSA *dupkey = NULL;
#ifndef FIPS_MODULE
    int pnum, i;
#endif

    /* Do not try to duplicate foreign RSA keys */
    if (ossl_rsa_is_foreign(rsa))
        return NULL;

    if ((dupkey = ossl_rsa_new_with_ctx(rsa->libctx)) == NULL)
        return NULL;

    /* public key */
    if ((selection & OSSL_KEYMGMT_SELECT_KEYPAIR) != 0) {
        if (!rsa_bn_dup_check(&dupkey->n, rsa->n))
            goto err;
        if (!rsa_bn_dup_check(&dupkey->e, rsa->e))
            goto err;
    }

    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0) {

        /* private key */
        if (!rsa_bn_dup_check(&dupkey->d, rsa->d))
            goto err;

        /* factors and crt params */
        if (!rsa_bn_dup_check(&dupkey->p, rsa->p))
            goto err;
        if (!rsa_bn_dup_check(&dupkey->q, rsa->q))
            goto err;
        if (!rsa_bn_dup_check(&dupkey->dmp1, rsa->dmp1))
            goto err;
        if (!rsa_bn_dup_check(&dupkey->dmq1, rsa->dmq1))
            goto err;
        if (!rsa_bn_dup_check(&dupkey->iqmp, rsa->iqmp))
            goto err;
    }

    dupkey->version = rsa->version;
    dupkey->flags = rsa->flags;
    /* we always copy the PSS parameters regardless of selection */
    dupkey->pss_params = rsa->pss_params;

#ifndef FIPS_MODULE
    /* multiprime */
    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0
        && (pnum = sk_RSA_PRIME_INFO_num(rsa->prime_infos)) > 0) {
        dupkey->prime_infos = sk_RSA_PRIME_INFO_new_reserve(NULL, pnum);
        if (dupkey->prime_infos == NULL)
            goto err;
        for (i = 0; i < pnum; i++) {
            const RSA_PRIME_INFO *pinfo = NULL;
            RSA_PRIME_INFO *duppinfo = NULL;

            if ((duppinfo = OPENSSL_zalloc(sizeof(*duppinfo))) == NULL)
                goto err;
            /* push first so cleanup in error case works */
            (void)sk_RSA_PRIME_INFO_push(dupkey->prime_infos, duppinfo);

            pinfo = sk_RSA_PRIME_INFO_value(rsa->prime_infos, i);
            if (!rsa_bn_dup_check(&duppinfo->r, pinfo->r))
                goto err;
            if (!rsa_bn_dup_check(&duppinfo->d, pinfo->d))
                goto err;
            if (!rsa_bn_dup_check(&duppinfo->t, pinfo->t))
                goto err;
        }
        if (!ossl_rsa_multip_calc_product(dupkey))
            goto err;
    }

    if (rsa->pss != NULL) {
        if ((dupkey->pss = RSA_PSS_PARAMS_dup(rsa->pss)) == NULL)
            goto err;
        if (rsa->pss->maskGenAlgorithm != NULL
            && dupkey->pss->maskGenAlgorithm == NULL) {
            dupkey->pss->maskHash = ossl_x509_algor_mgf1_decode(rsa->pss->maskGenAlgorithm);
            if (dupkey->pss->maskHash == NULL)
                goto err;
        }
    }
    if (!CRYPTO_dup_ex_data(CRYPTO_EX_INDEX_RSA,
            &dupkey->ex_data, &rsa->ex_data))
        goto err;
#endif

    return dupkey;

err:
    RSA_free(dupkey);
    return NULL;
}

#ifndef FIPS_MODULE
RSA_PSS_PARAMS *ossl_rsa_pss_decode(const X509_ALGOR *alg)
{
    RSA_PSS_PARAMS *pss;

    pss = ASN1_TYPE_unpack_sequence(ASN1_ITEM_rptr(RSA_PSS_PARAMS),
        alg->parameter);

    if (pss == NULL)
        return NULL;

    if (pss->maskGenAlgorithm != NULL) {
        pss->maskHash = ossl_x509_algor_mgf1_decode(pss->maskGenAlgorithm);
        if (pss->maskHash == NULL) {
            RSA_PSS_PARAMS_free(pss);
            return NULL;
        }
    }

    return pss;
}

static int ossl_rsa_sync_to_pss_params_30(RSA *rsa)
{
    const RSA_PSS_PARAMS *legacy_pss = NULL;
    RSA_PSS_PARAMS_30 *pss = NULL;

    if (rsa != NULL
        && (legacy_pss = RSA_get0_pss_params(rsa)) != NULL
        && (pss = ossl_rsa_get0_pss_params_30(rsa)) != NULL) {
        const EVP_MD *md = NULL, *mgf1md = NULL;
        int md_nid, mgf1md_nid, saltlen, trailerField;
        RSA_PSS_PARAMS_30 pss_params;

        /*
         * We don't care about the validity of the fields here, we just
         * want to synchronise values.  Verifying here makes it impossible
         * to even read a key with invalid values, making it hard to test
         * a bad situation.
         *
         * Other routines use ossl_rsa_pss_get_param(), so the values will
         * be checked, eventually.
         */
        if (!ossl_rsa_pss_get_param_unverified(legacy_pss, &md, &mgf1md,
                &saltlen, &trailerField))
            return 0;
        md_nid = EVP_MD_get_type(md);
        mgf1md_nid = EVP_MD_get_type(mgf1md);
        if (!ossl_rsa_pss_params_30_set_defaults(&pss_params)
            || !ossl_rsa_pss_params_30_set_hashalg(&pss_params, md_nid)
            || !ossl_rsa_pss_params_30_set_maskgenhashalg(&pss_params,
                mgf1md_nid)
            || !ossl_rsa_pss_params_30_set_saltlen(&pss_params, saltlen)
            || !ossl_rsa_pss_params_30_set_trailerfield(&pss_params,
                trailerField))
            return 0;
        *pss = pss_params;
    }
    return 1;
}

int ossl_rsa_pss_get_param_unverified(const RSA_PSS_PARAMS *pss,
    const EVP_MD **pmd, const EVP_MD **pmgf1md,
    int *psaltlen, int *ptrailerField)
{
    RSA_PSS_PARAMS_30 pss_params;

    /* Get the defaults from the ONE place */
    (void)ossl_rsa_pss_params_30_set_defaults(&pss_params);

    if (pss == NULL)
        return 0;
    *pmd = ossl_x509_algor_get_md(pss->hashAlgorithm);
    if (*pmd == NULL)
        return 0;
    *pmgf1md = ossl_x509_algor_get_md(pss->maskHash);
    if (*pmgf1md == NULL)
        return 0;
    if (pss->saltLength)
        *psaltlen = ASN1_INTEGER_get(pss->saltLength);
    else
        *psaltlen = ossl_rsa_pss_params_30_saltlen(&pss_params);
    if (pss->trailerField)
        *ptrailerField = ASN1_INTEGER_get(pss->trailerField);
    else
        *ptrailerField = ossl_rsa_pss_params_30_trailerfield(&pss_params);

    return 1;
}

int ossl_rsa_param_decode(RSA *rsa, const X509_ALGOR *alg)
{
    RSA_PSS_PARAMS *pss;
    const ASN1_OBJECT *algoid;
    const void *algp;
    int algptype;

    X509_ALGOR_get0(&algoid, &algptype, &algp, alg);
    if (OBJ_obj2nid(algoid) != EVP_PKEY_RSA_PSS)
        return 1;
    if (algptype == V_ASN1_UNDEF)
        return 1;
    if (algptype != V_ASN1_SEQUENCE) {
        ERR_raise(ERR_LIB_RSA, RSA_R_INVALID_PSS_PARAMETERS);
        return 0;
    }
    if ((pss = ossl_rsa_pss_decode(alg)) == NULL
        || !ossl_rsa_set0_pss_params(rsa, pss)) {
        RSA_PSS_PARAMS_free(pss);
        return 0;
    }
    if (!ossl_rsa_sync_to_pss_params_30(rsa))
        return 0;
    return 1;
}

RSA *ossl_rsa_key_from_pkcs8(const PKCS8_PRIV_KEY_INFO *p8inf,
    OSSL_LIB_CTX *libctx, const char *propq)
{
    const unsigned char *p;
    RSA *rsa;
    int pklen;
    const X509_ALGOR *alg;

    if (!PKCS8_pkey_get0(NULL, &p, &pklen, &alg, p8inf))
        return 0;
    rsa = d2i_RSAPrivateKey(NULL, &p, pklen);
    if (rsa == NULL) {
        ERR_raise(ERR_LIB_RSA, ERR_R_RSA_LIB);
        return NULL;
    }
    if (!ossl_rsa_param_decode(rsa, alg)) {
        RSA_free(rsa);
        return NULL;
    }

    RSA_clear_flags(rsa, RSA_FLAG_TYPE_MASK);
    switch (OBJ_obj2nid(alg->algorithm)) {
    case EVP_PKEY_RSA:
        RSA_set_flags(rsa, RSA_FLAG_TYPE_RSA);
        break;
    case EVP_PKEY_RSA_PSS:
        RSA_set_flags(rsa, RSA_FLAG_TYPE_RSASSAPSS);
        break;
    default:
        /* Leave the type bits zero */
        break;
    }

    return rsa;
}
#endif

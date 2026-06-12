/*
 * Copyright 2024-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include "internal/common.h"
#include "ml_dsa_local.h"
#include "ml_dsa_key.h"
#include "ml_dsa_matrix.h"
#include "ml_dsa_sign.h"
#include "ml_dsa_hash.h"

#define ML_DSA_MAX_LAMBDA 256 /* bit strength for ML-DSA-87 */

/*
 * @brief Initialize a Signature object by pointing all of its objects to
 * preallocated blocks. The values passed for hint, z and
 * c_tilde values are not owned/freed by the |sig| object.
 *
 * @param sig The ML_DSA_SIG to initialize.
 * @param hint A preallocated array of |k| polynomial blocks
 * @param k The number of |hint| polynomials
 * @param z A preallocated array of |l| polynomial blocks
 * @param l The number of |z| polynomials
 * @param c_tilde A preallocated buffer
 * @param c_tilde_len The size of |c_tilde|
 */
static void signature_init(ML_DSA_SIG *sig,
                           POLY *hint, uint32_t k, POLY *z, uint32_t l,
                           uint8_t *c_tilde, size_t c_tilde_len)
{
    vector_init(&sig->z, z, l);
    vector_init(&sig->hint, hint, k);
    sig->c_tilde = c_tilde;
    sig->c_tilde_len = c_tilde_len;
}

/*
 * @brief: Auxiliary functions to compute ML-DSA's MU.
 * This combines the steps of creating M' and concatenating it
 * to the Public Key Hash to obtain MU.
 * See FIPS 204 Algorithm 2 Step 10 (and algorithm 3 Step 5) as
 * well as Algorithm 7 Step 6 (and algorithm 8 Step 7)
 *
 * ML_DSA pure signatures are encoded as M' = 00 || ctx_len || ctx || msg
 * Where ctx is the empty string by default and ctx_len <= 255.
 * The message is appended to the encoded context.
 * Finally a public key hash is prepended, and the whole is hashed
 * to derive the mu value.
 *
 * @param key: A public or private ML-DSA key;
 * @param encode: if not set, assumes that M' is provided raw and the
 * following parameters are ignored.
 * @param ctx An optional context to add to the message encoding.
 * @param ctx_len The size of |ctx|. It must be in the range 0..255
 * @returns an EVP_MD_CTX if the operation is successful, NULL otherwise.
 */

EVP_MD_CTX *ossl_ml_dsa_mu_init(const ML_DSA_KEY *key, int encode,
                                const uint8_t *ctx, size_t ctx_len)
{
    EVP_MD_CTX *md_ctx;
    uint8_t itb[2];

    if (key == NULL)
        return NULL;

    md_ctx = EVP_MD_CTX_new();
    if (md_ctx == NULL)
        return NULL;

    /* H(.. */
    if (!EVP_DigestInit_ex2(md_ctx, key->shake256_md, NULL))
        goto err;
    /* ..pk (= key->tr) */
    if (!EVP_DigestUpdate(md_ctx, key->tr, sizeof(key->tr)))
        goto err;
    /* M' = .. */
    if (encode) {
        if (ctx_len > ML_DSA_MAX_CONTEXT_STRING_LEN)
            goto err;
        /* IntegerToBytes(0, 1) .. */
        itb[0] = 0;
        /* || IntegerToBytes(|ctx|, 1) || .. */
        itb[1] = (uint8_t)ctx_len;
        if (!EVP_DigestUpdate(md_ctx, itb, 2))
            goto err;
        /* ctx || .. */
        if (!EVP_DigestUpdate(md_ctx, ctx, ctx_len))
            goto err;
        /* .. msg) will follow in update and final functions */
    }

    return md_ctx;

err:
    EVP_MD_CTX_free(md_ctx);
    return NULL;
}

/*
 * @brief: updates the internal ML-DSA hash with an additional message chunk.
 *
 * @param md_ctx: The hashing context
 * @param msg: The next message chunk
 * @param msg_len: The length of the msg buffer to process
 * @returns 1 on success, 0 on error
 */
int ossl_ml_dsa_mu_update(EVP_MD_CTX *md_ctx, const uint8_t *msg, size_t msg_len)
{
    return EVP_DigestUpdate(md_ctx, msg, msg_len);
}

/*
 * @brief: finalizes the internal ML-DSA hash
 *
 * @param md_ctx: The hashing context
 * @param mu: The output buffer for Mu
 * @param mu_len: The size of the output buffer
 * @returns 1 on success, 0 on error
 */
int ossl_ml_dsa_mu_finalize(EVP_MD_CTX *md_ctx, uint8_t *mu, size_t mu_len)
{
    if (!ossl_assert(mu_len == ML_DSA_MU_BYTES)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_BAD_LENGTH);
        return 0;
    }
    return EVP_DigestSqueeze(md_ctx, mu, mu_len);
}

/*
 * @brief FIPS 204, Algorithm 7, ML-DSA.Sign_internal()
 *
 * This algorithm is decomposed in 2 steps, a set of functions to compute mu
 * and then the actual signing function.
 *
 * @param priv: The private ML-DSA key
 * @param mu: The pre-computed mu hash
 * @param mu_len: The length of the mu buffer
 * @param rnd: The random buffer
 * @param rnd_len: The length of the random buffer
 * @param out_sig: The output signature buffer
 * @returns 1 on success, 0 on error
 */
static int ml_dsa_sign_internal(const ML_DSA_KEY *priv,
                                const uint8_t *mu, size_t mu_len,
                                const uint8_t *rnd, size_t rnd_len,
                                uint8_t *out_sig)
{
    int ret = 0;
    const ML_DSA_PARAMS *params = priv->params;
    EVP_MD_CTX *md_ctx = NULL;
    uint32_t k = (uint32_t)params->k, l = (uint32_t)params->l;
    uint32_t gamma1 = params->gamma1, gamma2 = params->gamma2;
    uint8_t *alloc = NULL, *w1_encoded;
    size_t alloc_len, w1_encoded_len;
    size_t num_polys_sig_k = 2 * k;
    size_t num_polys_k = 5 * k;
    size_t num_polys_l = 3 * l;
    size_t num_polys_k_by_l = k * l;
    POLY *p, *c_ntt;
    VECTOR s1_ntt, s2_ntt, t0_ntt, w, w1, cs1, cs2, y;
    MATRIX a_ntt;
    ML_DSA_SIG sig;
    uint8_t rho_prime[ML_DSA_RHO_PRIME_BYTES];
    uint8_t c_tilde[ML_DSA_MAX_LAMBDA / 4];
    size_t c_tilde_len = params->bit_strength >> 2;
    size_t kappa;

    if (mu_len != ML_DSA_MU_BYTES) {
        ERR_raise(ERR_LIB_PROV, PROV_R_BAD_LENGTH);
        return 0;
    }

    /*
     * Allocate a single blob for most of the variable size temporary variables.
     * Mostly used for VECTOR POLYNOMIALS (every POLY is 1K).
     */
    w1_encoded_len = k * (gamma2 == ML_DSA_GAMMA2_Q_MINUS1_DIV88 ? 192 : 128);
    alloc_len = w1_encoded_len
        + sizeof(*p) * (1 + num_polys_k + num_polys_l
                        + num_polys_k_by_l + num_polys_sig_k);
    alloc = OPENSSL_malloc(alloc_len);
    if (alloc == NULL)
        return 0;
    md_ctx = EVP_MD_CTX_new();
    if (md_ctx == NULL)
        goto err;

    w1_encoded = alloc;
    /* Init the temp vectors to point to the allocated polys blob */
    p = (POLY *)(w1_encoded + w1_encoded_len);
    c_ntt = p++;
    matrix_init(&a_ntt, p, k, l);
    p += num_polys_k_by_l;
    vector_init(&s2_ntt, p, k);
    vector_init(&t0_ntt, s2_ntt.poly + k, k);
    vector_init(&w, t0_ntt.poly + k, k);
    vector_init(&w1, w.poly + k, k);
    vector_init(&cs2, w1.poly + k, k);
    p += num_polys_k;
    vector_init(&s1_ntt, p, l);
    vector_init(&y, p + l, l);
    vector_init(&cs1, p + 2 * l, l);
    p += num_polys_l;
    signature_init(&sig, p, k, p + k, l, c_tilde, c_tilde_len);
    /* End of the allocated blob setup */

    if (!matrix_expand_A(md_ctx, priv->shake128_md, priv->rho, &a_ntt))
        goto err;

    if (!shake_xof_3(md_ctx, priv->shake256_md, priv->K, sizeof(priv->K),
                     rnd, rnd_len, mu, mu_len,
                     rho_prime, sizeof(rho_prime)))
        goto err;

    vector_copy(&s1_ntt, &priv->s1);
    vector_ntt(&s1_ntt);
    vector_copy(&s2_ntt, &priv->s2);
    vector_ntt(&s2_ntt);
    vector_copy(&t0_ntt, &priv->t0);
    vector_ntt(&t0_ntt);

    /*
     * kappa must not exceed 2^16. But the probability of it
     * exceeding even 1000 iterations is vanishingly small.
     */
    for (kappa = 0; ; kappa += l) {
        VECTOR *y_ntt = &cs1;
        VECTOR *r0 = &w1;
        VECTOR *ct0 = &w1;
        uint32_t z_max, r0_max, ct0_max, h_ones;

        vector_expand_mask(&y, rho_prime, sizeof(rho_prime), (uint32_t)kappa,
                           gamma1, md_ctx, priv->shake256_md);
        vector_copy(y_ntt, &y);
        vector_ntt(y_ntt);

        matrix_mult_vector(&a_ntt, y_ntt, &w);
        vector_ntt_inverse(&w);

        vector_high_bits(&w, gamma2, &w1);
        ossl_ml_dsa_w1_encode(&w1, gamma2, w1_encoded, w1_encoded_len);

        if (!shake_xof_2(md_ctx, priv->shake256_md, mu, mu_len,
                         w1_encoded, w1_encoded_len, c_tilde, c_tilde_len))
            break;

        if (!poly_sample_in_ball_ntt(c_ntt, c_tilde, (int)c_tilde_len,
                                     md_ctx, priv->shake256_md, params->tau))
            break;

        vector_mult_scalar(&s1_ntt, c_ntt, &cs1);
        vector_ntt_inverse(&cs1);
        vector_mult_scalar(&s2_ntt, c_ntt, &cs2);
        vector_ntt_inverse(&cs2);

        vector_add(&y, &cs1, &sig.z);

        /* r0 = lowbits(w - cs2) */
        vector_sub(&w, &cs2, r0);
        vector_low_bits(r0, gamma2, r0);

        /*
         * Leaking that the signature is rejected is fine as the next attempt at a
         * signature will be (indistinguishable from) independent of this one.
         */
        z_max = vector_max(&sig.z);
        r0_max = vector_max_signed(r0);
        if (value_barrier_32(constant_time_ge(z_max, gamma1 - params->beta)
                             | constant_time_ge(r0_max, gamma2 - params->beta)))
            continue;

        vector_mult_scalar(&t0_ntt, c_ntt, ct0);
        vector_ntt_inverse(ct0);
        vector_make_hint(ct0, &cs2, &w, gamma2, &sig.hint);

        ct0_max = vector_max(ct0);
        h_ones = (uint32_t)vector_count_ones(&sig.hint);
        /* Same reasoning applies to the leak as above */
        if (value_barrier_32(constant_time_ge(ct0_max, gamma2)
                             | constant_time_lt(params->omega, h_ones)))
            continue;
        ret = ossl_ml_dsa_sig_encode(&sig, params, out_sig);
        break;
    }
err:
    EVP_MD_CTX_free(md_ctx);
    OPENSSL_clear_free(alloc, alloc_len);
    OPENSSL_cleanse(rho_prime, sizeof(rho_prime));
    return ret;
}

/*
 * @brief FIPS 204, Algorithm 8, ML-DSA.Verify_internal().
 *
 * This algorithm is decomposed in 2 steps, a set of functions to compute mu
 * and then the actual verification function.
 *
 * @param pub: The public ML-DSA key
 * @param mu: The pre-computed mu hash
 * @param mu_len: The length of the mu buffer
 * @param sig_enc: The encoded signature to be verified
 * @param sig_enc_len: the encoded csignature length
 * @returns 1 on success, 0 on error
 */
static int ml_dsa_verify_internal(const ML_DSA_KEY *pub,
                                  const uint8_t *mu, size_t mu_len,
                                  const uint8_t *sig_enc,
                                  size_t sig_enc_len)
{
    int ret = 0;
    uint8_t *alloc = NULL, *w1_encoded;
    POLY *p, *c_ntt;
    MATRIX a_ntt;
    VECTOR az_ntt, ct1_ntt, *z_ntt, *w1, *w_approx;
    ML_DSA_SIG sig;
    const ML_DSA_PARAMS *params = pub->params;
    uint32_t k = (uint32_t)pub->params->k;
    uint32_t l = (uint32_t)pub->params->l;
    uint32_t gamma2 = params->gamma2;
    size_t w1_encoded_len;
    size_t num_polys_sig = k + l;
    size_t num_polys_k = 2 * k;
    size_t num_polys_l = 1 * l;
    size_t num_polys_k_by_l = k * l;
    uint8_t c_tilde[ML_DSA_MAX_LAMBDA / 4];
    uint8_t c_tilde_sig[ML_DSA_MAX_LAMBDA / 4];
    EVP_MD_CTX *md_ctx = NULL;
    size_t c_tilde_len = params->bit_strength >> 2;
    uint32_t z_max;

    /* FIPS 204 compliance: Also validate signature length before decoding */
    if (mu_len != ML_DSA_MU_BYTES || sig_enc_len != params->sig_len) {
        ERR_raise(ERR_LIB_PROV, PROV_R_BAD_LENGTH);
        return 0;
    }


    /* Allocate space for all the POLYNOMIALS used by temporary VECTORS */
    w1_encoded_len = k * (gamma2 == ML_DSA_GAMMA2_Q_MINUS1_DIV88 ? 192 : 128);
    alloc = OPENSSL_malloc(w1_encoded_len
                           + sizeof(*p) * (1 + num_polys_k
                                           + num_polys_l
                                           + num_polys_k_by_l
                                           + num_polys_sig));
    if (alloc == NULL)
        return 0;
    md_ctx = EVP_MD_CTX_new();
    if (md_ctx == NULL)
        goto err;

    w1_encoded = alloc;
    /* Init the temp vectors to point to the allocated polys blob */
    p = (POLY *)(w1_encoded + w1_encoded_len);
    c_ntt = p++;
    matrix_init(&a_ntt, p, k, l);
    p += num_polys_k_by_l;
    signature_init(&sig, p, k, p + k, l, c_tilde_sig, c_tilde_len);
    p += num_polys_sig;
    vector_init(&az_ntt, p, k);
    vector_init(&ct1_ntt, p + k, k);

    if (!ossl_ml_dsa_sig_decode(&sig, sig_enc, sig_enc_len, pub->params)
            || !matrix_expand_A(md_ctx, pub->shake128_md, pub->rho, &a_ntt))
        goto err;

    /* Compute verifiers challenge c_ntt = NTT(SampleInBall(c_tilde)) */
    if (!poly_sample_in_ball_ntt(c_ntt, c_tilde_sig, (int)c_tilde_len,
                                 md_ctx, pub->shake256_md, params->tau))
        goto err;

    /* ct1_ntt = NTT(c) * NTT(t1 * 2^d) */
    vector_scale_power2_round_ntt(&pub->t1, &ct1_ntt);
    vector_mult_scalar(&ct1_ntt, c_ntt, &ct1_ntt);

    /* compute z_max early in order to reuse sig.z */
    z_max = vector_max(&sig.z);

    /* w_approx = NTT_inverse(A * NTT(z) - ct1_ntt) */
    z_ntt = &sig.z;
    vector_ntt(z_ntt);
    matrix_mult_vector(&a_ntt, z_ntt, &az_ntt);
    w_approx = &az_ntt;
    vector_sub(&az_ntt, &ct1_ntt, w_approx);
    vector_ntt_inverse(w_approx);

    /* compute w1_encoded */
    w1 = w_approx;
    vector_use_hint(&sig.hint, w_approx, gamma2, w1);
    ossl_ml_dsa_w1_encode(w1, gamma2, w1_encoded, w1_encoded_len);

    if (!shake_xof_3(md_ctx, pub->shake256_md, mu, mu_len,
                     w1_encoded, w1_encoded_len, NULL, 0, c_tilde, c_tilde_len))
        goto err;

    ret = (z_max < (uint32_t)(params->gamma1 - params->beta))
        && memcmp(c_tilde, sig.c_tilde, c_tilde_len) == 0;
err:
    OPENSSL_free(alloc);
    EVP_MD_CTX_free(md_ctx);
    return ret;
}

/**
 * See FIPS 204 Section 5.2 Algorithm 2 ML-DSA.Sign()
 *
 * @returns 1 on success, or 0 on error.
 */
int ossl_ml_dsa_sign(const ML_DSA_KEY *priv, int msg_is_mu,
                     const uint8_t *msg, size_t msg_len,
                     const uint8_t *context, size_t context_len,
                     const uint8_t *rand, size_t rand_len, int encode,
                     unsigned char *sig, size_t *sig_len, size_t sig_size)
{
    EVP_MD_CTX *md_ctx = NULL;
    uint8_t mu[ML_DSA_MU_BYTES];
    const uint8_t *mu_ptr = mu;
    size_t mu_len = sizeof(mu);
    int ret = 0;

    if (ossl_ml_dsa_key_get_priv(priv) == NULL)
        return 0;

    if (sig_len != NULL)
        *sig_len = priv->params->sig_len;

    if (sig == NULL)
        return (sig_len != NULL) ? 1 : 0;

    if (sig_size < priv->params->sig_len)
        return 0;

    if (msg_is_mu) {
        mu_ptr = msg;
        mu_len = msg_len;
    } else {
        md_ctx = ossl_ml_dsa_mu_init(priv, encode, context, context_len);
        if (md_ctx == NULL)
            return 0;

        if (!ossl_ml_dsa_mu_update(md_ctx, msg, msg_len))
            goto err;

        if (!ossl_ml_dsa_mu_finalize(md_ctx, mu, mu_len))
            goto err;
    }

    ret = ml_dsa_sign_internal(priv, mu_ptr, mu_len, rand, rand_len, sig);

err:
    EVP_MD_CTX_free(md_ctx);
    return ret;
}

/**
 * See FIPS 203 Section 5.3 Algorithm 3 ML-DSA.Verify()
 * @returns 1 on success, or 0 on error.
 */
int ossl_ml_dsa_verify(const ML_DSA_KEY *pub, int msg_is_mu,
                       const uint8_t *msg, size_t msg_len,
                       const uint8_t *context, size_t context_len, int encode,
                       const uint8_t *sig, size_t sig_len)
{
    EVP_MD_CTX *md_ctx = NULL;
    uint8_t mu[ML_DSA_MU_BYTES];
    const uint8_t *mu_ptr = mu;
    size_t mu_len = sizeof(mu);
    int ret = 0;

    if (ossl_ml_dsa_key_get_pub(pub) == NULL)
        return 0;

    if (msg_is_mu) {
        mu_ptr = msg;
        mu_len = msg_len;
    } else {
        md_ctx = ossl_ml_dsa_mu_init(pub, encode, context, context_len);
        if (md_ctx == NULL)
            return 0;

        if (!ossl_ml_dsa_mu_update(md_ctx, msg, msg_len))
            goto err;

        if (!ossl_ml_dsa_mu_finalize(md_ctx, mu, mu_len))
            goto err;
    }

    ret = ml_dsa_verify_internal(pub, mu_ptr, mu_len, sig, sig_len);
err:
    EVP_MD_CTX_free(md_ctx);
    return ret;
}

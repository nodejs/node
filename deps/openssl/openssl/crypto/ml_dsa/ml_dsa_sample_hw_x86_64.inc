/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2026 Intel Corporation. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#define ML_DSA_SHAKE_X4_BATCH_SIZE 4
#define ML_DSA_SHAKE_X4_DONE_MASK ((1 << ML_DSA_SHAKE_X4_BATCH_SIZE) - 1)
#define ML_DSA_EXPAND_MASK_BYTES_PER_COEFF 32
#define ML_DSA_EXPAND_MASK_COEFFS_GAMMA1_19 20
#define ML_DSA_EXPAND_MASK_COEFFS_GAMMA1_17 18
#define ML_DSA_EXPAND_MASK_BUF_SIZE_GAMMA1_19 \
    (ML_DSA_EXPAND_MASK_BYTES_PER_COEFF * ML_DSA_EXPAND_MASK_COEFFS_GAMMA1_19)
#define ML_DSA_EXPAND_MASK_BUF_SIZE_GAMMA1_17 \
    (ML_DSA_EXPAND_MASK_BYTES_PER_COEFF * ML_DSA_EXPAND_MASK_COEFFS_GAMMA1_17)
#define ML_DSA_EXPAND_MASK_BUF_SIZE(gamma1)         \
    ((gamma1) == ML_DSA_GAMMA1_TWO_POWER_19         \
            ? ML_DSA_EXPAND_MASK_BUF_SIZE_GAMMA1_19 \
            : ML_DSA_EXPAND_MASK_BUF_SIZE_GAMMA1_17)

static ossl_unused int rej_ntt_poly_mb(const uint8_t *seeds[ML_DSA_SHAKE_X4_BATCH_SIZE],
    const size_t seed_len, POLY *outs[ML_DSA_SHAKE_X4_BATCH_SIZE], const size_t count)
{
    KECCAK1600_X4_AVX512VL_CTX ctx;
    uint8_t blocks[ML_DSA_SHAKE_X4_BATCH_SIZE][SHAKE128_BLOCKSIZE];
    int coeff_idx[ML_DSA_SHAKE_X4_BATCH_SIZE] = { 0, 0, 0, 0 };
    size_t done_mask = 0;
    size_t lane;

    for (lane = count; lane < ML_DSA_SHAKE_X4_BATCH_SIZE; lane++)
        done_mask |= ((size_t)1 << lane);

    ossl_sha3_shake128_x4_inc_init_avx512vl(&ctx);
    ossl_sha3_shake128_x4_inc_absorb_avx512vl(&ctx, seeds[0], seeds[1],
        seeds[2], seeds[3], seed_len);

    while (done_mask != ML_DSA_SHAKE_X4_DONE_MASK) {
        ossl_sha3_shake128_x4_inc_squeeze_avx512vl(blocks[0], blocks[1],
            blocks[2], blocks[3], SHAKE128_BLOCKSIZE, &ctx);

        for (lane = 0; lane < ML_DSA_SHAKE_X4_BATCH_SIZE; lane++) {
            if (done_mask & ((size_t)1 << lane))
                continue;

            const uint8_t *b = blocks[lane];
            const uint8_t *end = b + SHAKE128_BLOCKSIZE;

            for (; b < end && coeff_idx[lane] < ML_DSA_NUM_POLY_COEFFICIENTS; b += 3) {
                uint32_t *coeff_ptr = &(outs[lane]->coeff[coeff_idx[lane]]);

                if (coeff_from_three_bytes(b, coeff_ptr))
                    coeff_idx[lane]++;
            }

            if (coeff_idx[lane] >= ML_DSA_NUM_POLY_COEFFICIENTS)
                done_mask |= ((size_t)1 << lane);
        }
    }

    return 1;
}

static void vector_expand_mask_mb(VECTOR *out,
    const uint8_t rho_prime[ML_DSA_RHO_PRIME_BYTES], const uint32_t kappa, const uint32_t gamma1,
    EVP_MD_CTX *h_ctx, const EVP_MD *md)
{
    size_t i;
    const size_t num_polys = out->num_poly;
    uint8_t derived_seeds[ML_DSA_SHAKE_X4_BATCH_SIZE][ML_DSA_RHO_PRIME_BYTES + 2];
    const size_t seed_len = sizeof(derived_seeds[0]);
    const size_t buf_size = ML_DSA_EXPAND_MASK_BUF_SIZE(gamma1);
    uint8_t buffers[ML_DSA_SHAKE_X4_BATCH_SIZE][ML_DSA_EXPAND_MASK_BUF_SIZE_GAMMA1_19];

    (void)h_ctx;
    (void)md;

    for (i = 0; i < ML_DSA_SHAKE_X4_BATCH_SIZE; i++)
        memcpy(derived_seeds[i], rho_prime, ML_DSA_RHO_PRIME_BYTES);

    for (i = 0; i + (ML_DSA_SHAKE_X4_BATCH_SIZE - 1) < num_polys; i += ML_DSA_SHAKE_X4_BATCH_SIZE) {
        size_t b;

        for (b = 0; b < ML_DSA_SHAKE_X4_BATCH_SIZE; b++) {
            const size_t index = kappa + i + b;

            derived_seeds[b][ML_DSA_RHO_PRIME_BYTES] = index & 0xFF;
            derived_seeds[b][ML_DSA_RHO_PRIME_BYTES + 1] = (index >> 8) & 0xFF;
        }

        ossl_sha3_shake256_x4_avx512vl(buffers[0], buffers[1], buffers[2], buffers[3], buf_size,
            derived_seeds[0], derived_seeds[1], derived_seeds[2], derived_seeds[3], seed_len);

        ossl_ml_dsa_poly_decode_expand_mask(&out->poly[i + 0], buffers[0], buf_size, gamma1);
        ossl_ml_dsa_poly_decode_expand_mask(&out->poly[i + 1], buffers[1], buf_size, gamma1);
        ossl_ml_dsa_poly_decode_expand_mask(&out->poly[i + 2], buffers[2], buf_size, gamma1);
        ossl_ml_dsa_poly_decode_expand_mask(&out->poly[i + 3], buffers[3], buf_size, gamma1);
    }

    /*
     * num_polys is always 4 (ML-DSA-44), 5 (ML-DSA-65), or 7 (ML-DSA-87), so the
     * above loops will always runs at least once, initializing derived_seeds.
     * As a result, 'left' below will be 0, 1, or 3, meaning the 4 way shake will
     * recalculate values that are not used.
     */
    if (i < num_polys) {
        const size_t left = num_polys - i;
        size_t b;

        for (b = 0; b < left; b++) {
            const size_t index = kappa + i + b;

            derived_seeds[b][ML_DSA_RHO_PRIME_BYTES] = (uint8_t)index;
            derived_seeds[b][ML_DSA_RHO_PRIME_BYTES + 1] = (uint8_t)(index >> 8);
        }

        ossl_sha3_shake256_x4_avx512vl(buffers[0], buffers[1], buffers[2], buffers[3], buf_size,
            derived_seeds[0], derived_seeds[1], derived_seeds[2], derived_seeds[3], seed_len);

        ossl_ml_dsa_poly_decode_expand_mask(&out->poly[i + 0], buffers[0], buf_size, gamma1);

        if ((i + 1) < num_polys)
            ossl_ml_dsa_poly_decode_expand_mask(&out->poly[i + 1], buffers[1], buf_size, gamma1);

        if ((i + 2) < num_polys)
            ossl_ml_dsa_poly_decode_expand_mask(&out->poly[i + 2], buffers[2], buf_size, gamma1);
    }

    OPENSSL_cleanse(buffers, sizeof(buffers));
    OPENSSL_cleanse(derived_seeds, sizeof(derived_seeds));
}

static ossl_unused int rej_bounded_poly_mb(COEFF_FROM_NIBBLE_FUNC *coef_from_nibble,
    const uint8_t *seeds[ML_DSA_SHAKE_X4_BATCH_SIZE], const size_t seed_len,
    POLY *outs[ML_DSA_SHAKE_X4_BATCH_SIZE], const size_t count)
{
    KECCAK1600_X4_AVX512VL_CTX ctx;
    uint8_t blocks[ML_DSA_SHAKE_X4_BATCH_SIZE][SHAKE256_BLOCKSIZE];
    int coeff_idx[ML_DSA_SHAKE_X4_BATCH_SIZE] = { 0, 0, 0, 0 };
    size_t done_mask = 0;
    size_t lane;

    for (lane = count; lane < ML_DSA_SHAKE_X4_BATCH_SIZE; lane++)
        done_mask |= ((size_t)1 << lane);

    ossl_sha3_shake256_x4_inc_init_avx512vl(&ctx);
    ossl_sha3_shake256_x4_inc_absorb_avx512vl(&ctx, seeds[0], seeds[1],
        seeds[2], seeds[3], seed_len);

    while (done_mask != ML_DSA_SHAKE_X4_DONE_MASK) {
        ossl_sha3_shake256_x4_inc_squeeze_avx512vl(blocks[0], blocks[1],
            blocks[2], blocks[3], SHAKE256_BLOCKSIZE, &ctx);

        for (lane = 0; lane < ML_DSA_SHAKE_X4_BATCH_SIZE; lane++) {
            if (done_mask & ((size_t)1 << lane))
                continue;

            const uint8_t *b = blocks[lane];
            const uint8_t *end = b + SHAKE256_BLOCKSIZE;

            for (; b < end && coeff_idx[lane] < ML_DSA_NUM_POLY_COEFFICIENTS; b++) {
                uint32_t z0 = *b & 0x0F;
                uint32_t z1 = *b >> 4;

                if (coef_from_nibble(z0, &outs[lane]->coeff[coeff_idx[lane]]))
                    coeff_idx[lane]++;

                if (coeff_idx[lane] >= ML_DSA_NUM_POLY_COEFFICIENTS) {
                    done_mask |= ((size_t)1 << lane);
                    break;
                }

                if (coef_from_nibble(z1, &outs[lane]->coeff[coeff_idx[lane]]))
                    coeff_idx[lane]++;

                if (coeff_idx[lane] >= ML_DSA_NUM_POLY_COEFFICIENTS) {
                    done_mask |= ((size_t)1 << lane);
                    break;
                }
            }
        }
    }

    OPENSSL_cleanse(blocks, sizeof(blocks));
    ossl_sha3_shake256_x4_inc_cleanup_avx512vl(&ctx);
    return 1;
}

static int matrix_expand_A_mb(EVP_MD_CTX *g_ctx, const EVP_MD *md,
    const uint8_t *rho, MATRIX *out)
{
    size_t b, idx;
    uint8_t derived_seeds[ML_DSA_SHAKE_X4_BATCH_SIZE][ML_DSA_RHO_BYTES + 2];
    const size_t seed_len = sizeof(derived_seeds[0]);
    const uint8_t *seeds[ML_DSA_SHAKE_X4_BATCH_SIZE];
    POLY *polys[ML_DSA_SHAKE_X4_BATCH_SIZE];
    POLY *poly = out->m_poly;

    /*
     * The seeds derived below and the sampling buffers in rej_ntt_poly_mb()
     * are not cleansed: per FIPS 204 section 3.6.3 the matrix A is easily
     * computed from the public key and does not require any special
     * protections.
     */

    for (b = 0; b < ML_DSA_SHAKE_X4_BATCH_SIZE; b++) {
        memcpy(derived_seeds[b], rho, ML_DSA_RHO_BYTES);
        seeds[b] = derived_seeds[b];
    }

    for (idx = 0; (idx + ML_DSA_SHAKE_X4_BATCH_SIZE - 1) < (out->k * out->l);
        idx += ML_DSA_SHAKE_X4_BATCH_SIZE) {
        for (b = 0; b < ML_DSA_SHAKE_X4_BATCH_SIZE; b++) {
            const size_t row = (idx + b) / out->l;
            const size_t col = (idx + b) % out->l;

            derived_seeds[b][ML_DSA_RHO_BYTES] = (uint8_t)col;
            derived_seeds[b][ML_DSA_RHO_BYTES + 1] = (uint8_t)row;
            polys[b] = &poly[idx + b];
        }

        if (!rej_ntt_poly_mb(seeds, seed_len, polys, 4))
            return 0;
    }

    if (idx < (out->k * out->l)) {
        const size_t left = (out->k * out->l) - idx;

        for (b = 0; b < left; b++) {
            const size_t row = (idx + b) / out->l;
            const size_t col = (idx + b) % out->l;

            derived_seeds[b][ML_DSA_RHO_BYTES] = (uint8_t)col;
            derived_seeds[b][ML_DSA_RHO_BYTES + 1] = (uint8_t)row;
            polys[b] = &poly[idx + b];
        }

        if (!rej_ntt_poly_mb(seeds, seed_len, polys, left))
            return 0;
    }

    return 1;
}

static int vector_expand_S_mb(EVP_MD_CTX *h_ctx, const EVP_MD *md, const int eta,
    const uint8_t *seed, VECTOR *s1, VECTOR *s2)
{
    int ret = 0;
    size_t b, idx;
    const size_t l = s1->num_poly;
    const size_t total = l + s2->num_poly;
    uint8_t derived_seeds[ML_DSA_SHAKE_X4_BATCH_SIZE][ML_DSA_PRIV_SEED_BYTES + 2];
    const uint8_t *seeds[ML_DSA_SHAKE_X4_BATCH_SIZE];
    const size_t seed_len = sizeof(derived_seeds[0]);
    POLY *polys[ML_DSA_SHAKE_X4_BATCH_SIZE];
    COEFF_FROM_NIBBLE_FUNC *coef_from_nibble_fn = (eta == ML_DSA_ETA_4) ? coeff_from_nibble_4 : coeff_from_nibble_2;

    for (b = 0; b < ML_DSA_SHAKE_X4_BATCH_SIZE; b++) {
        memcpy(derived_seeds[b], seed, ML_DSA_PRIV_SEED_BYTES);
        seeds[b] = derived_seeds[b];
    }

    for (idx = 0; (idx + ML_DSA_SHAKE_X4_BATCH_SIZE - 1) < total; idx += ML_DSA_SHAKE_X4_BATCH_SIZE) {
        for (b = 0; b < ML_DSA_SHAKE_X4_BATCH_SIZE; b++) {
            const size_t poly_idx = idx + b;

            derived_seeds[b][ML_DSA_PRIV_SEED_BYTES] = (uint8_t)(poly_idx);
            derived_seeds[b][ML_DSA_PRIV_SEED_BYTES + 1] = (uint8_t)(poly_idx >> 8);

            if (poly_idx < l)
                polys[b] = &s1->poly[poly_idx];
            else
                polys[b] = &s2->poly[poly_idx - l];
        }

        if (!rej_bounded_poly_mb(coef_from_nibble_fn,
                seeds, seed_len, polys, ML_DSA_SHAKE_X4_BATCH_SIZE))
            goto err;
    }

    if (idx < total) {
        const size_t batch_count = total - idx;

        for (b = 0; b < batch_count; b++) {
            const size_t poly_idx = idx + b;

            derived_seeds[b][ML_DSA_PRIV_SEED_BYTES] = (uint8_t)(poly_idx);
            derived_seeds[b][ML_DSA_PRIV_SEED_BYTES + 1] = (uint8_t)(poly_idx >> 8);

            if (poly_idx < l)
                polys[b] = &s1->poly[poly_idx];
            else
                polys[b] = &s2->poly[poly_idx - l];
        }

        if (!rej_bounded_poly_mb(coef_from_nibble_fn,
                seeds, seed_len, polys, batch_count))
            goto err;
    }

    ret = 1;
err:
    OPENSSL_cleanse(derived_seeds, sizeof(derived_seeds));
    return ret;
}

static const OSSL_ML_DSA_SAMPLE_OPS ml_dsa_sample_x86_64 = {
    matrix_expand_A_mb,
    vector_expand_S_mb,
    vector_expand_mask_mb
};

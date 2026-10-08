/*
 *  CTR_DRBG implementation based on AES-256 (NIST SP 800-90)
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 *  The NIST SP 800-90 DRBGs are described in the following publication.
 *
 *  https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-90r.pdf
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_CTR_DRBG_C)

#include "ctr.h"
#include "mbedtls/private/ctr_drbg.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/private/error_common.h"

#include <limits.h>
#include <string.h>

#if defined(MBEDTLS_FS_IO)
#include <stdio.h>
#endif

/* Using error translation functions from PSA to MbedTLS */
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
#include "psa_util_internal.h"
#endif

#include "mbedtls/platform.h"

#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
static psa_status_t ctr_drbg_setup_psa_context(mbedtls_ctr_drbg_psa_context *psa_ctx,
                                               unsigned char *key, size_t key_len)
{
    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status;

    psa_set_key_usage_flags(&key_attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&key_attr, PSA_ALG_ECB_NO_PADDING);
    psa_set_key_type(&key_attr, PSA_KEY_TYPE_AES);
    status = psa_import_key(&key_attr, key, key_len, &psa_ctx->key_id);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_cipher_encrypt_setup(&psa_ctx->operation, psa_ctx->key_id, PSA_ALG_ECB_NO_PADDING);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

exit:
    psa_reset_key_attributes(&key_attr);
    return status;
}

static void ctr_drbg_destroy_psa_contex(mbedtls_ctr_drbg_psa_context *psa_ctx)
{
    psa_cipher_abort(&psa_ctx->operation);
    psa_destroy_key(psa_ctx->key_id);

    psa_ctx->operation = psa_cipher_operation_init();
    psa_ctx->key_id = MBEDTLS_SVC_KEY_ID_INIT;
}
#endif

/*
 * CTR_DRBG context initialization
 */
void mbedtls_ctr_drbg_init(mbedtls_ctr_drbg_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_ctr_drbg_context));
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    ctx->psa_ctx.key_id = MBEDTLS_SVC_KEY_ID_INIT;
    ctx->psa_ctx.operation = psa_cipher_operation_init();
#else
    mbedtls_aes_init(&ctx->aes_ctx);
#endif
    /* Indicate that the entropy nonce length is not set explicitly.
     * See mbedtls_ctr_drbg_set_nonce_len(). */
    ctx->reseed_counter = -1;

    ctx->reseed_interval = MBEDTLS_PSA_RNG_RESEED_INTERVAL;
}

/*
 *  This function resets CTR_DRBG context to the state immediately
 *  after initial call of mbedtls_ctr_drbg_init().
 */
void mbedtls_ctr_drbg_free(mbedtls_ctr_drbg_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

#if defined(MBEDTLS_THREADING_C)
    /* The mutex is initialized iff f_entropy is set. */
    if (ctx->f_entropy != NULL) {
        mbedtls_mutex_free(&ctx->mutex);
    }
#endif
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    ctr_drbg_destroy_psa_contex(&ctx->psa_ctx);
#else
    mbedtls_aes_free(&ctx->aes_ctx);
#endif
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_ctr_drbg_context));
    ctx->reseed_interval = MBEDTLS_PSA_RNG_RESEED_INTERVAL;
    ctx->reseed_counter = -1;
}

void mbedtls_ctr_drbg_set_prediction_resistance(mbedtls_ctr_drbg_context *ctx,
                                                int resistance)
{
    ctx->prediction_resistance = resistance;
}

void mbedtls_ctr_drbg_set_entropy_len(mbedtls_ctr_drbg_context *ctx,
                                      size_t len)
{
    ctx->entropy_len = len;
}

int mbedtls_ctr_drbg_set_nonce_len(mbedtls_ctr_drbg_context *ctx,
                                   size_t len)
{
    /* If mbedtls_ctr_drbg_seed() has already been called, it's
     * too late. Return the error code that's closest to making sense. */
    if (ctx->f_entropy != NULL) {
        return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    }

    if (len > MBEDTLS_CTR_DRBG_MAX_SEED_INPUT) {
        return MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG;
    }

    /* This shouldn't be an issue because
     * MBEDTLS_CTR_DRBG_MAX_SEED_INPUT < INT_MAX in any sensible
     * configuration, but make sure anyway. */
    if (len > INT_MAX) {
        return MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG;
    }

    /* For backward compatibility with Mbed TLS <= 2.19, store the
     * entropy nonce length in a field that already exists, but isn't
     * used until after the initial seeding. */
    /* Due to the capping of len above, the value fits in an int. */
    ctx->reseed_counter = (int) len;
    return 0;
}

void mbedtls_ctr_drbg_set_reseed_interval(mbedtls_ctr_drbg_context *ctx,
                                          int interval)
{
    ctx->reseed_interval = interval;
}

static int block_cipher_df(unsigned char *output,
                           const unsigned char *data, size_t data_len)
{
    unsigned char buf[MBEDTLS_CTR_DRBG_MAX_SEED_INPUT +
                      MBEDTLS_CTR_DRBG_BLOCKSIZE + 16];
    unsigned char tmp[MBEDTLS_CTR_DRBG_SEEDLEN];
    unsigned char key[MBEDTLS_CTR_DRBG_KEYSIZE];
    unsigned char chain[MBEDTLS_CTR_DRBG_BLOCKSIZE];
    unsigned char *p, *iv;
    int ret = 0;
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    psa_status_t status;
    size_t tmp_len;
    mbedtls_ctr_drbg_psa_context psa_ctx;

    psa_ctx.key_id = MBEDTLS_SVC_KEY_ID_INIT;
    psa_ctx.operation = psa_cipher_operation_init();
#else
    mbedtls_aes_context aes_ctx;
#endif

    int i, j;
    size_t buf_len, use_len;

    if (data_len > MBEDTLS_CTR_DRBG_MAX_SEED_INPUT) {
        return MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG;
    }

    memset(buf, 0, MBEDTLS_CTR_DRBG_MAX_SEED_INPUT +
           MBEDTLS_CTR_DRBG_BLOCKSIZE + 16);

    /*
     * Construct IV (16 bytes) and S in buffer
     * IV = Counter (in 32-bits) padded to 16 with zeroes
     * S = Length input string (in 32-bits) || Length of output (in 32-bits) ||
     *     data || 0x80
     *     (Total is padded to a multiple of 16-bytes with zeroes)
     */
    p = buf + MBEDTLS_CTR_DRBG_BLOCKSIZE;
    MBEDTLS_PUT_UINT32_BE(data_len, p, 0);
    p += 4 + 3;
    *p++ = MBEDTLS_CTR_DRBG_SEEDLEN;
    memcpy(p, data, data_len);
    p[data_len] = 0x80;

    buf_len = MBEDTLS_CTR_DRBG_BLOCKSIZE + 8 + data_len + 1;

    for (i = 0; i < MBEDTLS_CTR_DRBG_KEYSIZE; i++) {
        key[i] = i;
    }

#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    status = ctr_drbg_setup_psa_context(&psa_ctx, key, sizeof(key));
    if (status != PSA_SUCCESS) {
        ret = psa_generic_status_to_mbedtls(status);
        goto exit;
    }
#else
    mbedtls_aes_init(&aes_ctx);

    if ((ret = mbedtls_aes_setkey_enc(&aes_ctx, key,
                                      MBEDTLS_CTR_DRBG_KEYBITS)) != 0) {
        goto exit;
    }
#endif

    /*
     * Reduce data to MBEDTLS_CTR_DRBG_SEEDLEN bytes of data
     */
    for (j = 0; j < MBEDTLS_CTR_DRBG_SEEDLEN; j += MBEDTLS_CTR_DRBG_BLOCKSIZE) {
        p = buf;
        memset(chain, 0, MBEDTLS_CTR_DRBG_BLOCKSIZE);
        use_len = buf_len;

        while (use_len > 0) {
            mbedtls_xor(chain, chain, p, MBEDTLS_CTR_DRBG_BLOCKSIZE);
            p += MBEDTLS_CTR_DRBG_BLOCKSIZE;
            use_len -= (use_len >= MBEDTLS_CTR_DRBG_BLOCKSIZE) ?
                       MBEDTLS_CTR_DRBG_BLOCKSIZE : use_len;

#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
            status = psa_cipher_update(&psa_ctx.operation, chain, MBEDTLS_CTR_DRBG_BLOCKSIZE,
                                       chain, MBEDTLS_CTR_DRBG_BLOCKSIZE, &tmp_len);
            if (status != PSA_SUCCESS) {
                ret = psa_generic_status_to_mbedtls(status);
                goto exit;
            }
#else
            if ((ret = mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT,
                                             chain, chain)) != 0) {
                goto exit;
            }
#endif
        }

        memcpy(tmp + j, chain, MBEDTLS_CTR_DRBG_BLOCKSIZE);

        /*
         * Update IV
         */
        buf[3]++;
    }

    /*
     * Do final encryption with reduced data
     */
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    ctr_drbg_destroy_psa_contex(&psa_ctx);

    status = ctr_drbg_setup_psa_context(&psa_ctx, tmp, MBEDTLS_CTR_DRBG_KEYSIZE);
    if (status != PSA_SUCCESS) {
        ret = psa_generic_status_to_mbedtls(status);
        goto exit;
    }
#else
    if ((ret = mbedtls_aes_setkey_enc(&aes_ctx, tmp,
                                      MBEDTLS_CTR_DRBG_KEYBITS)) != 0) {
        goto exit;
    }
#endif
    iv = tmp + MBEDTLS_CTR_DRBG_KEYSIZE;
    p = output;

    for (j = 0; j < MBEDTLS_CTR_DRBG_SEEDLEN; j += MBEDTLS_CTR_DRBG_BLOCKSIZE) {
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
        status = psa_cipher_update(&psa_ctx.operation, iv, MBEDTLS_CTR_DRBG_BLOCKSIZE,
                                   iv, MBEDTLS_CTR_DRBG_BLOCKSIZE, &tmp_len);
        if (status != PSA_SUCCESS) {
            ret = psa_generic_status_to_mbedtls(status);
            goto exit;
        }
#else
        if ((ret = mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT,
                                         iv, iv)) != 0) {
            goto exit;
        }
#endif
        memcpy(p, iv, MBEDTLS_CTR_DRBG_BLOCKSIZE);
        p += MBEDTLS_CTR_DRBG_BLOCKSIZE;
    }
exit:
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    ctr_drbg_destroy_psa_contex(&psa_ctx);
#else
    mbedtls_aes_free(&aes_ctx);
#endif
    /*
     * tidy up the stack
     */
    mbedtls_platform_zeroize(buf, sizeof(buf));
    mbedtls_platform_zeroize(tmp, sizeof(tmp));
    mbedtls_platform_zeroize(key, sizeof(key));
    mbedtls_platform_zeroize(chain, sizeof(chain));
    if (0 != ret) {
        /*
         * wipe partial seed from memory
         */
        mbedtls_platform_zeroize(output, MBEDTLS_CTR_DRBG_SEEDLEN);
    }

    return ret;
}

/* CTR_DRBG_Update (SP 800-90A &sect;10.2.1.2)
 * ctr_drbg_update_internal(ctx, provided_data)
 * implements
 * CTR_DRBG_Update(provided_data, Key, V)
 * with inputs and outputs
 *   ctx->aes_ctx = Key
 *   ctx->counter = V
 */
static int ctr_drbg_update_internal(mbedtls_ctr_drbg_context *ctx,
                                    const unsigned char data[MBEDTLS_CTR_DRBG_SEEDLEN])
{
    unsigned char tmp[MBEDTLS_CTR_DRBG_SEEDLEN];
    unsigned char *p = tmp;
    int j;
    int ret = 0;
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    psa_status_t status;
    size_t tmp_len;
#endif

    memset(tmp, 0, MBEDTLS_CTR_DRBG_SEEDLEN);

    for (j = 0; j < MBEDTLS_CTR_DRBG_SEEDLEN; j += MBEDTLS_CTR_DRBG_BLOCKSIZE) {
        /*
         * Increase counter
         */
        mbedtls_ctr_increment_counter(ctx->counter);

        /*
         * Crypt counter block
         */
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
        status = psa_cipher_update(&ctx->psa_ctx.operation, ctx->counter, sizeof(ctx->counter),
                                   p, MBEDTLS_CTR_DRBG_BLOCKSIZE, &tmp_len);
        if (status != PSA_SUCCESS) {
            ret = psa_generic_status_to_mbedtls(status);
            goto exit;
        }
#else
        if ((ret = mbedtls_aes_crypt_ecb(&ctx->aes_ctx, MBEDTLS_AES_ENCRYPT,
                                         ctx->counter, p)) != 0) {
            goto exit;
        }
#endif

        p += MBEDTLS_CTR_DRBG_BLOCKSIZE;
    }

    mbedtls_xor(tmp, tmp, data, MBEDTLS_CTR_DRBG_SEEDLEN);

    /*
     * Update key and counter
     */
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    ctr_drbg_destroy_psa_contex(&ctx->psa_ctx);

    status = ctr_drbg_setup_psa_context(&ctx->psa_ctx, tmp, MBEDTLS_CTR_DRBG_KEYSIZE);
    if (status != PSA_SUCCESS) {
        ret = psa_generic_status_to_mbedtls(status);
        goto exit;
    }
#else
    if ((ret = mbedtls_aes_setkey_enc(&ctx->aes_ctx, tmp,
                                      MBEDTLS_CTR_DRBG_KEYBITS)) != 0) {
        goto exit;
    }
#endif
    memcpy(ctx->counter, tmp + MBEDTLS_CTR_DRBG_KEYSIZE,
           MBEDTLS_CTR_DRBG_BLOCKSIZE);

exit:
    mbedtls_platform_zeroize(tmp, sizeof(tmp));
    return ret;
}

/* CTR_DRBG_Instantiate with derivation function (SP 800-90A &sect;10.2.1.3.2)
 * mbedtls_ctr_drbg_update(ctx, additional, add_len)
 * implements
 * CTR_DRBG_Instantiate(entropy_input, nonce, personalization_string,
 *                      security_strength) -> initial_working_state
 * with inputs
 *   ctx->counter = all-bits-0
 *   ctx->aes_ctx = context from all-bits-0 key
 *   additional[:add_len] = entropy_input || nonce || personalization_string
 * and with outputs
 *   ctx = initial_working_state
 */
int mbedtls_ctr_drbg_update(mbedtls_ctr_drbg_context *ctx,
                            const unsigned char *additional,
                            size_t add_len)
{
    unsigned char add_input[MBEDTLS_CTR_DRBG_SEEDLEN];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (add_len == 0) {
        return 0;
    }

    if ((ret = block_cipher_df(add_input, additional, add_len)) != 0) {
        goto exit;
    }
    if ((ret = ctr_drbg_update_internal(ctx, add_input)) != 0) {
        goto exit;
    }

exit:
    mbedtls_platform_zeroize(add_input, sizeof(add_input));
    return ret;
}

/* CTR_DRBG_Reseed with derivation function (SP 800-90A &sect;10.2.1.4.2)
 * mbedtls_ctr_drbg_reseed(ctx, additional, len, nonce_len)
 * implements
 * CTR_DRBG_Reseed(working_state, entropy_input, additional_input)
 *                -> new_working_state
 * with inputs
 *   ctx contains working_state
 *   additional[:len] = additional_input
 * and entropy_input comes from calling ctx->f_entropy
 *                              for (ctx->entropy_len + nonce_len) bytes
 * and with output
 *   ctx contains new_working_state
 */
static int mbedtls_ctr_drbg_reseed_internal(mbedtls_ctr_drbg_context *ctx,
                                            const unsigned char *additional,
                                            size_t len,
                                            size_t nonce_len)
{
    unsigned char seed[MBEDTLS_CTR_DRBG_MAX_SEED_INPUT];
    size_t seedlen = 0;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (ctx->entropy_len > MBEDTLS_CTR_DRBG_MAX_SEED_INPUT) {
        return MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG;
    }
    if (nonce_len > MBEDTLS_CTR_DRBG_MAX_SEED_INPUT - ctx->entropy_len) {
        return MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG;
    }
    if (len > MBEDTLS_CTR_DRBG_MAX_SEED_INPUT - ctx->entropy_len - nonce_len) {
        return MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG;
    }

    memset(seed, 0, MBEDTLS_CTR_DRBG_MAX_SEED_INPUT);

    /* Gather entropy_len bytes of entropy to seed state. */
    if (0 != ctx->f_entropy(ctx->p_entropy, seed, ctx->entropy_len)) {
        return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    }
    seedlen += ctx->entropy_len;

    /* Gather entropy for a nonce if requested. */
    if (nonce_len != 0) {
        if (0 != ctx->f_entropy(ctx->p_entropy, seed + seedlen, nonce_len)) {
            return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
        }
        seedlen += nonce_len;
    }

    /* Add additional data if provided. */
    if (additional != NULL && len != 0) {
        memcpy(seed + seedlen, additional, len);
        seedlen += len;
    }

    /* Reduce to 384 bits. */
    if ((ret = block_cipher_df(seed, seed, seedlen)) != 0) {
        goto exit;
    }

    /* Update state. */
    if ((ret = ctr_drbg_update_internal(ctx, seed)) != 0) {
        goto exit;
    }
    ctx->reseed_counter = 1;

exit:
    mbedtls_platform_zeroize(seed, sizeof(seed));
    return ret;
}

int mbedtls_ctr_drbg_reseed(mbedtls_ctr_drbg_context *ctx,
                            const unsigned char *additional, size_t len)
{
    return mbedtls_ctr_drbg_reseed_internal(ctx, additional, len, 0);
}

/* Return a "good" nonce length for CTR_DRBG. The chosen nonce length
 * is sufficient to achieve the maximum security strength given the key
 * size and entropy length. If there is enough entropy in the initial
 * call to the entropy function to serve as both the entropy input and
 * the nonce, don't make a second call to get a nonce. */
static size_t good_nonce_len(size_t entropy_len)
{
    if (entropy_len >= MBEDTLS_CTR_DRBG_KEYSIZE * 3 / 2) {
        return 0;
    } else {
        return (entropy_len + 1) / 2;
    }
}

/* CTR_DRBG_Instantiate with derivation function (SP 800-90A &sect;10.2.1.3.2)
 * mbedtls_ctr_drbg_seed(ctx, f_entropy, p_entropy, custom, len)
 * implements
 * CTR_DRBG_Instantiate(entropy_input, nonce, personalization_string,
 *                      security_strength) -> initial_working_state
 * with inputs
 *   custom[:len] = nonce || personalization_string
 * where entropy_input comes from f_entropy for ctx->entropy_len bytes
 * and with outputs
 *   ctx = initial_working_state
 */
int mbedtls_ctr_drbg_seed(mbedtls_ctr_drbg_context *ctx,
                          int (*f_entropy)(void *, unsigned char *, size_t),
                          void *p_entropy,
                          const unsigned char *custom,
                          size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char key[MBEDTLS_CTR_DRBG_KEYSIZE];
    size_t nonce_len;

    memset(key, 0, MBEDTLS_CTR_DRBG_KEYSIZE);

    /* The mutex is initialized iff f_entropy is set. */
#if defined(MBEDTLS_THREADING_C)
    mbedtls_mutex_init(&ctx->mutex);
#endif

    ctx->f_entropy = f_entropy;
    ctx->p_entropy = p_entropy;

    if (ctx->entropy_len == 0) {
        ctx->entropy_len = MBEDTLS_CTR_DRBG_ENTROPY_LEN;
    }
    /* ctx->reseed_counter contains the desired amount of entropy to
     * grab for a nonce (see mbedtls_ctr_drbg_set_nonce_len()).
     * If it's -1, indicating that the entropy nonce length was not set
     * explicitly, use a sufficiently large nonce for security. */
    nonce_len = (ctx->reseed_counter >= 0 ?
                 (size_t) ctx->reseed_counter :
                 good_nonce_len(ctx->entropy_len));

    /* Initialize with an empty key. */
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
    psa_status_t status;

    status = ctr_drbg_setup_psa_context(&ctx->psa_ctx, key, MBEDTLS_CTR_DRBG_KEYSIZE);
    if (status != PSA_SUCCESS) {
        ret = psa_generic_status_to_mbedtls(status);
        return status;
    }
#else
    if ((ret = mbedtls_aes_setkey_enc(&ctx->aes_ctx, key,
                                      MBEDTLS_CTR_DRBG_KEYBITS)) != 0) {
        return ret;
    }
#endif

    /* Do the initial seeding. */
    if ((ret = mbedtls_ctr_drbg_reseed_internal(ctx, custom, len,
                                                nonce_len)) != 0) {
        return ret;
    }
    return 0;
}

/* CTR_DRBG_Generate with derivation function (SP 800-90A &sect;10.2.1.5.2)
 * mbedtls_ctr_drbg_random_with_add(ctx, output, output_len, additional, add_len)
 * implements
 * CTR_DRBG_Reseed(working_state, entropy_input, additional[:add_len])
 *                -> working_state_after_reseed
 *                if required, then
 * CTR_DRBG_Generate(working_state_after_reseed,
 *                   requested_number_of_bits, additional_input)
 *                -> status, returned_bits, new_working_state
 * with inputs
 *   ctx contains working_state
 *   requested_number_of_bits = 8 * output_len
 *   additional[:add_len] = additional_input
 * and entropy_input comes from calling ctx->f_entropy
 * and with outputs
 *   status = SUCCESS (this function does the reseed internally)
 *   returned_bits = output[:output_len]
 *   ctx contains new_working_state
 */
int mbedtls_ctr_drbg_random_with_add(void *p_rng,
                                     unsigned char *output, size_t output_len,
                                     const unsigned char *additional, size_t add_len)
{
    int ret = 0;
    mbedtls_ctr_drbg_context *ctx = (mbedtls_ctr_drbg_context *) p_rng;
    unsigned char *p = output;
    struct {
        unsigned char add_input[MBEDTLS_CTR_DRBG_SEEDLEN];
        unsigned char tmp[MBEDTLS_CTR_DRBG_BLOCKSIZE];
    } locals;
    size_t use_len;

    if (output_len > MBEDTLS_CTR_DRBG_MAX_REQUEST) {
        return MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG;
    }

    if (add_len > MBEDTLS_CTR_DRBG_MAX_INPUT) {
        return MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG;
    }

    memset(locals.add_input, 0, MBEDTLS_CTR_DRBG_SEEDLEN);

    if (ctx->reseed_counter > ctx->reseed_interval ||
        ctx->prediction_resistance) {
        if ((ret = mbedtls_ctr_drbg_reseed(ctx, additional, add_len)) != 0) {
            return ret;
        }
        add_len = 0;
    }

    if (add_len > 0) {
        if ((ret = block_cipher_df(locals.add_input, additional, add_len)) != 0) {
            goto exit;
        }
        if ((ret = ctr_drbg_update_internal(ctx, locals.add_input)) != 0) {
            goto exit;
        }
    }

    while (output_len > 0) {
        /*
         * Increase counter (treat it as a 128-bit big-endian integer).
         */
        mbedtls_ctr_increment_counter(ctx->counter);

        /*
         * Crypt counter block
         */
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO)
        psa_status_t status;
        size_t tmp_len;

        status = psa_cipher_update(&ctx->psa_ctx.operation, ctx->counter, sizeof(ctx->counter),
                                   locals.tmp, MBEDTLS_CTR_DRBG_BLOCKSIZE, &tmp_len);
        if (status != PSA_SUCCESS) {
            ret = psa_generic_status_to_mbedtls(status);
            goto exit;
        }
#else
        if ((ret = mbedtls_aes_crypt_ecb(&ctx->aes_ctx, MBEDTLS_AES_ENCRYPT,
                                         ctx->counter, locals.tmp)) != 0) {
            goto exit;
        }
#endif

        use_len = (output_len > MBEDTLS_CTR_DRBG_BLOCKSIZE)
            ? MBEDTLS_CTR_DRBG_BLOCKSIZE : output_len;
        /*
         * Copy random block to destination
         */
        memcpy(p, locals.tmp, use_len);
        p += use_len;
        output_len -= use_len;
    }

    if ((ret = ctr_drbg_update_internal(ctx, locals.add_input)) != 0) {
        goto exit;
    }

    ctx->reseed_counter++;

exit:
    mbedtls_platform_zeroize(&locals, sizeof(locals));
    return ret;
}

int mbedtls_ctr_drbg_random(void *p_rng, unsigned char *output,
                            size_t output_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ctr_drbg_context *ctx = (mbedtls_ctr_drbg_context *) p_rng;

#if defined(MBEDTLS_THREADING_C)
    if ((ret = mbedtls_mutex_lock(&ctx->mutex)) != 0) {
        return ret;
    }
#endif

    ret = mbedtls_ctr_drbg_random_with_add(ctx, output, output_len, NULL, 0);

#if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&ctx->mutex) != 0) {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
#endif

    return ret;
}

#if defined(MBEDTLS_FS_IO)
int mbedtls_ctr_drbg_write_seed_file(mbedtls_ctr_drbg_context *ctx,
                                     const char *path)
{
    int ret = MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR;
    FILE *f;
    unsigned char buf[MBEDTLS_CTR_DRBG_MAX_INPUT];

    if ((f = fopen(path, "wb")) == NULL) {
        return MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR;
    }

    /* Ensure no stdio buffering of secrets, as such buffers cannot be wiped. */
    mbedtls_setbuf(f, NULL);

    if ((ret = mbedtls_ctr_drbg_random(ctx, buf,
                                       MBEDTLS_CTR_DRBG_MAX_INPUT)) != 0) {
        goto exit;
    }

    if (fwrite(buf, 1, MBEDTLS_CTR_DRBG_MAX_INPUT, f) !=
        MBEDTLS_CTR_DRBG_MAX_INPUT) {
        ret = MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR;
    } else {
        ret = 0;
    }

exit:
    mbedtls_platform_zeroize(buf, sizeof(buf));

    fclose(f);
    return ret;
}

int mbedtls_ctr_drbg_update_seed_file(mbedtls_ctr_drbg_context *ctx,
                                      const char *path)
{
    int ret = 0;
    FILE *f = NULL;
    size_t n;
    unsigned char buf[MBEDTLS_CTR_DRBG_MAX_INPUT];
    unsigned char c;

    if ((f = fopen(path, "rb")) == NULL) {
        return MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR;
    }

    /* Ensure no stdio buffering of secrets, as such buffers cannot be wiped. */
    mbedtls_setbuf(f, NULL);

    n = fread(buf, 1, sizeof(buf), f);
    if (fread(&c, 1, 1, f) != 0) {
        ret = MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG;
        goto exit;
    }
    if (n == 0 || ferror(f)) {
        ret = MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR;
        goto exit;
    }
    fclose(f);
    f = NULL;

    ret = mbedtls_ctr_drbg_update(ctx, buf, n);

exit:
    mbedtls_platform_zeroize(buf, sizeof(buf));
    if (f != NULL) {
        fclose(f);
    }
    if (ret != 0) {
        return ret;
    }
    return mbedtls_ctr_drbg_write_seed_file(ctx, path);
}
#endif /* MBEDTLS_FS_IO */

#if defined(MBEDTLS_SELF_TEST)

/* The CTR_DRBG NIST test vectors used here are available at
 * https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/drbg/drbgtestvectors.zip
 *
 * The parameters used to derive the test data are:
 *
 * [AES-128 use df]
 * [PredictionResistance = True/False]
 * [EntropyInputLen = 128]
 * [NonceLen = 64]
 * [PersonalizationStringLen = 128]
 * [AdditionalInputLen = 0]
 * [ReturnedBitsLen = 512]
 *
 * [AES-256 use df]
 * [PredictionResistance = True/False]
 * [EntropyInputLen = 256]
 * [NonceLen = 128]
 * [PersonalizationStringLen = 256]
 * [AdditionalInputLen = 0]
 * [ReturnedBitsLen = 512]
 *
 */

#if defined(MBEDTLS_CTR_DRBG_USE_128_BIT_KEY)
static const unsigned char entropy_source_pr[] =
{ 0x04, 0xd9, 0x49, 0xa6, 0xdc, 0xe8, 0x6e, 0xbb,
  0xf1, 0x08, 0x77, 0x2b, 0x9e, 0x08, 0xca, 0x92,
  0x65, 0x16, 0xda, 0x99, 0xa2, 0x59, 0xf3, 0xe8,
  0x38, 0x7e, 0x3f, 0x6b, 0x51, 0x70, 0x7b, 0x20,
  0xec, 0x53, 0xd0, 0x66, 0xc3, 0x0f, 0xe3, 0xb0,
  0xe0, 0x86, 0xa6, 0xaa, 0x5f, 0x72, 0x2f, 0xad,
  0xf7, 0xef, 0x06, 0xb8, 0xd6, 0x9c, 0x9d, 0xe8 };

static const unsigned char entropy_source_nopr[] =
{ 0x07, 0x0d, 0x59, 0x63, 0x98, 0x73, 0xa5, 0x45,
  0x27, 0x38, 0x22, 0x7b, 0x76, 0x85, 0xd1, 0xa9,
  0x74, 0x18, 0x1f, 0x3c, 0x22, 0xf6, 0x49, 0x20,
  0x4a, 0x47, 0xc2, 0xf3, 0x85, 0x16, 0xb4, 0x6f,
  0x00, 0x2e, 0x71, 0xda, 0xed, 0x16, 0x9b, 0x5c };

static const unsigned char pers_pr[] =
{ 0xbf, 0xa4, 0x9a, 0x8f, 0x7b, 0xd8, 0xb1, 0x7a,
  0x9d, 0xfa, 0x45, 0xed, 0x21, 0x52, 0xb3, 0xad };

static const unsigned char pers_nopr[] =
{ 0x4e, 0x61, 0x79, 0xd4, 0xc2, 0x72, 0xa1, 0x4c,
  0xf1, 0x3d, 0xf6, 0x5e, 0xa3, 0xa6, 0xe5, 0x0f };

static const unsigned char result_pr[] =
{ 0xc9, 0x0a, 0xaf, 0x85, 0x89, 0x71, 0x44, 0x66,
  0x4f, 0x25, 0x0b, 0x2b, 0xde, 0xd8, 0xfa, 0xff,
  0x52, 0x5a, 0x1b, 0x32, 0x5e, 0x41, 0x7a, 0x10,
  0x1f, 0xef, 0x1e, 0x62, 0x23, 0xe9, 0x20, 0x30,
  0xc9, 0x0d, 0xad, 0x69, 0xb4, 0x9c, 0x5b, 0xf4,
  0x87, 0x42, 0xd5, 0xae, 0x5e, 0x5e, 0x43, 0xcc,
  0xd9, 0xfd, 0x0b, 0x93, 0x4a, 0xe3, 0xd4, 0x06,
  0x37, 0x36, 0x0f, 0x3f, 0x72, 0x82, 0x0c, 0xcf };

static const unsigned char result_nopr[] =
{ 0x31, 0xc9, 0x91, 0x09, 0xf8, 0xc5, 0x10, 0x13,
  0x3c, 0xd3, 0x96, 0xf9, 0xbc, 0x2c, 0x12, 0xc0,
  0x7c, 0xc1, 0x61, 0x5f, 0xa3, 0x09, 0x99, 0xaf,
  0xd7, 0xf2, 0x36, 0xfd, 0x40, 0x1a, 0x8b, 0xf2,
  0x33, 0x38, 0xee, 0x1d, 0x03, 0x5f, 0x83, 0xb7,
  0xa2, 0x53, 0xdc, 0xee, 0x18, 0xfc, 0xa7, 0xf2,
  0xee, 0x96, 0xc6, 0xc2, 0xcd, 0x0c, 0xff, 0x02,
  0x76, 0x70, 0x69, 0xaa, 0x69, 0xd1, 0x3b, 0xe8 };
#else /* MBEDTLS_CTR_DRBG_USE_128_BIT_KEY */

static const unsigned char entropy_source_pr[] =
{ 0xca, 0x58, 0xfd, 0xf2, 0xb9, 0x77, 0xcb, 0x49,
  0xd4, 0xe0, 0x5b, 0xe2, 0x39, 0x50, 0xd9, 0x8a,
  0x6a, 0xb3, 0xc5, 0x2f, 0xdf, 0x74, 0xd5, 0x85,
  0x8f, 0xd1, 0xba, 0x64, 0x54, 0x7b, 0xdb, 0x1e,
  0xc5, 0xea, 0x24, 0xc0, 0xfa, 0x0c, 0x90, 0x15,
  0x09, 0x20, 0x92, 0x42, 0x32, 0x36, 0x45, 0x45,
  0x7d, 0x20, 0x76, 0x6b, 0xcf, 0xa2, 0x15, 0xc8,
  0x2f, 0x9f, 0xbc, 0x88, 0x3f, 0x80, 0xd1, 0x2c,
  0xb7, 0x16, 0xd1, 0x80, 0x9e, 0xe1, 0xc9, 0xb3,
  0x88, 0x1b, 0x21, 0x45, 0xef, 0xa1, 0x7f, 0xce,
  0xc8, 0x92, 0x35, 0x55, 0x2a, 0xd9, 0x1d, 0x8e,
  0x12, 0x38, 0xac, 0x01, 0x4e, 0x38, 0x18, 0x76,
  0x9c, 0xf2, 0xb6, 0xd4, 0x13, 0xb6, 0x2c, 0x77,
  0xc0, 0xe7, 0xe6, 0x0c, 0x47, 0x44, 0x95, 0xbe };

static const unsigned char entropy_source_nopr[] =
{ 0x4c, 0xfb, 0x21, 0x86, 0x73, 0x34, 0x6d, 0x9d,
  0x50, 0xc9, 0x22, 0xe4, 0x9b, 0x0d, 0xfc, 0xd0,
  0x90, 0xad, 0xf0, 0x4f, 0x5c, 0x3b, 0xa4, 0x73,
  0x27, 0xdf, 0xcd, 0x6f, 0xa6, 0x3a, 0x78, 0x5c,
  0x01, 0x69, 0x62, 0xa7, 0xfd, 0x27, 0x87, 0xa2,
  0x4b, 0xf6, 0xbe, 0x47, 0xef, 0x37, 0x83, 0xf1,
  0xb7, 0xec, 0x46, 0x07, 0x23, 0x63, 0x83, 0x4a,
  0x1b, 0x01, 0x33, 0xf2, 0xc2, 0x38, 0x91, 0xdb,
  0x4f, 0x11, 0xa6, 0x86, 0x51, 0xf2, 0x3e, 0x3a,
  0x8b, 0x1f, 0xdc, 0x03, 0xb1, 0x92, 0xc7, 0xe7 };

static const unsigned char pers_pr[] =
{ 0x5a, 0x70, 0x95, 0xe9, 0x81, 0x40, 0x52, 0x33,
  0x91, 0x53, 0x7e, 0x75, 0xd6, 0x19, 0x9d, 0x1e,
  0xad, 0x0d, 0xc6, 0xa7, 0xde, 0x6c, 0x1f, 0xe0,
  0xea, 0x18, 0x33, 0xa8, 0x7e, 0x06, 0x20, 0xe9 };

static const unsigned char pers_nopr[] =
{ 0x88, 0xee, 0xb8, 0xe0, 0xe8, 0x3b, 0xf3, 0x29,
  0x4b, 0xda, 0xcd, 0x60, 0x99, 0xeb, 0xe4, 0xbf,
  0x55, 0xec, 0xd9, 0x11, 0x3f, 0x71, 0xe5, 0xeb,
  0xcb, 0x45, 0x75, 0xf3, 0xd6, 0xa6, 0x8a, 0x6b };

static const unsigned char result_pr[] =
{ 0xce, 0x2f, 0xdb, 0xb6, 0xd9, 0xb7, 0x39, 0x85,
  0x04, 0xc5, 0xc0, 0x42, 0xc2, 0x31, 0xc6, 0x1d,
  0x9b, 0x5a, 0x59, 0xf8, 0x7e, 0x0d, 0xcc, 0x62,
  0x7b, 0x65, 0x11, 0x55, 0x10, 0xeb, 0x9e, 0x3d,
  0xa4, 0xfb, 0x1c, 0x6a, 0x18, 0xc0, 0x74, 0xdb,
  0xdd, 0xe7, 0x02, 0x23, 0x63, 0x21, 0xd0, 0x39,
  0xf9, 0xa7, 0xc4, 0x52, 0x84, 0x3b, 0x49, 0x40,
  0x72, 0x2b, 0xb0, 0x6c, 0x9c, 0xdb, 0xc3, 0x43 };

static const unsigned char result_nopr[] =
{ 0xa5, 0x51, 0x80, 0xa1, 0x90, 0xbe, 0xf3, 0xad,
  0xaf, 0x28, 0xf6, 0xb7, 0x95, 0xe9, 0xf1, 0xf3,
  0xd6, 0xdf, 0xa1, 0xb2, 0x7d, 0xd0, 0x46, 0x7b,
  0x0c, 0x75, 0xf5, 0xfa, 0x93, 0x1e, 0x97, 0x14,
  0x75, 0xb2, 0x7c, 0xae, 0x03, 0xa2, 0x96, 0x54,
  0xe2, 0xf4, 0x09, 0x66, 0xea, 0x33, 0x64, 0x30,
  0x40, 0xd1, 0x40, 0x0f, 0xe6, 0x77, 0x87, 0x3a,
  0xf8, 0x09, 0x7c, 0x1f, 0xe9, 0xf0, 0x02, 0x98 };
#endif /* MBEDTLS_CTR_DRBG_USE_128_BIT_KEY */

static size_t test_offset;
static int ctr_drbg_self_test_entropy(void *data, unsigned char *buf,
                                      size_t len)
{
    const unsigned char *p = data;
    memcpy(buf, p + test_offset, len);
    test_offset += len;
    return 0;
}

#define CHK(c)    if ((c) != 0)                          \
    {                                       \
        if (verbose != 0)                  \
        mbedtls_printf("failed\n");  \
        return 1;                        \
    }

#define SELF_TEST_OUTPUT_DISCARD_LENGTH 64

/*
 * Checkup routine
 */
int mbedtls_ctr_drbg_self_test(int verbose)
{
    mbedtls_ctr_drbg_context ctx;
    unsigned char buf[sizeof(result_pr)];

    mbedtls_ctr_drbg_init(&ctx);

    /*
     * Based on a NIST CTR_DRBG test vector (PR = True)
     */
    if (verbose != 0) {
        mbedtls_printf("  CTR_DRBG (PR = TRUE) : ");
    }

    test_offset = 0;
    mbedtls_ctr_drbg_set_entropy_len(&ctx, MBEDTLS_CTR_DRBG_KEYSIZE);
    mbedtls_ctr_drbg_set_nonce_len(&ctx, MBEDTLS_CTR_DRBG_KEYSIZE / 2);
    CHK(mbedtls_ctr_drbg_seed(&ctx,
                              ctr_drbg_self_test_entropy,
                              (void *) entropy_source_pr,
                              pers_pr, MBEDTLS_CTR_DRBG_KEYSIZE));
    mbedtls_ctr_drbg_set_prediction_resistance(&ctx, MBEDTLS_CTR_DRBG_PR_ON);
    CHK(mbedtls_ctr_drbg_random(&ctx, buf, SELF_TEST_OUTPUT_DISCARD_LENGTH));
    CHK(mbedtls_ctr_drbg_random(&ctx, buf, sizeof(result_pr)));
    CHK(memcmp(buf, result_pr, sizeof(result_pr)));

    mbedtls_ctr_drbg_free(&ctx);

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

    /*
     * Based on a NIST CTR_DRBG test vector (PR = FALSE)
     */
    if (verbose != 0) {
        mbedtls_printf("  CTR_DRBG (PR = FALSE): ");
    }

    mbedtls_ctr_drbg_init(&ctx);

    test_offset = 0;
    mbedtls_ctr_drbg_set_entropy_len(&ctx, MBEDTLS_CTR_DRBG_KEYSIZE);
    mbedtls_ctr_drbg_set_nonce_len(&ctx, MBEDTLS_CTR_DRBG_KEYSIZE / 2);
    CHK(mbedtls_ctr_drbg_seed(&ctx,
                              ctr_drbg_self_test_entropy,
                              (void *) entropy_source_nopr,
                              pers_nopr, MBEDTLS_CTR_DRBG_KEYSIZE));
    CHK(mbedtls_ctr_drbg_reseed(&ctx, NULL, 0));
    CHK(mbedtls_ctr_drbg_random(&ctx, buf, SELF_TEST_OUTPUT_DISCARD_LENGTH));
    CHK(mbedtls_ctr_drbg_random(&ctx, buf, sizeof(result_nopr)));
    CHK(memcmp(buf, result_nopr, sizeof(result_nopr)));

    mbedtls_ctr_drbg_free(&ctx);

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return 0;
}
#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_CTR_DRBG_C */

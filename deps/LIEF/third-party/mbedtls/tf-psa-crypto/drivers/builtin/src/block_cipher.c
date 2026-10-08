/**
 * \file block_cipher.c
 *
 * \brief Lightweight abstraction layer for block ciphers with 128 bit blocks,
 * for use by the GCM and CCM modules.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
#include "psa/crypto.h"
#include "psa_crypto_core.h"
#include "psa_util_internal.h"
#endif

#include "block_cipher_internal.h"

#if defined(MBEDTLS_BLOCK_CIPHER_C)

#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
static psa_key_type_t psa_key_type_from_block_cipher_id(mbedtls_block_cipher_id_t cipher_id)
{
    switch (cipher_id) {
#if defined(MBEDTLS_BLOCK_CIPHER_AES_VIA_PSA)
        case MBEDTLS_BLOCK_CIPHER_ID_AES:
            return PSA_KEY_TYPE_AES;
#endif
#if defined(MBEDTLS_BLOCK_CIPHER_ARIA_VIA_PSA)
        case MBEDTLS_BLOCK_CIPHER_ID_ARIA:
            return PSA_KEY_TYPE_ARIA;
#endif
#if defined(MBEDTLS_BLOCK_CIPHER_CAMELLIA_VIA_PSA)
        case MBEDTLS_BLOCK_CIPHER_ID_CAMELLIA:
            return PSA_KEY_TYPE_CAMELLIA;
#endif
        default:
            return PSA_KEY_TYPE_NONE;
    }
}

static int mbedtls_cipher_error_from_psa(psa_status_t status)
{
    return PSA_TO_MBEDTLS_ERR_LIST(status, psa_to_cipher_errors,
                                   psa_generic_status_to_mbedtls);
}
#endif /* MBEDTLS_BLOCK_CIPHER_SOME_PSA */

void mbedtls_block_cipher_free(mbedtls_block_cipher_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
    if (ctx->engine == MBEDTLS_BLOCK_CIPHER_ENGINE_PSA) {
        psa_destroy_key(ctx->psa_key_id);
        return;
    }
#endif
    switch (ctx->id) {
#if defined(MBEDTLS_AES_C)
        case MBEDTLS_BLOCK_CIPHER_ID_AES:
            mbedtls_aes_free(&ctx->ctx.aes);
            break;
#endif
#if defined(MBEDTLS_ARIA_C)
        case MBEDTLS_BLOCK_CIPHER_ID_ARIA:
            mbedtls_aria_free(&ctx->ctx.aria);
            break;
#endif
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_BLOCK_CIPHER_ID_CAMELLIA:
            mbedtls_camellia_free(&ctx->ctx.camellia);
            break;
#endif
        default:
            break;
    }
    ctx->id = MBEDTLS_BLOCK_CIPHER_ID_NONE;
}

int mbedtls_block_cipher_setup(mbedtls_block_cipher_context_t *ctx,
                               mbedtls_cipher_id_t cipher_id)
{
    ctx->id = (cipher_id == MBEDTLS_CIPHER_ID_AES) ? MBEDTLS_BLOCK_CIPHER_ID_AES :
              (cipher_id == MBEDTLS_CIPHER_ID_ARIA) ? MBEDTLS_BLOCK_CIPHER_ID_ARIA :
              (cipher_id == MBEDTLS_CIPHER_ID_CAMELLIA) ? MBEDTLS_BLOCK_CIPHER_ID_CAMELLIA :
              MBEDTLS_BLOCK_CIPHER_ID_NONE;

#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
    psa_key_type_t psa_key_type = psa_key_type_from_block_cipher_id(ctx->id);
    if (psa_key_type != PSA_KEY_TYPE_NONE &&
        psa_is_ready_for_cipher()) {
        ctx->engine = MBEDTLS_BLOCK_CIPHER_ENGINE_PSA;
        return 0;
    }
    ctx->engine = MBEDTLS_BLOCK_CIPHER_ENGINE_LEGACY;
#endif

    switch (ctx->id) {
#if defined(MBEDTLS_AES_C)
        case MBEDTLS_BLOCK_CIPHER_ID_AES:
            mbedtls_aes_init(&ctx->ctx.aes);
            return 0;
#endif
#if defined(MBEDTLS_ARIA_C)
        case MBEDTLS_BLOCK_CIPHER_ID_ARIA:
            mbedtls_aria_init(&ctx->ctx.aria);
            return 0;
#endif
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_BLOCK_CIPHER_ID_CAMELLIA:
            mbedtls_camellia_init(&ctx->ctx.camellia);
            return 0;
#endif
        default:
            ctx->id = MBEDTLS_BLOCK_CIPHER_ID_NONE;
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }
}

int mbedtls_block_cipher_setkey(mbedtls_block_cipher_context_t *ctx,
                                const unsigned char *key,
                                unsigned key_bitlen)
{
#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
    if (ctx->engine == MBEDTLS_BLOCK_CIPHER_ENGINE_PSA) {
        psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
        psa_status_t status;

        psa_set_key_type(&key_attr, psa_key_type_from_block_cipher_id(ctx->id));
        psa_set_key_bits(&key_attr, key_bitlen);
        psa_set_key_algorithm(&key_attr, PSA_ALG_ECB_NO_PADDING);
        psa_set_key_usage_flags(&key_attr, PSA_KEY_USAGE_ENCRYPT);

        status = psa_import_key(&key_attr, key, PSA_BITS_TO_BYTES(key_bitlen), &ctx->psa_key_id);
        if (status != PSA_SUCCESS) {
            return mbedtls_cipher_error_from_psa(status);
        }
        psa_reset_key_attributes(&key_attr);

        return 0;
    }
#endif /* MBEDTLS_BLOCK_CIPHER_SOME_PSA */

    switch (ctx->id) {
#if defined(MBEDTLS_AES_C)
        case MBEDTLS_BLOCK_CIPHER_ID_AES:
            return mbedtls_aes_setkey_enc(&ctx->ctx.aes, key, key_bitlen);
#endif
#if defined(MBEDTLS_ARIA_C)
        case MBEDTLS_BLOCK_CIPHER_ID_ARIA:
            return mbedtls_aria_setkey_enc(&ctx->ctx.aria, key, key_bitlen);
#endif
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_BLOCK_CIPHER_ID_CAMELLIA:
            return mbedtls_camellia_setkey_enc(&ctx->ctx.camellia, key, key_bitlen);
#endif
        default:
            return MBEDTLS_ERR_CIPHER_INVALID_CONTEXT;
    }
}

int mbedtls_block_cipher_encrypt(mbedtls_block_cipher_context_t *ctx,
                                 const unsigned char input[16],
                                 unsigned char output[16])
{
#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
    if (ctx->engine == MBEDTLS_BLOCK_CIPHER_ENGINE_PSA) {
        psa_status_t status;
        size_t olen;

        status = psa_cipher_encrypt(ctx->psa_key_id, PSA_ALG_ECB_NO_PADDING,
                                    input, 16, output, 16, &olen);
        if (status != PSA_SUCCESS) {
            return mbedtls_cipher_error_from_psa(status);
        }
        return 0;
    }
#endif /* MBEDTLS_BLOCK_CIPHER_SOME_PSA */

    switch (ctx->id) {
#if defined(MBEDTLS_AES_C)
        case MBEDTLS_BLOCK_CIPHER_ID_AES:
            return mbedtls_aes_crypt_ecb(&ctx->ctx.aes, MBEDTLS_AES_ENCRYPT,
                                         input, output);
#endif
#if defined(MBEDTLS_ARIA_C)
        case MBEDTLS_BLOCK_CIPHER_ID_ARIA:
            return mbedtls_aria_crypt_ecb(&ctx->ctx.aria, input, output);
#endif
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_BLOCK_CIPHER_ID_CAMELLIA:
            return mbedtls_camellia_crypt_ecb(&ctx->ctx.camellia,
                                              MBEDTLS_CAMELLIA_ENCRYPT,
                                              input, output);
#endif
        default:
            return MBEDTLS_ERR_CIPHER_INVALID_CONTEXT;
    }
}

#endif /* MBEDTLS_BLOCK_CIPHER_C */

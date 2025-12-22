/*
 *  PSA cipher driver entry points
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_PSA_CRYPTO_C)

#include "psa_crypto_cipher.h"
#include "psa_crypto_core.h"
#include "psa_crypto_random_impl.h"

#include "mbedtls/cipher.h"
#include "mbedtls/error.h"

#include <string.h>

/* mbedtls_cipher_values_from_psa() below only checks if the proper build symbols
 * are enabled, but it does not provide any compatibility check between them
 * (i.e. if the specified key works with the specified algorithm). This helper
 * function is meant to provide this support.
 * mbedtls_cipher_info_from_psa() might be used for the same purpose, but it
 * requires CIPHER_C to be enabled.
 */
static psa_status_t mbedtls_cipher_validate_values(
    psa_algorithm_t alg,
    psa_key_type_t key_type)
{
    /* Reduce code size - hinting to the compiler about what it can assume allows the compiler to
       eliminate bits of the logic below. */
#if !defined(PSA_WANT_KEY_TYPE_AES)
    MBEDTLS_ASSUME(key_type != PSA_KEY_TYPE_AES);
#endif
#if !defined(PSA_WANT_KEY_TYPE_ARIA)
    MBEDTLS_ASSUME(key_type != PSA_KEY_TYPE_ARIA);
#endif
#if !defined(PSA_WANT_KEY_TYPE_CAMELLIA)
    MBEDTLS_ASSUME(key_type != PSA_KEY_TYPE_CAMELLIA);
#endif
#if !defined(PSA_WANT_KEY_TYPE_CHACHA20)
    MBEDTLS_ASSUME(key_type != PSA_KEY_TYPE_CHACHA20);
#endif
#if !defined(PSA_WANT_KEY_TYPE_DES)
    MBEDTLS_ASSUME(key_type != PSA_KEY_TYPE_DES);
#endif
#if !defined(PSA_WANT_ALG_CCM)
    MBEDTLS_ASSUME(alg != PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, 0));
#endif
#if !defined(PSA_WANT_ALG_GCM)
    MBEDTLS_ASSUME(alg != PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, 0));
#endif
#if !defined(PSA_WANT_ALG_STREAM_CIPHER)
    MBEDTLS_ASSUME(alg != PSA_ALG_STREAM_CIPHER);
#endif
#if !defined(PSA_WANT_ALG_CHACHA20_POLY1305)
    MBEDTLS_ASSUME(alg != PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CHACHA20_POLY1305, 0));
#endif
#if !defined(PSA_WANT_ALG_CCM_STAR_NO_TAG)
    MBEDTLS_ASSUME(alg != PSA_ALG_CCM_STAR_NO_TAG);
#endif
#if !defined(PSA_WANT_ALG_CTR)
    MBEDTLS_ASSUME(alg != PSA_ALG_CTR);
#endif
#if !defined(PSA_WANT_ALG_CFB)
    MBEDTLS_ASSUME(alg != PSA_ALG_CFB);
#endif
#if !defined(PSA_WANT_ALG_OFB)
    MBEDTLS_ASSUME(alg != PSA_ALG_OFB);
#endif
#if !defined(PSA_WANT_ALG_ECB_NO_PADDING)
    MBEDTLS_ASSUME(alg != PSA_ALG_ECB_NO_PADDING);
#endif
#if !defined(PSA_WANT_ALG_CBC_NO_PADDING)
    MBEDTLS_ASSUME(alg != PSA_ALG_CBC_NO_PADDING);
#endif
#if !defined(PSA_WANT_ALG_CBC_PKCS7)
    MBEDTLS_ASSUME(alg != PSA_ALG_CBC_PKCS7);
#endif
#if !defined(PSA_WANT_ALG_CMAC)
    MBEDTLS_ASSUME(alg != PSA_ALG_CMAC);
#endif

    if (alg == PSA_ALG_STREAM_CIPHER ||
        alg == PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CHACHA20_POLY1305, 0)) {
        if (key_type == PSA_KEY_TYPE_CHACHA20) {
            return PSA_SUCCESS;
        }
    }

    if (alg == PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, 0) ||
        alg == PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, 0) ||
        alg == PSA_ALG_CCM_STAR_NO_TAG) {
        if (key_type == PSA_KEY_TYPE_AES ||
            key_type == PSA_KEY_TYPE_ARIA ||
            key_type == PSA_KEY_TYPE_CAMELLIA) {
            return PSA_SUCCESS;
        }
    }

    if (alg == PSA_ALG_CTR ||
        alg == PSA_ALG_CFB ||
        alg == PSA_ALG_OFB ||
        alg == PSA_ALG_XTS ||
        alg == PSA_ALG_ECB_NO_PADDING ||
        alg == PSA_ALG_CBC_NO_PADDING ||
        alg == PSA_ALG_CBC_PKCS7 ||
        alg == PSA_ALG_CMAC) {
        if (key_type == PSA_KEY_TYPE_AES ||
            key_type == PSA_KEY_TYPE_ARIA ||
            key_type == PSA_KEY_TYPE_DES ||
            key_type == PSA_KEY_TYPE_CAMELLIA) {
            return PSA_SUCCESS;
        }
    }

    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t mbedtls_cipher_values_from_psa(
    psa_algorithm_t alg,
    psa_key_type_t key_type,
    size_t *key_bits,
    mbedtls_cipher_mode_t *mode,
    mbedtls_cipher_id_t *cipher_id)
{
    mbedtls_cipher_id_t cipher_id_tmp;
    /* Only DES modifies key_bits */
#if !defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES)
    (void) key_bits;
#endif

    if (PSA_ALG_IS_AEAD(alg)) {
        alg = PSA_ALG_AEAD_WITH_SHORTENED_TAG(alg, 0);
    }

    if (PSA_ALG_IS_CIPHER(alg) || PSA_ALG_IS_AEAD(alg)) {
        switch (alg) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_STREAM_CIPHER)
            case PSA_ALG_STREAM_CIPHER:
                *mode = MBEDTLS_MODE_STREAM;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CTR)
            case PSA_ALG_CTR:
                *mode = MBEDTLS_MODE_CTR;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CFB)
            case PSA_ALG_CFB:
                *mode = MBEDTLS_MODE_CFB;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_OFB)
            case PSA_ALG_OFB:
                *mode = MBEDTLS_MODE_OFB;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_ECB_NO_PADDING)
            case PSA_ALG_ECB_NO_PADDING:
                *mode = MBEDTLS_MODE_ECB;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CBC_NO_PADDING)
            case PSA_ALG_CBC_NO_PADDING:
                *mode = MBEDTLS_MODE_CBC;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CBC_PKCS7)
            case PSA_ALG_CBC_PKCS7:
                *mode = MBEDTLS_MODE_CBC;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM_STAR_NO_TAG)
            case PSA_ALG_CCM_STAR_NO_TAG:
                *mode = MBEDTLS_MODE_CCM_STAR_NO_TAG;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
            case PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, 0):
                *mode = MBEDTLS_MODE_CCM;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
            case PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, 0):
                *mode = MBEDTLS_MODE_GCM;
                break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
            case PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CHACHA20_POLY1305, 0):
                *mode = MBEDTLS_MODE_CHACHAPOLY;
                break;
#endif
            default:
                return PSA_ERROR_NOT_SUPPORTED;
        }
    } else if (alg == PSA_ALG_CMAC) {
        *mode = MBEDTLS_MODE_ECB;
    } else {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    switch (key_type) {
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_AES)
        case PSA_KEY_TYPE_AES:
            cipher_id_tmp = MBEDTLS_CIPHER_ID_AES;
            break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ARIA)
        case PSA_KEY_TYPE_ARIA:
            cipher_id_tmp = MBEDTLS_CIPHER_ID_ARIA;
            break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES)
        case PSA_KEY_TYPE_DES:
            /* key_bits is 64 for Single-DES, 128 for two-key Triple-DES,
             * and 192 for three-key Triple-DES. */
            if (*key_bits == 64) {
                cipher_id_tmp = MBEDTLS_CIPHER_ID_DES;
            } else {
                cipher_id_tmp = MBEDTLS_CIPHER_ID_3DES;
            }
            /* mbedtls doesn't recognize two-key Triple-DES as an algorithm,
             * but two-key Triple-DES is functionally three-key Triple-DES
             * with K1=K3, so that's how we present it to mbedtls. */
            if (*key_bits == 128) {
                *key_bits = 192;
            }
            break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_CAMELLIA)
        case PSA_KEY_TYPE_CAMELLIA:
            cipher_id_tmp = MBEDTLS_CIPHER_ID_CAMELLIA;
            break;
#endif
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_CHACHA20)
        case PSA_KEY_TYPE_CHACHA20:
            cipher_id_tmp = MBEDTLS_CIPHER_ID_CHACHA20;
            break;
#endif
        default:
            return PSA_ERROR_NOT_SUPPORTED;
    }
    if (cipher_id != NULL) {
        *cipher_id = cipher_id_tmp;
    }

    return mbedtls_cipher_validate_values(alg, key_type);
}

#if defined(MBEDTLS_CIPHER_C)
const mbedtls_cipher_info_t *mbedtls_cipher_info_from_psa(
    psa_algorithm_t alg,
    psa_key_type_t key_type,
    size_t key_bits,
    mbedtls_cipher_id_t *cipher_id)
{
    mbedtls_cipher_mode_t mode;
    psa_status_t status;
    mbedtls_cipher_id_t cipher_id_tmp = MBEDTLS_CIPHER_ID_NONE;

    status = mbedtls_cipher_values_from_psa(alg, key_type, &key_bits, &mode, &cipher_id_tmp);
    if (status != PSA_SUCCESS) {
        return NULL;
    }
    if (cipher_id != NULL) {
        *cipher_id = cipher_id_tmp;
    }

    return mbedtls_cipher_info_from_values(cipher_id_tmp, (int) key_bits, mode);
}
#endif /* MBEDTLS_CIPHER_C */

#if defined(MBEDTLS_PSA_BUILTIN_CIPHER)

static psa_status_t psa_cipher_setup(
    mbedtls_psa_cipher_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg,
    mbedtls_operation_t cipher_operation)
{
    int ret = 0;
    size_t key_bits;
    const mbedtls_cipher_info_t *cipher_info = NULL;
    psa_key_type_t key_type = attributes->type;

    (void) key_buffer_size;

    mbedtls_cipher_init(&operation->ctx.cipher);

    operation->alg = alg;
    key_bits = attributes->bits;
    cipher_info = mbedtls_cipher_info_from_psa(alg, key_type,
                                               key_bits, NULL);
    if (cipher_info == NULL) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    ret = mbedtls_cipher_setup(&operation->ctx.cipher, cipher_info);
    if (ret != 0) {
        goto exit;
    }

#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES)
    if (key_type == PSA_KEY_TYPE_DES && key_bits == 128) {
        /* Two-key Triple-DES is 3-key Triple-DES with K1=K3 */
        uint8_t keys[24];
        memcpy(keys, key_buffer, 16);
        memcpy(keys + 16, key_buffer, 8);
        ret = mbedtls_cipher_setkey(&operation->ctx.cipher,
                                    keys,
                                    192, cipher_operation);
    } else
#endif
    {
        ret = mbedtls_cipher_setkey(&operation->ctx.cipher, key_buffer,
                                    (int) key_bits, cipher_operation);
    }
    if (ret != 0) {
        goto exit;
    }

#if defined(MBEDTLS_PSA_BUILTIN_ALG_CBC_NO_PADDING) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CBC_PKCS7)
    switch (alg) {
        case PSA_ALG_CBC_NO_PADDING:
            ret = mbedtls_cipher_set_padding_mode(&operation->ctx.cipher,
                                                  MBEDTLS_PADDING_NONE);
            break;
        case PSA_ALG_CBC_PKCS7:
            ret = mbedtls_cipher_set_padding_mode(&operation->ctx.cipher,
                                                  MBEDTLS_PADDING_PKCS7);
            break;
        default:
            /* The algorithm doesn't involve padding. */
            ret = 0;
            break;
    }
    if (ret != 0) {
        goto exit;
    }
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CBC_NO_PADDING ||
          MBEDTLS_PSA_BUILTIN_ALG_CBC_PKCS7 */

    operation->block_length = (PSA_ALG_IS_STREAM_CIPHER(alg) ? 1 :
                               PSA_BLOCK_CIPHER_BLOCK_LENGTH(key_type));
    operation->iv_length = PSA_CIPHER_IV_LENGTH(key_type, alg);

exit:
    return mbedtls_to_psa_error(ret);
}

psa_status_t mbedtls_psa_cipher_encrypt_setup(
    mbedtls_psa_cipher_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg)
{
    return psa_cipher_setup(operation, attributes,
                            key_buffer, key_buffer_size,
                            alg, MBEDTLS_ENCRYPT);
}

psa_status_t mbedtls_psa_cipher_decrypt_setup(
    mbedtls_psa_cipher_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg)
{
    return psa_cipher_setup(operation, attributes,
                            key_buffer, key_buffer_size,
                            alg, MBEDTLS_DECRYPT);
}

psa_status_t mbedtls_psa_cipher_set_iv(
    mbedtls_psa_cipher_operation_t *operation,
    const uint8_t *iv, size_t iv_length)
{
    if (iv_length != operation->iv_length) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    return mbedtls_to_psa_error(
        mbedtls_cipher_set_iv(&operation->ctx.cipher,
                              iv, iv_length));
}

#if defined(MBEDTLS_PSA_BUILTIN_ALG_ECB_NO_PADDING)
/** Process input for which the algorithm is set to ECB mode.
 *
 * This requires manual processing, since the PSA API is defined as being
 * able to process arbitrary-length calls to psa_cipher_update() with ECB mode,
 * but the underlying mbedtls_cipher_update only takes full blocks.
 *
 * \param ctx           The mbedtls cipher context to use. It must have been
 *                      set up for ECB.
 * \param[in] input     The input plaintext or ciphertext to process.
 * \param input_length  The number of bytes to process from \p input.
 *                      This does not need to be aligned to a block boundary.
 *                      If there is a partial block at the end of the input,
 *                      it is stored in \p ctx for future processing.
 * \param output        The buffer where the output is written. It must be
 *                      at least `BS * floor((p + input_length) / BS)` bytes
 *                      long, where `p` is the number of bytes in the
 *                      unprocessed partial block in \p ctx (with
 *                      `0 <= p <= BS - 1`) and `BS` is the block size.
 * \param output_length On success, the number of bytes written to \p output.
 *                      \c 0 on error.
 *
 * \return #PSA_SUCCESS or an error from a hardware accelerator
 */
static psa_status_t psa_cipher_update_ecb(
    mbedtls_cipher_context_t *ctx,
    const uint8_t *input,
    size_t input_length,
    uint8_t *output,
    size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t block_size = mbedtls_cipher_info_get_block_size(ctx->cipher_info);
    size_t internal_output_length = 0;
    *output_length = 0;

    if (input_length == 0) {
        status = PSA_SUCCESS;
        goto exit;
    }

    if (ctx->unprocessed_len > 0) {
        /* Fill up to block size, and run the block if there's a full one. */
        size_t bytes_to_copy = block_size - ctx->unprocessed_len;

        if (input_length < bytes_to_copy) {
            bytes_to_copy = input_length;
        }

        memcpy(&(ctx->unprocessed_data[ctx->unprocessed_len]),
               input, bytes_to_copy);
        input_length -= bytes_to_copy;
        input += bytes_to_copy;
        ctx->unprocessed_len += bytes_to_copy;

        if (ctx->unprocessed_len == block_size) {
            status = mbedtls_to_psa_error(
                mbedtls_cipher_update(ctx,
                                      ctx->unprocessed_data,
                                      block_size,
                                      output, &internal_output_length));

            if (status != PSA_SUCCESS) {
                goto exit;
            }

            output += internal_output_length;
            *output_length += internal_output_length;
            ctx->unprocessed_len = 0;
        }
    }

    while (input_length >= block_size) {
        /* Run all full blocks we have, one by one */
        status = mbedtls_to_psa_error(
            mbedtls_cipher_update(ctx, input,
                                  block_size,
                                  output, &internal_output_length));

        if (status != PSA_SUCCESS) {
            goto exit;
        }

        input_length -= block_size;
        input += block_size;

        output += internal_output_length;
        *output_length += internal_output_length;
    }

    if (input_length > 0) {
        /* Save unprocessed bytes for later processing */
        memcpy(&(ctx->unprocessed_data[ctx->unprocessed_len]),
               input, input_length);
        ctx->unprocessed_len += input_length;
    }

    status = PSA_SUCCESS;

exit:
    return status;
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_ECB_NO_PADDING */

psa_status_t mbedtls_psa_cipher_update(
    mbedtls_psa_cipher_operation_t *operation,
    const uint8_t *input, size_t input_length,
    uint8_t *output, size_t output_size, size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t expected_output_size;

    if (!PSA_ALG_IS_STREAM_CIPHER(operation->alg)) {
        /* Take the unprocessed partial block left over from previous
         * update calls, if any, plus the input to this call. Remove
         * the last partial block, if any. You get the data that will be
         * output in this call. */
        expected_output_size =
            (operation->ctx.cipher.unprocessed_len + input_length)
            / operation->block_length * operation->block_length;
    } else {
        expected_output_size = input_length;
    }

    if (output_size < expected_output_size) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

#if defined(MBEDTLS_PSA_BUILTIN_ALG_ECB_NO_PADDING)
    if (operation->alg == PSA_ALG_ECB_NO_PADDING) {
        /* mbedtls_cipher_update has an API inconsistency: it will only
         * process a single block at a time in ECB mode. Abstract away that
         * inconsistency here to match the PSA API behaviour. */
        status = psa_cipher_update_ecb(&operation->ctx.cipher,
                                       input,
                                       input_length,
                                       output,
                                       output_length);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_ECB_NO_PADDING */
    if (input_length == 0) {
        /* There is no input, nothing to be done */
        *output_length = 0;
        status = PSA_SUCCESS;
    } else {
        status = mbedtls_to_psa_error(
            mbedtls_cipher_update(&operation->ctx.cipher, input,
                                  input_length, output, output_length));

        if (*output_length > output_size) {
            return PSA_ERROR_CORRUPTION_DETECTED;
        }
    }

    return status;
}

psa_status_t mbedtls_psa_cipher_finish(
    mbedtls_psa_cipher_operation_t *operation,
    uint8_t *output, size_t output_size, size_t *output_length)
{
    psa_status_t status = PSA_ERROR_GENERIC_ERROR;
    uint8_t temp_output_buffer[MBEDTLS_MAX_BLOCK_LENGTH];

    if (operation->ctx.cipher.unprocessed_len != 0) {
        if (operation->alg == PSA_ALG_ECB_NO_PADDING ||
            operation->alg == PSA_ALG_CBC_NO_PADDING) {
            status = PSA_ERROR_INVALID_ARGUMENT;
            goto exit;
        }
    }

    status = mbedtls_to_psa_error(
        mbedtls_cipher_finish(&operation->ctx.cipher,
                              temp_output_buffer,
                              output_length));
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (*output_length == 0) {
        ; /* Nothing to copy. Note that output may be NULL in this case. */
    } else if (output_size >= *output_length) {
        memcpy(output, temp_output_buffer, *output_length);
    } else {
        status = PSA_ERROR_BUFFER_TOO_SMALL;
    }

exit:
    mbedtls_platform_zeroize(temp_output_buffer,
                             sizeof(temp_output_buffer));

    return status;
}

psa_status_t mbedtls_psa_cipher_abort(
    mbedtls_psa_cipher_operation_t *operation)
{
    /* Sanity check (shouldn't happen: operation->alg should
     * always have been initialized to a valid value). */
    if (!PSA_ALG_IS_CIPHER(operation->alg)) {
        return PSA_ERROR_BAD_STATE;
    }

    mbedtls_cipher_free(&operation->ctx.cipher);

    return PSA_SUCCESS;
}

psa_status_t mbedtls_psa_cipher_encrypt(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *iv,
    size_t iv_length,
    const uint8_t *input,
    size_t input_length,
    uint8_t *output,
    size_t output_size,
    size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    mbedtls_psa_cipher_operation_t operation = MBEDTLS_PSA_CIPHER_OPERATION_INIT;
    size_t update_output_length, finish_output_length;

    status = mbedtls_psa_cipher_encrypt_setup(&operation, attributes,
                                              key_buffer, key_buffer_size,
                                              alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (iv_length > 0) {
        status = mbedtls_psa_cipher_set_iv(&operation, iv, iv_length);
        if (status != PSA_SUCCESS) {
            goto exit;
        }
    }

    status = mbedtls_psa_cipher_update(&operation, input, input_length,
                                       output, output_size,
                                       &update_output_length);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = mbedtls_psa_cipher_finish(
        &operation,
        mbedtls_buffer_offset(output, update_output_length),
        output_size - update_output_length, &finish_output_length);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    *output_length = update_output_length + finish_output_length;

exit:
    if (status == PSA_SUCCESS) {
        status = mbedtls_psa_cipher_abort(&operation);
    } else {
        mbedtls_psa_cipher_abort(&operation);
    }

    return status;
}

psa_status_t mbedtls_psa_cipher_decrypt(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *input,
    size_t input_length,
    uint8_t *output,
    size_t output_size,
    size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    mbedtls_psa_cipher_operation_t operation = MBEDTLS_PSA_CIPHER_OPERATION_INIT;
    size_t olength, accumulated_length;

    status = mbedtls_psa_cipher_decrypt_setup(&operation, attributes,
                                              key_buffer, key_buffer_size,
                                              alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (operation.iv_length > 0) {
        status = mbedtls_psa_cipher_set_iv(&operation,
                                           input, operation.iv_length);
        if (status != PSA_SUCCESS) {
            goto exit;
        }
    }

    status = mbedtls_psa_cipher_update(
        &operation,
        mbedtls_buffer_offset_const(input, operation.iv_length),
        input_length - operation.iv_length,
        output, output_size, &olength);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    accumulated_length = olength;

    status = mbedtls_psa_cipher_finish(
        &operation,
        mbedtls_buffer_offset(output, accumulated_length),
        output_size - accumulated_length, &olength);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    *output_length = accumulated_length + olength;

exit:
    if (status == PSA_SUCCESS) {
        status = mbedtls_psa_cipher_abort(&operation);
    } else {
        mbedtls_psa_cipher_abort(&operation);
    }

    return status;
}
#endif /* MBEDTLS_PSA_BUILTIN_CIPHER */

#endif /* MBEDTLS_PSA_CRYPTO_C */

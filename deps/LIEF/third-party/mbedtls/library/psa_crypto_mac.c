/*
 *  PSA MAC layer on top of Mbed TLS software crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_PSA_CRYPTO_C)

#include <psa/crypto.h>
#include "psa_crypto_core.h"
#include "psa_crypto_cipher.h"
#include "psa_crypto_mac.h"
#include <mbedtls/md.h>

#include <mbedtls/error.h>
#include "mbedtls/constant_time.h"
#include <string.h>

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
static psa_status_t psa_hmac_abort_internal(
    mbedtls_psa_hmac_operation_t *hmac)
{
    mbedtls_platform_zeroize(hmac->opad, sizeof(hmac->opad));
    return psa_hash_abort(&hmac->hash_ctx);
}

static psa_status_t psa_hmac_setup_internal(
    mbedtls_psa_hmac_operation_t *hmac,
    const uint8_t *key,
    size_t key_length,
    psa_algorithm_t hash_alg)
{
    uint8_t ipad[PSA_HMAC_MAX_HASH_BLOCK_SIZE];
    size_t i;
    size_t hash_size = PSA_HASH_LENGTH(hash_alg);
    size_t block_size = PSA_HASH_BLOCK_LENGTH(hash_alg);
    psa_status_t status;

    hmac->alg = hash_alg;

    /* Sanity checks on block_size, to guarantee that there won't be a buffer
     * overflow below. This should never trigger if the hash algorithm
     * is implemented correctly. */
    /* The size checks against the ipad and opad buffers cannot be written
     * `block_size > sizeof( ipad ) || block_size > sizeof( hmac->opad )`
     * because that triggers -Wlogical-op on GCC 7.3. */
    if (block_size > sizeof(ipad)) {
        return PSA_ERROR_NOT_SUPPORTED;
    }
    if (block_size > sizeof(hmac->opad)) {
        return PSA_ERROR_NOT_SUPPORTED;
    }
    if (block_size < hash_size) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    if (key_length > block_size) {
        status = psa_hash_compute(hash_alg, key, key_length,
                                  ipad, sizeof(ipad), &key_length);
        if (status != PSA_SUCCESS) {
            goto cleanup;
        }
    }
    /* A 0-length key is not commonly used in HMAC when used as a MAC,
     * but it is permitted. It is common when HMAC is used in HKDF, for
     * example. Don't call `memcpy` in the 0-length because `key` could be
     * an invalid pointer which would make the behavior undefined. */
    else if (key_length != 0) {
        memcpy(ipad, key, key_length);
    }

    /* ipad contains the key followed by garbage. Xor and fill with 0x36
     * to create the ipad value. */
    for (i = 0; i < key_length; i++) {
        ipad[i] ^= 0x36;
    }
    memset(ipad + key_length, 0x36, block_size - key_length);

    /* Copy the key material from ipad to opad, flipping the requisite bits,
     * and filling the rest of opad with the requisite constant. */
    for (i = 0; i < key_length; i++) {
        hmac->opad[i] = ipad[i] ^ 0x36 ^ 0x5C;
    }
    memset(hmac->opad + key_length, 0x5C, block_size - key_length);

    status = psa_hash_setup(&hmac->hash_ctx, hash_alg);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }

    status = psa_hash_update(&hmac->hash_ctx, ipad, block_size);

cleanup:
    mbedtls_platform_zeroize(ipad, sizeof(ipad));

    return status;
}

static psa_status_t psa_hmac_update_internal(
    mbedtls_psa_hmac_operation_t *hmac,
    const uint8_t *data,
    size_t data_length)
{
    return psa_hash_update(&hmac->hash_ctx, data, data_length);
}

static psa_status_t psa_hmac_finish_internal(
    mbedtls_psa_hmac_operation_t *hmac,
    uint8_t *mac,
    size_t mac_size)
{
    uint8_t tmp[PSA_HASH_MAX_SIZE];
    psa_algorithm_t hash_alg = hmac->alg;
    size_t hash_size = 0;
    size_t block_size = PSA_HASH_BLOCK_LENGTH(hash_alg);
    psa_status_t status;

    status = psa_hash_finish(&hmac->hash_ctx, tmp, sizeof(tmp), &hash_size);
    if (status != PSA_SUCCESS) {
        return status;
    }
    /* From here on, tmp needs to be wiped. */

    status = psa_hash_setup(&hmac->hash_ctx, hash_alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&hmac->hash_ctx, hmac->opad, block_size);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&hmac->hash_ctx, tmp, hash_size);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_finish(&hmac->hash_ctx, tmp, sizeof(tmp), &hash_size);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    memcpy(mac, tmp, mac_size);

exit:
    mbedtls_platform_zeroize(tmp, hash_size);
    return status;
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC)
static psa_status_t cmac_setup(mbedtls_psa_mac_operation_t *operation,
                               const psa_key_attributes_t *attributes,
                               const uint8_t *key_buffer)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(PSA_WANT_KEY_TYPE_DES)
    /* Mbed TLS CMAC does not accept 3DES with only two keys, nor does it accept
     * to do CMAC with pure DES, so return NOT_SUPPORTED here. */
    if (psa_get_key_type(attributes) == PSA_KEY_TYPE_DES &&
        (psa_get_key_bits(attributes) == 64 ||
         psa_get_key_bits(attributes) == 128)) {
        return PSA_ERROR_NOT_SUPPORTED;
    }
#endif

    const mbedtls_cipher_info_t *cipher_info =
        mbedtls_cipher_info_from_psa(
            PSA_ALG_CMAC,
            psa_get_key_type(attributes),
            psa_get_key_bits(attributes),
            NULL);

    if (cipher_info == NULL) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    ret = mbedtls_cipher_setup(&operation->ctx.cmac, cipher_info);
    if (ret != 0) {
        goto exit;
    }

    ret = mbedtls_cipher_cmac_starts(&operation->ctx.cmac,
                                     key_buffer,
                                     psa_get_key_bits(attributes));
exit:
    return mbedtls_to_psa_error(ret);
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC)

/* Initialize this driver's MAC operation structure. Once this function has been
 * called, mbedtls_psa_mac_abort can run and will do the right thing. */
static psa_status_t mac_init(
    mbedtls_psa_mac_operation_t *operation,
    psa_algorithm_t alg)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    operation->alg = alg;

#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC)
    if (PSA_ALG_FULL_LENGTH_MAC(operation->alg) == PSA_ALG_CMAC) {
        mbedtls_cipher_init(&operation->ctx.cmac);
        status = PSA_SUCCESS;
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
    if (PSA_ALG_IS_HMAC(operation->alg)) {
        /* We'll set up the hash operation later in psa_hmac_setup_internal. */
        operation->ctx.hmac.alg = 0;
        status = PSA_SUCCESS;
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */
    {
        (void) operation;
        status = PSA_ERROR_NOT_SUPPORTED;
    }

    if (status != PSA_SUCCESS) {
        memset(operation, 0, sizeof(*operation));
    }
    return status;
}

psa_status_t mbedtls_psa_mac_abort(mbedtls_psa_mac_operation_t *operation)
{
    if (operation->alg == 0) {
        /* The object has (apparently) been initialized but it is not
         * in use. It's ok to call abort on such an object, and there's
         * nothing to do. */
        return PSA_SUCCESS;
    } else
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC)
    if (PSA_ALG_FULL_LENGTH_MAC(operation->alg) == PSA_ALG_CMAC) {
        mbedtls_cipher_free(&operation->ctx.cmac);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
    if (PSA_ALG_IS_HMAC(operation->alg)) {
        psa_hmac_abort_internal(&operation->ctx.hmac);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */
    {
        /* Sanity check (shouldn't happen: operation->alg should
         * always have been initialized to a valid value). */
        goto bad_state;
    }

    operation->alg = 0;

    return PSA_SUCCESS;

bad_state:
    /* If abort is called on an uninitialized object, we can't trust
     * anything. Wipe the object in case it contains confidential data.
     * This may result in a memory leak if a pointer gets overwritten,
     * but it's too late to do anything about this. */
    memset(operation, 0, sizeof(*operation));
    return PSA_ERROR_BAD_STATE;
}

static psa_status_t psa_mac_setup(mbedtls_psa_mac_operation_t *operation,
                                  const psa_key_attributes_t *attributes,
                                  const uint8_t *key_buffer,
                                  size_t key_buffer_size,
                                  psa_algorithm_t alg)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    /* A context must be freshly initialized before it can be set up. */
    if (operation->alg != 0) {
        return PSA_ERROR_BAD_STATE;
    }

    status = mac_init(operation, alg);
    if (status != PSA_SUCCESS) {
        return status;
    }

#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC)
    if (PSA_ALG_FULL_LENGTH_MAC(alg) == PSA_ALG_CMAC) {
        /* Key buffer size for CMAC is dictated by the key bits set on the
         * attributes, and previously validated by the core on key import. */
        (void) key_buffer_size;
        status = cmac_setup(operation, attributes, key_buffer);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
    if (PSA_ALG_IS_HMAC(alg)) {
        status = psa_hmac_setup_internal(&operation->ctx.hmac,
                                         key_buffer,
                                         key_buffer_size,
                                         PSA_ALG_HMAC_GET_HASH(alg));
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */
    {
        (void) attributes;
        (void) key_buffer;
        (void) key_buffer_size;
        status = PSA_ERROR_NOT_SUPPORTED;
    }

    if (status != PSA_SUCCESS) {
        mbedtls_psa_mac_abort(operation);
    }

    return status;
}

psa_status_t mbedtls_psa_mac_sign_setup(
    mbedtls_psa_mac_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg)
{
    return psa_mac_setup(operation, attributes,
                         key_buffer, key_buffer_size, alg);
}

psa_status_t mbedtls_psa_mac_verify_setup(
    mbedtls_psa_mac_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg)
{
    return psa_mac_setup(operation, attributes,
                         key_buffer, key_buffer_size, alg);
}

psa_status_t mbedtls_psa_mac_update(
    mbedtls_psa_mac_operation_t *operation,
    const uint8_t *input,
    size_t input_length)
{
    if (operation->alg == 0) {
        return PSA_ERROR_BAD_STATE;
    }

#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC)
    if (PSA_ALG_FULL_LENGTH_MAC(operation->alg) == PSA_ALG_CMAC) {
        return mbedtls_to_psa_error(
            mbedtls_cipher_cmac_update(&operation->ctx.cmac,
                                       input, input_length));
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
    if (PSA_ALG_IS_HMAC(operation->alg)) {
        return psa_hmac_update_internal(&operation->ctx.hmac,
                                        input, input_length);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */
    {
        /* This shouldn't happen if `operation` was initialized by
         * a setup function. */
        (void) input;
        (void) input_length;
        return PSA_ERROR_BAD_STATE;
    }
}

static psa_status_t psa_mac_finish_internal(
    mbedtls_psa_mac_operation_t *operation,
    uint8_t *mac, size_t mac_size)
{
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC)
    if (PSA_ALG_FULL_LENGTH_MAC(operation->alg) == PSA_ALG_CMAC) {
        uint8_t tmp[PSA_BLOCK_CIPHER_BLOCK_MAX_SIZE];
        int ret = mbedtls_cipher_cmac_finish(&operation->ctx.cmac, tmp);
        if (ret == 0) {
            memcpy(mac, tmp, mac_size);
        }
        mbedtls_platform_zeroize(tmp, sizeof(tmp));
        return mbedtls_to_psa_error(ret);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
    if (PSA_ALG_IS_HMAC(operation->alg)) {
        return psa_hmac_finish_internal(&operation->ctx.hmac,
                                        mac, mac_size);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */
    {
        /* This shouldn't happen if `operation` was initialized by
         * a setup function. */
        (void) operation;
        (void) mac;
        (void) mac_size;
        return PSA_ERROR_BAD_STATE;
    }
}

psa_status_t mbedtls_psa_mac_sign_finish(
    mbedtls_psa_mac_operation_t *operation,
    uint8_t *mac,
    size_t mac_size,
    size_t *mac_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    if (operation->alg == 0) {
        return PSA_ERROR_BAD_STATE;
    }

    status = psa_mac_finish_internal(operation, mac, mac_size);
    if (status == PSA_SUCCESS) {
        *mac_length = mac_size;
    }

    return status;
}

psa_status_t mbedtls_psa_mac_verify_finish(
    mbedtls_psa_mac_operation_t *operation,
    const uint8_t *mac,
    size_t mac_length)
{
    uint8_t actual_mac[PSA_MAC_MAX_SIZE];
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    if (operation->alg == 0) {
        return PSA_ERROR_BAD_STATE;
    }

    /* Consistency check: requested MAC length fits our local buffer */
    if (mac_length > sizeof(actual_mac)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    status = psa_mac_finish_internal(operation, actual_mac, mac_length);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }

    if (mbedtls_ct_memcmp(mac, actual_mac, mac_length) != 0) {
        status = PSA_ERROR_INVALID_SIGNATURE;
    }

cleanup:
    mbedtls_platform_zeroize(actual_mac, sizeof(actual_mac));

    return status;
}

psa_status_t mbedtls_psa_mac_compute(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *input,
    size_t input_length,
    uint8_t *mac,
    size_t mac_size,
    size_t *mac_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    mbedtls_psa_mac_operation_t operation = MBEDTLS_PSA_MAC_OPERATION_INIT;
    /* Make sure the whole operation is zeroed.
     * PSA_MAC_OPERATION_INIT does not necessarily do it fully,
     * since one field is a union and initializing a union does not
     * necessarily initialize all of its members.
     * In multipart operations, this is done in the API functions,
     * before driver dispatch, since it needs to be done before calling
     * the driver entry point. Here, we bypass the multipart API,
     * so it's our job. */
    memset(&operation, 0, sizeof(operation));

    status = psa_mac_setup(&operation,
                           attributes, key_buffer, key_buffer_size,
                           alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (input_length > 0) {
        status = mbedtls_psa_mac_update(&operation, input, input_length);
        if (status != PSA_SUCCESS) {
            goto exit;
        }
    }

    status = psa_mac_finish_internal(&operation, mac, mac_size);
    if (status == PSA_SUCCESS) {
        *mac_length = mac_size;
    }

exit:
    mbedtls_psa_mac_abort(&operation);

    return status;
}

#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC || MBEDTLS_PSA_BUILTIN_ALG_CMAC */

#endif /* MBEDTLS_PSA_CRYPTO_C */

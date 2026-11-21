/*
 *  RSA setters for PK.
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#include "mbedtls/pk.h"
#include "mbedtls/private/error_common.h"
#include "pk_internal.h"

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)

int mbedtls_pk_rsa_set_key(mbedtls_pk_context *pk, const unsigned char *key, size_t key_len)
{
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status;
    size_t key_bits = 0;

    psa_set_key_type(&attr, PSA_KEY_TYPE_RSA_KEY_PAIR);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_SIGN_MESSAGE |
                            PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_VERIFY_MESSAGE |
                            PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_COPY);
    psa_set_key_algorithm(&attr, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_ANY_HASH));
#if defined(MBEDTLS_PSA_CRYPTO_C)
    psa_set_key_enrollment_algorithm(&attr, PSA_ALG_RSA_PSS(PSA_ALG_ANY_HASH));
#endif

    status = psa_import_key(&attr, key, key_len, &pk->priv_id);
    if (status != PSA_SUCCESS) {
        return psa_pk_status_to_mbedtls(status);
    }

    /* psa_import_key() will also determine the size of the key in bits during import.
     * We use this to update the PK context structure as well. */
    status = psa_get_key_attributes(pk->priv_id, &attr);
    if (status != PSA_SUCCESS) {
        psa_destroy_key(pk->priv_id);
        return psa_pk_status_to_mbedtls(status);
    }

    key_bits = psa_get_key_bits(&attr);
    /* If "bits" was already setup previously its value must be correct. */
    if ((pk->bits != 0) && (pk->bits != key_bits)) {
        psa_destroy_key(pk->priv_id);
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }
    pk->bits = key_bits;

    psa_reset_key_attributes(&attr);

    return 0;
}

int mbedtls_pk_rsa_set_pubkey(mbedtls_pk_context *pk, const unsigned char *key, size_t key_len)
{
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
    psa_status_t status;
    size_t key_bits;

    if (key_len > sizeof(pk->pub_raw)) {
        return MBEDTLS_ERR_PK_INVALID_PUBKEY;
    }

    /* Try importing the key to check if it's valid. */
    psa_set_key_type(&attr, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_VERIFY_MESSAGE |
                            PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_COPY);
    psa_set_key_algorithm(&attr, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_ANY_HASH));

    status = psa_import_key(&attr, key, key_len, &key_id);
    if (status != PSA_SUCCESS) {
        return MBEDTLS_ERR_PK_INVALID_PUBKEY;
    }

    status = psa_get_key_attributes(key_id, &attr);
    if (status != PSA_SUCCESS) {
        psa_destroy_key(key_id);
        return psa_pk_status_to_mbedtls(status);
    }

    status = psa_destroy_key(key_id);
    if (status != PSA_SUCCESS) {
        psa_reset_key_attributes(&attr);
        return psa_pk_status_to_mbedtls(status);
    }

    key_bits = psa_get_key_bits(&attr);
    psa_reset_key_attributes(&attr);
    if ((pk->bits != 0) && (pk->bits != key_bits)) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }
    pk->bits = key_bits;
    pk->rsa_padding = MBEDTLS_PK_RSA_PKCS_V15;

    memcpy(pk->pub_raw, key, key_len);
    pk->pub_raw_len = key_len;

    return 0;
}

int mbedtls_pk_rsa_set_pubkey_from_prv(mbedtls_pk_context *pk)
{
    psa_status_t status;

    status = psa_export_public_key(pk->priv_id, pk->pub_raw, sizeof(pk->pub_raw),
                                   &pk->pub_raw_len);
    return PSA_PK_TO_MBEDTLS_ERR(status);
}
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

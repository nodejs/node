/**
 * \file pk_internal.h
 *
 * \brief Public Key abstraction layer: internal (i.e. library only) functions
 *        and definitions.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_PK_INTERNAL_H
#define MBEDTLS_PK_INTERNAL_H

#include "mbedtls/pk.h"
#if defined(MBEDTLS_PK_HAVE_PRIVATE_HEADER)
#include <mbedtls/private/pk_private.h>
#endif /* MBEDTLS_PK_HAVE_PRIVATE_HEADER */

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
#include "mbedtls/private/ecp.h"
#endif

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)
#include "psa/crypto.h"

#include "psa_util_internal.h"
#define PSA_PK_TO_MBEDTLS_ERR(status) psa_pk_status_to_mbedtls(status)
#define PSA_PK_RSA_TO_MBEDTLS_ERR(status) PSA_TO_MBEDTLS_ERR_LIST(status,     \
                                                                  psa_to_pk_rsa_errors,            \
                                                                  psa_pk_status_to_mbedtls)
#define PSA_PK_ECDSA_TO_MBEDTLS_ERR(status) PSA_TO_MBEDTLS_ERR_LIST(status,   \
                                                                    psa_to_pk_ecdsa_errors,        \
                                                                    psa_pk_status_to_mbedtls)
#endif /* MBEDTLS_PSA_CRYPTO_CLIENT */

/* Headers/footers for PEM files */
#define PEM_BEGIN_PUBLIC_KEY    "-----BEGIN PUBLIC KEY-----"
#define PEM_END_PUBLIC_KEY      "-----END PUBLIC KEY-----"
#define PEM_BEGIN_PRIVATE_KEY_RSA   "-----BEGIN RSA PRIVATE KEY-----"
#define PEM_END_PRIVATE_KEY_RSA     "-----END RSA PRIVATE KEY-----"
#define PEM_BEGIN_PUBLIC_KEY_RSA     "-----BEGIN RSA PUBLIC KEY-----"
#define PEM_END_PUBLIC_KEY_RSA     "-----END RSA PUBLIC KEY-----"
#define PEM_BEGIN_PRIVATE_KEY_EC    "-----BEGIN EC PRIVATE KEY-----"
#define PEM_END_PRIVATE_KEY_EC      "-----END EC PRIVATE KEY-----"
#define PEM_BEGIN_PRIVATE_KEY_PKCS8 "-----BEGIN PRIVATE KEY-----"
#define PEM_END_PRIVATE_KEY_PKCS8   "-----END PRIVATE KEY-----"
#define PEM_BEGIN_ENCRYPTED_PRIVATE_KEY_PKCS8 "-----BEGIN ENCRYPTED PRIVATE KEY-----"
#define PEM_END_ENCRYPTED_PRIVATE_KEY_PKCS8   "-----END ENCRYPTED PRIVATE KEY-----"

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)

static inline mbedtls_ecp_group_id mbedtls_pk_get_ec_group_id(const mbedtls_pk_context *pk)
{
    mbedtls_ecp_group_id id;

    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        psa_key_attributes_t opaque_attrs = PSA_KEY_ATTRIBUTES_INIT;
        psa_key_type_t opaque_key_type;
        psa_ecc_family_t curve;

        if (psa_get_key_attributes(pk->priv_id, &opaque_attrs) != PSA_SUCCESS) {
            return MBEDTLS_ECP_DP_NONE;
        }
        opaque_key_type = psa_get_key_type(&opaque_attrs);
        curve = PSA_KEY_TYPE_ECC_GET_FAMILY(opaque_key_type);
        id = mbedtls_ecc_group_from_psa(curve, psa_get_key_bits(&opaque_attrs));
        psa_reset_key_attributes(&opaque_attrs);
    } else {
        id = mbedtls_ecc_group_from_psa(pk->ec_family, pk->bits);
    }

    return id;
}

/* Helper for Montgomery curves */
#if defined(PSA_WANT_ECC_MONTGOMERY_255) || defined(PSA_WANT_ECC_MONTGOMERY_448)
#define MBEDTLS_PK_HAVE_RFC8410_CURVES
#endif /* PSA_WANT_ECC_MONTGOMERY_255 || PSA_WANT_ECC_MONTGOMERY_448 */

#define MBEDTLS_PK_IS_RFC8410_GROUP_ID(id)  \
    ((id == MBEDTLS_ECP_DP_CURVE25519) || (id == MBEDTLS_ECP_DP_CURVE448))

static inline int mbedtls_pk_is_rfc8410(const mbedtls_pk_context *pk)
{
    mbedtls_ecp_group_id id = mbedtls_pk_get_ec_group_id(pk);

    return MBEDTLS_PK_IS_RFC8410_GROUP_ID(id);
}

/*
 * Set the group used by this key.
 *
 * [in/out] pk: in: must have been pk_setup() to an ECC type
 *              out: will have group (curve) information set
 * [in] grp_in: a supported group ID (not NONE)
 */
int mbedtls_pk_ecc_set_group(mbedtls_pk_context *pk, mbedtls_ecp_group_id grp_id);

/*
 * Set the private key material
 *
 * [in/out] pk: in: must have the group set already, see mbedtls_pk_ecc_set_group().
 *              out: will have the private key set.
 * [in] key, key_len: the raw private key (no ASN.1 wrapping).
 */
int mbedtls_pk_ecc_set_key(mbedtls_pk_context *pk, unsigned char *key, size_t key_len);

/*
 * Set the public key.
 *
 * [in/out] pk: in: must have its group set, see mbedtls_pk_ecc_set_group().
 *              out: will have the public key set.
 * [in] pub, pub_len: the raw public key (an ECPoint).
 *
 * Return:
 * - 0 on success;
 * - MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE if the format is potentially valid
 *   but not supported;
 * - another error code otherwise.
 */
int mbedtls_pk_ecc_set_pubkey(mbedtls_pk_context *pk, const unsigned char *pub, size_t pub_len);

/*
 * Derive a public key from its private counterpart.
 * Computationally intensive, only use when public key is not available.
 *
 * [in/out] pk: in: must have the private key set, see mbedtls_pk_ecc_set_key().
 *              out: will have the public key set.
 * [in] prv, prv_len: the raw private key (see note below).
 *
 * Note: the private key information is always available from pk,
 * however for convenience the serialized version is also passed,
 * as it's available at each calling site, and useful in some configs
 * (as otherwise we would have to re-serialize it from the pk context).
 */
int mbedtls_pk_ecc_set_pubkey_from_prv(mbedtls_pk_context *pk,
                                       const unsigned char *prv, size_t prv_len);
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
/*
 * Parse a private RSA key.
 */
int mbedtls_pk_rsa_set_key(mbedtls_pk_context *pk, const unsigned char *key, size_t key_len);

/*
 * Parse an RSA public key.
 */
int mbedtls_pk_rsa_set_pubkey(mbedtls_pk_context *pk, const unsigned char *key, size_t key_len);

/*
 * Set the public key field in PK context by exporting it from the private key.
 */
int mbedtls_pk_rsa_set_pubkey_from_prv(mbedtls_pk_context *pk);

/*
 * Set the padding for the RSA key.
 */
static inline int mbedtls_pk_set_rsa_padding(mbedtls_pk_context *pk, mbedtls_pk_rsa_padding_t type)
{
    if ((type != MBEDTLS_PK_RSA_PKCS_V15) && (type != MBEDTLS_PK_RSA_PKCS_V21)) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    pk->rsa_padding = type;

    return 0;
}

/*
 * Set the hash algorithm to be used with RSA public key.
 */
static inline int mbedtls_pk_set_rsa_hash_alg(mbedtls_pk_context *pk, psa_algorithm_t alg)
{
    pk->rsa_hash_alg = alg;

    return 0;
}
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

#if defined(MBEDTLS_TEST_HOOKS)

MBEDTLS_STATIC_TESTABLE int mbedtls_pk_parse_key_pkcs8_encrypted_der(
    mbedtls_pk_context *pk,
    unsigned char *key, size_t keylen,
    const unsigned char *pwd, size_t pwdlen);

#if defined(MBEDTLS_PK_PARSE_C)
MBEDTLS_STATIC_TESTABLE int mbedtls_pk_parse_key_pkcs8_unencrypted_der(
    mbedtls_pk_context *pk,
    const unsigned char *key,
    size_t keylen);
#endif /* MBEDTLS_PK_PARSE_C */

#endif /* MBEDTLS_TEST_HOOKS */

#if defined(MBEDTLS_FS_IO)
int mbedtls_pk_load_file(const char *path, unsigned char **buf, size_t *n);
#endif

#endif /* MBEDTLS_PK_INTERNAL_H */

/*
 *  Public Key abstraction layer
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * This file is private and for internal
 * use only.
 */
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_PK_C)
#include "mbedtls/pk.h"
#if defined(MBEDTLS_PK_HAVE_PRIVATE_HEADER)
#include <mbedtls/private/pk_private.h>
#endif /* MBEDTLS_PK_HAVE_PRIVATE_HEADER */
#include "pk_wrap.h"
#include "pkwrite.h"
#include "pk_internal.h"

#include "mbedtls/platform_util.h"
#include "mbedtls/private/error_common.h"

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
#include "mbedtls/private/ecp.h"
#endif
#if defined(MBEDTLS_ECDSA_C)
#include "mbedtls/private/ecdsa.h"
#endif

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)
#include "psa_util_internal.h"
#include "mbedtls/psa_util.h"
#endif

#include <limits.h>
#include <stdint.h>

/*
 * Initialise a mbedtls_pk_context
 */
void mbedtls_pk_init(mbedtls_pk_context *ctx)
{
    /*
     * Note: if any of the fields needs to be initialized to non-zero,
     * we need to add a call to this as the end of mbedtls_pk_free()!
     */
    ctx->pk_info = NULL;
    ctx->pk_ctx = NULL;
    ctx->priv_id = MBEDTLS_SVC_KEY_ID_INIT;
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY) || defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
    memset(ctx->pub_raw, 0, sizeof(ctx->pub_raw));
    ctx->pub_raw_len = 0;
    ctx->bits = 0;
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY || PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
    ctx->ec_family = 0;
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
    ctx->rsa_padding = MBEDTLS_PK_RSA_PKCS_V15;
    ctx->rsa_hash_alg = PSA_ALG_ANY_HASH;
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
}

/*
 * Free (the components of) a mbedtls_pk_context
 */
void mbedtls_pk_free(mbedtls_pk_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if ((ctx->pk_info != NULL) && (ctx->pk_info->ctx_free_func != NULL)) {
        ctx->pk_info->ctx_free_func(ctx->pk_ctx);
    }

    /* The ownership of the priv_id key for opaque keys is external of the PK
     * module. It's the user responsibility to clear it after use. */
    if ((ctx->pk_info != NULL) && (ctx->pk_info->type != MBEDTLS_PK_OPAQUE)) {
        psa_destroy_key(ctx->priv_id);
    }

    /* Leaves the context in the same state as mbedtls_pk_init(). */
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_pk_context));
}

#if defined(MBEDTLS_ECP_RESTARTABLE)
/*
 * Initialize a restart context
 */
void mbedtls_pk_restart_init(mbedtls_pk_restart_ctx *ctx)
{
    ctx->pk_info = NULL;
    ctx->rs_ctx = NULL;
}

/*
 * Free the components of a restart context
 */
void mbedtls_pk_restart_free(mbedtls_pk_restart_ctx *ctx)
{
    if (ctx == NULL || ctx->pk_info == NULL ||
        ctx->pk_info->rs_free_func == NULL) {
        return;
    }

    ctx->pk_info->rs_free_func(ctx->rs_ctx);

    ctx->pk_info = NULL;
    ctx->rs_ctx = NULL;
}
#endif /* MBEDTLS_ECP_RESTARTABLE */

/*
 * Get pk_info structure from type
 */
const mbedtls_pk_info_t *mbedtls_pk_info_from_type(mbedtls_pk_type_t pk_type)
{
    switch (pk_type) {
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
        case MBEDTLS_PK_RSA:
            return &mbedtls_rsa_info;
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
        case MBEDTLS_PK_ECKEY:
            return &mbedtls_eckey_info;
        case MBEDTLS_PK_ECKEY_DH:
            return &mbedtls_eckeydh_info;
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
#if defined(PSA_HAVE_ALG_SOME_ECDSA)
        case MBEDTLS_PK_ECDSA:
            return &mbedtls_ecdsa_info;
#endif /* PSA_HAVE_ALG_SOME_ECDSA */
        default:
            return NULL;
    }
}

/*
 * Initialise context
 */
int mbedtls_pk_setup(mbedtls_pk_context *ctx, const mbedtls_pk_info_t *info)
{
    if (info == NULL || ctx->pk_info != NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if ((info->ctx_alloc_func != NULL) &&
        ((ctx->pk_ctx = info->ctx_alloc_func()) == NULL)) {
        return MBEDTLS_ERR_PK_ALLOC_FAILED;
    }

    ctx->pk_info = info;

    return 0;
}

/*
 * Initialise a PSA-wrapping context
 */
int mbedtls_pk_wrap_psa(mbedtls_pk_context *ctx,
                        const mbedtls_svc_key_id_t key)
{
    const mbedtls_pk_info_t *info = NULL;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_type_t type;

    if (ctx == NULL || ctx->pk_info != NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (PSA_SUCCESS != psa_get_key_attributes(key, &attributes)) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }
    type = psa_get_key_type(&attributes);
    psa_reset_key_attributes(&attributes);

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
    if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(type)) {
        info = &mbedtls_ecdsa_opaque_info;
    } else
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
    if (type == PSA_KEY_TYPE_RSA_KEY_PAIR) {
        info = &mbedtls_rsa_opaque_info;
    } else {
        return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
    }

    ctx->pk_info = info;
    ctx->priv_id = key;

    return 0;
}

/*
 * Tell if a PK can do the operations of the given type
 */
int mbedtls_pk_can_do(const mbedtls_pk_context *ctx, mbedtls_pk_type_t type)
{
    /* A context with null pk_info is not set up yet and can't do anything.
     * For backward compatibility, also accept NULL instead of a context
     * pointer. */
    if (ctx == NULL || ctx->pk_info == NULL) {
        return 0;
    }

    return ctx->pk_info->can_do(type);
}

/*
 * Tell if a PK can do the operations of the given PSA algorithm
 */
int mbedtls_pk_can_do_ext(const mbedtls_pk_context *ctx, psa_algorithm_t alg,
                          psa_key_usage_t usage)
{
    psa_key_usage_t key_usage;

    /* A context with null pk_info is not set up yet and can't do anything.
     * For backward compatibility, also accept NULL instead of a context
     * pointer. */
    if (ctx == NULL || ctx->pk_info == NULL) {
        return 0;
    }

    /* Filter out non allowed algorithms */
    if (PSA_ALG_IS_ECDSA(alg) == 0 &&
        PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg) == 0 &&
        PSA_ALG_IS_RSA_PSS(alg) == 0 &&
        alg != PSA_ALG_RSA_PKCS1V15_CRYPT &&
        PSA_ALG_IS_ECDH(alg) == 0) {
        return 0;
    }

    /* Filter out non allowed usage flags */
    if (usage == 0 ||
        (usage & ~(PSA_KEY_USAGE_SIGN_HASH |
                   PSA_KEY_USAGE_DECRYPT |
                   PSA_KEY_USAGE_DERIVE)) != 0) {
        return 0;
    }

    /* Wildcard hash is not allowed */
    if (PSA_ALG_IS_SIGN_HASH(alg) &&
        PSA_ALG_SIGN_GET_HASH(alg) == PSA_ALG_ANY_HASH) {
        return 0;
    }

    if (mbedtls_pk_get_type(ctx) != MBEDTLS_PK_OPAQUE) {
        mbedtls_pk_type_t type;

        if (PSA_ALG_IS_ECDSA(alg) || PSA_ALG_IS_ECDH(alg)) {
            type = MBEDTLS_PK_ECKEY;
        } else if (PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg) ||
                   alg == PSA_ALG_RSA_PKCS1V15_CRYPT) {
            type = MBEDTLS_PK_RSA;
        } else if (PSA_ALG_IS_RSA_PSS(alg)) {
            type = MBEDTLS_PK_RSASSA_PSS;
        } else {
            return 0;
        }

        if (ctx->pk_info->can_do(type) == 0) {
            return 0;
        }

        switch (type) {
            case MBEDTLS_PK_ECKEY:
                key_usage = PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_DERIVE;
                break;
            case MBEDTLS_PK_RSA:
            case MBEDTLS_PK_RSASSA_PSS:
                key_usage = PSA_KEY_USAGE_SIGN_HASH |
                            PSA_KEY_USAGE_SIGN_MESSAGE |
                            PSA_KEY_USAGE_DECRYPT;
                break;
            default:
                /* Should never happen */
                return 0;
        }

        return (key_usage & usage) == usage;
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status;

    status = psa_get_key_attributes(ctx->priv_id, &attributes);
    if (status != PSA_SUCCESS) {
        return 0;
    }

    psa_algorithm_t key_alg = psa_get_key_algorithm(&attributes);
    /* Key's enrollment is available only when an Mbed TLS implementation of PSA
     * Crypto is being used, i.e. when MBEDTLS_PSA_CRYPTO_C is defined.
     * Even though we don't officially support using other implementations of PSA
     * Crypto with TLS and X.509 (yet), we try to keep vendor's customizations
     * separated. */
#if defined(MBEDTLS_PSA_CRYPTO_C)
    psa_algorithm_t key_alg2 = psa_get_key_enrollment_algorithm(&attributes);
#endif /* MBEDTLS_PSA_CRYPTO_C */
    key_usage = psa_get_key_usage_flags(&attributes);
    psa_reset_key_attributes(&attributes);

    if ((key_usage & usage) != usage) {
        return 0;
    }

    /*
     * Common case: the key alg [or alg2] only allows alg.
     * This will match PSA_ALG_RSA_PKCS1V15_CRYPT & PSA_ALG_IS_ECDH
     * directly.
     * This would also match ECDSA/RSA_PKCS1V15_SIGN/RSA_PSS with
     * a fixed hash on key_alg [or key_alg2].
     */
    if (alg == key_alg) {
        return 1;
    }
#if defined(MBEDTLS_PSA_CRYPTO_C)
    if (alg == key_alg2) {
        return 1;
    }
#endif /* MBEDTLS_PSA_CRYPTO_C */

    /*
     * If key_alg [or key_alg2] is a hash-and-sign with a wildcard for the hash,
     * and alg is the same hash-and-sign family with any hash,
     * then alg is compliant with this key alg
     */
    if (PSA_ALG_IS_SIGN_HASH(alg)) {
        if (PSA_ALG_IS_SIGN_HASH(key_alg) &&
            PSA_ALG_SIGN_GET_HASH(key_alg) == PSA_ALG_ANY_HASH &&
            (alg & ~PSA_ALG_HASH_MASK) == (key_alg & ~PSA_ALG_HASH_MASK)) {
            return 1;
        }
#if defined(MBEDTLS_PSA_CRYPTO_C)
        if (PSA_ALG_IS_SIGN_HASH(key_alg2) &&
            PSA_ALG_SIGN_GET_HASH(key_alg2) == PSA_ALG_ANY_HASH &&
            (alg & ~PSA_ALG_HASH_MASK) == (key_alg2 & ~PSA_ALG_HASH_MASK)) {
            return 1;
        }
#endif /* MBEDTLS_PSA_CRYPTO_C */
    }

    return 0;
}

/* Check that the specified check_alg is compatible with key's type and algorithm.
 *
 * check_alg: the algorithm to verify compatibility for.
 * key_type: type of key being checked.
 * key_alg: algorithm associated with the key. This can be the main algorithm or
 *          the enrollment one, depending on which of the 2 is passed when calling
 *          this function.
 */
static int is_alg_compatible_with_key(psa_algorithm_t check_alg,
                                      psa_key_type_t key_type,
                                      psa_algorithm_t key_alg)
{
    /* Ensure that check_alg is compatible with key type */
    if (PSA_KEY_TYPE_IS_ECC(key_type)) {
        psa_ecc_family_t key_ec_family = PSA_KEY_TYPE_ECC_GET_FAMILY(key_type);
        if (PSA_ECC_FAMILY_IS_WEIERSTRASS(key_ec_family)) {
            if (!(PSA_ALG_IS_ECDH(check_alg) || PSA_ALG_IS_ECDSA(check_alg))) {
                return 0;
            }
        } else if (key_ec_family == PSA_ECC_FAMILY_MONTGOMERY) {
            if (!PSA_ALG_IS_ECDH(check_alg)) {
                return 0;
            }
        } else if (key_ec_family == PSA_ECC_FAMILY_TWISTED_EDWARDS) {
            if (!(PSA_ALG_IS_HASH_EDDSA(check_alg) || check_alg == PSA_ALG_PURE_EDDSA)) {
                return 0;
            }
        } else {
            return 0;
        }
    } else if (PSA_KEY_TYPE_IS_RSA(key_type)) {
        if (!(PSA_ALG_IS_RSA_PKCS1V15_SIGN(check_alg) || PSA_ALG_IS_RSA_PSS(check_alg) ||
              PSA_ALG_IS_RSA_OAEP(check_alg) || (check_alg == PSA_ALG_RSA_PKCS1V15_CRYPT))) {
            return 0;
        }
    } else {
        /* Unsupported key type */
        return 0;
    }

    /* Simplest case: perfect match */
    if (check_alg == key_alg) {
        return 1;
    }

    /* Check for PSA_ALG_ANY_HASH wildcard. */
    if (PSA_ALG_IS_SIGN_HASH(key_alg) && PSA_ALG_IS_SIGN_HASH(check_alg)) {
        if ((PSA_ALG_SIGN_GET_HASH(key_alg) == PSA_ALG_ANY_HASH) &&
            (check_alg & ~PSA_ALG_HASH_MASK) == (key_alg & ~PSA_ALG_HASH_MASK)) {
            return 1;
        }
    }

    return 0;
}

static int is_psa_key_compatible_with_alg_usage(mbedtls_svc_key_id_t key_id,
                                                psa_algorithm_t alg,
                                                psa_key_usage_t usage)
{
    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_type_t key_type;
    int ret = 0;

    if (psa_get_key_attributes(key_id, &key_attr) != PSA_SUCCESS) {
        return 0;
    }

    key_type = psa_get_key_type(&key_attr);

    /* PSA_KEY_USAGE_DERIVE_PUBLIC deserves a special treatment (see the
     * definition of the symbol for further details). Therefore we skip normal
     * checks and only verify that the key is an ECC one and that the requested
     * algorithm is PSA_ALG_ECDH.
     */
    if ((usage == PSA_KEY_USAGE_DERIVE_PUBLIC) && (alg == PSA_ALG_ECDH) &&
        PSA_KEY_TYPE_IS_ECC(key_type)) {
        ret = 1;
        goto exit;
    }

    ret = ((psa_get_key_usage_flags(&key_attr) & usage) == usage);
    if (ret == 0) {
        goto exit;
    }

    ret = is_alg_compatible_with_key(alg, key_type, psa_get_key_algorithm(&key_attr));
#if defined(MBEDTLS_PSA_CRYPTO_C)
    ret |= is_alg_compatible_with_key(alg, key_type, psa_get_key_enrollment_algorithm(&key_attr));
#endif /* MBEDTLS_PSA_CRYPTO_C */

exit:
    psa_reset_key_attributes(&key_attr);

    return ret;
}

int mbedtls_pk_can_do_psa(const mbedtls_pk_context *pk, psa_algorithm_t alg,
                          psa_key_usage_t usage)
{
    /* A context with null pk_info is not set up yet and can't do anything. */
    if (pk == NULL || pk->pk_info == NULL) {
        return 0;
    }

    /* Check algorithm <-> usage compatibility. */
    switch (usage) {
        case PSA_KEY_USAGE_SIGN_HASH:
        case PSA_KEY_USAGE_VERIFY_HASH:
            if (!PSA_ALG_IS_SIGN_HASH(alg)) {
                return 0;
            }
            break;
        case PSA_KEY_USAGE_DECRYPT:
        case PSA_KEY_USAGE_ENCRYPT:
            if (!((alg == PSA_ALG_RSA_PKCS1V15_CRYPT) || PSA_ALG_IS_RSA_OAEP(alg))) {
                return 0;
            }
            break;
        case PSA_KEY_USAGE_DERIVE:
        case PSA_KEY_USAGE_DERIVE_PUBLIC:
            if (!PSA_ALG_IS_ECDH(alg)) {
                return 0;
            }
            break;
        default:
            /* Reject unknown usages or multiple flags */
            return 0;
    }

    /* Basic checks on private and public keys availability */
    int has_private = !mbedtls_svc_key_id_is_null(pk->priv_id);
    int has_public = has_private || (pk->pub_raw_len > 0);
    int want_private = ((usage & (PSA_KEY_USAGE_SIGN_HASH |
                                  PSA_KEY_USAGE_DECRYPT |
                                  PSA_KEY_USAGE_DERIVE)) != 0);
    if ((!has_public && !has_private) ||
        (want_private && !has_private)) {
        return 0;
    }

    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        return is_psa_key_compatible_with_alg_usage(pk->priv_id, alg, usage);
    } else if (has_private) {
        return is_psa_key_compatible_with_alg_usage(pk->priv_id, alg, usage);
    } else {
        mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(pk);
        switch (pk_type) {
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
            case MBEDTLS_PK_RSA:
            case MBEDTLS_PK_RSASSA_PSS:
                if (PSA_ALG_IS_RSA_OAEP(alg) ||
                    PSA_ALG_IS_RSA_PSS(alg) ||
                    PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg) ||
                    (alg == PSA_ALG_RSA_PKCS1V15_CRYPT)) {
                    return 1;
                }
                break;
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
            case MBEDTLS_PK_ECKEY:
                if (PSA_ALG_IS_ECDH(alg) ||
                    (PSA_ALG_IS_ECDSA(alg) && pk->ec_family != PSA_ECC_FAMILY_MONTGOMERY)) {
                    return 1;
                }
                break;

            case MBEDTLS_PK_ECDSA:
                if (PSA_ALG_IS_ECDSA(alg) && pk->ec_family != PSA_ECC_FAMILY_MONTGOMERY) {
                    return 1;
                }
                break;

            case MBEDTLS_PK_ECKEY_DH:
                if (PSA_ALG_IS_ECDH(alg)) {
                    return 1;
                }
                break;
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

            default:
                return 0;
        }
    }

    return 0;
}

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)
int mbedtls_pk_get_psa_attributes(const mbedtls_pk_context *pk,
                                  psa_key_usage_t usage,
                                  psa_key_attributes_t *attributes)
{
    mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(pk);

    psa_key_usage_t more_usage = usage;
    if (usage == PSA_KEY_USAGE_SIGN_MESSAGE) {
        more_usage |= PSA_KEY_USAGE_VERIFY_MESSAGE;
    } else if (usage == PSA_KEY_USAGE_SIGN_HASH) {
        more_usage |= PSA_KEY_USAGE_VERIFY_HASH;
    } else if (usage == PSA_KEY_USAGE_DECRYPT) {
        more_usage |= PSA_KEY_USAGE_ENCRYPT;
    }
    more_usage |= PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_COPY;

    int want_private = !(usage == PSA_KEY_USAGE_VERIFY_MESSAGE ||
                         usage == PSA_KEY_USAGE_VERIFY_HASH ||
                         usage == PSA_KEY_USAGE_ENCRYPT);

    switch (pk_type) {
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
        case MBEDTLS_PK_RSA:
        {
            psa_algorithm_t alg = 0;
            switch (usage) {
                case PSA_KEY_USAGE_SIGN_MESSAGE:
                case PSA_KEY_USAGE_SIGN_HASH:
                case PSA_KEY_USAGE_VERIFY_MESSAGE:
                case PSA_KEY_USAGE_VERIFY_HASH:
                    alg = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_ANY_HASH);
                    break;
                case PSA_KEY_USAGE_DECRYPT:
                case PSA_KEY_USAGE_ENCRYPT:
                    alg = PSA_ALG_RSA_PKCS1V15_CRYPT;
                    break;
                default:
                    return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            int has_private = !mbedtls_svc_key_id_is_null(pk->priv_id);
            if (want_private && !has_private) {
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            psa_set_key_type(attributes, (want_private ?
                                          PSA_KEY_TYPE_RSA_KEY_PAIR :
                                          PSA_KEY_TYPE_RSA_PUBLIC_KEY));
            psa_set_key_bits(attributes, mbedtls_pk_get_bitlen(pk));
            psa_set_key_algorithm(attributes, alg);
            break;
        }
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
        case MBEDTLS_PK_ECKEY:
        case MBEDTLS_PK_ECKEY_DH:
        case MBEDTLS_PK_ECDSA:
        {
            int sign_ok = (pk_type != MBEDTLS_PK_ECKEY_DH);
            int derive_ok = (pk_type != MBEDTLS_PK_ECDSA);
            psa_ecc_family_t family = pk->ec_family;
            size_t bits = pk->bits;
            int has_private = 0;
            psa_algorithm_t alg = 0;

            if (!mbedtls_svc_key_id_is_null(pk->priv_id)) {
                has_private = 1;
            }
            switch (usage) {
                case PSA_KEY_USAGE_SIGN_MESSAGE:
                case PSA_KEY_USAGE_SIGN_HASH:
                case PSA_KEY_USAGE_VERIFY_MESSAGE:
                case PSA_KEY_USAGE_VERIFY_HASH:
                    if (!sign_ok) {
                        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
                    }
                    alg = MBEDTLS_PK_ALG_ECDSA(PSA_ALG_ANY_HASH);
                    break;
                case PSA_KEY_USAGE_DERIVE:
                    alg = PSA_ALG_ECDH;
                    if (!derive_ok) {
                        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
                    }
                    break;
                default:
                    return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            if (want_private && !has_private) {
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            psa_set_key_type(attributes, (want_private ?
                                          PSA_KEY_TYPE_ECC_KEY_PAIR(family) :
                                          PSA_KEY_TYPE_ECC_PUBLIC_KEY(family)));
            psa_set_key_bits(attributes, bits);
            psa_set_key_algorithm(attributes, alg);
            break;
        }
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

        case MBEDTLS_PK_OPAQUE:
        {
            psa_key_attributes_t old_attributes = PSA_KEY_ATTRIBUTES_INIT;
            psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
            status = psa_get_key_attributes(pk->priv_id, &old_attributes);
            if (status != PSA_SUCCESS) {
                return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
            }
            psa_key_type_t old_type = psa_get_key_type(&old_attributes);
            switch (usage) {
                case PSA_KEY_USAGE_SIGN_MESSAGE:
                case PSA_KEY_USAGE_SIGN_HASH:
                case PSA_KEY_USAGE_VERIFY_MESSAGE:
                case PSA_KEY_USAGE_VERIFY_HASH:
                    if (!(PSA_KEY_TYPE_IS_ECC_KEY_PAIR(old_type) ||
                          old_type == PSA_KEY_TYPE_RSA_KEY_PAIR)) {
                        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
                    }
                    break;
                case PSA_KEY_USAGE_DECRYPT:
                case PSA_KEY_USAGE_ENCRYPT:
                    if (old_type != PSA_KEY_TYPE_RSA_KEY_PAIR) {
                        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
                    }
                    break;
                case PSA_KEY_USAGE_DERIVE:
                    if (!(PSA_KEY_TYPE_IS_ECC_KEY_PAIR(old_type))) {
                        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
                    }
                    break;
                default:
                    return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            psa_key_type_t new_type = old_type;
            /* Opaque keys are always key pairs, so we don't need a check
             * on the input if the required usage is private. We just need
             * to adjust the type correctly if the required usage is public. */
            if (!want_private) {
                new_type = PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(new_type);
            }
            more_usage = psa_get_key_usage_flags(&old_attributes);
            if ((usage & more_usage) == 0) {
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            psa_set_key_type(attributes, new_type);
            psa_set_key_bits(attributes, psa_get_key_bits(&old_attributes));
            psa_set_key_algorithm(attributes, psa_get_key_algorithm(&old_attributes));
            break;
        }

        default:
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    psa_set_key_usage_flags(attributes, more_usage);
    /* Key's enrollment is available only when an Mbed TLS implementation of PSA
     * Crypto is being used, i.e. when MBEDTLS_PSA_CRYPTO_C is defined.
     * Even though we don't officially support using other implementations of PSA
     * Crypto with TLS and X.509 (yet), we try to keep vendor's customizations
     * separated. */
#if defined(MBEDTLS_PSA_CRYPTO_C)
    psa_set_key_enrollment_algorithm(attributes, PSA_ALG_NONE);
#endif

    return 0;
}

static psa_status_t export_import_into_psa(mbedtls_svc_key_id_t old_key_id,
                                           const psa_key_attributes_t *attributes,
                                           mbedtls_svc_key_id_t *new_key_id)
{
    unsigned char key_buffer[PSA_EXPORT_KEY_PAIR_MAX_SIZE];
    size_t key_length = 0;
    psa_status_t status = psa_export_key(old_key_id,
                                         key_buffer, sizeof(key_buffer),
                                         &key_length);
    if (status != PSA_SUCCESS) {
        return status;
    }
    status = psa_import_key(attributes, key_buffer, key_length, new_key_id);
    mbedtls_platform_zeroize(key_buffer, key_length);
    return status;
}

static int copy_into_psa(mbedtls_svc_key_id_t old_key_id,
                         const psa_key_attributes_t *attributes,
                         mbedtls_svc_key_id_t *new_key_id)
{
    /* Normally, we prefer copying: it's more efficient and works even
     * for non-exportable keys. */
    psa_status_t status = psa_copy_key(old_key_id, attributes, new_key_id);
    if (status == PSA_ERROR_NOT_PERMITTED /*missing COPY usage*/ ||
        status == PSA_ERROR_INVALID_ARGUMENT /*incompatible policy*/) {
        /* There are edge cases where copying won't work, but export+import
         * might:
         * - If the old key does not allow PSA_KEY_USAGE_COPY.
         * - If the old key's usage does not allow what attributes wants.
         *   Because the key was intended for use in the pk module, and may
         *   have had a policy chosen solely for what pk needs rather than
         *   based on a detailed understanding of PSA policies, we are a bit
         *   more liberal than psa_copy_key() here.
         */
        /* Here we need to check that the types match, otherwise we risk
         * importing nonsensical data. */
        psa_key_attributes_t old_attributes = PSA_KEY_ATTRIBUTES_INIT;
        status = psa_get_key_attributes(old_key_id, &old_attributes);
        if (status != PSA_SUCCESS) {
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        }
        psa_key_type_t old_type = psa_get_key_type(&old_attributes);
        psa_reset_key_attributes(&old_attributes);
        if (old_type != psa_get_key_type(attributes)) {
            return MBEDTLS_ERR_PK_TYPE_MISMATCH;
        }
        status = export_import_into_psa(old_key_id, attributes, new_key_id);
    }
    return PSA_PK_TO_MBEDTLS_ERR(status);
}

static int import_pair_into_psa(const mbedtls_pk_context *pk,
                                const psa_key_attributes_t *attributes,
                                mbedtls_svc_key_id_t *key_id)
{
    switch (mbedtls_pk_get_type(pk)) {
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
        case MBEDTLS_PK_RSA:
        {
            if (psa_get_key_type(attributes) != PSA_KEY_TYPE_RSA_KEY_PAIR) {
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            if (mbedtls_svc_key_id_is_null(pk->priv_id)) {
                /* We have a public key and want a key pair. */
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            return copy_into_psa(pk->priv_id, attributes, key_id);
        }
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
        case MBEDTLS_PK_ECKEY:
        case MBEDTLS_PK_ECKEY_DH:
        case MBEDTLS_PK_ECDSA:
        {
            /* We need to check the curve family, otherwise the import could
             * succeed with nonsensical data.
             * We don't check the bit-size: it's optional in attributes,
             * and if it's specified, psa_import_key() will know from the key
             * data length and will check that the bit-size matches. */
            psa_key_type_t to_type = psa_get_key_type(attributes);
            psa_ecc_family_t from_family = pk->ec_family;
            if (to_type != PSA_KEY_TYPE_ECC_KEY_PAIR(from_family)) {
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }

            if (mbedtls_svc_key_id_is_null(pk->priv_id)) {
                /* We have a public key and want a key pair. */
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            return copy_into_psa(pk->priv_id, attributes, key_id);
        }
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

        case MBEDTLS_PK_OPAQUE:
            return copy_into_psa(pk->priv_id, attributes, key_id);

        default:
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }
}

static int import_public_into_psa(const mbedtls_pk_context *pk,
                                  const psa_key_attributes_t *attributes,
                                  mbedtls_svc_key_id_t *key_id)
{
    psa_key_type_t psa_type = psa_get_key_type(attributes);
    unsigned char key_buffer[PSA_EXPORT_PUBLIC_KEY_MAX_SIZE];
    unsigned char *key_data = NULL;
    size_t key_length = 0;

    switch (mbedtls_pk_get_type(pk)) {
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
        case MBEDTLS_PK_RSA:
        {
            if (psa_type != PSA_KEY_TYPE_RSA_PUBLIC_KEY) {
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            key_data = (unsigned char *) pk->pub_raw;
            key_length = pk->pub_raw_len;
            break;
        }
#endif /*PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
        case MBEDTLS_PK_ECKEY:
        case MBEDTLS_PK_ECKEY_DH:
        case MBEDTLS_PK_ECDSA:
        {
            /* We need to check the curve family, otherwise the import could
             * succeed with nonsensical data.
             * We don't check the bit-size: it's optional in attributes,
             * and if it's specified, psa_import_key() will know from the key
             * data length and will check that the bit-size matches. */
            if (psa_type != PSA_KEY_TYPE_ECC_PUBLIC_KEY(pk->ec_family)) {
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            key_data = (unsigned char *) pk->pub_raw;
            key_length = pk->pub_raw_len;
            break;
        }
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

        case MBEDTLS_PK_OPAQUE:
        {
            psa_key_attributes_t old_attributes = PSA_KEY_ATTRIBUTES_INIT;
            psa_status_t status =
                psa_get_key_attributes(pk->priv_id, &old_attributes);
            if (status != PSA_SUCCESS) {
                return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
            }
            psa_key_type_t old_type = psa_get_key_type(&old_attributes);
            psa_reset_key_attributes(&old_attributes);
            if (psa_type != PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(old_type)) {
                return MBEDTLS_ERR_PK_TYPE_MISMATCH;
            }
            status = psa_export_public_key(pk->priv_id,
                                           key_buffer, sizeof(key_buffer),
                                           &key_length);
            if (status != PSA_SUCCESS) {
                return PSA_PK_TO_MBEDTLS_ERR(status);
            }
            key_data = key_buffer;
            break;
        }

        default:
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    return PSA_PK_TO_MBEDTLS_ERR(psa_import_key(attributes,
                                                key_data, key_length,
                                                key_id));
}

int mbedtls_pk_import_into_psa(const mbedtls_pk_context *pk,
                               const psa_key_attributes_t *attributes,
                               mbedtls_svc_key_id_t *key_id)
{
    /* Set the output immediately so that it won't contain garbage even
     * if we error out before calling psa_import_key(). */
    *key_id = MBEDTLS_SVC_KEY_ID_INIT;

    int want_public = PSA_KEY_TYPE_IS_PUBLIC_KEY(psa_get_key_type(attributes));
    if (want_public) {
        return import_public_into_psa(pk, attributes, key_id);
    } else {
        return import_pair_into_psa(pk, attributes, key_id);
    }
}

static int copy_from_psa(mbedtls_svc_key_id_t key_id,
                         mbedtls_pk_context *pk,
                         int public_only)
{
    psa_status_t status;
    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_type_t key_type;
    size_t key_bits;
    /* Use a buffer size large enough to contain either a key pair or public key. */
    unsigned char exp_key[PSA_EXPORT_KEY_PAIR_OR_PUBLIC_MAX_SIZE];
    size_t exp_key_len;
    int ret = MBEDTLS_ERR_PK_BAD_INPUT_DATA;

    if (pk == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    status = psa_get_key_attributes(key_id, &key_attr);
    if (status != PSA_SUCCESS) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (public_only) {
        status = psa_export_public_key(key_id, exp_key, sizeof(exp_key), &exp_key_len);
    } else {
        status = psa_export_key(key_id, exp_key, sizeof(exp_key), &exp_key_len);
    }
    if (status != PSA_SUCCESS) {
        ret = PSA_PK_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    key_type = psa_get_key_type(&key_attr);
    if (public_only) {
        key_type = PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(key_type);
    }
    key_bits = psa_get_key_bits(&key_attr);

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
    if ((key_type == PSA_KEY_TYPE_RSA_KEY_PAIR) ||
        (key_type == PSA_KEY_TYPE_RSA_PUBLIC_KEY)) {

        ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
        if (ret != 0) {
            goto exit;
        }

        if (key_type == PSA_KEY_TYPE_RSA_KEY_PAIR) {
            ret = mbedtls_pk_rsa_set_key(pk, exp_key, exp_key_len);
            if (ret != 0) {
                goto exit;
            }
            ret = mbedtls_pk_rsa_set_pubkey_from_prv(pk);
        } else {
            ret = mbedtls_pk_rsa_set_pubkey(pk, exp_key, exp_key_len);
        }
        if (ret != 0) {
            goto exit;
        }
    } else
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
    if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(key_type) ||
        PSA_KEY_TYPE_IS_ECC_PUBLIC_KEY(key_type)) {
        mbedtls_ecp_group_id grp_id;

        ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
        if (ret != 0) {
            goto exit;
        }

        grp_id = mbedtls_ecc_group_from_psa(PSA_KEY_TYPE_ECC_GET_FAMILY(key_type), key_bits);
        ret = mbedtls_pk_ecc_set_group(pk, grp_id);
        if (ret != 0) {
            goto exit;
        }

        if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(key_type)) {
            ret = mbedtls_pk_ecc_set_key(pk, exp_key, exp_key_len);
            if (ret != 0) {
                goto exit;
            }
            ret = mbedtls_pk_ecc_set_pubkey_from_prv(pk, exp_key, exp_key_len);
        } else {
            ret = mbedtls_pk_ecc_set_pubkey(pk, exp_key, exp_key_len);
        }
        if (ret != 0) {
            goto exit;
        }
    } else
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
    {
        (void) key_bits;
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

exit:
    psa_reset_key_attributes(&key_attr);
    mbedtls_platform_zeroize(exp_key, sizeof(exp_key));

    return ret;
}

int mbedtls_pk_copy_from_psa(mbedtls_svc_key_id_t key_id,
                             mbedtls_pk_context *pk)
{
    return copy_from_psa(key_id, pk, 0);
}

int mbedtls_pk_copy_public_from_psa(mbedtls_svc_key_id_t key_id,
                                    mbedtls_pk_context *pk)
{
    return copy_from_psa(key_id, pk, 1);
}
#endif /* MBEDTLS_PSA_CRYPTO_CLIENT */

/*
 * Helper for mbedtls_pk_sign and mbedtls_pk_verify
 */
static inline int pk_hashlen_helper(mbedtls_md_type_t md_alg, size_t *hash_len)
{
    if (*hash_len != 0) {
        return 0;
    }

    *hash_len = mbedtls_md_get_size_from_type(md_alg);

    if (*hash_len == 0) {
        return -1;
    }

    return 0;
}

#if defined(MBEDTLS_ECP_RESTARTABLE)
/*
 * Helper to set up a restart context if needed
 */
static int pk_restart_setup(mbedtls_pk_restart_ctx *ctx,
                            const mbedtls_pk_info_t *info,
                            mbedtls_pk_rs_op_t rs_op)
{
    /* Don't do anything if already set up or invalid */
    if (ctx == NULL || ctx->pk_info != NULL) {
        return 0;
    }

    /* Should never happen when we're called */
    if (info->rs_alloc_func == NULL || info->rs_free_func == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if ((ctx->rs_ctx = info->rs_alloc_func(rs_op)) == NULL) {
        return MBEDTLS_ERR_PK_ALLOC_FAILED;
    }

    ctx->pk_info = info;

    return 0;
}
#endif /* MBEDTLS_ECP_RESTARTABLE */

/*
 * Verify a signature (restartable)
 */
int mbedtls_pk_verify_restartable(mbedtls_pk_context *ctx,
                                  mbedtls_md_type_t md_alg,
                                  const unsigned char *hash, size_t hash_len,
                                  const unsigned char *sig, size_t sig_len,
                                  mbedtls_pk_restart_ctx *rs_ctx)
{
    if ((md_alg != MBEDTLS_MD_NONE || hash_len != 0) && hash == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (ctx->pk_info == NULL ||
        pk_hashlen_helper(md_alg, &hash_len) != 0) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_ECP_RESTARTABLE)
    int is_restartable_enabled = psa_interruptible_get_max_ops() != 0;
    /* optimization: use non-restartable version if restart disabled */
    if (rs_ctx != NULL &&
        is_restartable_enabled &&
        ctx->pk_info->verify_rs_func != NULL) {
        int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

        ret = pk_restart_setup(rs_ctx, ctx->pk_info, MBEDTLS_PK_RS_OP_VERIFY);
        if (ret != 0) {
            return ret;
        }

        ret = ctx->pk_info->verify_rs_func(ctx,
                                           md_alg, hash, hash_len, sig, sig_len, rs_ctx->rs_ctx);

        if (ret != MBEDTLS_ERR_ECP_IN_PROGRESS) {
            mbedtls_pk_restart_free(rs_ctx);
        }

        return ret;
    }
#else /* MBEDTLS_ECP_RESTARTABLE */
    (void) rs_ctx;
#endif /* MBEDTLS_ECP_RESTARTABLE */

    if (ctx->pk_info->verify_func == NULL) {
        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
    }

    return ctx->pk_info->verify_func(ctx, md_alg, hash, hash_len,
                                     sig, sig_len);
}

/*
 * Verify a signature
 */
int mbedtls_pk_verify(mbedtls_pk_context *ctx, mbedtls_md_type_t md_alg,
                      const unsigned char *hash, size_t hash_len,
                      const unsigned char *sig, size_t sig_len)
{
    return mbedtls_pk_verify_restartable(ctx, md_alg, hash, hash_len,
                                         sig, sig_len, NULL);
}

/*
 * Verify a signature, with explicit selection of the signature algorithm.
 */
int mbedtls_pk_verify_ext(mbedtls_pk_sigalg_t type,
                          mbedtls_pk_context *ctx, mbedtls_md_type_t md_alg,
                          const unsigned char *hash, size_t hash_len,
                          const unsigned char *sig, size_t sig_len)
{
    if ((md_alg != MBEDTLS_MD_NONE || hash_len != 0) && hash == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (ctx->pk_info == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (!mbedtls_pk_can_do(ctx, (mbedtls_pk_type_t) type)) {
        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
    }

    if (type != MBEDTLS_PK_SIGALG_RSA_PSS) {
        return mbedtls_pk_verify(ctx, md_alg, hash, hash_len, sig, sig_len);
    }

    /* Ensure the PK context is of the right type otherwise mbedtls_pk_rsa()
     * below would return a NULL pointer. */
    if (mbedtls_pk_get_type(ctx) != MBEDTLS_PK_RSA) {
        return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
    }

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)

#if SIZE_MAX > UINT_MAX
    if (md_alg == MBEDTLS_MD_NONE && UINT_MAX < hash_len) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }
#endif

    size_t signature_length;
    psa_status_t status = PSA_ERROR_DATA_CORRUPT;
    psa_status_t destruction_status = PSA_ERROR_DATA_CORRUPT;

    psa_algorithm_t psa_md_alg = mbedtls_md_psa_alg_from_type(md_alg);
    mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_algorithm_t psa_sig_alg = PSA_ALG_RSA_PSS_ANY_SALT(psa_md_alg);

    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attributes, psa_sig_alg);

    status = psa_import_key(&attributes,
                            ctx->pub_raw, ctx->pub_raw_len,
                            &key_id);
    if (status != PSA_SUCCESS) {
        psa_destroy_key(key_id);
        return PSA_PK_TO_MBEDTLS_ERR(status);
    }

    /* This function must fail on a valid signature with trailing data in a
     * buffer (checked below). Moreover mbedtls_psa_rsa_verify_hash() requires
     * the sig_len to be exact. For this reason the passed sig_len is
     * overwritten. Smaller signature lengths should not be accepted for
     * verification. */
    signature_length = sig_len > mbedtls_pk_get_len(ctx) ?
                       mbedtls_pk_get_len(ctx) : sig_len;
    status = psa_verify_hash(key_id, psa_sig_alg, hash,
                             hash_len, sig, signature_length);
    destruction_status = psa_destroy_key(key_id);

    if (status == PSA_SUCCESS && sig_len > mbedtls_pk_get_len(ctx)) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (status == PSA_SUCCESS) {
        status = destruction_status;
    }

    return PSA_PK_RSA_TO_MBEDTLS_ERR(status);
#else
    return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
}

/*
 * Verify a signature
 */
int mbedtls_pk_verify_new(mbedtls_pk_type_t type, mbedtls_pk_context *ctx,
                          mbedtls_md_type_t md_alg, const unsigned char *hash,
                          size_t hash_len, const unsigned char *sig, size_t sig_len)
{
    return mbedtls_pk_verify_ext((mbedtls_pk_sigalg_t) type,
                                 ctx,
                                 md_alg,
                                 hash,
                                 hash_len,
                                 sig,
                                 sig_len);
}

/*
 * Make a signature (restartable)
 */
int mbedtls_pk_sign_restartable(mbedtls_pk_context *ctx,
                                mbedtls_md_type_t md_alg,
                                const unsigned char *hash, size_t hash_len,
                                unsigned char *sig, size_t sig_size, size_t *sig_len,
                                mbedtls_pk_restart_ctx *rs_ctx)
{
    if ((md_alg != MBEDTLS_MD_NONE || hash_len != 0) && hash == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (ctx->pk_info == NULL || pk_hashlen_helper(md_alg, &hash_len) != 0) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_ECP_RESTARTABLE)
    int is_restartable_enabled = psa_interruptible_get_max_ops() != 0;
    /* optimization: use non-restartable version if restart disabled */
    if (rs_ctx != NULL &&
        is_restartable_enabled &&
        ctx->pk_info->sign_rs_func != NULL) {
        int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

        ret = pk_restart_setup(rs_ctx, ctx->pk_info, MBEDTLS_PK_RS_OP_SIGN);
        if (ret != 0) {
            return ret;
        }

        ret = ctx->pk_info->sign_rs_func(ctx, md_alg,
                                         hash, hash_len,
                                         sig, sig_size, sig_len,
                                         rs_ctx->rs_ctx);

        if (ret != MBEDTLS_ERR_ECP_IN_PROGRESS) {
            mbedtls_pk_restart_free(rs_ctx);
        }

        return ret;
    }
#else /* MBEDTLS_ECP_RESTARTABLE */
    (void) rs_ctx;
#endif /* MBEDTLS_ECP_RESTARTABLE */

    if (ctx->pk_info->sign_func == NULL) {
        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
    }

    return ctx->pk_info->sign_func(ctx, md_alg,
                                   hash, hash_len,
                                   sig, sig_size, sig_len);
}

/*
 * Make a signature
 */
int mbedtls_pk_sign(mbedtls_pk_context *ctx, mbedtls_md_type_t md_alg,
                    const unsigned char *hash, size_t hash_len,
                    unsigned char *sig, size_t sig_size, size_t *sig_len)
{
    return mbedtls_pk_sign_restartable(ctx, md_alg, hash, hash_len,
                                       sig, sig_size, sig_len,
                                       NULL);
}

/*
 * Make a signature given a signature type.
 */
int mbedtls_pk_sign_ext(mbedtls_pk_sigalg_t pk_type,
                        mbedtls_pk_context *ctx,
                        mbedtls_md_type_t md_alg,
                        const unsigned char *hash, size_t hash_len,
                        unsigned char *sig, size_t sig_size, size_t *sig_len)
{
    if (ctx->pk_info == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (!mbedtls_pk_can_do(ctx, (mbedtls_pk_type_t) pk_type)) {
        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
    }

    if (pk_type != MBEDTLS_PK_SIGALG_RSA_PSS) {
        return mbedtls_pk_sign(ctx, md_alg, hash, hash_len,
                               sig, sig_size, sig_len);
    }

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
    const psa_algorithm_t psa_md_alg = mbedtls_md_psa_alg_from_type(md_alg);
    if (psa_md_alg == 0) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (mbedtls_pk_get_type(ctx) == MBEDTLS_PK_OPAQUE) {
        psa_status_t status;

        /* PSA_ALG_RSA_PSS() behaves the same as PSA_ALG_RSA_PSS_ANY_SALT() when
         * performing a signature, but they are encoded differently. Instead of
         * extracting the proper one from the wrapped key policy, just try both. */
        status = psa_sign_hash(ctx->priv_id, PSA_ALG_RSA_PSS(psa_md_alg),
                               hash, hash_len,
                               sig, sig_size, sig_len);
        if (status == PSA_ERROR_NOT_PERMITTED) {
            status = psa_sign_hash(ctx->priv_id, PSA_ALG_RSA_PSS_ANY_SALT(psa_md_alg),
                                   hash, hash_len,
                                   sig, sig_size, sig_len);
        }
        return PSA_PK_RSA_TO_MBEDTLS_ERR(status);
    }

    return mbedtls_pk_psa_rsa_sign_ext(PSA_ALG_RSA_PSS(psa_md_alg),
                                       ctx, hash, hash_len,
                                       sig, sig_size, sig_len);
#else
    return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
}

/*
 * Check public-private key pair
 */
int mbedtls_pk_check_pair(const mbedtls_pk_context *pub,
                          const mbedtls_pk_context *prv)
{
    if (pub->pk_info == NULL ||
        prv->pk_info == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (prv->pk_info->check_pair_func == NULL) {
        return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
    }

    if ((prv->pk_info->type != MBEDTLS_PK_OPAQUE) &&
        (pub->pk_info != prv->pk_info)) {
        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
    }

    return prv->pk_info->check_pair_func((mbedtls_pk_context *) pub,
                                         (mbedtls_pk_context *) prv);
}

/*
 * Get key size in bits
 */
size_t mbedtls_pk_get_bitlen(const mbedtls_pk_context *ctx)
{
    /* For backward compatibility, accept NULL or a context that
     * isn't set up yet, and return a fake value that should be safe. */
    if (ctx == NULL || ctx->pk_info == NULL) {
        return 0;
    }

    return ctx->pk_info->get_bitlen((mbedtls_pk_context *) ctx);
}

/*
 * Export debug information
 */
int mbedtls_pk_debug(const mbedtls_pk_context *ctx, mbedtls_pk_debug_item *items)
{
    if (ctx->pk_info == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (ctx->pk_info->debug_func == NULL) {
        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
    }

    ctx->pk_info->debug_func((mbedtls_pk_context *) ctx, items);
    return 0;
}

/*
 * Access the PK type name
 */
const char *mbedtls_pk_get_name(const mbedtls_pk_context *ctx)
{
    if (ctx == NULL || ctx->pk_info == NULL) {
        return "invalid PK";
    }

    return ctx->pk_info->name;
}

/*
 * Access the PK type
 */
mbedtls_pk_type_t mbedtls_pk_get_type(const mbedtls_pk_context *ctx)
{
    if (ctx == NULL || ctx->pk_info == NULL) {
        return MBEDTLS_PK_NONE;
    }

    return ctx->pk_info->type;
}

#endif /* MBEDTLS_PK_C */

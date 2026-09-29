/*
 *  Public Key abstraction layer: wrapper functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_PK_C)
#include "pk_wrap.h"
#include "pk_internal.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/psa_util.h"

/* Even if RSA not activated, for the sake of RSA-alt */
#include "mbedtls/private/rsa.h"

#if defined(MBEDTLS_ECP_C)
#include "mbedtls/private/ecp.h"
#endif

#if defined(MBEDTLS_ECDSA_C)
#include "mbedtls/private/ecdsa.h"
#endif

#include "psa_util_internal.h"
#include "psa/crypto.h"
#include "mbedtls/psa_util.h"

#if defined(PSA_HAVE_ALG_SOME_ECDSA)
#include "mbedtls/asn1write.h"
#include "mbedtls/asn1.h"
#endif

#include "mbedtls/platform.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
static int rsa_can_do(mbedtls_pk_type_t type)
{
    return type == MBEDTLS_PK_RSA ||
           type == MBEDTLS_PK_RSASSA_PSS;
}

static size_t rsa_get_bitlen(mbedtls_pk_context *pk)
{
    return pk->bits;
}

static int rsa_verify_wrap(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                           const unsigned char *hash, size_t hash_len,
                           const unsigned char *sig, size_t sig_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
    psa_status_t status;
    psa_algorithm_t psa_alg_md;
    size_t rsa_len = mbedtls_pk_get_len(pk);

#if SIZE_MAX > UINT_MAX
    if (md_alg == MBEDTLS_MD_NONE && UINT_MAX < hash_len) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }
#endif

    if (pk->rsa_padding == MBEDTLS_PK_RSA_PKCS_V21) {
        psa_alg_md = PSA_ALG_RSA_PSS(mbedtls_md_psa_alg_from_type(md_alg));
    } else {
        psa_alg_md = PSA_ALG_RSA_PKCS1V15_SIGN(mbedtls_md_psa_alg_from_type(md_alg));
    }

    if (sig_len < rsa_len) {
        return MBEDTLS_ERR_RSA_VERIFY_FAILED;
    }

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attributes, psa_alg_md);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_PUBLIC_KEY);

    status = psa_import_key(&attributes, pk->pub_raw, pk->pub_raw_len, &key_id);
    if (status != PSA_SUCCESS) {
        ret = PSA_PK_TO_MBEDTLS_ERR(status);
        goto cleanup;
    }

    status = psa_verify_hash(key_id, psa_alg_md, hash, hash_len,
                             sig, sig_len);
    if (status != PSA_SUCCESS) {
        ret = PSA_PK_RSA_TO_MBEDTLS_ERR(status);
        goto cleanup;
    }
    ret = 0;

cleanup:
    status = psa_destroy_key(key_id);
    if (ret == 0 && status != PSA_SUCCESS) {
        ret = PSA_PK_TO_MBEDTLS_ERR(status);
    }

    return ret;
}

int  mbedtls_pk_psa_rsa_sign_ext(psa_algorithm_t alg,
                                 mbedtls_pk_context *pk,
                                 const unsigned char *hash, size_t hash_len,
                                 unsigned char *sig, size_t sig_size,
                                 size_t *sig_len)
{
    psa_status_t status;

    *sig_len = mbedtls_pk_get_len(pk);
    if (sig_size < *sig_len) {
        return MBEDTLS_ERR_PK_BUFFER_TOO_SMALL;
    }

    status = psa_sign_hash(pk->priv_id, alg, hash, hash_len,
                           sig, sig_size, sig_len);
    return PSA_PK_TO_MBEDTLS_ERR(status);
}

static int rsa_sign_wrap(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                         const unsigned char *hash, size_t hash_len,
                         unsigned char *sig, size_t sig_size, size_t *sig_len)
{
    psa_algorithm_t psa_md_alg;
    psa_md_alg = mbedtls_md_psa_alg_from_type(md_alg);
    if (psa_md_alg == 0) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }
    psa_algorithm_t psa_alg;
    if (pk->rsa_padding == MBEDTLS_PK_RSA_PKCS_V21) {
        psa_alg = PSA_ALG_RSA_PSS(psa_md_alg);
    } else {
        psa_alg = PSA_ALG_RSA_PKCS1V15_SIGN(psa_md_alg);
    }

    return mbedtls_pk_psa_rsa_sign_ext(psa_alg, pk, hash, hash_len,
                                       sig, sig_size, sig_len);
}

static int rsa_check_pair_wrap(mbedtls_pk_context *pub, mbedtls_pk_context *prv)
{
    psa_status_t status;
    uint8_t exp_pub_key[MBEDTLS_PK_MAX_RSA_PUBKEY_RAW_LEN];
    size_t exp_pub_key_len;

    status = psa_export_public_key(prv->priv_id, exp_pub_key, sizeof(exp_pub_key),
                                   &exp_pub_key_len);
    if (status != PSA_SUCCESS) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    if (memcmp(pub->pub_raw, exp_pub_key, exp_pub_key_len) != 0) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }
    return 0;
}

static void rsa_debug(mbedtls_pk_context *pk, mbedtls_pk_debug_item *items)
{
    items->type = MBEDTLS_PK_DEBUG_PSA_RSA;
    items->name = "rsa";
    items->value = pk;
}

const mbedtls_pk_info_t mbedtls_rsa_info = {
    .type = MBEDTLS_PK_RSA,
    .name = "RSA",
    .get_bitlen = rsa_get_bitlen,
    .can_do = rsa_can_do,
    .verify_func = rsa_verify_wrap,
    .sign_func = rsa_sign_wrap,
#if defined(MBEDTLS_ECP_RESTARTABLE)
    .verify_rs_func = NULL,
    .sign_rs_func = NULL,
    .rs_alloc_func = NULL,
    .rs_free_func = NULL,
#endif /* MBEDTLS_ECP_RESTARTABLE */
    .check_pair_func = rsa_check_pair_wrap,
    .ctx_alloc_func = NULL,
    .ctx_free_func = NULL,
    .debug_func = rsa_debug,
};
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
/*
 * Generic EC key
 */
static int eckey_can_do(mbedtls_pk_type_t type)
{
    return type == MBEDTLS_PK_ECKEY ||
           type == MBEDTLS_PK_ECKEY_DH ||
           type == MBEDTLS_PK_ECDSA;
}

static size_t eckey_get_bitlen(mbedtls_pk_context *pk)
{
    return pk->bits;
}

#if defined(PSA_HAVE_ALG_ECDSA_VERIFY)
/* Common helper for ECDSA verify using PSA functions. */
static int ecdsa_verify_psa(unsigned char *key, size_t key_len,
                            psa_ecc_family_t curve, size_t curve_bits,
                            const unsigned char *hash, size_t hash_len,
                            const unsigned char *sig, size_t sig_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
    psa_algorithm_t psa_sig_md = PSA_ALG_ECDSA_ANY;
    size_t signature_len = PSA_ECDSA_SIGNATURE_SIZE(curve_bits);
    size_t converted_sig_len;
    unsigned char extracted_sig[PSA_VENDOR_ECDSA_SIGNATURE_MAX_SIZE];
    unsigned char *p;
    psa_status_t status;

    if (curve == 0) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(curve));
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attributes, psa_sig_md);

    status = psa_import_key(&attributes, key, key_len, &key_id);
    if (status != PSA_SUCCESS) {
        ret = PSA_PK_TO_MBEDTLS_ERR(status);
        goto cleanup;
    }

    if (signature_len > sizeof(extracted_sig)) {
        ret = MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        goto cleanup;
    }

    p = (unsigned char *) sig;
    ret = mbedtls_ecdsa_der_to_raw(curve_bits, p, sig_len, extracted_sig,
                                   sizeof(extracted_sig), &converted_sig_len);
    if (ret != 0) {
        goto cleanup;
    }

    if (converted_sig_len != signature_len) {
        ret = MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        goto cleanup;
    }

    status = psa_verify_hash(key_id, psa_sig_md, hash, hash_len,
                             extracted_sig, signature_len);
    if (status != PSA_SUCCESS) {
        ret = PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
        goto cleanup;
    }

    ret = 0;

cleanup:
    status = psa_destroy_key(key_id);
    if (ret == 0 && status != PSA_SUCCESS) {
        ret = PSA_PK_TO_MBEDTLS_ERR(status);
    }

    return ret;
}

static int ecdsa_opaque_verify_wrap(mbedtls_pk_context *pk,
                                    mbedtls_md_type_t md_alg,
                                    const unsigned char *hash, size_t hash_len,
                                    const unsigned char *sig, size_t sig_len)
{
    (void) md_alg;
    unsigned char key[MBEDTLS_PK_MAX_EC_PUBKEY_RAW_LEN];
    size_t key_len;
    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_ecc_family_t curve;
    size_t curve_bits;
    psa_status_t status;

    status = psa_get_key_attributes(pk->priv_id, &key_attr);
    if (status != PSA_SUCCESS) {
        return PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
    }
    curve = PSA_KEY_TYPE_ECC_GET_FAMILY(psa_get_key_type(&key_attr));
    curve_bits = psa_get_key_bits(&key_attr);
    psa_reset_key_attributes(&key_attr);

    status = psa_export_public_key(pk->priv_id, key, sizeof(key), &key_len);
    if (status != PSA_SUCCESS) {
        return PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
    }

    return ecdsa_verify_psa(key, key_len, curve, curve_bits,
                            hash, hash_len, sig, sig_len);
}

static int ecdsa_verify_wrap(mbedtls_pk_context *pk,
                             mbedtls_md_type_t md_alg,
                             const unsigned char *hash, size_t hash_len,
                             const unsigned char *sig, size_t sig_len)
{
    (void) md_alg;
    psa_ecc_family_t curve = pk->ec_family;
    size_t curve_bits = pk->bits;

    return ecdsa_verify_psa(pk->pub_raw, pk->pub_raw_len, curve, curve_bits,
                            hash, hash_len, sig, sig_len);
}
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY */

#if defined(PSA_HAVE_ALG_ECDSA_SIGN)
/* Common helper for ECDSA sign using PSA functions.
 * Instead of extracting key's properties in order to check which kind of ECDSA
 * signature it supports, we try both deterministic and non-deterministic.
 */
static int ecdsa_sign_psa(mbedtls_svc_key_id_t key_id, mbedtls_md_type_t md_alg,
                          const unsigned char *hash, size_t hash_len,
                          unsigned char *sig, size_t sig_size, size_t *sig_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    psa_status_t status;
    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    size_t key_bits = 0;

    status = psa_get_key_attributes(key_id, &key_attr);
    if (status != PSA_SUCCESS) {
        return PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
    }
    key_bits = psa_get_key_bits(&key_attr);
    psa_reset_key_attributes(&key_attr);

    status = psa_sign_hash(key_id,
                           PSA_ALG_DETERMINISTIC_ECDSA(mbedtls_md_psa_alg_from_type(md_alg)),
                           hash, hash_len, sig, sig_size, sig_len);
    if (status == PSA_SUCCESS) {
        goto done;
    } else if (status != PSA_ERROR_NOT_PERMITTED) {
        return PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
    }

    status = psa_sign_hash(key_id,
                           PSA_ALG_ECDSA(mbedtls_md_psa_alg_from_type(md_alg)),
                           hash, hash_len, sig, sig_size, sig_len);
    if (status != PSA_SUCCESS) {
        return PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
    }

done:
    ret = mbedtls_ecdsa_raw_to_der(key_bits, sig, *sig_len, sig, sig_size, sig_len);

    return ret;
}

static int ecdsa_opaque_sign_wrap(mbedtls_pk_context *pk,
                                  mbedtls_md_type_t md_alg,
                                  const unsigned char *hash, size_t hash_len,
                                  unsigned char *sig, size_t sig_size,
                                  size_t *sig_len)
{
    return ecdsa_sign_psa(pk->priv_id, md_alg, hash, hash_len, sig, sig_size,
                          sig_len);
}

#define ecdsa_sign_wrap     ecdsa_opaque_sign_wrap

#endif /* PSA_HAVE_ALG_ECDSA_SIGN */

#if defined(MBEDTLS_ECP_RESTARTABLE)

/*
 * Restart context for ECDSA operations with ECKEY context
 *
 * We need to store an actual ECDSA context, as we need to pass the same to
 * the underlying ecdsa function, so we can't create it on the fly every time.
 */
typedef struct {
    mbedtls_ecdsa_restart_ctx ecdsa_rs;
    mbedtls_ecdsa_context ecdsa_ctx;
} eckey_restart_ctx;

#if defined(PSA_HAVE_ALG_ECDSA_SIGN) || defined(PSA_HAVE_ALG_ECDSA_VERIFY)
static void *eckey_rs_alloc(mbedtls_pk_rs_op_t op_type)
{
    mbedtls_pk_psa_restartable_ctx_t *rs_ctx;

    rs_ctx = mbedtls_calloc(1, sizeof(mbedtls_pk_psa_restartable_ctx_t));
    if (rs_ctx == NULL) {
        return NULL;
    }

    rs_ctx->op_type = op_type;
    rs_ctx->pub_id = MBEDTLS_SVC_KEY_ID_INIT;
    if (op_type == MBEDTLS_PK_RS_OP_VERIFY) {
        rs_ctx->op = mbedtls_calloc(1, sizeof(psa_verify_hash_interruptible_operation_t));
        psa_verify_hash_interruptible_operation_t *op = rs_ctx->op;
        *op = psa_verify_hash_interruptible_operation_init();
    } else {
        rs_ctx->op = mbedtls_calloc(1, sizeof(psa_sign_hash_interruptible_operation_t));
        psa_sign_hash_interruptible_operation_t *op = rs_ctx->op;
        *op = psa_sign_hash_interruptible_operation_init();
    }

    return (void *) rs_ctx;
}

static void eckey_rs_free(void *ctx)
{
    mbedtls_pk_psa_restartable_ctx_t *rs_ctx = ctx;

    if (rs_ctx->op_type == MBEDTLS_PK_RS_OP_VERIFY) {
        psa_verify_hash_abort(rs_ctx->op);
    } else {
        psa_sign_hash_abort(rs_ctx->op);
    }

    mbedtls_free(rs_ctx->op);

    if (!mbedtls_svc_key_id_is_null(rs_ctx->pub_id)) {
        psa_destroy_key(rs_ctx->pub_id);
        rs_ctx->pub_id = MBEDTLS_SVC_KEY_ID_INIT;
    }

    mbedtls_free(rs_ctx);
}
#endif /* PSA_HAVE_ALG_ECDSA_SIGN || PSA_HAVE_ALG_ECDSA_VERIFY */

#if defined(PSA_HAVE_ALG_ECDSA_VERIFY)
static int eckey_verify_rs_wrap(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                                const unsigned char *hash, size_t hash_len,
                                const unsigned char *sig, size_t sig_len,
                                void *_rs_ctx)
{
    mbedtls_pk_psa_restartable_ctx_t *rs_ctx = _rs_ctx;
    psa_verify_hash_interruptible_operation_t *op;
    psa_status_t status_tmp = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    unsigned char raw_sig[PSA_VENDOR_ECDSA_SIGNATURE_MAX_SIZE];
    size_t raw_sig_len;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (rs_ctx->op_type != MBEDTLS_PK_RS_OP_VERIFY) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    ret = mbedtls_ecdsa_der_to_raw(pk->bits, sig, sig_len,
                                   raw_sig, sizeof(raw_sig), &raw_sig_len);
    if (ret != 0) {
        return ret;
    }

    op = rs_ctx->op;

    if (psa_verify_hash_get_num_ops(op) == 0) {
        psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
        psa_algorithm_t alg = PSA_ALG_ECDSA(mbedtls_md_psa_alg_from_type(md_alg));
        psa_set_key_algorithm(&attr, alg);
        psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(pk->ec_family));
        psa_set_key_bits(&attr, pk->bits);
        psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_VERIFY_MESSAGE);

        status = psa_import_key(&attr, pk->pub_raw, pk->pub_raw_len, &rs_ctx->pub_id);
        if (status != PSA_SUCCESS) {
            return PSA_PK_TO_MBEDTLS_ERR(status);
        }
        status = psa_verify_hash_start(op, rs_ctx->pub_id, alg, hash, hash_len,
                                       raw_sig, raw_sig_len);
        if (status != PSA_SUCCESS) {
            psa_destroy_key(rs_ctx->pub_id);
            return PSA_PK_TO_MBEDTLS_ERR(status);
        }
    }

    status = psa_verify_hash_complete(op);
    if (status == PSA_OPERATION_INCOMPLETE) {
        return MBEDTLS_ERR_ECP_IN_PROGRESS;
    }

    status_tmp = psa_verify_hash_abort(op);
    status = (status != PSA_SUCCESS) ? status : status_tmp;

    status_tmp = psa_destroy_key(rs_ctx->pub_id);
    rs_ctx->pub_id = MBEDTLS_SVC_KEY_ID_INIT;
    status = (status != PSA_SUCCESS) ? status : status_tmp;

    return PSA_PK_TO_MBEDTLS_ERR(status);
}
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY */

#if defined(PSA_HAVE_ALG_ECDSA_SIGN)
static int eckey_sign_rs_wrap(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                              const unsigned char *hash, size_t hash_len,
                              unsigned char *sig, size_t sig_size, size_t *sig_len,
                              void *_rs_ctx)
{
    mbedtls_pk_psa_restartable_ctx_t *rs_ctx = _rs_ctx;
    psa_sign_hash_interruptible_operation_t *op;
    psa_status_t tmp_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    if (rs_ctx->op_type != MBEDTLS_PK_RS_OP_SIGN) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    op = rs_ctx->op;

    if (psa_sign_hash_get_num_ops(op) == 0) {
        psa_algorithm_t alg =
            MBEDTLS_PK_ALG_ECDSA(mbedtls_md_psa_alg_from_type(md_alg));

        status = psa_sign_hash_start(op, pk->priv_id, alg, hash, hash_len);
        if (status != PSA_SUCCESS) {
            return PSA_PK_TO_MBEDTLS_ERR(status);
        }
    }

    status = psa_sign_hash_complete(op, sig, sig_size, sig_len);
    if (status == PSA_OPERATION_INCOMPLETE) {
        return MBEDTLS_ERR_ECP_IN_PROGRESS;
    }

    tmp_status = psa_sign_hash_abort(op);
    status = (status != PSA_SUCCESS) ? status : tmp_status;

    if (status != PSA_SUCCESS) {
        return PSA_PK_TO_MBEDTLS_ERR(status);
    }

    return mbedtls_ecdsa_raw_to_der(pk->bits, sig, *sig_len, sig, sig_size, sig_len);
}
#endif /* PSA_HAVE_ALG_ECDSA_SIGN */
#endif /* MBEDTLS_ECP_RESTARTABLE */

static int eckey_check_pair_psa(mbedtls_pk_context *pub, mbedtls_pk_context *prv)
{
    psa_status_t status;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    uint8_t prv_key_buf[MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH];
    size_t prv_key_len;
    mbedtls_svc_key_id_t key_id = prv->priv_id;

    status = psa_export_public_key(key_id, prv_key_buf, sizeof(prv_key_buf),
                                   &prv_key_len);
    ret = PSA_PK_TO_MBEDTLS_ERR(status);
    if (ret != 0) {
        return ret;
    }

    if (memcmp(prv_key_buf, pub->pub_raw, pub->pub_raw_len) != 0) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    return 0;
}

static int eckey_check_pair_wrap(mbedtls_pk_context *pub, mbedtls_pk_context *prv)
{
    return eckey_check_pair_psa(pub, prv);
}

#define ecdsa_opaque_check_pair_wrap    eckey_check_pair_wrap

static void eckey_debug(mbedtls_pk_context *pk, mbedtls_pk_debug_item *items)
{
    items->type = MBEDTLS_PK_DEBUG_PSA_EC;
    items->name = "eckey.Q";
    items->value = pk;
}

const mbedtls_pk_info_t mbedtls_eckey_info = {
    .type = MBEDTLS_PK_ECKEY,
    .name = "EC",
    .get_bitlen = eckey_get_bitlen,
    .can_do = eckey_can_do,
#if defined(PSA_HAVE_ALG_ECDSA_VERIFY)
    .verify_func = ecdsa_verify_wrap,   /* Compatible key structures */
#else /* PSA_HAVE_ALG_ECDSA_VERIFY */
    .verify_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY */
#if defined(PSA_HAVE_ALG_ECDSA_SIGN)
    .sign_func = ecdsa_sign_wrap,   /* Compatible key structures */
#else /* PSA_HAVE_ALG_ECDSA_VERIFY */
    .sign_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY */
#if defined(MBEDTLS_ECP_RESTARTABLE)
#if defined(PSA_HAVE_ALG_ECDSA_VERIFY)
    .verify_rs_func = eckey_verify_rs_wrap,
#else /* PSA_HAVE_ALG_ECDSA_VERIFY */
    .verify_rs_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY */
#if defined(PSA_HAVE_ALG_ECDSA_SIGN)
    .sign_rs_func = eckey_sign_rs_wrap,
#else /* PSA_HAVE_ALG_ECDSA_SIGN */
    .sign_rs_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_SIGN */
#if defined(PSA_HAVE_ALG_ECDSA_SIGN) || defined(PSA_HAVE_ALG_ECDSA_VERIFY)
    .rs_alloc_func = eckey_rs_alloc,
    .rs_free_func = eckey_rs_free,
#else /* PSA_HAVE_ALG_ECDSA_SIGN || PSA_HAVE_ALG_ECDSA_VERIFY */
    .rs_alloc_func = NULL,
    .rs_free_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_SIGN || PSA_HAVE_ALG_ECDSA_VERIFY */
#endif /* MBEDTLS_ECP_RESTARTABLE */
    .check_pair_func = eckey_check_pair_wrap,
    .ctx_alloc_func = NULL,
    .ctx_free_func = NULL,
    .debug_func = eckey_debug,
};

/*
 * EC key restricted to ECDH
 */
static int eckeydh_can_do(mbedtls_pk_type_t type)
{
    return type == MBEDTLS_PK_ECKEY ||
           type == MBEDTLS_PK_ECKEY_DH;
}

const mbedtls_pk_info_t mbedtls_eckeydh_info = {
    .type = MBEDTLS_PK_ECKEY_DH,
    .name = "EC_DH",
    .get_bitlen = eckey_get_bitlen,         /* Same underlying key structure */
    .can_do = eckeydh_can_do,
    .verify_func = NULL,
    .sign_func = NULL,
#if defined(MBEDTLS_ECP_RESTARTABLE)
    .verify_rs_func = NULL,
    .sign_rs_func = NULL,
#endif /* MBEDTLS_ECP_RESTARTABLE */
    .check_pair_func = eckey_check_pair_wrap,
    .ctx_alloc_func = NULL,
    .ctx_free_func = NULL,
    .debug_func = eckey_debug,            /* Same underlying key structure */
};

#if defined(PSA_HAVE_ALG_SOME_ECDSA)
static int ecdsa_can_do(mbedtls_pk_type_t type)
{
    return type == MBEDTLS_PK_ECDSA;
}

const mbedtls_pk_info_t mbedtls_ecdsa_info = {
    .type = MBEDTLS_PK_ECDSA,
    .name = "ECDSA",
    .get_bitlen = eckey_get_bitlen,     /* Compatible key structures */
    .can_do = ecdsa_can_do,
#if defined(PSA_HAVE_ALG_ECDSA_VERIFY)
    .verify_func = ecdsa_verify_wrap,   /* Compatible key structures */
#else /* PSA_HAVE_ALG_ECDSA_VERIFY */
    .verify_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY */
#if defined(PSA_HAVE_ALG_ECDSA_SIGN)
    .sign_func = ecdsa_sign_wrap,   /* Compatible key structures */
#else /* PSA_HAVE_ALG_ECDSA_SIGN */
    .sign_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_SIGN */
#if defined(MBEDTLS_ECP_RESTARTABLE)
#if defined(PSA_HAVE_ALG_ECDSA_VERIFY)
    .verify_rs_func = eckey_verify_rs_wrap,
#else /* PSA_HAVE_ALG_ECDSA_VERIFY */
    .verify_rs_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY */
#if defined(PSA_HAVE_ALG_ECDSA_SIGN)
    .sign_rs_func = eckey_sign_rs_wrap,
#else /* PSA_HAVE_ALG_ECDSA_SIGN */
    .sign_rs_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_SIGN */
#if defined(PSA_HAVE_ALG_ECDSA_VERIFY) || defined(PSA_HAVE_ALG_ECDSA_SIGN)
    .rs_alloc_func = eckey_rs_alloc,
    .rs_free_func = eckey_rs_free,
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY || PSA_HAVE_ALG_ECDSA_SIGN */
#endif /* MBEDTLS_ECP_RESTARTABLE */
    .check_pair_func = eckey_check_pair_wrap,   /* Compatible key structures */
    .ctx_alloc_func = NULL,
    .ctx_free_func = NULL,
    .debug_func = eckey_debug,        /* Compatible key structures */
};
#endif /* PSA_HAVE_ALG_SOME_ECDSA */
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

static size_t opaque_get_bitlen(mbedtls_pk_context *pk)
{
    size_t bits;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

    if (PSA_SUCCESS != psa_get_key_attributes(pk->priv_id, &attributes)) {
        return 0;
    }

    bits = psa_get_key_bits(&attributes);
    psa_reset_key_attributes(&attributes);
    return bits;
}

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
static int ecdsa_opaque_can_do(mbedtls_pk_type_t type)
{
    return type == MBEDTLS_PK_ECKEY ||
           type == MBEDTLS_PK_ECDSA;
}

const mbedtls_pk_info_t mbedtls_ecdsa_opaque_info = {
    .type = MBEDTLS_PK_OPAQUE,
    .name = "Opaque",
    .get_bitlen = opaque_get_bitlen,
    .can_do = ecdsa_opaque_can_do,
#if defined(PSA_HAVE_ALG_ECDSA_VERIFY)
    .verify_func = ecdsa_opaque_verify_wrap,
#else /* PSA_HAVE_ALG_ECDSA_VERIFY */
    .verify_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_VERIFY */
#if defined(PSA_HAVE_ALG_ECDSA_SIGN)
    .sign_func = ecdsa_opaque_sign_wrap,
#else /* PSA_HAVE_ALG_ECDSA_SIGN */
    .sign_func = NULL,
#endif /* PSA_HAVE_ALG_ECDSA_SIGN */
#if defined(MBEDTLS_ECP_RESTARTABLE)
    .verify_rs_func = NULL,
    .sign_rs_func = NULL,
    .rs_alloc_func = NULL,
    .rs_free_func = NULL,
#endif /* MBEDTLS_ECP_RESTARTABLE */
    .check_pair_func = ecdsa_opaque_check_pair_wrap,
    .ctx_alloc_func = NULL,
    .ctx_free_func = NULL,
    .debug_func = NULL,
};
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

static int rsa_opaque_can_do(mbedtls_pk_type_t type)
{
    return type == MBEDTLS_PK_RSA ||
           type == MBEDTLS_PK_RSASSA_PSS;
}

static int rsa_opaque_sign_wrap(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                                const unsigned char *hash, size_t hash_len,
                                unsigned char *sig, size_t sig_size, size_t *sig_len)
{
#if defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR_BASIC)
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_algorithm_t alg;
    psa_key_type_t type;
    psa_status_t status;

    /* PSA has its own RNG */

    status = psa_get_key_attributes(pk->priv_id, &attributes);
    if (status != PSA_SUCCESS) {
        return PSA_PK_TO_MBEDTLS_ERR(status);
    }

    type = psa_get_key_type(&attributes);
    alg = psa_get_key_algorithm(&attributes);
    psa_reset_key_attributes(&attributes);

    if (PSA_KEY_TYPE_IS_RSA(type)) {
        alg = (alg & ~PSA_ALG_HASH_MASK) | mbedtls_md_psa_alg_from_type(md_alg);
    } else {
        return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
    }

    status = psa_sign_hash(pk->priv_id, alg, hash, hash_len, sig, sig_size, sig_len);
    if (status != PSA_SUCCESS) {
        if (PSA_KEY_TYPE_IS_RSA(type)) {
            return PSA_PK_RSA_TO_MBEDTLS_ERR(status);
        } else {
            return PSA_PK_TO_MBEDTLS_ERR(status);
        }
    }

    return 0;
#else /* PSA_WANT_KEY_TYPE_RSA_KEY_PAIR_BASIC */
    ((void) pk);
    ((void) md_alg);
    ((void) hash);
    ((void) hash_len);
    ((void) sig);
    ((void) sig_size);
    ((void) sig_len);
    return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
#endif /* PSA_WANT_KEY_TYPE_RSA_KEY_PAIR_BASIC */
}

const mbedtls_pk_info_t mbedtls_rsa_opaque_info = {
    .type = MBEDTLS_PK_OPAQUE,
    .name = "Opaque",
    .get_bitlen = opaque_get_bitlen,
    .can_do = rsa_opaque_can_do,
    .verify_func = NULL,
    .sign_func = rsa_opaque_sign_wrap,
#if defined(MBEDTLS_ECP_RESTARTABLE)
    .verify_rs_func = NULL,
    .sign_rs_func = NULL,
    .rs_alloc_func = NULL,
    .rs_free_func = NULL,
#endif /* MBEDTLS_ECP_RESTARTABLE */
    .check_pair_func = NULL,
    .ctx_alloc_func = NULL,
    .ctx_free_func = NULL,
    .debug_func = NULL,
};

#endif /* MBEDTLS_PK_C */

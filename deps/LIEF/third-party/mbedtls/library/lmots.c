/*
 * The LM-OTS one-time public-key signature scheme
 *
 * Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 *  The following sources were referenced in the design of this implementation
 *  of the LM-OTS algorithm:
 *
 *  [1] IETF RFC8554
 *      D. McGrew, M. Curcio, S.Fluhrer
 *      https://datatracker.ietf.org/doc/html/rfc8554
 *
 *  [2] NIST Special Publication 800-208
 *      David A. Cooper et. al.
 *      https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-208.pdf
 */

#include "common.h"

#if defined(MBEDTLS_LMS_C)

#include <string.h>

#include "lmots.h"

#include "mbedtls/lms.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "psa_util_internal.h"

#include "psa/crypto.h"

/* Define a local translating function to save code size by not using too many
 * arguments in each translating place. */
static int local_err_translation(psa_status_t status)
{
    return psa_status_to_mbedtls(status, psa_to_lms_errors,
                                 ARRAY_LENGTH(psa_to_lms_errors),
                                 psa_generic_status_to_mbedtls);
}
#define PSA_TO_MBEDTLS_ERR(status) local_err_translation(status)

#define PUBLIC_KEY_TYPE_OFFSET     (0)
#define PUBLIC_KEY_I_KEY_ID_OFFSET (PUBLIC_KEY_TYPE_OFFSET + \
                                    MBEDTLS_LMOTS_TYPE_LEN)
#define PUBLIC_KEY_Q_LEAF_ID_OFFSET (PUBLIC_KEY_I_KEY_ID_OFFSET + \
                                     MBEDTLS_LMOTS_I_KEY_ID_LEN)
#define PUBLIC_KEY_KEY_HASH_OFFSET (PUBLIC_KEY_Q_LEAF_ID_OFFSET + \
                                    MBEDTLS_LMOTS_Q_LEAF_ID_LEN)

/* We only support parameter sets that use 8-bit digits, as it does not require
 * translation logic between digits and bytes */
#define W_WINTERNITZ_PARAMETER (8u)
#define CHECKSUM_LEN           (2)
#define I_DIGIT_IDX_LEN        (2)
#define J_HASH_IDX_LEN         (1)
#define D_CONST_LEN            (2)

#define DIGIT_MAX_VALUE        ((1u << W_WINTERNITZ_PARAMETER) - 1u)

#define D_CONST_LEN            (2)
static const unsigned char D_PUBLIC_CONSTANT_BYTES[D_CONST_LEN] = { 0x80, 0x80 };
static const unsigned char D_MESSAGE_CONSTANT_BYTES[D_CONST_LEN] = { 0x81, 0x81 };

#if defined(MBEDTLS_TEST_HOOKS)
int (*mbedtls_lmots_sign_private_key_invalidated_hook)(unsigned char *) = NULL;
#endif /* defined(MBEDTLS_TEST_HOOKS) */

/* Calculate the checksum digits that are appended to the end of the LMOTS digit
 * string. See NIST SP800-208 section 3.1 or RFC8554 Algorithm 2 for details of
 * the checksum algorithm.
 *
 *  params              The LMOTS parameter set, I and q values which
 *                      describe the key being used.
 *
 *  digest              The digit string to create the digest from. As
 *                      this does not contain a checksum, it is the same
 *                      size as a hash output.
 */
static unsigned short lmots_checksum_calculate(const mbedtls_lmots_parameters_t *params,
                                               const unsigned char *digest)
{
    size_t idx;
    unsigned sum = 0;

    for (idx = 0; idx < MBEDTLS_LMOTS_N_HASH_LEN(params->type); idx++) {
        sum += DIGIT_MAX_VALUE - digest[idx];
    }

    return sum;
}

/* Create the string of digest digits (in the base determined by the Winternitz
 * parameter with the checksum appended to the end (Q || cksm(Q)). See NIST
 * SP800-208 section 3.1 or RFC8554 Algorithm 3 step 5 (also used in Algorithm
 * 4b step 3) for details.
 *
 *  params              The LMOTS parameter set, I and q values which
 *                      describe the key being used.
 *
 *  msg                 The message that will be hashed to create the
 *                      digest.
 *
 *  msg_size            The size of the message.
 *
 *  C_random_value      The random value that will be combined with the
 *                      message digest. This is always the same size as a
 *                      hash output for whichever hash algorithm is
 *                      determined by the parameter set.
 *
 *  output              An output containing the digit string (+
 *                      checksum) of length P digits (in the case of
 *                      MBEDTLS_LMOTS_SHA256_N32_W8, this means it is of
 *                      size P bytes).
 */
static int create_digit_array_with_checksum(const mbedtls_lmots_parameters_t *params,
                                            const unsigned char *msg,
                                            size_t msg_len,
                                            const unsigned char *C_random_value,
                                            unsigned char *out)
{
    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t output_hash_len;
    unsigned short checksum;

    status = psa_hash_setup(&op, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, params->I_key_identifier,
                             MBEDTLS_LMOTS_I_KEY_ID_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, params->q_leaf_identifier,
                             MBEDTLS_LMOTS_Q_LEAF_ID_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, D_MESSAGE_CONSTANT_BYTES, D_CONST_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, C_random_value,
                             MBEDTLS_LMOTS_C_RANDOM_VALUE_LEN(params->type));
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, msg, msg_len);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_finish(&op, out,
                             MBEDTLS_LMOTS_N_HASH_LEN(params->type),
                             &output_hash_len);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    checksum = lmots_checksum_calculate(params, out);
    MBEDTLS_PUT_UINT16_BE(checksum, out, MBEDTLS_LMOTS_N_HASH_LEN(params->type));

exit:
    psa_hash_abort(&op);

    return PSA_TO_MBEDTLS_ERR(status);
}

/* Hash each element of the string of digits (+ checksum), producing a hash
 * output for each element. This is used in several places (by varying the
 * hash_idx_min/max_values) in order to calculate a public key from a private
 * key (RFC8554 Algorithm 1 step 4), in order to sign a message (RFC8554
 * Algorithm 3 step 5), and to calculate a public key candidate from a
 * signature and message (RFC8554 Algorithm 4b step 3).
 *
 *  params              The LMOTS parameter set, I and q values which
 *                      describe the key being used.
 *
 *  x_digit_array       The array of digits (of size P, 34 in the case of
 *                      MBEDTLS_LMOTS_SHA256_N32_W8).
 *
 *  hash_idx_min_values An array of the starting values of the j iterator
 *                      for each of the members of the digit array. If
 *                      this value in NULL, then all iterators will start
 *                      at 0.
 *
 *  hash_idx_max_values An array of the upper bound values of the j
 *                      iterator for each of the members of the digit
 *                      array. If this value in NULL, then iterator is
 *                      bounded to be less than 2^w - 1 (255 in the case
 *                      of MBEDTLS_LMOTS_SHA256_N32_W8)
 *
 *  output              An array containing a hash output for each member
 *                      of the digit string P. In the case of
 *                      MBEDTLS_LMOTS_SHA256_N32_W8, this is of size 32 *
 *                      34.
 */
static int hash_digit_array(const mbedtls_lmots_parameters_t *params,
                            const unsigned char *x_digit_array,
                            const unsigned char *hash_idx_min_values,
                            const unsigned char *hash_idx_max_values,
                            unsigned char *output)
{
    unsigned int i_digit_idx;
    unsigned char i_digit_idx_bytes[I_DIGIT_IDX_LEN];
    unsigned int j_hash_idx;
    unsigned char j_hash_idx_bytes[J_HASH_IDX_LEN];
    unsigned int j_hash_idx_min;
    unsigned int j_hash_idx_max;
    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t output_hash_len;
    unsigned char tmp_hash[MBEDTLS_LMOTS_N_HASH_LEN_MAX];

    for (i_digit_idx = 0;
         i_digit_idx < MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT(params->type);
         i_digit_idx++) {

        memcpy(tmp_hash,
               &x_digit_array[i_digit_idx * MBEDTLS_LMOTS_N_HASH_LEN(params->type)],
               MBEDTLS_LMOTS_N_HASH_LEN(params->type));

        j_hash_idx_min = hash_idx_min_values != NULL ?
                         hash_idx_min_values[i_digit_idx] : 0;
        j_hash_idx_max = hash_idx_max_values != NULL ?
                         hash_idx_max_values[i_digit_idx] : DIGIT_MAX_VALUE;

        for (j_hash_idx = j_hash_idx_min;
             j_hash_idx < j_hash_idx_max;
             j_hash_idx++) {
            status = psa_hash_setup(&op, PSA_ALG_SHA_256);
            if (status != PSA_SUCCESS) {
                goto exit;
            }

            status = psa_hash_update(&op,
                                     params->I_key_identifier,
                                     MBEDTLS_LMOTS_I_KEY_ID_LEN);
            if (status != PSA_SUCCESS) {
                goto exit;
            }

            status = psa_hash_update(&op,
                                     params->q_leaf_identifier,
                                     MBEDTLS_LMOTS_Q_LEAF_ID_LEN);
            if (status != PSA_SUCCESS) {
                goto exit;
            }

            MBEDTLS_PUT_UINT16_BE(i_digit_idx, i_digit_idx_bytes, 0);
            status = psa_hash_update(&op, i_digit_idx_bytes, I_DIGIT_IDX_LEN);
            if (status != PSA_SUCCESS) {
                goto exit;
            }

            j_hash_idx_bytes[0] = (uint8_t) j_hash_idx;
            status = psa_hash_update(&op, j_hash_idx_bytes, J_HASH_IDX_LEN);
            if (status != PSA_SUCCESS) {
                goto exit;
            }

            status = psa_hash_update(&op, tmp_hash,
                                     MBEDTLS_LMOTS_N_HASH_LEN(params->type));
            if (status != PSA_SUCCESS) {
                goto exit;
            }

            status = psa_hash_finish(&op, tmp_hash, sizeof(tmp_hash),
                                     &output_hash_len);
            if (status != PSA_SUCCESS) {
                goto exit;
            }

            psa_hash_abort(&op);
        }

        memcpy(&output[i_digit_idx * MBEDTLS_LMOTS_N_HASH_LEN(params->type)],
               tmp_hash, MBEDTLS_LMOTS_N_HASH_LEN(params->type));
    }

exit:
    psa_hash_abort(&op);
    mbedtls_platform_zeroize(tmp_hash, sizeof(tmp_hash));

    return PSA_TO_MBEDTLS_ERR(status);
}

/* Combine the hashes of the digit array into a public key. This is used in
 * in order to calculate a public key from a private key (RFC8554 Algorithm 1
 * step 4), and to calculate a public key candidate from a signature and message
 * (RFC8554 Algorithm 4b step 3).
 *
 *  params           The LMOTS parameter set, I and q values which describe
 *                   the key being used.
 *  y_hashed_digits  The array of hashes, one hash for each digit of the
 *                   symbol array (which is of size P, 34 in the case of
 *                   MBEDTLS_LMOTS_SHA256_N32_W8)
 *
 *  pub_key          The output public key (or candidate public key in
 *                   case this is being run as part of signature
 *                   verification), in the form of a hash output.
 */
static int public_key_from_hashed_digit_array(const mbedtls_lmots_parameters_t *params,
                                              const unsigned char *y_hashed_digits,
                                              unsigned char *pub_key)
{
    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t output_hash_len;

    status = psa_hash_setup(&op, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op,
                             params->I_key_identifier,
                             MBEDTLS_LMOTS_I_KEY_ID_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, params->q_leaf_identifier,
                             MBEDTLS_LMOTS_Q_LEAF_ID_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, D_PUBLIC_CONSTANT_BYTES, D_CONST_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, y_hashed_digits,
                             MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT(params->type) *
                             MBEDTLS_LMOTS_N_HASH_LEN(params->type));
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_finish(&op, pub_key,
                             MBEDTLS_LMOTS_N_HASH_LEN(params->type),
                             &output_hash_len);
    if (status != PSA_SUCCESS) {

exit:
        psa_hash_abort(&op);
    }

    return PSA_TO_MBEDTLS_ERR(status);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
int mbedtls_lms_error_from_psa(psa_status_t status)
{
    switch (status) {
        case PSA_SUCCESS:
            return 0;
        case PSA_ERROR_HARDWARE_FAILURE:
            return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
        case PSA_ERROR_NOT_SUPPORTED:
            return MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
        case PSA_ERROR_BUFFER_TOO_SMALL:
            return MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL;
        case PSA_ERROR_INVALID_ARGUMENT:
            return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
        default:
            return MBEDTLS_ERR_ERROR_GENERIC_ERROR;
    }
}
#endif /* !MBEDTLS_DEPRECATED_REMOVED */

void mbedtls_lmots_public_init(mbedtls_lmots_public_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_lmots_public_free(mbedtls_lmots_public_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(*ctx));
}

int mbedtls_lmots_import_public_key(mbedtls_lmots_public_t *ctx,
                                    const unsigned char *key, size_t key_len)
{
    if (key_len < MBEDTLS_LMOTS_SIG_TYPE_OFFSET + MBEDTLS_LMOTS_TYPE_LEN) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    uint32_t type = MBEDTLS_GET_UINT32_BE(key, MBEDTLS_LMOTS_SIG_TYPE_OFFSET);
    if (type != (uint32_t) MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }
    ctx->params.type = (mbedtls_lmots_algorithm_type_t) type;

    if (key_len != MBEDTLS_LMOTS_PUBLIC_KEY_LEN(ctx->params.type)) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    memcpy(ctx->params.I_key_identifier,
           key + PUBLIC_KEY_I_KEY_ID_OFFSET,
           MBEDTLS_LMOTS_I_KEY_ID_LEN);

    memcpy(ctx->params.q_leaf_identifier,
           key + PUBLIC_KEY_Q_LEAF_ID_OFFSET,
           MBEDTLS_LMOTS_Q_LEAF_ID_LEN);

    memcpy(ctx->public_key,
           key + PUBLIC_KEY_KEY_HASH_OFFSET,
           MBEDTLS_LMOTS_N_HASH_LEN(ctx->params.type));

    ctx->have_public_key = 1;

    return 0;
}

int mbedtls_lmots_export_public_key(const mbedtls_lmots_public_t *ctx,
                                    unsigned char *key, size_t key_size,
                                    size_t *key_len)
{
    if (key_size < MBEDTLS_LMOTS_PUBLIC_KEY_LEN(ctx->params.type)) {
        return MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL;
    }

    if (!ctx->have_public_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    MBEDTLS_PUT_UINT32_BE(ctx->params.type, key, MBEDTLS_LMOTS_SIG_TYPE_OFFSET);

    memcpy(key + PUBLIC_KEY_I_KEY_ID_OFFSET,
           ctx->params.I_key_identifier,
           MBEDTLS_LMOTS_I_KEY_ID_LEN);

    memcpy(key + PUBLIC_KEY_Q_LEAF_ID_OFFSET,
           ctx->params.q_leaf_identifier,
           MBEDTLS_LMOTS_Q_LEAF_ID_LEN);

    memcpy(key + PUBLIC_KEY_KEY_HASH_OFFSET, ctx->public_key,
           MBEDTLS_LMOTS_N_HASH_LEN(ctx->params.type));

    if (key_len != NULL) {
        *key_len = MBEDTLS_LMOTS_PUBLIC_KEY_LEN(ctx->params.type);
    }

    return 0;
}

int mbedtls_lmots_calculate_public_key_candidate(const mbedtls_lmots_parameters_t *params,
                                                 const unsigned char  *msg,
                                                 size_t msg_size,
                                                 const unsigned char *sig,
                                                 size_t sig_size,
                                                 unsigned char *out,
                                                 size_t out_size,
                                                 size_t *out_len)
{
    unsigned char tmp_digit_array[MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT_MAX];
    unsigned char y_hashed_digits[MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT_MAX][MBEDTLS_LMOTS_N_HASH_LEN_MAX];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (msg == NULL && msg_size != 0) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (sig_size != MBEDTLS_LMOTS_SIG_LEN(params->type) ||
        out_size < MBEDTLS_LMOTS_N_HASH_LEN(params->type)) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    ret = create_digit_array_with_checksum(params, msg, msg_size,
                                           sig + MBEDTLS_LMOTS_SIG_C_RANDOM_OFFSET,
                                           tmp_digit_array);
    if (ret) {
        return ret;
    }

    ret = hash_digit_array(params,
                           sig + MBEDTLS_LMOTS_SIG_SIGNATURE_OFFSET(params->type),
                           tmp_digit_array, NULL, (unsigned char *) y_hashed_digits);
    if (ret) {
        return ret;
    }

    ret = public_key_from_hashed_digit_array(params,
                                             (unsigned char *) y_hashed_digits,
                                             out);
    if (ret) {
        return ret;
    }

    if (out_len != NULL) {
        *out_len = MBEDTLS_LMOTS_N_HASH_LEN(params->type);
    }

    return 0;
}

int mbedtls_lmots_verify(const mbedtls_lmots_public_t *ctx,
                         const unsigned char *msg, size_t msg_size,
                         const unsigned char *sig, size_t sig_size)
{
    unsigned char Kc_public_key_candidate[MBEDTLS_LMOTS_N_HASH_LEN_MAX];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (msg == NULL && msg_size != 0) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (!ctx->have_public_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (ctx->params.type != MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (sig_size < MBEDTLS_LMOTS_SIG_TYPE_OFFSET + MBEDTLS_LMOTS_TYPE_LEN) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    if (MBEDTLS_GET_UINT32_BE(sig, MBEDTLS_LMOTS_SIG_TYPE_OFFSET) != MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    ret = mbedtls_lmots_calculate_public_key_candidate(&ctx->params,
                                                       msg, msg_size, sig, sig_size,
                                                       Kc_public_key_candidate,
                                                       MBEDTLS_LMOTS_N_HASH_LEN(ctx->params.type),
                                                       NULL);
    if (ret) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    if (memcmp(&Kc_public_key_candidate, ctx->public_key,
               sizeof(ctx->public_key))) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    return 0;
}

#if defined(MBEDTLS_LMS_PRIVATE)

void mbedtls_lmots_private_init(mbedtls_lmots_private_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_lmots_private_free(mbedtls_lmots_private_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx,
                             sizeof(*ctx));
}

int mbedtls_lmots_generate_private_key(mbedtls_lmots_private_t *ctx,
                                       mbedtls_lmots_algorithm_type_t type,
                                       const unsigned char I_key_identifier[MBEDTLS_LMOTS_I_KEY_ID_LEN],
                                       uint32_t q_leaf_identifier,
                                       const unsigned char *seed,
                                       size_t seed_size)
{
    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t output_hash_len;
    unsigned int i_digit_idx;
    unsigned char i_digit_idx_bytes[2];
    unsigned char const_bytes[1] = { 0xFF };

    if (ctx->have_private_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (type != MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    ctx->params.type = type;

    memcpy(ctx->params.I_key_identifier,
           I_key_identifier,
           sizeof(ctx->params.I_key_identifier));

    MBEDTLS_PUT_UINT32_BE(q_leaf_identifier, ctx->params.q_leaf_identifier, 0);

    for (i_digit_idx = 0;
         i_digit_idx < MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT(ctx->params.type);
         i_digit_idx++) {
        status = psa_hash_setup(&op, PSA_ALG_SHA_256);
        if (status != PSA_SUCCESS) {
            goto exit;
        }

        status = psa_hash_update(&op,
                                 ctx->params.I_key_identifier,
                                 sizeof(ctx->params.I_key_identifier));
        if (status != PSA_SUCCESS) {
            goto exit;
        }

        status = psa_hash_update(&op,
                                 ctx->params.q_leaf_identifier,
                                 MBEDTLS_LMOTS_Q_LEAF_ID_LEN);
        if (status != PSA_SUCCESS) {
            goto exit;
        }

        MBEDTLS_PUT_UINT16_BE(i_digit_idx, i_digit_idx_bytes, 0);
        status = psa_hash_update(&op, i_digit_idx_bytes, I_DIGIT_IDX_LEN);
        if (status != PSA_SUCCESS) {
            goto exit;
        }

        status = psa_hash_update(&op, const_bytes, sizeof(const_bytes));
        if (status != PSA_SUCCESS) {
            goto exit;
        }

        status = psa_hash_update(&op, seed, seed_size);
        if (status != PSA_SUCCESS) {
            goto exit;
        }

        status = psa_hash_finish(&op,
                                 ctx->private_key[i_digit_idx],
                                 MBEDTLS_LMOTS_N_HASH_LEN(ctx->params.type),
                                 &output_hash_len);
        if (status != PSA_SUCCESS) {
            goto exit;
        }

        psa_hash_abort(&op);
    }

    ctx->have_private_key = 1;

exit:
    psa_hash_abort(&op);

    return PSA_TO_MBEDTLS_ERR(status);
}

int mbedtls_lmots_calculate_public_key(mbedtls_lmots_public_t *ctx,
                                       const mbedtls_lmots_private_t *priv_ctx)
{
    unsigned char y_hashed_digits[MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT_MAX][MBEDTLS_LMOTS_N_HASH_LEN_MAX];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /* Check that a private key is loaded */
    if (!priv_ctx->have_private_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    ret = hash_digit_array(&priv_ctx->params,
                           (unsigned char *) priv_ctx->private_key, NULL,
                           NULL, (unsigned char *) y_hashed_digits);
    if (ret) {
        goto exit;
    }

    ret = public_key_from_hashed_digit_array(&priv_ctx->params,
                                             (unsigned char *) y_hashed_digits,
                                             ctx->public_key);
    if (ret) {
        goto exit;
    }

    memcpy(&ctx->params, &priv_ctx->params,
           sizeof(ctx->params));

    ctx->have_public_key = 1;

exit:
    mbedtls_platform_zeroize(y_hashed_digits, sizeof(y_hashed_digits));

    return ret;
}

int mbedtls_lmots_sign(mbedtls_lmots_private_t *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng, const unsigned char *msg, size_t msg_size,
                       unsigned char *sig, size_t sig_size, size_t *sig_len)
{
    unsigned char tmp_digit_array[MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT_MAX];
    /* Create a temporary buffer to prepare the signature in. This allows us to
     * finish creating a signature (ensuring the process doesn't fail), and then
     * erase the private key **before** writing any data into the sig parameter
     * buffer. If data were directly written into the sig buffer, it might leak
     * a partial signature on failure, which effectively compromises the private
     * key.
     */
    unsigned char tmp_sig[MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT_MAX][MBEDTLS_LMOTS_N_HASH_LEN_MAX];
    unsigned char tmp_c_random[MBEDTLS_LMOTS_N_HASH_LEN_MAX];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (msg == NULL && msg_size != 0) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (sig_size < MBEDTLS_LMOTS_SIG_LEN(ctx->params.type)) {
        return MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL;
    }

    /* Check that a private key is loaded */
    if (!ctx->have_private_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    ret = f_rng(p_rng, tmp_c_random,
                MBEDTLS_LMOTS_N_HASH_LEN(ctx->params.type));
    if (ret) {
        return ret;
    }

    ret = create_digit_array_with_checksum(&ctx->params,
                                           msg, msg_size,
                                           tmp_c_random,
                                           tmp_digit_array);
    if (ret) {
        goto exit;
    }

    ret = hash_digit_array(&ctx->params, (unsigned char *) ctx->private_key,
                           NULL, tmp_digit_array, (unsigned char *) tmp_sig);
    if (ret) {
        goto exit;
    }

    MBEDTLS_PUT_UINT32_BE(ctx->params.type, sig, MBEDTLS_LMOTS_SIG_TYPE_OFFSET);

    /* Test hook to check if sig is being written to before we invalidate the
     * private key.
     */
#if defined(MBEDTLS_TEST_HOOKS)
    if (mbedtls_lmots_sign_private_key_invalidated_hook != NULL) {
        ret = (*mbedtls_lmots_sign_private_key_invalidated_hook)(sig);
        if (ret != 0) {
            return ret;
        }
    }
#endif /* defined(MBEDTLS_TEST_HOOKS) */

    /* We've got a valid signature now, so it's time to make sure the private
     * key can't be reused.
     */
    ctx->have_private_key = 0;
    mbedtls_platform_zeroize(ctx->private_key,
                             sizeof(ctx->private_key));

    memcpy(sig + MBEDTLS_LMOTS_SIG_C_RANDOM_OFFSET, tmp_c_random,
           MBEDTLS_LMOTS_C_RANDOM_VALUE_LEN(ctx->params.type));

    memcpy(sig + MBEDTLS_LMOTS_SIG_SIGNATURE_OFFSET(ctx->params.type), tmp_sig,
           MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT(ctx->params.type)
           * MBEDTLS_LMOTS_N_HASH_LEN(ctx->params.type));

    if (sig_len != NULL) {
        *sig_len = MBEDTLS_LMOTS_SIG_LEN(ctx->params.type);
    }

    ret = 0;

exit:
    mbedtls_platform_zeroize(tmp_digit_array, sizeof(tmp_digit_array));
    mbedtls_platform_zeroize(tmp_sig, sizeof(tmp_sig));

    return ret;
}

#endif /* defined(MBEDTLS_LMS_PRIVATE) */
#endif /* defined(MBEDTLS_LMS_C) */

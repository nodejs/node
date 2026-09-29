/*
 *  The LMS stateful-hash public-key signature scheme
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 *  The following sources were referenced in the design of this implementation
 *  of the LMS algorithm:
 *
 *  [1] IETF RFC8554
 *      D. McGrew, M. Curcio, S.Fluhrer
 *      https://datatracker.ietf.org/doc/html/rfc8554
 *
 *  [2] NIST Special Publication 800-208
 *      David A. Cooper et. al.
 *      https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-208.pdf
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_LMS_C)

#include <string.h>

#include "lmots.h"

#include "psa/crypto.h"
#include "psa_util_internal.h"
#include "mbedtls/lms.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/platform_util.h"

#include "mbedtls/platform.h"

/* Define a local translating function to save code size by not using too many
 * arguments in each translating place. */
static int local_err_translation(psa_status_t status)
{
    return psa_status_to_mbedtls(status, psa_to_lms_errors,
                                 ARRAY_LENGTH(psa_to_lms_errors),
                                 psa_generic_status_to_mbedtls);
}
#define PSA_TO_MBEDTLS_ERR(status) local_err_translation(status)

#define SIG_Q_LEAF_ID_OFFSET     (0)
#define SIG_OTS_SIG_OFFSET       (SIG_Q_LEAF_ID_OFFSET + \
                                  MBEDTLS_LMOTS_Q_LEAF_ID_LEN)
#define SIG_TYPE_OFFSET(otstype) (SIG_OTS_SIG_OFFSET   + \
                                  MBEDTLS_LMOTS_SIG_LEN(otstype))
#define SIG_PATH_OFFSET(otstype) (SIG_TYPE_OFFSET(otstype) + \
                                  MBEDTLS_LMS_TYPE_LEN)

#define PUBLIC_KEY_TYPE_OFFSET      (0)
#define PUBLIC_KEY_OTSTYPE_OFFSET   (PUBLIC_KEY_TYPE_OFFSET + \
                                     MBEDTLS_LMS_TYPE_LEN)
#define PUBLIC_KEY_I_KEY_ID_OFFSET  (PUBLIC_KEY_OTSTYPE_OFFSET  + \
                                     MBEDTLS_LMOTS_TYPE_LEN)
#define PUBLIC_KEY_ROOT_NODE_OFFSET (PUBLIC_KEY_I_KEY_ID_OFFSET + \
                                     MBEDTLS_LMOTS_I_KEY_ID_LEN)


/* Currently only support H=10 */
#define H_TREE_HEIGHT_MAX                  10
#define MERKLE_TREE_NODE_AM(type)          ((size_t) 1 << (MBEDTLS_LMS_H_TREE_HEIGHT(type) + 1u))
#define MERKLE_TREE_LEAF_NODE_AM(type)     ((size_t) 1 << MBEDTLS_LMS_H_TREE_HEIGHT(type))
#define MERKLE_TREE_INTERNAL_NODE_AM(type) ((unsigned int) \
                                            (1u << MBEDTLS_LMS_H_TREE_HEIGHT(type)))

#define D_CONST_LEN           (2)
static const unsigned char D_LEAF_CONSTANT_BYTES[D_CONST_LEN] = { 0x82, 0x82 };
static const unsigned char D_INTR_CONSTANT_BYTES[D_CONST_LEN] = { 0x83, 0x83 };


/* Calculate the value of a leaf node of the Merkle tree (which is a hash of a
 * public key and some other parameters like the leaf index). This function
 * implements RFC8554 section 5.3, in the case where r >= 2^h.
 *
 *  params              The LMS parameter set, the underlying LMOTS
 *                      parameter set, and I value which describe the key
 *                      being used.
 *
 *  pub_key             The public key of the private whose index
 *                      corresponds to the index of this leaf node. This
 *                      is a hash output.
 *
 *  r_node_idx          The index of this node in the Merkle tree. Note
 *                      that the root node of the Merkle tree is
 *                      1-indexed.
 *
 *  out                 The output node value, which is a hash output.
 */
static int create_merkle_leaf_value(const mbedtls_lms_parameters_t *params,
                                    unsigned char *pub_key,
                                    unsigned int r_node_idx,
                                    unsigned char *out)
{
    psa_hash_operation_t op;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t output_hash_len;
    unsigned char r_node_idx_bytes[4];

    /* Always zeroize the output buffer because it may contain data from the previous invocation */
    memset(out, 0, MBEDTLS_LMS_M_NODE_BYTES(params->type));

    op = psa_hash_operation_init();
    status = psa_hash_setup(&op, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, params->I_key_identifier,
                             MBEDTLS_LMOTS_I_KEY_ID_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    MBEDTLS_PUT_UINT32_BE(r_node_idx, r_node_idx_bytes, 0);
    status = psa_hash_update(&op, r_node_idx_bytes, 4);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, D_LEAF_CONSTANT_BYTES, D_CONST_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, pub_key,
                             MBEDTLS_LMOTS_N_HASH_LEN(params->otstype));
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_finish(&op, out, MBEDTLS_LMS_M_NODE_BYTES(params->type),
                             &output_hash_len);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

exit:
    psa_hash_abort(&op);

    return PSA_TO_MBEDTLS_ERR(status);
}

/* Calculate the value of an internal node of the Merkle tree (which is a hash
 * of a public key and some other parameters like the node index). This function
 * implements RFC8554 section 5.3, in the case where r < 2^h.
 *
 *  params              The LMS parameter set, the underlying LMOTS
 *                      parameter set, and I value which describe the key
 *                      being used.
 *
 *  left_node           The value of the child of this node which is on
 *                      the left-hand side. As with all nodes on the
 *                      Merkle tree, this is a hash output.
 *
 *  right_node          The value of the child of this node which is on
 *                      the right-hand side. As with all nodes on the
 *                      Merkle tree, this is a hash output.
 *
 *  r_node_idx          The index of this node in the Merkle tree. Note
 *                      that the root node of the Merkle tree is
 *                      1-indexed.
 *
 *  out                 The output node value, which is a hash output.
 */
static int create_merkle_internal_value(const mbedtls_lms_parameters_t *params,
                                        const unsigned char *left_node,
                                        const unsigned char *right_node,
                                        unsigned int r_node_idx,
                                        unsigned char *out)
{
    psa_hash_operation_t op;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t output_hash_len;
    unsigned char r_node_idx_bytes[4];

    op = psa_hash_operation_init();
    status = psa_hash_setup(&op, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, params->I_key_identifier,
                             MBEDTLS_LMOTS_I_KEY_ID_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    MBEDTLS_PUT_UINT32_BE(r_node_idx, r_node_idx_bytes, 0);
    status = psa_hash_update(&op, r_node_idx_bytes, 4);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, D_INTR_CONSTANT_BYTES, D_CONST_LEN);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, left_node,
                             MBEDTLS_LMS_M_NODE_BYTES(params->type));
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_update(&op, right_node,
                             MBEDTLS_LMS_M_NODE_BYTES(params->type));
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_hash_finish(&op, out, MBEDTLS_LMS_M_NODE_BYTES(params->type),
                             &output_hash_len);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

exit:
    psa_hash_abort(&op);

    return PSA_TO_MBEDTLS_ERR(status);
}

void mbedtls_lms_public_init(mbedtls_lms_public_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_lms_public_free(mbedtls_lms_public_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(*ctx));
}

int mbedtls_lms_import_public_key(mbedtls_lms_public_t *ctx,
                                  const unsigned char *key, size_t key_size)
{
    if (key_size < 4) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    uint32_t type = MBEDTLS_GET_UINT32_BE(key, PUBLIC_KEY_TYPE_OFFSET);
    if (type != (uint32_t) MBEDTLS_LMS_SHA256_M32_H10) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }
    ctx->params.type = (mbedtls_lms_algorithm_type_t) type;

    if (key_size != MBEDTLS_LMS_PUBLIC_KEY_LEN(ctx->params.type)) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    uint32_t otstype = MBEDTLS_GET_UINT32_BE(key, PUBLIC_KEY_OTSTYPE_OFFSET);
    if (otstype != (uint32_t) MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }
    ctx->params.otstype = (mbedtls_lmots_algorithm_type_t) otstype;

    memcpy(ctx->params.I_key_identifier,
           key + PUBLIC_KEY_I_KEY_ID_OFFSET,
           MBEDTLS_LMOTS_I_KEY_ID_LEN);
    memcpy(ctx->T_1_pub_key, key + PUBLIC_KEY_ROOT_NODE_OFFSET,
           MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type));

    ctx->have_public_key = 1;

    return 0;
}

int mbedtls_lms_export_public_key(const mbedtls_lms_public_t *ctx,
                                  unsigned char *key,
                                  size_t key_size, size_t *key_len)
{
    if (key_size < MBEDTLS_LMS_PUBLIC_KEY_LEN(ctx->params.type)) {
        return MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL;
    }

    if (!ctx->have_public_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    MBEDTLS_PUT_UINT32_BE(ctx->params.type, key, PUBLIC_KEY_TYPE_OFFSET);
    MBEDTLS_PUT_UINT32_BE(ctx->params.otstype, key, PUBLIC_KEY_OTSTYPE_OFFSET);
    memcpy(key + PUBLIC_KEY_I_KEY_ID_OFFSET,
           ctx->params.I_key_identifier,
           MBEDTLS_LMOTS_I_KEY_ID_LEN);
    memcpy(key +PUBLIC_KEY_ROOT_NODE_OFFSET,
           ctx->T_1_pub_key,
           MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type));

    if (key_len != NULL) {
        *key_len = MBEDTLS_LMS_PUBLIC_KEY_LEN(ctx->params.type);
    }

    return 0;
}

int mbedtls_lms_verify(const mbedtls_lms_public_t *ctx,
                       const unsigned char *msg, size_t msg_size,
                       const unsigned char *sig, size_t sig_size)
{
    unsigned int q_leaf_identifier;
    unsigned char Kc_candidate_ots_pub_key[MBEDTLS_LMOTS_N_HASH_LEN_MAX];
    unsigned char Tc_candidate_root_node[MBEDTLS_LMS_M_NODE_BYTES_MAX];
    unsigned int height;
    unsigned int curr_node_id;
    unsigned int parent_node_id;
    const unsigned char *left_node;
    const unsigned char *right_node;
    mbedtls_lmots_parameters_t ots_params;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (!ctx->have_public_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (ctx->params.type
        != MBEDTLS_LMS_SHA256_M32_H10) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (ctx->params.otstype
        != MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (sig_size != MBEDTLS_LMS_SIG_LEN(ctx->params.type, ctx->params.otstype)) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    if (sig_size < SIG_OTS_SIG_OFFSET + MBEDTLS_LMOTS_TYPE_LEN) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    if (MBEDTLS_GET_UINT32_BE(sig, SIG_OTS_SIG_OFFSET + MBEDTLS_LMOTS_SIG_TYPE_OFFSET)
        != MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    if (sig_size < SIG_TYPE_OFFSET(ctx->params.otstype) + MBEDTLS_LMS_TYPE_LEN) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    if (MBEDTLS_GET_UINT32_BE(sig, SIG_TYPE_OFFSET(ctx->params.otstype))
        != MBEDTLS_LMS_SHA256_M32_H10) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }


    q_leaf_identifier = MBEDTLS_GET_UINT32_BE(sig, SIG_Q_LEAF_ID_OFFSET);

    if (q_leaf_identifier >= MERKLE_TREE_LEAF_NODE_AM(ctx->params.type)) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    memcpy(ots_params.I_key_identifier,
           ctx->params.I_key_identifier,
           MBEDTLS_LMOTS_I_KEY_ID_LEN);
    MBEDTLS_PUT_UINT32_BE(q_leaf_identifier, ots_params.q_leaf_identifier, 0);
    ots_params.type = ctx->params.otstype;

    ret = mbedtls_lmots_calculate_public_key_candidate(&ots_params,
                                                       msg,
                                                       msg_size,
                                                       sig + SIG_OTS_SIG_OFFSET,
                                                       MBEDTLS_LMOTS_SIG_LEN(ctx->params.otstype),
                                                       Kc_candidate_ots_pub_key,
                                                       sizeof(Kc_candidate_ots_pub_key),
                                                       NULL);
    if (ret != 0) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    ret = create_merkle_leaf_value(
        &ctx->params,
        Kc_candidate_ots_pub_key,
        MERKLE_TREE_INTERNAL_NODE_AM(ctx->params.type) + q_leaf_identifier,
        Tc_candidate_root_node);

    if (ret != 0) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }
    curr_node_id = MERKLE_TREE_INTERNAL_NODE_AM(ctx->params.type) +
                   q_leaf_identifier;

    for (height = 0; height < MBEDTLS_LMS_H_TREE_HEIGHT(ctx->params.type);
         height++) {
        parent_node_id = curr_node_id / 2;

        /* Left/right node ordering matters for the hash */
        if (curr_node_id & 1) {
            left_node = sig + SIG_PATH_OFFSET(ctx->params.otstype) +
                        height * MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type);
            right_node = Tc_candidate_root_node;
        } else {
            left_node = Tc_candidate_root_node;
            right_node = sig + SIG_PATH_OFFSET(ctx->params.otstype) +
                         height * MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type);
        }

        ret = create_merkle_internal_value(&ctx->params, left_node, right_node,
                                           parent_node_id, Tc_candidate_root_node);
        if (ret != 0) {
            return MBEDTLS_ERR_LMS_VERIFY_FAILED;
        }
        curr_node_id /= 2;
    }

    if (memcmp(Tc_candidate_root_node, ctx->T_1_pub_key,
               MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type))) {
        return MBEDTLS_ERR_LMS_VERIFY_FAILED;
    }

    return 0;
}

#if defined(MBEDTLS_LMS_PRIVATE)

/* Calculate a full Merkle tree based on a private key. This function
 * implements RFC8554 section 5.3, and is used to generate a public key (as the
 * public key is the root node of the Merkle tree).
 *
 *  ctx                 The LMS private context, containing a parameter
 *                      set and private key material consisting of both
 *                      public and private OTS.
 *
 *  tree                The output tree, which is 2^(H + 1) hash outputs.
 *                      In the case of H=10 we have 2048 tree nodes (of
 *                      which 1024 of them are leaf nodes). Note that
 *                      because the Merkle tree root is 1-indexed, the 0
 *                      index tree node is never used.
 */
static int calculate_merkle_tree(const mbedtls_lms_private_t *ctx,
                                 unsigned char *tree)
{
    unsigned int priv_key_idx;
    unsigned int r_node_idx;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /* First create the leaf nodes, in ascending order */
    for (priv_key_idx = 0;
         priv_key_idx < MERKLE_TREE_INTERNAL_NODE_AM(ctx->params.type);
         priv_key_idx++) {
        r_node_idx = MERKLE_TREE_INTERNAL_NODE_AM(ctx->params.type) + priv_key_idx;

        ret = create_merkle_leaf_value(&ctx->params,
                                       ctx->ots_public_keys[priv_key_idx].public_key,
                                       r_node_idx,
                                       &tree[r_node_idx * MBEDTLS_LMS_M_NODE_BYTES(
                                                 ctx->params.type)]);
        if (ret != 0) {
            return ret;
        }
    }

    /* Then the internal nodes, in reverse order so that we can guarantee the
     * parent has been created */
    for (r_node_idx = MERKLE_TREE_INTERNAL_NODE_AM(ctx->params.type) - 1;
         r_node_idx > 0;
         r_node_idx--) {
        ret = create_merkle_internal_value(&ctx->params,
                                           &tree[(r_node_idx * 2) *
                                                 MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type)],
                                           &tree[(r_node_idx * 2 + 1) *
                                                 MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type)],
                                           r_node_idx,
                                           &tree[r_node_idx *
                                                 MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type)]);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

/* Calculate a path from a leaf node of the Merkle tree to the root of the tree,
 * and return the full path. This function implements RFC8554 section 5.4.1, as
 * the Merkle path is the main component of an LMS signature.
 *
 *  ctx                 The LMS private context, containing a parameter
 *                      set and private key material consisting of both
 *                      public and private OTS.
 *
 *  leaf_node_id        Which leaf node to calculate the path from.
 *
 *  path                The output path, which is H hash outputs.
 */
static int get_merkle_path(mbedtls_lms_private_t *ctx,
                           unsigned int leaf_node_id,
                           unsigned char *path)
{
    const size_t node_bytes = MBEDTLS_LMS_M_NODE_BYTES(ctx->params.type);
    unsigned int curr_node_id = leaf_node_id;
    unsigned int adjacent_node_id;
    unsigned char *tree = NULL;
    unsigned int height;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    tree = mbedtls_calloc((size_t) MERKLE_TREE_NODE_AM(ctx->params.type),
                          node_bytes);
    if (tree == NULL) {
        return MBEDTLS_ERR_LMS_ALLOC_FAILED;
    }

    ret = calculate_merkle_tree(ctx, tree);
    if (ret != 0) {
        goto exit;
    }

    for (height = 0; height < MBEDTLS_LMS_H_TREE_HEIGHT(ctx->params.type);
         height++) {
        adjacent_node_id = curr_node_id ^ 1;

        memcpy(&path[height * node_bytes],
               &tree[adjacent_node_id * node_bytes], node_bytes);

        curr_node_id >>= 1;
    }

    ret = 0;

exit:
    mbedtls_zeroize_and_free(tree, node_bytes *
                             (size_t) MERKLE_TREE_NODE_AM(ctx->params.type));

    return ret;
}

void mbedtls_lms_private_init(mbedtls_lms_private_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_lms_private_free(mbedtls_lms_private_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    unsigned int idx;

    if (ctx->have_private_key) {
        if (ctx->ots_private_keys != NULL) {
            for (idx = 0; idx < MERKLE_TREE_LEAF_NODE_AM(ctx->params.type); idx++) {
                mbedtls_lmots_private_free(&ctx->ots_private_keys[idx]);
            }
        }

        if (ctx->ots_public_keys != NULL) {
            for (idx = 0; idx < MERKLE_TREE_LEAF_NODE_AM(ctx->params.type); idx++) {
                mbedtls_lmots_public_free(&ctx->ots_public_keys[idx]);
            }
        }

        mbedtls_free(ctx->ots_private_keys);
        mbedtls_free(ctx->ots_public_keys);
    }

    mbedtls_platform_zeroize(ctx, sizeof(*ctx));
}


int mbedtls_lms_generate_private_key(mbedtls_lms_private_t *ctx,
                                     mbedtls_lms_algorithm_type_t type,
                                     mbedtls_lmots_algorithm_type_t otstype,
                                     const unsigned char *seed,
                                     size_t seed_size)
{
    unsigned int idx = 0;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (type != MBEDTLS_LMS_SHA256_M32_H10) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (otstype != MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (ctx->have_private_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    ctx->params.type = type;
    ctx->params.otstype = otstype;
    ctx->have_private_key = 1;

    ret = psa_generate_random(ctx->params.I_key_identifier,
                              MBEDTLS_LMOTS_I_KEY_ID_LEN);
    if (ret != 0) {
        goto exit;
    }

    /* Requires a cast to size_t to avoid an implicit cast warning on certain
     * platforms (particularly Windows) */
    ctx->ots_private_keys = mbedtls_calloc((size_t) MERKLE_TREE_LEAF_NODE_AM(ctx->params.type),
                                           sizeof(*ctx->ots_private_keys));
    if (ctx->ots_private_keys == NULL) {
        ret = MBEDTLS_ERR_LMS_ALLOC_FAILED;
        goto exit;
    }

    /* Requires a cast to size_t to avoid an implicit cast warning on certain
     * platforms (particularly Windows) */
    ctx->ots_public_keys = mbedtls_calloc((size_t) MERKLE_TREE_LEAF_NODE_AM(ctx->params.type),
                                          sizeof(*ctx->ots_public_keys));
    if (ctx->ots_public_keys == NULL) {
        ret = MBEDTLS_ERR_LMS_ALLOC_FAILED;
        goto exit;
    }

    for (idx = 0; idx < MERKLE_TREE_LEAF_NODE_AM(ctx->params.type); idx++) {
        mbedtls_lmots_private_init(&ctx->ots_private_keys[idx]);
        mbedtls_lmots_public_init(&ctx->ots_public_keys[idx]);
    }


    for (idx = 0; idx < MERKLE_TREE_LEAF_NODE_AM(ctx->params.type); idx++) {
        ret = mbedtls_lmots_generate_private_key(&ctx->ots_private_keys[idx],
                                                 otstype,
                                                 ctx->params.I_key_identifier,
                                                 idx, seed, seed_size);
        if (ret != 0) {
            goto exit;
        }

        ret = mbedtls_lmots_calculate_public_key(&ctx->ots_public_keys[idx],
                                                 &ctx->ots_private_keys[idx]);
        if (ret != 0) {
            goto exit;
        }
    }

    ctx->q_next_usable_key = 0;

exit:
    if (ret != 0) {
        mbedtls_lms_private_free(ctx);
    }

    return ret;
}

int mbedtls_lms_calculate_public_key(mbedtls_lms_public_t *ctx,
                                     const mbedtls_lms_private_t *priv_ctx)
{
    const size_t node_bytes = MBEDTLS_LMS_M_NODE_BYTES(priv_ctx->params.type);
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *tree = NULL;

    if (!priv_ctx->have_private_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (priv_ctx->params.type
        != MBEDTLS_LMS_SHA256_M32_H10) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (priv_ctx->params.otstype
        != MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    tree = mbedtls_calloc((size_t) MERKLE_TREE_NODE_AM(priv_ctx->params.type),
                          node_bytes);
    if (tree == NULL) {
        return MBEDTLS_ERR_LMS_ALLOC_FAILED;
    }

    memcpy(&ctx->params, &priv_ctx->params,
           sizeof(mbedtls_lmots_parameters_t));

    ret = calculate_merkle_tree(priv_ctx, tree);
    if (ret != 0) {
        goto exit;
    }

    /* Root node is always at position 1, due to 1-based indexing */
    memcpy(ctx->T_1_pub_key, &tree[node_bytes], node_bytes);

    ctx->have_public_key = 1;

    ret = 0;

exit:
    mbedtls_zeroize_and_free(tree, node_bytes *
                             (size_t) MERKLE_TREE_NODE_AM(priv_ctx->params.type));

    return ret;
}


int mbedtls_lms_sign(mbedtls_lms_private_t *ctx,
                     const unsigned char *msg,
                     unsigned int msg_size, unsigned char *sig, size_t sig_size,
                     size_t *sig_len)
{
    uint32_t q_leaf_identifier;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (!ctx->have_private_key) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (sig_size < MBEDTLS_LMS_SIG_LEN(ctx->params.type, ctx->params.otstype)) {
        return MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL;
    }

    if (ctx->params.type != MBEDTLS_LMS_SHA256_M32_H10) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (ctx->params.otstype
        != MBEDTLS_LMOTS_SHA256_N32_W8) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    if (ctx->q_next_usable_key >= MERKLE_TREE_LEAF_NODE_AM(ctx->params.type)) {
        return MBEDTLS_ERR_LMS_OUT_OF_PRIVATE_KEYS;
    }


    q_leaf_identifier = ctx->q_next_usable_key;
    /* This new value must _always_ be written back to the disk before the
     * signature is returned.
     */
    ctx->q_next_usable_key += 1;

    if (MBEDTLS_LMS_SIG_LEN(ctx->params.type, ctx->params.otstype)
        < SIG_OTS_SIG_OFFSET) {
        return MBEDTLS_ERR_LMS_BAD_INPUT_DATA;
    }

    ret = mbedtls_lmots_sign(&ctx->ots_private_keys[q_leaf_identifier],
                             msg,
                             msg_size,
                             sig + SIG_OTS_SIG_OFFSET,
                             MBEDTLS_LMS_SIG_LEN(ctx->params.type,
                                                 ctx->params.otstype) - SIG_OTS_SIG_OFFSET,
                             NULL);
    if (ret != 0) {
        return ret;
    }

    MBEDTLS_PUT_UINT32_BE(ctx->params.type, sig, SIG_TYPE_OFFSET(ctx->params.otstype));
    MBEDTLS_PUT_UINT32_BE(q_leaf_identifier, sig, SIG_Q_LEAF_ID_OFFSET);

    ret = get_merkle_path(ctx,
                          MERKLE_TREE_INTERNAL_NODE_AM(ctx->params.type) + q_leaf_identifier,
                          sig + SIG_PATH_OFFSET(ctx->params.otstype));
    if (ret != 0) {
        return ret;
    }

    if (sig_len != NULL) {
        *sig_len = MBEDTLS_LMS_SIG_LEN(ctx->params.type, ctx->params.otstype);
    }


    return 0;
}

#endif /* defined(MBEDTLS_LMS_PRIVATE) */
#endif /* defined(MBEDTLS_LMS_C) */

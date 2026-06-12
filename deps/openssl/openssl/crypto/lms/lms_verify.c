/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/byteorder.h>
#include "crypto/lms_sig.h"
#include "crypto/lms_util.h"
#include "internal/common.h"

/*
 * Constants used for obtaining unique inputs for different hashing operations
 * e.g H(I || q || OSSL_LMS_D_LEAF || ... )
 */
const uint16_t OSSL_LMS_D_PBLC          = 0x8080;
const uint16_t OSSL_LMS_D_MESG          = 0x8181;
const uint16_t OSSL_LMS_D_LEAF          = 0x8282;
const uint16_t OSSL_LMS_D_INTR          = 0x8383;

/*
 * @brief Compute the candidate LMS root value Tc
 *
 * @param paths An array of bytes representing the hash values associated with
 *              the path through the tree from the leaf associated with the
 *              LM-OTS signature to the root public key node.
 * @param n The hash output size (The size of each path in |paths|)
 * @param nodenum The leaf index node number. The root node had a value of 1
 *                Each subsequent level has nodes in the range 2^h...2^(h+1)-1
 * @param ctx A EVP_MD_CTX object used for calculations
 * @param ctxI A EVP_MD_CTX object containing an unfinalised H(I)
 * @param Tc Contains H(I || u32str(node_num) || u16str(D_LEAF) || Kc) on input,
 *           and on output returns the calculated candidate public key.
 * @returns 1 on success, or 0 otherwise.
 */
static
int lms_sig_compute_tc_from_path(const unsigned char *paths, uint32_t n,
                                 uint32_t node_num,
                                 EVP_MD_CTX *ctx, EVP_MD_CTX *ctxI,
                                 unsigned char *Tc)
{
    int ret = 0;
    unsigned char qbuf[4];
    unsigned char d_intr[sizeof(uint16_t)];
    const unsigned char *path = paths;

    OPENSSL_store_u16_be(d_intr, OSSL_LMS_D_INTR);

    /*
     * Calculate the public key Tc using the path
     * The root hash is the hash of its 2 children's Hash values.
     * A child hash for each level is passed in by paths, and we have
     * a leaf value that can be used with the path to calculate the parent
     * hash.
     */
    while (node_num > 1) {
        /* At each level the path contains either the left or right child */
        int odd = node_num & 1;

        node_num = node_num >> 1; /* get the parent node_num */
        OPENSSL_store_u32_be(qbuf, node_num);

        /*
         * Calculate Tc as either
         *   Tc(parent) = H(I || node_q || 0x8383 || paths[i][n] || Tc(right) OR
         *   Tc(parent) = H(I || node_q || 0x8383 || Tc(left) || paths[i][n])
         */
        if (!EVP_MD_CTX_copy_ex(ctx, ctxI)
                || !EVP_DigestUpdate(ctx, qbuf, sizeof(qbuf))
                || !EVP_DigestUpdate(ctx, d_intr, sizeof(d_intr)))
            goto err;

        if (odd) {
            if (!EVP_DigestUpdate(ctx, path, n)
                || !EVP_DigestUpdate(ctx, Tc, n))
                goto err;
        } else {
            if (!EVP_DigestUpdate(ctx, Tc, n)
                || !EVP_DigestUpdate(ctx, path, n))
                goto err;
        }
        /*
         * Tc = parent Hash, which is either the left or right child for the next
         * level up (node_num determines if it is left or right).
         */
        if (!EVP_DigestFinal_ex(ctx, Tc, NULL))
            goto err;
        path += n;
    }
    ret = 1;
err:
    return ret;
}

/*
 * @brief LMS signature verification.
 * See RFC 8554 Section 5.4.2. Algorithm 6: Steps 3 & 4
 *
 * @param lms_sig Is a valid decoded LMS_SIG signature object.
 * @param pub Is a valid LMS public key object.
 * @param md Contains the fetched digest to be used for Hash operations
 * @param msg A message to verify
 * @param msglen The size of |msg|
 * @returns 1 if the verification succeeded, or 0 otherwise.
 */
int ossl_lms_sig_verify(const LMS_SIG *lms_sig, const LMS_KEY *pub,
                        const EVP_MD *md,
                        const unsigned char *msg, size_t msglen)
{
    int ret = 0;
    EVP_MD_CTX *ctx = NULL, *ctxIq = NULL;
    EVP_MD_CTX *ctxI;
    unsigned char Kc[LMS_MAX_DIGEST_SIZE];
    unsigned char Tc[LMS_MAX_DIGEST_SIZE];
    unsigned char qbuf[4];
    unsigned char d_leaf[sizeof(uint16_t)];
    const LMS_PARAMS *lms_params = pub->lms_params;
    uint32_t n = lms_params->n;
    uint32_t node_num;

    ctx = EVP_MD_CTX_create();
    ctxIq = EVP_MD_CTX_create();
    if (ctx == NULL || ctxIq == NULL)
        goto err;

    if (!lms_evp_md_ctx_init(ctxIq, md, lms_sig->params))
        goto err;
    /*
     * Algorithm 6a: Step 3.
     * Calculate a candidate public key |Kc| using the lmots_signature, message,
     * and the identifiers I, q
     */
    if (!ossl_lm_ots_compute_pubkey(ctx, ctxIq, &lms_sig->sig,
                                    pub->ots_params, pub->Id,
                                    lms_sig->q, msg, msglen, Kc))
        goto err;

    /*
     * Algorithm 6a: Step 4
     * Compute the candidate LMS root value Tc
     */
    if (!ossl_assert(lms_sig->q < (uint32_t)(1 << lms_params->h)))
        return 0;
    node_num = (1 << lms_params->h) + lms_sig->q;

    OPENSSL_store_u32_be(qbuf, node_num);
    OPENSSL_store_u16_be(d_leaf, OSSL_LMS_D_LEAF);
    ctxI = ctxIq;
    /*
     * Tc = H(I || u32str(node_num) || u16str(D_LEAF) || Kc)
     *
     * ctx is left initialised with the md from ossl_lm_ots_compute_pubkey,
     * so there is no need to reinitialise it here.
     */
    if (!EVP_DigestInit_ex2(ctx, NULL, NULL)
            || !EVP_DigestUpdate(ctx, pub->Id, LMS_SIZE_I)
            || !EVP_MD_CTX_copy_ex(ctxI, ctx)
            || !EVP_DigestUpdate(ctx, qbuf, sizeof(qbuf))
            || !EVP_DigestUpdate(ctx, d_leaf, sizeof(d_leaf))
            || !EVP_DigestUpdate(ctx, Kc, n)
            || !EVP_DigestFinal_ex(ctx, Tc, NULL)
            || !lms_sig_compute_tc_from_path(lms_sig->paths, n, node_num,
                                             ctx, ctxI, Tc))
        goto err;
    /* Algorithm 6: Step 4 */
    ret = (memcmp(pub->pub.K, Tc, n) == 0);
err:
    EVP_MD_CTX_free(ctxIq);
    EVP_MD_CTX_free(ctx);
    return ret;
}

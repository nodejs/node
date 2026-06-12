/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/lms_sig.h"
#include "crypto/lms_util.h"

/**
 * @brief Decode a byte array containing XDR signature data into a LMS_SIG object.
 *
 * This is used for LMS Signature Verification.
 * This function may be called multiple times.
 * See RFC 8554 Algorithm 6a: Steps 1 and 2.
 * It uses shallow copies for C, y and path.
 *
 * @param pkt Contains the signature data to decode. There may still be data
 *            remaining in pkt after decoding.
 * @param pub A public key that contains LMS_PARAMS and LM_OTS_PARAMS associated
 *            with the signature.
 * @returns The created LMS_SIG object if successful, or NULL on failure. A
 *          failure may occur if the passed in LMS public key |pub| is not
 *          compatible with the decoded LMS_SIG object,
 */
LMS_SIG *ossl_lms_sig_from_pkt(PACKET *pkt, const LMS_KEY *pub)
{
    uint32_t sig_ots_type = 0, sig_lms_type = 0;
    const LMS_PARAMS *lparams = pub->lms_params;
    const LM_OTS_PARAMS *pub_ots_params = pub->ots_params;
    const LM_OTS_PARAMS *sig_params;
    LMS_SIG *lsig = NULL;

    lsig = ossl_lms_sig_new();
    if (lsig == NULL)
        return NULL;

    if (!PACKET_get_net_4_len_u32(pkt, &lsig->q)    /* q = Leaf Index */
            || !PACKET_get_net_4_len_u32(pkt, &sig_ots_type)
            || pub_ots_params->lm_ots_type != sig_ots_type)
        goto err;
    sig_params = pub_ots_params;
    lsig->sig.params = sig_params;
    lsig->params = lparams;

    if (!PACKET_get_bytes(pkt, (const unsigned char **)&lsig->sig.C,
                          sig_params->n)
            || !PACKET_get_bytes(pkt, (const unsigned char **)&lsig->sig.y,
                                 sig_params->p * sig_params->n)
            || !PACKET_get_net_4_len_u32(pkt, &sig_lms_type)
            || (lparams->lms_type != sig_lms_type)
            || HASH_NOT_MATCHED(lparams, sig_params)
            || lsig->q >= (uint32_t)(1 << lparams->h)
            || !PACKET_get_bytes(pkt, (const unsigned char **)&lsig->paths,
                                 lparams->h * lparams->n)
            || PACKET_remaining(pkt) > 0)
        goto err;
    return lsig;
err:
    ossl_lms_sig_free(lsig);
    return NULL;
}

/**
 * @brief Decode a byte array of LMS signature data.
 *
 * This function does not duplicate any of the byte data contained within
 * |sig|. So it is expected that |sig| will exist for the duration of the
 * returned signature |out|.
 *
 * @param out Used to return the LMS_SIG object.
 * @param pub The root public LMS key
 * @param sig A input byte array of signature data.
 * @param siglen The size of sig.
 * @returns 1 if the signature is successfully decoded,
 *          otherwise it returns 0.
 */
int ossl_lms_sig_decode(LMS_SIG **out, LMS_KEY *pub,
                        const unsigned char *sig, size_t siglen)
{
    PACKET pkt;
    LMS_SIG *s = NULL;

    if (pub == NULL)
        return 0;

    if (!PACKET_buf_init(&pkt, sig, siglen))
        return 0;

    s = ossl_lms_sig_from_pkt(&pkt, pub);
    if (s == NULL)
        return 0;

    /* Fail if there are trailing bytes */
    if (PACKET_remaining(&pkt) > 0)
        goto err;
    *out = s;
    return 1;
err:
    ossl_lms_sig_free(s);
    return 0;
}

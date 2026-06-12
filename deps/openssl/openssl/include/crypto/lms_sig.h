/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Internal LMS/LM_OTS functions for other submodules,
 * not for application use
 *
 * Refer to RFC 8554 Sections 5.4 & 4.5 for information related to
 * LMS Signatures & LM_OTS Signature Generation respectively.
 */

#ifndef OSSL_CRYPTO_LMS_SIG_H
# define OSSL_CRYPTO_LMS_SIG_H
# pragma once
# ifndef OPENSSL_NO_LMS
#  include "lms.h"
#  include "internal/packet.h"

/* The values defined for 8 byte TAGS */
extern const uint16_t OSSL_LMS_D_PBLC;      /* 8080 */
extern const uint16_t OSSL_LMS_D_MESG;      /* 8181 */
extern const uint16_t OSSL_LMS_D_LEAF;      /* 8282 */
extern const uint16_t OSSL_LMS_D_INTR;      /* 8383 */

/* Used by OTS signature when calculating Q || Cksm(Q) */
#  define LMS_SIZE_CHECKSUM 2
#  define LMS_SIZE_QSUM 2

/*
 * An object for storing a One-Time Signature
 * See RFC 8554 Section 4.5
 */
typedef struct lm_ots_sig_st {
    const LM_OTS_PARAMS *params;
    /* For verify operations the following pointers are not allocated */
    unsigned char *C; /* A salt value of size n */
    unsigned char *y; /* The trailing part of a signature of size p * n */
} LM_OTS_SIG;

/*
 * An object for storing a LMS signature
 * See RFC 8554 Section 5.4
 */
typedef struct lms_signature_st {
    uint32_t q;
    LM_OTS_SIG sig;
    const LMS_PARAMS *params; /* contains the LMS type */
    unsigned char *paths; /* size is h * m */
} LMS_SIG;

LMS_SIG *ossl_lms_sig_new(void);
void ossl_lms_sig_free(LMS_SIG *sig);
LMS_SIG *ossl_lms_sig_from_pkt(PACKET *pkt, const LMS_KEY *pub);
int ossl_lms_sig_decode(LMS_SIG **out, LMS_KEY *pub,
                        const unsigned char *sig, size_t siglen);
int ossl_lms_sig_verify(const LMS_SIG *lms_sig, const LMS_KEY *pub,
                        const EVP_MD *md,
                        const unsigned char *msg, size_t msglen);

int ossl_lm_ots_compute_pubkey(EVP_MD_CTX *ctx, EVP_MD_CTX *ctxIq,
                               const LM_OTS_SIG *sig, const LM_OTS_PARAMS *pub,
                               const unsigned char *Id, uint32_t q,
                               const unsigned char *msg, size_t msglen,
                               unsigned char *Kc);
uint16_t ossl_lm_ots_params_checksum(const LM_OTS_PARAMS *params,
                                     const unsigned char *S);

# endif /* OPENSSL_NO_LMS */
#endif /* OSSL_CRYPTO_LMS_SIG_H */

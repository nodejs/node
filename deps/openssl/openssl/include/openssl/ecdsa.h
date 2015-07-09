/* crypto/ecdsa/ecdsa.h */
/**
 * \file   crypto/ecdsa/ecdsa.h Include file for the OpenSSL ECDSA functions
 * \author Written by Nils Larsch for the OpenSSL project
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
#ifndef HEADER_ECDSA_H
# define HEADER_ECDSA_H

# include <openssl/opensslconf.h>

# ifdef OPENSSL_NO_ECDSA
#  error ECDSA is disabled.
# endif

# include <openssl/ec.h>
# include <openssl/ossl_typ.h>
# ifndef OPENSSL_NO_DEPRECATED
#  include <openssl/bn.h>
# endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ECDSA_SIG_st {
    BIGNUM *r;
    BIGNUM *s;
} ECDSA_SIG;

/** Allocates and initialize a ECDSA_SIG structure
 *  \return pointer to a ECDSA_SIG structure or NULL if an error occurred
 */
ECDSA_SIG *ECDSA_SIG_new(void);

/** frees a ECDSA_SIG structure
 *  \param  sig  pointer to the ECDSA_SIG structure
 */
void ECDSA_SIG_free(ECDSA_SIG *sig);

/** DER encode content of ECDSA_SIG object (note: this function modifies *pp
 *  (*pp += length of the DER encoded signature)).
 *  \param  sig  pointer to the ECDSA_SIG object
 *  \param  pp   pointer to a unsigned char pointer for the output or NULL
 *  \return the length of the DER encoded ECDSA_SIG object or 0
 */
int i2d_ECDSA_SIG(const ECDSA_SIG *sig, unsigned char **pp);

/** Decodes a DER encoded ECDSA signature (note: this function changes *pp
 *  (*pp += len)).
 *  \param  sig  pointer to ECDSA_SIG pointer (may be NULL)
 *  \param  pp   memory buffer with the DER encoded signature
 *  \param  len  length of the buffer
 *  \return pointer to the decoded ECDSA_SIG structure (or NULL)
 */
ECDSA_SIG *d2i_ECDSA_SIG(ECDSA_SIG **sig, const unsigned char **pp, long len);

/** Computes the ECDSA signature of the given hash value using
 *  the supplied private key and returns the created signature.
 *  \param  dgst      pointer to the hash value
 *  \param  dgst_len  length of the hash value
 *  \param  eckey     EC_KEY object containing a private EC key
 *  \return pointer to a ECDSA_SIG structure or NULL if an error occurred
 */
ECDSA_SIG *ECDSA_do_sign(const unsigned char *dgst, int dgst_len,
                         EC_KEY *eckey);

/** Computes ECDSA signature of a given hash value using the supplied
 *  private key (note: sig must point to ECDSA_size(eckey) bytes of memory).
 *  \param  dgst     pointer to the hash value to sign
 *  \param  dgstlen  length of the hash value
 *  \param  kinv     BIGNUM with a pre-computed inverse k (optional)
 *  \param  rp       BIGNUM with a pre-computed rp value (optioanl),
 *                   see ECDSA_sign_setup
 *  \param  eckey    EC_KEY object containing a private EC key
 *  \return pointer to a ECDSA_SIG structure or NULL if an error occurred
 */
ECDSA_SIG *ECDSA_do_sign_ex(const unsigned char *dgst, int dgstlen,
                            const BIGNUM *kinv, const BIGNUM *rp,
                            EC_KEY *eckey);

/** Verifies that the supplied signature is a valid ECDSA
 *  signature of the supplied hash value using the supplied public key.
 *  \param  dgst      pointer to the hash value
 *  \param  dgst_len  length of the hash value
 *  \param  sig       ECDSA_SIG structure
 *  \param  eckey     EC_KEY object containing a public EC key
 *  \return 1 if the signature is valid, 0 if the signature is invalid
 *          and -1 on error
 */
int ECDSA_do_verify(const unsigned char *dgst, int dgst_len,
                    const ECDSA_SIG *sig, EC_KEY *eckey);

const ECDSA_METHOD *ECDSA_OpenSSL(void);

/** Sets the default ECDSA method
 *  \param  meth  new default ECDSA_METHOD
 */
void ECDSA_set_default_method(const ECDSA_METHOD *meth);

/** Returns the default ECDSA method
 *  \return pointer to ECDSA_METHOD structure containing the default method
 */
const ECDSA_METHOD *ECDSA_get_default_method(void);

/** Sets method to be used for the ECDSA operations
 *  \param  eckey  EC_KEY object
 *  \param  meth   new method
 *  \return 1 on success and 0 otherwise
 */
int ECDSA_set_method(EC_KEY *eckey, const ECDSA_METHOD *meth);

/** Returns the maximum length of the DER encoded signature
 *  \param  eckey  EC_KEY object
 *  \return numbers of bytes required for the DER encoded signature
 */
int ECDSA_size(const EC_KEY *eckey);

/** Precompute parts of the signing operation
 *  \param  eckey  EC_KEY object containing a private EC key
 *  \param  ctx    BN_CTX object (optional)
 *  \param  kinv   BIGNUM pointer for the inverse of k
 *  \param  rp     BIGNUM pointer for x coordinate of k * generator
 *  \return 1 on success and 0 otherwise
 */
int ECDSA_sign_setup(EC_KEY *eckey, BN_CTX *ctx, BIGNUM **kinv, BIGNUM **rp);

/** Computes ECDSA signature of a given hash value using the supplied
 *  private key (note: sig must point to ECDSA_size(eckey) bytes of memory).
 *  \param  type     this parameter is ignored
 *  \param  dgst     pointer to the hash value to sign
 *  \param  dgstlen  length of the hash value
 *  \param  sig      memory for the DER encoded created signature
 *  \param  siglen   pointer to the length of the returned signature
 *  \param  eckey    EC_KEY object containing a private EC key
 *  \return 1 on success and 0 otherwise
 */
int ECDSA_sign(int type, const unsigned char *dgst, int dgstlen,
               unsigned char *sig, unsigned int *siglen, EC_KEY *eckey);

/** Computes ECDSA signature of a given hash value using the supplied
 *  private key (note: sig must point to ECDSA_size(eckey) bytes of memory).
 *  \param  type     this parameter is ignored
 *  \param  dgst     pointer to the hash value to sign
 *  \param  dgstlen  length of the hash value
 *  \param  sig      buffer to hold the DER encoded signature
 *  \param  siglen   pointer to the length of the returned signature
 *  \param  kinv     BIGNUM with a pre-computed inverse k (optional)
 *  \param  rp       BIGNUM with a pre-computed rp value (optioanl),
 *                   see ECDSA_sign_setup
 *  \param  eckey    EC_KEY object containing a private EC key
 *  \return 1 on success and 0 otherwise
 */
int ECDSA_sign_ex(int type, const unsigned char *dgst, int dgstlen,
                  unsigned char *sig, unsigned int *siglen,
                  const BIGNUM *kinv, const BIGNUM *rp, EC_KEY *eckey);

/** Verifies that the given signature is valid ECDSA signature
 *  of the supplied hash value using the specified public key.
 *  \param  type     this parameter is ignored
 *  \param  dgst     pointer to the hash value
 *  \param  dgstlen  length of the hash value
 *  \param  sig      pointer to the DER encoded signature
 *  \param  siglen   length of the DER encoded signature
 *  \param  eckey    EC_KEY object containing a public EC key
 *  \return 1 if the signature is valid, 0 if the signature is invalid
 *          and -1 on error
 */
int ECDSA_verify(int type, const unsigned char *dgst, int dgstlen,
                 const unsigned char *sig, int siglen, EC_KEY *eckey);

/* the standard ex_data functions */
int ECDSA_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new
                           *new_func, CRYPTO_EX_dup *dup_func,
                           CRYPTO_EX_free *free_func);
int ECDSA_set_ex_data(EC_KEY *d, int idx, void *arg);
void *ECDSA_get_ex_data(EC_KEY *d, int idx);

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_ECDSA_strings(void);

/* Error codes for the ECDSA functions. */

/* Function codes. */
# define ECDSA_F_ECDSA_CHECK                              104
# define ECDSA_F_ECDSA_DATA_NEW_METHOD                    100
# define ECDSA_F_ECDSA_DO_SIGN                            101
# define ECDSA_F_ECDSA_DO_VERIFY                          102
# define ECDSA_F_ECDSA_SIGN_SETUP                         103

/* Reason codes. */
# define ECDSA_R_BAD_SIGNATURE                            100
# define ECDSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE              101
# define ECDSA_R_ERR_EC_LIB                               102
# define ECDSA_R_MISSING_PARAMETERS                       103
# define ECDSA_R_NEED_NEW_SETUP_VALUES                    106
# define ECDSA_R_NON_FIPS_METHOD                          107
# define ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED          104
# define ECDSA_R_SIGNATURE_MALLOC_FAILED                  105

#ifdef  __cplusplus
}
#endif
#endif

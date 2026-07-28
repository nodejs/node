/**
 * \file lmots.h
 *
 * \brief This file provides an API for the LM-OTS post-quantum-safe one-time
 *        public-key signature scheme as defined in RFC8554 and NIST.SP.200-208.
 *        This implementation currently only supports a single parameter set
 *        MBEDTLS_LMOTS_SHA256_N32_W8 in order to reduce complexity.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_LMOTS_H
#define MBEDTLS_LMOTS_H

#include "mbedtls/build_info.h"

#include "psa/crypto.h"

#include "mbedtls/lms.h"

#include <stdint.h>
#include <stddef.h>


#define MBEDTLS_LMOTS_PUBLIC_KEY_LEN(type) (MBEDTLS_LMOTS_TYPE_LEN + \
                                            MBEDTLS_LMOTS_I_KEY_ID_LEN + \
                                            MBEDTLS_LMOTS_Q_LEAF_ID_LEN + \
                                            MBEDTLS_LMOTS_N_HASH_LEN(type))

#define MBEDTLS_LMOTS_SIG_TYPE_OFFSET       (0)
#define MBEDTLS_LMOTS_SIG_C_RANDOM_OFFSET (MBEDTLS_LMOTS_SIG_TYPE_OFFSET + \
                                           MBEDTLS_LMOTS_TYPE_LEN)
#define MBEDTLS_LMOTS_SIG_SIGNATURE_OFFSET(type) (MBEDTLS_LMOTS_SIG_C_RANDOM_OFFSET + \
                                                  MBEDTLS_LMOTS_C_RANDOM_VALUE_LEN(type))

#ifdef __cplusplus
extern "C" {
#endif


#if defined(MBEDTLS_TEST_HOOKS)
extern int (*mbedtls_lmots_sign_private_key_invalidated_hook)(unsigned char *);
#endif /* defined(MBEDTLS_TEST_HOOKS) */

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
/**
 * \brief                    This function converts a \ref psa_status_t to a
 *                           low-level LMS error code.
 *
 * \param status             The psa_status_t to convert
 *
 * \return                   The corresponding LMS error code.
 */
int MBEDTLS_DEPRECATED mbedtls_lms_error_from_psa(psa_status_t status);
#endif

/**
 * \brief                    This function initializes a public LMOTS context
 *
 * \param ctx                The uninitialized LMOTS context that will then be
 *                           initialized.
 */
void mbedtls_lmots_public_init(mbedtls_lmots_public_t *ctx);

/**
 * \brief                    This function uninitializes a public LMOTS context
 *
 * \param ctx                The initialized LMOTS context that will then be
 *                           uninitialized.
 */
void mbedtls_lmots_public_free(mbedtls_lmots_public_t *ctx);

/**
 * \brief                    This function imports an LMOTS public key into a
 *                           LMOTS context.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized.
 *
 * \note                     See IETF RFC8554 for details of the encoding of
 *                           this public key.
 *
 * \param ctx                The initialized LMOTS context store the key in.
 * \param key                The buffer from which the key will be read.
 *                           #MBEDTLS_LMOTS_PUBLIC_KEY_LEN bytes will be read
 *                           from this.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lmots_import_public_key(mbedtls_lmots_public_t *ctx,
                                    const unsigned char *key, size_t key_size);

/**
 * \brief                    This function exports an LMOTS public key from a
 *                           LMOTS context that already contains a public key.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized and the context must contain
 *                           a public key.
 *
 * \note                     See IETF RFC8554 for details of the encoding of
 *                           this public key.
 *
 * \param ctx                The initialized LMOTS context that contains the
 *                           public key.
 * \param key                The buffer into which the key will be output. Must
 *                           be at least #MBEDTLS_LMOTS_PUBLIC_KEY_LEN in size.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lmots_export_public_key(const mbedtls_lmots_public_t *ctx,
                                    unsigned char *key, size_t key_size,
                                    size_t *key_len);

/**
 * \brief                    This function creates a candidate public key from
 *                           an LMOTS signature. This can then be compared to
 *                           the real public key to determine the validity of
 *                           the signature.
 *
 * \note                     This function is exposed publicly to be used in LMS
 *                           signature verification, it is expected that
 *                           mbedtls_lmots_verify will be used for LMOTS
 *                           signature verification.
 *
 * \param params             The LMOTS parameter set, q and I values as an
 *                           mbedtls_lmots_parameters_t struct.
 * \param msg                The buffer from which the message will be read.
 * \param msg_size           The size of the message that will be read.
 * \param sig                The buffer from which the signature will be read.
 *                           #MBEDTLS_LMOTS_SIG_LEN bytes will be read from
 *                           this.
 * \param out                The buffer where the candidate public key will be
 *                           stored. Must be at least #MBEDTLS_LMOTS_N_HASH_LEN
 *                           bytes in size.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lmots_calculate_public_key_candidate(const mbedtls_lmots_parameters_t *params,
                                                 const unsigned char *msg,
                                                 size_t msg_size,
                                                 const unsigned char *sig,
                                                 size_t sig_size,
                                                 unsigned char *out,
                                                 size_t out_size,
                                                 size_t *out_len);

/**
 * \brief                    This function verifies a LMOTS signature, using a
 *                           LMOTS context that contains a public key.
 *
 * \warning                  This function is **not intended for use in
 *                           production**, due to as-yet unsolved problems with
 *                           handling stateful keys. The API for this function
 *                           may change considerably in future versions.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized and must contain a public key
 *                           (either by import or calculation from a private
 *                           key).
 *
 * \param ctx                The initialized LMOTS context from which the public
 *                           key will be read.
 * \param msg                The buffer from which the message will be read.
 * \param msg_size           The size of the message that will be read.
 * \param sig                The buf from which the signature will be read.
 *                           #MBEDTLS_LMOTS_SIG_LEN bytes will be read from
 *                           this.
 *
 * \return         \c 0 on successful verification.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lmots_verify(const mbedtls_lmots_public_t *ctx,
                         const unsigned char *msg,
                         size_t msg_size, const unsigned char *sig,
                         size_t sig_size);

#if defined(MBEDTLS_LMS_PRIVATE)

/**
 * \brief                    This function initializes a private LMOTS context
 *
 * \param ctx                The uninitialized LMOTS context that will then be
 *                           initialized.
 */
void mbedtls_lmots_private_init(mbedtls_lmots_private_t *ctx);

/**
 * \brief                    This function uninitializes a private LMOTS context
 *
 * \param ctx                The initialized LMOTS context that will then be
 *                           uninitialized.
 */
void mbedtls_lmots_private_free(mbedtls_lmots_private_t *ctx);

/**
 * \brief                    This function calculates an LMOTS private key, and
 *                           stores in into an LMOTS context.
 *
 * \warning                  This function is **not intended for use in
 *                           production**, due to as-yet unsolved problems with
 *                           handling stateful keys. The API for this function
 *                           may change considerably in future versions.
 *
 * \note                     The seed must have at least 256 bits of entropy.
 *
 * \param ctx                The initialized LMOTS context to generate the key
 *                           into.
 * \param I_key_identifier   The key identifier of the key, as a 16-byte string.
 * \param q_leaf_identifier  The leaf identifier of key. If this LMOTS key is
 *                           not being used as part of an LMS key, this should
 *                           be set to 0.
 * \param seed               The seed used to deterministically generate the
 *                           key.
 * \param seed_size          The length of the seed.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lmots_generate_private_key(mbedtls_lmots_private_t *ctx,
                                       mbedtls_lmots_algorithm_type_t type,
                                       const unsigned char I_key_identifier[MBEDTLS_LMOTS_I_KEY_ID_LEN],
                                       uint32_t q_leaf_identifier,
                                       const unsigned char *seed,
                                       size_t seed_size);

/**
 * \brief                    This function generates an LMOTS public key from a
 *                           LMOTS context that already contains a private key.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized and the context must contain
 *                           a private key.
 *
 * \param ctx                The initialized LMOTS context to generate the key
 *                           from and store it into.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lmots_calculate_public_key(mbedtls_lmots_public_t *ctx,
                                       const mbedtls_lmots_private_t *priv_ctx);

/**
 * \brief                    This function creates a LMOTS signature, using a
 *                           LMOTS context that contains a private key.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized and must contain a private
 *                           key.
 *
 * \note                     LMOTS private keys can only be used once, otherwise
 *                           attackers may be able to create forged signatures.
 *                           If the signing operation is successful, the private
 *                           key in the context will be erased, and no further
 *                           signing will be possible until another private key
 *                           is loaded
 *
 * \param ctx                The initialized LMOTS context from which the
 *                           private key will be read.
 * \param f_rng              The RNG function to be used for signature
 *                           generation.
 * \param p_rng              The RNG context to be passed to f_rng
 * \param msg                The buffer from which the message will be read.
 * \param msg_size           The size of the message that will be read.
 * \param sig                The buf into which the signature will be stored.
 *                           Must be at least #MBEDTLS_LMOTS_SIG_LEN in size.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lmots_sign(mbedtls_lmots_private_t *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng, const unsigned char *msg, size_t msg_size,
                       unsigned char *sig, size_t sig_size, size_t *sig_len);

#endif /* defined(MBEDTLS_LMS_PRIVATE) */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_LMOTS_H */

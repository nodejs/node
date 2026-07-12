/**
 * \file ecjpake.h
 *
 * \brief Elliptic curve J-PAKE
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_ECJPAKE_H
#define MBEDTLS_ECJPAKE_H
#include "mbedtls/private_access.h"

/*
 * J-PAKE is a password-authenticated key exchange that allows deriving a
 * strong shared secret from a (potentially low entropy) pre-shared
 * passphrase, with forward secrecy and mutual authentication.
 * https://en.wikipedia.org/wiki/Password_Authenticated_Key_Exchange_by_Juggling
 *
 * This file implements the Elliptic Curve variant of J-PAKE,
 * as defined in Chapter 7.4 of the Thread v1.0 Specification,
 * available to members of the Thread Group http://threadgroup.org/
 *
 * As the J-PAKE algorithm is inherently symmetric, so is our API.
 * Each party needs to send its first round message, in any order, to the
 * other party, then each sends its second round message, in any order.
 * The payloads are serialized in a way suitable for use in TLS, but could
 * also be use outside TLS.
 */
#include "mbedtls/build_info.h"

#include "mbedtls/ecp.h"
#include "mbedtls/md.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Roles in the EC J-PAKE exchange
 */
typedef enum {
    MBEDTLS_ECJPAKE_CLIENT = 0,         /**< Client                         */
    MBEDTLS_ECJPAKE_SERVER,             /**< Server                         */
    MBEDTLS_ECJPAKE_NONE,               /**< Undefined                      */
} mbedtls_ecjpake_role;

#if !defined(MBEDTLS_ECJPAKE_ALT)
/**
 * EC J-PAKE context structure.
 *
 * J-PAKE is a symmetric protocol, except for the identifiers used in
 * Zero-Knowledge Proofs, and the serialization of the second message
 * (KeyExchange) as defined by the Thread spec.
 *
 * In order to benefit from this symmetry, we choose a different naming
 * convention from the Thread v1.0 spec. Correspondence is indicated in the
 * description as a pair C: client name, S: server name
 */
typedef struct mbedtls_ecjpake_context {
    mbedtls_md_type_t MBEDTLS_PRIVATE(md_type);          /**< Hash to use                    */
    mbedtls_ecp_group MBEDTLS_PRIVATE(grp);              /**< Elliptic curve                 */
    mbedtls_ecjpake_role MBEDTLS_PRIVATE(role);          /**< Are we client or server?       */
    int MBEDTLS_PRIVATE(point_format);                   /**< Format for point export        */

    mbedtls_ecp_point MBEDTLS_PRIVATE(Xm1);              /**< My public key 1   C: X1, S: X3 */
    mbedtls_ecp_point MBEDTLS_PRIVATE(Xm2);              /**< My public key 2   C: X2, S: X4 */
    mbedtls_ecp_point MBEDTLS_PRIVATE(Xp1);              /**< Peer public key 1 C: X3, S: X1 */
    mbedtls_ecp_point MBEDTLS_PRIVATE(Xp2);              /**< Peer public key 2 C: X4, S: X2 */
    mbedtls_ecp_point MBEDTLS_PRIVATE(Xp);               /**< Peer public key   C: Xs, S: Xc */

    mbedtls_mpi MBEDTLS_PRIVATE(xm1);                    /**< My private key 1  C: x1, S: x3 */
    mbedtls_mpi MBEDTLS_PRIVATE(xm2);                    /**< My private key 2  C: x2, S: x4 */

    mbedtls_mpi MBEDTLS_PRIVATE(s);                      /**< Pre-shared secret (passphrase) */
} mbedtls_ecjpake_context;

#else  /* MBEDTLS_ECJPAKE_ALT */
#include "ecjpake_alt.h"
#endif /* MBEDTLS_ECJPAKE_ALT */

/**
 * \brief           Initialize an ECJPAKE context.
 *
 * \param ctx       The ECJPAKE context to initialize.
 *                  This must not be \c NULL.
 */
void mbedtls_ecjpake_init(mbedtls_ecjpake_context *ctx);

/**
 * \brief           Set up an ECJPAKE context for use.
 *
 * \note            Currently the only values for hash/curve allowed by the
 *                  standard are #MBEDTLS_MD_SHA256/#MBEDTLS_ECP_DP_SECP256R1.
 *
 * \param ctx       The ECJPAKE context to set up. This must be initialized.
 * \param role      The role of the caller. This must be either
 *                  #MBEDTLS_ECJPAKE_CLIENT or #MBEDTLS_ECJPAKE_SERVER.
 * \param hash      The identifier of the hash function to use,
 *                  for example #MBEDTLS_MD_SHA256.
 * \param curve     The identifier of the elliptic curve to use,
 *                  for example #MBEDTLS_ECP_DP_SECP256R1.
 * \param secret    The pre-shared secret (passphrase). This must be
 *                  a readable not empty buffer of length \p len Bytes. It need
 *                  only be valid for the duration of this call.
 * \param len       The length of the pre-shared secret \p secret.
 *
 * \return          \c 0 if successful.
 * \return          A negative error code on failure.
 */
int mbedtls_ecjpake_setup(mbedtls_ecjpake_context *ctx,
                          mbedtls_ecjpake_role role,
                          mbedtls_md_type_t hash,
                          mbedtls_ecp_group_id curve,
                          const unsigned char *secret,
                          size_t len);

/**
 * \brief               Set the point format for future reads and writes.
 *
 * \param ctx           The ECJPAKE context to configure.
 * \param point_format  The point format to use:
 *                      #MBEDTLS_ECP_PF_UNCOMPRESSED (default)
 *                      or #MBEDTLS_ECP_PF_COMPRESSED.
 *
 * \return              \c 0 if successful.
 * \return              #MBEDTLS_ERR_ECP_BAD_INPUT_DATA if \p point_format
 *                      is invalid.
 */
int mbedtls_ecjpake_set_point_format(mbedtls_ecjpake_context *ctx,
                                     int point_format);

/**
 * \brief           Check if an ECJPAKE context is ready for use.
 *
 * \param ctx       The ECJPAKE context to check. This must be
 *                  initialized.
 *
 * \return          \c 0 if the context is ready for use.
 * \return          #MBEDTLS_ERR_ECP_BAD_INPUT_DATA otherwise.
 */
int mbedtls_ecjpake_check(const mbedtls_ecjpake_context *ctx);

/**
 * \brief           Generate and write the first round message
 *                  (TLS: contents of the Client/ServerHello extension,
 *                  excluding extension type and length bytes).
 *
 * \param ctx       The ECJPAKE context to use. This must be
 *                  initialized and set up.
 * \param buf       The buffer to write the contents to. This must be a
 *                  writable buffer of length \p len Bytes.
 * \param len       The length of \p buf in Bytes.
 * \param olen      The address at which to store the total number
 *                  of Bytes written to \p buf. This must not be \c NULL.
 * \param f_rng     The RNG function to use. This must not be \c NULL.
 * \param p_rng     The RNG parameter to be passed to \p f_rng. This
 *                  may be \c NULL if \p f_rng doesn't use a context.
 *
 * \return          \c 0 if successful.
 * \return          A negative error code on failure.
 */
int mbedtls_ecjpake_write_round_one(mbedtls_ecjpake_context *ctx,
                                    unsigned char *buf, size_t len, size_t *olen,
                                    mbedtls_f_rng_t *f_rng,
                                    void *p_rng);

/**
 * \brief           Read and process the first round message
 *                  (TLS: contents of the Client/ServerHello extension,
 *                  excluding extension type and length bytes).
 *
 * \param ctx       The ECJPAKE context to use. This must be initialized
 *                  and set up.
 * \param buf       The buffer holding the first round message. This must
 *                  be a readable buffer of length \p len Bytes.
 * \param len       The length in Bytes of \p buf.
 *
 * \return          \c 0 if successful.
 * \return          A negative error code on failure.
 */
int mbedtls_ecjpake_read_round_one(mbedtls_ecjpake_context *ctx,
                                   const unsigned char *buf,
                                   size_t len);

/**
 * \brief           Generate and write the second round message
 *                  (TLS: contents of the Client/ServerKeyExchange).
 *
 * \param ctx       The ECJPAKE context to use. This must be initialized,
 *                  set up, and already have performed round one.
 * \param buf       The buffer to write the round two contents to.
 *                  This must be a writable buffer of length \p len Bytes.
 * \param len       The size of \p buf in Bytes.
 * \param olen      The address at which to store the total number of Bytes
 *                  written to \p buf. This must not be \c NULL.
 * \param f_rng     The RNG function to use. This must not be \c NULL.
 * \param p_rng     The RNG parameter to be passed to \p f_rng. This
 *                  may be \c NULL if \p f_rng doesn't use a context.
 *
 * \return          \c 0 if successful.
 * \return          A negative error code on failure.
 */
int mbedtls_ecjpake_write_round_two(mbedtls_ecjpake_context *ctx,
                                    unsigned char *buf, size_t len, size_t *olen,
                                    mbedtls_f_rng_t *f_rng,
                                    void *p_rng);

/**
 * \brief           Read and process the second round message
 *                  (TLS: contents of the Client/ServerKeyExchange).
 *
 * \param ctx       The ECJPAKE context to use. This must be initialized
 *                  and set up and already have performed round one.
 * \param buf       The buffer holding the second round message. This must
 *                  be a readable buffer of length \p len Bytes.
 * \param len       The length in Bytes of \p buf.
 *
 * \return          \c 0 if successful.
 * \return          A negative error code on failure.
 */
int mbedtls_ecjpake_read_round_two(mbedtls_ecjpake_context *ctx,
                                   const unsigned char *buf,
                                   size_t len);

/**
 * \brief           Derive the shared secret
 *                  (TLS: Pre-Master Secret).
 *
 * \param ctx       The ECJPAKE context to use. This must be initialized,
 *                  set up and have performed both round one and two.
 * \param buf       The buffer to write the derived secret to. This must
 *                  be a writable buffer of length \p len Bytes.
 * \param len       The length of \p buf in Bytes.
 * \param olen      The address at which to store the total number of Bytes
 *                  written to \p buf. This must not be \c NULL.
 * \param f_rng     The RNG function to use. This must not be \c NULL.
 * \param p_rng     The RNG parameter to be passed to \p f_rng. This
 *                  may be \c NULL if \p f_rng doesn't use a context.
 *
 * \return          \c 0 if successful.
 * \return          A negative error code on failure.
 */
int mbedtls_ecjpake_derive_secret(mbedtls_ecjpake_context *ctx,
                                  unsigned char *buf, size_t len, size_t *olen,
                                  mbedtls_f_rng_t *f_rng,
                                  void *p_rng);

/**
 * \brief           Write the shared key material to be passed to a Key
 *                  Derivation Function as described in RFC8236.
 *
 * \param ctx       The ECJPAKE context to use. This must be initialized,
 *                  set up and have performed both round one and two.
 * \param buf       The buffer to write the derived secret to. This must
 *                  be a writable buffer of length \p len Bytes.
 * \param len       The length of \p buf in Bytes.
 * \param olen      The address at which to store the total number of bytes
 *                  written to \p buf. This must not be \c NULL.
 * \param f_rng     The RNG function to use. This must not be \c NULL.
 * \param p_rng     The RNG parameter to be passed to \p f_rng. This
 *                  may be \c NULL if \p f_rng doesn't use a context.
 *
 * \return          \c 0 if successful.
 * \return          A negative error code on failure.
 */
int mbedtls_ecjpake_write_shared_key(mbedtls_ecjpake_context *ctx,
                                     unsigned char *buf, size_t len, size_t *olen,
                                     mbedtls_f_rng_t *f_rng,
                                     void *p_rng);

/**
 * \brief           This clears an ECJPAKE context and frees any
 *                  embedded data structure.
 *
 * \param ctx       The ECJPAKE context to free. This may be \c NULL,
 *                  in which case this function does nothing. If it is not
 *                  \c NULL, it must point to an initialized ECJPAKE context.
 */
void mbedtls_ecjpake_free(mbedtls_ecjpake_context *ctx);

#if defined(MBEDTLS_SELF_TEST)

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if a test failed
 */
int mbedtls_ecjpake_self_test(int verbose);

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif


#endif /* ecjpake.h */

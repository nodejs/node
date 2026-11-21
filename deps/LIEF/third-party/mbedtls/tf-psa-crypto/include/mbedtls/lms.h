/**
 * \file lms.h
 *
 * \brief This file provides an API for the LMS post-quantum-safe stateful-hash
          public-key signature scheme as defined in RFC8554 and NIST.SP.200-208.
 *        This implementation currently only supports a single parameter set
 *        MBEDTLS_LMS_SHA256_M32_H10 in order to reduce complexity. This is one
 *        of the signature schemes recommended by the IETF draft SUIT standard
 *        for IOT firmware upgrades (RFC9019).
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_LMS_H
#define MBEDTLS_LMS_H

#include <stdint.h>
#include <stddef.h>

#include "mbedtls/private_access.h"
#include "tf-psa-crypto/build_info.h"
#include "mbedtls/compat-3-crypto.h"

#define MBEDTLS_ERR_LMS_OUT_OF_PRIVATE_KEYS -0x0013 /**< Specified LMS key has utilised all of its private keys */
#define MBEDTLS_ERR_LMS_VERIFY_FAILED    -0x0015 /**< LMS signature verification failed */

/* Currently only defined for SHA256, 32 is the max hash output size */
#define MBEDTLS_LMOTS_N_HASH_LEN_MAX           (32u)
#define MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT_MAX    (34u)
#define MBEDTLS_LMOTS_N_HASH_LEN(type)         ((type) == MBEDTLS_LMOTS_SHA256_N32_W8 ? 32u : 0)
#define MBEDTLS_LMOTS_I_KEY_ID_LEN             (16u)
#define MBEDTLS_LMOTS_Q_LEAF_ID_LEN            (4u)
#define MBEDTLS_LMOTS_TYPE_LEN                 (4u)
#define MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT(type)  ((type) == MBEDTLS_LMOTS_SHA256_N32_W8 ? 34u : 0)
#define MBEDTLS_LMOTS_C_RANDOM_VALUE_LEN(type) (MBEDTLS_LMOTS_N_HASH_LEN(type))

#define MBEDTLS_LMOTS_SIG_LEN(type) (MBEDTLS_LMOTS_TYPE_LEN + \
                                     MBEDTLS_LMOTS_C_RANDOM_VALUE_LEN(type) + \
                                     (MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT(type) * \
                                      MBEDTLS_LMOTS_N_HASH_LEN(type)))


#define MBEDTLS_LMS_TYPE_LEN            (4)
#define MBEDTLS_LMS_H_TREE_HEIGHT(type) ((type) == MBEDTLS_LMS_SHA256_M32_H10 ? 10u : 0)

/* The length of a hash output, Currently only implemented for SHA256.
 * Max is 32 bytes.
 */
#define MBEDTLS_LMS_M_NODE_BYTES(type) ((type) == MBEDTLS_LMS_SHA256_M32_H10 ? 32 : 0)
#define MBEDTLS_LMS_M_NODE_BYTES_MAX 32

#define MBEDTLS_LMS_SIG_LEN(type, otstype) (MBEDTLS_LMOTS_Q_LEAF_ID_LEN + \
                                            MBEDTLS_LMOTS_SIG_LEN(otstype) + \
                                            MBEDTLS_LMS_TYPE_LEN + \
                                            (MBEDTLS_LMS_H_TREE_HEIGHT(type) * \
                                             MBEDTLS_LMS_M_NODE_BYTES(type)))

#define MBEDTLS_LMS_PUBLIC_KEY_LEN(type) (MBEDTLS_LMS_TYPE_LEN + \
                                          MBEDTLS_LMOTS_TYPE_LEN + \
                                          MBEDTLS_LMOTS_I_KEY_ID_LEN + \
                                          MBEDTLS_LMS_M_NODE_BYTES(type))


#ifdef __cplusplus
extern "C" {
#endif

/** The Identifier of the LMS parameter set, as per
 * https://www.iana.org/assignments/leighton-micali-signatures/leighton-micali-signatures.xhtml
 * We are only implementing a subset of the types, particularly H10, for the sake of simplicity.
 */
typedef enum {
    MBEDTLS_LMS_SHA256_M32_H10 = 0x6,
} mbedtls_lms_algorithm_type_t;

/** The Identifier of the LMOTS parameter set, as per
 *  https://www.iana.org/assignments/leighton-micali-signatures/leighton-micali-signatures.xhtml.
 *  We are only implementing a subset of the types, particularly N32_W8, for the sake of simplicity.
 */
typedef enum {
    MBEDTLS_LMOTS_SHA256_N32_W8 = 4
} mbedtls_lmots_algorithm_type_t;

/** LMOTS parameters structure.
 *
 * This contains the metadata associated with an LMOTS key, detailing the
 * algorithm type, the key ID, and the leaf identifier should be key be part of
 * a LMS key.
 */
typedef struct {
    unsigned char MBEDTLS_PRIVATE(I_key_identifier[MBEDTLS_LMOTS_I_KEY_ID_LEN]); /*!< The key
                                                                                    identifier. */
    unsigned char MBEDTLS_PRIVATE(q_leaf_identifier[MBEDTLS_LMOTS_Q_LEAF_ID_LEN]); /*!< Which
                                                                                      leaf of the LMS key this is.
                                                                                      0 if the key is not part of an LMS key. */
    mbedtls_lmots_algorithm_type_t MBEDTLS_PRIVATE(type); /*!< The LM-OTS key type identifier as
                                                               per IANA. Only SHA256_N32_W8 is
                                                               currently supported. */
} mbedtls_lmots_parameters_t;

/** LMOTS public context structure.
 *
 * A LMOTS public key is a hash output, and the applicable parameter set.
 *
 * The context must be initialized before it is used. A public key must either
 * be imported or generated from a private context.
 *
 * \dot
 * digraph lmots_public_t {
 *   UNINITIALIZED -> INIT [label="init"];
 *   HAVE_PUBLIC_KEY -> INIT [label="free"];
 *   INIT -> HAVE_PUBLIC_KEY [label="import_public_key"];
 *   INIT -> HAVE_PUBLIC_KEY [label="calculate_public_key from private key"];
 *   HAVE_PUBLIC_KEY -> HAVE_PUBLIC_KEY [label="export_public_key"];
 * }
 * \enddot
 */
typedef struct {
    mbedtls_lmots_parameters_t MBEDTLS_PRIVATE(params);
    unsigned char MBEDTLS_PRIVATE(public_key)[MBEDTLS_LMOTS_N_HASH_LEN_MAX];
    unsigned char MBEDTLS_PRIVATE(have_public_key); /*!< Whether the context contains a public key.
                                                       Boolean values only. */
} mbedtls_lmots_public_t;

#if defined(MBEDTLS_LMS_PRIVATE)
/** LMOTS private context structure.
 *
 * A LMOTS private key is one hash output for each of digit of the digest +
 * checksum, and the applicable parameter set.
 *
 * The context must be initialized before it is used. A public key must either
 * be imported or generated from a private context.
 *
 * \dot
 * digraph lmots_public_t {
 *   UNINITIALIZED -> INIT [label="init"];
 *   HAVE_PRIVATE_KEY -> INIT [label="free"];
 *   INIT -> HAVE_PRIVATE_KEY [label="generate_private_key"];
 *   HAVE_PRIVATE_KEY -> INIT [label="sign"];
 * }
 * \enddot
 */
typedef struct {
    mbedtls_lmots_parameters_t MBEDTLS_PRIVATE(params);
    unsigned char MBEDTLS_PRIVATE(private_key)[MBEDTLS_LMOTS_P_SIG_DIGIT_COUNT_MAX][
        MBEDTLS_LMOTS_N_HASH_LEN_MAX];
    unsigned char MBEDTLS_PRIVATE(have_private_key); /*!< Whether the context contains a private key.
                                                        Boolean values only. */
} mbedtls_lmots_private_t;
#endif /* defined(MBEDTLS_LMS_PRIVATE) */


/** LMS parameters structure.
 *
 * This contains the metadata associated with an LMS key, detailing the
 * algorithm type, the type of the underlying OTS algorithm, and the key ID.
 */
typedef struct {
    unsigned char MBEDTLS_PRIVATE(I_key_identifier[MBEDTLS_LMOTS_I_KEY_ID_LEN]); /*!< The key
                                                                                    identifier. */
    mbedtls_lmots_algorithm_type_t MBEDTLS_PRIVATE(otstype); /*!< The LM-OTS key type identifier as
                                                                per IANA. Only SHA256_N32_W8 is
                                                                currently supported. */
    mbedtls_lms_algorithm_type_t MBEDTLS_PRIVATE(type); /*!< The LMS key type identifier as per
                                                             IANA. Only SHA256_M32_H10 is currently
                                                             supported. */
} mbedtls_lms_parameters_t;

/** LMS public context structure.
 *
 * A LMS public key is the hash output that is the root of the Merkle tree, and
 * the applicable parameter set
 *
 * The context must be initialized before it is used. A public key must either
 * be imported or generated from a private context.
 *
 * \dot
 * digraph lms_public_t {
 *   UNINITIALIZED -> INIT [label="init"];
 *   HAVE_PUBLIC_KEY -> INIT [label="free"];
 *   INIT -> HAVE_PUBLIC_KEY [label="import_public_key"];
 *   INIT -> HAVE_PUBLIC_KEY [label="calculate_public_key from private key"];
 *   HAVE_PUBLIC_KEY -> HAVE_PUBLIC_KEY [label="export_public_key"];
 * }
 * \enddot
 */
typedef struct {
    mbedtls_lms_parameters_t MBEDTLS_PRIVATE(params);
    unsigned char MBEDTLS_PRIVATE(T_1_pub_key)[MBEDTLS_LMS_M_NODE_BYTES_MAX]; /*!< The public key, in
                                                                                 the form of the Merkle tree root node. */
    unsigned char MBEDTLS_PRIVATE(have_public_key); /*!< Whether the context contains a public key.
                                                       Boolean values only. */
} mbedtls_lms_public_t;


#if defined(MBEDTLS_LMS_PRIVATE)
/** LMS private context structure.
 *
 * A LMS private key is a set of LMOTS private keys, an index to the next usable
 * key, and the applicable parameter set.
 *
 * The context must be initialized before it is used. A public key must either
 * be imported or generated from a private context.
 *
 * \dot
 * digraph lms_public_t {
 *   UNINITIALIZED -> INIT [label="init"];
 *   HAVE_PRIVATE_KEY -> INIT [label="free"];
 *   INIT -> HAVE_PRIVATE_KEY [label="generate_private_key"];
 * }
 * \enddot
 */
typedef struct {
    mbedtls_lms_parameters_t MBEDTLS_PRIVATE(params);
    uint32_t MBEDTLS_PRIVATE(q_next_usable_key); /*!< The index of the next OTS key that has not
                                                      been used. */
    mbedtls_lmots_private_t *MBEDTLS_PRIVATE(ots_private_keys); /*!< The private key material. One OTS key
                                                                   for each leaf node in the Merkle tree. NULL
                                                                   when have_private_key is 0 and non-NULL otherwise.
                                                                   is 2^MBEDTLS_LMS_H_TREE_HEIGHT(type) in length. */
    mbedtls_lmots_public_t *MBEDTLS_PRIVATE(ots_public_keys); /*!< The OTS key public keys, used to
                                                                   build the Merkle tree. NULL
                                                                   when have_private_key is 0 and
                                                                   non-NULL otherwise.
                                                                   Is 2^MBEDTLS_LMS_H_TREE_HEIGHT(type)
                                                                   in length. */
    unsigned char MBEDTLS_PRIVATE(have_private_key); /*!< Whether the context contains a private key.
                                                        Boolean values only. */
} mbedtls_lms_private_t;
#endif /* defined(MBEDTLS_LMS_PRIVATE) */

/**
 * \brief                    This function initializes an LMS public context
 *
 * \param ctx                The uninitialized LMS context that will then be
 *                           initialized.
 */
void mbedtls_lms_public_init(mbedtls_lms_public_t *ctx);

/**
 * \brief                    This function uninitializes an LMS public context
 *
 * \param ctx                The initialized LMS context that will then be
 *                           uninitialized.
 */
void mbedtls_lms_public_free(mbedtls_lms_public_t *ctx);

/**
 * \brief                    This function imports an LMS public key into a
 *                           public LMS context.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized.
 *
 * \note                     See IETF RFC8554 for details of the encoding of
 *                           this public key.
 *
 * \param ctx                The initialized LMS context store the key in.
 * \param key                The buffer from which the key will be read.
 *                           #MBEDTLS_LMS_PUBLIC_KEY_LEN bytes will be read from
 *                           this.
 * \param key_size           The size of the key being imported.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lms_import_public_key(mbedtls_lms_public_t *ctx,
                                  const unsigned char *key, size_t key_size);

/**
 * \brief                    This function exports an LMS public key from a
 *                           LMS public context that already contains a public
 *                           key.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized and the context must contain
 *                           a public key.
 *
 * \note                     See IETF RFC8554 for details of the encoding of
 *                           this public key.
 *
 * \param ctx                The initialized LMS public context that contains
 *                           the public key.
 * \param key                The buffer into which the key will be output. Must
 *                           be at least #MBEDTLS_LMS_PUBLIC_KEY_LEN in size.
 * \param key_size           The size of the key buffer.
 * \param key_len            If not NULL, will be written with the size of the
 *                           key.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lms_export_public_key(const mbedtls_lms_public_t *ctx,
                                  unsigned char *key, size_t key_size,
                                  size_t *key_len);

/**
 * \brief                    This function verifies a LMS signature, using a
 *                           LMS context that contains a public key.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized and must contain a public key
 *                           (either by import or generation).
 *
 * \param ctx                The initialized LMS public context from which the
 *                           public key will be read.
 * \param msg                The buffer from which the message will be read.
 * \param msg_size           The size of the message that will be read.
 * \param sig                The buf from which the signature will be read.
 *                           #MBEDTLS_LMS_SIG_LEN bytes will be read from
 *                           this.
 * \param sig_size           The size of the signature to be verified.
 *
 * \return         \c 0 on successful verification.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lms_verify(const mbedtls_lms_public_t *ctx,
                       const unsigned char *msg, size_t msg_size,
                       const unsigned char *sig, size_t sig_size);

#if defined(MBEDTLS_LMS_PRIVATE)
/**
 * \brief                    This function initializes an LMS private context
 *
 * \param ctx                The uninitialized LMS private context that will
 *                           then be initialized. */
void mbedtls_lms_private_init(mbedtls_lms_private_t *ctx);

/**
 * \brief                    This function uninitializes an LMS private context
 *
 * \param ctx                The initialized LMS private context that will then
 *                           be uninitialized.
 */
void mbedtls_lms_private_free(mbedtls_lms_private_t *ctx);

/**
 * \brief                    This function generates an LMS private key, and
 *                           stores in into an LMS private context.
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
 * \param type               The LMS parameter set identifier.
 * \param otstype            The LMOTS parameter set identifier.
 * \param seed               The seed used to deterministically generate the
 *                           key.
 * \param seed_size          The length of the seed.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lms_generate_private_key(mbedtls_lms_private_t *ctx,
                                     mbedtls_lms_algorithm_type_t type,
                                     mbedtls_lmots_algorithm_type_t otstype,
                                     const unsigned char *seed,
                                     size_t seed_size);

/**
 * \brief                    This function calculates an LMS public key from a
 *                           LMS context that already contains a private key.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized and the context must contain
 *                           a private key.
 *
 * \param ctx                The initialized LMS public context to calculate the key
 *                           from and store it into.
 *
 * \param priv_ctx           The LMS private context to read the private key
 *                           from. This must have been initialized and contain a
 *                           private key.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lms_calculate_public_key(mbedtls_lms_public_t *ctx,
                                     const mbedtls_lms_private_t *priv_ctx);

/**
 * \brief                    This function creates a LMS signature, using a
 *                           LMS context that contains unused private keys.
 *
 * \warning                  This function is **not intended for use in
 *                           production**, due to as-yet unsolved problems with
 *                           handling stateful keys. The API for this function
 *                           may change considerably in future versions.
 *
 * \note                     Before this function is called, the context must
 *                           have been initialized and must contain a private
 *                           key.
 *
 * \note                     Each of the LMOTS private keys inside a LMS private
 *                           key can only be used once. If they are reused, then
 *                           attackers may be able to forge signatures with that
 *                           key. This is all handled transparently, but it is
 *                           important to not perform copy operations on LMS
 *                           contexts that contain private key material.
 *
 * \param ctx                The initialized LMS private context from which the
 *                           private key will be read.
 * \param msg                The buffer from which the message will be read.
 * \param msg_size           The size of the message that will be read.
 * \param sig                The buf into which the signature will be stored.
 *                           Must be at least #MBEDTLS_LMS_SIG_LEN in size.
 * \param sig_size           The size of the buffer the signature will be
 *                           written into.
 * \param sig_len            If not NULL, will be written with the size of the
 *                           signature.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_lms_sign(mbedtls_lms_private_t *ctx,
                     const unsigned char *msg,
                     unsigned int msg_size, unsigned char *sig, size_t sig_size,
                     size_t *sig_len);
#endif /* defined(MBEDTLS_LMS_PRIVATE) */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_LMS_H */

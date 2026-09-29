/**
 * \file pk.h
 *
 * \brief Public Key abstraction layer
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_PK_H
#define MBEDTLS_PK_H
#define MBEDTLS_PK_HAVE_PRIVATE_HEADER

#include "mbedtls/private_access.h"

#include "tf-psa-crypto/build_info.h"
#include "mbedtls/compat-3-crypto.h"

#include "mbedtls/md.h"

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)
#include "psa/crypto.h"
#endif

/** Type mismatch, eg attempt to do ECDSA with an RSA key */
#define MBEDTLS_ERR_PK_TYPE_MISMATCH       -0x3F00
/** Read/write of file failed. */
#define MBEDTLS_ERR_PK_FILE_IO_ERROR       -0x3E00
/** Unsupported key version */
#define MBEDTLS_ERR_PK_KEY_INVALID_VERSION -0x3D80
/** Invalid key tag or value. */
#define MBEDTLS_ERR_PK_KEY_INVALID_FORMAT  -0x3D00
/** Key algorithm is unsupported (only RSA and EC are supported). */
#define MBEDTLS_ERR_PK_UNKNOWN_PK_ALG      -0x3C80
/** Private key password can't be empty. */
#define MBEDTLS_ERR_PK_PASSWORD_REQUIRED   -0x3C00
/** Given private key password does not allow for correct decryption. */
#define MBEDTLS_ERR_PK_PASSWORD_MISMATCH   -0x3B80
/** The pubkey tag or value is invalid (only RSA and EC are supported). */
#define MBEDTLS_ERR_PK_INVALID_PUBKEY      -0x3B00
/** The algorithm tag or value is invalid. */
#define MBEDTLS_ERR_PK_INVALID_ALG         -0x3A80
/** Elliptic curve is unsupported (only NIST curves are supported). */
#define MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE -0x3A00
/** Unavailable feature, e.g. RSA disabled for RSA key. */
#define MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE -0x3980

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MBEDTLS_PK_SIGALG_NONE = 0,
    MBEDTLS_PK_SIGALG_RSA_PKCS1V15, // PSA_ALG_RSA_PKCS1V15_SIGN
    MBEDTLS_PK_SIGALG_RSA_PSS,      // PSA_ALG_RSA_PSS_ANY_SALT
    MBEDTLS_PK_SIGALG_ECDSA,        // MBEDTLS_PK_ALG_ECDSA
} mbedtls_pk_sigalg_t;

/**
 * \brief   Maximum size of a signature made by mbedtls_pk_sign() and other
 *          signature functions.
 */
/* We need to set MBEDTLS_PK_SIGNATURE_MAX_SIZE to the maximum signature
 * size among the supported signature types. Do it by starting at 0,
 * then incrementally increasing to be large enough for each supported
 * signature mechanism.
 *
 * The resulting value can be 0, for example if MBEDTLS_ECDH_C is enabled
 * (which allows the pk module to be included) but neither MBEDTLS_ECDSA_C
 * nor MBEDTLS_RSA_C nor any PSA signature mechanism (PSA).
 */
#define MBEDTLS_PK_SIGNATURE_MAX_SIZE 0

#if defined(MBEDTLS_RSA_C) && \
    MBEDTLS_MPI_MAX_SIZE > MBEDTLS_PK_SIGNATURE_MAX_SIZE
/* For RSA, the signature can be as large as the bignum module allows.*/
#undef MBEDTLS_PK_SIGNATURE_MAX_SIZE
#define MBEDTLS_PK_SIGNATURE_MAX_SIZE MBEDTLS_MPI_MAX_SIZE
#endif

#if defined(MBEDTLS_ECDSA_C) &&                                 \
    MBEDTLS_ECDSA_MAX_LEN > MBEDTLS_PK_SIGNATURE_MAX_SIZE
/* For ECDSA, the ecdsa module exports a constant for the maximum
 * signature size. */
#undef MBEDTLS_PK_SIGNATURE_MAX_SIZE
#define MBEDTLS_PK_SIGNATURE_MAX_SIZE MBEDTLS_ECDSA_MAX_LEN
#endif

#if PSA_SIGNATURE_MAX_SIZE > MBEDTLS_PK_SIGNATURE_MAX_SIZE
/* PSA_SIGNATURE_MAX_SIZE is the maximum size of a signature made
 * through the PSA API in the PSA representation. */
#undef MBEDTLS_PK_SIGNATURE_MAX_SIZE
#define MBEDTLS_PK_SIGNATURE_MAX_SIZE PSA_SIGNATURE_MAX_SIZE
#endif

#if PSA_VENDOR_ECDSA_SIGNATURE_MAX_SIZE + 11 > MBEDTLS_PK_SIGNATURE_MAX_SIZE
/* The Mbed TLS representation is different for ECDSA signatures:
 * PSA uses the raw concatenation of r and s,
 * whereas Mbed TLS uses the ASN.1 representation (SEQUENCE of two INTEGERs).
 * Add the overhead of ASN.1: up to (1+2) + 2 * (1+2+1) for the
 * types, lengths (represented by up to 2 bytes), and potential leading
 * zeros of the INTEGERs and the SEQUENCE. */
#undef MBEDTLS_PK_SIGNATURE_MAX_SIZE
#define MBEDTLS_PK_SIGNATURE_MAX_SIZE (PSA_VENDOR_ECDSA_SIGNATURE_MAX_SIZE + 11)
#endif

/* Keep this symbol for backward compatibility. There is code in the framework
 * which depends on this. Once 3.6 LTS branch will reach end-of-life framework's
 * code can be adjusted and this define removed. */
#define MBEDTLS_PK_USE_PSA_EC_DATA

/* This is identical to MBEDTLS_PK_USE_PSA_EC_DATA above, but for RSA keys.
 * The main reason for having it is that framework code is shared between
 * the develoment branch and the 3.6 LTS one and we need a way to tell from which
 * of the two we're building.
 * This symbol is not used in builtin driver and tests and it can be removed
 * at the same time as MBEDTLS_PK_USE_PSA_EC_DATA. */
#define MBEDTLS_PK_USE_PSA_RSA_DATA

/* Opaque internal type */
typedef struct mbedtls_pk_info_t mbedtls_pk_info_t;

#define MBEDTLS_PK_MAX_EC_PUBKEY_RAW_LEN \
    PSA_KEY_EXPORT_ECC_PUBLIC_KEY_MAX_SIZE(PSA_VENDOR_ECC_MAX_CURVE_BITS)

#define MBEDTLS_PK_MAX_RSA_PUBKEY_RAW_LEN \
    PSA_KEY_EXPORT_RSA_PUBLIC_KEY_MAX_SIZE(PSA_VENDOR_RSA_MAX_KEY_BITS)

#define MBEDTLS_PK_MAX_PUBKEY_RAW_LEN \
    (MBEDTLS_PK_MAX_EC_PUBKEY_RAW_LEN > MBEDTLS_PK_MAX_RSA_PUBKEY_RAW_LEN) ? \
    MBEDTLS_PK_MAX_EC_PUBKEY_RAW_LEN : MBEDTLS_PK_MAX_RSA_PUBKEY_RAW_LEN

typedef enum {
    MBEDTLS_PK_RSA_PKCS_V15 = 0,
    MBEDTLS_PK_RSA_PKCS_V21,
} mbedtls_pk_rsa_padding_t;

/**
 * \brief           Public key container
 */
typedef struct mbedtls_pk_context {
    /* Public key information. */
    const mbedtls_pk_info_t *MBEDTLS_PRIVATE(pk_info);
    /* Previously: underlying public key context.
     * Now unused (by public functions at least). */
    void *MBEDTLS_PRIVATE(pk_ctx);

    /* The following field is used to store the ID of a private key for:
     * - EC keys (MBEDTLS_PK_ECKEY, MBEDTLS_PK_ECKEY_DH, MBEDTLS_PK_ECDSA)
     * - Wrapped keys (EC or RSA).
     *
     * priv_id = MBEDTLS_SVC_KEY_ID_INIT when PK context wraps only the public
     * key.
     *
     * Other keys still use the pk_ctx to store their own context. */
    mbedtls_svc_key_id_t MBEDTLS_PRIVATE(priv_id);

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY) || defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
    /* Public EC or RSA key in raw format, where raw here means the format returned
     * by psa_export_public_key(). */
    uint8_t MBEDTLS_PRIVATE(pub_raw)[MBEDTLS_PK_MAX_PUBKEY_RAW_LEN];

    /* Lenght of the raw key above in bytes. */
    size_t MBEDTLS_PRIVATE(pub_raw_len);

    /* Bits of the private/public key. */
    size_t MBEDTLS_PRIVATE(bits);
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY || PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
    /* EC family. Only applies to EC keys. */
    psa_ecc_family_t MBEDTLS_PRIVATE(ec_family);
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
    /* Padding associated to the RSA key. It only affects RSA public key since
     * the private one is imported into PSA with v1.5 as main algorithm and
     * v2.1 as enrollment algorithm. */
    mbedtls_pk_rsa_padding_t MBEDTLS_PRIVATE(rsa_padding);

    /* Hash algorithm to be used with RSA public key. */
    psa_algorithm_t MBEDTLS_PRIVATE(rsa_hash_alg);
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
} mbedtls_pk_context;

#if defined(MBEDTLS_ECP_RESTARTABLE)
/**
 * \brief           Context for resuming operations
 */
typedef struct {
    const mbedtls_pk_info_t *MBEDTLS_PRIVATE(pk_info);    /* Public key information         */
    void *MBEDTLS_PRIVATE(rs_ctx);                        /* Underlying restart context     */
} mbedtls_pk_restart_ctx;

#else /* MBEDTLS_ECP_RESTARTABLE */
/* Now we can declare functions that take a pointer to that */
typedef void mbedtls_pk_restart_ctx;
#endif /* MBEDTLS_ECP_RESTARTABLE */

/**
 * This helper exposes which ECDSA variant the PK module uses by default:
 * this is deterministic ECDSA if available, or randomized otherwise.
 *
 * \warning This default algorithm selection might change in the future.
 */
#if defined(PSA_WANT_ALG_DETERMINISTIC_ECDSA)
#define MBEDTLS_PK_ALG_ECDSA(hash_alg) PSA_ALG_DETERMINISTIC_ECDSA(hash_alg)
#else
#define MBEDTLS_PK_ALG_ECDSA(hash_alg) PSA_ALG_ECDSA(hash_alg)
#endif

/**
 * \brief           Initialize a #mbedtls_pk_context (as empty).
 *
 *                  After this, you want to populate the context using one of the
 *                  following functions:
 *                  - \c mbedtls_pk_wrap_psa()
 *                  - \c mbedtls_pk_copy_from_psa()
 *                  - \c mbedtls_pk_copy_public_from_psa()
 *                  - \c mbedtls_pk_parse_key()
 *                  - \c mbedtls_pk_parse_public_key()
 *                  - \c mbedtls_pk_parse_keyfile()
 *                  - \c mbedtls_pk_parse_public_keyfile()
 *
 * \param ctx       The context to initialize.
 *                  This must not be \c NULL.
 */
void mbedtls_pk_init(mbedtls_pk_context *ctx);

/**
 * \brief           Empty a #mbedtls_pk_context.
 *                  After this, the context can be re-used as if it had been
 *                  freshly initialized.
 *
 * \param ctx       The context to clear. It must have been initialized.
 *                  If this is \c NULL, this function does nothing.
 *
 * \note            For contexts that have been populated with
 *                  mbedtls_pk_wrap_psa(), this does not free the underlying
 *                  PSA key and you still need to call psa_destroy_key()
 *                  independently if you want to destroy that key.
 */
void mbedtls_pk_free(mbedtls_pk_context *ctx);

#if defined(MBEDTLS_ECP_RESTARTABLE)
/**
 * \brief           Initialize a restart context
 *
 * \param ctx       The context to initialize.
 *                  This must not be \c NULL.
 */
void mbedtls_pk_restart_init(mbedtls_pk_restart_ctx *ctx);

/**
 * \brief           Free the components of a restart context
 *
 * \param ctx       The context to clear. It must have been initialized.
 *                  If this is \c NULL, this function does nothing.
 */
void mbedtls_pk_restart_free(mbedtls_pk_restart_ctx *ctx);
#endif /* MBEDTLS_ECP_RESTARTABLE */

/**
 * \brief Populate a PK context by wrapping a PSA key pair.
 *
 * The PSA key must be an EC or RSA key pair (FFDH is not suported in PK).
 *
 * The resulting context can only perform operations that are allowed by the
 * key's policy. Additionally, it currently has the following limitations:
 * - restartable operations can't be used;
 * - for RSA keys, signature verification is not supported, and neither is use
 *   of \c mbedtls_pk_check_pair().
 *
 * \warning The PSA wrapped key must remain valid as long as the wrapping PK
 *          context is in use, that is at least between the point this function
 *          is called and the point mbedtls_pk_free() is called on this context.
 *
 * \param ctx The context to populate. It must be empty.
 * \param key The PSA key to wrap, which must hold an ECC or RSA key pair.
 *
 * \return    \c 0 on success.
 * \return    #PSA_ERROR_INVALID_ARGUMENT on invalid input (context already
 *            used, invalid key identifier).
 * \return    #MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE if the key is not an ECC or
 *            RSA key pair.
 * \return    #PSA_ERROR_INSUFFICIENT_MEMORY on allocation failure.
 */
int mbedtls_pk_wrap_psa(mbedtls_pk_context *ctx,
                        const mbedtls_svc_key_id_t key);

/**
 * \brief           Get the size in bits of the underlying key
 *
 * \param ctx       The context to query. It must have been populated.
 *
 * \return          Key size in bits, or 0 on error
 */
size_t mbedtls_pk_get_bitlen(const mbedtls_pk_context *ctx);

/**
 * \brief           Tell if the key wrapped in the PK context is able to perform
 *                  the \p usage operation using the \p alg algorithm.
 *
 *                  The operation may be a PK function, a PSA operation on
 *                  the underlying PSA key if the PK object wraps a PSA key,
 *                  or a PSA operation on a key obtained with
 *                  mbedtls_pk_import_into_psa().
 *
 * \note            As of TF-PSA-Crypto 1.0.0, this function returns \c 0
 *                  if the key type and policy are suitable for the
 *                  requested algorithm and usage, even if the key would
 *                  not work for some other reason, for example an RSA
 *                  key that is too small for OAEP with the specified hash.
 *                  This behavior may change without notice in future
 *                  versions of the library.
 *
 * \param pk        The context to query. It must have been populated.
 * \param alg       PSA algorithm to check against.
 *                  Allowed values are:
 *                  - #PSA_ALG_RSA_PKCS1V15_SIGN(hash),
 *                  - #PSA_ALG_RSA_PSS(hash),
 *                  - #PSA_ALG_RSA_PSS_ANY_SALT(hash),
 *                  - #PSA_ALG_RSA_PKCS1V15_CRYPT,
 *                  - #PSA_ALG_RSA_OAEP(hash),
 *                  - #PSA_ALG_ECDSA(hash),
 *                  - #MBEDTLS_PK_ALG_ECDSA(hash),
 *                  where hash is a specified algorithm.
 * \param usage     PSA usage flag that the key must be verified against.
 *                  A single flag from the following list must be specified:
 *                  - #PSA_KEY_USAGE_SIGN_HASH,
 *                  - #PSA_KEY_USAGE_VERIFY_HASH,
 *                  - #PSA_KEY_USAGE_DECRYPT,
 *                  - #PSA_KEY_USAGE_ENCRYPT,
 *                  - #PSA_KEY_USAGE_DERIVE,
 *                  - #PSA_KEY_USAGE_DERIVE_PUBLIC.
 *
 * \return          1 if the key can do operation on the given type.
 * \return          0 if the key cannot do the operations,
 *                  or the context has not been populated.
 */
int mbedtls_pk_can_do_psa(const mbedtls_pk_context *pk, psa_algorithm_t alg,
                          psa_key_usage_t usage);

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)
/**
 * \brief           Determine valid PSA attributes that can be used to
 *                  import a key into PSA.
 *
 * The attributes determined by this function are suitable
 * for calling mbedtls_pk_import_into_psa() to create
 * a PSA key with the same key material.
 *
 * The typical flow of operations involving this function is
 * ```
 * psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
 * int ret = mbedtls_pk_get_psa_attributes(pk, &attributes);
 * if (ret != 0) ...; // error handling omitted
 * // Tweak attributes if desired
 * psa_key_id_t key_id = 0;
 * ret = mbedtls_pk_import_into_psa(pk, &attributes, &key_id);
 * if (ret != 0) ...; // error handling omitted
 * ```
 *
 * \param[in] pk    The PK context to use. It must have been populated.
 *                  It can either contain a key pair or just a public key.
 * \param usage     A single `PSA_KEY_USAGE_xxx` flag among the following:
 *                  - #PSA_KEY_USAGE_DECRYPT: \p pk must contain a
 *                    key pair. The output \p attributes will contain a
 *                    key pair type, and the usage policy will allow
 *                    #PSA_KEY_USAGE_ENCRYPT as well as
 *                    #PSA_KEY_USAGE_DECRYPT.
 *                  - #PSA_KEY_USAGE_DERIVE: \p pk must contain a
 *                    key pair. The output \p attributes will contain a
 *                    key pair type.
 *                  - #PSA_KEY_USAGE_ENCRYPT: The output
 *                    \p attributes will contain a public key type.
 *                  - #PSA_KEY_USAGE_SIGN_HASH: \p pk must contain a
 *                    key pair. The output \p attributes will contain a
 *                    key pair type, and the usage policy will allow
 *                    #PSA_KEY_USAGE_VERIFY_HASH as well as
 *                    #PSA_KEY_USAGE_SIGN_HASH.
 *                  - #PSA_KEY_USAGE_SIGN_MESSAGE: \p pk must contain a
 *                    key pair. The output \p attributes will contain a
 *                    key pair type, and the usage policy will allow
 *                    #PSA_KEY_USAGE_VERIFY_MESSAGE as well as
 *                    #PSA_KEY_USAGE_SIGN_MESSAGE.
 *                  - #PSA_KEY_USAGE_VERIFY_HASH: The output
 *                    \p attributes will contain a public key type.
 *                  - #PSA_KEY_USAGE_VERIFY_MESSAGE: The output
 *                    \p attributes will contain a public key type.
 * \param[out] attributes
 *                  On success, valid attributes to import the key into PSA.
 *                  - The lifetime and key identifier are unchanged. If the
 *                    attribute structure was initialized or reset before
 *                    calling this function, this will result in a volatile
 *                    key. Call psa_set_key_identifier() before or after this
 *                    function if you wish to create a persistent key. Call
 *                    psa_set_key_lifetime() before or after this function if
 *                    you wish to import the key in a secure element.
 *                  - The key type and bit-size are determined by the contents
 *                    of the PK context. If the PK context contains a key
 *                    pair, the key type can be either a key pair type or
 *                    the corresponding public key type, depending on
 *                    \p usage. If the PK context contains a public key,
 *                    the key type is a public key type.
 *                  - The key's policy is determined by the key type and
 *                    the \p usage parameter. The usage always allows
 *                    \p usage, exporting and copying the key, and
 *                    possibly other permissions as documented for the
 *                    \p usage parameter.
 *                    The enrolment algorithm (if available in this build) is
 *                    left unchanged.
 *                    For keys created with \c mbedtls_pk_wrap_psa(), the
 *                    primary algorithm is the same as the original PSA key.
 *                    Otherwise, it is determined as follows:
 *                      - For RSA keys:
 *                        #PSA_ALG_RSA_PKCS1V15_SIGN(#PSA_ALG_ANY_HASH)
 *                        if \p usage is SIGN/VERIFY, and
 *                        #PSA_ALG_RSA_PKCS1V15_CRYPT
 *                        if \p usage is ENCRYPT/DECRYPT.
 *                      - For ECC keys:
 *                        #MBEDTLS_PK_ALG_ECDSA(#PSA_ALG_ANY_HASH)
 *                        if \p usage is SIGN/VERIFY, and
 *                        #PSA_ALG_ECDH if \p usage is DERIVE.
 *
 * \return          0 on success.
 *                  #MBEDTLS_ERR_PK_TYPE_MISMATCH if \p pk does not contain
 *                  a key compatible with the desired \p usage.
 *                  Another error code on other failures.
 */
int mbedtls_pk_get_psa_attributes(const mbedtls_pk_context *pk,
                                  psa_key_usage_t usage,
                                  psa_key_attributes_t *attributes);

/**
 * \brief           Import a key into the PSA key store.
 *
 * This function is equivalent to calling psa_import_key()
 * with the key material from \p pk.
 *
 * The typical way to use this function is:
 * -# Call mbedtls_pk_get_psa_attributes() to obtain
 *    attributes for the given key.
 * -# If desired, modify the attributes, for example:
 *     - To create a persistent key, call
 *       psa_set_key_identifier() and optionally
 *       psa_set_key_lifetime().
 *     - To import only the public part of a key pair:
 *
 *           psa_set_key_type(&attributes,
 *                            PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(
 *                                psa_get_key_type(&attributes)));
 *     - Restrict the key usage if desired.
 * -# Call mbedtls_pk_import_into_psa().
 *
 * \param[in] pk    The PK context to use. It must have been populated.
 *                  It can either contain a key pair or just a public key.
 * \param[in] attributes
 *                  The attributes to use for the new key. They must be
 *                  compatible with \p pk. In particular, the key type
 *                  must match the content of \p pk.
 *                  If \p pk contains a key pair, the key type in
 *                  attributes can be either the key pair type or the
 *                  corresponding public key type (to import only the
 *                  public part).
 * \param[out] key_id
 *                  On success, the identifier of the newly created key.
 *                  On error, this is #MBEDTLS_SVC_KEY_ID_INIT.
 *
 * \return          0 on success.
 *                  #MBEDTLS_ERR_PK_TYPE_MISMATCH if \p pk does not contain
 *                  a key of the type identified in \p attributes.
 *                  Another error code on other failures.
 */
int mbedtls_pk_import_into_psa(const mbedtls_pk_context *pk,
                               const psa_key_attributes_t *attributes,
                               mbedtls_svc_key_id_t *key_id);

/**
 * \brief           Populate a PK context with the key material from a PSA key.
 *
 *                  This key:
 *                  - must be exportable and
 *                  - must be an RSA or EC key pair or public key
 *                    (FFDH is not supported in PK).
 *
 *                  Once this function returns the PK object will be completely
 *                  independent from the original PSA key that it was generated
 *                  from.
 *
 * \note            This function only copies the key material but discards
 *                  policy information entirely. See \c
 *                  mbedtls_pk_get_psa_attributes() for details on which
 *                  algorithm is going to be used by PK for contexts populated with
 *                  this function.
 *
 *                  If you want to retain the PSA policy, see \c
 *                  mbedtls_pk_wrap_psa() - but then the PSA key needs to live
 *                  at least as long as the PK context.
 *
 * \param key_id    The key identifier of the key stored in PSA.
 * \param pk        The PK context to populate. It must be empty.
 *
 * \return          0 on success.
 * \return          #PSA_ERROR_INVALID_ARGUMENT in case the provided input
 *                  parameters are not correct.
 */
int mbedtls_pk_copy_from_psa(mbedtls_svc_key_id_t key_id, mbedtls_pk_context *pk);

/**
 * \brief           Populate a PK context with the public key material of a PSA
 *                  key.
 *
 *                  The key must be an RSA or ECC key. It can be either a
 *                  public key or a key pair, and only the public key is copied.
 *
 *                  Once this function returns the PK object will be completely
 *                  independent from the original PSA key that it was generated
 *                  from.
 *
 * \note            This function only copies the key material but discards
 *                  policy information entirely. See \c
 *                  mbedtls_pk_get_psa_attributes() for details on which
 *                  algorithm is going to be used by PK for contexts populated with
 *                  this function.
 *
 *                  If you want to retain the PSA policy, see \c
 *                  mbedtls_pk_wrap_psa() - but then the PSA key needs to live
 *                  at least as long as the PK context.
 *
 * \param key_id    The key identifier of the key stored in PSA.
 * \param pk        The PK context to populate. It must be empty.
 *
 * \return          0 on success.
 * \return          #PSA_ERROR_INVALID_ARGUMENT in case the provided input
 *                  parameters are not correct.
 */
int mbedtls_pk_copy_public_from_psa(mbedtls_svc_key_id_t key_id, mbedtls_pk_context *pk);
#endif /* MBEDTLS_PSA_CRYPTO_CLIENT */

/**
 * \brief           Verify signature.
 *
 * \note            The signature algorithm used will be the one that would be
 *                  selected by \c mbedtls_pk_get_psa_attributes() called with a
 *                  usage of #PSA_KEY_USAGE_VERIFY_HASH - see that function's
 *                  documentation for details.
 *                  If you want to select a specific signature algorithm, see
 *                  \c mbedtls_pk_verify_ext().
 *
 * \note            This function currently does not work on RSA keys created
 *                  with \c mbedtls_pk_wrap_psa().
 *
 * \param ctx       The PK context to use. It must have been populated.
 * \param md_alg    Hash algorithm used.
 * \param hash      Hash of the message to sign
 * \param hash_len  Hash length
 * \param sig       Signature to verify
 * \param sig_len   Signature length
 *
 * \return          0 on success (signature is valid),
 *                  #PSA_ERROR_INVALID_SIGNATURE if the signature is invalid,
 *                  or another specific error code.
 */
int mbedtls_pk_verify(mbedtls_pk_context *ctx, mbedtls_md_type_t md_alg,
                      const unsigned char *hash, size_t hash_len,
                      const unsigned char *sig, size_t sig_len);

/**
 * \brief           Restartable version of \c mbedtls_pk_verify()
 *
 * \note            Performs the same job as \c mbedtls_pk_verify(), but can
 *                  return early and restart according to the limit set with
 *                  \c psa_interruptible_set_max_ops() to reduce blocking for ECC
 *                  operations. For RSA, same as \c mbedtls_pk_verify().
 *
 * \param ctx       The PK context to use. It must have been populated.
 * \param md_alg    Hash algorithm used
 * \param hash      Hash of the message to sign
 * \param hash_len  Hash length
 * \param sig       Signature to verify
 * \param sig_len   Signature length
 * \param rs_ctx    Restart context (NULL to disable restart)
 *
 * \return          See \c mbedtls_pk_verify(), or
 * \return          #PSA_OPERATION_INCOMPLETE if maximum number of
 *                  operations was reached: see \c psa_interruptible_set_max_ops().
 */
int mbedtls_pk_verify_restartable(mbedtls_pk_context *ctx,
                                  mbedtls_md_type_t md_alg,
                                  const unsigned char *hash, size_t hash_len,
                                  const unsigned char *sig, size_t sig_len,
                                  mbedtls_pk_restart_ctx *rs_ctx);

/**
 * \brief           Verify signature, selecting a specific algorithm.
 *
 * \param type      Signature type to verify
 * \param ctx       The PK context to use. It must have been populated.
 * \param md_alg    Hash algorithm used.
 * \param hash      Hash of the message to sign
 * \param hash_len  Hash length
 * \param sig       Signature to verify
 * \param sig_len   Signature length
 *
 * \note            If \p type is #MBEDTLS_PK_SIGALG_RSA_PSS, then any salt
 *                  length is accepted: #PSA_ALG_RSA_PSS_ANY_SALT is used.
 *
 * \return          0 on success (signature is valid),
 *                  #MBEDTLS_ERR_PK_TYPE_MISMATCH if the PK context can't be
 *                  used for this type of signature,
 *                  #PSA_ERROR_INVALID_SIGNATURE if the signature is invalid,
 *                  or a specific error code.
 *
 */
int mbedtls_pk_verify_ext(mbedtls_pk_sigalg_t type,
                          mbedtls_pk_context *ctx, mbedtls_md_type_t md_alg,
                          const unsigned char *hash, size_t hash_len,
                          const unsigned char *sig, size_t sig_len);

/**
 * \brief           Make signature.
 *
 * \note            The signature algorithm used will be the one that would be
 *                  selected by \c mbedtls_pk_get_psa_attributes() called with a
 *                  usage of #PSA_KEY_USAGE_SIGN_HASH - see that function's
 *                  documentation for details.
 *                  If you want to select a specific signature algorithm, see
 *                  \c mbedtls_pk_sign_ext().
 *
 * \param ctx       The PK context to use. It must have been populated
 *                  with a private key.
 * \param md_alg    Hash algorithm used
 * \param hash      Hash of the message to sign
 * \param hash_len  Hash length
 * \param sig       Place to write the signature.
 *                  It must have enough room for the signature.
 *                  #MBEDTLS_PK_SIGNATURE_MAX_SIZE is always enough.
 *                  You may use a smaller buffer if it is large enough
 *                  given the key type.
 * \param sig_size  The size of the \p sig buffer in bytes.
 * \param sig_len   On successful return,
 *                  the number of bytes written to \p sig.
 *
 * \return          0 on success, or a specific error code.
 *
 */
int mbedtls_pk_sign(mbedtls_pk_context *ctx, mbedtls_md_type_t md_alg,
                    const unsigned char *hash, size_t hash_len,
                    unsigned char *sig, size_t sig_size, size_t *sig_len);

/**
 * \brief           Restartable version of \c mbedtls_pk_sign()
 *
 * \note            Performs the same job as \c mbedtls_pk_sign(), but can
 *                  return early and restart according to the limit set with \c
 *                  psa_interruptible_set_max_ops() to reduce blocking for ECC
 *                  operations. For RSA, same as \c mbedtls_pk_sign().
 *
 * \note            For ECC keys, always uses #MBEDTLS_PK_ALG_ECDSA(hash), where
 *                  hash is the PSA alg identifier corresponding to \p hash.
 *
 * \note            This function currently does not work on ECC keys created
 *                  with \c mbedtls_pk_wrap_psa().
 *
 * \param ctx       The PK context to use. It must have been populated
 *                  with a private key.
 * \param md_alg    Hash algorithm used.
 * \param hash      Hash of the message to sign
 * \param hash_len  Hash length
 * \param sig       Place to write the signature.
 *                  It must have enough room for the signature.
 *                  #MBEDTLS_PK_SIGNATURE_MAX_SIZE is always enough.
 *                  You may use a smaller buffer if it is large enough
 *                  given the key type.
 * \param sig_size  The size of the \p sig buffer in bytes.
 * \param sig_len   On successful return,
 *                  the number of bytes written to \p sig.
 * \param rs_ctx    Restart context (NULL to disable restart)
 *
 * \return          See \c mbedtls_pk_sign().
 * \return          #PSA_OPERATION_INCOMPLETE if the maximum number of
 *                  operations was reached: see \c
 *                  psa_interruptible_set_max_ops().
 */
int mbedtls_pk_sign_restartable(mbedtls_pk_context *ctx,
                                mbedtls_md_type_t md_alg,
                                const unsigned char *hash, size_t hash_len,
                                unsigned char *sig, size_t sig_size, size_t *sig_len,
                                mbedtls_pk_restart_ctx *rs_ctx);

/**
 * \brief           Generate a signature, selecting a specific algorithm.
 *
 * \param sig_type  Signature type to generate.
 * \param ctx       The PK context to use. It must have been populated
 *                  with a private key.
 * \param md_alg    Hash algorithm used
 * \param hash      Hash of the message to sign
 * \param hash_len  Hash length
 * \param sig       Place to write the signature.
 *                  It must have enough room for the signature.
 *                  #MBEDTLS_PK_SIGNATURE_MAX_SIZE is always enough.
 *                  You may use a smaller buffer if it is large enough
 *                  given the key type.
 * \param sig_size  The size of the \p sig buffer in bytes.
 * \param sig_len   On successful return,
 *                  the number of bytes written to \p sig.
 *
 * \return          0 on success,
 *                  #MBEDTLS_ERR_PK_TYPE_MISMATCH if the PK context can't be
 *                  used for this type of signature,
 *                  or a specific error code.
 */
int mbedtls_pk_sign_ext(mbedtls_pk_sigalg_t sig_type,
                        mbedtls_pk_context *ctx,
                        mbedtls_md_type_t md_alg,
                        const unsigned char *hash, size_t hash_len,
                        unsigned char *sig, size_t sig_size, size_t *sig_len);

/**
 * \brief           Check if a public-private pair of keys matches.
 *
 * \note            This function currently does not work on keys created with
 *                  \c mbedtls_pk_wrap_psa().
 *
 * \param pub       Context holding a public key.
 * \param prv       Context holding a private (and public) key.
 *
 * \return          \c 0 on success (keys were checked and match each other).
 * \return          #MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE if the keys could not
 *                  be checked - in that case they may or may not match.
 * \return          #PSA_ERROR_INVALID_ARGUMENT if a context is invalid.
 * \return          Another non-zero value if the keys do not match.
 */
int mbedtls_pk_check_pair(const mbedtls_pk_context *pub,
                          const mbedtls_pk_context *prv);

#if defined(MBEDTLS_PK_PARSE_C)
/** \ingroup pk_module */
/**
 * \brief           Parse a private key in PEM or DER format
 *
 * \param ctx       The PK context to populate. It must be empty.
 * \param key       Input buffer to parse.
 *                  The buffer must contain the input exactly, with no
 *                  extra trailing material. For PEM, the buffer must
 *                  contain a null-terminated string.
 * \param keylen    Size of \b key in bytes.
 *                  For PEM data, this includes the terminating null byte,
 *                  so \p keylen must be equal to `strlen(key) + 1`.
 * \param pwd       Optional password for decryption.
 *                  Pass \c NULL if expecting a non-encrypted key.
 *                  Pass a string of \p pwdlen bytes if expecting an encrypted
 *                  key; a non-encrypted key will also be accepted.
 *                  The empty password is not supported.
 * \param pwdlen    Size of the password in bytes.
 *                  Ignored if \p pwd is \c NULL.
 *
 * \note            If you need a specific key type, check the result with
 *                  \c mbedtls_pk_can_do_psa().
 *
 * \note            The key is also checked for correctness.
 *
 * \return          0 if successful, or a specific PK or PEM error code
 */
int mbedtls_pk_parse_key(mbedtls_pk_context *ctx,
                         const unsigned char *key, size_t keylen,
                         const unsigned char *pwd, size_t pwdlen);

/** \ingroup pk_module */
/**
 * \brief           Parse a public key in PEM or DER format
 *
 * \param ctx       The PK context to populate. It must be empty.
 * \param key       Input buffer to parse.
 *                  The buffer must contain the input exactly, with no
 *                  extra trailing material. For PEM, the buffer must
 *                  contain a null-terminated string.
 * \param keylen    Size of \b key in bytes.
 *                  For PEM data, this includes the terminating null byte,
 *                  so \p keylen must be equal to `strlen(key) + 1`.
 *
 * \note            If you need a specific key type, check the result with
 *                  \c mbedtls_pk_can_do_psa().
 *
 * \note            The key is also checked for correctness.
 *
 * \return          0 if successful, or a specific PK or PEM error code
 */
int mbedtls_pk_parse_public_key(mbedtls_pk_context *ctx,
                                const unsigned char *key, size_t keylen);

#if defined(MBEDTLS_FS_IO)
/** \ingroup pk_module */
/**
 * \brief           Load and parse a private key
 *
 * \param ctx       The PK context to populate. It must be empty.
 * \param path      filename to read the private key from
 * \param password  Optional password to decrypt the file.
 *                  Pass \c NULL if expecting a non-encrypted key.
 *                  Pass a null-terminated string if expecting an encrypted
 *                  key; a non-encrypted key will also be accepted.
 *                  The empty password is not supported.
 *
 * \note            If you need a specific key type, check the result with
 *                  \c mbedtls_pk_can_do_psa().
 *
 * \note            The key is also checked for correctness.
 *
 * \return          0 if successful, or a specific PK or PEM error code
 */
int mbedtls_pk_parse_keyfile(mbedtls_pk_context *ctx,
                             const char *path, const char *password);

/** \ingroup pk_module */
/**
 * \brief           Load and parse a public key
 *
 * \param ctx       The PK context to populate. It must be empty.
 * \param path      filename to read the public key from
 *
 * \note            If you need a specific key type, check the result with
 *                  \c mbedtls_pk_can_do_psa().
 *
 * \note            The key is also checked for correctness.
 *
 * \return          0 if successful, or a specific PK or PEM error code
 */
int mbedtls_pk_parse_public_keyfile(mbedtls_pk_context *ctx, const char *path);
#endif /* MBEDTLS_FS_IO */
#endif /* MBEDTLS_PK_PARSE_C */

#if defined(MBEDTLS_PK_WRITE_C)
/**
 * \brief           Write a private key to a PKCS#1 or SEC1 DER structure
 *                  Note: data is written at the end of the buffer! Use the
 *                        return value to determine where you should start
 *                        using the buffer
 *
 * \param ctx       PK context which must contain a valid private key.
 * \param buf       buffer to write to
 * \param size      size of the buffer
 *
 * \return          length of data written if successful, or a specific
 *                  error code
 */
int mbedtls_pk_write_key_der(const mbedtls_pk_context *ctx, unsigned char *buf, size_t size);

/**
 * \brief           Write a public key to a SubjectPublicKeyInfo DER structure
 *                  Note: data is written at the end of the buffer! Use the
 *                        return value to determine where you should start
 *                        using the buffer
 *
 * \param ctx       PK context which must contain a valid public or private key.
 * \param buf       buffer to write to
 * \param size      size of the buffer
 *
 * \return          length of data written if successful, or a specific
 *                  error code
 */
int mbedtls_pk_write_pubkey_der(const mbedtls_pk_context *ctx, unsigned char *buf, size_t size);

#if defined(MBEDTLS_PEM_WRITE_C)
/**
 * \brief           Write a public key to a PEM string
 *
 * \param ctx       PK context which must contain a valid public or private key.
 * \param buf       Buffer to write to. The output includes a
 *                  terminating null byte.
 * \param size      Size of the buffer in bytes.
 *
 * \return          0 if successful, or a specific error code
 */
int mbedtls_pk_write_pubkey_pem(const mbedtls_pk_context *ctx, unsigned char *buf, size_t size);

/**
 * \brief           Write a private key to a PKCS#1 or SEC1 PEM string
 *
 * \param ctx       PK context which must contain a valid private key.
 * \param buf       Buffer to write to. The output includes a
 *                  terminating null byte.
 * \param size      Size of the buffer in bytes.
 *
 * \return          0 if successful, or a specific error code
 */
int mbedtls_pk_write_key_pem(const mbedtls_pk_context *ctx, unsigned char *buf, size_t size);
#endif /* MBEDTLS_PEM_WRITE_C */
#endif /* MBEDTLS_PK_WRITE_C */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_PK_H */

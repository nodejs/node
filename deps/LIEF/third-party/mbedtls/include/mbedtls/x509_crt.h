/**
 * \file x509_crt.h
 *
 * \brief X.509 certificate parsing and writing
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_X509_CRT_H
#define MBEDTLS_X509_CRT_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/x509.h"
#include "mbedtls/x509_crl.h"
#include "mbedtls/bignum.h"

/**
 * \addtogroup x509_module
 * \{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \name Structures and functions for parsing and writing X.509 certificates
 * \{
 */

/**
 * Container for an X.509 certificate. The certificate may be chained.
 *
 * Some fields of this structure are publicly readable. Do not modify
 * them except via Mbed TLS library functions: the effect of modifying
 * those fields or the data that those fields points to is unspecified.
 */
typedef struct mbedtls_x509_crt {
    int MBEDTLS_PRIVATE(own_buffer);                     /**< Indicates if \c raw is owned
                                                          *   by the structure or not.        */
    mbedtls_x509_buf raw;               /**< The raw certificate data (DER). */
    mbedtls_x509_buf tbs;               /**< The raw certificate body (DER). The part that is To Be Signed. */

    int version;                /**< The X.509 version. (1=v1, 2=v2, 3=v3) */
    mbedtls_x509_buf serial;            /**< Unique id for certificate issued by a specific CA. */
    mbedtls_x509_buf sig_oid;           /**< Signature algorithm, e.g. sha1RSA */

    mbedtls_x509_buf issuer_raw;        /**< The raw issuer data (DER). Used for quick comparison. */
    mbedtls_x509_buf subject_raw;       /**< The raw subject data (DER). Used for quick comparison. */

    mbedtls_x509_name issuer;           /**< The parsed issuer data (named information object). */
    mbedtls_x509_name subject;          /**< The parsed subject data (named information object). */

    mbedtls_x509_time valid_from;       /**< Start time of certificate validity. */
    mbedtls_x509_time valid_to;         /**< End time of certificate validity. */

    mbedtls_x509_buf pk_raw;
    mbedtls_pk_context pk;              /**< Container for the public key context. */

    mbedtls_x509_buf issuer_id;         /**< Optional X.509 v2/v3 issuer unique identifier. */
    mbedtls_x509_buf subject_id;        /**< Optional X.509 v2/v3 subject unique identifier. */
    mbedtls_x509_buf v3_ext;            /**< Optional X.509 v3 extensions.  */
    mbedtls_x509_sequence subject_alt_names; /**< Optional list of raw entries of Subject Alternative Names extension. These can be later parsed by mbedtls_x509_parse_subject_alt_name. */
    mbedtls_x509_buf subject_key_id;    /**< Optional X.509 v3 extension subject key identifier. */
    mbedtls_x509_authority authority_key_id;    /**< Optional X.509 v3 extension authority key identifier. */

    mbedtls_x509_sequence certificate_policies; /**< Optional list of certificate policies (Only anyPolicy is printed and enforced, however the rest of the policies are still listed). */

    int MBEDTLS_PRIVATE(ext_types);              /**< Bit string containing detected and parsed extensions */
    int MBEDTLS_PRIVATE(ca_istrue);              /**< Optional Basic Constraint extension value: 1 if this certificate belongs to a CA, 0 otherwise. */
    int MBEDTLS_PRIVATE(max_pathlen);            /**< Optional Basic Constraint extension value: The maximum path length to the root certificate. Path length is 1 higher than RFC 5280 'meaning', so 1+ */

    unsigned int MBEDTLS_PRIVATE(key_usage);     /**< Optional key usage extension value: See the values in x509.h */

    mbedtls_x509_sequence ext_key_usage; /**< Optional list of extended key usage OIDs. */

    unsigned char MBEDTLS_PRIVATE(ns_cert_type); /**< Optional Netscape certificate type extension value: See the values in x509.h */

    mbedtls_x509_buf MBEDTLS_PRIVATE(sig);               /**< Signature: hash of the tbs part signed with the private key. */
    mbedtls_md_type_t MBEDTLS_PRIVATE(sig_md);           /**< Internal representation of the MD algorithm of the signature algorithm, e.g. MBEDTLS_MD_SHA256 */
    mbedtls_pk_type_t MBEDTLS_PRIVATE(sig_pk);           /**< Internal representation of the Public Key algorithm of the signature algorithm, e.g. MBEDTLS_PK_RSA */
    void *MBEDTLS_PRIVATE(sig_opts);             /**< Signature options to be passed to mbedtls_pk_verify_ext(), e.g. for RSASSA-PSS */

    /** Next certificate in the linked list that constitutes the CA chain.
     * \p NULL indicates the end of the list.
     * Do not modify this field directly. */
    struct mbedtls_x509_crt *next;
}
mbedtls_x509_crt;

/**
 * Build flag from an algorithm/curve identifier (pk, md, ecp)
 * Since 0 is always XXX_NONE, ignore it.
 */
#define MBEDTLS_X509_ID_FLAG(id)   (1 << ((id) - 1))

/**
 * Security profile for certificate verification.
 *
 * All lists are bitfields, built by ORing flags from MBEDTLS_X509_ID_FLAG().
 *
 * The fields of this structure are part of the public API and can be
 * manipulated directly by applications. Future versions of the library may
 * add extra fields or reorder existing fields.
 *
 * You can create custom profiles by starting from a copy of
 * an existing profile, such as mbedtls_x509_crt_profile_default or
 * mbedtls_x509_ctr_profile_none and then tune it to your needs.
 *
 * For example to allow SHA-224 in addition to the default:
 *
 *  mbedtls_x509_crt_profile my_profile = mbedtls_x509_crt_profile_default;
 *  my_profile.allowed_mds |= MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA224 );
 *
 * Or to allow only RSA-3072+ with SHA-256:
 *
 *  mbedtls_x509_crt_profile my_profile = mbedtls_x509_crt_profile_none;
 *  my_profile.allowed_mds = MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA256 );
 *  my_profile.allowed_pks = MBEDTLS_X509_ID_FLAG( MBEDTLS_PK_RSA );
 *  my_profile.rsa_min_bitlen = 3072;
 */
typedef struct mbedtls_x509_crt_profile {
    uint32_t allowed_mds;       /**< MDs for signatures         */
    uint32_t allowed_pks;       /**< PK algs for public keys;
                                 *   this applies to all certificates
                                 *   in the provided chain.     */
    uint32_t allowed_curves;    /**< Elliptic curves for ECDSA  */
    uint32_t rsa_min_bitlen;    /**< Minimum size for RSA keys  */
}
mbedtls_x509_crt_profile;

#define MBEDTLS_X509_CRT_VERSION_1              0
#define MBEDTLS_X509_CRT_VERSION_2              1
#define MBEDTLS_X509_CRT_VERSION_3              2

#define MBEDTLS_X509_RFC5280_MAX_SERIAL_LEN 20
#define MBEDTLS_X509_RFC5280_UTC_TIME_LEN   15

#if !defined(MBEDTLS_X509_MAX_FILE_PATH_LEN)
#define MBEDTLS_X509_MAX_FILE_PATH_LEN 512
#endif

/* This macro unfolds to the concatenation of macro invocations
 * X509_CRT_ERROR_INFO( error code,
 *                             error code as string,
 *                             human readable description )
 * where X509_CRT_ERROR_INFO is defined by the user.
 * See x509_crt.c for an example of how to use this. */
#define MBEDTLS_X509_CRT_ERROR_INFO_LIST                                  \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_EXPIRED,            \
                        "MBEDTLS_X509_BADCERT_EXPIRED",          \
                        "The certificate validity has expired") \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_REVOKED,            \
                        "MBEDTLS_X509_BADCERT_REVOKED",          \
                        "The certificate has been revoked (is on a CRL)") \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_CN_MISMATCH,                  \
                        "MBEDTLS_X509_BADCERT_CN_MISMATCH",                \
                        "The certificate Common Name (CN) does not match with the expected CN") \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_NOT_TRUSTED,                             \
                        "MBEDTLS_X509_BADCERT_NOT_TRUSTED",                           \
                        "The certificate is not correctly signed by the trusted CA") \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCRL_NOT_TRUSTED,                      \
                        "MBEDTLS_X509_BADCRL_NOT_TRUSTED",                    \
                        "The CRL is not correctly signed by the trusted CA") \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCRL_EXPIRED,    \
                        "MBEDTLS_X509_BADCRL_EXPIRED",  \
                        "The CRL is expired")          \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_MISSING,   \
                        "MBEDTLS_X509_BADCERT_MISSING", \
                        "Certificate was missing")     \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_SKIP_VERIFY,         \
                        "MBEDTLS_X509_BADCERT_SKIP_VERIFY",       \
                        "Certificate verification was skipped")  \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_OTHER,                          \
                        "MBEDTLS_X509_BADCERT_OTHER",                        \
                        "Other reason (can be used by verify callback)")    \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_FUTURE,                         \
                        "MBEDTLS_X509_BADCERT_FUTURE",                       \
                        "The certificate validity starts in the future")    \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCRL_FUTURE,     \
                        "MBEDTLS_X509_BADCRL_FUTURE",   \
                        "The CRL is from the future")  \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_KEY_USAGE,                      \
                        "MBEDTLS_X509_BADCERT_KEY_USAGE",                    \
                        "Usage does not match the keyUsage extension")      \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_EXT_KEY_USAGE,                       \
                        "MBEDTLS_X509_BADCERT_EXT_KEY_USAGE",                     \
                        "Usage does not match the extendedKeyUsage extension")   \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_NS_CERT_TYPE,                        \
                        "MBEDTLS_X509_BADCERT_NS_CERT_TYPE",                      \
                        "Usage does not match the nsCertType extension")         \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_BAD_MD,                              \
                        "MBEDTLS_X509_BADCERT_BAD_MD",                            \
                        "The certificate is signed with an unacceptable hash.")  \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_BAD_PK,                                                  \
                        "MBEDTLS_X509_BADCERT_BAD_PK",                                                \
                        "The certificate is signed with an unacceptable PK alg (eg RSA vs ECDSA).")  \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCERT_BAD_KEY,                                                            \
                        "MBEDTLS_X509_BADCERT_BAD_KEY",                                                          \
                        "The certificate is signed with an unacceptable key (eg bad curve, RSA too short).")    \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCRL_BAD_MD,                          \
                        "MBEDTLS_X509_BADCRL_BAD_MD",                        \
                        "The CRL is signed with an unacceptable hash.")     \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCRL_BAD_PK,                                            \
                        "MBEDTLS_X509_BADCRL_BAD_PK",                                          \
                        "The CRL is signed with an unacceptable PK alg (eg RSA vs ECDSA).")   \
    X509_CRT_ERROR_INFO(MBEDTLS_X509_BADCRL_BAD_KEY,                                                    \
                        "MBEDTLS_X509_BADCRL_BAD_KEY",                                                  \
                        "The CRL is signed with an unacceptable key (eg bad curve, RSA too short).")

/**
 * Container for writing a certificate (CRT)
 */
typedef struct mbedtls_x509write_cert {
    int MBEDTLS_PRIVATE(version);
    unsigned char MBEDTLS_PRIVATE(serial)[MBEDTLS_X509_RFC5280_MAX_SERIAL_LEN];
    size_t MBEDTLS_PRIVATE(serial_len);
    mbedtls_pk_context *MBEDTLS_PRIVATE(subject_key);
    mbedtls_pk_context *MBEDTLS_PRIVATE(issuer_key);
    mbedtls_asn1_named_data *MBEDTLS_PRIVATE(subject);
    mbedtls_asn1_named_data *MBEDTLS_PRIVATE(issuer);
    mbedtls_md_type_t MBEDTLS_PRIVATE(md_alg);
    char MBEDTLS_PRIVATE(not_before)[MBEDTLS_X509_RFC5280_UTC_TIME_LEN + 1];
    char MBEDTLS_PRIVATE(not_after)[MBEDTLS_X509_RFC5280_UTC_TIME_LEN + 1];
    mbedtls_asn1_named_data *MBEDTLS_PRIVATE(extensions);
}
mbedtls_x509write_cert;

/**
 * \brief           Set Subject Alternative Name
 *
 * \param ctx       Certificate context to use
 * \param san_list  List of SAN values
 *
 * \return          0 if successful, or MBEDTLS_ERR_X509_ALLOC_FAILED
 *
 * \note            "dnsName", "uniformResourceIdentifier", "IP address",
 *                  "otherName", and "DirectoryName", as defined in RFC 5280,
 *                  are supported.
 */
int mbedtls_x509write_crt_set_subject_alternative_name(mbedtls_x509write_cert *ctx,
                                                       const mbedtls_x509_san_list *san_list);

/**
 * Item in a verification chain: cert and flags for it
 */
typedef struct {
    mbedtls_x509_crt *MBEDTLS_PRIVATE(crt);
    uint32_t MBEDTLS_PRIVATE(flags);
} mbedtls_x509_crt_verify_chain_item;

/**
 * Max size of verification chain: end-entity + intermediates + trusted root
 */
#define MBEDTLS_X509_MAX_VERIFY_CHAIN_SIZE  (MBEDTLS_X509_MAX_INTERMEDIATE_CA + 2)

/**
 * Verification chain as built by \c mbedtls_crt_verify_chain()
 */
typedef struct {
    mbedtls_x509_crt_verify_chain_item MBEDTLS_PRIVATE(items)[MBEDTLS_X509_MAX_VERIFY_CHAIN_SIZE];
    unsigned MBEDTLS_PRIVATE(len);

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
    /* This stores the list of potential trusted signers obtained from
     * the CA callback used for the CRT verification, if configured.
     * We must track it somewhere because the callback passes its
     * ownership to the caller. */
    mbedtls_x509_crt *MBEDTLS_PRIVATE(trust_ca_cb_result);
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */
} mbedtls_x509_crt_verify_chain;

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)

/**
 * \brief       Context for resuming X.509 verify operations
 */
typedef struct {
    /* for check_signature() */
    mbedtls_pk_restart_ctx MBEDTLS_PRIVATE(pk);

    /* for find_parent_in() */
    mbedtls_x509_crt *MBEDTLS_PRIVATE(parent); /* non-null iff parent_in in progress */
    mbedtls_x509_crt *MBEDTLS_PRIVATE(fallback_parent);
    int MBEDTLS_PRIVATE(fallback_signature_is_good);

    /* for find_parent() */
    int MBEDTLS_PRIVATE(parent_is_trusted); /* -1 if find_parent is not in progress */

    /* for verify_chain() */
    enum {
        x509_crt_rs_none,
        x509_crt_rs_find_parent,
    } MBEDTLS_PRIVATE(in_progress);  /* none if no operation is in progress */
    int MBEDTLS_PRIVATE(self_cnt);
    mbedtls_x509_crt_verify_chain MBEDTLS_PRIVATE(ver_chain);

} mbedtls_x509_crt_restart_ctx;

#else /* MBEDTLS_ECDSA_C && MBEDTLS_ECP_RESTARTABLE */

/* Now we can declare functions that take a pointer to that */
typedef void mbedtls_x509_crt_restart_ctx;

#endif /* MBEDTLS_ECDSA_C && MBEDTLS_ECP_RESTARTABLE */

#if defined(MBEDTLS_X509_CRT_PARSE_C)
/**
 * Default security profile. Should provide a good balance between security
 * and compatibility with current deployments.
 *
 * This profile permits:
 * - SHA2 hashes with at least 256 bits: SHA-256, SHA-384, SHA-512.
 * - Elliptic curves with 255 bits and above except secp256k1.
 * - RSA with 2048 bits and above.
 *
 * New minor versions of Mbed TLS may extend this profile, for example if
 * new algorithms are added to the library. New minor versions of Mbed TLS will
 * not reduce this profile unless serious security concerns require it.
 */
extern const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_default;

/**
 * Expected next default profile. Recommended for new deployments.
 * Currently targets a 128-bit security level, except for allowing RSA-2048.
 * This profile may change at any time.
 */
extern const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_next;

/**
 * NSA Suite B profile.
 */
extern const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_suiteb;

/**
 * Empty profile that allows nothing. Useful as a basis for constructing
 * custom profiles.
 */
extern const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_none;

/**
 * \brief          Parse a single DER formatted certificate and add it
 *                 to the end of the provided chained list.
 *
 * \note           If #MBEDTLS_USE_PSA_CRYPTO is enabled, the PSA crypto
 *                 subsystem must have been initialized by calling
 *                 psa_crypto_init() before calling this function.
 *
 * \param chain    The pointer to the start of the CRT chain to attach to.
 *                 When parsing the first CRT in a chain, this should point
 *                 to an instance of ::mbedtls_x509_crt initialized through
 *                 mbedtls_x509_crt_init().
 * \param buf      The buffer holding the DER encoded certificate.
 * \param buflen   The size in Bytes of \p buf.
 *
 * \note           This function makes an internal copy of the CRT buffer
 *                 \p buf. In particular, \p buf may be destroyed or reused
 *                 after this call returns. To avoid duplicating the CRT
 *                 buffer (at the cost of stricter lifetime constraints),
 *                 use mbedtls_x509_crt_parse_der_nocopy() instead.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_x509_crt_parse_der(mbedtls_x509_crt *chain,
                               const unsigned char *buf,
                               size_t buflen);

/**
 * \brief          The type of certificate extension callbacks.
 *
 *                 Callbacks of this type are passed to and used by the
 *                 mbedtls_x509_crt_parse_der_with_ext_cb() routine when
 *                 it encounters either an unsupported extension or a
 *                 "certificate policies" extension containing any
 *                 unsupported certificate policies.
 *                 Future versions of the library may invoke the callback
 *                 in other cases, if and when the need arises.
 *
 * \param p_ctx    An opaque context passed to the callback.
 * \param crt      The certificate being parsed.
 * \param oid      The OID of the extension.
 * \param critical Whether the extension is critical.
 * \param p        Pointer to the start of the extension value
 *                 (the content of the OCTET STRING).
 * \param end      End of extension value.
 *
 * \note           The callback must fail and return a negative error code
 *                 if it can not parse or does not support the extension.
 *                 When the callback fails to parse a critical extension
 *                 mbedtls_x509_crt_parse_der_with_ext_cb() also fails.
 *                 When the callback fails to parse a non critical extension
 *                 mbedtls_x509_crt_parse_der_with_ext_cb() simply skips
 *                 the extension and continues parsing.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
typedef int (*mbedtls_x509_crt_ext_cb_t)(void *p_ctx,
                                         mbedtls_x509_crt const *crt,
                                         mbedtls_x509_buf const *oid,
                                         int critical,
                                         const unsigned char *p,
                                         const unsigned char *end);

/**
 * \brief            Parse a single DER formatted certificate and add it
 *                   to the end of the provided chained list.
 *
 * \note             If #MBEDTLS_USE_PSA_CRYPTO is enabled, the PSA crypto
 *                   subsystem must have been initialized by calling
 *                   psa_crypto_init() before calling this function.
 *
 * \param chain      The pointer to the start of the CRT chain to attach to.
 *                   When parsing the first CRT in a chain, this should point
 *                   to an instance of ::mbedtls_x509_crt initialized through
 *                   mbedtls_x509_crt_init().
 * \param buf        The buffer holding the DER encoded certificate.
 * \param buflen     The size in Bytes of \p buf.
 * \param make_copy  When not zero this function makes an internal copy of the
 *                   CRT buffer \p buf. In particular, \p buf may be destroyed
 *                   or reused after this call returns.
 *                   When zero this function avoids duplicating the CRT buffer
 *                   by taking temporary ownership thereof until the CRT
 *                   is destroyed (like mbedtls_x509_crt_parse_der_nocopy())
 * \param cb         A callback invoked for every unsupported certificate
 *                   extension.
 * \param p_ctx      An opaque context passed to the callback.
 *
 * \note             This call is functionally equivalent to
 *                   mbedtls_x509_crt_parse_der(), and/or
 *                   mbedtls_x509_crt_parse_der_nocopy()
 *                   but it calls the callback with every unsupported
 *                   certificate extension and additionally the
 *                   "certificate policies" extension if it contains any
 *                   unsupported certificate policies.
 *                   The callback must return a negative error code if it
 *                   does not know how to handle such an extension.
 *                   When the callback fails to parse a critical extension
 *                   mbedtls_x509_crt_parse_der_with_ext_cb() also fails.
 *                   When the callback fails to parse a non critical extension
 *                   mbedtls_x509_crt_parse_der_with_ext_cb() simply skips
 *                   the extension and continues parsing.
 *                   Future versions of the library may invoke the callback
 *                   in other cases, if and when the need arises.
 *
 * \return           \c 0 if successful.
 * \return           A negative error code on failure.
 */
int mbedtls_x509_crt_parse_der_with_ext_cb(mbedtls_x509_crt *chain,
                                           const unsigned char *buf,
                                           size_t buflen,
                                           int make_copy,
                                           mbedtls_x509_crt_ext_cb_t cb,
                                           void *p_ctx);

/**
 * \brief          Parse a single DER formatted certificate and add it
 *                 to the end of the provided chained list. This is a
 *                 variant of mbedtls_x509_crt_parse_der() which takes
 *                 temporary ownership of the CRT buffer until the CRT
 *                 is destroyed.
 *
 * \note           If #MBEDTLS_USE_PSA_CRYPTO is enabled, the PSA crypto
 *                 subsystem must have been initialized by calling
 *                 psa_crypto_init() before calling this function.
 *
 * \param chain    The pointer to the start of the CRT chain to attach to.
 *                 When parsing the first CRT in a chain, this should point
 *                 to an instance of ::mbedtls_x509_crt initialized through
 *                 mbedtls_x509_crt_init().
 * \param buf      The address of the readable buffer holding the DER encoded
 *                 certificate to use. On success, this buffer must be
 *                 retained and not be changed for the lifetime of the
 *                 CRT chain \p chain, that is, until \p chain is destroyed
 *                 through a call to mbedtls_x509_crt_free().
 * \param buflen   The size in Bytes of \p buf.
 *
 * \note           This call is functionally equivalent to
 *                 mbedtls_x509_crt_parse_der(), but it avoids creating a
 *                 copy of the input buffer at the cost of stronger lifetime
 *                 constraints. This is useful in constrained environments
 *                 where duplication of the CRT cannot be tolerated.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_x509_crt_parse_der_nocopy(mbedtls_x509_crt *chain,
                                      const unsigned char *buf,
                                      size_t buflen);

/**
 * \brief          Parse one DER-encoded or one or more concatenated PEM-encoded
 *                 certificates and add them to the chained list.
 *
 *                 For CRTs in PEM encoding, the function parses permissively:
 *                 if at least one certificate can be parsed, the function
 *                 returns the number of certificates for which parsing failed
 *                 (hence \c 0 if all certificates were parsed successfully).
 *                 If no certificate could be parsed, the function returns
 *                 the first (negative) error encountered during parsing.
 *
 *                 PEM encoded certificates may be interleaved by other data
 *                 such as human readable descriptions of their content, as
 *                 long as the certificates are enclosed in the PEM specific
 *                 '-----{BEGIN/END} CERTIFICATE-----' delimiters.
 *
 * \note           If #MBEDTLS_USE_PSA_CRYPTO is enabled, the PSA crypto
 *                 subsystem must have been initialized by calling
 *                 psa_crypto_init() before calling this function.
 *
 * \param chain    The chain to which to add the parsed certificates.
 * \param buf      The buffer holding the certificate data in PEM or DER format.
 *                 For certificates in PEM encoding, this may be a concatenation
 *                 of multiple certificates; for DER encoding, the buffer must
 *                 comprise exactly one certificate.
 * \param buflen   The size of \p buf, including the terminating \c NULL byte
 *                 in case of PEM encoded data.
 *
 * \return         \c 0 if all certificates were parsed successfully.
 * \return         The (positive) number of certificates that couldn't
 *                 be parsed if parsing was partly successful (see above).
 * \return         A negative X509 or PEM error code otherwise.
 *
 */
int mbedtls_x509_crt_parse(mbedtls_x509_crt *chain, const unsigned char *buf, size_t buflen);

#if defined(MBEDTLS_FS_IO)
/**
 * \brief          Load one or more certificates and add them
 *                 to the chained list. Parses permissively. If some
 *                 certificates can be parsed, the result is the number
 *                 of failed certificates it encountered. If none complete
 *                 correctly, the first error is returned.
 *
 * \note           If #MBEDTLS_USE_PSA_CRYPTO is enabled, the PSA crypto
 *                 subsystem must have been initialized by calling
 *                 psa_crypto_init() before calling this function.
 *
 * \param chain    points to the start of the chain
 * \param path     filename to read the certificates from
 *
 * \return         0 if all certificates parsed successfully, a positive number
 *                 if partly successful or a specific X509 or PEM error code
 */
int mbedtls_x509_crt_parse_file(mbedtls_x509_crt *chain, const char *path);

/**
 * \brief          Load one or more certificate files from a path and add them
 *                 to the chained list. Parses permissively. If some
 *                 certificates can be parsed, the result is the number
 *                 of failed certificates it encountered. If none complete
 *                 correctly, the first error is returned.
 *
 * \param chain    points to the start of the chain
 * \param path     directory / folder to read the certificate files from
 *
 * \return         0 if all certificates parsed successfully, a positive number
 *                 if partly successful or a specific X509 or PEM error code
 */
int mbedtls_x509_crt_parse_path(mbedtls_x509_crt *chain, const char *path);

#endif /* MBEDTLS_FS_IO */

#if !defined(MBEDTLS_X509_REMOVE_INFO)
/**
 * \brief          Returns an informational string about the
 *                 certificate.
 *
 * \param buf      Buffer to write to
 * \param size     Maximum size of buffer
 * \param prefix   A line prefix
 * \param crt      The X509 certificate to represent
 *
 * \return         The length of the string written (not including the
 *                 terminated nul byte), or a negative error code.
 */
int mbedtls_x509_crt_info(char *buf, size_t size, const char *prefix,
                          const mbedtls_x509_crt *crt);

/**
 * \brief          Returns an informational string about the
 *                 verification status of a certificate.
 *
 * \param buf      Buffer to write to
 * \param size     Maximum size of buffer
 * \param prefix   A line prefix
 * \param flags    Verification flags created by mbedtls_x509_crt_verify()
 *
 * \return         The length of the string written (not including the
 *                 terminated nul byte), or a negative error code.
 */
int mbedtls_x509_crt_verify_info(char *buf, size_t size, const char *prefix,
                                 uint32_t flags);
#endif /* !MBEDTLS_X509_REMOVE_INFO */

/**
 * \brief          Verify a chain of certificates.
 *
 *                 The verify callback is a user-supplied callback that
 *                 can clear / modify / add flags for a certificate. If set,
 *                 the verification callback is called for each
 *                 certificate in the chain (from the trust-ca down to the
 *                 presented crt). The parameters for the callback are:
 *                 (void *parameter, mbedtls_x509_crt *crt, int certificate_depth,
 *                 int *flags). With the flags representing current flags for
 *                 that specific certificate and the certificate depth from
 *                 the bottom (Peer cert depth = 0).
 *
 *                 All flags left after returning from the callback
 *                 are also returned to the application. The function should
 *                 return 0 for anything (including invalid certificates)
 *                 other than fatal error, as a non-zero return code
 *                 immediately aborts the verification process. For fatal
 *                 errors, a specific error code should be used (different
 *                 from MBEDTLS_ERR_X509_CERT_VERIFY_FAILED which should not
 *                 be returned at this point), or MBEDTLS_ERR_X509_FATAL_ERROR
 *                 can be used if no better code is available.
 *
 * \note           In case verification failed, the results can be displayed
 *                 using \c mbedtls_x509_crt_verify_info()
 *
 * \note           Same as \c mbedtls_x509_crt_verify_with_profile() with the
 *                 default security profile.
 *
 * \note           It is your responsibility to provide up-to-date CRLs for
 *                 all trusted CAs. If no CRL is provided for the CA that was
 *                 used to sign the certificate, CRL verification is skipped
 *                 silently, that is *without* setting any flag.
 *
 * \note           The \c trust_ca list can contain two types of certificates:
 *                 (1) those of trusted root CAs, so that certificates
 *                 chaining up to those CAs will be trusted, and (2)
 *                 self-signed end-entity certificates to be trusted (for
 *                 specific peers you know) - in that case, the self-signed
 *                 certificate doesn't need to have the CA bit set.
 *
 * \param crt      The certificate chain to be verified.
 * \param trust_ca The list of trusted CAs.
 * \param ca_crl   The list of CRLs for trusted CAs.
 * \param cn       The expected Common Name. This will be checked to be
 *                 present in the certificate's subjectAltNames extension or,
 *                 if this extension is absent, as a CN component in its
 *                 Subject name. DNS names and IP addresses are fully
 *                 supported, while the URI subtype is partially supported:
 *                 only exact matching, without any normalization procedures
 *                 described in 7.4 of RFC5280, will result in a positive
 *                 URI verification.
 *                 This may be \c NULL if the CN need not be verified.
 * \param flags    The address at which to store the result of the verification.
 *                 If the verification couldn't be completed, the flag value is
 *                 set to (uint32_t) -1.
 * \param f_vrfy   The verification callback to use. See the documentation
 *                 of mbedtls_x509_crt_verify() for more information.
 * \param p_vrfy   The context to be passed to \p f_vrfy.
 *
 * \return         \c 0 if the chain is valid with respect to the
 *                 passed CN, CAs, CRLs and security profile.
 * \return         #MBEDTLS_ERR_X509_CERT_VERIFY_FAILED in case the
 *                 certificate chain verification failed. In this case,
 *                 \c *flags will have one or more
 *                 \c MBEDTLS_X509_BADCERT_XXX or \c MBEDTLS_X509_BADCRL_XXX
 *                 flags set.
 * \return         Another negative error code in case of a fatal error
 *                 encountered during the verification process.
 */
int mbedtls_x509_crt_verify(mbedtls_x509_crt *crt,
                            mbedtls_x509_crt *trust_ca,
                            mbedtls_x509_crl *ca_crl,
                            const char *cn, uint32_t *flags,
                            int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
                            void *p_vrfy);

/**
 * \brief          Verify a chain of certificates with respect to
 *                 a configurable security profile.
 *
 * \note           Same as \c mbedtls_x509_crt_verify(), but with explicit
 *                 security profile.
 *
 * \note           The restrictions on keys (RSA minimum size, allowed curves
 *                 for ECDSA) apply to all certificates: trusted root,
 *                 intermediate CAs if any, and end entity certificate.
 *
 * \param crt      The certificate chain to be verified.
 * \param trust_ca The list of trusted CAs.
 * \param ca_crl   The list of CRLs for trusted CAs.
 * \param profile  The security profile to use for the verification.
 * \param cn       The expected Common Name. This may be \c NULL if the
 *                 CN need not be verified.
 * \param flags    The address at which to store the result of the verification.
 *                 If the verification couldn't be completed, the flag value is
 *                 set to (uint32_t) -1.
 * \param f_vrfy   The verification callback to use. See the documentation
 *                 of mbedtls_x509_crt_verify() for more information.
 * \param p_vrfy   The context to be passed to \p f_vrfy.
 *
 * \return         \c 0 if the chain is valid with respect to the
 *                 passed CN, CAs, CRLs and security profile.
 * \return         #MBEDTLS_ERR_X509_CERT_VERIFY_FAILED in case the
 *                 certificate chain verification failed. In this case,
 *                 \c *flags will have one or more
 *                 \c MBEDTLS_X509_BADCERT_XXX or \c MBEDTLS_X509_BADCRL_XXX
 *                 flags set.
 * \return         Another negative error code in case of a fatal error
 *                 encountered during the verification process.
 */
int mbedtls_x509_crt_verify_with_profile(mbedtls_x509_crt *crt,
                                         mbedtls_x509_crt *trust_ca,
                                         mbedtls_x509_crl *ca_crl,
                                         const mbedtls_x509_crt_profile *profile,
                                         const char *cn, uint32_t *flags,
                                         int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
                                         void *p_vrfy);

/**
 * \brief          Restartable version of \c mbedtls_crt_verify_with_profile()
 *
 * \note           Performs the same job as \c mbedtls_crt_verify_with_profile()
 *                 but can return early and restart according to the limit
 *                 set with \c mbedtls_ecp_set_max_ops() to reduce blocking.
 *
 * \param crt      The certificate chain to be verified.
 * \param trust_ca The list of trusted CAs.
 * \param ca_crl   The list of CRLs for trusted CAs.
 * \param profile  The security profile to use for the verification.
 * \param cn       The expected Common Name. This may be \c NULL if the
 *                 CN need not be verified.
 * \param flags    The address at which to store the result of the verification.
 *                 If the verification couldn't be completed, the flag value is
 *                 set to (uint32_t) -1.
 * \param f_vrfy   The verification callback to use. See the documentation
 *                 of mbedtls_x509_crt_verify() for more information.
 * \param p_vrfy   The context to be passed to \p f_vrfy.
 * \param rs_ctx   The restart context to use. This may be set to \c NULL
 *                 to disable restartable ECC.
 *
 * \return         See \c mbedtls_crt_verify_with_profile(), or
 * \return         #MBEDTLS_ERR_ECP_IN_PROGRESS if maximum number of
 *                 operations was reached: see \c mbedtls_ecp_set_max_ops().
 */
int mbedtls_x509_crt_verify_restartable(mbedtls_x509_crt *crt,
                                        mbedtls_x509_crt *trust_ca,
                                        mbedtls_x509_crl *ca_crl,
                                        const mbedtls_x509_crt_profile *profile,
                                        const char *cn, uint32_t *flags,
                                        int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
                                        void *p_vrfy,
                                        mbedtls_x509_crt_restart_ctx *rs_ctx);

/**
 * \brief               The type of trusted certificate callbacks.
 *
 *                      Callbacks of this type are passed to and used by the CRT
 *                      verification routine mbedtls_x509_crt_verify_with_ca_cb()
 *                      when looking for trusted signers of a given certificate.
 *
 *                      On success, the callback returns a list of trusted
 *                      certificates to be considered as potential signers
 *                      for the input certificate.
 *
 * \param p_ctx         An opaque context passed to the callback.
 * \param child         The certificate for which to search a potential signer.
 *                      This will point to a readable certificate.
 * \param candidate_cas The address at which to store the address of the first
 *                      entry in the generated linked list of candidate signers.
 *                      This will not be \c NULL.
 *
 * \note                The callback must only return a non-zero value on a
 *                      fatal error. If, in contrast, the search for a potential
 *                      signer completes without a single candidate, the
 *                      callback must return \c 0 and set \c *candidate_cas
 *                      to \c NULL.
 *
 * \return              \c 0 on success. In this case, \c *candidate_cas points
 *                      to a heap-allocated linked list of instances of
 *                      ::mbedtls_x509_crt, and ownership of this list is passed
 *                      to the caller.
 * \return              A negative error code on failure.
 */
typedef int (*mbedtls_x509_crt_ca_cb_t)(void *p_ctx,
                                        mbedtls_x509_crt const *child,
                                        mbedtls_x509_crt **candidate_cas);

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
/**
 * \brief          Version of \c mbedtls_x509_crt_verify_with_profile() which
 *                 uses a callback to acquire the list of trusted CA
 *                 certificates.
 *
 * \param crt      The certificate chain to be verified.
 * \param f_ca_cb  The callback to be used to query for potential signers
 *                 of a given child certificate. See the documentation of
 *                 ::mbedtls_x509_crt_ca_cb_t for more information.
 * \param p_ca_cb  The opaque context to be passed to \p f_ca_cb.
 * \param profile  The security profile for the verification.
 * \param cn       The expected Common Name. This may be \c NULL if the
 *                 CN need not be verified.
 * \param flags    The address at which to store the result of the verification.
 *                 If the verification couldn't be completed, the flag value is
 *                 set to (uint32_t) -1.
 * \param f_vrfy   The verification callback to use. See the documentation
 *                 of mbedtls_x509_crt_verify() for more information.
 * \param p_vrfy   The context to be passed to \p f_vrfy.
 *
 * \return         See \c mbedtls_crt_verify_with_profile().
 */
int mbedtls_x509_crt_verify_with_ca_cb(mbedtls_x509_crt *crt,
                                       mbedtls_x509_crt_ca_cb_t f_ca_cb,
                                       void *p_ca_cb,
                                       const mbedtls_x509_crt_profile *profile,
                                       const char *cn, uint32_t *flags,
                                       int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
                                       void *p_vrfy);

#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */

/**
 * \brief          Check usage of certificate against keyUsage extension.
 *
 * \param crt      Leaf certificate used.
 * \param usage    Intended usage(s) (eg MBEDTLS_X509_KU_KEY_ENCIPHERMENT
 *                 before using the certificate to perform an RSA key
 *                 exchange).
 *
 * \note           Except for decipherOnly and encipherOnly, a bit set in the
 *                 usage argument means this bit MUST be set in the
 *                 certificate. For decipherOnly and encipherOnly, it means
 *                 that bit MAY be set.
 *
 * \return         0 is these uses of the certificate are allowed,
 *                 MBEDTLS_ERR_X509_BAD_INPUT_DATA if the keyUsage extension
 *                 is present but does not match the usage argument.
 *
 * \note           You should only call this function on leaf certificates, on
 *                 (intermediate) CAs the keyUsage extension is automatically
 *                 checked by \c mbedtls_x509_crt_verify().
 */
int mbedtls_x509_crt_check_key_usage(const mbedtls_x509_crt *crt,
                                     unsigned int usage);

/**
 * \brief           Check usage of certificate against extendedKeyUsage.
 *
 * \param crt       Leaf certificate used.
 * \param usage_oid Intended usage (eg MBEDTLS_OID_SERVER_AUTH or
 *                  MBEDTLS_OID_CLIENT_AUTH).
 * \param usage_len Length of usage_oid (eg given by MBEDTLS_OID_SIZE()).
 *
 * \return          0 if this use of the certificate is allowed,
 *                  MBEDTLS_ERR_X509_BAD_INPUT_DATA if not.
 *
 * \note            Usually only makes sense on leaf certificates.
 */
int mbedtls_x509_crt_check_extended_key_usage(const mbedtls_x509_crt *crt,
                                              const char *usage_oid,
                                              size_t usage_len);

#if defined(MBEDTLS_X509_CRL_PARSE_C)
/**
 * \brief          Verify the certificate revocation status
 *
 * \param crt      a certificate to be verified
 * \param crl      the CRL to verify against
 *
 * \return         1 if the certificate is revoked, 0 otherwise
 *
 */
int mbedtls_x509_crt_is_revoked(const mbedtls_x509_crt *crt, const mbedtls_x509_crl *crl);
#endif /* MBEDTLS_X509_CRL_PARSE_C */

/**
 * \brief          Initialize a certificate (chain)
 *
 * \param crt      Certificate chain to initialize
 */
void mbedtls_x509_crt_init(mbedtls_x509_crt *crt);

/**
 * \brief          Unallocate all certificate data
 *
 * \param crt      Certificate chain to free
 */
void mbedtls_x509_crt_free(mbedtls_x509_crt *crt);

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
/**
 * \brief           Initialize a restart context
 */
void mbedtls_x509_crt_restart_init(mbedtls_x509_crt_restart_ctx *ctx);

/**
 * \brief           Free the components of a restart context
 */
void mbedtls_x509_crt_restart_free(mbedtls_x509_crt_restart_ctx *ctx);
#endif /* MBEDTLS_ECDSA_C && MBEDTLS_ECP_RESTARTABLE */
#endif /* MBEDTLS_X509_CRT_PARSE_C */

/**
 * \brief               Query certificate for given extension type
 *
 * \param[in] ctx       Certificate context to be queried, must not be \c NULL
 * \param ext_type      Extension type being queried for, must be a valid
 *                      extension type. Must be one of the MBEDTLS_X509_EXT_XXX
 *                      values
 *
 * \return              0 if the given extension type is not present,
 *                      non-zero otherwise
 */
static inline int mbedtls_x509_crt_has_ext_type(const mbedtls_x509_crt *ctx,
                                                int ext_type)
{
    return ctx->MBEDTLS_PRIVATE(ext_types) & ext_type;
}

/**
 * \brief               Access the ca_istrue field
 *
 * \param[in] crt       Certificate to be queried, must not be \c NULL
 *
 * \return              \c 1 if this a CA certificate \c 0 otherwise.
 * \return              MBEDTLS_ERR_X509_INVALID_EXTENSIONS if the certificate does not contain
 *                      the Optional Basic Constraint extension.
 *
 */
int mbedtls_x509_crt_get_ca_istrue(const mbedtls_x509_crt *crt);

/** \} name Structures and functions for parsing and writing X.509 certificates */

#if defined(MBEDTLS_X509_CRT_WRITE_C)
/**
 * \brief           Initialize a CRT writing context
 *
 * \param ctx       CRT context to initialize
 */
void mbedtls_x509write_crt_init(mbedtls_x509write_cert *ctx);

/**
 * \brief           Set the version for a Certificate
 *                  Default: MBEDTLS_X509_CRT_VERSION_3
 *
 * \param ctx       CRT context to use
 * \param version   version to set (MBEDTLS_X509_CRT_VERSION_1, MBEDTLS_X509_CRT_VERSION_2 or
 *                                  MBEDTLS_X509_CRT_VERSION_3)
 */
void mbedtls_x509write_crt_set_version(mbedtls_x509write_cert *ctx, int version);

#if defined(MBEDTLS_BIGNUM_C) && !defined(MBEDTLS_DEPRECATED_REMOVED)
/**
 * \brief           Set the serial number for a Certificate.
 *
 * \deprecated      This function is deprecated and will be removed in a
 *                  future version of the library. Please use
 *                  mbedtls_x509write_crt_set_serial_raw() instead.
 *
 * \note            Even though the MBEDTLS_BIGNUM_C guard looks redundant since
 *                  X509 depends on PK and PK depends on BIGNUM, this emphasizes
 *                  a direct dependency between X509 and BIGNUM which is going
 *                  to be deprecated in the future.
 *
 * \param ctx       CRT context to use
 * \param serial    serial number to set
 *
 * \return          0 if successful
 */
int MBEDTLS_DEPRECATED mbedtls_x509write_crt_set_serial(
    mbedtls_x509write_cert *ctx, const mbedtls_mpi *serial);
#endif // MBEDTLS_BIGNUM_C && !MBEDTLS_DEPRECATED_REMOVED

/**
 * \brief           Set the serial number for a Certificate.
 *
 * \param ctx          CRT context to use
 * \param serial       A raw array of bytes containing the serial number in big
 *                     endian format
 * \param serial_len   Length of valid bytes (expressed in bytes) in \p serial
 *                     input buffer
 *
 * \return          0 if successful, or
 *                  MBEDTLS_ERR_X509_BAD_INPUT_DATA if the provided input buffer
 *                  is too big (longer than MBEDTLS_X509_RFC5280_MAX_SERIAL_LEN)
 */
int mbedtls_x509write_crt_set_serial_raw(mbedtls_x509write_cert *ctx,
                                         unsigned char *serial, size_t serial_len);

/**
 * \brief           Set the validity period for a Certificate
 *                  Timestamps should be in string format for UTC timezone
 *                  i.e. "YYYYMMDDhhmmss"
 *                  e.g. "20131231235959" for December 31st 2013
 *                       at 23:59:59
 *
 * \param ctx       CRT context to use
 * \param not_before    not_before timestamp
 * \param not_after     not_after timestamp
 *
 * \return          0 if timestamp was parsed successfully, or
 *                  a specific error code
 */
int mbedtls_x509write_crt_set_validity(mbedtls_x509write_cert *ctx, const char *not_before,
                                       const char *not_after);

/**
 * \brief           Set the issuer name for a Certificate
 *                  Issuer names should contain a comma-separated list
 *                  of OID types and values:
 *                  e.g. "C=UK,O=ARM,CN=Mbed TLS CA"
 *
 * \param ctx           CRT context to use
 * \param issuer_name   issuer name to set
 *
 * \return          0 if issuer name was parsed successfully, or
 *                  a specific error code
 */
int mbedtls_x509write_crt_set_issuer_name(mbedtls_x509write_cert *ctx,
                                          const char *issuer_name);

/**
 * \brief           Set the subject name for a Certificate
 *                  Subject names should contain a comma-separated list
 *                  of OID types and values:
 *                  e.g. "C=UK,O=ARM,CN=Mbed TLS Server 1"
 *
 * \param ctx           CRT context to use
 * \param subject_name  subject name to set
 *
 * \return          0 if subject name was parsed successfully, or
 *                  a specific error code
 */
int mbedtls_x509write_crt_set_subject_name(mbedtls_x509write_cert *ctx,
                                           const char *subject_name);

/**
 * \brief           Set the subject public key for the certificate
 *
 * \param ctx       CRT context to use
 * \param key       public key to include
 */
void mbedtls_x509write_crt_set_subject_key(mbedtls_x509write_cert *ctx, mbedtls_pk_context *key);

/**
 * \brief           Set the issuer key used for signing the certificate
 *
 * \param ctx       CRT context to use
 * \param key       private key to sign with
 */
void mbedtls_x509write_crt_set_issuer_key(mbedtls_x509write_cert *ctx, mbedtls_pk_context *key);

/**
 * \brief           Set the MD algorithm to use for the signature
 *                  (e.g. MBEDTLS_MD_SHA1)
 *
 * \param ctx       CRT context to use
 * \param md_alg    MD algorithm to use
 */
void mbedtls_x509write_crt_set_md_alg(mbedtls_x509write_cert *ctx, mbedtls_md_type_t md_alg);

/**
 * \brief           Generic function to add to or replace an extension in the
 *                  CRT
 *
 * \param ctx       CRT context to use
 * \param oid       OID of the extension
 * \param oid_len   length of the OID
 * \param critical  if the extension is critical (per the RFC's definition)
 * \param val       value of the extension OCTET STRING
 * \param val_len   length of the value data
 *
 * \return          0 if successful, or a MBEDTLS_ERR_X509_ALLOC_FAILED
 */
int mbedtls_x509write_crt_set_extension(mbedtls_x509write_cert *ctx,
                                        const char *oid, size_t oid_len,
                                        int critical,
                                        const unsigned char *val, size_t val_len);

/**
 * \brief           Set the basicConstraints extension for a CRT
 *
 * \param ctx       CRT context to use
 * \param is_ca     is this a CA certificate
 * \param max_pathlen   maximum length of certificate chains below this
 *                      certificate (only for CA certificates, -1 is
 *                      unlimited)
 *
 * \return          0 if successful, or a MBEDTLS_ERR_X509_ALLOC_FAILED
 */
int mbedtls_x509write_crt_set_basic_constraints(mbedtls_x509write_cert *ctx,
                                                int is_ca, int max_pathlen);

#if defined(MBEDTLS_MD_CAN_SHA1)
/**
 * \brief           Set the subjectKeyIdentifier extension for a CRT
 *                  Requires that mbedtls_x509write_crt_set_subject_key() has been
 *                  called before
 *
 * \param ctx       CRT context to use
 *
 * \return          0 if successful, or a MBEDTLS_ERR_X509_ALLOC_FAILED
 */
int mbedtls_x509write_crt_set_subject_key_identifier(mbedtls_x509write_cert *ctx);

/**
 * \brief           Set the authorityKeyIdentifier extension for a CRT
 *                  Requires that mbedtls_x509write_crt_set_issuer_key() has been
 *                  called before
 *
 * \param ctx       CRT context to use
 *
 * \return          0 if successful, or a MBEDTLS_ERR_X509_ALLOC_FAILED
 */
int mbedtls_x509write_crt_set_authority_key_identifier(mbedtls_x509write_cert *ctx);
#endif /* MBEDTLS_MD_CAN_SHA1 */

/**
 * \brief           Set the Key Usage Extension flags
 *                  (e.g. MBEDTLS_X509_KU_DIGITAL_SIGNATURE | MBEDTLS_X509_KU_KEY_CERT_SIGN)
 *
 * \param ctx       CRT context to use
 * \param key_usage key usage flags to set
 *
 * \return          0 if successful, or MBEDTLS_ERR_X509_ALLOC_FAILED
 */
int mbedtls_x509write_crt_set_key_usage(mbedtls_x509write_cert *ctx,
                                        unsigned int key_usage);

/**
 * \brief           Set the Extended Key Usage Extension
 *                  (e.g. MBEDTLS_OID_SERVER_AUTH)
 *
 * \param ctx       CRT context to use
 * \param exts      extended key usage extensions to set, a sequence of
 *                  MBEDTLS_ASN1_OID objects
 *
 * \return          0 if successful, or MBEDTLS_ERR_X509_ALLOC_FAILED
 */
int mbedtls_x509write_crt_set_ext_key_usage(mbedtls_x509write_cert *ctx,
                                            const mbedtls_asn1_sequence *exts);

/**
 * \brief           Set the Netscape Cert Type flags
 *                  (e.g. MBEDTLS_X509_NS_CERT_TYPE_SSL_CLIENT | MBEDTLS_X509_NS_CERT_TYPE_EMAIL)
 *
 * \param ctx           CRT context to use
 * \param ns_cert_type  Netscape Cert Type flags to set
 *
 * \return          0 if successful, or MBEDTLS_ERR_X509_ALLOC_FAILED
 */
int mbedtls_x509write_crt_set_ns_cert_type(mbedtls_x509write_cert *ctx,
                                           unsigned char ns_cert_type);

/**
 * \brief           Free the contents of a CRT write context
 *
 * \param ctx       CRT context to free
 */
void mbedtls_x509write_crt_free(mbedtls_x509write_cert *ctx);

/**
 * \brief           Write a built up certificate to a X509 DER structure
 *                  Note: data is written at the end of the buffer! Use the
 *                        return value to determine where you should start
 *                        using the buffer
 *
 * \param ctx       certificate to write away
 * \param buf       buffer to write to
 * \param size      size of the buffer
 * \param f_rng     RNG function. This must not be \c NULL.
 * \param p_rng     RNG parameter
 *
 * \return          length of data written if successful, or a specific
 *                  error code
 *
 * \note            \p f_rng is used for the signature operation.
 */
int mbedtls_x509write_crt_der(mbedtls_x509write_cert *ctx, unsigned char *buf, size_t size,
                              mbedtls_f_rng_t *f_rng,
                              void *p_rng);

#if defined(MBEDTLS_PEM_WRITE_C)
/**
 * \brief           Write a built up certificate to a X509 PEM string
 *
 * \param ctx       certificate to write away
 * \param buf       buffer to write to
 * \param size      size of the buffer
 * \param f_rng     RNG function. This must not be \c NULL.
 * \param p_rng     RNG parameter
 *
 * \return          0 if successful, or a specific error code
 *
 * \note            \p f_rng is used for the signature operation.
 */
int mbedtls_x509write_crt_pem(mbedtls_x509write_cert *ctx, unsigned char *buf, size_t size,
                              mbedtls_f_rng_t *f_rng,
                              void *p_rng);
#endif /* MBEDTLS_PEM_WRITE_C */
#endif /* MBEDTLS_X509_CRT_WRITE_C */

/** \} addtogroup x509_module */

#ifdef __cplusplus
}
#endif

#endif /* mbedtls_x509_crt.h */

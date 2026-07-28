/**
 * \file x509.h
 *
 * \brief X.509 generic defines and structures
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_X509_H
#define MBEDTLS_X509_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/asn1.h"
#include "mbedtls/pk.h"

#if defined(MBEDTLS_RSA_C)
#include "mbedtls/rsa.h"
#endif

/**
 * \addtogroup x509_module
 * \{
 */

#if !defined(MBEDTLS_X509_MAX_INTERMEDIATE_CA)
/**
 * Maximum number of intermediate CAs in a verification chain.
 * That is, maximum length of the chain, excluding the end-entity certificate
 * and the trusted root certificate.
 *
 * Set this to a low value to prevent an adversary from making you waste
 * resources verifying an overlong certificate chain.
 */
#define MBEDTLS_X509_MAX_INTERMEDIATE_CA   8
#endif

/**
 * \name X509 Error codes
 * \{
 */
/** Unavailable feature, e.g. RSA hashing/encryption combination. */
#define MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE              -0x2080
/** Requested OID is unknown. */
#define MBEDTLS_ERR_X509_UNKNOWN_OID                      -0x2100
/** The CRT/CRL/CSR format is invalid, e.g. different type expected. */
#define MBEDTLS_ERR_X509_INVALID_FORMAT                   -0x2180
/** The CRT/CRL/CSR version element is invalid. */
#define MBEDTLS_ERR_X509_INVALID_VERSION                  -0x2200
/** The serial tag or value is invalid. */
#define MBEDTLS_ERR_X509_INVALID_SERIAL                   -0x2280
/** The algorithm tag or value is invalid. */
#define MBEDTLS_ERR_X509_INVALID_ALG                      -0x2300
/** The name tag or value is invalid. */
#define MBEDTLS_ERR_X509_INVALID_NAME                     -0x2380
/** The date tag or value is invalid. */
#define MBEDTLS_ERR_X509_INVALID_DATE                     -0x2400
/** The signature tag or value invalid. */
#define MBEDTLS_ERR_X509_INVALID_SIGNATURE                -0x2480
/** The extension tag or value is invalid. */
#define MBEDTLS_ERR_X509_INVALID_EXTENSIONS               -0x2500
/** CRT/CRL/CSR has an unsupported version number. */
#define MBEDTLS_ERR_X509_UNKNOWN_VERSION                  -0x2580
/** Signature algorithm (oid) is unsupported. */
#define MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG                  -0x2600
/** Signature algorithms do not match. (see \c ::mbedtls_x509_crt sig_oid) */
#define MBEDTLS_ERR_X509_SIG_MISMATCH                     -0x2680
/** Certificate verification failed, e.g. CRL, CA or signature check failed. */
#define MBEDTLS_ERR_X509_CERT_VERIFY_FAILED               -0x2700
/** Format not recognized as DER or PEM. */
#define MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT              -0x2780
/** Input invalid. */
#define MBEDTLS_ERR_X509_BAD_INPUT_DATA                   -0x2800
/** Allocation of memory failed. */
#define MBEDTLS_ERR_X509_ALLOC_FAILED                     -0x2880
/** Read/write of file failed. */
#define MBEDTLS_ERR_X509_FILE_IO_ERROR                    -0x2900
/** Destination buffer is too small. */
#define MBEDTLS_ERR_X509_BUFFER_TOO_SMALL                 -0x2980
/** A fatal error occurred, eg the chain is too long or the vrfy callback failed. */
#define MBEDTLS_ERR_X509_FATAL_ERROR                      -0x3000
/** \} name X509 Error codes */

/**
 * \name X509 Verify codes
 * \{
 */
/* Reminder: update x509_crt_verify_strings[] in library/x509_crt.c */
#define MBEDTLS_X509_BADCERT_EXPIRED             0x01  /**< The certificate validity has expired. */
#define MBEDTLS_X509_BADCERT_REVOKED             0x02  /**< The certificate has been revoked (is on a CRL). */
#define MBEDTLS_X509_BADCERT_CN_MISMATCH         0x04  /**< The certificate Common Name (CN) does not match with the expected CN. */
#define MBEDTLS_X509_BADCERT_NOT_TRUSTED         0x08  /**< The certificate is not correctly signed by the trusted CA. */
#define MBEDTLS_X509_BADCRL_NOT_TRUSTED          0x10  /**< The CRL is not correctly signed by the trusted CA. */
#define MBEDTLS_X509_BADCRL_EXPIRED              0x20  /**< The CRL is expired. */
#define MBEDTLS_X509_BADCERT_MISSING             0x40  /**< Certificate was missing. */
#define MBEDTLS_X509_BADCERT_SKIP_VERIFY         0x80  /**< Certificate verification was skipped. */
#define MBEDTLS_X509_BADCERT_OTHER             0x0100  /**< Other reason (can be used by verify callback) */
#define MBEDTLS_X509_BADCERT_FUTURE            0x0200  /**< The certificate validity starts in the future. */
#define MBEDTLS_X509_BADCRL_FUTURE             0x0400  /**< The CRL is from the future */
#define MBEDTLS_X509_BADCERT_KEY_USAGE         0x0800  /**< Usage does not match the keyUsage extension. */
#define MBEDTLS_X509_BADCERT_EXT_KEY_USAGE     0x1000  /**< Usage does not match the extendedKeyUsage extension. */
#define MBEDTLS_X509_BADCERT_NS_CERT_TYPE      0x2000  /**< Usage does not match the nsCertType extension. */
#define MBEDTLS_X509_BADCERT_BAD_MD            0x4000  /**< The certificate is signed with an unacceptable hash. */
#define MBEDTLS_X509_BADCERT_BAD_PK            0x8000  /**< The certificate is signed with an unacceptable PK alg (eg RSA vs ECDSA). */
#define MBEDTLS_X509_BADCERT_BAD_KEY         0x010000  /**< The certificate is signed with an unacceptable key (eg bad curve, RSA too short). */
#define MBEDTLS_X509_BADCRL_BAD_MD           0x020000  /**< The CRL is signed with an unacceptable hash. */
#define MBEDTLS_X509_BADCRL_BAD_PK           0x040000  /**< The CRL is signed with an unacceptable PK alg (eg RSA vs ECDSA). */
#define MBEDTLS_X509_BADCRL_BAD_KEY          0x080000  /**< The CRL is signed with an unacceptable key (eg bad curve, RSA too short). */

/** \} name X509 Verify codes */
/** \} addtogroup x509_module */

/*
 * X.509 v3 Subject Alternative Name types.
 *      otherName                       [0]     OtherName,
 *      rfc822Name                      [1]     IA5String,
 *      dNSName                         [2]     IA5String,
 *      x400Address                     [3]     ORAddress,
 *      directoryName                   [4]     Name,
 *      ediPartyName                    [5]     EDIPartyName,
 *      uniformResourceIdentifier       [6]     IA5String,
 *      iPAddress                       [7]     OCTET STRING,
 *      registeredID                    [8]     OBJECT IDENTIFIER
 */
#define MBEDTLS_X509_SAN_OTHER_NAME                      0
#define MBEDTLS_X509_SAN_RFC822_NAME                     1
#define MBEDTLS_X509_SAN_DNS_NAME                        2
#define MBEDTLS_X509_SAN_X400_ADDRESS_NAME               3
#define MBEDTLS_X509_SAN_DIRECTORY_NAME                  4
#define MBEDTLS_X509_SAN_EDI_PARTY_NAME                  5
#define MBEDTLS_X509_SAN_UNIFORM_RESOURCE_IDENTIFIER     6
#define MBEDTLS_X509_SAN_IP_ADDRESS                      7
#define MBEDTLS_X509_SAN_REGISTERED_ID                   8

/*
 * X.509 v3 Key Usage Extension flags
 * Reminder: update mbedtls_x509_info_key_usage() when adding new flags.
 */
#define MBEDTLS_X509_KU_DIGITAL_SIGNATURE            (0x80)  /* bit 0 */
#define MBEDTLS_X509_KU_NON_REPUDIATION              (0x40)  /* bit 1 */
#define MBEDTLS_X509_KU_KEY_ENCIPHERMENT             (0x20)  /* bit 2 */
#define MBEDTLS_X509_KU_DATA_ENCIPHERMENT            (0x10)  /* bit 3 */
#define MBEDTLS_X509_KU_KEY_AGREEMENT                (0x08)  /* bit 4 */
#define MBEDTLS_X509_KU_KEY_CERT_SIGN                (0x04)  /* bit 5 */
#define MBEDTLS_X509_KU_CRL_SIGN                     (0x02)  /* bit 6 */
#define MBEDTLS_X509_KU_ENCIPHER_ONLY                (0x01)  /* bit 7 */
#define MBEDTLS_X509_KU_DECIPHER_ONLY              (0x8000)  /* bit 8 */

/*
 * Netscape certificate types
 * (http://www.mozilla.org/projects/security/pki/nss/tech-notes/tn3.html)
 */

#define MBEDTLS_X509_NS_CERT_TYPE_SSL_CLIENT         (0x80)  /* bit 0 */
#define MBEDTLS_X509_NS_CERT_TYPE_SSL_SERVER         (0x40)  /* bit 1 */
#define MBEDTLS_X509_NS_CERT_TYPE_EMAIL              (0x20)  /* bit 2 */
#define MBEDTLS_X509_NS_CERT_TYPE_OBJECT_SIGNING     (0x10)  /* bit 3 */
#define MBEDTLS_X509_NS_CERT_TYPE_RESERVED           (0x08)  /* bit 4 */
#define MBEDTLS_X509_NS_CERT_TYPE_SSL_CA             (0x04)  /* bit 5 */
#define MBEDTLS_X509_NS_CERT_TYPE_EMAIL_CA           (0x02)  /* bit 6 */
#define MBEDTLS_X509_NS_CERT_TYPE_OBJECT_SIGNING_CA  (0x01)  /* bit 7 */

/*
 * X.509 extension types
 *
 * Comments refer to the status for using certificates. Status can be
 * different for writing certificates or reading CRLs or CSRs.
 *
 * Those are defined in oid.h as oid.c needs them in a data structure. Since
 * these were previously defined here, let's have aliases for compatibility.
 */
#define MBEDTLS_X509_EXT_AUTHORITY_KEY_IDENTIFIER MBEDTLS_OID_X509_EXT_AUTHORITY_KEY_IDENTIFIER
#define MBEDTLS_X509_EXT_SUBJECT_KEY_IDENTIFIER   MBEDTLS_OID_X509_EXT_SUBJECT_KEY_IDENTIFIER
#define MBEDTLS_X509_EXT_KEY_USAGE                MBEDTLS_OID_X509_EXT_KEY_USAGE
#define MBEDTLS_X509_EXT_CERTIFICATE_POLICIES     MBEDTLS_OID_X509_EXT_CERTIFICATE_POLICIES
#define MBEDTLS_X509_EXT_POLICY_MAPPINGS          MBEDTLS_OID_X509_EXT_POLICY_MAPPINGS
#define MBEDTLS_X509_EXT_SUBJECT_ALT_NAME         MBEDTLS_OID_X509_EXT_SUBJECT_ALT_NAME         /* Supported (DNS) */
#define MBEDTLS_X509_EXT_ISSUER_ALT_NAME          MBEDTLS_OID_X509_EXT_ISSUER_ALT_NAME
#define MBEDTLS_X509_EXT_SUBJECT_DIRECTORY_ATTRS  MBEDTLS_OID_X509_EXT_SUBJECT_DIRECTORY_ATTRS
#define MBEDTLS_X509_EXT_BASIC_CONSTRAINTS        MBEDTLS_OID_X509_EXT_BASIC_CONSTRAINTS        /* Supported */
#define MBEDTLS_X509_EXT_NAME_CONSTRAINTS         MBEDTLS_OID_X509_EXT_NAME_CONSTRAINTS
#define MBEDTLS_X509_EXT_POLICY_CONSTRAINTS       MBEDTLS_OID_X509_EXT_POLICY_CONSTRAINTS
#define MBEDTLS_X509_EXT_EXTENDED_KEY_USAGE       MBEDTLS_OID_X509_EXT_EXTENDED_KEY_USAGE
#define MBEDTLS_X509_EXT_CRL_DISTRIBUTION_POINTS  MBEDTLS_OID_X509_EXT_CRL_DISTRIBUTION_POINTS
#define MBEDTLS_X509_EXT_INIHIBIT_ANYPOLICY       MBEDTLS_OID_X509_EXT_INIHIBIT_ANYPOLICY
#define MBEDTLS_X509_EXT_FRESHEST_CRL             MBEDTLS_OID_X509_EXT_FRESHEST_CRL
#define MBEDTLS_X509_EXT_NS_CERT_TYPE             MBEDTLS_OID_X509_EXT_NS_CERT_TYPE

/*
 * Storage format identifiers
 * Recognized formats: PEM and DER
 */
#define MBEDTLS_X509_FORMAT_DER                 1
#define MBEDTLS_X509_FORMAT_PEM                 2

#define MBEDTLS_X509_MAX_DN_NAME_SIZE         256 /**< Maximum value size of a DN entry */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup x509_module
 * \{ */

/**
 * \name Structures for parsing X.509 certificates, CRLs and CSRs
 * \{
 */

/**
 * Type-length-value structure that allows for ASN1 using DER.
 */
typedef mbedtls_asn1_buf mbedtls_x509_buf;

/**
 * Container for ASN1 bit strings.
 */
typedef mbedtls_asn1_bitstring mbedtls_x509_bitstring;

/**
 * Container for ASN1 named information objects.
 * It allows for Relative Distinguished Names (e.g. cn=localhost,ou=code,etc.).
 */
typedef mbedtls_asn1_named_data mbedtls_x509_name;

/**
 * Container for a sequence of ASN.1 items
 */
typedef mbedtls_asn1_sequence mbedtls_x509_sequence;

/*
 * Container for the fields of the Authority Key Identifier object
 */
typedef struct mbedtls_x509_authority {
    mbedtls_x509_buf keyIdentifier;
    mbedtls_x509_sequence authorityCertIssuer;
    mbedtls_x509_buf authorityCertSerialNumber;
    mbedtls_x509_buf raw;
}
mbedtls_x509_authority;

/** Container for date and time (precision in seconds). */
typedef struct mbedtls_x509_time {
    int year, mon, day;         /**< Date. */
    int hour, min, sec;         /**< Time. */
}
mbedtls_x509_time;

/**
 * From RFC 5280 section 4.2.1.6:
 * OtherName ::= SEQUENCE {
 *      type-id    OBJECT IDENTIFIER,
 *      value      [0] EXPLICIT ANY DEFINED BY type-id }
 *
 * Future versions of the library may add new fields to this structure or
 * to its embedded union and structure.
 */
typedef struct mbedtls_x509_san_other_name {
    /**
     * The type_id is an OID as defined in RFC 5280.
     * To check the value of the type id, you should use
     * \p MBEDTLS_OID_CMP with a known OID mbedtls_x509_buf.
     */
    mbedtls_x509_buf type_id;                   /**< The type id. */
    union {
        /**
         * From RFC 4108 section 5:
         * HardwareModuleName ::= SEQUENCE {
         *                         hwType OBJECT IDENTIFIER,
         *                         hwSerialNum OCTET STRING }
         */
        struct {
            mbedtls_x509_buf oid;               /**< The object identifier. */
            mbedtls_x509_buf val;               /**< The named value. */
        }
        hardware_module_name;
    }
    value;
}
mbedtls_x509_san_other_name;

/**
 * A structure for holding the parsed Subject Alternative Name,
 * according to type.
 *
 * Future versions of the library may add new fields to this structure or
 * to its embedded union and structure.
 */
typedef struct mbedtls_x509_subject_alternative_name {
    int type;                              /**< The SAN type, value of MBEDTLS_X509_SAN_XXX. */
    union {
        mbedtls_x509_san_other_name other_name;
        mbedtls_x509_name directory_name;
        mbedtls_x509_buf unstructured_name; /**< The buffer for the unstructured types. rfc822Name, dnsName and uniformResourceIdentifier are currently supported. */
    }
    san; /**< A union of the supported SAN types */
}
mbedtls_x509_subject_alternative_name;

typedef struct mbedtls_x509_san_list {
    mbedtls_x509_subject_alternative_name node;
    struct mbedtls_x509_san_list *next;
}
mbedtls_x509_san_list;

/** \} name Structures for parsing X.509 certificates, CRLs and CSRs */
/** \} addtogroup x509_module */

/**
 * \brief          Store the certificate DN in printable form into buf;
 *                 no more than size characters will be written.
 *
 * \param buf      Buffer to write to
 * \param size     Maximum size of buffer
 * \param dn       The X509 name to represent
 *
 * \return         The length of the string written (not including the
 *                 terminated nul byte), or a negative error code.
 */
int mbedtls_x509_dn_gets(char *buf, size_t size, const mbedtls_x509_name *dn);

/**
 * \brief            Convert the certificate DN string \p name into
 *                   a linked list of mbedtls_x509_name (equivalent to
 *                   mbedtls_asn1_named_data).
 *
 * \note             This function allocates a linked list, and places the head
 *                   pointer in \p head. This list must later be freed by a
 *                   call to mbedtls_asn1_free_named_data_list().
 *
 * \param[out] head  Address in which to store the pointer to the head of the
 *                   allocated list of mbedtls_x509_name. Must point to NULL on
 *                   entry.
 * \param[in] name   The string representation of a DN to convert
 *
 * \return           0 on success, or a negative error code.
 */
int mbedtls_x509_string_to_names(mbedtls_asn1_named_data **head, const char *name);

/**
 * \brief          Return the next relative DN in an X509 name.
 *
 * \note           Intended use is to compare function result to dn->next
 *                 in order to detect boundaries of multi-valued RDNs.
 *
 * \param dn       Current node in the X509 name
 *
 * \return         Pointer to the first attribute-value pair of the
 *                 next RDN in sequence, or NULL if end is reached.
 */
static inline mbedtls_x509_name *mbedtls_x509_dn_get_next(
    mbedtls_x509_name *dn)
{
    while (dn->MBEDTLS_PRIVATE(next_merged) && dn->next != NULL) {
        dn = dn->next;
    }
    return dn->next;
}

/**
 * \brief          Store the certificate serial in printable form into buf;
 *                 no more than size characters will be written.
 *
 * \param buf      Buffer to write to
 * \param size     Maximum size of buffer
 * \param serial   The X509 serial to represent
 *
 * \return         The length of the string written (not including the
 *                 terminated nul byte), or a negative error code.
 */
int mbedtls_x509_serial_gets(char *buf, size_t size, const mbedtls_x509_buf *serial);

/**
 * \brief          Compare pair of mbedtls_x509_time.
 *
 * \param t1       mbedtls_x509_time to compare
 * \param t2       mbedtls_x509_time to compare
 *
 * \return         < 0 if t1 is before t2
 *                   0 if t1 equals t2
 *                 > 0 if t1 is after t2
 */
int mbedtls_x509_time_cmp(const mbedtls_x509_time *t1, const mbedtls_x509_time *t2);

#if defined(MBEDTLS_HAVE_TIME_DATE)
/**
 * \brief          Fill mbedtls_x509_time with provided mbedtls_time_t.
 *
 * \param tt       mbedtls_time_t to convert
 * \param now      mbedtls_x509_time to fill with converted mbedtls_time_t
 *
 * \return         \c 0 on success
 * \return         A non-zero return value on failure.
 */
int mbedtls_x509_time_gmtime(mbedtls_time_t tt, mbedtls_x509_time *now);
#endif /* MBEDTLS_HAVE_TIME_DATE */

/**
 * \brief          Check a given mbedtls_x509_time against the system time
 *                 and tell if it's in the past.
 *
 * \note           Intended usage is "if( is_past( valid_to ) ) ERROR".
 *                 Hence the return value of 1 if on internal errors.
 *
 * \param to       mbedtls_x509_time to check
 *
 * \return         1 if the given time is in the past or an error occurred,
 *                 0 otherwise.
 */
int mbedtls_x509_time_is_past(const mbedtls_x509_time *to);

/**
 * \brief          Check a given mbedtls_x509_time against the system time
 *                 and tell if it's in the future.
 *
 * \note           Intended usage is "if( is_future( valid_from ) ) ERROR".
 *                 Hence the return value of 1 if on internal errors.
 *
 * \param from     mbedtls_x509_time to check
 *
 * \return         1 if the given time is in the future or an error occurred,
 *                 0 otherwise.
 */
int mbedtls_x509_time_is_future(const mbedtls_x509_time *from);

/**
 * \brief          This function parses an item in the SubjectAlternativeNames
 *                 extension. Please note that this function might allocate
 *                 additional memory for a subject alternative name, thus
 *                 mbedtls_x509_free_subject_alt_name has to be called
 *                 to dispose of this additional memory afterwards.
 *
 * \param san_buf  The buffer holding the raw data item of the subject
 *                 alternative name.
 * \param san      The target structure to populate with the parsed presentation
 *                 of the subject alternative name encoded in \p san_buf.
 *
 * \note           Supported GeneralName types, as defined in RFC 5280:
 *                 "rfc822Name", "dnsName", "directoryName",
 *                 "uniformResourceIdentifier" and "hardware_module_name"
 *                 of type "otherName", as defined in RFC 4108.
 *
 * \note           This function should be called on a single raw data of
 *                 subject alternative name. For example, after successful
 *                 certificate parsing, one must iterate on every item in the
 *                 \c crt->subject_alt_names sequence, and pass it to
 *                 this function.
 *
 * \warning        The target structure contains pointers to the raw data of the
 *                 parsed certificate, and its lifetime is restricted by the
 *                 lifetime of the certificate.
 *
 * \return         \c 0 on success
 * \return         #MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE for an unsupported
 *                 SAN type.
 * \return         Another negative value for any other failure.
 */
int mbedtls_x509_parse_subject_alt_name(const mbedtls_x509_buf *san_buf,
                                        mbedtls_x509_subject_alternative_name *san);
/**
 * \brief          Unallocate all data related to subject alternative name
 *
 * \param san      SAN structure - extra memory owned by this structure will be freed
 */
void mbedtls_x509_free_subject_alt_name(mbedtls_x509_subject_alternative_name *san);

/**
 * \brief          This function parses a CN string as an IP address.
 *
 * \param cn       The CN string to parse. CN string MUST be null-terminated.
 * \param dst      The target buffer to populate with the binary IP address.
 *                 The buffer MUST be 16 bytes to save IPv6, and should be
 *                 4-byte aligned if the result will be used as struct in_addr.
 *                 e.g. uint32_t dst[4]
 *
 * \note           \p cn is parsed as an IPv6 address if string contains ':',
 *                 else \p cn is parsed as an IPv4 address.
 *
 * \return         Length of binary IP address; num bytes written to target.
 * \return         \c 0 on failure to parse CN string as an IP address.
 */
size_t mbedtls_x509_crt_parse_cn_inet_pton(const char *cn, void *dst);

#define MBEDTLS_X509_SAFE_SNPRINTF                          \
    do {                                                    \
        if (ret < 0 || (size_t) ret >= n)                  \
        return MBEDTLS_ERR_X509_BUFFER_TOO_SMALL;    \
                                                          \
        n -= (size_t) ret;                                  \
        p += (size_t) ret;                                  \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_X509_H */

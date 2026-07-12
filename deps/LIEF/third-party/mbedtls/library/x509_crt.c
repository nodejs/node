/*
 *  X.509 certificate parsing and verification
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 *  The ITU-T X.509 standard defines a certificate format for PKI.
 *
 *  http://www.ietf.org/rfc/rfc5280.txt (Certificates and CRLs)
 *  http://www.ietf.org/rfc/rfc3279.txt (Alg IDs for CRLs)
 *  http://www.ietf.org/rfc/rfc2986.txt (CSRs, aka PKCS#10)
 *
 *  http://www.itu.int/ITU-T/studygroups/com17/languages/X.680-0207.pdf
 *  http://www.itu.int/ITU-T/studygroups/com17/languages/X.690-0207.pdf
 *
 *  [SIRO] https://cabforum.org/wp-content/uploads/Chunghwatelecom201503cabforumV4.pdf
 */

#include "common.h"

#if defined(MBEDTLS_X509_CRT_PARSE_C)

#include "mbedtls/x509_crt.h"
#include "x509_internal.h"
#include "mbedtls/error.h"
#include "mbedtls/oid.h"
#include "mbedtls/platform_util.h"

#include <string.h>

#if defined(MBEDTLS_PEM_PARSE_C)
#include "mbedtls/pem.h"
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa/crypto.h"
#include "psa_util_internal.h"
#include "mbedtls/psa_util.h"
#endif /* MBEDTLS_USE_PSA_CRYPTO */
#include "pk_internal.h"

#include "mbedtls/platform.h"

#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif

#if defined(MBEDTLS_HAVE_TIME)
#if defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif
#endif

#if defined(MBEDTLS_FS_IO)
#include <stdio.h>
#if !defined(_WIN32) || defined(EFIX64) || defined(EFI32)
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__MBED__)
#include <platform/mbed_retarget.h>
#else
#include <dirent.h>
#endif /* __MBED__ */
#include <errno.h>
#endif /* !_WIN32 || EFIX64 || EFI32 */
#endif

/*
 * Item in a verification chain: cert and flags for it
 */
typedef struct {
    mbedtls_x509_crt *crt;
    uint32_t flags;
} x509_crt_verify_chain_item;

/*
 * Max size of verification chain: end-entity + intermediates + trusted root
 */
#define X509_MAX_VERIFY_CHAIN_SIZE    (MBEDTLS_X509_MAX_INTERMEDIATE_CA + 2)

/* Default profile. Do not remove items unless there are serious security
 * concerns. */
const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_default =
{
    /* Hashes from SHA-256 and above. Note that this selection
     * should be aligned with ssl_preset_default_hashes in ssl_tls.c. */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    0xFFFFFFF, /* Any PK alg    */
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    /* Curves at or above 128-bit security level. Note that this selection
     * should be aligned with ssl_preset_default_curves in ssl_tls.c. */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP384R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP521R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_BP256R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_BP384R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_BP512R1) |
    0,
#else /* MBEDTLS_PK_HAVE_ECC_KEYS */
    0,
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */
    2048,
};

/* Next-generation profile. Currently identical to the default, but may
 * be tightened at any time. */
const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_next =
{
    /* Hashes from SHA-256 and above. */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    0xFFFFFFF, /* Any PK alg    */
#if defined(MBEDTLS_ECP_C)
    /* Curves at or above 128-bit security level. */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP384R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP521R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_BP256R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_BP384R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_BP512R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256K1),
#else
    0,
#endif
    2048,
};

/*
 * NSA Suite B Profile
 */
const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_suiteb =
{
    /* Only SHA-256 and 384 */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384),
    /* Only ECDSA */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECKEY),
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    /* Only NIST P-256 and P-384 */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP384R1),
#else /* MBEDTLS_PK_HAVE_ECC_KEYS */
    0,
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */
    0,
};

/*
 * Empty / all-forbidden profile
 */
const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_none =
{
    0,
    0,
    0,
    (uint32_t) -1,
};

/*
 * Check md_alg against profile
 * Return 0 if md_alg is acceptable for this profile, -1 otherwise
 */
static int x509_profile_check_md_alg(const mbedtls_x509_crt_profile *profile,
                                     mbedtls_md_type_t md_alg)
{
    if (md_alg == MBEDTLS_MD_NONE) {
        return -1;
    }

    if ((profile->allowed_mds & MBEDTLS_X509_ID_FLAG(md_alg)) != 0) {
        return 0;
    }

    return -1;
}

/*
 * Check pk_alg against profile
 * Return 0 if pk_alg is acceptable for this profile, -1 otherwise
 */
static int x509_profile_check_pk_alg(const mbedtls_x509_crt_profile *profile,
                                     mbedtls_pk_type_t pk_alg)
{
    if (pk_alg == MBEDTLS_PK_NONE) {
        return -1;
    }

    if ((profile->allowed_pks & MBEDTLS_X509_ID_FLAG(pk_alg)) != 0) {
        return 0;
    }

    return -1;
}

/*
 * Check key against profile
 * Return 0 if pk is acceptable for this profile, -1 otherwise
 */
static int x509_profile_check_key(const mbedtls_x509_crt_profile *profile,
                                  const mbedtls_pk_context *pk)
{
    const mbedtls_pk_type_t pk_alg = mbedtls_pk_get_type(pk);

#if defined(MBEDTLS_RSA_C)
    if (pk_alg == MBEDTLS_PK_RSA || pk_alg == MBEDTLS_PK_RSASSA_PSS) {
        if (mbedtls_pk_get_bitlen(pk) >= profile->rsa_min_bitlen) {
            return 0;
        }

        return -1;
    }
#endif /* MBEDTLS_RSA_C */

#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (pk_alg == MBEDTLS_PK_ECDSA ||
        pk_alg == MBEDTLS_PK_ECKEY ||
        pk_alg == MBEDTLS_PK_ECKEY_DH) {
        const mbedtls_ecp_group_id gid = mbedtls_pk_get_ec_group_id(pk);

        if (gid == MBEDTLS_ECP_DP_NONE) {
            return -1;
        }

        if ((profile->allowed_curves & MBEDTLS_X509_ID_FLAG(gid)) != 0) {
            return 0;
        }

        return -1;
    }
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

    return -1;
}

/*
 * Like memcmp, but case-insensitive and always returns -1 if different
 */
static int x509_memcasecmp(const void *s1, const void *s2, size_t len)
{
    size_t i;
    unsigned char diff;
    const unsigned char *n1 = s1, *n2 = s2;

    for (i = 0; i < len; i++) {
        diff = n1[i] ^ n2[i];

        if (diff == 0) {
            continue;
        }

        if (diff == 32 &&
            ((n1[i] >= 'a' && n1[i] <= 'z') ||
             (n1[i] >= 'A' && n1[i] <= 'Z'))) {
            continue;
        }

        return -1;
    }

    return 0;
}

/*
 * Return 0 if name matches wildcard, -1 otherwise
 */
static int x509_check_wildcard(const char *cn, const mbedtls_x509_buf *name)
{
    size_t i;
    size_t cn_idx = 0, cn_len = strlen(cn);

    /* We can't have a match if there is no wildcard to match */
    if (name->len < 3 || name->p[0] != '*' || name->p[1] != '.') {
        return -1;
    }

    for (i = 0; i < cn_len; ++i) {
        if (cn[i] == '.') {
            cn_idx = i;
            break;
        }
    }

    if (cn_idx == 0) {
        return -1;
    }

    if (cn_len - cn_idx == name->len - 1 &&
        x509_memcasecmp(name->p + 1, cn + cn_idx, name->len - 1) == 0) {
        return 0;
    }

    return -1;
}

/*
 * Compare two X.509 strings, case-insensitive, and allowing for some encoding
 * variations (but not all).
 *
 * Return 0 if equal, -1 otherwise.
 */
static int x509_string_cmp(const mbedtls_x509_buf *a, const mbedtls_x509_buf *b)
{
    if (a->tag == b->tag &&
        a->len == b->len &&
        memcmp(a->p, b->p, b->len) == 0) {
        return 0;
    }

    if ((a->tag == MBEDTLS_ASN1_UTF8_STRING || a->tag == MBEDTLS_ASN1_PRINTABLE_STRING) &&
        (b->tag == MBEDTLS_ASN1_UTF8_STRING || b->tag == MBEDTLS_ASN1_PRINTABLE_STRING) &&
        a->len == b->len &&
        x509_memcasecmp(a->p, b->p, b->len) == 0) {
        return 0;
    }

    return -1;
}

/*
 * Compare two X.509 Names (aka rdnSequence).
 *
 * See RFC 5280 section 7.1, though we don't implement the whole algorithm:
 * we sometimes return unequal when the full algorithm would return equal,
 * but never the other way. (In particular, we don't do Unicode normalisation
 * or space folding.)
 *
 * Return 0 if equal, -1 otherwise.
 */
static int x509_name_cmp(const mbedtls_x509_name *a, const mbedtls_x509_name *b)
{
    /* Avoid recursion, it might not be optimised by the compiler */
    while (a != NULL || b != NULL) {
        if (a == NULL || b == NULL) {
            return -1;
        }

        /* type */
        if (a->oid.tag != b->oid.tag ||
            a->oid.len != b->oid.len ||
            memcmp(a->oid.p, b->oid.p, b->oid.len) != 0) {
            return -1;
        }

        /* value */
        if (x509_string_cmp(&a->val, &b->val) != 0) {
            return -1;
        }

        /* structure of the list of sets */
        if (a->next_merged != b->next_merged) {
            return -1;
        }

        a = a->next;
        b = b->next;
    }

    /* a == NULL == b */
    return 0;
}

/*
 * Reset (init or clear) a verify_chain
 */
static void x509_crt_verify_chain_reset(
    mbedtls_x509_crt_verify_chain *ver_chain)
{
    size_t i;

    for (i = 0; i < MBEDTLS_X509_MAX_VERIFY_CHAIN_SIZE; i++) {
        ver_chain->items[i].crt = NULL;
        ver_chain->items[i].flags = (uint32_t) -1;
    }

    ver_chain->len = 0;

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
    ver_chain->trust_ca_cb_result = NULL;
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */
}

/*
 *  Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
 */
static int x509_get_version(unsigned char **p,
                            const unsigned char *end,
                            int *ver)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                    MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED |
                                    0)) != 0) {
        if (ret == MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
            *ver = 0;
            return 0;
        }

        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT, ret);
    }

    end = *p + len;

    if ((ret = mbedtls_asn1_get_int(p, end, ver)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_VERSION, ret);
    }

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_VERSION,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 *  Validity ::= SEQUENCE {
 *       notBefore      Time,
 *       notAfter       Time }
 */
static int x509_get_dates(unsigned char **p,
                          const unsigned char *end,
                          mbedtls_x509_time *from,
                          mbedtls_x509_time *to)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_DATE, ret);
    }

    end = *p + len;

    if ((ret = mbedtls_x509_get_time(p, end, from)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_x509_get_time(p, end, to)) != 0) {
        return ret;
    }

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_DATE,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 * X.509 v2/v3 unique identifier (not parsed)
 */
static int x509_get_uid(unsigned char **p,
                        const unsigned char *end,
                        mbedtls_x509_buf *uid, int n)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (*p == end) {
        return 0;
    }

    uid->tag = **p;

    if ((ret = mbedtls_asn1_get_tag(p, end, &uid->len,
                                    MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED |
                                    n)) != 0) {
        if (ret == MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
            return 0;
        }

        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT, ret);
    }

    uid->p = *p;
    *p += uid->len;

    return 0;
}

static int x509_get_basic_constraints(unsigned char **p,
                                      const unsigned char *end,
                                      int *ca_istrue,
                                      int *max_pathlen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    /*
     * BasicConstraints ::= SEQUENCE {
     *      cA                      BOOLEAN DEFAULT FALSE,
     *      pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
     */
    *ca_istrue = 0; /* DEFAULT FALSE */
    *max_pathlen = 0; /* endless */

    if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
    }

    if (*p == end) {
        return 0;
    }

    if ((ret = mbedtls_asn1_get_bool(p, end, ca_istrue)) != 0) {
        if (ret == MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
            ret = mbedtls_asn1_get_int(p, end, ca_istrue);
        }

        if (ret != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }

        if (*ca_istrue != 0) {
            *ca_istrue = 1;
        }
    }

    if (*p == end) {
        return 0;
    }

    if ((ret = mbedtls_asn1_get_int(p, end, max_pathlen)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
    }

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    /* Do not accept max_pathlen equal to INT_MAX to avoid a signed integer
     * overflow, which is an undefined behavior. */
    if (*max_pathlen == INT_MAX) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_INVALID_LENGTH);
    }

    (*max_pathlen)++;

    return 0;
}

/*
 * ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
 *
 * KeyPurposeId ::= OBJECT IDENTIFIER
 */
static int x509_get_ext_key_usage(unsigned char **p,
                                  const unsigned char *end,
                                  mbedtls_x509_sequence *ext_key_usage)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if ((ret = mbedtls_asn1_get_sequence_of(p, end, ext_key_usage, MBEDTLS_ASN1_OID)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
    }

    /* Sequence length must be >= 1 */
    if (ext_key_usage->buf.p == NULL) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_INVALID_LENGTH);
    }

    return 0;
}

/*
 * SubjectKeyIdentifier ::= KeyIdentifier
 *
 * KeyIdentifier ::= OCTET STRING
 */
static int x509_get_subject_key_id(unsigned char **p,
                                   const unsigned char *end,
                                   mbedtls_x509_buf *subject_key_id)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0u;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                    MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
    }

    subject_key_id->len = len;
    subject_key_id->tag = MBEDTLS_ASN1_OCTET_STRING;
    subject_key_id->p = *p;
    *p += len;

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 * AuthorityKeyIdentifier ::= SEQUENCE {
 *        keyIdentifier [0] KeyIdentifier OPTIONAL,
 *        authorityCertIssuer [1] GeneralNames OPTIONAL,
 *        authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL }
 *
 *    KeyIdentifier ::= OCTET STRING
 */
static int x509_get_authority_key_id(unsigned char **p,
                                     unsigned char *end,
                                     mbedtls_x509_authority *authority_key_id)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0u;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
    }

    if (*p + len != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    ret = mbedtls_asn1_get_tag(p, end, &len,
                               MBEDTLS_ASN1_CONTEXT_SPECIFIC);

    /* KeyIdentifier is an OPTIONAL field */
    if (ret == 0) {
        authority_key_id->keyIdentifier.len = len;
        authority_key_id->keyIdentifier.p = *p;
        /* Setting tag of the keyIdentfier intentionally to 0x04.
         * Although the .keyIdentfier field is CONTEXT_SPECIFIC ([0] OPTIONAL),
         * its tag with the content is the payload of on OCTET STRING primitive */
        authority_key_id->keyIdentifier.tag = MBEDTLS_ASN1_OCTET_STRING;

        *p += len;
    } else if (ret != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
    }

    if (*p < end) {
        /* Getting authorityCertIssuer using the required specific class tag [1] */
        if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                        MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED |
                                        1)) != 0) {
            /* authorityCertIssuer and authorityCertSerialNumber MUST both
               be present or both be absent. At this point we expect to have both. */
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }
        /* "end" also includes the CertSerialNumber field so "len" shall be used */
        ret = mbedtls_x509_get_subject_alt_name_ext(p,
                                                    (*p+len),
                                                    &authority_key_id->authorityCertIssuer);
        if (ret != 0) {
            return ret;
        }

        /* Getting authorityCertSerialNumber using the required specific class tag [2] */
        if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                        MBEDTLS_ASN1_CONTEXT_SPECIFIC | 2)) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }
        authority_key_id->authorityCertSerialNumber.len = len;
        authority_key_id->authorityCertSerialNumber.p = *p;
        authority_key_id->authorityCertSerialNumber.tag = MBEDTLS_ASN1_INTEGER;
        *p += len;
    }

    if (*p != end) {
        return MBEDTLS_ERR_X509_INVALID_EXTENSIONS +
               MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
    }

    return 0;
}

/*
 * id-ce-certificatePolicies OBJECT IDENTIFIER ::=  { id-ce 32 }
 *
 * anyPolicy OBJECT IDENTIFIER ::= { id-ce-certificatePolicies 0 }
 *
 * certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
 *
 * PolicyInformation ::= SEQUENCE {
 *     policyIdentifier   CertPolicyId,
 *     policyQualifiers   SEQUENCE SIZE (1..MAX) OF
 *                             PolicyQualifierInfo OPTIONAL }
 *
 * CertPolicyId ::= OBJECT IDENTIFIER
 *
 * PolicyQualifierInfo ::= SEQUENCE {
 *      policyQualifierId  PolicyQualifierId,
 *      qualifier          ANY DEFINED BY policyQualifierId }
 *
 * -- policyQualifierIds for Internet policy qualifiers
 *
 * id-qt          OBJECT IDENTIFIER ::=  { id-pkix 2 }
 * id-qt-cps      OBJECT IDENTIFIER ::=  { id-qt 1 }
 * id-qt-unotice  OBJECT IDENTIFIER ::=  { id-qt 2 }
 *
 * PolicyQualifierId ::= OBJECT IDENTIFIER ( id-qt-cps | id-qt-unotice )
 *
 * Qualifier ::= CHOICE {
 *      cPSuri           CPSuri,
 *      userNotice       UserNotice }
 *
 * CPSuri ::= IA5String
 *
 * UserNotice ::= SEQUENCE {
 *      noticeRef        NoticeReference OPTIONAL,
 *      explicitText     DisplayText OPTIONAL }
 *
 * NoticeReference ::= SEQUENCE {
 *      organization     DisplayText,
 *      noticeNumbers    SEQUENCE OF INTEGER }
 *
 * DisplayText ::= CHOICE {
 *      ia5String        IA5String      (SIZE (1..200)),
 *      visibleString    VisibleString  (SIZE (1..200)),
 *      bmpString        BMPString      (SIZE (1..200)),
 *      utf8String       UTF8String     (SIZE (1..200)) }
 *
 * NOTE: we only parse and use anyPolicy without qualifiers at this point
 * as defined in RFC 5280.
 */
static int x509_get_certificate_policies(unsigned char **p,
                                         const unsigned char *end,
                                         mbedtls_x509_sequence *certificate_policies)
{
    int ret, parse_ret = 0;
    size_t len;
    mbedtls_asn1_buf *buf;
    mbedtls_asn1_sequence *cur = certificate_policies;

    /* Get main sequence tag */
    ret = mbedtls_asn1_get_tag(p, end, &len,
                               MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
    }

    if (*p + len != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    /*
     * Cannot be an empty sequence.
     */
    if (len == 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    while (*p < end) {
        mbedtls_x509_buf policy_oid;
        const unsigned char *policy_end;

        /*
         * Get the policy sequence
         */
        if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                        MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }

        policy_end = *p + len;

        if ((ret = mbedtls_asn1_get_tag(p, policy_end, &len,
                                        MBEDTLS_ASN1_OID)) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }

        policy_oid.tag = MBEDTLS_ASN1_OID;
        policy_oid.len = len;
        policy_oid.p = *p;

        /*
         * Only AnyPolicy is currently supported when enforcing policy.
         */
        if (MBEDTLS_OID_CMP(MBEDTLS_OID_ANY_POLICY, &policy_oid) != 0) {
            /*
             * Set the parsing return code but continue parsing, in case this
             * extension is critical.
             */
            parse_ret = MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE;
        }

        /* Allocate and assign next pointer */
        if (cur->buf.p != NULL) {
            if (cur->next != NULL) {
                return MBEDTLS_ERR_X509_INVALID_EXTENSIONS;
            }

            cur->next = mbedtls_calloc(1, sizeof(mbedtls_asn1_sequence));

            if (cur->next == NULL) {
                return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                         MBEDTLS_ERR_ASN1_ALLOC_FAILED);
            }

            cur = cur->next;
        }

        buf = &(cur->buf);
        buf->tag = policy_oid.tag;
        buf->p = policy_oid.p;
        buf->len = policy_oid.len;

        *p += len;

        /*
         * If there is an optional qualifier, then *p < policy_end
         * Check the Qualifier len to verify it doesn't exceed policy_end.
         */
        if (*p < policy_end) {
            if ((ret = mbedtls_asn1_get_tag(p, policy_end, &len,
                                            MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) !=
                0) {
                return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
            }
            /*
             * Skip the optional policy qualifiers.
             */
            *p += len;
        }

        if (*p != policy_end) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                     MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
        }
    }

    /* Set final sequence entry's next pointer to NULL */
    cur->next = NULL;

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return parse_ret;
}

/*
 * X.509 v3 extensions
 *
 */
static int x509_get_crt_ext(unsigned char **p,
                            const unsigned char *end,
                            mbedtls_x509_crt *crt,
                            mbedtls_x509_crt_ext_cb_t cb,
                            void *p_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;
    unsigned char *end_ext_data, *start_ext_octet, *end_ext_octet;

    if (*p == end) {
        return 0;
    }

    if ((ret = mbedtls_x509_get_ext(p, end, &crt->v3_ext, 3)) != 0) {
        return ret;
    }

    end = crt->v3_ext.p + crt->v3_ext.len;
    while (*p < end) {
        /*
         * Extension  ::=  SEQUENCE  {
         *      extnID      OBJECT IDENTIFIER,
         *      critical    BOOLEAN DEFAULT FALSE,
         *      extnValue   OCTET STRING  }
         */
        mbedtls_x509_buf extn_oid = { 0, 0, NULL };
        int is_critical = 0; /* DEFAULT FALSE */
        int ext_type = 0;

        if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                        MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }

        end_ext_data = *p + len;

        /* Get extension ID */
        if ((ret = mbedtls_asn1_get_tag(p, end_ext_data, &extn_oid.len,
                                        MBEDTLS_ASN1_OID)) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }

        extn_oid.tag = MBEDTLS_ASN1_OID;
        extn_oid.p = *p;
        *p += extn_oid.len;

        /* Get optional critical */
        if ((ret = mbedtls_asn1_get_bool(p, end_ext_data, &is_critical)) != 0 &&
            (ret != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG)) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }

        /* Data should be octet string type */
        if ((ret = mbedtls_asn1_get_tag(p, end_ext_data, &len,
                                        MBEDTLS_ASN1_OCTET_STRING)) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }

        start_ext_octet = *p;
        end_ext_octet = *p + len;

        if (end_ext_octet != end_ext_data) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                     MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
        }

        /*
         * Detect supported extensions
         */
        ret = mbedtls_oid_get_x509_ext_type(&extn_oid, &ext_type);

        if (ret != 0) {
            /* Give the callback (if any) a chance to handle the extension */
            if (cb != NULL) {
                ret = cb(p_ctx, crt, &extn_oid, is_critical, *p, end_ext_octet);
                if (ret != 0 && is_critical) {
                    return ret;
                }
                *p = end_ext_octet;
                continue;
            }

            /* No parser found, skip extension */
            *p = end_ext_octet;

            if (is_critical) {
                /* Data is marked as critical: fail */
                return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                         MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
            }
            continue;
        }

        /* Forbid repeated extensions */
        if ((crt->ext_types & ext_type) != 0) {
            return MBEDTLS_ERR_X509_INVALID_EXTENSIONS;
        }

        crt->ext_types |= ext_type;

        switch (ext_type) {
            case MBEDTLS_X509_EXT_BASIC_CONSTRAINTS:
                /* Parse basic constraints */
                if ((ret = x509_get_basic_constraints(p, end_ext_octet,
                                                      &crt->ca_istrue, &crt->max_pathlen)) != 0) {
                    return ret;
                }
                break;

            case MBEDTLS_X509_EXT_KEY_USAGE:
                /* Parse key usage */
                if ((ret = mbedtls_x509_get_key_usage(p, end_ext_octet,
                                                      &crt->key_usage)) != 0) {
                    return ret;
                }
                break;

            case MBEDTLS_X509_EXT_EXTENDED_KEY_USAGE:
                /* Parse extended key usage */
                if ((ret = x509_get_ext_key_usage(p, end_ext_octet,
                                                  &crt->ext_key_usage)) != 0) {
                    return ret;
                }
                break;

            case MBEDTLS_X509_EXT_SUBJECT_KEY_IDENTIFIER:
                /* Parse subject key identifier */
                if ((ret = x509_get_subject_key_id(p, end_ext_data,
                                                   &crt->subject_key_id)) != 0) {
                    return ret;
                }
                break;

            case MBEDTLS_X509_EXT_AUTHORITY_KEY_IDENTIFIER:
                /* Parse authority key identifier */
                if ((ret = x509_get_authority_key_id(p, end_ext_octet,
                                                     &crt->authority_key_id)) != 0) {
                    return ret;
                }
                break;
            case MBEDTLS_X509_EXT_SUBJECT_ALT_NAME:
                /* Parse subject alt name
                 * SubjectAltName ::= GeneralNames
                 */
                if ((ret = mbedtls_x509_get_subject_alt_name(p, end_ext_octet,
                                                             &crt->subject_alt_names)) != 0) {
                    return ret;
                }
                break;

            case MBEDTLS_X509_EXT_NS_CERT_TYPE:
                /* Parse netscape certificate type */
                if ((ret = mbedtls_x509_get_ns_cert_type(p, end_ext_octet,
                                                         &crt->ns_cert_type)) != 0) {
                    return ret;
                }
                break;

            case MBEDTLS_OID_X509_EXT_CERTIFICATE_POLICIES:
                /* Parse certificate policies type */
                if ((ret = x509_get_certificate_policies(p, end_ext_octet,
                                                         &crt->certificate_policies)) != 0) {
                    /* Give the callback (if any) a chance to handle the extension
                     * if it contains unsupported policies */
                    if (ret == MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE && cb != NULL &&
                        cb(p_ctx, crt, &extn_oid, is_critical,
                           start_ext_octet, end_ext_octet) == 0) {
                        break;
                    }

                    if (is_critical) {
                        return ret;
                    } else
                    /*
                     * If MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE is returned, then we
                     * cannot interpret or enforce the policy. However, it is up to
                     * the user to choose how to enforce the policies,
                     * unless the extension is critical.
                     */
                    if (ret != MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE) {
                        return ret;
                    }
                }
                break;

            default:
                /*
                 * If this is a non-critical extension, which the oid layer
                 * supports, but there isn't an x509 parser for it,
                 * skip the extension.
                 */
                if (is_critical) {
                    return MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE;
                } else {
                    *p = end_ext_octet;
                }
        }
    }

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 * Parse and fill a single X.509 certificate in DER format
 */
static int x509_crt_parse_der_core(mbedtls_x509_crt *crt,
                                   const unsigned char *buf,
                                   size_t buflen,
                                   int make_copy,
                                   mbedtls_x509_crt_ext_cb_t cb,
                                   void *p_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;
    unsigned char *p, *end, *crt_end;
    mbedtls_x509_buf sig_params1, sig_params2, sig_oid2;

    memset(&sig_params1, 0, sizeof(mbedtls_x509_buf));
    memset(&sig_params2, 0, sizeof(mbedtls_x509_buf));
    memset(&sig_oid2, 0, sizeof(mbedtls_x509_buf));

    /*
     * Check for valid input
     */
    if (crt == NULL || buf == NULL) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    /* Use the original buffer until we figure out actual length. */
    p = (unsigned char *) buf;
    len = buflen;
    end = p + len;

    /*
     * Certificate  ::=  SEQUENCE  {
     *      tbsCertificate       TBSCertificate,
     *      signatureAlgorithm   AlgorithmIdentifier,
     *      signatureValue       BIT STRING  }
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        mbedtls_x509_crt_free(crt);
        return MBEDTLS_ERR_X509_INVALID_FORMAT;
    }

    end = crt_end = p + len;
    crt->raw.len = (size_t) (crt_end - buf);
    if (make_copy != 0) {
        /* Create and populate a new buffer for the raw field. */
        crt->raw.p = p = mbedtls_calloc(1, crt->raw.len);
        if (crt->raw.p == NULL) {
            return MBEDTLS_ERR_X509_ALLOC_FAILED;
        }

        memcpy(crt->raw.p, buf, crt->raw.len);
        crt->own_buffer = 1;

        p += crt->raw.len - len;
        end = crt_end = p + len;
    } else {
        crt->raw.p = (unsigned char *) buf;
        crt->own_buffer = 0;
    }

    /*
     * TBSCertificate  ::=  SEQUENCE  {
     */
    crt->tbs.p = p;

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        mbedtls_x509_crt_free(crt);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT, ret);
    }

    end = p + len;
    crt->tbs.len = (size_t) (end - crt->tbs.p);

    /*
     * Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
     *
     * CertificateSerialNumber  ::=  INTEGER
     *
     * signature            AlgorithmIdentifier
     */
    if ((ret = x509_get_version(&p, end, &crt->version)) != 0 ||
        (ret = mbedtls_x509_get_serial(&p, end, &crt->serial)) != 0 ||
        (ret = mbedtls_x509_get_alg(&p, end, &crt->sig_oid,
                                    &sig_params1)) != 0) {
        mbedtls_x509_crt_free(crt);
        return ret;
    }

    if (crt->version < 0 || crt->version > 2) {
        mbedtls_x509_crt_free(crt);
        return MBEDTLS_ERR_X509_UNKNOWN_VERSION;
    }

    crt->version++;

    if ((ret = mbedtls_x509_get_sig_alg(&crt->sig_oid, &sig_params1,
                                        &crt->sig_md, &crt->sig_pk,
                                        &crt->sig_opts)) != 0) {
        mbedtls_x509_crt_free(crt);
        return ret;
    }

    /*
     * issuer               Name
     */
    crt->issuer_raw.p = p;

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        mbedtls_x509_crt_free(crt);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT, ret);
    }

    if ((ret = mbedtls_x509_get_name(&p, p + len, &crt->issuer)) != 0) {
        mbedtls_x509_crt_free(crt);
        return ret;
    }

    crt->issuer_raw.len = (size_t) (p - crt->issuer_raw.p);

    /*
     * Validity ::= SEQUENCE {
     *      notBefore      Time,
     *      notAfter       Time }
     *
     */
    if ((ret = x509_get_dates(&p, end, &crt->valid_from,
                              &crt->valid_to)) != 0) {
        mbedtls_x509_crt_free(crt);
        return ret;
    }

    /*
     * subject              Name
     */
    crt->subject_raw.p = p;

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        mbedtls_x509_crt_free(crt);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT, ret);
    }

    if (len && (ret = mbedtls_x509_get_name(&p, p + len, &crt->subject)) != 0) {
        mbedtls_x509_crt_free(crt);
        return ret;
    }

    crt->subject_raw.len = (size_t) (p - crt->subject_raw.p);

    /*
     * SubjectPublicKeyInfo
     */
    crt->pk_raw.p = p;
    if ((ret = mbedtls_pk_parse_subpubkey(&p, end, &crt->pk)) != 0) {
        mbedtls_x509_crt_free(crt);
        return ret;
    }
    crt->pk_raw.len = (size_t) (p - crt->pk_raw.p);

    /*
     *  issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
     *                       -- If present, version shall be v2 or v3
     *  subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
     *                       -- If present, version shall be v2 or v3
     *  extensions      [3]  EXPLICIT Extensions OPTIONAL
     *                       -- If present, version shall be v3
     */
    if (crt->version == 2 || crt->version == 3) {
        ret = x509_get_uid(&p, end, &crt->issuer_id,  1);
        if (ret != 0) {
            mbedtls_x509_crt_free(crt);
            return ret;
        }
    }

    if (crt->version == 2 || crt->version == 3) {
        ret = x509_get_uid(&p, end, &crt->subject_id,  2);
        if (ret != 0) {
            mbedtls_x509_crt_free(crt);
            return ret;
        }
    }

    if (crt->version == 3) {
        ret = x509_get_crt_ext(&p, end, crt, cb, p_ctx);
        if (ret != 0) {
            mbedtls_x509_crt_free(crt);
            return ret;
        }
    }

    if (p != end) {
        mbedtls_x509_crt_free(crt);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    end = crt_end;

    /*
     *  }
     *  -- end of TBSCertificate
     *
     *  signatureAlgorithm   AlgorithmIdentifier,
     *  signatureValue       BIT STRING
     */
    if ((ret = mbedtls_x509_get_alg(&p, end, &sig_oid2, &sig_params2)) != 0) {
        mbedtls_x509_crt_free(crt);
        return ret;
    }

    if (crt->sig_oid.len != sig_oid2.len ||
        memcmp(crt->sig_oid.p, sig_oid2.p, crt->sig_oid.len) != 0 ||
        sig_params1.tag != sig_params2.tag ||
        sig_params1.len != sig_params2.len ||
        (sig_params1.len != 0 &&
         memcmp(sig_params1.p, sig_params2.p, sig_params1.len) != 0)) {
        mbedtls_x509_crt_free(crt);
        return MBEDTLS_ERR_X509_SIG_MISMATCH;
    }

    if ((ret = mbedtls_x509_get_sig(&p, end, &crt->sig)) != 0) {
        mbedtls_x509_crt_free(crt);
        return ret;
    }

    if (p != end) {
        mbedtls_x509_crt_free(crt);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 * Parse one X.509 certificate in DER format from a buffer and add them to a
 * chained list
 */
static int mbedtls_x509_crt_parse_der_internal(mbedtls_x509_crt *chain,
                                               const unsigned char *buf,
                                               size_t buflen,
                                               int make_copy,
                                               mbedtls_x509_crt_ext_cb_t cb,
                                               void *p_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_x509_crt *crt = chain, *prev = NULL;

    /*
     * Check for valid input
     */
    if (crt == NULL || buf == NULL) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    while (crt->version != 0 && crt->next != NULL) {
        prev = crt;
        crt = crt->next;
    }

    /*
     * Add new certificate on the end of the chain if needed.
     */
    if (crt->version != 0 && crt->next == NULL) {
        crt->next = mbedtls_calloc(1, sizeof(mbedtls_x509_crt));

        if (crt->next == NULL) {
            return MBEDTLS_ERR_X509_ALLOC_FAILED;
        }

        prev = crt;
        mbedtls_x509_crt_init(crt->next);
        crt = crt->next;
    }

    ret = x509_crt_parse_der_core(crt, buf, buflen, make_copy, cb, p_ctx);
    if (ret != 0) {
        if (prev) {
            prev->next = NULL;
        }

        if (crt != chain) {
            mbedtls_free(crt);
        }

        return ret;
    }

    return 0;
}

int mbedtls_x509_crt_parse_der_nocopy(mbedtls_x509_crt *chain,
                                      const unsigned char *buf,
                                      size_t buflen)
{
    return mbedtls_x509_crt_parse_der_internal(chain, buf, buflen, 0, NULL, NULL);
}

int mbedtls_x509_crt_parse_der_with_ext_cb(mbedtls_x509_crt *chain,
                                           const unsigned char *buf,
                                           size_t buflen,
                                           int make_copy,
                                           mbedtls_x509_crt_ext_cb_t cb,
                                           void *p_ctx)
{
    return mbedtls_x509_crt_parse_der_internal(chain, buf, buflen, make_copy, cb, p_ctx);
}

int mbedtls_x509_crt_parse_der(mbedtls_x509_crt *chain,
                               const unsigned char *buf,
                               size_t buflen)
{
    return mbedtls_x509_crt_parse_der_internal(chain, buf, buflen, 1, NULL, NULL);
}

/*
 * Parse one or more PEM certificates from a buffer and add them to the chained
 * list
 */
int mbedtls_x509_crt_parse(mbedtls_x509_crt *chain,
                           const unsigned char *buf,
                           size_t buflen)
{
#if defined(MBEDTLS_PEM_PARSE_C)
    int success = 0, first_error = 0, total_failed = 0;
    int buf_format = MBEDTLS_X509_FORMAT_DER;
#endif

    /*
     * Check for valid input
     */
    if (chain == NULL || buf == NULL) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    /*
     * Determine buffer content. Buffer contains either one DER certificate or
     * one or more PEM certificates.
     */
#if defined(MBEDTLS_PEM_PARSE_C)
    if (buflen != 0 && buf[buflen - 1] == '\0' &&
        strstr((const char *) buf, "-----BEGIN CERTIFICATE-----") != NULL) {
        buf_format = MBEDTLS_X509_FORMAT_PEM;
    }

    if (buf_format == MBEDTLS_X509_FORMAT_DER) {
        return mbedtls_x509_crt_parse_der(chain, buf, buflen);
    }
#else
    return mbedtls_x509_crt_parse_der(chain, buf, buflen);
#endif

#if defined(MBEDTLS_PEM_PARSE_C)
    if (buf_format == MBEDTLS_X509_FORMAT_PEM) {
        int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
        mbedtls_pem_context pem;

        /* 1 rather than 0 since the terminating NULL byte is counted in */
        while (buflen > 1) {
            size_t use_len;
            mbedtls_pem_init(&pem);

            /* If we get there, we know the string is null-terminated */
            ret = mbedtls_pem_read_buffer(&pem,
                                          "-----BEGIN CERTIFICATE-----",
                                          "-----END CERTIFICATE-----",
                                          buf, NULL, 0, &use_len);

            if (ret == 0) {
                /*
                 * Was PEM encoded
                 */
                buflen -= use_len;
                buf += use_len;
            } else if (ret == MBEDTLS_ERR_PEM_BAD_INPUT_DATA) {
                return ret;
            } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
                mbedtls_pem_free(&pem);

                /*
                 * PEM header and footer were found
                 */
                buflen -= use_len;
                buf += use_len;

                if (first_error == 0) {
                    first_error = ret;
                }

                total_failed++;
                continue;
            } else {
                break;
            }

            ret = mbedtls_x509_crt_parse_der(chain, pem.buf, pem.buflen);

            mbedtls_pem_free(&pem);

            if (ret != 0) {
                /*
                 * Quit parsing on a memory error
                 */
                if (ret == MBEDTLS_ERR_X509_ALLOC_FAILED) {
                    return ret;
                }

                if (first_error == 0) {
                    first_error = ret;
                }

                total_failed++;
                continue;
            }

            success = 1;
        }
    }

    if (success) {
        return total_failed;
    } else if (first_error) {
        return first_error;
    } else {
        return MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT;
    }
#endif /* MBEDTLS_PEM_PARSE_C */
}

#if defined(MBEDTLS_FS_IO)
/*
 * Load one or more certificates and add them to the chained list
 */
int mbedtls_x509_crt_parse_file(mbedtls_x509_crt *chain, const char *path)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    unsigned char *buf;

    if ((ret = mbedtls_pk_load_file(path, &buf, &n)) != 0) {
        return ret;
    }

    ret = mbedtls_x509_crt_parse(chain, buf, n);

    mbedtls_zeroize_and_free(buf, n);

    return ret;
}

int mbedtls_x509_crt_parse_path(mbedtls_x509_crt *chain, const char *path)
{
    int ret = 0;
#if defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)
    int w_ret;
    WCHAR szDir[MAX_PATH];
    char filename[MAX_PATH];
    char *p;
    size_t len = strlen(path);

    WIN32_FIND_DATAW file_data;
    HANDLE hFind;

    if (len > MAX_PATH - 3) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    memset(szDir, 0, sizeof(szDir));
    memset(filename, 0, MAX_PATH);
    memcpy(filename, path, len);
    filename[len++] = '\\';
    p = filename + len;
    filename[len++] = '*';

    /*
     * Note this function uses the code page CP_ACP which is the system default
     * ANSI codepage. The input string is always described in BYTES and the
     * output length is described in WCHARs.
     */
    w_ret = MultiByteToWideChar(CP_ACP, 0, filename, (int) len, szDir,
                                MAX_PATH - 3);
    if (w_ret == 0) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    hFind = FindFirstFileW(szDir, &file_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return MBEDTLS_ERR_X509_FILE_IO_ERROR;
    }

    len = MAX_PATH - len;
    do {
        memset(p, 0, len);

        if (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        w_ret = WideCharToMultiByte(CP_ACP, 0, file_data.cFileName,
                                    -1, p, (int) len, NULL, NULL);
        if (w_ret == 0) {
            ret = MBEDTLS_ERR_X509_FILE_IO_ERROR;
            goto cleanup;
        }

        w_ret = mbedtls_x509_crt_parse_file(chain, filename);
        if (w_ret < 0) {
            ret++;
        } else {
            ret += w_ret;
        }
    } while (FindNextFileW(hFind, &file_data) != 0);

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        ret = MBEDTLS_ERR_X509_FILE_IO_ERROR;
    }

cleanup:
    FindClose(hFind);
#else /* _WIN32 */
    int t_ret;
    int snp_ret;
    struct stat sb;
    struct dirent *entry;
    char entry_name[MBEDTLS_X509_MAX_FILE_PATH_LEN];
    DIR *dir = opendir(path);

    if (dir == NULL) {
        return MBEDTLS_ERR_X509_FILE_IO_ERROR;
    }

#if defined(MBEDTLS_THREADING_C)
    if ((ret = mbedtls_mutex_lock(&mbedtls_threading_readdir_mutex)) != 0) {
        closedir(dir);
        return ret;
    }
#endif /* MBEDTLS_THREADING_C */

    memset(&sb, 0, sizeof(sb));

    while ((entry = readdir(dir)) != NULL) {
        snp_ret = mbedtls_snprintf(entry_name, sizeof(entry_name),
                                   "%s/%s", path, entry->d_name);

        if (snp_ret < 0 || (size_t) snp_ret >= sizeof(entry_name)) {
            ret = MBEDTLS_ERR_X509_BUFFER_TOO_SMALL;
            goto cleanup;
        } else if (stat(entry_name, &sb) == -1) {
            if (errno == ENOENT) {
                /* Broken symbolic link - ignore this entry.
                    stat(2) will return this error for either (a) a dangling
                    symlink or (b) a missing file.
                    Given that we have just obtained the filename from readdir,
                    assume that it does exist and therefore treat this as a
                    dangling symlink. */
                continue;
            } else {
                /* Some other file error; report the error. */
                ret = MBEDTLS_ERR_X509_FILE_IO_ERROR;
                goto cleanup;
            }
        }

        if (!S_ISREG(sb.st_mode)) {
            continue;
        }

        // Ignore parse errors
        //
        t_ret = mbedtls_x509_crt_parse_file(chain, entry_name);
        if (t_ret < 0) {
            ret++;
        } else {
            ret += t_ret;
        }
    }

cleanup:
    closedir(dir);

#if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mbedtls_threading_readdir_mutex) != 0) {
        ret = MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
#endif /* MBEDTLS_THREADING_C */

#endif /* _WIN32 */

    return ret;
}
#endif /* MBEDTLS_FS_IO */

#if !defined(MBEDTLS_X509_REMOVE_INFO)
#define PRINT_ITEM(i)                               \
    do {                                            \
        ret = mbedtls_snprintf(p, n, "%s" i, sep);  \
        MBEDTLS_X509_SAFE_SNPRINTF;                 \
        sep = ", ";                                 \
    } while (0)

#define CERT_TYPE(type, name)          \
    do {                               \
        if (ns_cert_type & (type)) {   \
            PRINT_ITEM(name);          \
        }                              \
    } while (0)

#define KEY_USAGE(code, name)      \
    do {                           \
        if (key_usage & (code)) {  \
            PRINT_ITEM(name);      \
        }                          \
    } while (0)

static int x509_info_ext_key_usage(char **buf, size_t *size,
                                   const mbedtls_x509_sequence *extended_key_usage)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const char *desc;
    size_t n = *size;
    char *p = *buf;
    const mbedtls_x509_sequence *cur = extended_key_usage;
    const char *sep = "";

    while (cur != NULL) {
        if (mbedtls_oid_get_extended_key_usage(&cur->buf, &desc) != 0) {
            desc = "???";
        }

        ret = mbedtls_snprintf(p, n, "%s%s", sep, desc);
        MBEDTLS_X509_SAFE_SNPRINTF;

        sep = ", ";

        cur = cur->next;
    }

    *size = n;
    *buf = p;

    return 0;
}

static int x509_info_cert_policies(char **buf, size_t *size,
                                   const mbedtls_x509_sequence *certificate_policies)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const char *desc;
    size_t n = *size;
    char *p = *buf;
    const mbedtls_x509_sequence *cur = certificate_policies;
    const char *sep = "";

    while (cur != NULL) {
        if (mbedtls_oid_get_certificate_policies(&cur->buf, &desc) != 0) {
            desc = "???";
        }

        ret = mbedtls_snprintf(p, n, "%s%s", sep, desc);
        MBEDTLS_X509_SAFE_SNPRINTF;

        sep = ", ";

        cur = cur->next;
    }

    *size = n;
    *buf = p;

    return 0;
}

/*
 * Return an informational string about the certificate.
 */
#define BEFORE_COLON    18
#define BC              "18"
int mbedtls_x509_crt_info(char *buf, size_t size, const char *prefix,
                          const mbedtls_x509_crt *crt)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    char *p;
    char key_size_str[BEFORE_COLON];

    p = buf;
    n = size;

    if (NULL == crt) {
        ret = mbedtls_snprintf(p, n, "\nCertificate is uninitialised!\n");
        MBEDTLS_X509_SAFE_SNPRINTF;

        return (int) (size - n);
    }

    ret = mbedtls_snprintf(p, n, "%scert. version     : %d\n",
                           prefix, crt->version);
    MBEDTLS_X509_SAFE_SNPRINTF;
    ret = mbedtls_snprintf(p, n, "%sserial number     : ",
                           prefix);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_x509_serial_gets(p, n, &crt->serial);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_snprintf(p, n, "\n%sissuer name       : ", prefix);
    MBEDTLS_X509_SAFE_SNPRINTF;
    ret = mbedtls_x509_dn_gets(p, n, &crt->issuer);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_snprintf(p, n, "\n%ssubject name      : ", prefix);
    MBEDTLS_X509_SAFE_SNPRINTF;
    ret = mbedtls_x509_dn_gets(p, n, &crt->subject);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_snprintf(p, n, "\n%sissued  on        : " \
                                 "%04d-%02d-%02d %02d:%02d:%02d", prefix,
                           crt->valid_from.year, crt->valid_from.mon,
                           crt->valid_from.day,  crt->valid_from.hour,
                           crt->valid_from.min,  crt->valid_from.sec);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_snprintf(p, n, "\n%sexpires on        : " \
                                 "%04d-%02d-%02d %02d:%02d:%02d", prefix,
                           crt->valid_to.year, crt->valid_to.mon,
                           crt->valid_to.day,  crt->valid_to.hour,
                           crt->valid_to.min,  crt->valid_to.sec);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_snprintf(p, n, "\n%ssigned using      : ", prefix);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_x509_sig_alg_gets(p, n, &crt->sig_oid, crt->sig_pk,
                                    crt->sig_md, crt->sig_opts);
    MBEDTLS_X509_SAFE_SNPRINTF;

    /* Key size */
    if ((ret = mbedtls_x509_key_size_helper(key_size_str, BEFORE_COLON,
                                            mbedtls_pk_get_name(&crt->pk))) != 0) {
        return ret;
    }

    ret = mbedtls_snprintf(p, n, "\n%s%-" BC "s: %d bits", prefix, key_size_str,
                           (int) mbedtls_pk_get_bitlen(&crt->pk));
    MBEDTLS_X509_SAFE_SNPRINTF;

    /*
     * Optional extensions
     */

    if (crt->ext_types & MBEDTLS_X509_EXT_BASIC_CONSTRAINTS) {
        ret = mbedtls_snprintf(p, n, "\n%sbasic constraints : CA=%s", prefix,
                               crt->ca_istrue ? "true" : "false");
        MBEDTLS_X509_SAFE_SNPRINTF;

        if (crt->max_pathlen > 0) {
            ret = mbedtls_snprintf(p, n, ", max_pathlen=%d", crt->max_pathlen - 1);
            MBEDTLS_X509_SAFE_SNPRINTF;
        }
    }

    if (crt->ext_types & MBEDTLS_X509_EXT_SUBJECT_ALT_NAME) {
        ret = mbedtls_snprintf(p, n, "\n%ssubject alt name  :", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;

        if ((ret = mbedtls_x509_info_subject_alt_name(&p, &n,
                                                      &crt->subject_alt_names,
                                                      prefix)) != 0) {
            return ret;
        }
    }

    if (crt->ext_types & MBEDTLS_X509_EXT_NS_CERT_TYPE) {
        ret = mbedtls_snprintf(p, n, "\n%scert. type        : ", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;

        if ((ret = mbedtls_x509_info_cert_type(&p, &n, crt->ns_cert_type)) != 0) {
            return ret;
        }
    }

    if (crt->ext_types & MBEDTLS_X509_EXT_KEY_USAGE) {
        ret = mbedtls_snprintf(p, n, "\n%skey usage         : ", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;

        if ((ret = mbedtls_x509_info_key_usage(&p, &n, crt->key_usage)) != 0) {
            return ret;
        }
    }

    if (crt->ext_types & MBEDTLS_X509_EXT_EXTENDED_KEY_USAGE) {
        ret = mbedtls_snprintf(p, n, "\n%sext key usage     : ", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;

        if ((ret = x509_info_ext_key_usage(&p, &n,
                                           &crt->ext_key_usage)) != 0) {
            return ret;
        }
    }

    if (crt->ext_types & MBEDTLS_OID_X509_EXT_CERTIFICATE_POLICIES) {
        ret = mbedtls_snprintf(p, n, "\n%scertificate policies : ", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;

        if ((ret = x509_info_cert_policies(&p, &n,
                                           &crt->certificate_policies)) != 0) {
            return ret;
        }
    }

    ret = mbedtls_snprintf(p, n, "\n");
    MBEDTLS_X509_SAFE_SNPRINTF;

    return (int) (size - n);
}

struct x509_crt_verify_string {
    int code;
    const char *string;
};

#define X509_CRT_ERROR_INFO(err, err_str, info) { err, info },
static const struct x509_crt_verify_string x509_crt_verify_strings[] = {
    MBEDTLS_X509_CRT_ERROR_INFO_LIST
    { 0, NULL }
};
#undef X509_CRT_ERROR_INFO

int mbedtls_x509_crt_verify_info(char *buf, size_t size, const char *prefix,
                                 uint32_t flags)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const struct x509_crt_verify_string *cur;
    char *p = buf;
    size_t n = size;

    for (cur = x509_crt_verify_strings; cur->string != NULL; cur++) {
        if ((flags & cur->code) == 0) {
            continue;
        }

        ret = mbedtls_snprintf(p, n, "%s%s\n", prefix, cur->string);
        MBEDTLS_X509_SAFE_SNPRINTF;
        flags ^= cur->code;
    }

    if (flags != 0) {
        ret = mbedtls_snprintf(p, n, "%sUnknown reason "
                                     "(this should not happen)\n", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;
    }

    return (int) (size - n);
}
#endif /* MBEDTLS_X509_REMOVE_INFO */

int mbedtls_x509_crt_check_key_usage(const mbedtls_x509_crt *crt,
                                     unsigned int usage)
{
    unsigned int usage_must, usage_may;
    unsigned int may_mask = MBEDTLS_X509_KU_ENCIPHER_ONLY
                            | MBEDTLS_X509_KU_DECIPHER_ONLY;

    if ((crt->ext_types & MBEDTLS_X509_EXT_KEY_USAGE) == 0) {
        return 0;
    }

    usage_must = usage & ~may_mask;

    if (((crt->key_usage & ~may_mask) & usage_must) != usage_must) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    usage_may = usage & may_mask;

    if (((crt->key_usage & may_mask) | usage_may) != usage_may) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    return 0;
}

int mbedtls_x509_crt_check_extended_key_usage(const mbedtls_x509_crt *crt,
                                              const char *usage_oid,
                                              size_t usage_len)
{
    const mbedtls_x509_sequence *cur;

    /* Extension is not mandatory, absent means no restriction */
    if ((crt->ext_types & MBEDTLS_X509_EXT_EXTENDED_KEY_USAGE) == 0) {
        return 0;
    }

    /*
     * Look for the requested usage (or wildcard ANY) in our list
     */
    for (cur = &crt->ext_key_usage; cur != NULL; cur = cur->next) {
        const mbedtls_x509_buf *cur_oid = &cur->buf;

        if (cur_oid->len == usage_len &&
            memcmp(cur_oid->p, usage_oid, usage_len) == 0) {
            return 0;
        }

        if (MBEDTLS_OID_CMP(MBEDTLS_OID_ANY_EXTENDED_KEY_USAGE, cur_oid) == 0) {
            return 0;
        }
    }

    return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
}

#if defined(MBEDTLS_X509_CRL_PARSE_C)
/*
 * Return 1 if the certificate is revoked, or 0 otherwise.
 */
int mbedtls_x509_crt_is_revoked(const mbedtls_x509_crt *crt, const mbedtls_x509_crl *crl)
{
    const mbedtls_x509_crl_entry *cur = &crl->entry;

    while (cur != NULL && cur->serial.len != 0) {
        if (crt->serial.len == cur->serial.len &&
            memcmp(crt->serial.p, cur->serial.p, crt->serial.len) == 0) {
            return 1;
        }

        cur = cur->next;
    }

    return 0;
}

/*
 * Check that the given certificate is not revoked according to the CRL.
 * Skip validation if no CRL for the given CA is present.
 */
static int x509_crt_verifycrl(mbedtls_x509_crt *crt, mbedtls_x509_crt *ca,
                              mbedtls_x509_crl *crl_list,
                              const mbedtls_x509_crt_profile *profile,
                              const mbedtls_x509_time *now)
{
    int flags = 0;
    unsigned char hash[MBEDTLS_MD_MAX_SIZE];
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_algorithm_t psa_algorithm;
#else
    const mbedtls_md_info_t *md_info;
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    size_t hash_length;

    if (ca == NULL) {
        return flags;
    }

    while (crl_list != NULL) {
        if (crl_list->version == 0 ||
            x509_name_cmp(&crl_list->issuer, &ca->subject) != 0) {
            crl_list = crl_list->next;
            continue;
        }

        /*
         * Check if the CA is configured to sign CRLs
         */
        if (mbedtls_x509_crt_check_key_usage(ca,
                                             MBEDTLS_X509_KU_CRL_SIGN) != 0) {
            flags |= MBEDTLS_X509_BADCRL_NOT_TRUSTED;
            break;
        }

        /*
         * Check if CRL is correctly signed by the trusted CA
         */
        if (x509_profile_check_md_alg(profile, crl_list->sig_md) != 0) {
            flags |= MBEDTLS_X509_BADCRL_BAD_MD;
        }

        if (x509_profile_check_pk_alg(profile, crl_list->sig_pk) != 0) {
            flags |= MBEDTLS_X509_BADCRL_BAD_PK;
        }

#if defined(MBEDTLS_USE_PSA_CRYPTO)
        psa_algorithm = mbedtls_md_psa_alg_from_type(crl_list->sig_md);
        if (psa_hash_compute(psa_algorithm,
                             crl_list->tbs.p,
                             crl_list->tbs.len,
                             hash,
                             sizeof(hash),
                             &hash_length) != PSA_SUCCESS) {
            /* Note: this can't happen except after an internal error */
            flags |= MBEDTLS_X509_BADCRL_NOT_TRUSTED;
            break;
        }
#else
        md_info = mbedtls_md_info_from_type(crl_list->sig_md);
        hash_length = mbedtls_md_get_size(md_info);
        if (mbedtls_md(md_info,
                       crl_list->tbs.p,
                       crl_list->tbs.len,
                       hash) != 0) {
            /* Note: this can't happen except after an internal error */
            flags |= MBEDTLS_X509_BADCRL_NOT_TRUSTED;
            break;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        if (x509_profile_check_key(profile, &ca->pk) != 0) {
            flags |= MBEDTLS_X509_BADCERT_BAD_KEY;
        }

        if (mbedtls_pk_verify_ext(crl_list->sig_pk, crl_list->sig_opts, &ca->pk,
                                  crl_list->sig_md, hash, hash_length,
                                  crl_list->sig.p, crl_list->sig.len) != 0) {
            flags |= MBEDTLS_X509_BADCRL_NOT_TRUSTED;
            break;
        }

#if defined(MBEDTLS_HAVE_TIME_DATE)
        /*
         * Check for validity of CRL (Do not drop out)
         */
        if (mbedtls_x509_time_cmp(&crl_list->next_update, now) < 0) {
            flags |= MBEDTLS_X509_BADCRL_EXPIRED;
        }

        if (mbedtls_x509_time_cmp(&crl_list->this_update, now) > 0) {
            flags |= MBEDTLS_X509_BADCRL_FUTURE;
        }
#else
        ((void) now);
#endif

        /*
         * Check if certificate is revoked
         */
        if (mbedtls_x509_crt_is_revoked(crt, crl_list)) {
            flags |= MBEDTLS_X509_BADCERT_REVOKED;
            break;
        }

        crl_list = crl_list->next;
    }

    return flags;
}
#endif /* MBEDTLS_X509_CRL_PARSE_C */

/*
 * Check the signature of a certificate by its parent
 */
static int x509_crt_check_signature(const mbedtls_x509_crt *child,
                                    mbedtls_x509_crt *parent,
                                    mbedtls_x509_crt_restart_ctx *rs_ctx)
{
    size_t hash_len;
    unsigned char hash[MBEDTLS_MD_MAX_SIZE];
#if !defined(MBEDTLS_USE_PSA_CRYPTO)
    const mbedtls_md_info_t *md_info;
    md_info = mbedtls_md_info_from_type(child->sig_md);
    hash_len = mbedtls_md_get_size(md_info);

    /* Note: hash errors can happen only after an internal error */
    if (mbedtls_md(md_info, child->tbs.p, child->tbs.len, hash) != 0) {
        return -1;
    }
#else
    psa_algorithm_t hash_alg = mbedtls_md_psa_alg_from_type(child->sig_md);
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    status = psa_hash_compute(hash_alg,
                              child->tbs.p,
                              child->tbs.len,
                              hash,
                              sizeof(hash),
                              &hash_len);
    if (status != PSA_SUCCESS) {
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
    }

#endif /* MBEDTLS_USE_PSA_CRYPTO */
    /* Skip expensive computation on obvious mismatch */
    if (!mbedtls_pk_can_do(&parent->pk, child->sig_pk)) {
        return -1;
    }

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && child->sig_pk == MBEDTLS_PK_ECDSA) {
        return mbedtls_pk_verify_restartable(&parent->pk,
                                             child->sig_md, hash, hash_len,
                                             child->sig.p, child->sig.len, &rs_ctx->pk);
    }
#else
    (void) rs_ctx;
#endif

    return mbedtls_pk_verify_ext(child->sig_pk, child->sig_opts, &parent->pk,
                                 child->sig_md, hash, hash_len,
                                 child->sig.p, child->sig.len);
}

/*
 * Check if 'parent' is a suitable parent (signing CA) for 'child'.
 * Return 0 if yes, -1 if not.
 *
 * top means parent is a locally-trusted certificate
 */
static int x509_crt_check_parent(const mbedtls_x509_crt *child,
                                 const mbedtls_x509_crt *parent,
                                 int top)
{
    int need_ca_bit;

    /* Parent must be the issuer */
    if (x509_name_cmp(&child->issuer, &parent->subject) != 0) {
        return -1;
    }

    /* Parent must have the basicConstraints CA bit set as a general rule */
    need_ca_bit = 1;

    /* Exception: v1/v2 certificates that are locally trusted. */
    if (top && parent->version < 3) {
        need_ca_bit = 0;
    }

    if (need_ca_bit && !parent->ca_istrue) {
        return -1;
    }

    if (need_ca_bit &&
        mbedtls_x509_crt_check_key_usage(parent, MBEDTLS_X509_KU_KEY_CERT_SIGN) != 0) {
        return -1;
    }

    return 0;
}

/*
 * Find a suitable parent for child in candidates, or return NULL.
 *
 * Here suitable is defined as:
 *  1. subject name matches child's issuer
 *  2. if necessary, the CA bit is set and key usage allows signing certs
 *  3. for trusted roots, the signature is correct
 *     (for intermediates, the signature is checked and the result reported)
 *  4. pathlen constraints are satisfied
 *
 * If there's a suitable candidate which is also time-valid, return the first
 * such. Otherwise, return the first suitable candidate (or NULL if there is
 * none).
 *
 * The rationale for this rule is that someone could have a list of trusted
 * roots with two versions on the same root with different validity periods.
 * (At least one user reported having such a list and wanted it to just work.)
 * The reason we don't just require time-validity is that generally there is
 * only one version, and if it's expired we want the flags to state that
 * rather than NOT_TRUSTED, as would be the case if we required it here.
 *
 * The rationale for rule 3 (signature for trusted roots) is that users might
 * have two versions of the same CA with different keys in their list, and the
 * way we select the correct one is by checking the signature (as we don't
 * rely on key identifier extensions). (This is one way users might choose to
 * handle key rollover, another relies on self-issued certs, see [SIRO].)
 *
 * Arguments:
 *  - [in] child: certificate for which we're looking for a parent
 *  - [in] candidates: chained list of potential parents
 *  - [out] r_parent: parent found (or NULL)
 *  - [out] r_signature_is_good: 1 if child signature by parent is valid, or 0
 *  - [in] top: 1 if candidates consists of trusted roots, ie we're at the top
 *         of the chain, 0 otherwise
 *  - [in] path_cnt: number of intermediates seen so far
 *  - [in] self_cnt: number of self-signed intermediates seen so far
 *         (will never be greater than path_cnt)
 *  - [in-out] rs_ctx: context for restarting operations
 *
 * Return value:
 *  - 0 on success
 *  - MBEDTLS_ERR_ECP_IN_PROGRESS otherwise
 */
static int x509_crt_find_parent_in(
    mbedtls_x509_crt *child,
    mbedtls_x509_crt *candidates,
    mbedtls_x509_crt **r_parent,
    int *r_signature_is_good,
    int top,
    unsigned path_cnt,
    unsigned self_cnt,
    mbedtls_x509_crt_restart_ctx *rs_ctx,
    const mbedtls_x509_time *now)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_x509_crt *parent, *fallback_parent;
    int signature_is_good = 0, fallback_signature_is_good;

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
    /* did we have something in progress? */
    if (rs_ctx != NULL && rs_ctx->parent != NULL) {
        /* restore saved state */
        parent = rs_ctx->parent;
        fallback_parent = rs_ctx->fallback_parent;
        fallback_signature_is_good = rs_ctx->fallback_signature_is_good;

        /* clear saved state */
        rs_ctx->parent = NULL;
        rs_ctx->fallback_parent = NULL;
        rs_ctx->fallback_signature_is_good = 0;

        /* resume where we left */
        goto check_signature;
    }
#endif

    fallback_parent = NULL;
    fallback_signature_is_good = 0;

    for (parent = candidates; parent != NULL; parent = parent->next) {
        /* basic parenting skills (name, CA bit, key usage) */
        if (x509_crt_check_parent(child, parent, top) != 0) {
            continue;
        }

        /* +1 because stored max_pathlen is 1 higher that the actual value */
        if (parent->max_pathlen > 0 &&
            (size_t) parent->max_pathlen < 1 + path_cnt - self_cnt) {
            continue;
        }

        /* Signature */
#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
check_signature:
#endif
        ret = x509_crt_check_signature(child, parent, rs_ctx);

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
        if (rs_ctx != NULL && ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
            /* save state */
            rs_ctx->parent = parent;
            rs_ctx->fallback_parent = fallback_parent;
            rs_ctx->fallback_signature_is_good = fallback_signature_is_good;

            return ret;
        }
#else
        (void) ret;
#endif

        signature_is_good = ret == 0;
        if (top && !signature_is_good) {
            continue;
        }

#if defined(MBEDTLS_HAVE_TIME_DATE)
        /* optional time check */
        if (mbedtls_x509_time_cmp(&parent->valid_to, now) < 0 ||    /* past */
            mbedtls_x509_time_cmp(&parent->valid_from, now) > 0) {  /* future */
            if (fallback_parent == NULL) {
                fallback_parent = parent;
                fallback_signature_is_good = signature_is_good;
            }

            continue;
        }
#else
        ((void) now);
#endif

        *r_parent = parent;
        *r_signature_is_good = signature_is_good;

        break;
    }

    if (parent == NULL) {
        *r_parent = fallback_parent;
        *r_signature_is_good = fallback_signature_is_good;
    }

    return 0;
}

/*
 * Find a parent in trusted CAs or the provided chain, or return NULL.
 *
 * Searches in trusted CAs first, and return the first suitable parent found
 * (see find_parent_in() for definition of suitable).
 *
 * Arguments:
 *  - [in] child: certificate for which we're looking for a parent, followed
 *         by a chain of possible intermediates
 *  - [in] trust_ca: list of locally trusted certificates
 *  - [out] parent: parent found (or NULL)
 *  - [out] parent_is_trusted: 1 if returned `parent` is trusted, or 0
 *  - [out] signature_is_good: 1 if child signature by parent is valid, or 0
 *  - [in] path_cnt: number of links in the chain so far (EE -> ... -> child)
 *  - [in] self_cnt: number of self-signed certs in the chain so far
 *         (will always be no greater than path_cnt)
 *  - [in-out] rs_ctx: context for restarting operations
 *
 * Return value:
 *  - 0 on success
 *  - MBEDTLS_ERR_ECP_IN_PROGRESS otherwise
 */
static int x509_crt_find_parent(
    mbedtls_x509_crt *child,
    mbedtls_x509_crt *trust_ca,
    mbedtls_x509_crt **parent,
    int *parent_is_trusted,
    int *signature_is_good,
    unsigned path_cnt,
    unsigned self_cnt,
    mbedtls_x509_crt_restart_ctx *rs_ctx,
    const mbedtls_x509_time *now)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_x509_crt *search_list;

    *parent_is_trusted = 1;

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
    /* restore then clear saved state if we have some stored */
    if (rs_ctx != NULL && rs_ctx->parent_is_trusted != -1) {
        *parent_is_trusted = rs_ctx->parent_is_trusted;
        rs_ctx->parent_is_trusted = -1;
    }
#endif

    while (1) {
        search_list = *parent_is_trusted ? trust_ca : child->next;

        ret = x509_crt_find_parent_in(child, search_list,
                                      parent, signature_is_good,
                                      *parent_is_trusted,
                                      path_cnt, self_cnt, rs_ctx, now);

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
        if (rs_ctx != NULL && ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
            /* save state */
            rs_ctx->parent_is_trusted = *parent_is_trusted;
            return ret;
        }
#else
        (void) ret;
#endif

        /* stop here if found or already in second iteration */
        if (*parent != NULL || *parent_is_trusted == 0) {
            break;
        }

        /* prepare second iteration */
        *parent_is_trusted = 0;
    }

    /* extra precaution against mistakes in the caller */
    if (*parent == NULL) {
        *parent_is_trusted = 0;
        *signature_is_good = 0;
    }

    return 0;
}

/*
 * Check if an end-entity certificate is locally trusted
 *
 * Currently we require such certificates to be self-signed (actually only
 * check for self-issued as self-signatures are not checked)
 */
static int x509_crt_check_ee_locally_trusted(
    mbedtls_x509_crt *crt,
    mbedtls_x509_crt *trust_ca)
{
    mbedtls_x509_crt *cur;

    /* must be self-issued */
    if (x509_name_cmp(&crt->issuer, &crt->subject) != 0) {
        return -1;
    }

    /* look for an exact match with trusted cert */
    for (cur = trust_ca; cur != NULL; cur = cur->next) {
        if (crt->raw.len == cur->raw.len &&
            memcmp(crt->raw.p, cur->raw.p, crt->raw.len) == 0) {
            return 0;
        }
    }

    /* too bad */
    return -1;
}

/*
 * Build and verify a certificate chain
 *
 * Given a peer-provided list of certificates EE, C1, ..., Cn and
 * a list of trusted certs R1, ... Rp, try to build and verify a chain
 *      EE, Ci1, ... Ciq [, Rj]
 * such that every cert in the chain is a child of the next one,
 * jumping to a trusted root as early as possible.
 *
 * Verify that chain and return it with flags for all issues found.
 *
 * Special cases:
 * - EE == Rj -> return a one-element list containing it
 * - EE, Ci1, ..., Ciq cannot be continued with a trusted root
 *   -> return that chain with NOT_TRUSTED set on Ciq
 *
 * Tests for (aspects of) this function should include at least:
 * - trusted EE
 * - EE -> trusted root
 * - EE -> intermediate CA -> trusted root
 * - if relevant: EE untrusted
 * - if relevant: EE -> intermediate, untrusted
 * with the aspect under test checked at each relevant level (EE, int, root).
 * For some aspects longer chains are required, but usually length 2 is
 * enough (but length 1 is not in general).
 *
 * Arguments:
 *  - [in] crt: the cert list EE, C1, ..., Cn
 *  - [in] trust_ca: the trusted list R1, ..., Rp
 *  - [in] ca_crl, profile: as in verify_with_profile()
 *  - [out] ver_chain: the built and verified chain
 *      Only valid when return value is 0, may contain garbage otherwise!
 *      Restart note: need not be the same when calling again to resume.
 *  - [in-out] rs_ctx: context for restarting operations
 *
 * Return value:
 *  - non-zero if the chain could not be fully built and examined
 *  - 0 is the chain was successfully built and examined,
 *      even if it was found to be invalid
 */
static int x509_crt_verify_chain(
    mbedtls_x509_crt *crt,
    mbedtls_x509_crt *trust_ca,
    mbedtls_x509_crl *ca_crl,
    mbedtls_x509_crt_ca_cb_t f_ca_cb,
    void *p_ca_cb,
    const mbedtls_x509_crt_profile *profile,
    mbedtls_x509_crt_verify_chain *ver_chain,
    mbedtls_x509_crt_restart_ctx *rs_ctx)
{
    /* Don't initialize any of those variables here, so that the compiler can
     * catch potential issues with jumping ahead when restarting */
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    uint32_t *flags;
    mbedtls_x509_crt_verify_chain_item *cur;
    mbedtls_x509_crt *child;
    mbedtls_x509_crt *parent;
    int parent_is_trusted;
    int child_is_trusted;
    int signature_is_good;
    unsigned self_cnt;
    mbedtls_x509_crt *cur_trust_ca = NULL;
    mbedtls_x509_time now;

#if defined(MBEDTLS_HAVE_TIME_DATE)
    if (mbedtls_x509_time_gmtime(mbedtls_time(NULL), &now) != 0) {
        return MBEDTLS_ERR_X509_FATAL_ERROR;
    }
#endif

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
    /* resume if we had an operation in progress */
    if (rs_ctx != NULL && rs_ctx->in_progress == x509_crt_rs_find_parent) {
        /* restore saved state */
        *ver_chain = rs_ctx->ver_chain; /* struct copy */
        self_cnt = rs_ctx->self_cnt;

        /* restore derived state */
        cur = &ver_chain->items[ver_chain->len - 1];
        child = cur->crt;
        flags = &cur->flags;

        goto find_parent;
    }
#endif /* MBEDTLS_ECDSA_C && MBEDTLS_ECP_RESTARTABLE */

    child = crt;
    self_cnt = 0;
    parent_is_trusted = 0;
    child_is_trusted = 0;

    while (1) {
        /* Add certificate to the verification chain */
        cur = &ver_chain->items[ver_chain->len];
        cur->crt = child;
        cur->flags = 0;
        ver_chain->len++;
        flags = &cur->flags;

#if defined(MBEDTLS_HAVE_TIME_DATE)
        /* Check time-validity (all certificates) */
        if (mbedtls_x509_time_cmp(&child->valid_to, &now) < 0) {
            *flags |= MBEDTLS_X509_BADCERT_EXPIRED;
        }

        if (mbedtls_x509_time_cmp(&child->valid_from, &now) > 0) {
            *flags |= MBEDTLS_X509_BADCERT_FUTURE;
        }
#endif

        /* Stop here for trusted roots (but not for trusted EE certs) */
        if (child_is_trusted) {
            return 0;
        }

        /* Check signature algorithm: MD & PK algs */
        if (x509_profile_check_md_alg(profile, child->sig_md) != 0) {
            *flags |= MBEDTLS_X509_BADCERT_BAD_MD;
        }

        if (x509_profile_check_pk_alg(profile, child->sig_pk) != 0) {
            *flags |= MBEDTLS_X509_BADCERT_BAD_PK;
        }

        /* Special case: EE certs that are locally trusted */
        if (ver_chain->len == 1 &&
            x509_crt_check_ee_locally_trusted(child, trust_ca) == 0) {
            return 0;
        }

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
find_parent:
#endif

        /* Obtain list of potential trusted signers from CA callback,
         * or use statically provided list. */
#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
        if (f_ca_cb != NULL) {
            mbedtls_x509_crt_free(ver_chain->trust_ca_cb_result);
            mbedtls_free(ver_chain->trust_ca_cb_result);
            ver_chain->trust_ca_cb_result = NULL;

            ret = f_ca_cb(p_ca_cb, child, &ver_chain->trust_ca_cb_result);
            if (ret != 0) {
                return MBEDTLS_ERR_X509_FATAL_ERROR;
            }

            cur_trust_ca = ver_chain->trust_ca_cb_result;
        } else
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */
        {
            ((void) f_ca_cb);
            ((void) p_ca_cb);
            cur_trust_ca = trust_ca;
        }

        /* Look for a parent in trusted CAs or up the chain */
        ret = x509_crt_find_parent(child, cur_trust_ca, &parent,
                                   &parent_is_trusted, &signature_is_good,
                                   ver_chain->len - 1, self_cnt, rs_ctx,
                                   &now);

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
        if (rs_ctx != NULL && ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
            /* save state */
            rs_ctx->in_progress = x509_crt_rs_find_parent;
            rs_ctx->self_cnt = self_cnt;
            rs_ctx->ver_chain = *ver_chain; /* struct copy */

            return ret;
        }
#else
        (void) ret;
#endif

        /* No parent? We're done here */
        if (parent == NULL) {
            *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
            return 0;
        }

        /* Count intermediate self-issued (not necessarily self-signed) certs.
         * These can occur with some strategies for key rollover, see [SIRO],
         * and should be excluded from max_pathlen checks. */
        if (ver_chain->len != 1 &&
            x509_name_cmp(&child->issuer, &child->subject) == 0) {
            self_cnt++;
        }

        /* path_cnt is 0 for the first intermediate CA,
         * and if parent is trusted it's not an intermediate CA */
        if (!parent_is_trusted &&
            ver_chain->len > MBEDTLS_X509_MAX_INTERMEDIATE_CA) {
            /* return immediately to avoid overflow the chain array */
            return MBEDTLS_ERR_X509_FATAL_ERROR;
        }

        /* signature was checked while searching parent */
        if (!signature_is_good) {
            *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
        }

        /* check size of signing key */
        if (x509_profile_check_key(profile, &parent->pk) != 0) {
            *flags |= MBEDTLS_X509_BADCERT_BAD_KEY;
        }

#if defined(MBEDTLS_X509_CRL_PARSE_C)
        /* Check trusted CA's CRL for the given crt */
        *flags |= x509_crt_verifycrl(child, parent, ca_crl, profile, &now);
#else
        (void) ca_crl;
#endif

        /* prepare for next iteration */
        child = parent;
        parent = NULL;
        child_is_trusted = parent_is_trusted;
        signature_is_good = 0;
    }
}

#ifdef _WIN32
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <ws2tcpip.h>
#elif (defined(__MINGW32__) || defined(__MINGW64__)) && _WIN32_WINNT >= 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#else
/* inet_pton() is not supported, fallback to software version */
#define MBEDTLS_TEST_SW_INET_PTON
#endif
#elif defined(__sun)
/* Solaris requires -lsocket -lnsl for inet_pton() */
#elif defined(__has_include)
#if __has_include(<sys/socket.h>)
#include <sys/socket.h>
#endif
#if __has_include(<arpa/inet.h>)
#include <arpa/inet.h>
#endif
#endif

/* Use whether or not AF_INET6 is defined to indicate whether or not to use
 * the platform inet_pton() or a local implementation (below).  The local
 * implementation may be used even in cases where the platform provides
 * inet_pton(), e.g. when there are different includes required and/or the
 * platform implementation requires dependencies on additional libraries.
 * Specifically, Windows requires custom includes and additional link
 * dependencies, and Solaris requires additional link dependencies.
 * Also, as a coarse heuristic, use the local implementation if the compiler
 * does not support __has_include(), or if the definition of AF_INET6 is not
 * provided by headers included (or not) via __has_include() above.
 * MBEDTLS_TEST_SW_INET_PTON is a bypass define to force testing of this code //no-check-names
 * despite having a platform that has inet_pton. */
#if !defined(AF_INET6) || defined(MBEDTLS_TEST_SW_INET_PTON) //no-check-names
/* Definition located further below to possibly reduce compiler inlining */
static int x509_inet_pton_ipv4(const char *src, void *dst);

#define li_cton(c, n) \
    (((n) = (c) - '0') <= 9 || (((n) = ((c)&0xdf) - 'A') <= 5 ? ((n) += 10) : 0))

static int x509_inet_pton_ipv6(const char *src, void *dst)
{
    const unsigned char *p = (const unsigned char *) src;
    int nonzero_groups = 0, num_digits, zero_group_start = -1;
    uint16_t addr[8];
    do {
        /* note: allows excess leading 0's, e.g. 1:0002:3:... */
        uint16_t group = num_digits = 0;
        for (uint8_t digit; num_digits < 4; num_digits++) {
            if (li_cton(*p, digit) == 0) {
                break;
            }
            group = (group << 4) | digit;
            p++;
        }
        if (num_digits != 0) {
            MBEDTLS_PUT_UINT16_BE(group, addr, nonzero_groups);
            nonzero_groups++;
            if (*p == '\0') {
                break;
            } else if (*p == '.') {
                /* Don't accept IPv4 too early or late */
                if ((nonzero_groups == 0 && zero_group_start == -1) ||
                    nonzero_groups >= 7) {
                    break;
                }

                /* Walk back to prior ':', then parse as IPv4-mapped */
                int steps = 4;
                do {
                    p--;
                    steps--;
                } while (*p != ':' && steps > 0);

                if (*p != ':') {
                    break;
                }
                p++;
                nonzero_groups--;
                if (x509_inet_pton_ipv4((const char *) p,
                                        addr + nonzero_groups) != 0) {
                    break;
                }

                nonzero_groups += 2;
                p = (const unsigned char *) "";
                break;
            } else if (*p != ':') {
                return -1;
            }
        } else {
            /* Don't accept a second zero group or an invalid delimiter */
            if (zero_group_start != -1 || *p != ':') {
                return -1;
            }
            zero_group_start = nonzero_groups;

            /* Accept a zero group at start, but it has to be a double colon */
            if (zero_group_start == 0 && *++p != ':') {
                return -1;
            }

            if (p[1] == '\0') {
                ++p;
                break;
            }
        }
        ++p;
    } while (nonzero_groups < 8);

    if (*p != '\0') {
        return -1;
    }

    if (zero_group_start != -1) {
        if (nonzero_groups > 6) {
            return -1;
        }
        int zero_groups = 8 - nonzero_groups;
        int groups_after_zero = nonzero_groups - zero_group_start;

        /* Move the non-zero part to after the zeroes */
        if (groups_after_zero) {
            memmove(addr + zero_group_start + zero_groups,
                    addr + zero_group_start,
                    groups_after_zero * sizeof(*addr));
        }
        memset(addr + zero_group_start, 0, zero_groups * sizeof(*addr));
    } else {
        if (nonzero_groups != 8) {
            return -1;
        }
    }
    memcpy(dst, addr, sizeof(addr));
    return 0;
}

static int x509_inet_pton_ipv4(const char *src, void *dst)
{
    const unsigned char *p = (const unsigned char *) src;
    uint8_t *res = (uint8_t *) dst;
    uint8_t digit, num_digits = 0;
    uint8_t num_octets = 0;
    uint16_t octet;

    do {
        octet = num_digits = 0;
        do {
            digit = *p - '0';
            if (digit > 9) {
                break;
            }

            /* Don't allow leading zeroes. These might mean octal format,
             * which this implementation does not support. */
            if (octet == 0 && num_digits > 0) {
                return -1;
            }

            octet = octet * 10 + digit;
            num_digits++;
            p++;
        } while (num_digits < 3);

        if (octet >= 256 || num_digits > 3 || num_digits == 0) {
            return -1;
        }
        *res++ = (uint8_t) octet;
        num_octets++;
    } while (num_octets < 4 && *p++ == '.');
    return num_octets == 4 && *p == '\0' ? 0 : -1;
}

#else

static int x509_inet_pton_ipv6(const char *src, void *dst)
{
    return inet_pton(AF_INET6, src, dst) == 1 ? 0 : -1;
}

static int x509_inet_pton_ipv4(const char *src, void *dst)
{
    return inet_pton(AF_INET, src, dst) == 1 ? 0 : -1;
}

#endif /* !AF_INET6 || MBEDTLS_TEST_SW_INET_PTON */ //no-check-names

size_t mbedtls_x509_crt_parse_cn_inet_pton(const char *cn, void *dst)
{
    return strchr(cn, ':') == NULL
            ? x509_inet_pton_ipv4(cn, dst) == 0 ? 4 : 0
            : x509_inet_pton_ipv6(cn, dst) == 0 ? 16 : 0;
}

/*
 * Check for CN match
 */
static int x509_crt_check_cn(const mbedtls_x509_buf *name,
                             const char *cn, size_t cn_len)
{
    /* try exact match */
    if (name->len == cn_len &&
        x509_memcasecmp(cn, name->p, cn_len) == 0) {
        return 0;
    }

    /* try wildcard match */
    if (x509_check_wildcard(cn, name) == 0) {
        return 0;
    }

    return -1;
}

static int x509_crt_check_san_ip(const mbedtls_x509_sequence *san,
                                 const char *cn, size_t cn_len)
{
    uint32_t ip[4];
    cn_len = mbedtls_x509_crt_parse_cn_inet_pton(cn, ip);
    if (cn_len == 0) {
        return -1;
    }

    for (const mbedtls_x509_sequence *cur = san; cur != NULL; cur = cur->next) {
        const unsigned char san_type = (unsigned char) cur->buf.tag &
                                       MBEDTLS_ASN1_TAG_VALUE_MASK;
        if (san_type == MBEDTLS_X509_SAN_IP_ADDRESS &&
            cur->buf.len == cn_len && memcmp(cur->buf.p, ip, cn_len) == 0) {
            return 0;
        }
    }

    return -1;
}

static int x509_crt_check_san_uri(const mbedtls_x509_sequence *san,
                                  const char *cn, size_t cn_len)
{
    for (const mbedtls_x509_sequence *cur = san; cur != NULL; cur = cur->next) {
        const unsigned char san_type = (unsigned char) cur->buf.tag &
                                       MBEDTLS_ASN1_TAG_VALUE_MASK;
        if (san_type == MBEDTLS_X509_SAN_UNIFORM_RESOURCE_IDENTIFIER &&
            cur->buf.len == cn_len && memcmp(cur->buf.p, cn, cn_len) == 0) {
            return 0;
        }
    }

    return -1;
}

/*
 * Check for SAN match, see RFC 5280 Section 4.2.1.6
 */
static int x509_crt_check_san(const mbedtls_x509_sequence *san,
                              const char *cn, size_t cn_len)
{
    int san_ip = 0;
    int san_uri = 0;
    /* Prioritize DNS name over other subtypes due to popularity */
    for (const mbedtls_x509_sequence *cur = san; cur != NULL; cur = cur->next) {
        switch ((unsigned char) cur->buf.tag & MBEDTLS_ASN1_TAG_VALUE_MASK) {
            case MBEDTLS_X509_SAN_DNS_NAME:
                if (x509_crt_check_cn(&cur->buf, cn, cn_len) == 0) {
                    return 0;
                }
                break;
            case MBEDTLS_X509_SAN_IP_ADDRESS:
                san_ip = 1;
                break;
            case MBEDTLS_X509_SAN_UNIFORM_RESOURCE_IDENTIFIER:
                san_uri = 1;
                break;
            /* (We may handle other types here later.) */
            default: /* Unrecognized type */
                break;
        }
    }
    if (san_ip) {
        if (x509_crt_check_san_ip(san, cn, cn_len) == 0) {
            return 0;
        }
    }
    if (san_uri) {
        if (x509_crt_check_san_uri(san, cn, cn_len) == 0) {
            return 0;
        }
    }

    return -1;
}

/*
 * Verify the requested CN - only call this if cn is not NULL!
 */
static void x509_crt_verify_name(const mbedtls_x509_crt *crt,
                                 const char *cn,
                                 uint32_t *flags)
{
    const mbedtls_x509_name *name;
    size_t cn_len = strlen(cn);

    if (crt->ext_types & MBEDTLS_X509_EXT_SUBJECT_ALT_NAME) {
        if (x509_crt_check_san(&crt->subject_alt_names, cn, cn_len) == 0) {
            return;
        }
    } else {
        for (name = &crt->subject; name != NULL; name = name->next) {
            if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid) == 0 &&
                x509_crt_check_cn(&name->val, cn, cn_len) == 0) {
                return;
            }
        }

    }

    *flags |= MBEDTLS_X509_BADCERT_CN_MISMATCH;
}

/*
 * Merge the flags for all certs in the chain, after calling callback
 */
static int x509_crt_merge_flags_with_cb(
    uint32_t *flags,
    const mbedtls_x509_crt_verify_chain *ver_chain,
    int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
    void *p_vrfy)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned i;
    uint32_t cur_flags;
    const mbedtls_x509_crt_verify_chain_item *cur;

    for (i = ver_chain->len; i != 0; --i) {
        cur = &ver_chain->items[i-1];
        cur_flags = cur->flags;

        if (NULL != f_vrfy) {
            if ((ret = f_vrfy(p_vrfy, cur->crt, (int) i-1, &cur_flags)) != 0) {
                return ret;
            }
        }

        *flags |= cur_flags;
    }

    return 0;
}

/*
 * Verify the certificate validity, with profile, restartable version
 *
 * This function:
 *  - checks the requested CN (if any)
 *  - checks the type and size of the EE cert's key,
 *    as that isn't done as part of chain building/verification currently
 *  - builds and verifies the chain
 *  - then calls the callback and merges the flags
 *
 * The parameters pairs `trust_ca`, `ca_crl` and `f_ca_cb`, `p_ca_cb`
 * are mutually exclusive: If `f_ca_cb != NULL`, it will be used by the
 * verification routine to search for trusted signers, and CRLs will
 * be disabled. Otherwise, `trust_ca` will be used as the static list
 * of trusted signers, and `ca_crl` will be use as the static list
 * of CRLs.
 */
static int x509_crt_verify_restartable_ca_cb(mbedtls_x509_crt *crt,
                                             mbedtls_x509_crt *trust_ca,
                                             mbedtls_x509_crl *ca_crl,
                                             mbedtls_x509_crt_ca_cb_t f_ca_cb,
                                             void *p_ca_cb,
                                             const mbedtls_x509_crt_profile *profile,
                                             const char *cn, uint32_t *flags,
                                             int (*f_vrfy)(void *,
                                                           mbedtls_x509_crt *,
                                                           int,
                                                           uint32_t *),
                                             void *p_vrfy,
                                             mbedtls_x509_crt_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_pk_type_t pk_type;
    mbedtls_x509_crt_verify_chain ver_chain;
    uint32_t ee_flags;

    *flags = 0;
    ee_flags = 0;
    x509_crt_verify_chain_reset(&ver_chain);

    if (profile == NULL) {
        ret = MBEDTLS_ERR_X509_BAD_INPUT_DATA;
        goto exit;
    }

    /* check name if requested */
    if (cn != NULL) {
        x509_crt_verify_name(crt, cn, &ee_flags);
    }

    /* Check the type and size of the key */
    pk_type = mbedtls_pk_get_type(&crt->pk);

    if (x509_profile_check_pk_alg(profile, pk_type) != 0) {
        ee_flags |= MBEDTLS_X509_BADCERT_BAD_PK;
    }

    if (x509_profile_check_key(profile, &crt->pk) != 0) {
        ee_flags |= MBEDTLS_X509_BADCERT_BAD_KEY;
    }

    /* Check the chain */
    ret = x509_crt_verify_chain(crt, trust_ca, ca_crl,
                                f_ca_cb, p_ca_cb, profile,
                                &ver_chain, rs_ctx);

    if (ret != 0) {
        goto exit;
    }

    /* Merge end-entity flags */
    ver_chain.items[0].flags |= ee_flags;

    /* Build final flags, calling callback on the way if any */
    ret = x509_crt_merge_flags_with_cb(flags, &ver_chain, f_vrfy, p_vrfy);

exit:

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
    mbedtls_x509_crt_free(ver_chain.trust_ca_cb_result);
    mbedtls_free(ver_chain.trust_ca_cb_result);
    ver_chain.trust_ca_cb_result = NULL;
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && ret != MBEDTLS_ERR_ECP_IN_PROGRESS) {
        mbedtls_x509_crt_restart_free(rs_ctx);
    }
#endif

    /* prevent misuse of the vrfy callback - VERIFY_FAILED would be ignored by
     * the SSL module for authmode optional, but non-zero return from the
     * callback means a fatal error so it shouldn't be ignored */
    if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
        ret = MBEDTLS_ERR_X509_FATAL_ERROR;
    }

    if (ret != 0) {
        *flags = (uint32_t) -1;
        return ret;
    }

    if (*flags != 0) {
        return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    }

    return 0;
}


/*
 * Verify the certificate validity (default profile, not restartable)
 */
int mbedtls_x509_crt_verify(mbedtls_x509_crt *crt,
                            mbedtls_x509_crt *trust_ca,
                            mbedtls_x509_crl *ca_crl,
                            const char *cn, uint32_t *flags,
                            int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
                            void *p_vrfy)
{
    return x509_crt_verify_restartable_ca_cb(crt, trust_ca, ca_crl,
                                             NULL, NULL,
                                             &mbedtls_x509_crt_profile_default,
                                             cn, flags,
                                             f_vrfy, p_vrfy, NULL);
}

/*
 * Verify the certificate validity (user-chosen profile, not restartable)
 */
int mbedtls_x509_crt_verify_with_profile(mbedtls_x509_crt *crt,
                                         mbedtls_x509_crt *trust_ca,
                                         mbedtls_x509_crl *ca_crl,
                                         const mbedtls_x509_crt_profile *profile,
                                         const char *cn, uint32_t *flags,
                                         int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
                                         void *p_vrfy)
{
    return x509_crt_verify_restartable_ca_cb(crt, trust_ca, ca_crl,
                                             NULL, NULL,
                                             profile, cn, flags,
                                             f_vrfy, p_vrfy, NULL);
}

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
/*
 * Verify the certificate validity (user-chosen profile, CA callback,
 *                                  not restartable).
 */
int mbedtls_x509_crt_verify_with_ca_cb(mbedtls_x509_crt *crt,
                                       mbedtls_x509_crt_ca_cb_t f_ca_cb,
                                       void *p_ca_cb,
                                       const mbedtls_x509_crt_profile *profile,
                                       const char *cn, uint32_t *flags,
                                       int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
                                       void *p_vrfy)
{
    return x509_crt_verify_restartable_ca_cb(crt, NULL, NULL,
                                             f_ca_cb, p_ca_cb,
                                             profile, cn, flags,
                                             f_vrfy, p_vrfy, NULL);
}
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */

int mbedtls_x509_crt_verify_restartable(mbedtls_x509_crt *crt,
                                        mbedtls_x509_crt *trust_ca,
                                        mbedtls_x509_crl *ca_crl,
                                        const mbedtls_x509_crt_profile *profile,
                                        const char *cn, uint32_t *flags,
                                        int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
                                        void *p_vrfy,
                                        mbedtls_x509_crt_restart_ctx *rs_ctx)
{
    return x509_crt_verify_restartable_ca_cb(crt, trust_ca, ca_crl,
                                             NULL, NULL,
                                             profile, cn, flags,
                                             f_vrfy, p_vrfy, rs_ctx);
}


/*
 * Initialize a certificate chain
 */
void mbedtls_x509_crt_init(mbedtls_x509_crt *crt)
{
    memset(crt, 0, sizeof(mbedtls_x509_crt));
}

/*
 * Unallocate all certificate data
 */
void mbedtls_x509_crt_free(mbedtls_x509_crt *crt)
{
    mbedtls_x509_crt *cert_cur = crt;
    mbedtls_x509_crt *cert_prv;

    while (cert_cur != NULL) {
        mbedtls_pk_free(&cert_cur->pk);

#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT)
        mbedtls_free(cert_cur->sig_opts);
#endif

        mbedtls_asn1_free_named_data_list_shallow(cert_cur->issuer.next);
        mbedtls_asn1_free_named_data_list_shallow(cert_cur->subject.next);
        mbedtls_asn1_sequence_free(cert_cur->ext_key_usage.next);
        mbedtls_asn1_sequence_free(cert_cur->subject_alt_names.next);
        mbedtls_asn1_sequence_free(cert_cur->certificate_policies.next);
        mbedtls_asn1_sequence_free(cert_cur->authority_key_id.authorityCertIssuer.next);

        if (cert_cur->raw.p != NULL && cert_cur->own_buffer) {
            mbedtls_zeroize_and_free(cert_cur->raw.p, cert_cur->raw.len);
        }

        cert_prv = cert_cur;
        cert_cur = cert_cur->next;

        mbedtls_platform_zeroize(cert_prv, sizeof(mbedtls_x509_crt));
        if (cert_prv != crt) {
            mbedtls_free(cert_prv);
        }
    }
}

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
/*
 * Initialize a restart context
 */
void mbedtls_x509_crt_restart_init(mbedtls_x509_crt_restart_ctx *ctx)
{
    mbedtls_pk_restart_init(&ctx->pk);

    ctx->parent = NULL;
    ctx->fallback_parent = NULL;
    ctx->fallback_signature_is_good = 0;

    ctx->parent_is_trusted = -1;

    ctx->in_progress = x509_crt_rs_none;
    ctx->self_cnt = 0;
    x509_crt_verify_chain_reset(&ctx->ver_chain);
}

/*
 * Free the components of a restart context
 */
void mbedtls_x509_crt_restart_free(mbedtls_x509_crt_restart_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_pk_restart_free(&ctx->pk);
    mbedtls_x509_crt_restart_init(ctx);
}
#endif /* MBEDTLS_ECDSA_C && MBEDTLS_ECP_RESTARTABLE */

int mbedtls_x509_crt_get_ca_istrue(const mbedtls_x509_crt *crt)
{
    if ((crt->ext_types & MBEDTLS_X509_EXT_BASIC_CONSTRAINTS) != 0) {
        return crt->MBEDTLS_PRIVATE(ca_istrue);
    }
    return MBEDTLS_ERR_X509_INVALID_EXTENSIONS;
}

#endif /* MBEDTLS_X509_CRT_PARSE_C */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#include "common.h"

#include "mbedtls/build_info.h"
#if defined(MBEDTLS_PKCS7_C)
#include "mbedtls/pkcs7.h"
#include "x509_internal.h"
#include "mbedtls/asn1.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509_crl.h"
#include "mbedtls/oid.h"
#include "mbedtls/error.h"

#if defined(MBEDTLS_FS_IO)
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_HAVE_TIME)
#include "mbedtls/platform_time.h"
#endif
#if defined(MBEDTLS_HAVE_TIME_DATE)
#include <time.h>
#endif

/**
 * Initializes the mbedtls_pkcs7 structure.
 */
void mbedtls_pkcs7_init(mbedtls_pkcs7 *pkcs7)
{
    memset(pkcs7, 0, sizeof(*pkcs7));
}

static int pkcs7_get_next_content_len(unsigned char **p, unsigned char *end,
                                      size_t *len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = mbedtls_asn1_get_tag(p, end, len, MBEDTLS_ASN1_CONSTRUCTED
                               | MBEDTLS_ASN1_CONTEXT_SPECIFIC);
    if (ret != 0) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO, ret);
    } else if ((size_t) (end - *p) != *len) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO,
                                MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return ret;
}

/**
 * version Version
 * Version ::= INTEGER
 **/
static int pkcs7_get_version(unsigned char **p, unsigned char *end, int *ver)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = mbedtls_asn1_get_int(p, end, ver);
    if (ret != 0) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_VERSION, ret);
    }

    /* If version != 1, return invalid version */
    if (*ver != MBEDTLS_PKCS7_SUPPORTED_VERSION) {
        ret = MBEDTLS_ERR_PKCS7_INVALID_VERSION;
    }

    return ret;
}

/**
 * ContentInfo ::= SEQUENCE {
 *      contentType ContentType,
 *      content
 *              [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL }
 **/
static int pkcs7_get_content_info_type(unsigned char **p, unsigned char *end,
                                       unsigned char **seq_end,
                                       mbedtls_pkcs7_buf *pkcs7)
{
    size_t len = 0;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *start = *p;

    ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_CONSTRUCTED
                               | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        *p = start;
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO, ret);
    }
    *seq_end = *p + len;
    ret = mbedtls_asn1_get_tag(p, *seq_end, &len, MBEDTLS_ASN1_OID);
    if (ret != 0) {
        *p = start;
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO, ret);
    }

    pkcs7->tag = MBEDTLS_ASN1_OID;
    pkcs7->len = len;
    pkcs7->p = *p;
    *p += len;

    return ret;
}

/**
 * DigestAlgorithmIdentifier ::= AlgorithmIdentifier
 *
 * This is from x509.h
 **/
static int pkcs7_get_digest_algorithm(unsigned char **p, unsigned char *end,
                                      mbedtls_x509_buf *alg)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if ((ret = mbedtls_asn1_get_alg_null(p, end, alg)) != 0) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_ALG, ret);
    }

    return ret;
}

/**
 * DigestAlgorithmIdentifiers :: SET of DigestAlgorithmIdentifier
 **/
static int pkcs7_get_digest_algorithm_set(unsigned char **p,
                                          unsigned char *end,
                                          mbedtls_x509_buf *alg)
{
    size_t len = 0;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_CONSTRUCTED
                               | MBEDTLS_ASN1_SET);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_ALG, ret);
    }

    end = *p + len;

    ret = mbedtls_asn1_get_alg_null(p, end, alg);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_ALG, ret);
    }

    /** For now, it assumes there is only one digest algorithm specified **/
    if (*p != end) {
        return MBEDTLS_ERR_PKCS7_FEATURE_UNAVAILABLE;
    }

    return 0;
}

/**
 * certificates :: SET OF ExtendedCertificateOrCertificate,
 * ExtendedCertificateOrCertificate ::= CHOICE {
 *      certificate Certificate -- x509,
 *      extendedCertificate[0] IMPLICIT ExtendedCertificate }
 * Return number of certificates added to the signed data,
 * 0 or higher is valid.
 * Return negative error code for failure.
 **/
static int pkcs7_get_certificates(unsigned char **p, unsigned char *end,
                                  mbedtls_x509_crt *certs)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len1 = 0;
    size_t len2 = 0;
    unsigned char *end_set, *end_cert, *start;

    ret = mbedtls_asn1_get_tag(p, end, &len1, MBEDTLS_ASN1_CONSTRUCTED
                               | MBEDTLS_ASN1_CONTEXT_SPECIFIC);
    if (ret == MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
        return 0;
    }
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_FORMAT, ret);
    }
    start = *p;
    end_set = *p + len1;

    ret = mbedtls_asn1_get_tag(p, end_set, &len2, MBEDTLS_ASN1_CONSTRUCTED
                               | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_CERT, ret);
    }

    end_cert = *p + len2;

    /*
     * This is to verify that there is only one signer certificate. It seems it is
     * not easy to differentiate between the chain vs different signer's certificate.
     * So, we support only the root certificate and the single signer.
     * The behaviour would be improved with addition of multiple signer support.
     */
    if (end_cert != end_set) {
        return MBEDTLS_ERR_PKCS7_FEATURE_UNAVAILABLE;
    }

    if ((ret = mbedtls_x509_crt_parse_der(certs, start, len1)) < 0) {
        return MBEDTLS_ERR_PKCS7_INVALID_CERT;
    }

    *p = end_cert;

    /*
     * Since in this version we strictly support single certificate, and reaching
     * here implies we have parsed successfully, we return 1.
     */
    return 1;
}

/**
 * EncryptedDigest ::= OCTET STRING
 **/
static int pkcs7_get_signature(unsigned char **p, unsigned char *end,
                               mbedtls_pkcs7_buf *signature)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING);
    if (ret != 0) {
        return ret;
    }

    signature->tag = MBEDTLS_ASN1_OCTET_STRING;
    signature->len = len;
    signature->p = *p;

    *p = *p + len;

    return 0;
}

static void pkcs7_free_signer_info(mbedtls_pkcs7_signer_info *signer)
{
    mbedtls_x509_name *name_cur;
    mbedtls_x509_name *name_prv;

    if (signer == NULL) {
        return;
    }

    name_cur = signer->issuer.next;
    while (name_cur != NULL) {
        name_prv = name_cur;
        name_cur = name_cur->next;
        mbedtls_free(name_prv);
    }
    signer->issuer.next = NULL;
}

/**
 * SignerInfo ::= SEQUENCE {
 *      version Version;
 *      issuerAndSerialNumber   IssuerAndSerialNumber,
 *      digestAlgorithm DigestAlgorithmIdentifier,
 *      authenticatedAttributes
 *              [0] IMPLICIT Attributes OPTIONAL,
 *      digestEncryptionAlgorithm DigestEncryptionAlgorithmIdentifier,
 *      encryptedDigest EncryptedDigest,
 *      unauthenticatedAttributes
 *              [1] IMPLICIT Attributes OPTIONAL,
 * Returns 0 if the signerInfo is valid.
 * Return negative error code for failure.
 * Structure must not contain vales for authenticatedAttributes
 * and unauthenticatedAttributes.
 **/
static int pkcs7_get_signer_info(unsigned char **p, unsigned char *end,
                                 mbedtls_pkcs7_signer_info *signer,
                                 mbedtls_x509_buf *alg)
{
    unsigned char *end_signer, *end_issuer_and_sn;
    int asn1_ret = 0, ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    asn1_ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_CONSTRUCTED
                                    | MBEDTLS_ASN1_SEQUENCE);
    if (asn1_ret != 0) {
        goto out;
    }

    end_signer = *p + len;

    ret = pkcs7_get_version(p, end_signer, &signer->version);
    if (ret != 0) {
        goto out;
    }

    asn1_ret = mbedtls_asn1_get_tag(p, end_signer, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (asn1_ret != 0) {
        goto out;
    }

    end_issuer_and_sn = *p + len;
    /* Parsing IssuerAndSerialNumber */
    signer->issuer_raw.p = *p;

    asn1_ret = mbedtls_asn1_get_tag(p, end_issuer_and_sn, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (asn1_ret != 0) {
        goto out;
    }

    ret  = mbedtls_x509_get_name(p, *p + len, &signer->issuer);
    if (ret != 0) {
        goto out;
    }

    signer->issuer_raw.len =  (size_t) (*p - signer->issuer_raw.p);

    ret = mbedtls_x509_get_serial(p, end_issuer_and_sn, &signer->serial);
    if (ret != 0) {
        goto out;
    }

    /* ensure no extra or missing bytes */
    if (*p != end_issuer_and_sn) {
        ret = MBEDTLS_ERR_PKCS7_INVALID_SIGNER_INFO;
        goto out;
    }

    ret = pkcs7_get_digest_algorithm(p, end_signer, &signer->alg_identifier);
    if (ret != 0) {
        goto out;
    }

    /* Check that the digest algorithm used matches the one provided earlier */
    if (signer->alg_identifier.tag != alg->tag ||
        signer->alg_identifier.len != alg->len ||
        memcmp(signer->alg_identifier.p, alg->p, alg->len) != 0) {
        ret = MBEDTLS_ERR_PKCS7_INVALID_SIGNER_INFO;
        goto out;
    }

    /* Assume authenticatedAttributes is nonexistent */
    ret = pkcs7_get_digest_algorithm(p, end_signer, &signer->sig_alg_identifier);
    if (ret != 0) {
        goto out;
    }

    ret = pkcs7_get_signature(p, end_signer, &signer->sig);
    if (ret != 0) {
        goto out;
    }

    /* Do not permit any unauthenticated attributes */
    if (*p != end_signer) {
        ret = MBEDTLS_ERR_PKCS7_INVALID_SIGNER_INFO;
    }

out:
    if (asn1_ret != 0 || ret != 0) {
        pkcs7_free_signer_info(signer);
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_SIGNER_INFO,
                                asn1_ret);
    }

    return ret;
}

/**
 * SignerInfos ::= SET of SignerInfo
 * Return number of signers added to the signed data,
 * 0 or higher is valid.
 * Return negative error code for failure.
 **/
static int pkcs7_get_signers_info_set(unsigned char **p, unsigned char *end,
                                      mbedtls_pkcs7_signer_info *signers_set,
                                      mbedtls_x509_buf *digest_alg)
{
    unsigned char *end_set;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int count = 0;
    size_t len = 0;

    ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_CONSTRUCTED
                               | MBEDTLS_ASN1_SET);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_SIGNER_INFO, ret);
    }

    /* Detect zero signers */
    if (len == 0) {
        return 0;
    }

    end_set = *p + len;

    ret = pkcs7_get_signer_info(p, end_set, signers_set, digest_alg);
    if (ret != 0) {
        return ret;
    }
    count++;

    mbedtls_pkcs7_signer_info *prev = signers_set;
    while (*p != end_set) {
        mbedtls_pkcs7_signer_info *signer =
            mbedtls_calloc(1, sizeof(mbedtls_pkcs7_signer_info));
        if (!signer) {
            ret = MBEDTLS_ERR_PKCS7_ALLOC_FAILED;
            goto cleanup;
        }

        ret = pkcs7_get_signer_info(p, end_set, signer, digest_alg);
        if (ret != 0) {
            mbedtls_free(signer);
            goto cleanup;
        }
        prev->next = signer;
        prev = signer;
        count++;
    }

    return count;

cleanup:
    pkcs7_free_signer_info(signers_set);
    mbedtls_pkcs7_signer_info *signer = signers_set->next;
    while (signer != NULL) {
        prev = signer;
        signer = signer->next;
        pkcs7_free_signer_info(prev);
        mbedtls_free(prev);
    }
    signers_set->next = NULL;
    return ret;
}

/**
 * SignedData ::= SEQUENCE {
 *      version Version,
 *      digestAlgorithms DigestAlgorithmIdentifiers,
 *      contentInfo ContentInfo,
 *      certificates
 *              [0] IMPLICIT ExtendedCertificatesAndCertificates
 *                  OPTIONAL,
 *      crls
 *              [0] IMPLICIT CertificateRevocationLists OPTIONAL,
 *      signerInfos SignerInfos }
 */
static int pkcs7_get_signed_data(unsigned char *buf, size_t buflen,
                                 mbedtls_pkcs7_signed_data *signed_data)
{
    unsigned char *p = buf;
    unsigned char *end = buf + buflen;
    unsigned char *end_content_info = NULL;
    size_t len = 0;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_md_type_t md_alg;

    ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_CONSTRUCTED
                               | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_FORMAT, ret);
    }

    if (p + len != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    /* Get version of signed data */
    ret = pkcs7_get_version(&p, end, &signed_data->version);
    if (ret != 0) {
        return ret;
    }

    /* Get digest algorithm */
    ret = pkcs7_get_digest_algorithm_set(&p, end,
                                         &signed_data->digest_alg_identifiers);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_oid_get_md_alg(&signed_data->digest_alg_identifiers, &md_alg);
    if (ret != 0) {
        return MBEDTLS_ERR_PKCS7_INVALID_ALG;
    }

    mbedtls_pkcs7_buf content_type;
    memset(&content_type, 0, sizeof(content_type));
    ret = pkcs7_get_content_info_type(&p, end, &end_content_info, &content_type);
    if (ret != 0) {
        return ret;
    }
    if (MBEDTLS_OID_CMP(MBEDTLS_OID_PKCS7_DATA, &content_type)) {
        return MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO;
    }

    if (p != end_content_info) {
        /* Determine if valid content is present */
        ret = mbedtls_asn1_get_tag(&p,
                                   end_content_info,
                                   &len,
                                   MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC);
        if (ret != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO, ret);
        }
        p += len;
        if (p != end_content_info) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO, ret);
        }
        /* Valid content is present - this is not supported */
        return MBEDTLS_ERR_PKCS7_FEATURE_UNAVAILABLE;
    }

    /* Look for certificates, there may or may not be any */
    mbedtls_x509_crt_init(&signed_data->certs);
    ret = pkcs7_get_certificates(&p, end, &signed_data->certs);
    if (ret < 0) {
        return ret;
    }

    signed_data->no_of_certs = ret;

    /*
     * Currently CRLs are not supported. If CRL exist, the parsing will fail
     * at next step of getting signers info and return error as invalid
     * signer info.
     */

    signed_data->no_of_crls = 0;

    /* Get signers info */
    ret = pkcs7_get_signers_info_set(&p,
                                     end,
                                     &signed_data->signers,
                                     &signed_data->digest_alg_identifiers);
    if (ret < 0) {
        return ret;
    }

    signed_data->no_of_signers = ret;

    /* Don't permit trailing data */
    if (p != end) {
        return MBEDTLS_ERR_PKCS7_INVALID_FORMAT;
    }

    return 0;
}

int mbedtls_pkcs7_parse_der(mbedtls_pkcs7 *pkcs7, const unsigned char *buf,
                            const size_t buflen)
{
    unsigned char *p;
    unsigned char *end;
    size_t len = 0;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (pkcs7 == NULL) {
        return MBEDTLS_ERR_PKCS7_BAD_INPUT_DATA;
    }

    /* make an internal copy of the buffer for parsing */
    pkcs7->raw.p = p = mbedtls_calloc(1, buflen);
    if (pkcs7->raw.p == NULL) {
        ret = MBEDTLS_ERR_PKCS7_ALLOC_FAILED;
        goto out;
    }
    memcpy(p, buf, buflen);
    pkcs7->raw.len = buflen;
    end = p + buflen;

    ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_CONSTRUCTED
                               | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_FORMAT, ret);
        goto out;
    }

    if ((size_t) (end - p) != len) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS7_INVALID_FORMAT,
                                MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
        goto out;
    }

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OID)) != 0) {
        if (ret != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
            goto out;
        }
        p = pkcs7->raw.p;
        len = buflen;
        goto try_data;
    }

    if (MBEDTLS_OID_CMP_RAW(MBEDTLS_OID_PKCS7_SIGNED_DATA, p, len)) {
        /* OID is not MBEDTLS_OID_PKCS7_SIGNED_DATA, which is the only supported feature */
        if (!MBEDTLS_OID_CMP_RAW(MBEDTLS_OID_PKCS7_DATA, p, len)
            || !MBEDTLS_OID_CMP_RAW(MBEDTLS_OID_PKCS7_ENCRYPTED_DATA, p, len)
            || !MBEDTLS_OID_CMP_RAW(MBEDTLS_OID_PKCS7_ENVELOPED_DATA, p, len)
            || !MBEDTLS_OID_CMP_RAW(MBEDTLS_OID_PKCS7_SIGNED_AND_ENVELOPED_DATA, p, len)
            || !MBEDTLS_OID_CMP_RAW(MBEDTLS_OID_PKCS7_DIGESTED_DATA, p, len)) {
            /* OID is valid according to the spec, but unsupported */
            ret =  MBEDTLS_ERR_PKCS7_FEATURE_UNAVAILABLE;
        } else {
            /* OID is invalid according to the spec */
            ret = MBEDTLS_ERR_PKCS7_BAD_INPUT_DATA;
        }
        goto out;
    }

    p += len;

    ret = pkcs7_get_next_content_len(&p, end, &len);
    if (ret != 0) {
        goto out;
    }

    /* ensure no extra/missing data */
    if (p + len != end) {
        ret = MBEDTLS_ERR_PKCS7_BAD_INPUT_DATA;
        goto out;
    }

try_data:
    ret = pkcs7_get_signed_data(p, len, &pkcs7->signed_data);
    if (ret != 0) {
        goto out;
    }

    ret = MBEDTLS_PKCS7_SIGNED_DATA;

out:
    if (ret < 0) {
        mbedtls_pkcs7_free(pkcs7);
    }

    return ret;
}

static int mbedtls_pkcs7_data_or_hash_verify(mbedtls_pkcs7 *pkcs7,
                                             const mbedtls_x509_crt *cert,
                                             const unsigned char *data,
                                             size_t datalen,
                                             const int is_data_hash)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *hash;
    mbedtls_pk_context pk_cxt = cert->pk;
    const mbedtls_md_info_t *md_info;
    mbedtls_md_type_t md_alg;
    mbedtls_pkcs7_signer_info *signer;

    if (pkcs7->signed_data.no_of_signers == 0) {
        return MBEDTLS_ERR_PKCS7_INVALID_CERT;
    }

    if (mbedtls_x509_time_is_past(&cert->valid_to) ||
        mbedtls_x509_time_is_future(&cert->valid_from)) {
        return MBEDTLS_ERR_PKCS7_CERT_DATE_INVALID;
    }

    ret = mbedtls_oid_get_md_alg(&pkcs7->signed_data.digest_alg_identifiers, &md_alg);
    if (ret != 0) {
        return ret;
    }

    md_info = mbedtls_md_info_from_type(md_alg);
    if (md_info == NULL) {
        return MBEDTLS_ERR_PKCS7_VERIFY_FAIL;
    }

    hash = mbedtls_calloc(mbedtls_md_get_size(md_info), 1);
    if (hash == NULL) {
        return MBEDTLS_ERR_PKCS7_ALLOC_FAILED;
    }

    /* BEGIN must free hash before jumping out */
    if (is_data_hash) {
        if (datalen != mbedtls_md_get_size(md_info)) {
            ret = MBEDTLS_ERR_PKCS7_VERIFY_FAIL;
        } else {
            memcpy(hash, data, datalen);
        }
    } else {
        ret = mbedtls_md(md_info, data, datalen, hash);
    }
    if (ret != 0) {
        mbedtls_free(hash);
        return MBEDTLS_ERR_PKCS7_VERIFY_FAIL;
    }

    /* assume failure */
    ret = MBEDTLS_ERR_PKCS7_VERIFY_FAIL;

    /*
     * Potential TODOs
     * Currently we iterate over all signers and return success if any of them
     * verify.
     *
     * However, we could make this better by checking against the certificate's
     * identification and SignerIdentifier fields first. That would also allow
     * us to distinguish between 'no signature for key' and 'signature for key
     * failed to validate'.
     */
    for (signer = &pkcs7->signed_data.signers; signer; signer = signer->next) {
        ret = mbedtls_pk_verify(&pk_cxt, md_alg, hash,
                                mbedtls_md_get_size(md_info),
                                signer->sig.p, signer->sig.len);

        if (ret == 0) {
            break;
        }
    }

    mbedtls_free(hash);
    /* END must free hash before jumping out */
    return ret;
}

int mbedtls_pkcs7_signed_data_verify(mbedtls_pkcs7 *pkcs7,
                                     const mbedtls_x509_crt *cert,
                                     const unsigned char *data,
                                     size_t datalen)
{
    if (data == NULL) {
        return MBEDTLS_ERR_PKCS7_BAD_INPUT_DATA;
    }
    return mbedtls_pkcs7_data_or_hash_verify(pkcs7, cert, data, datalen, 0);
}

int mbedtls_pkcs7_signed_hash_verify(mbedtls_pkcs7 *pkcs7,
                                     const mbedtls_x509_crt *cert,
                                     const unsigned char *hash,
                                     size_t hashlen)
{
    if (hash == NULL) {
        return MBEDTLS_ERR_PKCS7_BAD_INPUT_DATA;
    }
    return mbedtls_pkcs7_data_or_hash_verify(pkcs7, cert, hash, hashlen, 1);
}

/*
 * Unallocate all pkcs7 data
 */
void mbedtls_pkcs7_free(mbedtls_pkcs7 *pkcs7)
{
    mbedtls_pkcs7_signer_info *signer_cur;
    mbedtls_pkcs7_signer_info *signer_prev;

    if (pkcs7 == NULL || pkcs7->raw.p == NULL) {
        return;
    }

    mbedtls_free(pkcs7->raw.p);

    mbedtls_x509_crt_free(&pkcs7->signed_data.certs);
    mbedtls_x509_crl_free(&pkcs7->signed_data.crl);

    signer_cur = pkcs7->signed_data.signers.next;
    pkcs7_free_signer_info(&pkcs7->signed_data.signers);
    while (signer_cur != NULL) {
        signer_prev = signer_cur;
        signer_cur = signer_prev->next;
        pkcs7_free_signer_info(signer_prev);
        mbedtls_free(signer_prev);
    }

    pkcs7->raw.p = NULL;
}

#endif

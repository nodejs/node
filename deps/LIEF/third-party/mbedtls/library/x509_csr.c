/*
 *  X.509 Certificate Signing Request (CSR) parsing
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
 */

#include "common.h"

#if defined(MBEDTLS_X509_CSR_PARSE_C)

#include "mbedtls/x509_csr.h"
#include "x509_internal.h"
#include "mbedtls/error.h"
#include "mbedtls/oid.h"
#include "mbedtls/platform_util.h"

#include <string.h>

#if defined(MBEDTLS_PEM_PARSE_C)
#include "mbedtls/pem.h"
#endif

#include "mbedtls/platform.h"

#if defined(MBEDTLS_FS_IO) || defined(EFIX64) || defined(EFI32)
#include <stdio.h>
#endif

/*
 *  Version  ::=  INTEGER  {  v1(0)  }
 */
static int x509_csr_get_version(unsigned char **p,
                                const unsigned char *end,
                                int *ver)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if ((ret = mbedtls_asn1_get_int(p, end, ver)) != 0) {
        if (ret == MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
            *ver = 0;
            return 0;
        }

        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_VERSION, ret);
    }

    return 0;
}

/*
 * Parse CSR extension requests in DER format
 */
static int x509_csr_parse_extensions(mbedtls_x509_csr *csr,
                                     unsigned char **p, const unsigned char *end,
                                     mbedtls_x509_csr_ext_cb_t cb,
                                     void *p_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;
    unsigned char *end_ext_data, *end_ext_octet;

    while (*p < end) {
        mbedtls_x509_buf extn_oid = { 0, 0, NULL };
        int is_critical = 0; /* DEFAULT FALSE */
        int ext_type = 0;

        /* Read sequence tag */
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

        end_ext_octet = *p + len;

        if (end_ext_octet != end_ext_data) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                     MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
        }

        /*
         * Detect supported extensions and skip unsupported extensions
         */
        ret = mbedtls_oid_get_x509_ext_type(&extn_oid, &ext_type);

        if (ret != 0) {
            /* Give the callback (if any) a chance to handle the extension */
            if (cb != NULL) {
                ret = cb(p_ctx, csr, &extn_oid, is_critical, *p, end_ext_octet);
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
        if ((csr->ext_types & ext_type) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                     MBEDTLS_ERR_ASN1_INVALID_DATA);
        }

        csr->ext_types |= ext_type;

        switch (ext_type) {
            case MBEDTLS_X509_EXT_KEY_USAGE:
                /* Parse key usage */
                if ((ret = mbedtls_x509_get_key_usage(p, end_ext_data,
                                                      &csr->key_usage)) != 0) {
                    return ret;
                }
                break;

            case MBEDTLS_X509_EXT_SUBJECT_ALT_NAME:
                /* Parse subject alt name */
                if ((ret = mbedtls_x509_get_subject_alt_name(p, end_ext_data,
                                                             &csr->subject_alt_names)) != 0) {
                    return ret;
                }
                break;

            case MBEDTLS_X509_EXT_NS_CERT_TYPE:
                /* Parse netscape certificate type */
                if ((ret = mbedtls_x509_get_ns_cert_type(p, end_ext_data,
                                                         &csr->ns_cert_type)) != 0) {
                    return ret;
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
 * Parse CSR attributes in DER format
 */
static int x509_csr_parse_attributes(mbedtls_x509_csr *csr,
                                     const unsigned char *start, const unsigned char *end,
                                     mbedtls_x509_csr_ext_cb_t cb,
                                     void *p_ctx)
{
    int ret;
    size_t len;
    unsigned char *end_attr_data;
    unsigned char **p = (unsigned char **) &start;

    while (*p < end) {
        mbedtls_x509_buf attr_oid = { 0, 0, NULL };

        if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                        MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }
        end_attr_data = *p + len;

        /* Get attribute ID */
        if ((ret = mbedtls_asn1_get_tag(p, end_attr_data, &attr_oid.len,
                                        MBEDTLS_ASN1_OID)) != 0) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
        }

        attr_oid.tag = MBEDTLS_ASN1_OID;
        attr_oid.p = *p;
        *p += attr_oid.len;

        /* Check that this is an extension-request attribute */
        if (MBEDTLS_OID_CMP(MBEDTLS_OID_PKCS9_CSR_EXT_REQ, &attr_oid) == 0) {
            if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                            MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET)) != 0) {
                return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
            }

            if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                            MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) !=
                0) {
                return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS, ret);
            }

            if ((ret = x509_csr_parse_extensions(csr, p, *p + len, cb, p_ctx)) != 0) {
                return ret;
            }

            if (*p != end_attr_data) {
                return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                         MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
            }
        }

        *p = end_attr_data;
    }

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_EXTENSIONS,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 * Parse a CSR in DER format
 */
static int mbedtls_x509_csr_parse_der_internal(mbedtls_x509_csr *csr,
                                               const unsigned char *buf, size_t buflen,
                                               mbedtls_x509_csr_ext_cb_t cb,
                                               void *p_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;
    unsigned char *p, *end;
    mbedtls_x509_buf sig_params;

    memset(&sig_params, 0, sizeof(mbedtls_x509_buf));

    /*
     * Check for valid input
     */
    if (csr == NULL || buf == NULL || buflen == 0) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    mbedtls_x509_csr_init(csr);

    /*
     * first copy the raw DER data
     */
    p = mbedtls_calloc(1, len = buflen);

    if (p == NULL) {
        return MBEDTLS_ERR_X509_ALLOC_FAILED;
    }

    memcpy(p, buf, buflen);

    csr->raw.p = p;
    csr->raw.len = len;
    end = p + len;

    /*
     *  CertificationRequest ::= SEQUENCE {
     *       certificationRequestInfo CertificationRequestInfo,
     *       signatureAlgorithm AlgorithmIdentifier,
     *       signature          BIT STRING
     *  }
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        mbedtls_x509_csr_free(csr);
        return MBEDTLS_ERR_X509_INVALID_FORMAT;
    }

    if (len != (size_t) (end - p)) {
        mbedtls_x509_csr_free(csr);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    /*
     *  CertificationRequestInfo ::= SEQUENCE {
     */
    csr->cri.p = p;

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        mbedtls_x509_csr_free(csr);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT, ret);
    }

    end = p + len;
    csr->cri.len = (size_t) (end - csr->cri.p);

    /*
     *  Version  ::=  INTEGER {  v1(0) }
     */
    if ((ret = x509_csr_get_version(&p, end, &csr->version)) != 0) {
        mbedtls_x509_csr_free(csr);
        return ret;
    }

    if (csr->version != 0) {
        mbedtls_x509_csr_free(csr);
        return MBEDTLS_ERR_X509_UNKNOWN_VERSION;
    }

    csr->version++;

    /*
     *  subject               Name
     */
    csr->subject_raw.p = p;

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        mbedtls_x509_csr_free(csr);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT, ret);
    }

    if ((ret = mbedtls_x509_get_name(&p, p + len, &csr->subject)) != 0) {
        mbedtls_x509_csr_free(csr);
        return ret;
    }

    csr->subject_raw.len = (size_t) (p - csr->subject_raw.p);

    /*
     *  subjectPKInfo SubjectPublicKeyInfo
     */
    if ((ret = mbedtls_pk_parse_subpubkey(&p, end, &csr->pk)) != 0) {
        mbedtls_x509_csr_free(csr);
        return ret;
    }

    /*
     *  attributes    [0] Attributes
     *
     *  The list of possible attributes is open-ended, though RFC 2985
     *  (PKCS#9) defines a few in section 5.4. We currently don't support any,
     *  so we just ignore them. This is a safe thing to do as the worst thing
     *  that could happen is that we issue a certificate that does not match
     *  the requester's expectations - this cannot cause a violation of our
     *  signature policies.
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC)) !=
        0) {
        mbedtls_x509_csr_free(csr);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT, ret);
    }

    if ((ret = x509_csr_parse_attributes(csr, p, p + len, cb, p_ctx)) != 0) {
        mbedtls_x509_csr_free(csr);
        return ret;
    }

    p += len;

    end = csr->raw.p + csr->raw.len;

    /*
     *  signatureAlgorithm   AlgorithmIdentifier,
     *  signature            BIT STRING
     */
    if ((ret = mbedtls_x509_get_alg(&p, end, &csr->sig_oid, &sig_params)) != 0) {
        mbedtls_x509_csr_free(csr);
        return ret;
    }

    if ((ret = mbedtls_x509_get_sig_alg(&csr->sig_oid, &sig_params,
                                        &csr->sig_md, &csr->sig_pk,
                                        &csr->sig_opts)) != 0) {
        mbedtls_x509_csr_free(csr);
        return MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG;
    }

    if ((ret = mbedtls_x509_get_sig(&p, end, &csr->sig)) != 0) {
        mbedtls_x509_csr_free(csr);
        return ret;
    }

    if (p != end) {
        mbedtls_x509_csr_free(csr);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 * Parse a CSR in DER format
 */
int mbedtls_x509_csr_parse_der(mbedtls_x509_csr *csr,
                               const unsigned char *buf, size_t buflen)
{
    return mbedtls_x509_csr_parse_der_internal(csr, buf, buflen, NULL, NULL);
}

/*
 * Parse a CSR in DER format with callback for unknown extensions
 */
int mbedtls_x509_csr_parse_der_with_ext_cb(mbedtls_x509_csr *csr,
                                           const unsigned char *buf, size_t buflen,
                                           mbedtls_x509_csr_ext_cb_t cb,
                                           void *p_ctx)
{
    return mbedtls_x509_csr_parse_der_internal(csr, buf, buflen, cb, p_ctx);
}

/*
 * Parse a CSR, allowing for PEM or raw DER encoding
 */
int mbedtls_x509_csr_parse(mbedtls_x509_csr *csr, const unsigned char *buf, size_t buflen)
{
#if defined(MBEDTLS_PEM_PARSE_C)
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t use_len;
    mbedtls_pem_context pem;
#endif

    /*
     * Check for valid input
     */
    if (csr == NULL || buf == NULL || buflen == 0) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_PEM_PARSE_C)
    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (buf[buflen - 1] == '\0') {
        mbedtls_pem_init(&pem);
        ret = mbedtls_pem_read_buffer(&pem,
                                      "-----BEGIN CERTIFICATE REQUEST-----",
                                      "-----END CERTIFICATE REQUEST-----",
                                      buf, NULL, 0, &use_len);
        if (ret == MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
            ret = mbedtls_pem_read_buffer(&pem,
                                          "-----BEGIN NEW CERTIFICATE REQUEST-----",
                                          "-----END NEW CERTIFICATE REQUEST-----",
                                          buf, NULL, 0, &use_len);
        }

        if (ret == 0) {
            /*
             * Was PEM encoded, parse the result
             */
            ret = mbedtls_x509_csr_parse_der(csr, pem.buf, pem.buflen);
        }

        mbedtls_pem_free(&pem);
        if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
            return ret;
        }
    }
#endif /* MBEDTLS_PEM_PARSE_C */
    return mbedtls_x509_csr_parse_der(csr, buf, buflen);
}

#if defined(MBEDTLS_FS_IO)
/*
 * Load a CSR into the structure
 */
int mbedtls_x509_csr_parse_file(mbedtls_x509_csr *csr, const char *path)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    unsigned char *buf;

    if ((ret = mbedtls_pk_load_file(path, &buf, &n)) != 0) {
        return ret;
    }

    ret = mbedtls_x509_csr_parse(csr, buf, n);

    mbedtls_zeroize_and_free(buf, n);

    return ret;
}
#endif /* MBEDTLS_FS_IO */

#if !defined(MBEDTLS_X509_REMOVE_INFO)
#define BEFORE_COLON    14
#define BC              "14"
/*
 * Return an informational string about the CSR.
 */
int mbedtls_x509_csr_info(char *buf, size_t size, const char *prefix,
                          const mbedtls_x509_csr *csr)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    char *p;
    char key_size_str[BEFORE_COLON];

    p = buf;
    n = size;

    ret = mbedtls_snprintf(p, n, "%sCSR version   : %d",
                           prefix, csr->version);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_snprintf(p, n, "\n%ssubject name  : ", prefix);
    MBEDTLS_X509_SAFE_SNPRINTF;
    ret = mbedtls_x509_dn_gets(p, n, &csr->subject);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_snprintf(p, n, "\n%ssigned using  : ", prefix);
    MBEDTLS_X509_SAFE_SNPRINTF;

    ret = mbedtls_x509_sig_alg_gets(p, n, &csr->sig_oid, csr->sig_pk, csr->sig_md,
                                    csr->sig_opts);
    MBEDTLS_X509_SAFE_SNPRINTF;

    if ((ret = mbedtls_x509_key_size_helper(key_size_str, BEFORE_COLON,
                                            mbedtls_pk_get_name(&csr->pk))) != 0) {
        return ret;
    }

    ret = mbedtls_snprintf(p, n, "\n%s%-" BC "s: %d bits\n", prefix, key_size_str,
                           (int) mbedtls_pk_get_bitlen(&csr->pk));
    MBEDTLS_X509_SAFE_SNPRINTF;

    /*
     * Optional extensions
     */

    if (csr->ext_types & MBEDTLS_X509_EXT_SUBJECT_ALT_NAME) {
        ret = mbedtls_snprintf(p, n, "\n%ssubject alt name  :", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;

        if ((ret = mbedtls_x509_info_subject_alt_name(&p, &n,
                                                      &csr->subject_alt_names,
                                                      prefix)) != 0) {
            return ret;
        }
    }

    if (csr->ext_types & MBEDTLS_X509_EXT_NS_CERT_TYPE) {
        ret = mbedtls_snprintf(p, n, "\n%scert. type        : ", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;

        if ((ret = mbedtls_x509_info_cert_type(&p, &n, csr->ns_cert_type)) != 0) {
            return ret;
        }
    }

    if (csr->ext_types & MBEDTLS_X509_EXT_KEY_USAGE) {
        ret = mbedtls_snprintf(p, n, "\n%skey usage         : ", prefix);
        MBEDTLS_X509_SAFE_SNPRINTF;

        if ((ret = mbedtls_x509_info_key_usage(&p, &n, csr->key_usage)) != 0) {
            return ret;
        }
    }

    if (csr->ext_types != 0) {
        ret = mbedtls_snprintf(p, n, "\n");
        MBEDTLS_X509_SAFE_SNPRINTF;
    }

    return (int) (size - n);
}
#endif /* MBEDTLS_X509_REMOVE_INFO */

/*
 * Initialize a CSR
 */
void mbedtls_x509_csr_init(mbedtls_x509_csr *csr)
{
    memset(csr, 0, sizeof(mbedtls_x509_csr));
}

/*
 * Unallocate all CSR data
 */
void mbedtls_x509_csr_free(mbedtls_x509_csr *csr)
{
    if (csr == NULL) {
        return;
    }

    mbedtls_pk_free(&csr->pk);

#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT)
    mbedtls_free(csr->sig_opts);
#endif

    mbedtls_asn1_free_named_data_list_shallow(csr->subject.next);
    mbedtls_asn1_sequence_free(csr->subject_alt_names.next);

    if (csr->raw.p != NULL) {
        mbedtls_zeroize_and_free(csr->raw.p, csr->raw.len);
    }

    mbedtls_platform_zeroize(csr, sizeof(mbedtls_x509_csr));
}

#endif /* MBEDTLS_X509_CSR_PARSE_C */

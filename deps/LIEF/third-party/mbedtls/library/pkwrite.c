/*
 *  Public Key layer for writing key files and structures
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_PK_WRITE_C)

#include "mbedtls/pk.h"
#include "mbedtls/asn1write.h"
#include "mbedtls/oid.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "pk_internal.h"

#include <string.h>

#if defined(MBEDTLS_ECP_C)
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/platform_util.h"
#endif
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
#include "pk_internal.h"
#endif
#if defined(MBEDTLS_RSA_C) || defined(MBEDTLS_PK_HAVE_ECC_KEYS)
#include "pkwrite.h"
#endif
#if defined(MBEDTLS_PEM_WRITE_C)
#include "mbedtls/pem.h"
#endif
#if defined(MBEDTLS_RSA_C)
#include "rsa_internal.h"
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa/crypto.h"
#include "psa_util_internal.h"
#endif
#include "mbedtls/platform.h"

/* Helpers for properly sizing buffers aimed at holding public keys or
 * key-pairs based on build symbols. */
#if defined(MBEDTLS_PK_USE_PSA_EC_DATA)
#define PK_MAX_EC_PUBLIC_KEY_SIZE       PSA_EXPORT_PUBLIC_KEY_MAX_SIZE
#define PK_MAX_EC_KEY_PAIR_SIZE         MBEDTLS_PSA_MAX_EC_KEY_PAIR_LENGTH
#elif defined(MBEDTLS_USE_PSA_CRYPTO)
#define PK_MAX_EC_PUBLIC_KEY_SIZE       PSA_EXPORT_PUBLIC_KEY_MAX_SIZE
#define PK_MAX_EC_KEY_PAIR_SIZE         MBEDTLS_PSA_MAX_EC_KEY_PAIR_LENGTH
#else
#define PK_MAX_EC_PUBLIC_KEY_SIZE       MBEDTLS_ECP_MAX_PT_LEN
#define PK_MAX_EC_KEY_PAIR_SIZE         MBEDTLS_ECP_MAX_BYTES
#endif

/******************************************************************************
 * Internal functions for RSA keys.
 ******************************************************************************/
#if defined(MBEDTLS_RSA_C)
static int pk_write_rsa_der(unsigned char **p, unsigned char *buf,
                            const mbedtls_pk_context *pk)
{
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        uint8_t tmp[PSA_EXPORT_KEY_PAIR_MAX_SIZE];
        size_t tmp_len = 0;

        if (psa_export_key(pk->priv_id, tmp, sizeof(tmp), &tmp_len) != PSA_SUCCESS) {
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        }
        /* Ensure there's enough space in the provided buffer before copying data into it. */
        if (tmp_len > (size_t) (*p - buf)) {
            mbedtls_platform_zeroize(tmp, sizeof(tmp));
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }
        *p -= tmp_len;
        memcpy(*p, tmp, tmp_len);
        mbedtls_platform_zeroize(tmp, sizeof(tmp));

        return (int) tmp_len;
    }
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    return mbedtls_rsa_write_key(mbedtls_pk_rsa(*pk), buf, p);
}
#endif /* MBEDTLS_RSA_C */

/******************************************************************************
 * Internal functions for EC keys.
 ******************************************************************************/
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
#if defined(MBEDTLS_PK_USE_PSA_EC_DATA)
static int pk_write_ec_pubkey(unsigned char **p, unsigned char *start,
                              const mbedtls_pk_context *pk)
{
    size_t len = 0;
    uint8_t buf[PK_MAX_EC_PUBLIC_KEY_SIZE];

    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        if (psa_export_public_key(pk->priv_id, buf, sizeof(buf), &len) != PSA_SUCCESS) {
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        }
    } else {
        len = pk->pub_raw_len;
        memcpy(buf, pk->pub_raw, len);
    }

    if (*p < start || (size_t) (*p - start) < len) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    *p -= len;
    memcpy(*p, buf, len);

    return (int) len;
}
#else /* MBEDTLS_PK_USE_PSA_EC_DATA */
static int pk_write_ec_pubkey(unsigned char **p, unsigned char *start,
                              const mbedtls_pk_context *pk)
{
    size_t len = 0;
    unsigned char buf[PK_MAX_EC_PUBLIC_KEY_SIZE];
    mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*pk);
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        if (psa_export_public_key(pk->priv_id, buf, sizeof(buf), &len) != PSA_SUCCESS) {
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        }
        /* Ensure there's enough space in the provided buffer before copying data into it. */
        if (len > (size_t) (*p - start)) {
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }
        *p -= len;
        memcpy(*p, buf, len);
        return (int) len;
    } else
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    {
        if ((ret = mbedtls_ecp_point_write_binary(&ec->grp, &ec->Q,
                                                  MBEDTLS_ECP_PF_UNCOMPRESSED,
                                                  &len, buf, sizeof(buf))) != 0) {
            return ret;
        }
    }

    if (*p < start || (size_t) (*p - start) < len) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    *p -= len;
    memcpy(*p, buf, len);

    return (int) len;
}
#endif /* MBEDTLS_PK_USE_PSA_EC_DATA */

/*
 * privateKey  OCTET STRING -- always of length ceil(log2(n)/8)
 */
#if defined(MBEDTLS_PK_USE_PSA_EC_DATA)
static int pk_write_ec_private(unsigned char **p, unsigned char *start,
                               const mbedtls_pk_context *pk)
{
    size_t byte_length;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char tmp[PK_MAX_EC_KEY_PAIR_SIZE];
    psa_status_t status;

    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        status = psa_export_key(pk->priv_id, tmp, sizeof(tmp), &byte_length);
        if (status != PSA_SUCCESS) {
            ret = PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
            return ret;
        }
    } else {
        status = psa_export_key(pk->priv_id, tmp, sizeof(tmp), &byte_length);
        if (status != PSA_SUCCESS) {
            ret = PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
            goto exit;
        }
    }

    ret = mbedtls_asn1_write_octet_string(p, start, tmp, byte_length);
exit:
    mbedtls_platform_zeroize(tmp, sizeof(tmp));
    return ret;
}
#else /* MBEDTLS_PK_USE_PSA_EC_DATA */
static int pk_write_ec_private(unsigned char **p, unsigned char *start,
                               const mbedtls_pk_context *pk)
{
    size_t byte_length;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char tmp[PK_MAX_EC_KEY_PAIR_SIZE];

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_status_t status;
    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        status = psa_export_key(pk->priv_id, tmp, sizeof(tmp), &byte_length);
        if (status != PSA_SUCCESS) {
            ret = PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
            return ret;
        }
    } else
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    {
        mbedtls_ecp_keypair *ec = mbedtls_pk_ec_rw(*pk);
        byte_length = (ec->grp.pbits + 7) / 8;

        ret = mbedtls_ecp_write_key_ext(ec, &byte_length, tmp, sizeof(tmp));
        if (ret != 0) {
            goto exit;
        }
    }
    ret = mbedtls_asn1_write_octet_string(p, start, tmp, byte_length);
exit:
    mbedtls_platform_zeroize(tmp, sizeof(tmp));
    return ret;
}
#endif /* MBEDTLS_PK_USE_PSA_EC_DATA */

/*
 * ECParameters ::= CHOICE {
 *   namedCurve         OBJECT IDENTIFIER
 * }
 */
static int pk_write_ec_param(unsigned char **p, unsigned char *start,
                             mbedtls_ecp_group_id grp_id)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;
    const char *oid;
    size_t oid_len;

    if ((ret = mbedtls_oid_get_oid_by_ec_grp(grp_id, &oid, &oid_len)) != 0) {
        return ret;
    }

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_oid(p, start, oid, oid_len));

    return (int) len;
}

#if defined(MBEDTLS_PK_HAVE_RFC8410_CURVES)
/*
 * RFC8410 section 7
 *
 * OneAsymmetricKey ::= SEQUENCE {
 *    version Version,
 *    privateKeyAlgorithm PrivateKeyAlgorithmIdentifier,
 *    privateKey PrivateKey,
 *    attributes [0] IMPLICIT Attributes OPTIONAL,
 *    ...,
 *    [[2: publicKey [1] IMPLICIT PublicKey OPTIONAL ]],
 *    ...
 * }
 * ...
 * CurvePrivateKey ::= OCTET STRING
 */
static int pk_write_ec_rfc8410_der(unsigned char **p, unsigned char *buf,
                                   const mbedtls_pk_context *pk)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;
    size_t oid_len = 0;
    const char *oid;
    mbedtls_ecp_group_id grp_id;

    /* privateKey */
    MBEDTLS_ASN1_CHK_ADD(len, pk_write_ec_private(p, buf, pk));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, buf, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, buf, MBEDTLS_ASN1_OCTET_STRING));

    grp_id = mbedtls_pk_get_ec_group_id(pk);
    /* privateKeyAlgorithm */
    if ((ret = mbedtls_oid_get_oid_by_ec_grp_algid(grp_id, &oid, &oid_len)) != 0) {
        return ret;
    }
    MBEDTLS_ASN1_CHK_ADD(len,
                         mbedtls_asn1_write_algorithm_identifier_ext(p, buf, oid, oid_len, 0, 0));

    /* version */
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_int(p, buf, 0));

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, buf, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, buf, MBEDTLS_ASN1_CONSTRUCTED |
                                                     MBEDTLS_ASN1_SEQUENCE));

    return (int) len;
}
#endif /* MBEDTLS_PK_HAVE_RFC8410_CURVES */

/*
 * RFC 5915, or SEC1 Appendix C.4
 *
 * ECPrivateKey ::= SEQUENCE {
 *      version        INTEGER { ecPrivkeyVer1(1) } (ecPrivkeyVer1),
 *      privateKey     OCTET STRING,
 *      parameters [0] ECParameters {{ NamedCurve }} OPTIONAL,
 *      publicKey  [1] BIT STRING OPTIONAL
 *    }
 */
static int pk_write_ec_der(unsigned char **p, unsigned char *buf,
                           const mbedtls_pk_context *pk)
{
    size_t len = 0;
    int ret;
    size_t pub_len = 0, par_len = 0;
    mbedtls_ecp_group_id grp_id;

    /* publicKey */
    MBEDTLS_ASN1_CHK_ADD(pub_len, pk_write_ec_pubkey(p, buf, pk));

    if (*p - buf < 1) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }
    (*p)--;
    **p = 0;
    pub_len += 1;

    MBEDTLS_ASN1_CHK_ADD(pub_len, mbedtls_asn1_write_len(p, buf, pub_len));
    MBEDTLS_ASN1_CHK_ADD(pub_len, mbedtls_asn1_write_tag(p, buf, MBEDTLS_ASN1_BIT_STRING));

    MBEDTLS_ASN1_CHK_ADD(pub_len, mbedtls_asn1_write_len(p, buf, pub_len));
    MBEDTLS_ASN1_CHK_ADD(pub_len, mbedtls_asn1_write_tag(p, buf,
                                                         MBEDTLS_ASN1_CONTEXT_SPECIFIC |
                                                         MBEDTLS_ASN1_CONSTRUCTED | 1));
    len += pub_len;

    /* parameters */
    grp_id = mbedtls_pk_get_ec_group_id(pk);
    MBEDTLS_ASN1_CHK_ADD(par_len, pk_write_ec_param(p, buf, grp_id));
    MBEDTLS_ASN1_CHK_ADD(par_len, mbedtls_asn1_write_len(p, buf, par_len));
    MBEDTLS_ASN1_CHK_ADD(par_len, mbedtls_asn1_write_tag(p, buf,
                                                         MBEDTLS_ASN1_CONTEXT_SPECIFIC |
                                                         MBEDTLS_ASN1_CONSTRUCTED | 0));
    len += par_len;

    /* privateKey */
    MBEDTLS_ASN1_CHK_ADD(len, pk_write_ec_private(p, buf, pk));

    /* version */
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_int(p, buf, 1));

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, buf, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, buf, MBEDTLS_ASN1_CONSTRUCTED |
                                                     MBEDTLS_ASN1_SEQUENCE));

    return (int) len;
}
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

/******************************************************************************
 * Internal functions for Opaque keys.
 ******************************************************************************/
#if defined(MBEDTLS_USE_PSA_CRYPTO)
static int pk_write_opaque_pubkey(unsigned char **p, unsigned char *start,
                                  const mbedtls_pk_context *pk)
{
    size_t buffer_size;
    size_t len = 0;

    if (*p < start) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    buffer_size = (size_t) (*p - start);
    if (psa_export_public_key(pk->priv_id, start, buffer_size,
                              &len) != PSA_SUCCESS) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    *p -= len;
    memmove(*p, start, len);

    return (int) len;
}
#endif /* MBEDTLS_USE_PSA_CRYPTO */

/******************************************************************************
 * Generic helpers
 ******************************************************************************/

/* Extend the public mbedtls_pk_get_type() by getting key type also in case of
 * opaque keys. */
static mbedtls_pk_type_t pk_get_type_ext(const mbedtls_pk_context *pk)
{
    mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(pk);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if (pk_type == MBEDTLS_PK_OPAQUE) {
        psa_key_attributes_t opaque_attrs = PSA_KEY_ATTRIBUTES_INIT;
        psa_key_type_t opaque_key_type;

        if (psa_get_key_attributes(pk->priv_id, &opaque_attrs) != PSA_SUCCESS) {
            return MBEDTLS_PK_NONE;
        }
        opaque_key_type = psa_get_key_type(&opaque_attrs);
        psa_reset_key_attributes(&opaque_attrs);

        if (PSA_KEY_TYPE_IS_ECC(opaque_key_type)) {
            return MBEDTLS_PK_ECKEY;
        } else if (PSA_KEY_TYPE_IS_RSA(opaque_key_type)) {
            return MBEDTLS_PK_RSA;
        } else {
            return MBEDTLS_PK_NONE;
        }
    } else
#endif
    return pk_type;
}

/******************************************************************************
 * Public functions for writing private/public DER keys.
 ******************************************************************************/
int mbedtls_pk_write_pubkey(unsigned char **p, unsigned char *start,
                            const mbedtls_pk_context *key)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

#if defined(MBEDTLS_RSA_C)
    if (mbedtls_pk_get_type(key) == MBEDTLS_PK_RSA) {
        MBEDTLS_ASN1_CHK_ADD(len, mbedtls_rsa_write_pubkey(mbedtls_pk_rsa(*key), start, p));
    } else
#endif
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (mbedtls_pk_get_type(key) == MBEDTLS_PK_ECKEY) {
        MBEDTLS_ASN1_CHK_ADD(len, pk_write_ec_pubkey(p, start, key));
    } else
#endif
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if (mbedtls_pk_get_type(key) == MBEDTLS_PK_OPAQUE) {
        MBEDTLS_ASN1_CHK_ADD(len, pk_write_opaque_pubkey(p, start, key));
    } else
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;

    return (int) len;
}

int mbedtls_pk_write_pubkey_der(const mbedtls_pk_context *key, unsigned char *buf, size_t size)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *c;
    int has_par = 1;
    size_t len = 0, par_len = 0, oid_len = 0;
    mbedtls_pk_type_t pk_type;
    const char *oid = NULL;

    if (size == 0) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    c = buf + size;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_pk_write_pubkey(&c, buf, key));

    if (c - buf < 1) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    /*
     *  SubjectPublicKeyInfo  ::=  SEQUENCE  {
     *       algorithm            AlgorithmIdentifier,
     *       subjectPublicKey     BIT STRING }
     */
    *--c = 0;
    len += 1;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&c, buf, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&c, buf, MBEDTLS_ASN1_BIT_STRING));

    pk_type = pk_get_type_ext(key);

#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (pk_get_type_ext(key) == MBEDTLS_PK_ECKEY) {
        mbedtls_ecp_group_id ec_grp_id = mbedtls_pk_get_ec_group_id(key);
        if (MBEDTLS_PK_IS_RFC8410_GROUP_ID(ec_grp_id)) {
            ret = mbedtls_oid_get_oid_by_ec_grp_algid(ec_grp_id, &oid, &oid_len);
            if (ret != 0) {
                return ret;
            }
            has_par = 0;
        } else {
            MBEDTLS_ASN1_CHK_ADD(par_len, pk_write_ec_param(&c, buf, ec_grp_id));
        }
    }
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

    /* At this point oid_len is not null only for EC Montgomery keys. */
    if (oid_len == 0) {
        ret = mbedtls_oid_get_oid_by_pk_alg(pk_type, &oid, &oid_len);
        if (ret != 0) {
            return ret;
        }
    }

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_algorithm_identifier_ext(&c, buf, oid, oid_len,
                                                                          par_len, has_par));

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&c, buf, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&c, buf, MBEDTLS_ASN1_CONSTRUCTED |
                                                     MBEDTLS_ASN1_SEQUENCE));

    return (int) len;
}

int mbedtls_pk_write_key_der(const mbedtls_pk_context *key, unsigned char *buf, size_t size)
{
    unsigned char *c;

    if (size == 0) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    c = buf + size;

#if defined(MBEDTLS_RSA_C)
    if (pk_get_type_ext(key) == MBEDTLS_PK_RSA) {
        return pk_write_rsa_der(&c, buf, key);
    } else
#endif /* MBEDTLS_RSA_C */
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (pk_get_type_ext(key) == MBEDTLS_PK_ECKEY) {
#if defined(MBEDTLS_PK_HAVE_RFC8410_CURVES)
        if (mbedtls_pk_is_rfc8410(key)) {
            return pk_write_ec_rfc8410_der(&c, buf, key);
        }
#endif /* MBEDTLS_PK_HAVE_RFC8410_CURVES */
        return pk_write_ec_der(&c, buf, key);
    } else
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */
    return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
}

/******************************************************************************
 * Public functions for wrinting private/public PEM keys.
 ******************************************************************************/
#if defined(MBEDTLS_PEM_WRITE_C)

#define PUB_DER_MAX_BYTES                                                   \
    (MBEDTLS_PK_RSA_PUB_DER_MAX_BYTES > MBEDTLS_PK_ECP_PUB_DER_MAX_BYTES ? \
     MBEDTLS_PK_RSA_PUB_DER_MAX_BYTES : MBEDTLS_PK_ECP_PUB_DER_MAX_BYTES)
#define PRV_DER_MAX_BYTES                                                   \
    (MBEDTLS_PK_RSA_PRV_DER_MAX_BYTES > MBEDTLS_PK_ECP_PRV_DER_MAX_BYTES ? \
     MBEDTLS_PK_RSA_PRV_DER_MAX_BYTES : MBEDTLS_PK_ECP_PRV_DER_MAX_BYTES)

int mbedtls_pk_write_pubkey_pem(const mbedtls_pk_context *key, unsigned char *buf, size_t size)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *output_buf = NULL;
    output_buf = mbedtls_calloc(1, PUB_DER_MAX_BYTES);
    if (output_buf == NULL) {
        return MBEDTLS_ERR_PK_ALLOC_FAILED;
    }
    size_t olen = 0;

    if ((ret = mbedtls_pk_write_pubkey_der(key, output_buf,
                                           PUB_DER_MAX_BYTES)) < 0) {
        goto cleanup;
    }

    if ((ret = mbedtls_pem_write_buffer(PEM_BEGIN_PUBLIC_KEY "\n", PEM_END_PUBLIC_KEY "\n",
                                        output_buf + PUB_DER_MAX_BYTES - ret,
                                        ret, buf, size, &olen)) != 0) {
        goto cleanup;
    }

    ret = 0;
cleanup:
    mbedtls_free(output_buf);
    return ret;
}

int mbedtls_pk_write_key_pem(const mbedtls_pk_context *key, unsigned char *buf, size_t size)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *output_buf = NULL;
    output_buf = mbedtls_calloc(1, PRV_DER_MAX_BYTES);
    if (output_buf == NULL) {
        return MBEDTLS_ERR_PK_ALLOC_FAILED;
    }
    const char *begin, *end;
    size_t olen = 0;

    if ((ret = mbedtls_pk_write_key_der(key, output_buf, PRV_DER_MAX_BYTES)) < 0) {
        goto cleanup;
    }

#if defined(MBEDTLS_RSA_C)
    if (pk_get_type_ext(key) == MBEDTLS_PK_RSA) {
        begin = PEM_BEGIN_PRIVATE_KEY_RSA "\n";
        end = PEM_END_PRIVATE_KEY_RSA "\n";
    } else
#endif
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (pk_get_type_ext(key) == MBEDTLS_PK_ECKEY) {
        if (mbedtls_pk_is_rfc8410(key)) {
            begin = PEM_BEGIN_PRIVATE_KEY_PKCS8 "\n";
            end = PEM_END_PRIVATE_KEY_PKCS8 "\n";
        } else {
            begin = PEM_BEGIN_PRIVATE_KEY_EC "\n";
            end = PEM_END_PRIVATE_KEY_EC "\n";
        }
    } else
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */
    {
        ret = MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
        goto cleanup;
    }

    if ((ret = mbedtls_pem_write_buffer(begin, end,
                                        output_buf + PRV_DER_MAX_BYTES - ret,
                                        ret, buf, size, &olen)) != 0) {
        goto cleanup;
    }

    ret = 0;
cleanup:
    mbedtls_zeroize_and_free(output_buf, PRV_DER_MAX_BYTES);
    return ret;
}
#endif /* MBEDTLS_PEM_WRITE_C */

#endif /* MBEDTLS_PK_WRITE_C */

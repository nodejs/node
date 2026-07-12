/*
 *  Public Key layer for parsing key files and structures
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_PK_PARSE_C)

#include "mbedtls/pk.h"
#include "mbedtls/asn1.h"
#include "mbedtls/oid.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/platform.h"
#include "mbedtls/error.h"
#include "mbedtls/ecp.h"
#include "pk_internal.h"

#include <string.h>

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "mbedtls/psa_util.h"
#include "psa/crypto.h"
#endif

/* Key types */
#if defined(MBEDTLS_RSA_C)
#include "mbedtls/rsa.h"
#include "rsa_internal.h"
#endif

/* Extended formats */
#if defined(MBEDTLS_PEM_PARSE_C)
#include "mbedtls/pem.h"
#endif
#if defined(MBEDTLS_PKCS5_C)
#include "mbedtls/pkcs5.h"
#endif
#if defined(MBEDTLS_PKCS12_C)
#include "mbedtls/pkcs12.h"
#endif

#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)

/***********************************************************************
 *
 *      Low-level ECC parsing: optional support for SpecifiedECDomain
 *
 * There are two functions here that are used by the rest of the code:
 * - pk_ecc_tag_is_speficied_ec_domain()
 * - pk_ecc_group_id_from_specified()
 *
 * All the other functions are internal to this section.
 *
 * The two "public" functions have a dummy variant provided
 * in configs without MBEDTLS_PK_PARSE_EC_EXTENDED. This acts as an
 * abstraction layer for this macro, which should not appear outside
 * this section.
 *
 **********************************************************************/

#if !defined(MBEDTLS_PK_PARSE_EC_EXTENDED)
/* See the "real" version for documentation */
static int pk_ecc_tag_is_specified_ec_domain(int tag)
{
    (void) tag;
    return 0;
}

/* See the "real" version for documentation */
static int pk_ecc_group_id_from_specified(const mbedtls_asn1_buf *params,
                                          mbedtls_ecp_group_id *grp_id)
{
    (void) params;
    (void) grp_id;
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
}
#else /* MBEDTLS_PK_PARSE_EC_EXTENDED */
/*
 * Tell if the passed tag might be the start of SpecifiedECDomain
 * (that is, a sequence).
 */
static int pk_ecc_tag_is_specified_ec_domain(int tag)
{
    return tag == (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
}

/*
 * Parse a SpecifiedECDomain (SEC 1 C.2) and (mostly) fill the group with it.
 * WARNING: the resulting group should only be used with
 * pk_ecc_group_id_from_specified(), since its base point may not be set correctly
 * if it was encoded compressed.
 *
 *  SpecifiedECDomain ::= SEQUENCE {
 *      version SpecifiedECDomainVersion(ecdpVer1 | ecdpVer2 | ecdpVer3, ...),
 *      fieldID FieldID {{FieldTypes}},
 *      curve Curve,
 *      base ECPoint,
 *      order INTEGER,
 *      cofactor INTEGER OPTIONAL,
 *      hash HashAlgorithm OPTIONAL,
 *      ...
 *  }
 *
 * We only support prime-field as field type, and ignore hash and cofactor.
 */
static int pk_group_from_specified(const mbedtls_asn1_buf *params, mbedtls_ecp_group *grp)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = params->p;
    const unsigned char *const end = params->p + params->len;
    const unsigned char *end_field, *end_curve;
    size_t len;
    int ver;

    /* SpecifiedECDomainVersion ::= INTEGER { 1, 2, 3 } */
    if ((ret = mbedtls_asn1_get_int(&p, end, &ver)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    if (ver < 1 || ver > 3) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }

    /*
     * FieldID { FIELD-ID:IOSet } ::= SEQUENCE { -- Finite field
     *       fieldType FIELD-ID.&id({IOSet}),
     *       parameters FIELD-ID.&Type({IOSet}{@fieldType})
     * }
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return ret;
    }

    end_field = p + len;

    /*
     * FIELD-ID ::= TYPE-IDENTIFIER
     * FieldTypes FIELD-ID ::= {
     *       { Prime-p IDENTIFIED BY prime-field } |
     *       { Characteristic-two IDENTIFIED BY characteristic-two-field }
     * }
     * prime-field OBJECT IDENTIFIER ::= { id-fieldType 1 }
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end_field, &len, MBEDTLS_ASN1_OID)) != 0) {
        return ret;
    }

    if (len != MBEDTLS_OID_SIZE(MBEDTLS_OID_ANSI_X9_62_PRIME_FIELD) ||
        memcmp(p, MBEDTLS_OID_ANSI_X9_62_PRIME_FIELD, len) != 0) {
        return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
    }

    p += len;

    /* Prime-p ::= INTEGER -- Field of size p. */
    if ((ret = mbedtls_asn1_get_mpi(&p, end_field, &grp->P)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    grp->pbits = mbedtls_mpi_bitlen(&grp->P);

    if (p != end_field) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    /*
     * Curve ::= SEQUENCE {
     *       a FieldElement,
     *       b FieldElement,
     *       seed BIT STRING OPTIONAL
     *       -- Shall be present if used in SpecifiedECDomain
     *       -- with version equal to ecdpVer2 or ecdpVer3
     * }
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return ret;
    }

    end_curve = p + len;

    /*
     * FieldElement ::= OCTET STRING
     * containing an integer in the case of a prime field
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end_curve, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0 ||
        (ret = mbedtls_mpi_read_binary(&grp->A, p, len)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    p += len;

    if ((ret = mbedtls_asn1_get_tag(&p, end_curve, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0 ||
        (ret = mbedtls_mpi_read_binary(&grp->B, p, len)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    p += len;

    /* Ignore seed BIT STRING OPTIONAL */
    if ((ret = mbedtls_asn1_get_tag(&p, end_curve, &len, MBEDTLS_ASN1_BIT_STRING)) == 0) {
        p += len;
    }

    if (p != end_curve) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    /*
     * ECPoint ::= OCTET STRING
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    if ((ret = mbedtls_ecp_point_read_binary(grp, &grp->G,
                                             (const unsigned char *) p, len)) != 0) {
        /*
         * If we can't read the point because it's compressed, cheat by
         * reading only the X coordinate and the parity bit of Y.
         */
        if (ret != MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE ||
            (p[0] != 0x02 && p[0] != 0x03) ||
            len != mbedtls_mpi_size(&grp->P) + 1 ||
            mbedtls_mpi_read_binary(&grp->G.X, p + 1, len - 1) != 0 ||
            mbedtls_mpi_lset(&grp->G.Y, p[0] - 2) != 0 ||
            mbedtls_mpi_lset(&grp->G.Z, 1) != 0) {
            return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
        }
    }

    p += len;

    /*
     * order INTEGER
     */
    if ((ret = mbedtls_asn1_get_mpi(&p, end, &grp->N)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    grp->nbits = mbedtls_mpi_bitlen(&grp->N);

    /*
     * Allow optional elements by purposefully not enforcing p == end here.
     */

    return 0;
}

/*
 * Find the group id associated with an (almost filled) group as generated by
 * pk_group_from_specified(), or return an error if unknown.
 */
static int pk_group_id_from_group(const mbedtls_ecp_group *grp, mbedtls_ecp_group_id *grp_id)
{
    int ret = 0;
    mbedtls_ecp_group ref;
    const mbedtls_ecp_group_id *id;

    mbedtls_ecp_group_init(&ref);

    for (id = mbedtls_ecp_grp_id_list(); *id != MBEDTLS_ECP_DP_NONE; id++) {
        /* Load the group associated to that id */
        mbedtls_ecp_group_free(&ref);
        MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&ref, *id));

        /* Compare to the group we were given, starting with easy tests */
        if (grp->pbits == ref.pbits && grp->nbits == ref.nbits &&
            mbedtls_mpi_cmp_mpi(&grp->P, &ref.P) == 0 &&
            mbedtls_mpi_cmp_mpi(&grp->A, &ref.A) == 0 &&
            mbedtls_mpi_cmp_mpi(&grp->B, &ref.B) == 0 &&
            mbedtls_mpi_cmp_mpi(&grp->N, &ref.N) == 0 &&
            mbedtls_mpi_cmp_mpi(&grp->G.X, &ref.G.X) == 0 &&
            mbedtls_mpi_cmp_mpi(&grp->G.Z, &ref.G.Z) == 0 &&
            /* For Y we may only know the parity bit, so compare only that */
            mbedtls_mpi_get_bit(&grp->G.Y, 0) == mbedtls_mpi_get_bit(&ref.G.Y, 0)) {
            break;
        }
    }

cleanup:
    mbedtls_ecp_group_free(&ref);

    *grp_id = *id;

    if (ret == 0 && *id == MBEDTLS_ECP_DP_NONE) {
        ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    return ret;
}

/*
 * Parse a SpecifiedECDomain (SEC 1 C.2) and find the associated group ID
 */
static int pk_ecc_group_id_from_specified(const mbedtls_asn1_buf *params,
                                          mbedtls_ecp_group_id *grp_id)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_group grp;

    mbedtls_ecp_group_init(&grp);

    if ((ret = pk_group_from_specified(params, &grp)) != 0) {
        goto cleanup;
    }

    ret = pk_group_id_from_group(&grp, grp_id);

cleanup:
    /* The API respecting lifecycle for mbedtls_ecp_group struct is
     * _init(), _load() and _free(). In pk_ecc_group_id_from_specified() the
     * temporary grp breaks that flow and it's members are populated
     * by pk_group_id_from_group(). As such mbedtls_ecp_group_free()
     * which is assuming a group populated by _setup() may not clean-up
     * properly -> Manually free it's members.
     */
    mbedtls_mpi_free(&grp.N);
    mbedtls_mpi_free(&grp.P);
    mbedtls_mpi_free(&grp.A);
    mbedtls_mpi_free(&grp.B);
    mbedtls_ecp_point_free(&grp.G);

    return ret;
}
#endif /* MBEDTLS_PK_PARSE_EC_EXTENDED */

/***********************************************************************
 *
 * Unsorted (yet!) from this point on until the next section header
 *
 **********************************************************************/

/* Minimally parse an ECParameters buffer to and mbedtls_asn1_buf
 *
 * ECParameters ::= CHOICE {
 *   namedCurve         OBJECT IDENTIFIER
 *   specifiedCurve     SpecifiedECDomain -- = SEQUENCE { ... }
 *   -- implicitCurve   NULL
 * }
 */
static int pk_get_ecparams(unsigned char **p, const unsigned char *end,
                           mbedtls_asn1_buf *params)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (end - *p < 1) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_OUT_OF_DATA);
    }

    /* Acceptable tags: OID for namedCurve, or specifiedECDomain */
    params->tag = **p;
    if (params->tag != MBEDTLS_ASN1_OID &&
        !pk_ecc_tag_is_specified_ec_domain(params->tag)) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
    }

    if ((ret = mbedtls_asn1_get_tag(p, end, &params->len, params->tag)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    params->p = *p;
    *p += params->len;

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 * Use EC parameters to initialise an EC group
 *
 * ECParameters ::= CHOICE {
 *   namedCurve         OBJECT IDENTIFIER
 *   specifiedCurve     SpecifiedECDomain -- = SEQUENCE { ... }
 *   -- implicitCurve   NULL
 */
static int pk_use_ecparams(const mbedtls_asn1_buf *params, mbedtls_pk_context *pk)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_group_id grp_id;

    if (params->tag == MBEDTLS_ASN1_OID) {
        if (mbedtls_oid_get_ec_grp(params, &grp_id) != 0) {
            return MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE;
        }
    } else {
        ret = pk_ecc_group_id_from_specified(params, &grp_id);
        if (ret != 0) {
            return ret;
        }
    }

    return mbedtls_pk_ecc_set_group(pk, grp_id);
}

#if defined(MBEDTLS_PK_HAVE_RFC8410_CURVES)

/*
 * Load an RFC8410 EC key, which doesn't have any parameters
 */
static int pk_use_ecparams_rfc8410(const mbedtls_asn1_buf *params,
                                   mbedtls_ecp_group_id grp_id,
                                   mbedtls_pk_context *pk)
{
    if (params->tag != 0 || params->len != 0) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }

    return mbedtls_pk_ecc_set_group(pk, grp_id);
}

/*
 * Parse an RFC 8410 encoded private EC key
 *
 * CurvePrivateKey ::= OCTET STRING
 */
static int pk_parse_key_rfc8410_der(mbedtls_pk_context *pk,
                                    unsigned char *key, size_t keylen, const unsigned char *end,
                                    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    if ((ret = mbedtls_asn1_get_tag(&key, (key + keylen), &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    if (key + len != end) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }

    /*
     * Load the private key
     */
    ret = mbedtls_pk_ecc_set_key(pk, key, len);
    if (ret != 0) {
        return ret;
    }

    /* pk_parse_key_pkcs8_unencrypted_der() only supports version 1 PKCS8 keys,
     * which never contain a public key. As such, derive the public key
     * unconditionally. */
    if ((ret = mbedtls_pk_ecc_set_pubkey_from_prv(pk, key, len, f_rng, p_rng)) != 0) {
        return ret;
    }

    return 0;
}
#endif /* MBEDTLS_PK_HAVE_RFC8410_CURVES */

#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

/* Get a PK algorithm identifier
 *
 *  AlgorithmIdentifier  ::=  SEQUENCE  {
 *       algorithm               OBJECT IDENTIFIER,
 *       parameters              ANY DEFINED BY algorithm OPTIONAL  }
 */
static int pk_get_pk_alg(unsigned char **p,
                         const unsigned char *end,
                         mbedtls_pk_type_t *pk_alg, mbedtls_asn1_buf *params,
                         mbedtls_ecp_group_id *ec_grp_id)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_asn1_buf alg_oid;

    memset(params, 0, sizeof(mbedtls_asn1_buf));

    if ((ret = mbedtls_asn1_get_alg(p, end, &alg_oid, params)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_INVALID_ALG, ret);
    }

    ret = mbedtls_oid_get_pk_alg(&alg_oid, pk_alg);
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (ret == MBEDTLS_ERR_OID_NOT_FOUND) {
        ret = mbedtls_oid_get_ec_grp_algid(&alg_oid, ec_grp_id);
        if (ret == 0) {
            *pk_alg = MBEDTLS_PK_ECKEY;
        }
    }
#else
    (void) ec_grp_id;
#endif
    if (ret != 0) {
        return MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;
    }

    /*
     * No parameters with RSA (only for EC)
     */
    if (*pk_alg == MBEDTLS_PK_RSA &&
        ((params->tag != MBEDTLS_ASN1_NULL && params->tag != 0) ||
         params->len != 0)) {
        return MBEDTLS_ERR_PK_INVALID_ALG;
    }

    return 0;
}

/*
 *  SubjectPublicKeyInfo  ::=  SEQUENCE  {
 *       algorithm            AlgorithmIdentifier,
 *       subjectPublicKey     BIT STRING }
 */
int mbedtls_pk_parse_subpubkey(unsigned char **p, const unsigned char *end,
                               mbedtls_pk_context *pk)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;
    mbedtls_asn1_buf alg_params;
    mbedtls_pk_type_t pk_alg = MBEDTLS_PK_NONE;
    mbedtls_ecp_group_id ec_grp_id = MBEDTLS_ECP_DP_NONE;
    const mbedtls_pk_info_t *pk_info;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    end = *p + len;

    if ((ret = pk_get_pk_alg(p, end, &pk_alg, &alg_params, &ec_grp_id)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_asn1_get_bitstring_null(p, end, &len)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_INVALID_PUBKEY, ret);
    }

    if (*p + len != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_INVALID_PUBKEY,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    if ((pk_info = mbedtls_pk_info_from_type(pk_alg)) == NULL) {
        return MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;
    }

    if ((ret = mbedtls_pk_setup(pk, pk_info)) != 0) {
        return ret;
    }

#if defined(MBEDTLS_RSA_C)
    if (pk_alg == MBEDTLS_PK_RSA) {
        ret = mbedtls_rsa_parse_pubkey(mbedtls_pk_rsa(*pk), *p, (size_t) (end - *p));
        if (ret == 0) {
            /* On success all the input has been consumed by the parsing function. */
            *p += end - *p;
        } else if ((ret <= MBEDTLS_ERR_ASN1_OUT_OF_DATA) &&
                   (ret >= MBEDTLS_ERR_ASN1_BUF_TOO_SMALL)) {
            /* In case of ASN1 error codes add MBEDTLS_ERR_PK_INVALID_PUBKEY. */
            ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_INVALID_PUBKEY, ret);
        } else {
            ret = MBEDTLS_ERR_PK_INVALID_PUBKEY;
        }
    } else
#endif /* MBEDTLS_RSA_C */
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (pk_alg == MBEDTLS_PK_ECKEY_DH || pk_alg == MBEDTLS_PK_ECKEY) {
#if defined(MBEDTLS_PK_HAVE_RFC8410_CURVES)
        if (MBEDTLS_PK_IS_RFC8410_GROUP_ID(ec_grp_id)) {
            ret = pk_use_ecparams_rfc8410(&alg_params, ec_grp_id, pk);
        } else
#endif
        {
            ret = pk_use_ecparams(&alg_params, pk);
        }
        if (ret == 0) {
            ret = mbedtls_pk_ecc_set_pubkey(pk, *p, (size_t) (end - *p));
            *p += end - *p;
        }
    } else
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */
    ret = MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;

    if (ret == 0 && *p != end) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_INVALID_PUBKEY,
                                MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    if (ret != 0) {
        mbedtls_pk_free(pk);
    }

    return ret;
}

#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
/*
 * Parse a SEC1 encoded private EC key
 */
static int pk_parse_key_sec1_der(mbedtls_pk_context *pk,
                                 const unsigned char *key, size_t keylen,
                                 int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int version, pubkey_done;
    size_t len, d_len;
    mbedtls_asn1_buf params = { 0, 0, NULL };
    unsigned char *p = (unsigned char *) key;
    unsigned char *d;
    unsigned char *end = p + keylen;
    unsigned char *end2;

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
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    end = p + len;

    if ((ret = mbedtls_asn1_get_int(&p, end, &version)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    if (version != 1) {
        return MBEDTLS_ERR_PK_KEY_INVALID_VERSION;
    }

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    /* Keep a reference to the position fo the private key. It will be used
     * later in this function. */
    d = p;
    d_len = len;

    p += len;

    pubkey_done = 0;
    if (p != end) {
        /*
         * Is 'parameters' present?
         */
        if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                        MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED |
                                        0)) == 0) {
            if ((ret = pk_get_ecparams(&p, p + len, &params)) != 0 ||
                (ret = pk_use_ecparams(&params, pk)) != 0) {
                return ret;
            }
        } else if (ret != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
        }
    }

    /*
     * Load the private key
     */
    ret = mbedtls_pk_ecc_set_key(pk, d, d_len);
    if (ret != 0) {
        return ret;
    }

    if (p != end) {
        /*
         * Is 'publickey' present? If not, or if we can't read it (eg because it
         * is compressed), create it from the private key.
         */
        if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                        MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED |
                                        1)) == 0) {
            end2 = p + len;

            if ((ret = mbedtls_asn1_get_bitstring_null(&p, end2, &len)) != 0) {
                return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
            }

            if (p + len != end2) {
                return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT,
                                         MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
            }

            if ((ret = mbedtls_pk_ecc_set_pubkey(pk, p, (size_t) (end2 - p))) == 0) {
                pubkey_done = 1;
            } else {
                /*
                 * The only acceptable failure mode of mbedtls_pk_ecc_set_pubkey() above
                 * is if the point format is not recognized.
                 */
                if (ret != MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE) {
                    return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
                }
            }
        } else if (ret != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
        }
    }

    if (!pubkey_done) {
        if ((ret = mbedtls_pk_ecc_set_pubkey_from_prv(pk, d, d_len, f_rng, p_rng)) != 0) {
            return ret;
        }
    }

    return 0;
}
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

/***********************************************************************
 *
 *      PKCS#8 parsing functions
 *
 **********************************************************************/

/*
 * Parse an unencrypted PKCS#8 encoded private key
 *
 * Notes:
 *
 * - This function does not own the key buffer. It is the
 *   responsibility of the caller to take care of zeroizing
 *   and freeing it after use.
 *
 * - The function is responsible for freeing the provided
 *   PK context on failure.
 *
 */
static int pk_parse_key_pkcs8_unencrypted_der(
    mbedtls_pk_context *pk,
    const unsigned char *key, size_t keylen,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret, version;
    size_t len;
    mbedtls_asn1_buf params;
    unsigned char *p = (unsigned char *) key;
    unsigned char *end = p + keylen;
    mbedtls_pk_type_t pk_alg = MBEDTLS_PK_NONE;
    mbedtls_ecp_group_id ec_grp_id = MBEDTLS_ECP_DP_NONE;
    const mbedtls_pk_info_t *pk_info;

#if !defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    (void) f_rng;
    (void) p_rng;
#endif

    /*
     * This function parses the PrivateKeyInfo object (PKCS#8 v1.2 = RFC 5208)
     *
     *    PrivateKeyInfo ::= SEQUENCE {
     *      version                   Version,
     *      privateKeyAlgorithm       PrivateKeyAlgorithmIdentifier,
     *      privateKey                PrivateKey,
     *      attributes           [0]  IMPLICIT Attributes OPTIONAL }
     *
     *    Version ::= INTEGER
     *    PrivateKeyAlgorithmIdentifier ::= AlgorithmIdentifier
     *    PrivateKey ::= OCTET STRING
     *
     *  The PrivateKey OCTET STRING is a SEC1 ECPrivateKey
     */

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    end = p + len;

    if ((ret = mbedtls_asn1_get_int(&p, end, &version)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    if (version != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_VERSION, ret);
    }

    if ((ret = pk_get_pk_alg(&p, end, &pk_alg, &params, &ec_grp_id)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    if (len < 1) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_OUT_OF_DATA);
    }

    if ((pk_info = mbedtls_pk_info_from_type(pk_alg)) == NULL) {
        return MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;
    }

    if ((ret = mbedtls_pk_setup(pk, pk_info)) != 0) {
        return ret;
    }

#if defined(MBEDTLS_RSA_C)
    if (pk_alg == MBEDTLS_PK_RSA) {
        if ((ret = mbedtls_rsa_parse_key(mbedtls_pk_rsa(*pk), p, len)) != 0) {
            mbedtls_pk_free(pk);
            return ret;
        }
    } else
#endif /* MBEDTLS_RSA_C */
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (pk_alg == MBEDTLS_PK_ECKEY || pk_alg == MBEDTLS_PK_ECKEY_DH) {
#if defined(MBEDTLS_PK_HAVE_RFC8410_CURVES)
        if (MBEDTLS_PK_IS_RFC8410_GROUP_ID(ec_grp_id)) {
            if ((ret =
                     pk_use_ecparams_rfc8410(&params, ec_grp_id, pk)) != 0 ||
                (ret =
                     pk_parse_key_rfc8410_der(pk, p, len, end, f_rng,
                                              p_rng)) != 0) {
                mbedtls_pk_free(pk);
                return ret;
            }
        } else
#endif
        {
            if ((ret = pk_use_ecparams(&params, pk)) != 0 ||
                (ret = pk_parse_key_sec1_der(pk, p, len, f_rng, p_rng)) != 0) {
                mbedtls_pk_free(pk);
                return ret;
            }
        }
    } else
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */
    return MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;

    end = p + len;
    if (end != (key + keylen)) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

/*
 * Parse an encrypted PKCS#8 encoded private key
 *
 * To save space, the decryption happens in-place on the given key buffer.
 * Also, while this function may modify the keybuffer, it doesn't own it,
 * and instead it is the responsibility of the caller to zeroize and properly
 * free it after use.
 *
 */
#if defined(MBEDTLS_PKCS12_C) || defined(MBEDTLS_PKCS5_C)
MBEDTLS_STATIC_TESTABLE int mbedtls_pk_parse_key_pkcs8_encrypted_der(
    mbedtls_pk_context *pk,
    unsigned char *key, size_t keylen,
    const unsigned char *pwd, size_t pwdlen,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret, decrypted = 0;
    size_t len;
    unsigned char *buf;
    unsigned char *p, *end;
    mbedtls_asn1_buf pbe_alg_oid, pbe_params;
#if defined(MBEDTLS_PKCS12_C) && defined(MBEDTLS_CIPHER_PADDING_PKCS7) && defined(MBEDTLS_CIPHER_C)
    mbedtls_cipher_type_t cipher_alg;
    mbedtls_md_type_t md_alg;
#endif
    size_t outlen = 0;

    p = key;
    end = p + keylen;

    if (pwdlen == 0) {
        return MBEDTLS_ERR_PK_PASSWORD_REQUIRED;
    }

    /*
     * This function parses the EncryptedPrivateKeyInfo object (PKCS#8)
     *
     *  EncryptedPrivateKeyInfo ::= SEQUENCE {
     *    encryptionAlgorithm  EncryptionAlgorithmIdentifier,
     *    encryptedData        EncryptedData
     *  }
     *
     *  EncryptionAlgorithmIdentifier ::= AlgorithmIdentifier
     *
     *  EncryptedData ::= OCTET STRING
     *
     *  The EncryptedData OCTET STRING is a PKCS#8 PrivateKeyInfo
     *
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    end = p + len;

    if ((ret = mbedtls_asn1_get_alg(&p, end, &pbe_alg_oid, &pbe_params)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }

    buf = p;

    /*
     * Decrypt EncryptedData with appropriate PBE
     */
#if defined(MBEDTLS_PKCS12_C) && defined(MBEDTLS_CIPHER_PADDING_PKCS7) && defined(MBEDTLS_CIPHER_C)
    if (mbedtls_oid_get_pkcs12_pbe_alg(&pbe_alg_oid, &md_alg, &cipher_alg) == 0) {
        if ((ret = mbedtls_pkcs12_pbe_ext(&pbe_params, MBEDTLS_PKCS12_PBE_DECRYPT,
                                          cipher_alg, md_alg,
                                          pwd, pwdlen, p, len, buf, len, &outlen)) != 0) {
            if (ret == MBEDTLS_ERR_PKCS12_PASSWORD_MISMATCH) {
                return MBEDTLS_ERR_PK_PASSWORD_MISMATCH;
            }

            return ret;
        }

        decrypted = 1;
    } else
#endif /* MBEDTLS_PKCS12_C && MBEDTLS_CIPHER_PADDING_PKCS7 && MBEDTLS_CIPHER_C */
#if defined(MBEDTLS_PKCS5_C) && defined(MBEDTLS_CIPHER_PADDING_PKCS7) && defined(MBEDTLS_CIPHER_C)
    if (MBEDTLS_OID_CMP(MBEDTLS_OID_PKCS5_PBES2, &pbe_alg_oid) == 0) {
        if ((ret = mbedtls_pkcs5_pbes2_ext(&pbe_params, MBEDTLS_PKCS5_DECRYPT, pwd, pwdlen,
                                           p, len, buf, len, &outlen)) != 0) {
            if (ret == MBEDTLS_ERR_PKCS5_PASSWORD_MISMATCH) {
                return MBEDTLS_ERR_PK_PASSWORD_MISMATCH;
            }

            return ret;
        }

        decrypted = 1;
    } else
#endif /* MBEDTLS_PKCS5_C && MBEDTLS_CIPHER_PADDING_PKCS7 && MBEDTLS_CIPHER_C */
    {
        ((void) pwd);
    }

    if (decrypted == 0) {
        return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
    }
    return pk_parse_key_pkcs8_unencrypted_der(pk, buf, outlen, f_rng, p_rng);
}
#endif /* MBEDTLS_PKCS12_C || MBEDTLS_PKCS5_C */

/***********************************************************************
 *
 *      Top-level functions, with format auto-discovery
 *
 **********************************************************************/

/*
 * Parse a private key
 */
int mbedtls_pk_parse_key(mbedtls_pk_context *pk,
                         const unsigned char *key, size_t keylen,
                         const unsigned char *pwd, size_t pwdlen,
                         int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const mbedtls_pk_info_t *pk_info;
#if defined(MBEDTLS_PEM_PARSE_C)
    size_t len;
    mbedtls_pem_context pem;
#endif

    if (keylen == 0) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }

#if defined(MBEDTLS_PEM_PARSE_C)
    mbedtls_pem_init(&pem);

#if defined(MBEDTLS_RSA_C)
    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (key[keylen - 1] != '\0') {
        ret = MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    } else {
        ret = mbedtls_pem_read_buffer(&pem,
                                      PEM_BEGIN_PRIVATE_KEY_RSA, PEM_END_PRIVATE_KEY_RSA,
                                      key, pwd, pwdlen, &len);
    }

    if (ret == 0) {
        pk_info = mbedtls_pk_info_from_type(MBEDTLS_PK_RSA);
        if ((ret = mbedtls_pk_setup(pk, pk_info)) != 0 ||
            (ret = mbedtls_rsa_parse_key(mbedtls_pk_rsa(*pk),
                                         pem.buf, pem.buflen)) != 0) {
            mbedtls_pk_free(pk);
        }

        mbedtls_pem_free(&pem);
        return ret;
    } else if (ret == MBEDTLS_ERR_PEM_PASSWORD_MISMATCH) {
        return MBEDTLS_ERR_PK_PASSWORD_MISMATCH;
    } else if (ret == MBEDTLS_ERR_PEM_PASSWORD_REQUIRED) {
        return MBEDTLS_ERR_PK_PASSWORD_REQUIRED;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
        return ret;
    }
#endif /* MBEDTLS_RSA_C */

#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (key[keylen - 1] != '\0') {
        ret = MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    } else {
        ret = mbedtls_pem_read_buffer(&pem,
                                      PEM_BEGIN_PRIVATE_KEY_EC,
                                      PEM_END_PRIVATE_KEY_EC,
                                      key, pwd, pwdlen, &len);
    }
    if (ret == 0) {
        pk_info = mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY);

        if ((ret = mbedtls_pk_setup(pk, pk_info)) != 0 ||
            (ret = pk_parse_key_sec1_der(pk,
                                         pem.buf, pem.buflen,
                                         f_rng, p_rng)) != 0) {
            mbedtls_pk_free(pk);
        }

        mbedtls_pem_free(&pem);
        return ret;
    } else if (ret == MBEDTLS_ERR_PEM_PASSWORD_MISMATCH) {
        return MBEDTLS_ERR_PK_PASSWORD_MISMATCH;
    } else if (ret == MBEDTLS_ERR_PEM_PASSWORD_REQUIRED) {
        return MBEDTLS_ERR_PK_PASSWORD_REQUIRED;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
        return ret;
    }
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (key[keylen - 1] != '\0') {
        ret = MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    } else {
        ret = mbedtls_pem_read_buffer(&pem,
                                      PEM_BEGIN_PRIVATE_KEY_PKCS8, PEM_END_PRIVATE_KEY_PKCS8,
                                      key, NULL, 0, &len);
    }
    if (ret == 0) {
        if ((ret = pk_parse_key_pkcs8_unencrypted_der(pk,
                                                      pem.buf, pem.buflen, f_rng, p_rng)) != 0) {
            mbedtls_pk_free(pk);
        }

        mbedtls_pem_free(&pem);
        return ret;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
        return ret;
    }

#if defined(MBEDTLS_PKCS12_C) || defined(MBEDTLS_PKCS5_C)
    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (key[keylen - 1] != '\0') {
        ret = MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    } else {
        ret = mbedtls_pem_read_buffer(&pem,
                                      PEM_BEGIN_ENCRYPTED_PRIVATE_KEY_PKCS8,
                                      PEM_END_ENCRYPTED_PRIVATE_KEY_PKCS8,
                                      key, NULL, 0, &len);
    }
    if (ret == 0) {
        if ((ret = mbedtls_pk_parse_key_pkcs8_encrypted_der(pk, pem.buf, pem.buflen,
                                                            pwd, pwdlen, f_rng, p_rng)) != 0) {
            mbedtls_pk_free(pk);
        }

        mbedtls_pem_free(&pem);
        return ret;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
        return ret;
    }
#endif /* MBEDTLS_PKCS12_C || MBEDTLS_PKCS5_C */
#else
    ((void) pwd);
    ((void) pwdlen);
#endif /* MBEDTLS_PEM_PARSE_C */

    /*
     * At this point we only know it's not a PEM formatted key. Could be any
     * of the known DER encoded private key formats
     *
     * We try the different DER format parsers to see if one passes without
     * error
     */
#if defined(MBEDTLS_PKCS12_C) || defined(MBEDTLS_PKCS5_C)
    if (pwdlen != 0) {
        unsigned char *key_copy;

        if ((key_copy = mbedtls_calloc(1, keylen)) == NULL) {
            return MBEDTLS_ERR_PK_ALLOC_FAILED;
        }

        memcpy(key_copy, key, keylen);

        ret = mbedtls_pk_parse_key_pkcs8_encrypted_der(pk, key_copy, keylen,
                                                       pwd, pwdlen, f_rng, p_rng);

        mbedtls_zeroize_and_free(key_copy, keylen);
    }

    if (ret == 0) {
        return 0;
    }

    mbedtls_pk_free(pk);
    mbedtls_pk_init(pk);

    if (ret == MBEDTLS_ERR_PK_PASSWORD_MISMATCH) {
        return ret;
    }
#endif /* MBEDTLS_PKCS12_C || MBEDTLS_PKCS5_C */

    ret = pk_parse_key_pkcs8_unencrypted_der(pk, key, keylen, f_rng, p_rng);
    if (ret == 0) {
        return 0;
    }

    mbedtls_pk_free(pk);
    mbedtls_pk_init(pk);

#if defined(MBEDTLS_RSA_C)

    pk_info = mbedtls_pk_info_from_type(MBEDTLS_PK_RSA);
    if (mbedtls_pk_setup(pk, pk_info) == 0 &&
        mbedtls_rsa_parse_key(mbedtls_pk_rsa(*pk), key, keylen) == 0) {
        return 0;
    }

    mbedtls_pk_free(pk);
    mbedtls_pk_init(pk);
#endif /* MBEDTLS_RSA_C */

#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    pk_info = mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY);
    if (mbedtls_pk_setup(pk, pk_info) == 0 &&
        pk_parse_key_sec1_der(pk,
                              key, keylen, f_rng, p_rng) == 0) {
        return 0;
    }
    mbedtls_pk_free(pk);
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */

    /* If MBEDTLS_RSA_C is defined but MBEDTLS_PK_HAVE_ECC_KEYS isn't,
     * it is ok to leave the PK context initialized but not
     * freed: It is the caller's responsibility to call pk_init()
     * before calling this function, and to call pk_free()
     * when it fails. If MBEDTLS_PK_HAVE_ECC_KEYS is defined but MBEDTLS_RSA_C
     * isn't, this leads to mbedtls_pk_free() being called
     * twice, once here and once by the caller, but this is
     * also ok and in line with the mbedtls_pk_free() calls
     * on failed PEM parsing attempts. */

    return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
}

/*
 * Parse a public key
 */
int mbedtls_pk_parse_public_key(mbedtls_pk_context *ctx,
                                const unsigned char *key, size_t keylen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p;
#if defined(MBEDTLS_RSA_C)
    const mbedtls_pk_info_t *pk_info;
#endif
#if defined(MBEDTLS_PEM_PARSE_C)
    size_t len;
    mbedtls_pem_context pem;
#endif

    if (keylen == 0) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }

#if defined(MBEDTLS_PEM_PARSE_C)
    mbedtls_pem_init(&pem);
#if defined(MBEDTLS_RSA_C)
    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (key[keylen - 1] != '\0') {
        ret = MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    } else {
        ret = mbedtls_pem_read_buffer(&pem,
                                      PEM_BEGIN_PUBLIC_KEY_RSA, PEM_END_PUBLIC_KEY_RSA,
                                      key, NULL, 0, &len);
    }

    if (ret == 0) {
        p = pem.buf;
        if ((pk_info = mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) == NULL) {
            mbedtls_pem_free(&pem);
            return MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;
        }

        if ((ret = mbedtls_pk_setup(ctx, pk_info)) != 0) {
            mbedtls_pem_free(&pem);
            return ret;
        }

        if ((ret = mbedtls_rsa_parse_pubkey(mbedtls_pk_rsa(*ctx), p, pem.buflen)) != 0) {
            mbedtls_pk_free(ctx);
        }

        mbedtls_pem_free(&pem);
        return ret;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
        mbedtls_pem_free(&pem);
        return ret;
    }
#endif /* MBEDTLS_RSA_C */

    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (key[keylen - 1] != '\0') {
        ret = MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    } else {
        ret = mbedtls_pem_read_buffer(&pem,
                                      PEM_BEGIN_PUBLIC_KEY, PEM_END_PUBLIC_KEY,
                                      key, NULL, 0, &len);
    }

    if (ret == 0) {
        /*
         * Was PEM encoded
         */
        p = pem.buf;

        ret = mbedtls_pk_parse_subpubkey(&p, p + pem.buflen, ctx);
        mbedtls_pem_free(&pem);
        return ret;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
        mbedtls_pem_free(&pem);
        return ret;
    }
    mbedtls_pem_free(&pem);
#endif /* MBEDTLS_PEM_PARSE_C */

#if defined(MBEDTLS_RSA_C)
    if ((pk_info = mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) == NULL) {
        return MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;
    }

    if ((ret = mbedtls_pk_setup(ctx, pk_info)) != 0) {
        return ret;
    }

    p = (unsigned char *) key;
    ret = mbedtls_rsa_parse_pubkey(mbedtls_pk_rsa(*ctx), p, keylen);
    if (ret == 0) {
        return ret;
    }
    mbedtls_pk_free(ctx);
    if (ret != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
        return ret;
    }
#endif /* MBEDTLS_RSA_C */
    p = (unsigned char *) key;

    ret = mbedtls_pk_parse_subpubkey(&p, p + keylen, ctx);

    return ret;
}

/***********************************************************************
 *
 *      Top-level functions, with filesystem support
 *
 **********************************************************************/

#if defined(MBEDTLS_FS_IO)
/*
 * Load all data from a file into a given buffer.
 *
 * The file is expected to contain either PEM or DER encoded data.
 * A terminating null byte is always appended. It is included in the announced
 * length only if the data looks like it is PEM encoded.
 */
int mbedtls_pk_load_file(const char *path, unsigned char **buf, size_t *n)
{
    FILE *f;
    long size;

    if ((f = fopen(path, "rb")) == NULL) {
        return MBEDTLS_ERR_PK_FILE_IO_ERROR;
    }

    /* Ensure no stdio buffering of secrets, as such buffers cannot be wiped. */
    mbedtls_setbuf(f, NULL);

    fseek(f, 0, SEEK_END);
    if ((size = ftell(f)) == -1) {
        fclose(f);
        return MBEDTLS_ERR_PK_FILE_IO_ERROR;
    }
    fseek(f, 0, SEEK_SET);

    *n = (size_t) size;

    if (*n + 1 == 0 ||
        (*buf = mbedtls_calloc(1, *n + 1)) == NULL) {
        fclose(f);
        return MBEDTLS_ERR_PK_ALLOC_FAILED;
    }

    if (fread(*buf, 1, *n, f) != *n) {
        fclose(f);

        mbedtls_zeroize_and_free(*buf, *n);

        return MBEDTLS_ERR_PK_FILE_IO_ERROR;
    }

    fclose(f);

    (*buf)[*n] = '\0';

    if (strstr((const char *) *buf, "-----BEGIN ") != NULL) {
        ++*n;
    }

    return 0;
}

/*
 * Load and parse a private key
 */
int mbedtls_pk_parse_keyfile(mbedtls_pk_context *ctx,
                             const char *path, const char *pwd,
                             int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    unsigned char *buf;

    if ((ret = mbedtls_pk_load_file(path, &buf, &n)) != 0) {
        return ret;
    }

    if (pwd == NULL) {
        ret = mbedtls_pk_parse_key(ctx, buf, n, NULL, 0, f_rng, p_rng);
    } else {
        ret = mbedtls_pk_parse_key(ctx, buf, n,
                                   (const unsigned char *) pwd, strlen(pwd), f_rng, p_rng);
    }

    mbedtls_zeroize_and_free(buf, n);

    return ret;
}

/*
 * Load and parse a public key
 */
int mbedtls_pk_parse_public_keyfile(mbedtls_pk_context *ctx, const char *path)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    unsigned char *buf;

    if ((ret = mbedtls_pk_load_file(path, &buf, &n)) != 0) {
        return ret;
    }

    ret = mbedtls_pk_parse_public_key(ctx, buf, n);

    mbedtls_zeroize_and_free(buf, n);

    return ret;
}
#endif /* MBEDTLS_FS_IO */

#endif /* MBEDTLS_PK_PARSE_C */

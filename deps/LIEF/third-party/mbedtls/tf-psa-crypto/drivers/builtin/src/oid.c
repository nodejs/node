/**
 * \file oid.c
 *
 * \brief Object Identifier (OID) database
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#include "crypto_oid.h"
#include "mbedtls/private/rsa.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/pk.h"
#if defined(MBEDTLS_PK_HAVE_PRIVATE_HEADER)
#include <mbedtls/private/pk_private.h>
#endif /* MBEDTLS_PK_HAVE_PRIVATE_HEADER */

#include <stdio.h>
#include <string.h>

#include "mbedtls/platform.h"

/*
 * Macro to automatically add the size of #define'd OIDs
 */
#define ADD_LEN(s)      s, MBEDTLS_OID_SIZE(s)

/*
 * Macro to generate mbedtls_oid_descriptor_t - the name and description
 * fields are present for historical reasons and are no longer used.
 */
#define OID_DESCRIPTOR(s, name, description)  { ADD_LEN(s) }
#define NULL_OID_DESCRIPTOR                   { NULL, 0 }

/*
 * Macro to generate an internal function for oid_XXX_from_asn1() (used by
 * the other functions)
 */
#define FN_OID_TYPED_FROM_ASN1(TYPE_T, NAME, LIST)                    \
    static const TYPE_T *oid_ ## NAME ## _from_asn1(                   \
        const mbedtls_asn1_buf *oid)     \
    {                                                                   \
        const TYPE_T *p = (LIST);                                       \
        const mbedtls_oid_descriptor_t *cur =                           \
            (const mbedtls_oid_descriptor_t *) p;                       \
        if (p == NULL || oid == NULL) return NULL;                  \
        while (cur->asn1 != NULL) {                                    \
            if (cur->asn1_len == oid->len &&                            \
                memcmp(cur->asn1, oid->p, oid->len) == 0) {          \
                return p;                                            \
            }                                                           \
            p++;                                                        \
            cur = (const mbedtls_oid_descriptor_t *) p;                 \
        }                                                               \
        return NULL;                                                 \
    }

/*
 * Macro to generate a function for retrieving a single attribute from an
 * mbedtls_oid_descriptor_t wrapper.
 */
#define FN_OID_GET_ATTR1(FN_NAME, TYPE_T, TYPE_NAME, ATTR1_TYPE, ATTR1) \
    int FN_NAME(const mbedtls_asn1_buf *oid, ATTR1_TYPE * ATTR1)                  \
    {                                                                       \
        const TYPE_T *data = oid_ ## TYPE_NAME ## _from_asn1(oid);        \
        if (data == NULL) return MBEDTLS_ERR_OID_NOT_FOUND;            \
        *ATTR1 = data->ATTR1;                                               \
        return 0;                                                        \
    }

/*
 * Macro to generate a function for retrieving two attributes from an
 * mbedtls_oid_descriptor_t wrapper.
 */
#define FN_OID_GET_ATTR2(FN_NAME, TYPE_T, TYPE_NAME, ATTR1_TYPE, ATTR1,     \
                         ATTR2_TYPE, ATTR2)                                 \
    int FN_NAME(const mbedtls_asn1_buf *oid, ATTR1_TYPE * ATTR1,               \
                ATTR2_TYPE * ATTR2)              \
    {                                                                           \
        const TYPE_T *data = oid_ ## TYPE_NAME ## _from_asn1(oid);            \
        if (data == NULL) return MBEDTLS_ERR_OID_NOT_FOUND;                 \
        *(ATTR1) = data->ATTR1;                                                 \
        *(ATTR2) = data->ATTR2;                                                 \
        return 0;                                                            \
    }

/*
 * Macro to generate a function for retrieving the OID based on a single
 * attribute from a mbedtls_oid_descriptor_t wrapper.
 */
#define FN_OID_GET_OID_BY_ATTR1(FN_NAME, TYPE_T, LIST, ATTR1_TYPE, ATTR1)   \
    int FN_NAME(ATTR1_TYPE ATTR1, const char **oid, size_t *olen)             \
    {                                                                           \
        const TYPE_T *cur = (LIST);                                             \
        while (cur->descriptor.asn1 != NULL) {                                 \
            if (cur->ATTR1 == (ATTR1)) {                                       \
                *oid = cur->descriptor.asn1;                                    \
                *olen = cur->descriptor.asn1_len;                               \
                return 0;                                                    \
            }                                                                   \
            cur++;                                                              \
        }                                                                       \
        return MBEDTLS_ERR_OID_NOT_FOUND;                                    \
    }

/* Note: while the data is shared, ideally individual functions that are used
 * only for writing or only for parsing should depend specifically on that.
 * See https://github.com/Mbed-TLS/TF-PSA-Crypto/issues/317
 */
#if defined(MBEDTLS_PK_PARSE_C) || defined(MBEDTLS_PK_WRITE_C)
/*
 * For PublicKeyInfo (PKCS1, RFC 5480)
 */
typedef struct {
    mbedtls_oid_descriptor_t    descriptor;
    mbedtls_pk_type_t           pk_alg;
} oid_pk_alg_t;

static const oid_pk_alg_t oid_pk_alg[] =
{
    {
        OID_DESCRIPTOR(MBEDTLS_OID_PKCS1_RSA,           "rsaEncryption",    "RSA"),
        MBEDTLS_PK_RSA,
    },
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_ALG_UNRESTRICTED, "id-ecPublicKey",   "Generic EC key"),
        MBEDTLS_PK_ECKEY,
    },
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_ALG_ECDH,         "id-ecDH",          "EC key for ECDH"),
        MBEDTLS_PK_ECKEY_DH,
    },
    {
        NULL_OID_DESCRIPTOR,
        MBEDTLS_PK_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_pk_alg_t, pk_alg, oid_pk_alg)
FN_OID_GET_ATTR1(mbedtls_oid_get_pk_alg, oid_pk_alg_t, pk_alg, mbedtls_pk_type_t, pk_alg)
FN_OID_GET_OID_BY_ATTR1(mbedtls_oid_get_oid_by_pk_alg,
                        oid_pk_alg_t,
                        oid_pk_alg,
                        mbedtls_pk_type_t,
                        pk_alg)

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
/*
 * For elliptic curves that use namedCurve inside ECParams (RFC 5480)
 */
typedef struct {
    mbedtls_oid_descriptor_t    descriptor;
    mbedtls_ecp_group_id        grp_id;
} oid_ecp_grp_t;

static const oid_ecp_grp_t oid_ecp_grp[] =
{
#if defined(PSA_WANT_ECC_SECP_R1_192)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_SECP192R1, "secp192r1",    "secp192r1"),
        MBEDTLS_ECP_DP_SECP192R1,
    },
#endif /* PSA_WANT_ECC_SECP_R1_192 */
#if defined(PSA_WANT_ECC_SECP_R1_256)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_SECP256R1, "secp256r1",    "secp256r1"),
        MBEDTLS_ECP_DP_SECP256R1,
    },
#endif /* PSA_WANT_ECC_SECP_R1_256 */
#if defined(PSA_WANT_ECC_SECP_R1_384)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_SECP384R1, "secp384r1",    "secp384r1"),
        MBEDTLS_ECP_DP_SECP384R1,
    },
#endif /* PSA_WANT_ECC_SECP_R1_384 */
#if defined(PSA_WANT_ECC_SECP_R1_521)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_SECP521R1, "secp521r1",    "secp521r1"),
        MBEDTLS_ECP_DP_SECP521R1,
    },
#endif /* PSA_WANT_ECC_SECP_R1_521 */
#if defined(PSA_WANT_ECC_SECP_K1_192)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_SECP192K1, "secp192k1",    "secp192k1"),
        MBEDTLS_ECP_DP_SECP192K1,
    },
#endif /* PSA_WANT_ECC_SECP_K1_192 */
#if defined(PSA_WANT_ECC_SECP_K1_256)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_SECP256K1, "secp256k1",    "secp256k1"),
        MBEDTLS_ECP_DP_SECP256K1,
    },
#endif /* PSA_WANT_ECC_SECP_K1_256 */
#if defined(PSA_WANT_ECC_BRAINPOOL_P_R1_256)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_BP256R1,   "brainpoolP256r1", "brainpool256r1"),
        MBEDTLS_ECP_DP_BP256R1,
    },
#endif /* PSA_WANT_ECC_BRAINPOOL_P_R1_256 */
#if defined(PSA_WANT_ECC_BRAINPOOL_P_R1_384)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_BP384R1,   "brainpoolP384r1", "brainpool384r1"),
        MBEDTLS_ECP_DP_BP384R1,
    },
#endif /* PSA_WANT_ECC_BRAINPOOL_P_R1_384 */
#if defined(PSA_WANT_ECC_BRAINPOOL_P_R1_512)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_EC_GRP_BP512R1,   "brainpoolP512r1", "brainpool512r1"),
        MBEDTLS_ECP_DP_BP512R1,
    },
#endif /* PSA_WANT_ECC_BRAINPOOL_P_R1_512 */
    {
        NULL_OID_DESCRIPTOR,
        MBEDTLS_ECP_DP_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_ecp_grp_t, grp_id, oid_ecp_grp)
FN_OID_GET_ATTR1(mbedtls_oid_get_ec_grp, oid_ecp_grp_t, grp_id, mbedtls_ecp_group_id, grp_id)
FN_OID_GET_OID_BY_ATTR1(mbedtls_oid_get_oid_by_ec_grp,
                        oid_ecp_grp_t,
                        oid_ecp_grp,
                        mbedtls_ecp_group_id,
                        grp_id)

/*
 * For Elliptic Curve algorithms that are directly
 * encoded in the AlgorithmIdentifier (RFC 8410)
 */
typedef struct {
    mbedtls_oid_descriptor_t    descriptor;
    mbedtls_ecp_group_id        grp_id;
} oid_ecp_grp_algid_t;

static const oid_ecp_grp_algid_t oid_ecp_grp_algid[] =
{
#if defined(PSA_WANT_ECC_MONTGOMERY_255)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_X25519,               "X25519",       "X25519"),
        MBEDTLS_ECP_DP_CURVE25519,
    },
#endif /* PSA_WANT_ECC_MONTGOMERY_255 */
#if defined(PSA_WANT_ECC_MONTGOMERY_448)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_X448,                 "X448",         "X448"),
        MBEDTLS_ECP_DP_CURVE448,
    },
#endif /* PSA_WANT_ECC_MONTGOMERY_448 */
    {
        NULL_OID_DESCRIPTOR,
        MBEDTLS_ECP_DP_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_ecp_grp_algid_t, grp_id_algid, oid_ecp_grp_algid)
FN_OID_GET_ATTR1(mbedtls_oid_get_ec_grp_algid,
                 oid_ecp_grp_algid_t,
                 grp_id_algid,
                 mbedtls_ecp_group_id,
                 grp_id)
FN_OID_GET_OID_BY_ATTR1(mbedtls_oid_get_oid_by_ec_grp_algid,
                        oid_ecp_grp_algid_t,
                        oid_ecp_grp_algid,
                        mbedtls_ecp_group_id,
                        grp_id)
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
#endif /* MBEDTLS_PK_PARSE_C || MBEDTLS_PK_WRITE_C */

/*
 * Note: the optimal dependency would also include CIPHER_C, see
 * https://github.com/Mbed-TLS/TF-PSA-Crypto/issues/317
 */
#if defined(MBEDTLS_PKCS5_C) && defined(MBEDTLS_ASN1_PARSE_C)
/*
 * For HMAC digestAlgorithm
 */
typedef struct {
    mbedtls_oid_descriptor_t    descriptor;
    mbedtls_md_type_t           md_hmac;
} oid_md_hmac_t;

static const oid_md_hmac_t oid_md_hmac[] =
{
#if defined(PSA_WANT_ALG_SHA_1)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA1,      "hmacSHA1",      "HMAC-SHA-1"),
        MBEDTLS_MD_SHA1,
    },
#endif /* PSA_WANT_ALG_SHA_1 */
#if defined(PSA_WANT_ALG_SHA_224)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA224,    "hmacSHA224",    "HMAC-SHA-224"),
        MBEDTLS_MD_SHA224,
    },
#endif /* PSA_WANT_ALG_SHA_224 */
#if defined(PSA_WANT_ALG_SHA_256)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA256,    "hmacSHA256",    "HMAC-SHA-256"),
        MBEDTLS_MD_SHA256,
    },
#endif /* PSA_WANT_ALG_SHA_256 */
#if defined(PSA_WANT_ALG_SHA_384)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA384,    "hmacSHA384",    "HMAC-SHA-384"),
        MBEDTLS_MD_SHA384,
    },
#endif /* PSA_WANT_ALG_SHA_384 */
#if defined(PSA_WANT_ALG_SHA_512)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA512,    "hmacSHA512",    "HMAC-SHA-512"),
        MBEDTLS_MD_SHA512,
    },
#endif /* PSA_WANT_ALG_SHA_512 */
#if defined(PSA_WANT_ALG_SHA3_224)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA3_224,    "hmacSHA3-224",    "HMAC-SHA3-224"),
        MBEDTLS_MD_SHA3_224,
    },
#endif /* PSA_WANT_ALG_SHA3_224 */
#if defined(PSA_WANT_ALG_SHA3_256)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA3_256,    "hmacSHA3-256",    "HMAC-SHA3-256"),
        MBEDTLS_MD_SHA3_256,
    },
#endif /* PSA_WANT_ALG_SHA3_256 */
#if defined(PSA_WANT_ALG_SHA3_384)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA3_384,    "hmacSHA3-384",    "HMAC-SHA3-384"),
        MBEDTLS_MD_SHA3_384,
    },
#endif /* PSA_WANT_ALG_SHA3_384 */
#if defined(PSA_WANT_ALG_SHA3_512)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_SHA3_512,    "hmacSHA3-512",    "HMAC-SHA3-512"),
        MBEDTLS_MD_SHA3_512,
    },
#endif /* PSA_WANT_ALG_SHA3_512 */
#if defined(PSA_WANT_ALG_RIPEMD160)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_HMAC_RIPEMD160,    "hmacRIPEMD160",    "HMAC-RIPEMD160"),
        MBEDTLS_MD_RIPEMD160,
    },
#endif /* PSA_WANT_ALG_RIPEMD160 */
    {
        NULL_OID_DESCRIPTOR,
        MBEDTLS_MD_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_md_hmac_t, md_hmac, oid_md_hmac)
FN_OID_GET_ATTR1(mbedtls_oid_get_md_hmac, oid_md_hmac_t, md_hmac, mbedtls_md_type_t, md_hmac)

#if defined(MBEDTLS_CIPHER_C)
/*
 * For PKCS#5 PBES2 encryption algorithm
 */
typedef struct {
    mbedtls_oid_descriptor_t    descriptor;
    mbedtls_cipher_type_t       cipher_alg;
} oid_cipher_alg_t;

static const oid_cipher_alg_t oid_cipher_alg[] =
{
    {
        OID_DESCRIPTOR(MBEDTLS_OID_AES_128_CBC,          "aes128-cbc", "AES128-CBC"),
        MBEDTLS_CIPHER_AES_128_CBC,
    },
    {
        OID_DESCRIPTOR(MBEDTLS_OID_AES_192_CBC,          "aes192-cbc", "AES192-CBC"),
        MBEDTLS_CIPHER_AES_192_CBC,
    },
    {
        OID_DESCRIPTOR(MBEDTLS_OID_AES_256_CBC,          "aes256-cbc", "AES256-CBC"),
        MBEDTLS_CIPHER_AES_256_CBC,
    },
    {
        NULL_OID_DESCRIPTOR,
        MBEDTLS_CIPHER_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_cipher_alg_t, cipher_alg, oid_cipher_alg)
FN_OID_GET_ATTR1(mbedtls_oid_get_cipher_alg,
                 oid_cipher_alg_t,
                 cipher_alg,
                 mbedtls_cipher_type_t,
                 cipher_alg)
#endif /* MBEDTLS_CIPHER_C */
#endif /* MBEDTLS_PKCS5_C && MBEDTLS_ASN1_PARSE_C */

#if defined(MBEDTLS_RSA_C) && defined(MBEDTLS_PKCS1_V15)
/*
 * For digestAlgorithm
 */
typedef struct {
    mbedtls_oid_descriptor_t    descriptor;
    mbedtls_md_type_t           md_alg;
} oid_md_alg_t;

static const oid_md_alg_t oid_md_alg[] =
{
#if defined(PSA_WANT_ALG_MD5)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_MD5,       "id-md5",       "MD5"),
        MBEDTLS_MD_MD5,
    },
#endif
#if defined(PSA_WANT_ALG_SHA_1)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA1,      "id-sha1",      "SHA-1"),
        MBEDTLS_MD_SHA1,
    },
#endif
#if defined(PSA_WANT_ALG_SHA_224)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA224,    "id-sha224",    "SHA-224"),
        MBEDTLS_MD_SHA224,
    },
#endif
#if defined(PSA_WANT_ALG_SHA_256)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA256,    "id-sha256",    "SHA-256"),
        MBEDTLS_MD_SHA256,
    },
#endif
#if defined(PSA_WANT_ALG_SHA_384)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA384,    "id-sha384",    "SHA-384"),
        MBEDTLS_MD_SHA384,
    },
#endif
#if defined(PSA_WANT_ALG_SHA_512)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA512,    "id-sha512",    "SHA-512"),
        MBEDTLS_MD_SHA512,
    },
#endif
#if defined(PSA_WANT_ALG_RIPEMD160)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_RIPEMD160, "id-ripemd160", "RIPEMD-160"),
        MBEDTLS_MD_RIPEMD160,
    },
#endif
#if defined(PSA_WANT_ALG_SHA3_224)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA3_224,    "id-sha3-224",    "SHA-3-224"),
        MBEDTLS_MD_SHA3_224,
    },
#endif
#if defined(PSA_WANT_ALG_SHA3_256)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA3_256,    "id-sha3-256",    "SHA-3-256"),
        MBEDTLS_MD_SHA3_256,
    },
#endif
#if defined(PSA_WANT_ALG_SHA3_384)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA3_384,    "id-sha3-384",    "SHA-3-384"),
        MBEDTLS_MD_SHA3_384,
    },
#endif
#if defined(PSA_WANT_ALG_SHA3_512)
    {
        OID_DESCRIPTOR(MBEDTLS_OID_DIGEST_ALG_SHA3_512,    "id-sha3-512",    "SHA-3-512"),
        MBEDTLS_MD_SHA3_512,
    },
#endif
    {
        NULL_OID_DESCRIPTOR,
        MBEDTLS_MD_NONE,
    },
};

FN_OID_GET_OID_BY_ATTR1(mbedtls_oid_get_oid_by_md,
                        oid_md_alg_t,
                        oid_md_alg,
                        mbedtls_md_type_t,
                        md_alg)
#endif /* MBEDTLS_RSA_C && MBEDTLS_PKCS1_V15 */

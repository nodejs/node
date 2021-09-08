/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/deprecated.h"

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include <openssl/core_names.h>
#include <openssl/obj_mac.h>
#include "prov/securitycheck.h"

/*
 * FIPS requires a minimum security strength of 112 bits (for encryption or
 * signing), and for legacy purposes 80 bits (for decryption or verifying).
 * Set protect = 1 for encryption or signing operations, or 0 otherwise. See
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-131Ar2.pdf.
 */
int ossl_rsa_check_key(OSSL_LIB_CTX *ctx, const RSA *rsa, int operation)
{
    int protect = 0;

    switch (operation) {
        case EVP_PKEY_OP_SIGN:
            protect = 1;
            /* fallthrough */
        case EVP_PKEY_OP_VERIFY:
            break;
        case EVP_PKEY_OP_ENCAPSULATE:
        case EVP_PKEY_OP_ENCRYPT:
            protect = 1;
            /* fallthrough */
        case EVP_PKEY_OP_VERIFYRECOVER:
        case EVP_PKEY_OP_DECAPSULATE:
        case EVP_PKEY_OP_DECRYPT:
            if (RSA_test_flags(rsa,
                               RSA_FLAG_TYPE_MASK) == RSA_FLAG_TYPE_RSASSAPSS) {
                ERR_raise_data(ERR_LIB_PROV,
                               PROV_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE,
                               "operation: %d", operation);
                return 0;
            }
            break;
        default:
            ERR_raise_data(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR,
                           "invalid operation: %d", operation);
            return 0;
    }

#if !defined(OPENSSL_NO_FIPS_SECURITYCHECKS)
    if (ossl_securitycheck_enabled(ctx)) {
        int sz = RSA_bits(rsa);

        if (protect ? (sz < 2048) : (sz < 1024)) {
            ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH,
                           "operation: %d", operation);
            return 0;
        }
    }
#else
    /* make protect used */
    (void)protect;
#endif /* OPENSSL_NO_FIPS_SECURITYCHECKS */
    return 1;
}

#ifndef OPENSSL_NO_EC
/*
 * In FIPS mode:
 * protect should be 1 for any operations that need 112 bits of security
 * strength (such as signing, and key exchange), or 0 for operations that allow
 * a lower security strength (such as verify).
 *
 * For ECDH key agreement refer to SP800-56A
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-56Ar3.pdf
 * "Appendix D"
 *
 * For ECDSA signatures refer to
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-131Ar2.pdf
 * "Table 2"
 */
int ossl_ec_check_key(OSSL_LIB_CTX *ctx, const EC_KEY *ec, int protect)
{
# if !defined(OPENSSL_NO_FIPS_SECURITYCHECKS)
    if (ossl_securitycheck_enabled(ctx)) {
        int nid, strength;
        const char *curve_name;
        const EC_GROUP *group = EC_KEY_get0_group(ec);

        if (group == NULL) {
            ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_CURVE, "No group");
            return 0;
        }
        nid = EC_GROUP_get_curve_name(group);
        if (nid == NID_undef) {
            ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_CURVE,
                           "Explicit curves are not allowed in fips mode");
            return 0;
        }

        curve_name = EC_curve_nid2nist(nid);
        if (curve_name == NULL) {
            ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_CURVE,
                           "Curve %s is not approved in FIPS mode", curve_name);
            return 0;
        }

        /*
         * For EC the security strength is the (order_bits / 2)
         * e.g. P-224 is 112 bits.
         */
        strength = EC_GROUP_order_bits(group) / 2;
        /* The min security strength allowed for legacy verification is 80 bits */
        if (strength < 80) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_CURVE);
            return 0;
        }

        /*
         * For signing or key agreement only allow curves with at least 112 bits of
         * security strength
         */
        if (protect && strength < 112) {
            ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_CURVE,
                           "Curve %s cannot be used for signing", curve_name);
            return 0;
        }
    }
# endif /* OPENSSL_NO_FIPS_SECURITYCHECKS */
    return 1;
}
#endif /* OPENSSL_NO_EC */

#ifndef OPENSSL_NO_DSA
/*
 * Check for valid key sizes if fips mode. Refer to
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-131Ar2.pdf
 * "Table 2"
 */
int ossl_dsa_check_key(OSSL_LIB_CTX *ctx, const DSA *dsa, int sign)
{
# if !defined(OPENSSL_NO_FIPS_SECURITYCHECKS)
    if (ossl_securitycheck_enabled(ctx)) {
        size_t L, N;
        const BIGNUM *p, *q;

        if (dsa == NULL)
            return 0;

        p = DSA_get0_p(dsa);
        q = DSA_get0_q(dsa);
        if (p == NULL || q == NULL)
            return 0;

        L = BN_num_bits(p);
        N = BN_num_bits(q);

        /*
         * For Digital signature verification DSA keys with < 112 bits of
         * security strength (i.e L < 2048 bits), are still allowed for legacy
         * use. The bounds given in SP800 131Ar2 - Table 2 are
         * (512 <= L < 2048 and 160 <= N < 224)
         */
        if (!sign && L < 2048)
            return (L >= 512 && N >= 160 && N < 224);

         /* Valid sizes for both sign and verify */
        if (L == 2048 && (N == 224 || N == 256))
            return 1;
        return (L == 3072 && N == 256);
    }
# endif /* OPENSSL_NO_FIPS_SECURITYCHECKS */
    return 1;
}
#endif /* OPENSSL_NO_DSA */

#ifndef OPENSSL_NO_DH
/*
 * For DH key agreement refer to SP800-56A
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-56Ar3.pdf
 * "Section 5.5.1.1FFC Domain Parameter Selection/Generation" and
 * "Appendix D" FFC Safe-prime Groups
 */
int ossl_dh_check_key(OSSL_LIB_CTX *ctx, const DH *dh)
{
# if !defined(OPENSSL_NO_FIPS_SECURITYCHECKS)
    if (ossl_securitycheck_enabled(ctx)) {
        size_t L, N;
        const BIGNUM *p, *q;

        if (dh == NULL)
            return 0;

        p = DH_get0_p(dh);
        q = DH_get0_q(dh);
        if (p == NULL || q == NULL)
            return 0;

        L = BN_num_bits(p);
        if (L < 2048)
            return 0;

        /* If it is a safe prime group then it is ok */
        if (DH_get_nid(dh))
            return 1;

        /* If not then it must be FFC, which only allows certain sizes. */
        N = BN_num_bits(q);

        return (L == 2048 && (N == 224 || N == 256));
    }
# endif /* OPENSSL_NO_FIPS_SECURITYCHECKS */
    return 1;
}
#endif /* OPENSSL_NO_DH */

int ossl_digest_get_approved_nid_with_sha1(OSSL_LIB_CTX *ctx, const EVP_MD *md,
                                           int sha1_allowed)
{
    int mdnid = ossl_digest_get_approved_nid(md);

# if !defined(OPENSSL_NO_FIPS_SECURITYCHECKS)
    if (ossl_securitycheck_enabled(ctx)) {
        if (mdnid == NID_undef || (mdnid == NID_sha1 && !sha1_allowed))
            mdnid = -1; /* disallowed by security checks */
    }
# endif /* OPENSSL_NO_FIPS_SECURITYCHECKS */
    return mdnid;
}

int ossl_digest_is_allowed(OSSL_LIB_CTX *ctx, const EVP_MD *md)
{
# if !defined(OPENSSL_NO_FIPS_SECURITYCHECKS)
    if (ossl_securitycheck_enabled(ctx))
        return ossl_digest_get_approved_nid(md) != NID_undef;
# endif /* OPENSSL_NO_FIPS_SECURITYCHECKS */
    return 1;
}

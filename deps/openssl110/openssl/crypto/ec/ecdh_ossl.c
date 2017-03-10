/*
 * Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * The Elliptic Curve Public-Key Crypto Library (ECC Code) included
 * herein is developed by SUN MICROSYSTEMS, INC., and is contributed
 * to the OpenSSL project.
 *
 * The ECC Code is licensed pursuant to the OpenSSL open source
 * license provided below.
 *
 * The ECDH software is originally written by Douglas Stebila of
 * Sun Microsystems Laboratories.
 *
 */

#include <string.h>
#include <limits.h>

#include "internal/cryptlib.h"

#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/objects.h>
#include <openssl/ec.h>
#include "ec_lcl.h"

int ossl_ecdh_compute_key(unsigned char **psec, size_t *pseclen,
                          const EC_POINT *pub_key, const EC_KEY *ecdh)
{
    if (ecdh->group->meth->ecdh_compute_key == NULL) {
        ECerr(EC_F_OSSL_ECDH_COMPUTE_KEY, EC_R_CURVE_DOES_NOT_SUPPORT_ECDH);
        return 0;
    }

    return ecdh->group->meth->ecdh_compute_key(psec, pseclen, pub_key, ecdh);
}

/*-
 * This implementation is based on the following primitives in the IEEE 1363 standard:
 *  - ECKAS-DH1
 *  - ECSVDP-DH
 */
int ecdh_simple_compute_key(unsigned char **pout, size_t *poutlen,
                            const EC_POINT *pub_key, const EC_KEY *ecdh)
{
    BN_CTX *ctx;
    EC_POINT *tmp = NULL;
    BIGNUM *x = NULL, *y = NULL;
    const BIGNUM *priv_key;
    const EC_GROUP *group;
    int ret = 0;
    size_t buflen, len;
    unsigned char *buf = NULL;

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;
    BN_CTX_start(ctx);
    x = BN_CTX_get(ctx);
    y = BN_CTX_get(ctx);

    priv_key = EC_KEY_get0_private_key(ecdh);
    if (priv_key == NULL) {
        ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, EC_R_NO_PRIVATE_VALUE);
        goto err;
    }

    group = EC_KEY_get0_group(ecdh);

    if (EC_KEY_get_flags(ecdh) & EC_FLAG_COFACTOR_ECDH) {
        if (!EC_GROUP_get_cofactor(group, x, NULL) ||
            !BN_mul(x, x, priv_key, ctx)) {
            ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        priv_key = x;
    }

    if ((tmp = EC_POINT_new(group)) == NULL) {
        ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!EC_POINT_mul(group, tmp, NULL, pub_key, priv_key, ctx)) {
        ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, EC_R_POINT_ARITHMETIC_FAILURE);
        goto err;
    }

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) ==
        NID_X9_62_prime_field) {
        if (!EC_POINT_get_affine_coordinates_GFp(group, tmp, x, y, ctx)) {
            ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, EC_R_POINT_ARITHMETIC_FAILURE);
            goto err;
        }
    }
#ifndef OPENSSL_NO_EC2M
    else {
        if (!EC_POINT_get_affine_coordinates_GF2m(group, tmp, x, y, ctx)) {
            ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, EC_R_POINT_ARITHMETIC_FAILURE);
            goto err;
        }
    }
#endif

    buflen = (EC_GROUP_get_degree(group) + 7) / 8;
    len = BN_num_bytes(x);
    if (len > buflen) {
        ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if ((buf = OPENSSL_malloc(buflen)) == NULL) {
        ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    memset(buf, 0, buflen - len);
    if (len != (size_t)BN_bn2bin(x, buf + buflen - len)) {
        ECerr(EC_F_ECDH_SIMPLE_COMPUTE_KEY, ERR_R_BN_LIB);
        goto err;
    }

    *pout = buf;
    *poutlen = buflen;
    buf = NULL;

    ret = 1;

 err:
    EC_POINT_free(tmp);
    if (ctx)
        BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    OPENSSL_free(buf);
    return ret;
}

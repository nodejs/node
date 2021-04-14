/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/types.h"

/* Functions that are common */
int ossl_rsa_check_key(OSSL_LIB_CTX *ctx, const RSA *rsa, int operation);
int ossl_ec_check_key(OSSL_LIB_CTX *ctx, const EC_KEY *ec, int protect);
int ossl_dsa_check_key(OSSL_LIB_CTX *ctx, const DSA *dsa, int sign);
int ossl_dh_check_key(OSSL_LIB_CTX *ctx, const DH *dh);

int ossl_digest_is_allowed(OSSL_LIB_CTX *ctx, const EVP_MD *md);
/* With security check enabled it can return -1 to indicate disallowed md */
int ossl_digest_get_approved_nid_with_sha1(OSSL_LIB_CTX *ctx, const EVP_MD *md,
                                           int sha1_allowed);

/* Functions that are common */
int ossl_digest_md_to_nid(const EVP_MD *md, const OSSL_ITEM *it, size_t it_len);
int ossl_digest_get_approved_nid(const EVP_MD *md);

/* Functions that have different implementations for the FIPS_MODULE */
int ossl_digest_rsa_sign_get_md_nid(OSSL_LIB_CTX *ctx, const EVP_MD *md,
                                    int sha1_allowed);
int ossl_securitycheck_enabled(OSSL_LIB_CTX *libctx);

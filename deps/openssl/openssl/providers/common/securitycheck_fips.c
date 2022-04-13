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
#include <openssl/err.h>
#include <openssl/proverr.h>
#include <openssl/core_names.h>
#include <openssl/obj_mac.h>
#include "prov/securitycheck.h"

int FIPS_security_check_enabled(OSSL_LIB_CTX *libctx);

int ossl_securitycheck_enabled(OSSL_LIB_CTX *libctx)
{
#if !defined(OPENSSL_NO_FIPS_SECURITYCHECKS)
    return FIPS_security_check_enabled(libctx);
#else
    return 0;
#endif /* OPENSSL_NO_FIPS_SECURITYCHECKS */
}

int ossl_digest_rsa_sign_get_md_nid(OSSL_LIB_CTX *ctx, const EVP_MD *md,
                                    int sha1_allowed)
{
#if !defined(OPENSSL_NO_FIPS_SECURITYCHECKS)
    if (ossl_securitycheck_enabled(ctx))
        return ossl_digest_get_approved_nid_with_sha1(ctx, md, sha1_allowed);
#endif /* OPENSSL_NO_FIPS_SECURITYCHECKS */
    return ossl_digest_get_approved_nid(md);
}

/*
 * Copyright 2019-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include "prov/digestcommon.h"
#include "internal/common.h"

#include "providers/implementations/digests/digestcommon.inc"

int ossl_digest_default_get_params(OSSL_PARAM params[], size_t blksz,
                                   size_t paramsz, unsigned long flags)
{
    struct digest_default_get_params_st p;

    if (!digest_default_get_params_decoder(params, &p))
        return 0;

    if (p.bsize != NULL && !OSSL_PARAM_set_size_t(p.bsize, blksz)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    if (p.size != NULL && !OSSL_PARAM_set_size_t(p.size, paramsz)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    if (p.xof != NULL
        && !OSSL_PARAM_set_int(p.xof, (flags & PROV_DIGEST_FLAG_XOF) != 0)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    if (p.aldid != NULL
        && !OSSL_PARAM_set_int(p.aldid, (flags & PROV_DIGEST_FLAG_ALGID_ABSENT) != 0)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    return 1;
}

const OSSL_PARAM *ossl_digest_default_gettable_params(void *provctx)
{
    return digest_default_get_params_list;
}

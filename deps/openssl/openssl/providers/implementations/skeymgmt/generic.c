/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/proverr.h>
#include "crypto/types.h"
#include "internal/cryptlib.h"
#include "internal/skey.h"
#include "prov/provider_ctx.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/skeymgmt_lcl.h"

#include "providers/implementations/skeymgmt/generic.inc"

void generic_free(void *keydata)
{
    PROV_SKEY *generic = keydata;

    if (generic == NULL)
        return;

    OPENSSL_clear_free(generic->data, generic->length);
    OPENSSL_free(generic);
}

void *generic_import(void *provctx, int selection, const OSSL_PARAM params[])
{
    OSSL_LIB_CTX *libctx = PROV_LIBCTX_OF(provctx);
    struct generic_skey_import_st p;
    PROV_SKEY *generic = NULL;
    int ok = 0;

    if (!ossl_prov_is_running())
        return NULL;

    if ((selection & OSSL_SKEYMGMT_SELECT_SECRET_KEY) == 0)
        return NULL;

    if (!generic_skey_import_decoder(params, &p))
        return NULL;

    if (p.raw_bytes == NULL
            || p.raw_bytes->data_type != OSSL_PARAM_OCTET_STRING)
        return NULL;

    generic = OPENSSL_zalloc(sizeof(PROV_SKEY));
    if (generic == NULL)
        return NULL;

    generic->libctx = libctx;

    generic->type = SKEY_TYPE_GENERIC;

    if ((generic->data = OPENSSL_memdup(p.raw_bytes->data,
                                        p.raw_bytes->data_size)) == NULL)
        goto end;
    generic->length = p.raw_bytes->data_size;
    ok = 1;

end:
    if (ok == 0) {
        generic_free(generic);
        generic = NULL;
    }
    return generic;
}

const OSSL_PARAM *generic_imp_settable_params(void *provctx)
{
    return generic_skey_import_list;
}

int generic_export(void *keydata, int selection,
                   OSSL_CALLBACK *param_callback, void *cbarg)
{
    PROV_SKEY *gen = keydata;
    OSSL_PARAM params[2];

    if (!ossl_prov_is_running() || gen == NULL)
        return 0;

    /* If we use generic SKEYMGMT as a "base class", we shouldn't check the type */
    if ((selection & OSSL_SKEYMGMT_SELECT_SECRET_KEY) == 0)
        return 0;

    params[0] = OSSL_PARAM_construct_octet_string(OSSL_SKEY_PARAM_RAW_BYTES,
                                                  gen->data, gen->length);
    params[1] = OSSL_PARAM_construct_end();

    return param_callback(params, cbarg);
}

const OSSL_DISPATCH ossl_generic_skeymgmt_functions[] = {
    { OSSL_FUNC_SKEYMGMT_FREE, (void (*)(void))generic_free },
    { OSSL_FUNC_SKEYMGMT_IMPORT, (void (*)(void))generic_import },
    { OSSL_FUNC_SKEYMGMT_EXPORT, (void (*)(void))generic_export },
    { OSSL_FUNC_SKEYMGMT_IMP_SETTABLE_PARAMS,
      (void (*)(void))generic_imp_settable_params },
    OSSL_DISPATCH_END
};

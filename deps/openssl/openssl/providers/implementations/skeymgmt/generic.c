/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
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
    OPENSSL_free(generic->alias);
    OPENSSL_free(generic->local_keyid);
    OPENSSL_free(generic->algorithm_oid);
    OPENSSL_free(generic->algorithm_params);
    OPENSSL_free(generic);
}

static int generic_import_metadata(PROV_SKEY *skey,
    const struct generic_skey_import_st *p)
{
    if (p->alias != NULL
        && p->alias->data_type == OSSL_PARAM_UTF8_STRING
        && skey->alias == NULL) {
        skey->alias = OPENSSL_strndup(p->alias->data, p->alias->data_size);
        if (skey->alias == NULL)
            return 0;
        if (strlen(skey->alias) != p->alias->data_size) {
            ERR_raise(ERR_R_PROV_LIB, PROV_R_INVALID_DATA);
            return 0;
        }
    }

    if (p->local_keyid != NULL
        && p->local_keyid->data_type == OSSL_PARAM_OCTET_STRING
        && skey->local_keyid == NULL) {
        skey->local_keyid = OPENSSL_memdup(p->local_keyid->data,
            p->local_keyid->data_size);
        if (skey->local_keyid == NULL)
            return 0;
        skey->local_keyid_len = p->local_keyid->data_size;
    }

    if (p->algorithm_oid != NULL
        && p->algorithm_oid->data_type == OSSL_PARAM_OCTET_STRING
        && skey->algorithm_oid == NULL) {
        skey->algorithm_oid = OPENSSL_memdup(p->algorithm_oid->data,
            p->algorithm_oid->data_size);
        if (skey->algorithm_oid == NULL)
            return 0;
        skey->algorithm_oid_len = p->algorithm_oid->data_size;
    }

    if (p->algorithm_params != NULL
        && p->algorithm_params->data_type == OSSL_PARAM_OCTET_STRING
        && skey->algorithm_params == NULL) {
        skey->algorithm_params = OPENSSL_memdup(p->algorithm_params->data,
            p->algorithm_params->data_size);
        if (skey->algorithm_params == NULL)
            return 0;
        skey->algorithm_params_len = p->algorithm_params->data_size;
    }

    return 1;
}

void *generic_import(void *provctx, int selection ossl_unused, const OSSL_PARAM params[])
{
    OSSL_LIB_CTX *libctx = PROV_LIBCTX_OF(provctx);
    struct generic_skey_import_st p;
    PROV_SKEY *generic = NULL;
    int ok = 0;

    if (!ossl_prov_is_running())
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
             p.raw_bytes->data_size))
        == NULL)
        goto end;
    generic->length = p.raw_bytes->data_size;

    if (!generic_import_metadata(generic, &p))
        goto end;

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
    OSSL_PARAM params[6];
    int idx = 0;

    if (!ossl_prov_is_running() || gen == NULL || selection == 0)
        return 0;

    /* If we use generic SKEYMGMT as a "base class", we shouldn't check the type */
    if ((selection & OSSL_SKEYMGMT_SELECT_SECRET_KEY) != 0)
        params[idx++] = OSSL_PARAM_construct_octet_string(OSSL_SKEY_PARAM_RAW_BYTES,
            gen->data, gen->length);

    if ((selection & OSSL_SKEYMGMT_SELECT_PARAMETERS) != 0) {
        if (gen->alias != NULL)
            params[idx++] = OSSL_PARAM_construct_utf8_string(
                OSSL_SKEY_PARAM_ALIAS, gen->alias, 0);

        if (gen->local_keyid != NULL)
            params[idx++] = OSSL_PARAM_construct_octet_string(
                OSSL_SKEY_PARAM_LOCAL_KEYID,
                gen->local_keyid, gen->local_keyid_len);

        if (gen->algorithm_oid != NULL)
            params[idx++] = OSSL_PARAM_construct_octet_string(
                OSSL_SKEY_PARAM_ALGORITHM_OID,
                gen->algorithm_oid, gen->algorithm_oid_len);

        if (gen->algorithm_params != NULL)
            params[idx++] = OSSL_PARAM_construct_octet_string(
                OSSL_SKEY_PARAM_ALGORITHM_PARAMS,
                gen->algorithm_params, gen->algorithm_params_len);
    }

    params[idx] = OSSL_PARAM_construct_end();

    return param_callback(params, cbarg);
}

const char *generic_get_key_id(void *keydata)
{
    PROV_SKEY *gen = keydata;

    if (gen == NULL)
        return NULL;

    return gen->alias;
}

int generic_get_local_keyid(void *keydata,
    const unsigned char **id, size_t *len)
{
    PROV_SKEY *gen = keydata;

    if (gen == NULL)
        return 0;
    if (id == NULL || len == NULL)
        return 0;

    *id = gen->local_keyid;
    *len = gen->local_keyid_len;
    return 1;
}

int generic_get_algorithm_id(void *keydata,
    const unsigned char **oid, size_t *oid_len,
    const unsigned char **params, size_t *params_len)
{
    PROV_SKEY *gen = keydata;
    int ret_oid = 0, ret_params = 0;

    if (gen == NULL)
        return 0;

    if (oid != NULL && oid_len != NULL)
        ret_oid = 1;
    if (params != NULL && params_len != NULL)
        ret_params = 1;

    if (ret_oid == 0 && ret_params == 0)
        return 0;

    if (ret_oid == 1) {
        *oid = gen->algorithm_oid;
        *oid_len = gen->algorithm_oid_len;
    }
    if (ret_params == 1) {
        *params = gen->algorithm_params;
        *params_len = gen->algorithm_params_len;
    }
    return 1;
}

const OSSL_DISPATCH ossl_generic_skeymgmt_functions[] = {
    { OSSL_FUNC_SKEYMGMT_FREE, (void (*)(void))generic_free },
    { OSSL_FUNC_SKEYMGMT_IMPORT, (void (*)(void))generic_import },
    { OSSL_FUNC_SKEYMGMT_EXPORT, (void (*)(void))generic_export },
    { OSSL_FUNC_SKEYMGMT_IMP_SETTABLE_PARAMS,
        (void (*)(void))generic_imp_settable_params },
    { OSSL_FUNC_SKEYMGMT_GET_KEY_ID, (void (*)(void))generic_get_key_id },
    { OSSL_FUNC_SKEYMGMT_GET_LOCAL_KEYID,
        (void (*)(void))generic_get_local_keyid },
    { OSSL_FUNC_SKEYMGMT_GET_ALGORITHM_ID,
        (void (*)(void))generic_get_algorithm_id },
    OSSL_DISPATCH_END
};

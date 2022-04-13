/*
 * Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */

#include <string.h>
#include <openssl/core_names.h>
#include <openssl/rand.h>
#include <openssl/provider.h>
#include "testutil.h"
#include "fake_rsaprov.h"

static OSSL_FUNC_keymgmt_new_fn fake_rsa_keymgmt_new;
static OSSL_FUNC_keymgmt_free_fn fake_rsa_keymgmt_free;
static OSSL_FUNC_keymgmt_has_fn fake_rsa_keymgmt_has;
static OSSL_FUNC_keymgmt_query_operation_name_fn fake_rsa_keymgmt_query;
static OSSL_FUNC_keymgmt_import_fn fake_rsa_keymgmt_import;
static OSSL_FUNC_keymgmt_import_types_fn fake_rsa_keymgmt_imptypes;

static int has_selection;
static int imptypes_selection;
static int query_id;

static void *fake_rsa_keymgmt_new(void *provctx)
{
    unsigned char *keydata = OPENSSL_zalloc(1);

    TEST_ptr(keydata);

    /* clear test globals */
    has_selection = 0;
    imptypes_selection = 0;
    query_id = 0;

    return keydata;
}

static void fake_rsa_keymgmt_free(void *keydata)
{
    OPENSSL_free(keydata);
}

static int fake_rsa_keymgmt_has(const void *key, int selection)
{
    /* record global for checking */
    has_selection = selection;

    return 1;
}


static const char *fake_rsa_keymgmt_query(int id)
{
    /* record global for checking */
    query_id = id;

    return "RSA";
}

static int fake_rsa_keymgmt_import(void *keydata, int selection,
                                   const OSSL_PARAM *p)
{
    unsigned char *fake_rsa_key = keydata;

    /* key was imported */
    *fake_rsa_key = 1;

    return 1;
}

static const OSSL_PARAM fake_rsa_import_key_types[] = {
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_N, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_E, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_D, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_FACTOR1, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_FACTOR2, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_EXPONENT1, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_EXPONENT2, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_COEFFICIENT1, NULL, 0),
    OSSL_PARAM_END
};

static const OSSL_PARAM *fake_rsa_keymgmt_imptypes(int selection)
{
    /* record global for checking */
    imptypes_selection = selection;

    return fake_rsa_import_key_types;
}

static const OSSL_DISPATCH fake_rsa_keymgmt_funcs[] = {
    { OSSL_FUNC_KEYMGMT_NEW, (void (*)(void))fake_rsa_keymgmt_new },
    { OSSL_FUNC_KEYMGMT_FREE, (void (*)(void))fake_rsa_keymgmt_free} ,
    { OSSL_FUNC_KEYMGMT_HAS, (void (*)(void))fake_rsa_keymgmt_has },
    { OSSL_FUNC_KEYMGMT_QUERY_OPERATION_NAME,
        (void (*)(void))fake_rsa_keymgmt_query },
    { OSSL_FUNC_KEYMGMT_IMPORT, (void (*)(void))fake_rsa_keymgmt_import },
    { OSSL_FUNC_KEYMGMT_IMPORT_TYPES,
        (void (*)(void))fake_rsa_keymgmt_imptypes },
    { 0, NULL }
};

static const OSSL_ALGORITHM fake_rsa_keymgmt_algs[] = {
    { "RSA:rsaEncryption", "provider=fake-rsa", fake_rsa_keymgmt_funcs, "Fake RSA Key Management" },
    { NULL, NULL, NULL, NULL }
};

static OSSL_FUNC_signature_newctx_fn fake_rsa_sig_newctx;
static OSSL_FUNC_signature_freectx_fn fake_rsa_sig_freectx;
static OSSL_FUNC_signature_sign_init_fn fake_rsa_sig_sign_init;
static OSSL_FUNC_signature_sign_fn fake_rsa_sig_sign;

static void *fake_rsa_sig_newctx(void *provctx, const char *propq)
{
    unsigned char *sigctx = OPENSSL_zalloc(1);

    TEST_ptr(sigctx);

    return sigctx;
}

static void fake_rsa_sig_freectx(void *sigctx)
{
    OPENSSL_free(sigctx);
}

static int fake_rsa_sig_sign_init(void *ctx, void *provkey,
                                  const OSSL_PARAM params[])
{
    unsigned char *sigctx = ctx;
    unsigned char *keydata = provkey;

    /* we must have a ctx */
    if (!TEST_ptr(sigctx))
        return 0;

    /* we must have some initialized key */
    if (!TEST_ptr(keydata) || !TEST_int_gt(keydata[0], 0))
        return 0;

    /* record that sign init was called */
    *sigctx = 1;
    return 1;
}

static int fake_rsa_sig_sign(void *ctx, unsigned char *sig,
                             size_t *siglen, size_t sigsize,
                             const unsigned char *tbs, size_t tbslen)
{
    unsigned char *sigctx = ctx;

    /* we must have a ctx and init was called upon it */
    if (!TEST_ptr(sigctx) || !TEST_int_eq(*sigctx, 1))
        return 0;

    *siglen = 256;
    /* record that the real sign operation was called */
    if (sig != NULL) {
        if (!TEST_int_ge(sigsize, *siglen))
            return 0;
        *sigctx = 2;
        /* produce a fake signature */
        memset(sig, 'a', *siglen);
    }

    return 1;
}

static const OSSL_DISPATCH fake_rsa_sig_funcs[] = {
    { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))fake_rsa_sig_newctx },
    { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))fake_rsa_sig_freectx },
    { OSSL_FUNC_SIGNATURE_SIGN_INIT, (void (*)(void))fake_rsa_sig_sign_init },
    { OSSL_FUNC_SIGNATURE_SIGN, (void (*)(void))fake_rsa_sig_sign },
    { 0, NULL }
};

static const OSSL_ALGORITHM fake_rsa_sig_algs[] = {
    { "RSA:rsaEncryption", "provider=fake-rsa", fake_rsa_sig_funcs, "Fake RSA Signature" },
    { NULL, NULL, NULL, NULL }
};

static const OSSL_ALGORITHM *fake_rsa_query(void *provctx,
                                            int operation_id,
                                            int *no_cache)
{
    *no_cache = 0;
    switch (operation_id) {
    case OSSL_OP_SIGNATURE:
        return fake_rsa_sig_algs;

    case OSSL_OP_KEYMGMT:
        return fake_rsa_keymgmt_algs;
    }
    return NULL;
}

/* Functions we provide to the core */
static const OSSL_DISPATCH fake_rsa_method[] = {
    { OSSL_FUNC_PROVIDER_TEARDOWN, (void (*)(void))OSSL_LIB_CTX_free },
    { OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))fake_rsa_query },
    { 0, NULL }
};

static int fake_rsa_provider_init(const OSSL_CORE_HANDLE *handle,
                                  const OSSL_DISPATCH *in,
                                  const OSSL_DISPATCH **out, void **provctx)
{
    if (!TEST_ptr(*provctx = OSSL_LIB_CTX_new()))
        return 0;
    *out = fake_rsa_method;
    return 1;
}

OSSL_PROVIDER *fake_rsa_start(OSSL_LIB_CTX *libctx)
{
    OSSL_PROVIDER *p;

    if (!TEST_true(OSSL_PROVIDER_add_builtin(libctx, "fake-rsa",
                                             fake_rsa_provider_init))
            || !TEST_ptr(p = OSSL_PROVIDER_try_load(libctx, "fake-rsa", 1)))
        return NULL;

    return p;
}

void fake_rsa_finish(OSSL_PROVIDER *p)
{
    OSSL_PROVIDER_unload(p);
}

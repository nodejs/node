/*
 * Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include <openssl/provider.h>
#include <openssl/decoder.h>
#include <openssl/encoder.h>
#include <openssl/store.h>
#include "testutil.h"

static int dummy_decoder_decode(void *ctx, OSSL_CORE_BIO *cin, int selection,
                                OSSL_CALLBACK *object_cb, void *object_cbarg,
                                OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    return 0;
}

static const OSSL_DISPATCH dummy_decoder_functions[] = {
    { OSSL_FUNC_DECODER_DECODE, (void (*)(void))dummy_decoder_decode },
    { 0, NULL }
};

static const OSSL_ALGORITHM dummy_decoders[] = {
    { "DUMMY", "provider=dummy,input=pem", dummy_decoder_functions },
    { NULL, NULL, NULL }
};

static int dummy_encoder_encode(void *ctx, OSSL_CORE_BIO *out,
                                const void *obj_raw,
                                const OSSL_PARAM obj_abstract[], int selection,
                                OSSL_PASSPHRASE_CALLBACK *cb, void *cbarg)
{
    return 0;
}

static const OSSL_DISPATCH dummy_encoder_functions[] = {
    { OSSL_FUNC_DECODER_DECODE, (void (*)(void))dummy_encoder_encode },
    { 0, NULL }
};

static const OSSL_ALGORITHM dummy_encoders[] = {
    { "DUMMY", "provider=dummy,output=pem", dummy_encoder_functions },
    { NULL, NULL, NULL }
};

static void *dummy_store_open(void *provctx, const char *uri)
{
    return NULL;
}

static int dummy_store_load(void *loaderctx,  OSSL_CALLBACK *object_cb,
                            void *object_cbarg, OSSL_PASSPHRASE_CALLBACK *pw_cb,
                            void *pw_cbarg)
{
    return 0;
}

static int dumm_store_eof(void *loaderctx)
{
    return 0;
}

static int dummy_store_close(void *loaderctx)
{
    return 0;
}

static const OSSL_DISPATCH dummy_store_functions[] = {
    { OSSL_FUNC_STORE_OPEN, (void (*)(void))dummy_store_open },
    { OSSL_FUNC_STORE_LOAD, (void (*)(void))dummy_store_load },
    { OSSL_FUNC_STORE_EOF, (void (*)(void))dumm_store_eof },
    { OSSL_FUNC_STORE_CLOSE, (void (*)(void))dummy_store_close },
    { 0, NULL }
};

static const OSSL_ALGORITHM dummy_store[] = {
    { "DUMMY", "provider=dummy", dummy_store_functions },
    { NULL, NULL, NULL }
};

static const OSSL_ALGORITHM *dummy_query(void *provctx, int operation_id,
                                         int *no_cache)
{
    *no_cache = 0;
    switch (operation_id) {
    case OSSL_OP_DECODER:
        return dummy_decoders;
    case OSSL_OP_ENCODER:
        return dummy_encoders;
    case OSSL_OP_STORE:
        return dummy_store;
    }
    return NULL;
}

static const OSSL_DISPATCH dummy_dispatch_table[] = {
    { OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))dummy_query },
    { OSSL_FUNC_PROVIDER_TEARDOWN, (void (*)(void))OSSL_LIB_CTX_free },
    { 0, NULL }
};

static int dummy_provider_init(const OSSL_CORE_HANDLE *handle,
                               const OSSL_DISPATCH *in,
                               const OSSL_DISPATCH **out,
                               void **provctx)
{
    OSSL_LIB_CTX *libctx = OSSL_LIB_CTX_new_child(handle, in);

    *provctx = (void *)libctx;
    *out = dummy_dispatch_table;

    return 1;
}

/*
 * Try fetching and freeing various things.
 * Test 0: Decoder
 * Test 1: Encoder
 * Test 2: Store loader
 */
static int fetch_test(int tst)
{
    OSSL_LIB_CTX *libctx = OSSL_LIB_CTX_new();
    OSSL_PROVIDER *dummyprov = NULL;
    OSSL_DECODER *decoder = NULL;
    OSSL_ENCODER *encoder = NULL;
    OSSL_STORE_LOADER *loader = NULL;
    int testresult = 0;

    if (!TEST_ptr(libctx))
        goto err;

    if (!TEST_true(OSSL_PROVIDER_add_builtin(libctx, "dummy-prov",
                                             dummy_provider_init))
            || !TEST_ptr(dummyprov = OSSL_PROVIDER_load(libctx, "dummy-prov")))
        goto err;

    switch(tst) {
    case 0:
        decoder = OSSL_DECODER_fetch(libctx, "DUMMY", NULL);
        if (!TEST_ptr(decoder))
            goto err;
        break;
    case 1:
        encoder = OSSL_ENCODER_fetch(libctx, "DUMMY", NULL);
        if (!TEST_ptr(encoder))
            goto err;
        break;
    case 2:
        loader = OSSL_STORE_LOADER_fetch(libctx, "DUMMY", NULL);
        if (!TEST_ptr(loader))
            goto err;
        break;
    default:
        goto err;
    }

    testresult = 1;
 err:
    OSSL_DECODER_free(decoder);
    OSSL_ENCODER_free(encoder);
    OSSL_STORE_LOADER_free(loader);
    OSSL_PROVIDER_unload(dummyprov);
    OSSL_LIB_CTX_free(libctx);
    return testresult;
}

int setup_tests(void)
{
    ADD_ALL_TESTS(fetch_test, 3);

    return 1;
}

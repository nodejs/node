/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * This is a decoder that's completely internal to the 'file:' store
 * implementation.  Only code in file_store.c know about this one.  Because
 * of this close relationship, we can cut certain corners, such as making
 * assumptions about the "provider context", which is currently simply the
 * provider context that the file_store.c code operates within.
 *
 * All this does is to read DER from the input if it can, and passes it on
 * to the data callback as an object abstraction, leaving it to the callback
 * to figure out what it actually is.
 *
 * This MUST be made the last decoder in a chain, leaving it to other more
 * specialized decoders to recognise and process their stuff first.
 */

#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/core_object.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/asn1err.h>
#include <openssl/params.h>
#include "internal/asn1.h"
#include "prov/bio.h"
#include "file_store_local.h"

/*
 * newctx and freectx are not strictly necessary.  However, the method creator,
 * ossl_decoder_from_algorithm(), demands that they exist, so we make sure to
 * oblige.
 */

static OSSL_FUNC_decoder_newctx_fn der2obj_newctx;
static OSSL_FUNC_decoder_freectx_fn der2obj_freectx;

static void *der2obj_newctx(void *provctx)
{
    return provctx;
}

static void der2obj_freectx(void *vctx)
{
}

static OSSL_FUNC_decoder_decode_fn der2obj_decode;

static int der2obj_decode(void *provctx, OSSL_CORE_BIO *cin, int selection,
                          OSSL_CALLBACK *data_cb, void *data_cbarg,
                          OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    /*
     * We're called from file_store.c, so we know that OSSL_CORE_BIO is a
     * BIO in this case.
     */
    BIO *in = ossl_bio_new_from_core_bio(provctx, cin);
    BUF_MEM *mem = NULL;
    int ok;

    if (in == NULL)
        return 0;

    ERR_set_mark();
    ok = (asn1_d2i_read_bio(in, &mem) >= 0);
    ERR_pop_to_mark();
    if (!ok && mem != NULL) {
        OPENSSL_free(mem->data);
        OPENSSL_free(mem);
        mem = NULL;
    }

    ok = 1;
    if (mem != NULL) {
        OSSL_PARAM params[3];
        int object_type = OSSL_OBJECT_UNKNOWN;

        params[0] =
            OSSL_PARAM_construct_int(OSSL_OBJECT_PARAM_TYPE, &object_type);
        params[1] =
            OSSL_PARAM_construct_octet_string(OSSL_OBJECT_PARAM_DATA,
                                              mem->data, mem->length);
        params[2] = OSSL_PARAM_construct_end();

        ok = data_cb(params, data_cbarg);
        OPENSSL_free(mem->data);
        OPENSSL_free(mem);
    }
    BIO_free(in);
    return ok;
}

static const OSSL_DISPATCH der_to_obj_decoder_functions[] = {
    { OSSL_FUNC_DECODER_NEWCTX, (void (*)(void))der2obj_newctx },
    { OSSL_FUNC_DECODER_FREECTX, (void (*)(void))der2obj_freectx },
    { OSSL_FUNC_DECODER_DECODE, (void (*)(void))der2obj_decode },
    { 0, NULL }
};

const OSSL_ALGORITHM ossl_der_to_obj_algorithm =
    { "obj", "input=DER", der_to_obj_decoder_functions };

/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core_object.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/params.h>
#include "prov/endecoder_local.h"
#include "crypto/lms.h"
#include "prov/bio.h"
#include "prov/implementations.h"

static OSSL_FUNC_decoder_newctx_fn lmsxdr2key_newctx;
static OSSL_FUNC_decoder_freectx_fn lmsxdr2key_freectx;
static OSSL_FUNC_decoder_decode_fn lmsxdr2key_decode;
static OSSL_FUNC_decoder_export_object_fn lmsxdr2key_export_object;

/* Context used for xdr to key decoding. */
struct lmsxdr2key_ctx_st {
    PROV_CTX *provctx;
    int selection; /* The selection that is passed to lmsxdr2key_decode() */
};

static void *lmsxdr2key_newctx(void *provctx)
{
    struct lmsxdr2key_ctx_st *ctx = OPENSSL_zalloc(sizeof(*ctx));

    if (ctx != NULL)
        ctx->provctx = provctx;
    return ctx;
}

static void lmsxdr2key_freectx(void *vctx)
{
    struct lmsxdr2key_ctx_st *ctx = vctx;

    OPENSSL_free(ctx);
}

static int lmsxdr2key_does_selection(void *provctx, int selection)
{
    if (selection == 0)
        return 1;

    if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0)
        return 1;

    return 0;
}

static int lmsxdr2key_decode(void *vctx, OSSL_CORE_BIO *cin, int selection,
                             OSSL_CALLBACK *data_cb, void *data_cbarg,
                             OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    struct lmsxdr2key_ctx_st *ctx = vctx;
    LMS_KEY *key = NULL;
    unsigned char buf[LMS_MAX_PUBKEY];
    size_t length;
    int ok = 0, inlen, readlen;
    BIO *in;

    in = ossl_bio_new_from_core_bio(ctx->provctx, cin);
    if (in == NULL)
        return 0;

    ctx->selection = selection;

    /* Read the header to determine the size */
    ERR_set_mark();
    readlen = BIO_read(in, buf, 4);
    ERR_pop_to_mark();
    if (readlen != 4)
        goto next;

    length = ossl_lms_pubkey_length(buf, 4);
    if (length <= 4)
        goto next;
    inlen = (int)length - 4;

    ERR_set_mark();
    readlen = BIO_read(in, buf + 4, inlen);
    ERR_pop_to_mark();
    if (readlen != inlen)
        goto next;

    if (selection == 0 || (selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
        key = ossl_lms_key_new(PROV_LIBCTX_OF(ctx->provctx));
        if (key == NULL || !ossl_lms_pubkey_decode(buf, length, key)) {
            ossl_lms_key_free(key);
            key = NULL;
        }
    }
 next:
    /*
     * Indicated that we successfully decoded something, or not at all.
     * Ending up "empty handed" is not an error.
     */
    ok = 1;

    /*
     * We free resources here so it's not held up during the callback, because
     * we know the process is recursive and the allocated chunks of memory
     * add up.
     */
    BIO_free(in);
    in = NULL;

    if (key != NULL) {
        OSSL_PARAM params[4];
        int object_type = OSSL_OBJECT_PKEY;

        params[0] =
            OSSL_PARAM_construct_int(OSSL_OBJECT_PARAM_TYPE, &object_type);
        params[1] =
            OSSL_PARAM_construct_utf8_string(OSSL_OBJECT_PARAM_DATA_TYPE,
                                             (char *)"lms", 0);
        /* The address of the key becomes the octet string */
        params[2] =
            OSSL_PARAM_construct_octet_string(OSSL_OBJECT_PARAM_REFERENCE,
                                              &key, sizeof(key));
        params[3] = OSSL_PARAM_construct_end();

        ok = data_cb(params, data_cbarg);
    }

    BIO_free(in);
    ossl_lms_key_free(key);
    return ok;
}

static int lmsxdr2key_export_object(void *vctx,
                                    const void *reference, size_t reference_sz,
                                    OSSL_CALLBACK *export_cb,
                                    void *export_cbarg)
{
    struct lmsxdr2key_ctx_st *ctx = vctx;
    OSSL_FUNC_keymgmt_export_fn *export =
        ossl_prov_get_keymgmt_export(ossl_lms_keymgmt_functions);
    void *keydata;

    if (reference_sz == sizeof(keydata) && export != NULL) {
        int selection = ctx->selection;

        if (selection == 0)
            selection = OSSL_KEYMGMT_SELECT_PUBLIC_KEY;
        /* The contents of the reference is the address to our object */
        keydata = *(void **)reference;

        return export(keydata, selection, export_cb, export_cbarg);
    }
    return 0;
}

const OSSL_DISPATCH ossl_xdr_to_lms_decoder_functions[] = {
    { OSSL_FUNC_DECODER_NEWCTX, (void (*)(void))lmsxdr2key_newctx },
    { OSSL_FUNC_DECODER_FREECTX, (void (*)(void))lmsxdr2key_freectx },
    { OSSL_FUNC_DECODER_DOES_SELECTION,
      (void (*)(void))lmsxdr2key_does_selection },
    { OSSL_FUNC_DECODER_DECODE, (void (*)(void))lmsxdr2key_decode },
    { OSSL_FUNC_DECODER_EXPORT_OBJECT,
      (void (*)(void))lmsxdr2key_export_object },
    OSSL_DISPATCH_END
};

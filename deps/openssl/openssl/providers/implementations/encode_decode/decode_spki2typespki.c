/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/asn1t.h>
#include <openssl/core_names.h>
#include <openssl/core_object.h>
#include <openssl/params.h>
#include <openssl/x509.h>
#include "internal/sizes.h"
#include "crypto/x509.h"
#include "crypto/ec.h"
#include "prov/bio.h"
#include "prov/implementations.h"
#include "endecoder_local.h"

static OSSL_FUNC_decoder_newctx_fn spki2typespki_newctx;
static OSSL_FUNC_decoder_freectx_fn spki2typespki_freectx;
static OSSL_FUNC_decoder_decode_fn spki2typespki_decode;

/*
 * Context used for SubjectPublicKeyInfo to Type specific SubjectPublicKeyInfo
 * decoding.
 */
struct spki2typespki_ctx_st {
    PROV_CTX *provctx;
};

static void *spki2typespki_newctx(void *provctx)
{
    struct spki2typespki_ctx_st *ctx = OPENSSL_zalloc(sizeof(*ctx));

    if (ctx != NULL)
        ctx->provctx = provctx;
    return ctx;
}

static void spki2typespki_freectx(void *vctx)
{
    struct spki2typespki_ctx_st *ctx = vctx;

    OPENSSL_free(ctx);
}

static int spki2typespki_decode(void *vctx, OSSL_CORE_BIO *cin, int selection,
                                OSSL_CALLBACK *data_cb, void *data_cbarg,
                                OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    struct spki2typespki_ctx_st *ctx = vctx;
    unsigned char *der, *derp;
    long len;
    int ok = 0;
    int objtype = OSSL_OBJECT_PKEY;
    X509_PUBKEY *xpub = NULL;
    X509_ALGOR *algor = NULL;
    const ASN1_OBJECT *oid = NULL;
    char dataname[OSSL_MAX_NAME_SIZE];
    OSSL_PARAM params[5], *p = params;

    if (!ossl_read_der(ctx->provctx, cin, &der, &len))
        return 1;
    derp = der;
    xpub = ossl_d2i_X509_PUBKEY_INTERNAL((const unsigned char **)&derp, len,
                                         PROV_LIBCTX_OF(ctx->provctx));


    if (xpub == NULL) {
        /* We return "empty handed".  This is not an error. */
        ok = 1;
        goto end;
    }

    if (!X509_PUBKEY_get0_param(NULL, NULL, NULL, &algor, xpub))
        goto end;
    X509_ALGOR_get0(&oid, NULL, NULL, algor);

#ifndef OPENSSL_NO_EC
    /* SM2 abuses the EC oid, so this could actually be SM2 */
    if (OBJ_obj2nid(oid) == NID_X9_62_id_ecPublicKey
            && ossl_x509_algor_is_sm2(algor))
        strcpy(dataname, "SM2");
    else
#endif
    if (OBJ_obj2txt(dataname, sizeof(dataname), oid, 0) <= 0)
        goto end;

    ossl_X509_PUBKEY_INTERNAL_free(xpub);
    xpub = NULL;

    *p++ =
        OSSL_PARAM_construct_utf8_string(OSSL_OBJECT_PARAM_DATA_TYPE,
                                            dataname, 0);

    *p++ =
        OSSL_PARAM_construct_utf8_string(OSSL_OBJECT_PARAM_DATA_STRUCTURE,
                                            "SubjectPublicKeyInfo",
                                            0);
    *p++ =
        OSSL_PARAM_construct_octet_string(OSSL_OBJECT_PARAM_DATA, der, len);
    *p++ =
        OSSL_PARAM_construct_int(OSSL_OBJECT_PARAM_TYPE, &objtype);

    *p = OSSL_PARAM_construct_end();

    ok = data_cb(params, data_cbarg);

 end:
    ossl_X509_PUBKEY_INTERNAL_free(xpub);
    OPENSSL_free(der);
    return ok;
}

const OSSL_DISPATCH ossl_SubjectPublicKeyInfo_der_to_der_decoder_functions[] = {
    { OSSL_FUNC_DECODER_NEWCTX, (void (*)(void))spki2typespki_newctx },
    { OSSL_FUNC_DECODER_FREECTX, (void (*)(void))spki2typespki_freectx },
    { OSSL_FUNC_DECODER_DECODE, (void (*)(void))spki2typespki_decode },
    { 0, NULL }
};

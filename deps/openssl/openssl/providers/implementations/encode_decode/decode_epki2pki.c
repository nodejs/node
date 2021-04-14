/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/core_object.h>
#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <openssl/proverr.h>
#include "internal/asn1.h"
#include "internal/sizes.h"
#include "prov/bio.h"
#include "prov/implementations.h"
#include "endecoder_local.h"

static OSSL_FUNC_decoder_newctx_fn epki2pki_newctx;
static OSSL_FUNC_decoder_freectx_fn epki2pki_freectx;
static OSSL_FUNC_decoder_decode_fn epki2pki_decode;

/*
 * Context used for EncryptedPrivateKeyInfo to PrivateKeyInfo decoding.
 */
struct epki2pki_ctx_st {
    PROV_CTX *provctx;
};

static void *epki2pki_newctx(void *provctx)
{
    struct epki2pki_ctx_st *ctx = OPENSSL_zalloc(sizeof(*ctx));

    if (ctx != NULL)
        ctx->provctx = provctx;
    return ctx;
}

static void epki2pki_freectx(void *vctx)
{
    struct epki2pki_ctx_st *ctx = vctx;

    OPENSSL_free(ctx);
}

/*
 * The selection parameter in epki2pki_decode() is not used by this function
 * because it's not relevant just to decode EncryptedPrivateKeyInfo to
 * PrivateKeyInfo.
 */
static int epki2pki_decode(void *vctx, OSSL_CORE_BIO *cin, int selection,
                           OSSL_CALLBACK *data_cb, void *data_cbarg,
                           OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    struct epki2pki_ctx_st *ctx = vctx;
    BUF_MEM *mem = NULL;
    unsigned char *der = NULL;
    const unsigned char *pder = NULL;
    long der_len = 0;
    X509_SIG *p8 = NULL;
    PKCS8_PRIV_KEY_INFO *p8inf = NULL;
    const X509_ALGOR *alg = NULL;
    BIO *in = ossl_bio_new_from_core_bio(ctx->provctx, cin);
    int ok = (asn1_d2i_read_bio(in, &mem) >= 0);

    BIO_free(in);

    /* We return "empty handed".  This is not an error. */
    if (!ok)
        return 1;

    pder = der = (unsigned char *)mem->data;
    der_len = (long)mem->length;
    OPENSSL_free(mem);

    ok = 1;                      /* Assume good */
    ERR_set_mark();
    if ((p8 = d2i_X509_SIG(NULL, &pder, der_len)) != NULL) {
        char pbuf[1024];
        size_t plen = 0;

        ERR_clear_last_mark();

        if (!pw_cb(pbuf, sizeof(pbuf), &plen, NULL, pw_cbarg)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_UNABLE_TO_GET_PASSPHRASE);
            ok = 0;
        } else {
            const ASN1_OCTET_STRING *oct;
            unsigned char *new_der = NULL;
            int new_der_len = 0;

            X509_SIG_get0(p8, &alg, &oct);
            if (!PKCS12_pbe_crypt_ex(alg, pbuf, plen,
                                     oct->data, oct->length,
                                     &new_der, &new_der_len, 0,
                                     PROV_LIBCTX_OF(ctx->provctx), NULL)) {
                ok = 0;
            } else {
                OPENSSL_free(der);
                der = new_der;
                der_len = new_der_len;
            }
            alg = NULL;
        }
        X509_SIG_free(p8);
    } else {
        ERR_pop_to_mark();
    }

    ERR_set_mark();
    pder = der;
    p8inf = d2i_PKCS8_PRIV_KEY_INFO(NULL, &pder, der_len);
    ERR_pop_to_mark();

    if (p8inf != NULL && PKCS8_pkey_get0(NULL, NULL, NULL, &alg, p8inf)) {
        /*
         * We have something and recognised it as PrivateKeyInfo, so let's
         * pass all the applicable data to the callback.
         */
        char keytype[OSSL_MAX_NAME_SIZE];
        OSSL_PARAM params[5], *p = params;
        int objtype = OSSL_OBJECT_PKEY;

        OBJ_obj2txt(keytype, sizeof(keytype), alg->algorithm, 0);

        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_OBJECT_PARAM_DATA_TYPE,
                                                keytype, 0);
        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_OBJECT_PARAM_DATA_STRUCTURE,
                                                "PrivateKeyInfo", 0);
        *p++ = OSSL_PARAM_construct_octet_string(OSSL_OBJECT_PARAM_DATA,
                                                 der, der_len);
        *p++ = OSSL_PARAM_construct_int(OSSL_OBJECT_PARAM_TYPE, &objtype);
        *p = OSSL_PARAM_construct_end();

        ok = data_cb(params, data_cbarg);
    }
    PKCS8_PRIV_KEY_INFO_free(p8inf);
    OPENSSL_free(der);
    return ok;
}

const OSSL_DISPATCH ossl_EncryptedPrivateKeyInfo_der_to_der_decoder_functions[] = {
    { OSSL_FUNC_DECODER_NEWCTX, (void (*)(void))epki2pki_newctx },
    { OSSL_FUNC_DECODER_FREECTX, (void (*)(void))epki2pki_freectx },
    { OSSL_FUNC_DECODER_DECODE, (void (*)(void))epki2pki_decode },
    { 0, NULL }
};

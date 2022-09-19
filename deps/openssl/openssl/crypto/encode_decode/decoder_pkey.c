/*
 * Copyright 2020-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core_names.h>
#include <openssl/core_object.h>
#include <openssl/provider.h>
#include <openssl/evp.h>
#include <openssl/ui.h>
#include <openssl/decoder.h>
#include <openssl/safestack.h>
#include <openssl/trace.h>
#include "crypto/evp.h"
#include "crypto/decoder.h"
#include "encoder_local.h"

int OSSL_DECODER_CTX_set_passphrase(OSSL_DECODER_CTX *ctx,
                                    const unsigned char *kstr,
                                    size_t klen)
{
    return ossl_pw_set_passphrase(&ctx->pwdata, kstr, klen);
}

int OSSL_DECODER_CTX_set_passphrase_ui(OSSL_DECODER_CTX *ctx,
                                       const UI_METHOD *ui_method,
                                       void *ui_data)
{
    return ossl_pw_set_ui_method(&ctx->pwdata, ui_method, ui_data);
}

int OSSL_DECODER_CTX_set_pem_password_cb(OSSL_DECODER_CTX *ctx,
                                         pem_password_cb *cb, void *cbarg)
{
    return ossl_pw_set_pem_password_cb(&ctx->pwdata, cb, cbarg);
}

int OSSL_DECODER_CTX_set_passphrase_cb(OSSL_DECODER_CTX *ctx,
                                       OSSL_PASSPHRASE_CALLBACK *cb,
                                       void *cbarg)
{
    return ossl_pw_set_ossl_passphrase_cb(&ctx->pwdata, cb, cbarg);
}

/*
 * Support for OSSL_DECODER_CTX_new_for_pkey:
 * The construct data, and collecting keymgmt information for it
 */

DEFINE_STACK_OF(EVP_KEYMGMT)

struct decoder_pkey_data_st {
    OSSL_LIB_CTX *libctx;
    char *propq;
    int selection;

    STACK_OF(EVP_KEYMGMT) *keymgmts;
    char *object_type;           /* recorded object data type, may be NULL */
    void **object;               /* Where the result should end up */
};

static int decoder_construct_pkey(OSSL_DECODER_INSTANCE *decoder_inst,
                                  const OSSL_PARAM *params,
                                  void *construct_data)
{
    struct decoder_pkey_data_st *data = construct_data;
    OSSL_DECODER *decoder = OSSL_DECODER_INSTANCE_get_decoder(decoder_inst);
    void *decoderctx = OSSL_DECODER_INSTANCE_get_decoder_ctx(decoder_inst);
    const OSSL_PROVIDER *decoder_prov = OSSL_DECODER_get0_provider(decoder);
    EVP_KEYMGMT *keymgmt = NULL;
    const OSSL_PROVIDER *keymgmt_prov = NULL;
    int i, end;
    /*
     * |object_ref| points to a provider reference to an object, its exact
     * contents entirely opaque to us, but may be passed to any provider
     * function that expects this (such as OSSL_FUNC_keymgmt_load().
     *
     * This pointer is considered volatile, i.e. whatever it points at
     * is assumed to be freed as soon as this function returns.
     */
    void *object_ref = NULL;
    size_t object_ref_sz = 0;
    const OSSL_PARAM *p;

    p = OSSL_PARAM_locate_const(params, OSSL_OBJECT_PARAM_DATA_TYPE);
    if (p != NULL) {
        char *object_type = NULL;

        if (!OSSL_PARAM_get_utf8_string(p, &object_type, 0))
            return 0;
        OPENSSL_free(data->object_type);
        data->object_type = object_type;
    }

    /*
     * For stuff that should end up in an EVP_PKEY, we only accept an object
     * reference for the moment.  This enforces that the key data itself
     * remains with the provider.
     */
    p = OSSL_PARAM_locate_const(params, OSSL_OBJECT_PARAM_REFERENCE);
    if (p == NULL || p->data_type != OSSL_PARAM_OCTET_STRING)
        return 0;
    object_ref = p->data;
    object_ref_sz = p->data_size;

    /*
     * First, we try to find a keymgmt that comes from the same provider as
     * the decoder that passed the params.
     */
    end = sk_EVP_KEYMGMT_num(data->keymgmts);
    for (i = 0; i < end; i++) {
        keymgmt = sk_EVP_KEYMGMT_value(data->keymgmts, i);
        keymgmt_prov = EVP_KEYMGMT_get0_provider(keymgmt);

        if (keymgmt_prov == decoder_prov
            && evp_keymgmt_has_load(keymgmt)
            && EVP_KEYMGMT_is_a(keymgmt, data->object_type))
            break;
    }
    if (i < end) {
        /* To allow it to be freed further down */
        if (!EVP_KEYMGMT_up_ref(keymgmt))
            return 0;
    } else if ((keymgmt = EVP_KEYMGMT_fetch(data->libctx,
                                            data->object_type,
                                            data->propq)) != NULL) {
        keymgmt_prov = EVP_KEYMGMT_get0_provider(keymgmt);
    }

    if (keymgmt != NULL) {
        EVP_PKEY *pkey = NULL;
        void *keydata = NULL;

        /*
         * If the EVP_KEYMGMT and the OSSL_DECODER are from the
         * same provider, we assume that the KEYMGMT has a key loading
         * function that can handle the provider reference we hold.
         *
         * Otherwise, we export from the decoder and import the
         * result in the keymgmt.
         */
        if (keymgmt_prov == decoder_prov) {
            keydata = evp_keymgmt_load(keymgmt, object_ref, object_ref_sz);
        } else {
            struct evp_keymgmt_util_try_import_data_st import_data;

            import_data.keymgmt = keymgmt;
            import_data.keydata = NULL;
            import_data.selection = data->selection;

            /*
             * No need to check for errors here, the value of
             * |import_data.keydata| is as much an indicator.
             */
            (void)decoder->export_object(decoderctx,
                                         object_ref, object_ref_sz,
                                         &evp_keymgmt_util_try_import,
                                         &import_data);
            keydata = import_data.keydata;
            import_data.keydata = NULL;
        }

        if (keydata != NULL
            && (pkey = evp_keymgmt_util_make_pkey(keymgmt, keydata)) == NULL)
            evp_keymgmt_freedata(keymgmt, keydata);

        *data->object = pkey;

        /*
         * evp_keymgmt_util_make_pkey() increments the reference count when
         * assigning the EVP_PKEY, so we can free the keymgmt here.
         */
        EVP_KEYMGMT_free(keymgmt);
    }
    /*
     * We successfully looked through, |*ctx->object| determines if we
     * actually found something.
     */
    return (*data->object != NULL);
}

static void decoder_clean_pkey_construct_arg(void *construct_data)
{
    struct decoder_pkey_data_st *data = construct_data;

    if (data != NULL) {
        sk_EVP_KEYMGMT_pop_free(data->keymgmts, EVP_KEYMGMT_free);
        OPENSSL_free(data->propq);
        OPENSSL_free(data->object_type);
        OPENSSL_free(data);
    }
}

static void collect_name(const char *name, void *arg)
{
    STACK_OF(OPENSSL_CSTRING) *names = arg;

    sk_OPENSSL_CSTRING_push(names, name);
}

static void collect_keymgmt(EVP_KEYMGMT *keymgmt, void *arg)
{
    STACK_OF(EVP_KEYMGMT) *keymgmts = arg;

    if (!EVP_KEYMGMT_up_ref(keymgmt) /* ref++ */)
        return;
    if (sk_EVP_KEYMGMT_push(keymgmts, keymgmt) <= 0) {
        EVP_KEYMGMT_free(keymgmt);   /* ref-- */
        return;
    }
}

struct collect_decoder_data_st {
    STACK_OF(OPENSSL_CSTRING) *names;
    OSSL_DECODER_CTX *ctx;

    int total;
    unsigned int error_occurred:1;
};

static void collect_decoder(OSSL_DECODER *decoder, void *arg)
{
    struct collect_decoder_data_st *data = arg;
    size_t i, end_i;
    const OSSL_PROVIDER *prov = OSSL_DECODER_get0_provider(decoder);
    void *provctx = OSSL_PROVIDER_get0_provider_ctx(prov);

    if (data->error_occurred)
        return;

    if (data->names == NULL) {
        data->error_occurred = 1;
        return;
    }

    /*
     * Either the caller didn't give a selection, or if they did,
     * the decoder must tell us if it supports that selection to
     * be accepted.  If the decoder doesn't have |does_selection|,
     * it's seen as taking anything.
     */
    if (decoder->does_selection != NULL
            && !decoder->does_selection(provctx, data->ctx->selection))
        return;

    OSSL_TRACE_BEGIN(DECODER) {
        BIO_printf(trc_out,
                   "(ctx %p) Checking out decoder %p:\n"
                   "    %s with %s\n",
                   (void *)data->ctx, (void *)decoder,
                   OSSL_DECODER_get0_name(decoder),
                   OSSL_DECODER_get0_properties(decoder));
    } OSSL_TRACE_END(DECODER);

    end_i = sk_OPENSSL_CSTRING_num(data->names);
    for (i = 0; i < end_i; i++) {
        const char *name = sk_OPENSSL_CSTRING_value(data->names, i);

        if (OSSL_DECODER_is_a(decoder, name)) {
            void *decoderctx = NULL;
            OSSL_DECODER_INSTANCE *di = NULL;

            if ((decoderctx = decoder->newctx(provctx)) == NULL) {
                data->error_occurred = 1;
                return;
            }
            if ((di = ossl_decoder_instance_new(decoder, decoderctx)) == NULL) {
                decoder->freectx(decoderctx);
                data->error_occurred = 1;
                return;
            }

            OSSL_TRACE_BEGIN(DECODER) {
                BIO_printf(trc_out,
                           "(ctx %p) Checking out decoder %p:\n"
                           "    %s with %s\n",
                           (void *)data->ctx, (void *)decoder,
                           OSSL_DECODER_get0_name(decoder),
                           OSSL_DECODER_get0_properties(decoder));
            } OSSL_TRACE_END(DECODER);

            if (!ossl_decoder_ctx_add_decoder_inst(data->ctx, di)) {
                ossl_decoder_instance_free(di);
                data->error_occurred = 1;
                return;
            }
            data->total++;

            /* Success */
            return;
        }
    }

    /* Decoder not suitable - but not a fatal error */
    data->error_occurred = 0;
}

int ossl_decoder_ctx_setup_for_pkey(OSSL_DECODER_CTX *ctx,
                                    EVP_PKEY **pkey, const char *keytype,
                                    OSSL_LIB_CTX *libctx,
                                    const char *propquery)
{
    struct decoder_pkey_data_st *process_data = NULL;
    STACK_OF(OPENSSL_CSTRING) *names = NULL;
    const char *input_type = ctx->start_input_type;
    const char *input_structure = ctx->input_structure;
    int ok = 0;
    int isecoid = 0;
    int i, end;

    if (keytype != NULL
            && (strcmp(keytype, "id-ecPublicKey") == 0
                || strcmp(keytype, "1.2.840.10045.2.1") == 0))
        isecoid = 1;

    OSSL_TRACE_BEGIN(DECODER) {
        BIO_printf(trc_out,
                   "(ctx %p) Looking for decoders producing %s%s%s%s%s%s\n",
                   (void *)ctx,
                   keytype != NULL ? keytype : "",
                   keytype != NULL ? " keys" : "keys of any type",
                   input_type != NULL ? " from " : "",
                   input_type != NULL ? input_type : "",
                   input_structure != NULL ? " with " : "",
                   input_structure != NULL ? input_structure : "");
    } OSSL_TRACE_END(DECODER);

    if ((process_data = OPENSSL_zalloc(sizeof(*process_data))) == NULL
        || (propquery != NULL
            && (process_data->propq = OPENSSL_strdup(propquery)) == NULL)
        || (process_data->keymgmts = sk_EVP_KEYMGMT_new_null()) == NULL
        || (names = sk_OPENSSL_CSTRING_new_null()) == NULL) {
        ERR_raise(ERR_LIB_OSSL_DECODER, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    process_data->object = (void **)pkey;
    process_data->libctx = libctx;
    process_data->selection = ctx->selection;

    /* First, find all keymgmts to form goals */
    EVP_KEYMGMT_do_all_provided(libctx, collect_keymgmt,
                                process_data->keymgmts);

    /* Then, we collect all the keymgmt names */
    end = sk_EVP_KEYMGMT_num(process_data->keymgmts);
    for (i = 0; i < end; i++) {
        EVP_KEYMGMT *keymgmt = sk_EVP_KEYMGMT_value(process_data->keymgmts, i);

        /*
         * If the key type is given by the caller, we only use the matching
         * KEYMGMTs, otherwise we use them all.
         * We have to special case SM2 here because of its abuse of the EC OID.
         * The EC OID can be used to identify an EC key or an SM2 key - so if
         * we have seen that OID we try both key types
         */
        if (keytype == NULL
                || EVP_KEYMGMT_is_a(keymgmt, keytype)
                || (isecoid && EVP_KEYMGMT_is_a(keymgmt, "SM2"))) {
            if (!EVP_KEYMGMT_names_do_all(keymgmt, collect_name, names)) {
                ERR_raise(ERR_LIB_OSSL_DECODER, ERR_R_INTERNAL_ERROR);
                goto err;
            }
        }
    }

    OSSL_TRACE_BEGIN(DECODER) {
        end = sk_OPENSSL_CSTRING_num(names);
        BIO_printf(trc_out,
                   "    Found %d keytypes (possibly with duplicates)",
                   end);
        for (i = 0; i < end; i++)
            BIO_printf(trc_out, "%s%s",
                       i == 0 ? ": " : ", ",
                       sk_OPENSSL_CSTRING_value(names, i));
        BIO_printf(trc_out, "\n");
    } OSSL_TRACE_END(DECODER);

    /*
     * Finally, find all decoders that have any keymgmt of the collected
     * keymgmt names
     */
    {
        struct collect_decoder_data_st collect_decoder_data = { NULL, };

        collect_decoder_data.names = names;
        collect_decoder_data.ctx = ctx;
        OSSL_DECODER_do_all_provided(libctx,
                                     collect_decoder, &collect_decoder_data);
        sk_OPENSSL_CSTRING_free(names);
        names = NULL;

        if (collect_decoder_data.error_occurred)
            goto err;

        OSSL_TRACE_BEGIN(DECODER) {
            BIO_printf(trc_out,
                       "(ctx %p) Got %d decoders producing keys\n",
                       (void *)ctx, collect_decoder_data.total);
        } OSSL_TRACE_END(DECODER);
    }

    if (OSSL_DECODER_CTX_get_num_decoders(ctx) != 0) {
        if (!OSSL_DECODER_CTX_set_construct(ctx, decoder_construct_pkey)
            || !OSSL_DECODER_CTX_set_construct_data(ctx, process_data)
            || !OSSL_DECODER_CTX_set_cleanup(ctx,
                                             decoder_clean_pkey_construct_arg))
            goto err;

        process_data = NULL; /* Avoid it being freed */
    }

    ok = 1;
 err:
    decoder_clean_pkey_construct_arg(process_data);
    sk_OPENSSL_CSTRING_free(names);

    return ok;
}

OSSL_DECODER_CTX *
OSSL_DECODER_CTX_new_for_pkey(EVP_PKEY **pkey,
                              const char *input_type,
                              const char *input_structure,
                              const char *keytype, int selection,
                              OSSL_LIB_CTX *libctx, const char *propquery)
{
    OSSL_DECODER_CTX *ctx = NULL;

    if ((ctx = OSSL_DECODER_CTX_new()) == NULL) {
        ERR_raise(ERR_LIB_OSSL_DECODER, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    OSSL_TRACE_BEGIN(DECODER) {
        BIO_printf(trc_out,
                   "(ctx %p) Looking for %s decoders with selection %d\n",
                   (void *)ctx, keytype, selection);
        BIO_printf(trc_out, "    input type: %s, input structure: %s\n",
                   input_type, input_structure);
    } OSSL_TRACE_END(DECODER);

    if (OSSL_DECODER_CTX_set_input_type(ctx, input_type)
        && OSSL_DECODER_CTX_set_input_structure(ctx, input_structure)
        && OSSL_DECODER_CTX_set_selection(ctx, selection)
        && ossl_decoder_ctx_setup_for_pkey(ctx, pkey, keytype,
                                           libctx, propquery)
        && OSSL_DECODER_CTX_add_extra(ctx, libctx, propquery)) {
        OSSL_TRACE_BEGIN(DECODER) {
            BIO_printf(trc_out, "(ctx %p) Got %d decoders\n",
                       (void *)ctx, OSSL_DECODER_CTX_get_num_decoders(ctx));
        } OSSL_TRACE_END(DECODER);
        return ctx;
    }

    OSSL_DECODER_CTX_free(ctx);
    return NULL;
}

/*
 * Copyright 2021-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */

#include <string.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/core_names.h>
#include <openssl/core_object.h>
#include <openssl/rand.h>
#include <openssl/provider.h>
#include "testutil.h"
#include "fake_rsaprov.h"
#include "internal/asn1.h"

static OSSL_FUNC_keymgmt_new_fn fake_rsa_keymgmt_new;
static OSSL_FUNC_keymgmt_free_fn fake_rsa_keymgmt_free;
static OSSL_FUNC_keymgmt_has_fn fake_rsa_keymgmt_has;
static OSSL_FUNC_keymgmt_query_operation_name_fn fake_rsa_keymgmt_query;
static OSSL_FUNC_keymgmt_import_fn fake_rsa_keymgmt_import;
static OSSL_FUNC_keymgmt_import_types_fn fake_rsa_keymgmt_imptypes;
static OSSL_FUNC_keymgmt_export_fn fake_rsa_keymgmt_export;
static OSSL_FUNC_keymgmt_export_types_fn fake_rsa_keymgmt_exptypes;
static OSSL_FUNC_keymgmt_load_fn fake_rsa_keymgmt_load;

static int has_selection;
static int imptypes_selection;
static int exptypes_selection;
static int query_id;
static int key_deleted;

unsigned fake_rsa_query_operation_name = 0;

typedef struct {
    OSSL_LIB_CTX *libctx;
} PROV_FAKE_RSA_CTX;

#define PROV_FAKE_RSA_LIBCTX_OF(provctx) (((PROV_FAKE_RSA_CTX *)provctx)->libctx)

#define FAKE_RSA_STATUS_IMPORTED    1
#define FAKE_RSA_STATUS_GENERATED   2
#define FAKE_RSA_STATUS_DECODED     3

struct fake_rsa_keydata {
    int selection;
    int status;
};

void fake_rsa_restore_store_state(void)
{
    key_deleted = 0;
}

static void *fake_rsa_keymgmt_new(void *provctx)
{
    struct fake_rsa_keydata *key;

    if (!TEST_ptr(key = OPENSSL_zalloc(sizeof(struct fake_rsa_keydata))))
        return NULL;

    /* clear test globals */
    has_selection = 0;
    imptypes_selection = 0;
    exptypes_selection = 0;
    query_id = 0;

    return key;
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

    return fake_rsa_query_operation_name ? NULL: "RSA";
}

static int fake_rsa_keymgmt_import(void *keydata, int selection,
                                   const OSSL_PARAM *p)
{
    struct fake_rsa_keydata *fake_rsa_key = keydata;

    /* key was imported */
    fake_rsa_key->status = FAKE_RSA_STATUS_IMPORTED;

    return 1;
}

static unsigned char fake_rsa_n[] =
   "\x00\xAA\x36\xAB\xCE\x88\xAC\xFD\xFF\x55\x52\x3C\x7F\xC4\x52\x3F"
   "\x90\xEF\xA0\x0D\xF3\x77\x4A\x25\x9F\x2E\x62\xB4\xC5\xD9\x9C\xB5"
   "\xAD\xB3\x00\xA0\x28\x5E\x53\x01\x93\x0E\x0C\x70\xFB\x68\x76\x93"
   "\x9C\xE6\x16\xCE\x62\x4A\x11\xE0\x08\x6D\x34\x1E\xBC\xAC\xA0\xA1"
   "\xF5";

static unsigned char fake_rsa_e[] = "\x11";

static unsigned char fake_rsa_d[] =
    "\x0A\x03\x37\x48\x62\x64\x87\x69\x5F\x5F\x30\xBC\x38\xB9\x8B\x44"
    "\xC2\xCD\x2D\xFF\x43\x40\x98\xCD\x20\xD8\xA1\x38\xD0\x90\xBF\x64"
    "\x79\x7C\x3F\xA7\xA2\xCD\xCB\x3C\xD1\xE0\xBD\xBA\x26\x54\xB4\xF9"
    "\xDF\x8E\x8A\xE5\x9D\x73\x3D\x9F\x33\xB3\x01\x62\x4A\xFD\x1D\x51";

static unsigned char fake_rsa_p[] =
    "\x00\xD8\x40\xB4\x16\x66\xB4\x2E\x92\xEA\x0D\xA3\xB4\x32\x04\xB5"
    "\xCF\xCE\x33\x52\x52\x4D\x04\x16\xA5\xA4\x41\xE7\x00\xAF\x46\x12"
    "\x0D";

static unsigned char fake_rsa_q[] =
    "\x00\xC9\x7F\xB1\xF0\x27\xF4\x53\xF6\x34\x12\x33\xEA\xAA\xD1\xD9"
    "\x35\x3F\x6C\x42\xD0\x88\x66\xB1\xD0\x5A\x0F\x20\x35\x02\x8B\x9D"
    "\x89";

static unsigned char fake_rsa_dmp1[] =
    "\x59\x0B\x95\x72\xA2\xC2\xA9\xC4\x06\x05\x9D\xC2\xAB\x2F\x1D\xAF"
    "\xEB\x7E\x8B\x4F\x10\xA7\x54\x9E\x8E\xED\xF5\xB4\xFC\xE0\x9E\x05";

static unsigned char fake_rsa_dmq1[] =
    "\x00\x8E\x3C\x05\x21\xFE\x15\xE0\xEA\x06\xA3\x6F\xF0\xF1\x0C\x99"
    "\x52\xC3\x5B\x7A\x75\x14\xFD\x32\x38\xB8\x0A\xAD\x52\x98\x62\x8D"
    "\x51";

static unsigned char fake_rsa_iqmp[] =
    "\x36\x3F\xF7\x18\x9D\xA8\xE9\x0B\x1D\x34\x1F\x71\xD0\x9B\x76\xA8"
    "\xA9\x43\xE1\x1D\x10\xB2\x4D\x24\x9F\x2D\xEA\xFE\xF8\x0C\x18\x26";

OSSL_PARAM *fake_rsa_key_params(int priv)
{
    if (priv) {
        OSSL_PARAM params[] = {
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_N, fake_rsa_n,
                          sizeof(fake_rsa_n) -1),
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_E, fake_rsa_e,
                          sizeof(fake_rsa_e) -1),
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_D, fake_rsa_d,
                          sizeof(fake_rsa_d) -1),
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_FACTOR1, fake_rsa_p,
                          sizeof(fake_rsa_p) -1),
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_FACTOR2, fake_rsa_q,
                          sizeof(fake_rsa_q) -1),
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_EXPONENT1, fake_rsa_dmp1,
                          sizeof(fake_rsa_dmp1) -1),
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_EXPONENT2, fake_rsa_dmq1,
                          sizeof(fake_rsa_dmq1) -1),
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_COEFFICIENT1, fake_rsa_iqmp,
                          sizeof(fake_rsa_iqmp) -1),
            OSSL_PARAM_END
        };
        return OSSL_PARAM_dup(params);
    } else {
        OSSL_PARAM params[] = {
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_N, fake_rsa_n,
                          sizeof(fake_rsa_n) -1),
            OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_E, fake_rsa_e,
                          sizeof(fake_rsa_e) -1),
            OSSL_PARAM_END
        };
        return OSSL_PARAM_dup(params);
    }
}

static int fake_rsa_keymgmt_export(void *keydata, int selection,
                                   OSSL_CALLBACK *param_callback, void *cbarg)
{
    OSSL_PARAM *params = NULL;
    int ret;

    if (selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY)
        return 0;

    if (!TEST_ptr(params = fake_rsa_key_params(0)))
        return 0;

    ret = param_callback(params, cbarg);
    OSSL_PARAM_free(params);
    return ret;
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

static const OSSL_PARAM fake_rsa_export_key_types[] = {
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_N, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_E, NULL, 0),
    OSSL_PARAM_END
};

static const OSSL_PARAM *fake_rsa_keymgmt_exptypes(int selection)
{
    /* record global for checking */
    exptypes_selection = selection;

    return fake_rsa_export_key_types;
}

static void *fake_rsa_keymgmt_load(const void *reference, size_t reference_sz)
{
    struct fake_rsa_keydata *key = NULL;

    if (reference_sz != sizeof(key))
        return NULL;

    key = *(struct fake_rsa_keydata **)reference;
    if (key->status != FAKE_RSA_STATUS_IMPORTED && key->status != FAKE_RSA_STATUS_DECODED)
        return NULL;

    /* detach the reference */
    *(struct fake_rsa_keydata  **)reference = NULL;

    return key;
}

static void *fake_rsa_gen_init(void *provctx, int selection,
                               const OSSL_PARAM params[])
{
    unsigned char *gctx = NULL;

    if (!TEST_ptr(gctx = OPENSSL_malloc(1)))
        return NULL;

    *gctx = 1;

    return gctx;
}

static void *fake_rsa_gen(void *genctx, OSSL_CALLBACK *osslcb, void *cbarg)
{
    unsigned char *gctx = genctx;
    static const unsigned char inited[] = { 1 };
    struct fake_rsa_keydata *keydata;

    if (!TEST_ptr(gctx)
        || !TEST_mem_eq(gctx, sizeof(*gctx), inited, sizeof(inited)))
        return NULL;

    if (!TEST_ptr(keydata = fake_rsa_keymgmt_new(NULL)))
        return NULL;

    keydata->status = FAKE_RSA_STATUS_GENERATED;
    return keydata;
}

static void fake_rsa_gen_cleanup(void *genctx)
{
   OPENSSL_free(genctx);
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
    { OSSL_FUNC_KEYMGMT_EXPORT, (void (*)(void))fake_rsa_keymgmt_export },
    { OSSL_FUNC_KEYMGMT_EXPORT_TYPES,
        (void (*)(void))fake_rsa_keymgmt_exptypes },
    { OSSL_FUNC_KEYMGMT_LOAD, (void (*)(void))fake_rsa_keymgmt_load },
    { OSSL_FUNC_KEYMGMT_GEN_INIT, (void (*)(void))fake_rsa_gen_init },
    { OSSL_FUNC_KEYMGMT_GEN, (void (*)(void))fake_rsa_gen },
    { OSSL_FUNC_KEYMGMT_GEN_CLEANUP, (void (*)(void))fake_rsa_gen_cleanup },
    OSSL_DISPATCH_END
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
    struct fake_rsa_keydata *keydata = provkey;

    /* we must have a ctx */
    if (!TEST_ptr(sigctx))
        return 0;

    /* we must have some initialized key */
    if (!TEST_ptr(keydata) || !TEST_int_gt(keydata->status, 0))
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
        if (!TEST_size_t_ge(sigsize, *siglen))
            return 0;
        *sigctx = 2;
        /* produce a fake signature */
        memset(sig, 'a', *siglen);
    }

    return 1;
}

#define FAKE_DGSTSGN_SIGN 0x01
#define FAKE_DGSTSGN_VERIFY 0x02
#define FAKE_DGSTSGN_UPDATED 0x04
#define FAKE_DGSTSGN_FINALISED 0x08
#define FAKE_DGSTSGN_NO_DUP 0xA0

static void *fake_rsa_sig_dupctx(void *ctx)
{
    unsigned char *sigctx = ctx;
    unsigned char *newctx;

    if ((*sigctx & FAKE_DGSTSGN_NO_DUP) != 0)
        return NULL;

    if (!TEST_ptr(newctx = OPENSSL_zalloc(1)))
        return NULL;

    *newctx = *sigctx;
    return newctx;
}

static int fake_rsa_dgstsgnvfy_init(void *ctx, unsigned char type,
                                    void *provkey, const OSSL_PARAM params[])
{
    unsigned char *sigctx = ctx;
    struct fake_rsa_keydata *keydata = provkey;

    /* we must have a ctx */
    if (!TEST_ptr(sigctx))
        return 0;

    /* we must have some initialized key */
    if (!TEST_ptr(keydata) || !TEST_int_gt(keydata->status, 0))
        return 0;

    /* record that sign/verify init was called */
    *sigctx = type;

    if (params) {
        const OSSL_PARAM *p;
        int dup;
        p = OSSL_PARAM_locate_const(params, "NO_DUP");
        if (p != NULL) {
            if (OSSL_PARAM_get_int(p, &dup)) {
                *sigctx |= FAKE_DGSTSGN_NO_DUP;
            }
        }
    }

    return 1;
}

static int fake_rsa_dgstsgn_init(void *ctx, const char *mdname,
                                 void *provkey, const OSSL_PARAM params[])
{
    return fake_rsa_dgstsgnvfy_init(ctx, FAKE_DGSTSGN_SIGN, provkey, params);
}

static int fake_rsa_dgstvfy_init(void *ctx, const char *mdname,
                                 void *provkey, const OSSL_PARAM params[])
{
    return fake_rsa_dgstsgnvfy_init(ctx, FAKE_DGSTSGN_VERIFY, provkey, params);
}

static int fake_rsa_dgstsgnvfy_update(void *ctx, const unsigned char *data,
                                      size_t datalen)
{
    unsigned char *sigctx = ctx;

    /* we must have a ctx */
    if (!TEST_ptr(sigctx))
        return 0;

    if (*sigctx == 0 || (*sigctx & FAKE_DGSTSGN_FINALISED) != 0)
        return 0;

    *sigctx |= FAKE_DGSTSGN_UPDATED;
    return 1;
}

static int fake_rsa_dgstsgnvfy_final(void *ctx, unsigned char *sig,
                                     size_t *siglen, size_t sigsize)
{
    unsigned char *sigctx = ctx;

    /* we must have a ctx */
    if (!TEST_ptr(sigctx))
        return 0;

    if (*sigctx == 0 || (*sigctx & FAKE_DGSTSGN_FINALISED) != 0)
        return 0;

    if ((*sigctx & FAKE_DGSTSGN_SIGN) != 0 && (siglen == NULL))
        return 0;

    if ((*sigctx & FAKE_DGSTSGN_VERIFY) != 0 && (siglen != NULL))
        return 0;

    /* this is sign op */
    if (siglen) {
        *siglen = 256;
        /* record that the real sign operation was called */
        if (sig != NULL) {
            if (!TEST_size_t_ge(sigsize, *siglen))
                return 0;
            /* produce a fake signature */
            memset(sig, 'a', *siglen);
        }
    }

    /* simulate inability to duplicate context and finalise it */
    if ((*sigctx & FAKE_DGSTSGN_NO_DUP) != 0) {
        *sigctx |= FAKE_DGSTSGN_FINALISED;
    }
    return 1;
}

static int fake_rsa_dgstvfy_final(void *ctx, unsigned char *sig,
                                  size_t siglen)
{
    return fake_rsa_dgstsgnvfy_final(ctx, sig, NULL, siglen);
}

static int fake_rsa_dgstsgn(void *ctx, unsigned char *sig, size_t *siglen,
                            size_t sigsize, const unsigned char *tbs,
                            size_t tbslen)
{
    if (!fake_rsa_dgstsgnvfy_update(ctx, tbs, tbslen))
        return 0;

    return fake_rsa_dgstsgnvfy_final(ctx, sig, siglen, sigsize);
}

static int fake_rsa_dgstvfy(void *ctx, unsigned char *sig, size_t siglen,
                            const unsigned char *tbv, size_t tbvlen)
{
    if (!fake_rsa_dgstsgnvfy_update(ctx, tbv, tbvlen))
        return 0;

    return fake_rsa_dgstvfy_final(ctx, sig, siglen);
}

static const OSSL_DISPATCH fake_rsa_sig_funcs[] = {
    { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))fake_rsa_sig_newctx },
    { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))fake_rsa_sig_freectx },
    { OSSL_FUNC_SIGNATURE_SIGN_INIT, (void (*)(void))fake_rsa_sig_sign_init },
    { OSSL_FUNC_SIGNATURE_SIGN, (void (*)(void))fake_rsa_sig_sign },
    { OSSL_FUNC_SIGNATURE_DUPCTX, (void (*)(void))fake_rsa_sig_dupctx },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN_INIT,
        (void (*)(void))fake_rsa_dgstsgn_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN_UPDATE,
        (void (*)(void))fake_rsa_dgstsgnvfy_update },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN_FINAL,
        (void (*)(void))fake_rsa_dgstsgnvfy_final },
    { OSSL_FUNC_SIGNATURE_DIGEST_SIGN,
        (void (*)(void))fake_rsa_dgstsgn },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_INIT,
        (void (*)(void))fake_rsa_dgstvfy_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_UPDATE,
        (void (*)(void))fake_rsa_dgstsgnvfy_update },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_FINAL,
        (void (*)(void))fake_rsa_dgstvfy_final },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY,
        (void (*)(void))fake_rsa_dgstvfy },
    OSSL_DISPATCH_END
};

static const OSSL_ALGORITHM fake_rsa_sig_algs[] = {
    { "RSA:rsaEncryption", "provider=fake-rsa", fake_rsa_sig_funcs, "Fake RSA Signature" },
    { NULL, NULL, NULL, NULL }
};

static OSSL_FUNC_store_open_fn fake_rsa_st_open;
static OSSL_FUNC_store_open_ex_fn fake_rsa_st_open_ex;
static OSSL_FUNC_store_settable_ctx_params_fn fake_rsa_st_settable_ctx_params;
static OSSL_FUNC_store_set_ctx_params_fn fake_rsa_st_set_ctx_params;
static OSSL_FUNC_store_load_fn fake_rsa_st_load;
static OSSL_FUNC_store_eof_fn fake_rsa_st_eof;
static OSSL_FUNC_store_close_fn fake_rsa_st_close;
static OSSL_FUNC_store_delete_fn fake_rsa_st_delete;

static const char fake_rsa_scheme[] = "fake_rsa:";
static const char fake_rsa_openpwtest[] = "fake_rsa:openpwtest";
static const char fake_rsa_prompt[] = "Fake Prompt Info";

static void *fake_rsa_st_open_ex(void *provctx, const char *uri,
                                 const OSSL_PARAM params[],
                                 OSSL_PASSPHRASE_CALLBACK *pw_cb,
                                 void *pw_cbarg)
{
    unsigned char *storectx = NULL;

    /* First check whether the uri is ours */
    if (strncmp(uri, fake_rsa_scheme, sizeof(fake_rsa_scheme) - 1) != 0)
        return NULL;

    if (strncmp(uri, fake_rsa_openpwtest,
                sizeof(fake_rsa_openpwtest) - 1) == 0) {
        const char *pw_check = FAKE_PASSPHRASE;
        char fakepw[sizeof(FAKE_PASSPHRASE) + 1] = { 0 };
        size_t fakepw_len = 0;
        OSSL_PARAM pw_params[2] = {
            OSSL_PARAM_utf8_string(OSSL_PASSPHRASE_PARAM_INFO,
                                   (void *)fake_rsa_prompt,
                                   sizeof(fake_rsa_prompt) - 1),
            OSSL_PARAM_END,
        };

        if (pw_cb == NULL) {
            return NULL;
        }

        if (!pw_cb(fakepw, sizeof(fakepw), &fakepw_len, pw_params, pw_cbarg)) {
            TEST_info("fake_rsa_open_ex failed passphrase callback");
            return NULL;
        }
        if (strncmp(pw_check, fakepw, sizeof(pw_check) - 1) != 0) {
            TEST_info("fake_rsa_open_ex failed passphrase check");
            return NULL;
        }
    }

    storectx = OPENSSL_zalloc(1);
    if (!TEST_ptr(storectx))
        return NULL;

    TEST_info("fake_rsa_open_ex called");

    return storectx;
}

static void *fake_rsa_st_open(void *provctx, const char *uri)
{
    unsigned char *storectx = NULL;

    storectx = fake_rsa_st_open_ex(provctx, uri, NULL, NULL, NULL);

    TEST_info("fake_rsa_open called");

    return storectx;
}

static const OSSL_PARAM *fake_rsa_st_settable_ctx_params(void *provctx)
{
    static const OSSL_PARAM known_settable_ctx_params[] = {
        OSSL_PARAM_END
    };
    return known_settable_ctx_params;
}

static int fake_rsa_st_set_ctx_params(void *loaderctx,
                                      const OSSL_PARAM params[])
{
    return 1;
}

static int fake_rsa_st_load(void *loaderctx,
                            OSSL_CALLBACK *object_cb, void *object_cbarg,
                            OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    unsigned char *storectx = loaderctx;
    OSSL_PARAM params[4];
    int object_type = OSSL_OBJECT_PKEY;
    struct fake_rsa_keydata *key = NULL;
    int rv = 0;

    switch (*storectx) {
    case 0:
        if (key_deleted == 1) {
            *storectx = 1;
            break;
        }

        /* Construct a new key using our keymgmt functions */
        if (!TEST_ptr(key = fake_rsa_keymgmt_new(NULL)))
            break;
        if (!TEST_int_gt(fake_rsa_keymgmt_import(key, 0, NULL), 0))
            break;
        params[0] =
            OSSL_PARAM_construct_int(OSSL_OBJECT_PARAM_TYPE, &object_type);
        params[1] =
            OSSL_PARAM_construct_utf8_string(OSSL_OBJECT_PARAM_DATA_TYPE,
                                             "RSA", 0);
        /* The address of the key becomes the octet string */
        params[2] =
            OSSL_PARAM_construct_octet_string(OSSL_OBJECT_PARAM_REFERENCE,
                                              &key, sizeof(key));
        params[3] = OSSL_PARAM_construct_end();
        rv = object_cb(params, object_cbarg);
        *storectx = 1;
        break;

    case 2:
        TEST_info("fake_rsa_load() called in error state");
        break;

    default:
        TEST_info("fake_rsa_load() called in eof state");
        break;
    }

    TEST_info("fake_rsa_load called - rv: %d", rv);

    if (rv == 0 && key_deleted == 0) {
        fake_rsa_keymgmt_free(key);
        *storectx = 2;
    }
    return rv;
}

static int fake_rsa_st_delete(void *loaderctx, const char *uri,
                              const OSSL_PARAM params[],
                              OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    key_deleted = 1;
    return 1;
}

static int fake_rsa_st_eof(void *loaderctx)
{
    unsigned char *storectx = loaderctx;

    /* just one key for now in the fake_rsa store */
    return *storectx != 0;
}

static int fake_rsa_st_close(void *loaderctx)
{
    OPENSSL_free(loaderctx);
    return 1;
}

static const OSSL_DISPATCH fake_rsa_store_funcs[] = {
    { OSSL_FUNC_STORE_OPEN, (void (*)(void))fake_rsa_st_open },
    { OSSL_FUNC_STORE_OPEN_EX, (void (*)(void))fake_rsa_st_open_ex },
    { OSSL_FUNC_STORE_SETTABLE_CTX_PARAMS,
      (void (*)(void))fake_rsa_st_settable_ctx_params },
    { OSSL_FUNC_STORE_SET_CTX_PARAMS, (void (*)(void))fake_rsa_st_set_ctx_params },
    { OSSL_FUNC_STORE_LOAD, (void (*)(void))fake_rsa_st_load },
    { OSSL_FUNC_STORE_EOF, (void (*)(void))fake_rsa_st_eof },
    { OSSL_FUNC_STORE_CLOSE, (void (*)(void))fake_rsa_st_close },
    { OSSL_FUNC_STORE_DELETE, (void (*)(void))fake_rsa_st_delete },
    OSSL_DISPATCH_END,
};

static const OSSL_ALGORITHM fake_rsa_store_algs[] = {
    { "fake_rsa", "provider=fake-rsa", fake_rsa_store_funcs },
    { NULL, NULL, NULL }
};

struct der2key_ctx_st;           /* Forward declaration */
typedef int check_key_fn(void *, struct der2key_ctx_st *ctx);
typedef void adjust_key_fn(void *, struct der2key_ctx_st *ctx);
typedef void free_key_fn(void *);
typedef void *d2i_PKCS8_fn(void **, const unsigned char **, long,
                           struct der2key_ctx_st *);
struct keytype_desc_st {
    const char *keytype_name;
    const OSSL_DISPATCH *fns; /* Keymgmt (to pilfer functions from) */

    /* The input structure name */
    const char *structure_name;

    /*
     * The EVP_PKEY_xxx type macro.  Should be zero for type specific
     * structures, non-zero when the outermost structure is PKCS#8 or
     * SubjectPublicKeyInfo.  This determines which of the function
     * pointers below will be used.
     */
    int evp_type;

    /* The selection mask for OSSL_FUNC_decoder_does_selection() */
    int selection_mask;

    /* For type specific decoders, we use the corresponding d2i */
    d2i_of_void *d2i_private_key; /* From type-specific DER */
    d2i_of_void *d2i_public_key;  /* From type-specific DER */
    d2i_of_void *d2i_key_params;  /* From type-specific DER */
    d2i_PKCS8_fn *d2i_PKCS8;      /* Wrapped in a PrivateKeyInfo */
    d2i_of_void *d2i_PUBKEY;      /* Wrapped in a SubjectPublicKeyInfo */

    /*
     * For any key, we may need to check that the key meets expectations.
     * This is useful when the same functions can decode several variants
     * of a key.
     */
    check_key_fn *check_key;

    /*
     * For any key, we may need to make provider specific adjustments, such
     * as ensure the key carries the correct library context.
     */
    adjust_key_fn *adjust_key;
    /* {type}_free() */
    free_key_fn *free_key;
};

/*
 * Start blatant code steal. Alternative: Open up d2i_X509_PUBKEY_INTERNAL
 * as per https://github.com/openssl/openssl/issues/16697 (TBD)
 * Code from openssl/crypto/x509/x_pubkey.c as
 * ossl_d2i_X509_PUBKEY_INTERNAL is presently not public
 */
struct X509_pubkey_st {
    X509_ALGOR *algor;
    ASN1_BIT_STRING *public_key;

    EVP_PKEY *pkey;

    /* extra data for the callback, used by d2i_PUBKEY_ex */
    OSSL_LIB_CTX *libctx;
    char *propq;
};

ASN1_SEQUENCE(X509_PUBKEY_INTERNAL) = {
        ASN1_SIMPLE(X509_PUBKEY, algor, X509_ALGOR),
        ASN1_SIMPLE(X509_PUBKEY, public_key, ASN1_BIT_STRING)
} static_ASN1_SEQUENCE_END_name(X509_PUBKEY, X509_PUBKEY_INTERNAL)

static X509_PUBKEY *fake_rsa_d2i_X509_PUBKEY_INTERNAL(const unsigned char **pp,
                                                      long len, OSSL_LIB_CTX *libctx)
{
    X509_PUBKEY *xpub = OPENSSL_zalloc(sizeof(*xpub));

    if (xpub == NULL)
        return NULL;
    return (X509_PUBKEY *)ASN1_item_d2i_ex((ASN1_VALUE **)&xpub, pp, len,
                                           ASN1_ITEM_rptr(X509_PUBKEY_INTERNAL),
                                           libctx, NULL);
}
/* end steal https://github.com/openssl/openssl/issues/16697 */

/*
 * Context used for DER to key decoding.
 */
struct der2key_ctx_st {
    PROV_FAKE_RSA_CTX *provctx;
    struct keytype_desc_st *desc;
    /* The selection that is passed to fake_rsa_der2key_decode() */
    int selection;
    /* Flag used to signal that a failure is fatal */
    unsigned int flag_fatal : 1;
};

static int fake_rsa_read_der(PROV_FAKE_RSA_CTX *provctx, OSSL_CORE_BIO *cin,
                             unsigned char **data, long *len)
{
    BUF_MEM *mem = NULL;
    BIO *in = BIO_new_from_core_bio(provctx->libctx, cin);
    int ok = (asn1_d2i_read_bio(in, &mem) >= 0);

    if (ok) {
        *data = (unsigned char *)mem->data;
        *len = (long)mem->length;
        OPENSSL_free(mem);
    }
    BIO_free(in);
    return ok;
}

typedef void *key_from_pkcs8_t(const PKCS8_PRIV_KEY_INFO *p8inf,
                               OSSL_LIB_CTX *libctx, const char *propq);
static void *fake_rsa_der2key_decode_p8(const unsigned char **input_der,
                                        long input_der_len, struct der2key_ctx_st *ctx,
                                        key_from_pkcs8_t *key_from_pkcs8)
{
    PKCS8_PRIV_KEY_INFO *p8inf = NULL;
    const X509_ALGOR *alg = NULL;
    void *key = NULL;

    if ((p8inf = d2i_PKCS8_PRIV_KEY_INFO(NULL, input_der, input_der_len)) != NULL
        && PKCS8_pkey_get0(NULL, NULL, NULL, &alg, p8inf)
        && OBJ_obj2nid(alg->algorithm) == ctx->desc->evp_type)
        key = key_from_pkcs8(p8inf, PROV_FAKE_RSA_LIBCTX_OF(ctx->provctx), NULL);
    PKCS8_PRIV_KEY_INFO_free(p8inf);

    return key;
}

static struct fake_rsa_keydata *fake_rsa_d2i_PUBKEY(struct fake_rsa_keydata **a,
                                                    const unsigned char **pp, long length)
{
    struct fake_rsa_keydata *key = NULL;
    X509_PUBKEY *xpk;

    xpk = fake_rsa_d2i_X509_PUBKEY_INTERNAL(pp, length, NULL);
    if (xpk == NULL)
        goto err_exit;

    key = fake_rsa_keymgmt_new(NULL);
    if (key == NULL)
        goto err_exit;

    key->status = FAKE_RSA_STATUS_DECODED;

    if (a != NULL) {
        fake_rsa_keymgmt_free(*a);
        *a = key;
    }

err_exit:
    X509_PUBKEY_free(xpk);
    return key;
}

/* ---------------------------------------------------------------------- */

static OSSL_FUNC_decoder_freectx_fn der2key_freectx;
static OSSL_FUNC_decoder_decode_fn fake_rsa_der2key_decode;
static OSSL_FUNC_decoder_export_object_fn der2key_export_object;

static struct der2key_ctx_st *
der2key_newctx(void *provctx, struct keytype_desc_st *desc, const char *tls_name)
{
    struct der2key_ctx_st *ctx = OPENSSL_zalloc(sizeof(*ctx));

    if (ctx != NULL) {
        ctx->provctx = provctx;
        ctx->desc = desc;
        if (desc->evp_type == 0)
            ctx->desc->evp_type = OBJ_sn2nid(tls_name);
    }
    return ctx;
}

static void der2key_freectx(void *vctx)
{
    struct der2key_ctx_st *ctx = vctx;

    OPENSSL_free(ctx);
}

static int der2key_check_selection(int selection,
                                   const struct keytype_desc_st *desc)
{
    /*
     * The selections are kinda sorta "levels", i.e. each selection given
     * here is assumed to include those following.
     */
    int checks[] = {
        OSSL_KEYMGMT_SELECT_PRIVATE_KEY,
        OSSL_KEYMGMT_SELECT_PUBLIC_KEY,
        OSSL_KEYMGMT_SELECT_ALL_PARAMETERS
    };
    size_t i;

    /* The decoder implementations made here support guessing */
    if (selection == 0)
        return 1;

    for (i = 0; i < OSSL_NELEM(checks); i++) {
        int check1 = (selection & checks[i]) != 0;
        int check2 = (desc->selection_mask & checks[i]) != 0;

        /*
         * If the caller asked for the currently checked bit(s), return
         * whether the decoder description says it's supported.
         */
        if (check1)
            return check2;
    }

    /* This should be dead code, but just to be safe... */
    return 0;
}

static int fake_rsa_der2key_decode(void *vctx, OSSL_CORE_BIO *cin, int selection,
                                   OSSL_CALLBACK *data_cb, void *data_cbarg,
                                   OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    struct der2key_ctx_st *ctx = vctx;
    unsigned char *der = NULL;
    const unsigned char *derp;
    long der_len = 0;
    void *key = NULL;
    int ok = 0;

    ctx->selection = selection;
    /*
     * The caller is allowed to specify 0 as a selection mark, to have the
     * structure and key type guessed.  For type-specific structures, this
     * is not recommended, as some structures are very similar.
     * Note that 0 isn't the same as OSSL_KEYMGMT_SELECT_ALL, as the latter
     * signifies a private key structure, where everything else is assumed
     * to be present as well.
     */
    if (selection == 0)
        selection = ctx->desc->selection_mask;
    if ((selection & ctx->desc->selection_mask) == 0) {
        ERR_raise(ERR_LIB_PROV, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }

    ok = fake_rsa_read_der(ctx->provctx, cin, &der, &der_len);
    if (!ok)
        goto next;

    ok = 0;                      /* Assume that we fail */

    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0) {
        derp = der;
        if (ctx->desc->d2i_PKCS8 != NULL) {
            key = ctx->desc->d2i_PKCS8(NULL, &derp, der_len, ctx);
            if (ctx->flag_fatal)
                goto end;
        } else if (ctx->desc->d2i_private_key != NULL) {
            key = ctx->desc->d2i_private_key(NULL, &derp, der_len);
        }
        if (key == NULL && ctx->selection != 0)
            goto next;
    }
    if (key == NULL && (selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
        derp = der;
        if (ctx->desc->d2i_PUBKEY != NULL)
            key = ctx->desc->d2i_PUBKEY(NULL, &derp, der_len);
        else
            key = ctx->desc->d2i_public_key(NULL, &derp, der_len);
        if (key == NULL && ctx->selection != 0)
            goto next;
    }
    if (key == NULL && (selection & OSSL_KEYMGMT_SELECT_ALL_PARAMETERS) != 0) {
        derp = der;
        if (ctx->desc->d2i_key_params != NULL)
            key = ctx->desc->d2i_key_params(NULL, &derp, der_len);
        if (key == NULL && ctx->selection != 0)
            goto next;
    }

    /*
     * Last minute check to see if this was the correct type of key.  This
     * should never lead to a fatal error, i.e. the decoding itself was
     * correct, it was just an unexpected key type.  This is generally for
     * classes of key types that have subtle variants, like RSA-PSS keys as
     * opposed to plain RSA keys.
     */
    if (key != NULL
        && ctx->desc->check_key != NULL
        && !ctx->desc->check_key(key, ctx)) {
        ctx->desc->free_key(key);
        key = NULL;
    }

    if (key != NULL && ctx->desc->adjust_key != NULL)
        ctx->desc->adjust_key(key, ctx);

 next:
    /*
     * Indicated that we successfully decoded something, or not at all.
     * Ending up "empty handed" is not an error.
     */
    ok = 1;

    /*
     * We free memory here so it's not held up during the callback, because
     * we know the process is recursive and the allocated chunks of memory
     * add up.
     */
    OPENSSL_free(der);
    der = NULL;

    if (key != NULL) {
        OSSL_PARAM params[4];
        int object_type = OSSL_OBJECT_PKEY;

        params[0] =
            OSSL_PARAM_construct_int(OSSL_OBJECT_PARAM_TYPE, &object_type);
        params[1] =
            OSSL_PARAM_construct_utf8_string(OSSL_OBJECT_PARAM_DATA_TYPE,
                                             (char *)ctx->desc->keytype_name,
                                             0);
        /* The address of the key becomes the octet string */
        params[2] =
            OSSL_PARAM_construct_octet_string(OSSL_OBJECT_PARAM_REFERENCE,
                                              &key, sizeof(key));
        params[3] = OSSL_PARAM_construct_end();

        ok = data_cb(params, data_cbarg);
    }

 end:
    ctx->desc->free_key(key);
    OPENSSL_free(der);

    return ok;
}

static OSSL_FUNC_keymgmt_export_fn *
fake_rsa_prov_get_keymgmt_export(const OSSL_DISPATCH *fns)
{
    /* Pilfer the keymgmt dispatch table */
    for (; fns->function_id != 0; fns++)
        if (fns->function_id == OSSL_FUNC_KEYMGMT_EXPORT)
            return OSSL_FUNC_keymgmt_export(fns);

    return NULL;
}

static int der2key_export_object(void *vctx,
                                 const void *reference, size_t reference_sz,
                                 OSSL_CALLBACK *export_cb, void *export_cbarg)
{
    struct der2key_ctx_st *ctx = vctx;
    OSSL_FUNC_keymgmt_export_fn *export = fake_rsa_prov_get_keymgmt_export(ctx->desc->fns);
    void *keydata;

    if (reference_sz == sizeof(keydata) && export != NULL) {
        /* The contents of the reference is the address to our object */
        keydata = *(void **)reference;

        return export(keydata, ctx->selection, export_cb, export_cbarg);
    }
    return 0;
}

/* ---------------------------------------------------------------------- */

static struct fake_rsa_keydata *fake_rsa_key_from_pkcs8(const PKCS8_PRIV_KEY_INFO *p8inf,
                                                        OSSL_LIB_CTX *libctx, const char *propq)
{
    struct fake_rsa_keydata *key = fake_rsa_keymgmt_new(NULL);

    if (key)
        key->status = FAKE_RSA_STATUS_DECODED;
    return key;
}

#define rsa_evp_type EVP_PKEY_RSA

static void *fake_rsa_d2i_PKCS8(void **key, const unsigned char **der, long der_len,
                                struct der2key_ctx_st *ctx)
{
    return fake_rsa_der2key_decode_p8(der, der_len, ctx,
                                      (key_from_pkcs8_t *)fake_rsa_key_from_pkcs8);
}

static void fake_rsa_key_adjust(void *key, struct der2key_ctx_st *ctx)
{
}

/* ---------------------------------------------------------------------- */

#define DO_PrivateKeyInfo(keytype)                      \
    "PrivateKeyInfo", keytype##_evp_type,               \
        (OSSL_KEYMGMT_SELECT_PRIVATE_KEY),              \
        NULL,                                           \
        NULL,                                           \
        NULL,                                           \
        fake_rsa_d2i_PKCS8,                             \
        NULL,                                           \
        NULL,                                           \
        fake_rsa_key_adjust,                            \
        (free_key_fn *)fake_rsa_keymgmt_free

#define DO_SubjectPublicKeyInfo(keytype)                \
    "SubjectPublicKeyInfo", keytype##_evp_type,         \
        (OSSL_KEYMGMT_SELECT_PUBLIC_KEY),               \
        NULL,                                           \
        NULL,                                           \
        NULL,                                           \
        NULL,                                           \
        (d2i_of_void *)fake_rsa_d2i_PUBKEY,             \
        NULL,                                           \
        fake_rsa_key_adjust,                            \
        (free_key_fn *)fake_rsa_keymgmt_free

/*
 * MAKE_DECODER is the single driver for creating OSSL_DISPATCH tables.
 * It takes the following arguments:
 *
 * keytype_name The implementation key type as a string.
 * keytype      The implementation key type.  This must correspond exactly
 *              to our existing keymgmt keytype names...  in other words,
 *              there must exist an ossl_##keytype##_keymgmt_functions.
 * type         The type name for the set of functions that implement the
 *              decoder for the key type.  This isn't necessarily the same
 *              as keytype.  For example, the key types ed25519, ed448,
 *              x25519 and x448 are all handled by the same functions with
 *              the common type name ecx.
 * kind         The kind of support to implement.  This translates into
 *              the DO_##kind macros above, to populate the keytype_desc_st
 *              structure.
 */
#define MAKE_DECODER(keytype_name, keytype, type, kind)                 \
    static struct keytype_desc_st kind##_##keytype##_desc =             \
        { keytype_name, fake_rsa_keymgmt_funcs,                         \
          DO_##kind(keytype) };                                         \
                                                                        \
    static OSSL_FUNC_decoder_newctx_fn kind##_der2##keytype##_newctx;   \
                                                                        \
    static void *kind##_der2##keytype##_newctx(void *provctx)           \
    {                                                                   \
        return der2key_newctx(provctx, &kind##_##keytype##_desc, keytype_name);\
    }                                                                   \
    static int kind##_der2##keytype##_does_selection(void *provctx,     \
                                                     int selection)     \
    {                                                                   \
        return der2key_check_selection(selection,                       \
                                       &kind##_##keytype##_desc);       \
    }                                                                   \
    static const OSSL_DISPATCH                                          \
    fake_rsa_##kind##_der_to_##keytype##_decoder_functions[] = {        \
        { OSSL_FUNC_DECODER_NEWCTX,                                     \
          (void (*)(void))kind##_der2##keytype##_newctx },              \
        { OSSL_FUNC_DECODER_FREECTX,                                    \
          (void (*)(void))der2key_freectx },                            \
        { OSSL_FUNC_DECODER_DOES_SELECTION,                             \
          (void (*)(void))kind##_der2##keytype##_does_selection },      \
        { OSSL_FUNC_DECODER_DECODE,                                     \
          (void (*)(void))fake_rsa_der2key_decode },                    \
        { OSSL_FUNC_DECODER_EXPORT_OBJECT,                              \
          (void (*)(void))der2key_export_object },                      \
        OSSL_DISPATCH_END                                               \
    }

MAKE_DECODER("RSA", rsa, rsa, PrivateKeyInfo);
MAKE_DECODER("RSA", rsa, rsa, SubjectPublicKeyInfo);

static const OSSL_ALGORITHM fake_rsa_decoder_algs[] = {
#define DECODER_PROVIDER "fake-rsa"
#define DECODER_STRUCTURE_SubjectPublicKeyInfo          "SubjectPublicKeyInfo"
#define DECODER_STRUCTURE_PrivateKeyInfo                "PrivateKeyInfo"

/* Arguments are prefixed with '_' to avoid build breaks on certain platforms */
/*
 * Obviously this is not FIPS approved, but in order to test in conjunction
 * with the FIPS provider we pretend that it is.
 */

#define DECODER(_name, _input, _output)                         \
    { _name,                                                    \
      "provider=" DECODER_PROVIDER ",fips=yes,input=" #_input,  \
      (fake_rsa_##_input##_to_##_output##_decoder_functions)    \
    }
#define DECODER_w_structure(_name, _input, _structure, _output) \
    { _name,                                                    \
      "provider=" DECODER_PROVIDER ",fips=yes,input=" #_input   \
      ",structure=" DECODER_STRUCTURE_##_structure,             \
      (fake_rsa_##_structure##_##_input##_to_##_output##_decoder_functions) \
    }

DECODER_w_structure("RSA:rsaEncryption", der, PrivateKeyInfo, rsa),
DECODER_w_structure("RSA:rsaEncryption", der, SubjectPublicKeyInfo, rsa),
#undef DECODER_PROVIDER
    { NULL, NULL, NULL }
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

    case OSSL_OP_STORE:
        return fake_rsa_store_algs;

    case OSSL_OP_DECODER:
        return fake_rsa_decoder_algs;
    }
    return NULL;
}

static void fake_rsa_prov_teardown(void *provctx)
{
    PROV_FAKE_RSA_CTX *pctx = (PROV_FAKE_RSA_CTX *)provctx;

    OSSL_LIB_CTX_free(pctx->libctx);
    OPENSSL_free(pctx);
}

/* Functions we provide to the core */
static const OSSL_DISPATCH fake_rsa_method[] = {
    { OSSL_FUNC_PROVIDER_TEARDOWN, (void (*)(void))fake_rsa_prov_teardown },
    { OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))fake_rsa_query },
    OSSL_DISPATCH_END
};

static int fake_rsa_provider_init(const OSSL_CORE_HANDLE *handle,
                                  const OSSL_DISPATCH *in,
                                  const OSSL_DISPATCH **out, void **provctx)
{
    OSSL_LIB_CTX *libctx;
    PROV_FAKE_RSA_CTX *prov_ctx;

    if (!TEST_ptr(libctx = OSSL_LIB_CTX_new_from_dispatch(handle, in)))
        return 0;

    if (!TEST_ptr(prov_ctx = OPENSSL_malloc(sizeof(*prov_ctx)))) {
        OSSL_LIB_CTX_free(libctx);
        return 0;
    }

    prov_ctx->libctx = libctx;

    *provctx = prov_ctx;
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

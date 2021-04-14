/*
 * Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/store.h>
#include "internal/cryptlib.h"
#include "crypto/x509.h"
#include "x509_local.h"

/* Generic object loader, given expected type and criterion */
static int cache_objects(X509_LOOKUP *lctx, const char *uri,
                         const OSSL_STORE_SEARCH *criterion,
                         int depth, OSSL_LIB_CTX *libctx, const char *propq)
{
    int ok = 0;
    OSSL_STORE_CTX *ctx = NULL;
    X509_STORE *xstore = X509_LOOKUP_get_store(lctx);

    if ((ctx = OSSL_STORE_open_ex(uri, libctx, propq, NULL, NULL, NULL,
                                  NULL, NULL)) == NULL)
        return 0;

    /*
     * We try to set the criterion, but don't care if it was valid or not.
     * For a OSSL_STORE, it merely serves as an optimization, the expectation
     * being that if the criterion couldn't be used, we will get *everything*
     * from the container that the URI represents rather than the subset that
     * the criterion indicates, so the biggest harm is that we cache more
     * objects certs and CRLs than we may expect, but that's ok.
     *
     * Specifically for OpenSSL's own file: scheme, the only workable
     * criterion is the BY_NAME one, which it can only apply on directories,
     * but it's possible that the URI is a single file rather than a directory,
     * and in that case, the BY_NAME criterion is pointless.
     *
     * We could very simply not apply any criterion at all here, and just let
     * the code that selects certs and CRLs from the cached objects do its job,
     * but it's a nice optimization when it can be applied (such as on an
     * actual directory with a thousand CA certs).
     */
    if (criterion != NULL)
        OSSL_STORE_find(ctx, criterion);

    for (;;) {
        OSSL_STORE_INFO *info = OSSL_STORE_load(ctx);
        int infotype;

        /* NULL means error or "end of file".  Either way, we break. */
        if (info == NULL)
            break;

        infotype = OSSL_STORE_INFO_get_type(info);
        ok = 0;

        if (infotype == OSSL_STORE_INFO_NAME) {
            /*
             * This is an entry in the "directory" represented by the current
             * uri.  if |depth| allows, dive into it.
             */
            if (depth > 0)
                ok = cache_objects(lctx, OSSL_STORE_INFO_get0_NAME(info),
                                   criterion, depth - 1, libctx, propq);
        } else {
            /*
             * We know that X509_STORE_add_{cert|crl} increments the object's
             * refcount, so we can safely use OSSL_STORE_INFO_get0_{cert,crl}
             * to get them.
             */
            switch (infotype) {
            case OSSL_STORE_INFO_CERT:
                ok = X509_STORE_add_cert(xstore,
                                         OSSL_STORE_INFO_get0_CERT(info));
                break;
            case OSSL_STORE_INFO_CRL:
                ok = X509_STORE_add_crl(xstore,
                                        OSSL_STORE_INFO_get0_CRL(info));
                break;
            }
        }

        OSSL_STORE_INFO_free(info);
        if (!ok)
            break;
    }
    OSSL_STORE_close(ctx);

    return ok;
}


/* Because OPENSSL_free is a macro and for C type match */
static void free_uri(OPENSSL_STRING data)
{
    OPENSSL_free(data);
}

static void by_store_free(X509_LOOKUP *ctx)
{
    STACK_OF(OPENSSL_STRING) *uris = X509_LOOKUP_get_method_data(ctx);
    sk_OPENSSL_STRING_pop_free(uris, free_uri);
}

static int by_store_ctrl_ex(X509_LOOKUP *ctx, int cmd, const char *argp,
                            long argl, char **retp, OSSL_LIB_CTX *libctx,
                            const char *propq)
{
    switch (cmd) {
    case X509_L_ADD_STORE:
        /* If no URI is given, use the default cert dir as default URI */
        if (argp == NULL)
            argp = ossl_safe_getenv(X509_get_default_cert_dir_env());
        if (argp == NULL)
            argp = X509_get_default_cert_dir();

        {
            STACK_OF(OPENSSL_STRING) *uris = X509_LOOKUP_get_method_data(ctx);

            if (uris == NULL) {
                uris = sk_OPENSSL_STRING_new_null();
                X509_LOOKUP_set_method_data(ctx, uris);
            }
            return sk_OPENSSL_STRING_push(uris, OPENSSL_strdup(argp)) > 0;
        }
    case X509_L_LOAD_STORE:
        /* This is a shortcut for quick loading of specific containers */
        return cache_objects(ctx, argp, NULL, 0, libctx, propq);
    }

    return 0;
}

static int by_store_ctrl(X509_LOOKUP *ctx, int cmd,
                         const char *argp, long argl, char **retp)
{
    return by_store_ctrl_ex(ctx, cmd, argp, argl, retp, NULL, NULL);
}

static int by_store(X509_LOOKUP *ctx, X509_LOOKUP_TYPE type,
                    const OSSL_STORE_SEARCH *criterion, X509_OBJECT *ret,
                    OSSL_LIB_CTX *libctx, const char *propq)
{
    STACK_OF(OPENSSL_STRING) *uris = X509_LOOKUP_get_method_data(ctx);
    int i;
    int ok = 0;

    for (i = 0; i < sk_OPENSSL_STRING_num(uris); i++) {
        ok = cache_objects(ctx, sk_OPENSSL_STRING_value(uris, i), criterion,
                           1 /* depth */, libctx, propq);

        if (ok)
            break;
    }
    return ok;
}

static int by_store_subject_ex(X509_LOOKUP *ctx, X509_LOOKUP_TYPE type,
                               const X509_NAME *name, X509_OBJECT *ret,
                               OSSL_LIB_CTX *libctx, const char *propq)
{
    OSSL_STORE_SEARCH *criterion =
        OSSL_STORE_SEARCH_by_name((X509_NAME *)name); /* won't modify it */
    int ok = by_store(ctx, type, criterion, ret, libctx, propq);
    STACK_OF(X509_OBJECT) *store_objects =
        X509_STORE_get0_objects(X509_LOOKUP_get_store(ctx));
    X509_OBJECT *tmp = NULL;

    OSSL_STORE_SEARCH_free(criterion);

    if (ok)
        tmp = X509_OBJECT_retrieve_by_subject(store_objects, type, name);

    ok = 0;
    if (tmp != NULL) {
        /*
         * This could also be done like this:
         *
         *     if (tmp != NULL) {
         *         *ret = *tmp;
         *         ok = 1;
         *     }
         *
         * However, we want to exercise the documented API to the max, so
         * we do it the hard way.
         *
         * To be noted is that X509_OBJECT_set1_* increment the refcount,
         * but so does X509_STORE_CTX_get_by_subject upon return of this
         * function, so we must ensure the refcount is decremented
         * before we return, or we will get a refcount leak.  We cannot do
         * this with X509_OBJECT_free(), though, as that will free a bit
         * too much.
         */
        switch (type) {
        case X509_LU_X509:
            ok = X509_OBJECT_set1_X509(ret, tmp->data.x509);
            if (ok)
                X509_free(tmp->data.x509);
            break;
        case X509_LU_CRL:
            ok = X509_OBJECT_set1_X509_CRL(ret, tmp->data.crl);
            if (ok)
                X509_CRL_free(tmp->data.crl);
            break;
        case X509_LU_NONE:
            break;
        }
    }
    return ok;
}

static int by_store_subject(X509_LOOKUP *ctx, X509_LOOKUP_TYPE type,
                            const X509_NAME *name, X509_OBJECT *ret)
{
    return by_store_subject_ex(ctx, type, name, ret, NULL, NULL);
}

/*
 * We lack the implementations for get_by_issuer_serial, get_by_fingerprint
 * and get_by_alias.  There's simply not enough support in the X509_LOOKUP
 * or X509_STORE APIs.
 */

static X509_LOOKUP_METHOD x509_store_lookup = {
    "Load certs from STORE URIs",
    NULL,                        /* new_item */
    by_store_free,               /* free */
    NULL,                        /* init */
    NULL,                        /* shutdown */
    by_store_ctrl,               /* ctrl */
    by_store_subject,            /* get_by_subject */
    NULL,                        /* get_by_issuer_serial */
    NULL,                        /* get_by_fingerprint */
    NULL,                        /* get_by_alias */
    by_store_subject_ex,
    by_store_ctrl_ex
};

X509_LOOKUP_METHOD *X509_LOOKUP_store(void)
{
    return &x509_store_lookup;
}

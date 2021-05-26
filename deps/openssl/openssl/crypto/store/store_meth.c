/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/store.h>
#include <openssl/crypto.h>
#include "internal/core.h"
#include "internal/namemap.h"
#include "internal/property.h"
#include "internal/provider.h"
#include "store_local.h"

int OSSL_STORE_LOADER_up_ref(OSSL_STORE_LOADER *loader)
{
    int ref = 0;

    if (loader->prov != NULL)
        CRYPTO_UP_REF(&loader->refcnt, &ref, loader->lock);
    return 1;
}

void OSSL_STORE_LOADER_free(OSSL_STORE_LOADER *loader)
{
    if (loader != NULL && loader->prov != NULL) {
        int i;

        CRYPTO_DOWN_REF(&loader->refcnt, &i, loader->lock);
        if (i > 0)
            return;
        ossl_provider_free(loader->prov);
        CRYPTO_THREAD_lock_free(loader->lock);
    }
    OPENSSL_free(loader);
}

/*
 * OSSL_STORE_LOADER_new() expects the scheme as a constant string,
 * which we currently don't have, so we need an alternative allocator.
 */
static OSSL_STORE_LOADER *new_loader(OSSL_PROVIDER *prov)
{
    OSSL_STORE_LOADER *loader;

    if ((loader = OPENSSL_zalloc(sizeof(*loader))) == NULL
        || (loader->lock = CRYPTO_THREAD_lock_new()) == NULL) {
        OPENSSL_free(loader);
        return NULL;
    }
    loader->prov = prov;
    ossl_provider_up_ref(prov);
    loader->refcnt = 1;

    return loader;
}

static int up_ref_loader(void *method)
{
    return OSSL_STORE_LOADER_up_ref(method);
}

static void free_loader(void *method)
{
    OSSL_STORE_LOADER_free(method);
}

/* Permanent loader method store, constructor and destructor */
static void loader_store_free(void *vstore)
{
    ossl_method_store_free(vstore);
}

static void *loader_store_new(OSSL_LIB_CTX *ctx)
{
    return ossl_method_store_new(ctx);
}


static const OSSL_LIB_CTX_METHOD loader_store_method = {
    OSSL_LIB_CTX_METHOD_DEFAULT_PRIORITY,
    loader_store_new,
    loader_store_free,
};

/* Data to be passed through ossl_method_construct() */
struct loader_data_st {
    OSSL_LIB_CTX *libctx;
    OSSL_METHOD_CONSTRUCT_METHOD *mcm;
    int scheme_id;               /* For get_loader_from_store() */
    const char *scheme;          /* For get_loader_from_store() */
    const char *propquery;       /* For get_loader_from_store() */

    unsigned int flag_construct_error_occurred : 1;
};

/*
 * Generic routines to fetch / create OSSL_STORE methods with
 * ossl_method_construct()
 */

/* Temporary loader method store, constructor and destructor */
static void *alloc_tmp_loader_store(OSSL_LIB_CTX *ctx)
{
    return ossl_method_store_new(ctx);
}

 static void dealloc_tmp_loader_store(void *store)
{
    if (store != NULL)
        ossl_method_store_free(store);
}

/* Get the permanent loader store */
static OSSL_METHOD_STORE *get_loader_store(OSSL_LIB_CTX *libctx)
{
    return ossl_lib_ctx_get_data(libctx, OSSL_LIB_CTX_STORE_LOADER_STORE_INDEX,
                                &loader_store_method);
}

/* Get loader methods from a store, or put one in */
static void *get_loader_from_store(OSSL_LIB_CTX *libctx, void *store,
                                   void *data)
{
    struct loader_data_st *methdata = data;
    void *method = NULL;
    int id;

    if ((id = methdata->scheme_id) == 0) {
        OSSL_NAMEMAP *namemap = ossl_namemap_stored(libctx);

        id = ossl_namemap_name2num(namemap, methdata->scheme);
    }

    if (store == NULL
        && (store = get_loader_store(libctx)) == NULL)
        return NULL;

    if (!ossl_method_store_fetch(store, id, methdata->propquery, &method))
        return NULL;
    return method;
}

static int put_loader_in_store(OSSL_LIB_CTX *libctx, void *store,
                               void *method, const OSSL_PROVIDER *prov,
                               int operation_id, const char *scheme,
                               const char *propdef, void *unused)
{
    OSSL_NAMEMAP *namemap;
    int id;

    if ((namemap = ossl_namemap_stored(libctx)) == NULL
        || (id = ossl_namemap_name2num(namemap, scheme)) == 0)
        return 0;

    if (store == NULL && (store = get_loader_store(libctx)) == NULL)
        return 0;

    return ossl_method_store_add(store, prov, id, propdef, method,
                                 up_ref_loader, free_loader);
}

static void *loader_from_algorithm(int scheme_id, const OSSL_ALGORITHM *algodef,
                                   OSSL_PROVIDER *prov)
{
    OSSL_STORE_LOADER *loader = NULL;
    const OSSL_DISPATCH *fns = algodef->implementation;

    if ((loader = new_loader(prov)) == NULL)
        return NULL;
    loader->scheme_id = scheme_id;
    loader->propdef = algodef->property_definition;
    loader->description = algodef->algorithm_description;

    for (; fns->function_id != 0; fns++) {
        switch (fns->function_id) {
        case OSSL_FUNC_STORE_OPEN:
            if (loader->p_open == NULL)
                loader->p_open = OSSL_FUNC_store_open(fns);
            break;
        case OSSL_FUNC_STORE_ATTACH:
            if (loader->p_attach == NULL)
                loader->p_attach = OSSL_FUNC_store_attach(fns);
            break;
        case OSSL_FUNC_STORE_SETTABLE_CTX_PARAMS:
            if (loader->p_settable_ctx_params == NULL)
                loader->p_settable_ctx_params =
                    OSSL_FUNC_store_settable_ctx_params(fns);
            break;
        case OSSL_FUNC_STORE_SET_CTX_PARAMS:
            if (loader->p_set_ctx_params == NULL)
                loader->p_set_ctx_params = OSSL_FUNC_store_set_ctx_params(fns);
            break;
        case OSSL_FUNC_STORE_LOAD:
            if (loader->p_load == NULL)
                loader->p_load = OSSL_FUNC_store_load(fns);
            break;
        case OSSL_FUNC_STORE_EOF:
            if (loader->p_eof == NULL)
                loader->p_eof = OSSL_FUNC_store_eof(fns);
            break;
        case OSSL_FUNC_STORE_CLOSE:
            if (loader->p_close == NULL)
                loader->p_close = OSSL_FUNC_store_close(fns);
            break;
        case OSSL_FUNC_STORE_EXPORT_OBJECT:
            if (loader->p_export_object == NULL)
                loader->p_export_object = OSSL_FUNC_store_export_object(fns);
            break;
        }
    }

    if ((loader->p_open == NULL && loader->p_attach == NULL)
        || loader->p_load == NULL
        || loader->p_eof == NULL
        || loader->p_close == NULL) {
        /* Only set_ctx_params is optionaal */
        OSSL_STORE_LOADER_free(loader);
        ERR_raise(ERR_LIB_OSSL_STORE, OSSL_STORE_R_LOADER_INCOMPLETE);
        return NULL;
    }
    return loader;
}

/*
 * The core fetching functionality passes the scheme of the implementation.
 * This function is responsible to getting an identity number for them,
 * then call loader_from_algorithm() with that identity number.
 */
static void *construct_loader(const OSSL_ALGORITHM *algodef,
                              OSSL_PROVIDER *prov, void *data)
{
    /*
     * This function is only called if get_loader_from_store() returned
     * NULL, so it's safe to say that of all the spots to create a new
     * namemap entry, this is it.  Should the scheme already exist there, we
     * know that ossl_namemap_add() will return its corresponding number.
     */
    struct loader_data_st *methdata = data;
    OSSL_LIB_CTX *libctx = ossl_provider_libctx(prov);
    OSSL_NAMEMAP *namemap = ossl_namemap_stored(libctx);
    const char *scheme = algodef->algorithm_names;
    int id = ossl_namemap_add_name(namemap, 0, scheme);
    void *method = NULL;

    if (id != 0)
        method = loader_from_algorithm(id, algodef, prov);

    /*
     * Flag to indicate that there was actual construction errors.  This
     * helps inner_evp_generic_fetch() determine what error it should
     * record on inaccessible algorithms.
     */
    if (method == NULL)
        methdata->flag_construct_error_occurred = 1;

    return method;
}

/* Intermediary function to avoid ugly casts, used below */
static void destruct_loader(void *method, void *data)
{
    OSSL_STORE_LOADER_free(method);
}

/* Fetching support.  Can fetch by numeric identity or by scheme */
static OSSL_STORE_LOADER *inner_loader_fetch(OSSL_LIB_CTX *libctx,
                                             int id, const char *scheme,
                                             const char *properties)
{
    OSSL_METHOD_STORE *store = get_loader_store(libctx);
    OSSL_NAMEMAP *namemap = ossl_namemap_stored(libctx);
    void *method = NULL;
    int unsupported = 0;

    if (store == NULL || namemap == NULL) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_PASSED_INVALID_ARGUMENT);
        return NULL;
    }

    /*
     * If we have been passed neither a scheme_id nor a scheme, we have an
     * internal programming error.
     */
    if (!ossl_assert(id != 0 || scheme != NULL)) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_INTERNAL_ERROR);
        return NULL;
    }

    /* If we haven't received a name id yet, try to get one for the name */
    if (id == 0)
        id = ossl_namemap_name2num(namemap, scheme);

    /*
     * If we haven't found the name yet, chances are that the algorithm to
     * be fetched is unsupported.
     */
    if (id == 0)
        unsupported = 1;

    if (id == 0
        || !ossl_method_store_cache_get(store, id, properties, &method)) {
        OSSL_METHOD_CONSTRUCT_METHOD mcm = {
            alloc_tmp_loader_store,
            dealloc_tmp_loader_store,
            get_loader_from_store,
            put_loader_in_store,
            construct_loader,
            destruct_loader
        };
        struct loader_data_st mcmdata;

        mcmdata.libctx = libctx;
        mcmdata.mcm = &mcm;
        mcmdata.scheme_id = id;
        mcmdata.scheme = scheme;
        mcmdata.propquery = properties;
        mcmdata.flag_construct_error_occurred = 0;
        if ((method = ossl_method_construct(libctx, OSSL_OP_STORE,
                                            0 /* !force_cache */,
                                            &mcm, &mcmdata)) != NULL) {
            /*
             * If construction did create a method for us, we know that there
             * is a correct scheme_id, since those have already been calculated
             * in get_loader_from_store() and put_loader_in_store() above.
             */
            if (id == 0)
                id = ossl_namemap_name2num(namemap, scheme);
            ossl_method_store_cache_set(store, id, properties, method,
                                        up_ref_loader, free_loader);
        }

        /*
         * If we never were in the constructor, the algorithm to be fetched
         * is unsupported.
         */
        unsupported = !mcmdata.flag_construct_error_occurred;
    }

    if (method == NULL) {
        int code = unsupported ? ERR_R_UNSUPPORTED : ERR_R_FETCH_FAILED;

        if (scheme == NULL)
            scheme = ossl_namemap_num2name(namemap, id, 0);
        ERR_raise_data(ERR_LIB_OSSL_STORE, code,
                       "%s, Scheme (%s : %d), Properties (%s)",
                       ossl_lib_ctx_get_descriptor(libctx),
                       scheme = NULL ? "<null>" : scheme, id,
                       properties == NULL ? "<null>" : properties);
    }

    return method;
}

OSSL_STORE_LOADER *OSSL_STORE_LOADER_fetch(const char *scheme,
                                           OSSL_LIB_CTX *libctx,
                                           const char *properties)
{
    return inner_loader_fetch(libctx, 0, scheme, properties);
}

OSSL_STORE_LOADER *ossl_store_loader_fetch_by_number(OSSL_LIB_CTX *libctx,
                                                     int scheme_id,
                                                     const char *properties)
{
    return inner_loader_fetch(libctx, scheme_id, NULL, properties);
}

/*
 * Library of basic method functions
 */

const OSSL_PROVIDER *OSSL_STORE_LOADER_provider(const OSSL_STORE_LOADER *loader)
{
    if (!ossl_assert(loader != NULL)) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    return loader->prov;
}

const char *OSSL_STORE_LOADER_properties(const OSSL_STORE_LOADER *loader)
{
    if (!ossl_assert(loader != NULL)) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    return loader->propdef;
}

int OSSL_STORE_LOADER_number(const OSSL_STORE_LOADER *loader)
{
    if (!ossl_assert(loader != NULL)) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    return loader->scheme_id;
}

const char *OSSL_STORE_LOADER_description(const OSSL_STORE_LOADER *loader)
{
    return loader->description;
}

int OSSL_STORE_LOADER_is_a(const OSSL_STORE_LOADER *loader, const char *name)
{
    if (loader->prov != NULL) {
        OSSL_LIB_CTX *libctx = ossl_provider_libctx(loader->prov);
        OSSL_NAMEMAP *namemap = ossl_namemap_stored(libctx);

        return ossl_namemap_name2num(namemap, name) == loader->scheme_id;
    }
    return 0;
}

struct loader_do_all_data_st {
    void (*user_fn)(void *method, void *arg);
    void *user_arg;
};

static void loader_do_one(OSSL_PROVIDER *provider,
                          const OSSL_ALGORITHM *algodef,
                          int no_store, void *vdata)
{
    struct loader_do_all_data_st *data = vdata;
    OSSL_LIB_CTX *libctx = ossl_provider_libctx(provider);
    OSSL_NAMEMAP *namemap = ossl_namemap_stored(libctx);
    const char *name = algodef->algorithm_names;
    int id = ossl_namemap_add_name(namemap, 0, name);
    void *method = NULL;

    if (id != 0)
        method =
            loader_from_algorithm(id, algodef, provider);

    if (method != NULL) {
        data->user_fn(method, data->user_arg);
        OSSL_STORE_LOADER_free(method);
    }
}

void OSSL_STORE_LOADER_do_all_provided(OSSL_LIB_CTX *libctx,
                                       void (*fn)(OSSL_STORE_LOADER *loader,
                                                  void *arg),
                                       void *arg)
{
    struct loader_do_all_data_st data;

    data.user_fn = (void (*)(void *, void *))fn;
    data.user_arg = arg;
    ossl_algorithm_do_all(libctx, OSSL_OP_STORE, NULL,
                          NULL, loader_do_one, NULL,
                          &data);
}

int OSSL_STORE_LOADER_names_do_all(const OSSL_STORE_LOADER *loader,
                                   void (*fn)(const char *name, void *data),
                                   void *data)
{
    if (loader == NULL)
        return 0;

    if (loader->prov != NULL) {
        OSSL_LIB_CTX *libctx = ossl_provider_libctx(loader->prov);
        OSSL_NAMEMAP *namemap = ossl_namemap_stored(libctx);

        return ossl_namemap_doall_names(namemap, loader->scheme_id, fn, data);
    }

    return 1;
}

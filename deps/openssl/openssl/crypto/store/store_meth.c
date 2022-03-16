/*
 * Copyright 2020-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include "crypto/store.h"
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
    /* We want loader_store to be cleaned up before the provider store */
    OSSL_LIB_CTX_METHOD_PRIORITY_2,
    loader_store_new,
    loader_store_free,
};

/* Data to be passed through ossl_method_construct() */
struct loader_data_st {
    OSSL_LIB_CTX *libctx;
    int scheme_id;               /* For get_loader_from_store() */
    const char *scheme;          /* For get_loader_from_store() */
    const char *propquery;       /* For get_loader_from_store() */

    OSSL_METHOD_STORE *tmp_store; /* For get_tmp_loader_store() */

    unsigned int flag_construct_error_occurred : 1;
};

/*
 * Generic routines to fetch / create OSSL_STORE methods with
 * ossl_method_construct()
 */

/* Temporary loader method store, constructor and destructor */
static void *get_tmp_loader_store(void *data)
{
    struct loader_data_st *methdata = data;

    if (methdata->tmp_store == NULL)
        methdata->tmp_store = ossl_method_store_new(methdata->libctx);
    return methdata->tmp_store;
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
static void *get_loader_from_store(void *store, const OSSL_PROVIDER **prov,
                                   void *data)
{
    struct loader_data_st *methdata = data;
    void *method = NULL;
    int id;

    if ((id = methdata->scheme_id) == 0) {
        OSSL_NAMEMAP *namemap = ossl_namemap_stored(methdata->libctx);

        id = ossl_namemap_name2num(namemap, methdata->scheme);
    }

    if (store == NULL
        && (store = get_loader_store(methdata->libctx)) == NULL)
        return NULL;

    if (!ossl_method_store_fetch(store, id, methdata->propquery, prov, &method))
        return NULL;
    return method;
}

static int put_loader_in_store(void *store, void *method,
                               const OSSL_PROVIDER *prov,
                               const char *scheme, const char *propdef,
                               void *data)
{
    struct loader_data_st *methdata = data;
    OSSL_NAMEMAP *namemap;
    int id;

    if ((namemap = ossl_namemap_stored(methdata->libctx)) == NULL
        || (id = ossl_namemap_name2num(namemap, scheme)) == 0)
        return 0;

    if (store == NULL && (store = get_loader_store(methdata->libctx)) == NULL)
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
     * helps inner_loader_fetch() determine what error it should
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
static OSSL_STORE_LOADER *
inner_loader_fetch(struct loader_data_st *methdata, int id,
                   const char *scheme, const char *properties)
{
    OSSL_METHOD_STORE *store = get_loader_store(methdata->libctx);
    OSSL_NAMEMAP *namemap = ossl_namemap_stored(methdata->libctx);
    const char *const propq = properties != NULL ? properties : "";
    void *method = NULL;
    int unsupported = 0;

    if (store == NULL || namemap == NULL) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_PASSED_INVALID_ARGUMENT);
        return NULL;
    }

    /*
     * If we have been passed both an id and a scheme, we have an
     * internal programming error.
     */
    if (!ossl_assert(id == 0 || scheme == NULL)) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_INTERNAL_ERROR);
        return NULL;
    }

    /* If we haven't received a name id yet, try to get one for the name */
    if (id == 0 && scheme != NULL)
        id = ossl_namemap_name2num(namemap, scheme);

    /*
     * If we haven't found the name yet, chances are that the algorithm to
     * be fetched is unsupported.
     */
    if (id == 0)
        unsupported = 1;

    if (id == 0
        || !ossl_method_store_cache_get(store, NULL, id, propq, &method)) {
        OSSL_METHOD_CONSTRUCT_METHOD mcm = {
            get_tmp_loader_store,
            get_loader_from_store,
            put_loader_in_store,
            construct_loader,
            destruct_loader
        };
        OSSL_PROVIDER *prov = NULL;

        methdata->scheme_id = id;
        methdata->scheme = scheme;
        methdata->propquery = propq;
        methdata->flag_construct_error_occurred = 0;
        if ((method = ossl_method_construct(methdata->libctx, OSSL_OP_STORE,
                                            &prov, 0 /* !force_cache */,
                                            &mcm, methdata)) != NULL) {
            /*
             * If construction did create a method for us, we know that there
             * is a correct scheme_id, since those have already been calculated
             * in get_loader_from_store() and put_loader_in_store() above.
             */
            if (id == 0)
                id = ossl_namemap_name2num(namemap, scheme);
            ossl_method_store_cache_set(store, prov, id, propq, method,
                                        up_ref_loader, free_loader);
        }

        /*
         * If we never were in the constructor, the algorithm to be fetched
         * is unsupported.
         */
        unsupported = !methdata->flag_construct_error_occurred;
    }

    if ((id != 0 || scheme != NULL) && method == NULL) {
        int code = unsupported ? ERR_R_UNSUPPORTED : ERR_R_FETCH_FAILED;
        const char *helpful_msg =
            unsupported
            ? ( "No store loader found. For standard store loaders you need "
                "at least one of the default or base providers available. "
                "Did you forget to load them? Info: " )
            : "";

        if (scheme == NULL)
            scheme = ossl_namemap_num2name(namemap, id, 0);
        ERR_raise_data(ERR_LIB_OSSL_STORE, code,
                       "%s%s, Scheme (%s : %d), Properties (%s)",
                       helpful_msg,
                       ossl_lib_ctx_get_descriptor(methdata->libctx),
                       scheme = NULL ? "<null>" : scheme, id,
                       properties == NULL ? "<null>" : properties);
    }

    return method;
}

OSSL_STORE_LOADER *OSSL_STORE_LOADER_fetch(OSSL_LIB_CTX *libctx,
                                           const char *scheme,
                                           const char *properties)
{
    struct loader_data_st methdata;
    void *method;

    methdata.libctx = libctx;
    methdata.tmp_store = NULL;
    method = inner_loader_fetch(&methdata, 0, scheme, properties);
    dealloc_tmp_loader_store(methdata.tmp_store);
    return method;
}

OSSL_STORE_LOADER *ossl_store_loader_fetch_by_number(OSSL_LIB_CTX *libctx,
                                                     int scheme_id,
                                                     const char *properties)
{
    struct loader_data_st methdata;
    void *method;

    methdata.libctx = libctx;
    methdata.tmp_store = NULL;
    method = inner_loader_fetch(&methdata, scheme_id, NULL, properties);
    dealloc_tmp_loader_store(methdata.tmp_store);
    return method;
}

/*
 * Library of basic method functions
 */

const OSSL_PROVIDER *OSSL_STORE_LOADER_get0_provider(const OSSL_STORE_LOADER *loader)
{
    if (!ossl_assert(loader != NULL)) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    return loader->prov;
}

const char *OSSL_STORE_LOADER_get0_properties(const OSSL_STORE_LOADER *loader)
{
    if (!ossl_assert(loader != NULL)) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    return loader->propdef;
}

int ossl_store_loader_get_number(const OSSL_STORE_LOADER *loader)
{
    if (!ossl_assert(loader != NULL)) {
        ERR_raise(ERR_LIB_OSSL_STORE, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    return loader->scheme_id;
}

const char *OSSL_STORE_LOADER_get0_description(const OSSL_STORE_LOADER *loader)
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

struct do_one_data_st {
    void (*user_fn)(OSSL_STORE_LOADER *loader, void *arg);
    void *user_arg;
};

static void do_one(ossl_unused int id, void *method, void *arg)
{
    struct do_one_data_st *data = arg;

    data->user_fn(method, data->user_arg);
}

void OSSL_STORE_LOADER_do_all_provided(OSSL_LIB_CTX *libctx,
                                       void (*user_fn)(OSSL_STORE_LOADER *loader,
                                                       void *arg),
                                       void *user_arg)
{
    struct loader_data_st methdata;
    struct do_one_data_st data;

    methdata.libctx = libctx;
    methdata.tmp_store = NULL;
    (void)inner_loader_fetch(&methdata, 0, NULL, NULL /* properties */);

    data.user_fn = user_fn;
    data.user_arg = user_arg;
    if (methdata.tmp_store != NULL)
        ossl_method_store_do_all(methdata.tmp_store, &do_one, &data);
    ossl_method_store_do_all(get_loader_store(libctx), &do_one, &data);
    dealloc_tmp_loader_store(methdata.tmp_store);
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

/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stddef.h>

#include <openssl/core.h>
#include "internal/cryptlib.h"
#include "internal/core.h"
#include "internal/property.h"
#include "internal/provider.h"

struct construct_data_st {
    OSSL_LIB_CTX *libctx;
    OSSL_METHOD_STORE *store;
    int operation_id;
    int force_store;
    OSSL_METHOD_CONSTRUCT_METHOD *mcm;
    void *mcm_data;
};

static int ossl_method_construct_precondition(OSSL_PROVIDER *provider,
                                              int operation_id, void *cbdata,
                                              int *result)
{
    if (!ossl_assert(result != NULL)) {
        ERR_raise(ERR_LIB_CRYPTO, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    if (!ossl_provider_test_operation_bit(provider, operation_id, result))
        return 0;

    /*
     * The result we get tells if methods have already been constructed.
     * However, we want to tell whether construction should happen (true)
     * or not (false), which is the opposite of what we got.
     */
    *result = !*result;

    return 1;
}

static int ossl_method_construct_postcondition(OSSL_PROVIDER *provider,
                                               int operation_id, int no_store,
                                               void *cbdata, int *result)
{
    if (!ossl_assert(result != NULL)) {
        ERR_raise(ERR_LIB_CRYPTO, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    *result = 1;
    return no_store != 0
        || ossl_provider_set_operation_bit(provider, operation_id);
}

static void ossl_method_construct_this(OSSL_PROVIDER *provider,
                                       const OSSL_ALGORITHM *algo,
                                       int no_store, void *cbdata)
{
    struct construct_data_st *data = cbdata;
    void *method = NULL;

    if ((method = data->mcm->construct(algo, provider, data->mcm_data))
        == NULL)
        return;

    /*
     * Note regarding putting the method in stores:
     *
     * we don't need to care if it actually got in or not here.
     * If it didn't get in, it will simply not be available when
     * ossl_method_construct() tries to get it from the store.
     *
     * It is *expected* that the put function increments the refcnt
     * of the passed method.
     */

    if (data->force_store || !no_store) {
        /*
         * If we haven't been told not to store,
         * add to the global store
         */
        data->mcm->put(data->libctx, NULL, method, provider,
                       data->operation_id, algo->algorithm_names,
                       algo->property_definition, data->mcm_data);
    }

    data->mcm->put(data->libctx, data->store, method, provider,
                   data->operation_id, algo->algorithm_names,
                   algo->property_definition, data->mcm_data);

    /* refcnt-- because we're dropping the reference */
    data->mcm->destruct(method, data->mcm_data);
}

void *ossl_method_construct(OSSL_LIB_CTX *libctx, int operation_id,
                            int force_store,
                            OSSL_METHOD_CONSTRUCT_METHOD *mcm, void *mcm_data)
{
    void *method = NULL;

    if ((method = mcm->get(libctx, NULL, mcm_data)) == NULL) {
        struct construct_data_st cbdata;

        /*
         * We have a temporary store to be able to easily search among new
         * items, or items that should find themselves in the global store.
         */
        if ((cbdata.store = mcm->alloc_tmp_store(libctx)) == NULL)
            goto fin;

        cbdata.libctx = libctx;
        cbdata.operation_id = operation_id;
        cbdata.force_store = force_store;
        cbdata.mcm = mcm;
        cbdata.mcm_data = mcm_data;
        ossl_algorithm_do_all(libctx, operation_id, NULL,
                              ossl_method_construct_precondition,
                              ossl_method_construct_this,
                              ossl_method_construct_postcondition,
                              &cbdata);

        method = mcm->get(libctx, cbdata.store, mcm_data);
        if (method == NULL) {
            /*
             * If we get here then we did not construct the method that we
             * attempted to construct. It's possible that another thread got
             * there first and so we skipped construction (pre-condition
             * failed). We check the global store again to see if it has
             * appeared by now.
             */
            method = mcm->get(libctx, NULL, mcm_data);
        }
        mcm->dealloc_tmp_store(cbdata.store);
    }

 fin:
    return method;
}

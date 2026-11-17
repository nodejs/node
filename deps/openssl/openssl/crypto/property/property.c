/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2019, Oracle and/or its affiliates.  All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <openssl/crypto.h>
#include <openssl/provider.h>
#include "internal/property.h"
#include "internal/provider.h"
#include "internal/tsan_assist.h"
#include "internal/threads_common.h"
#include "internal/list.h"
#include "internal/time.h"
#include <openssl/lhash.h>
#include <openssl/rand.h>
#include <openssl/trace.h>
#include "crypto/sparse_array.h"
#include "property_local.h"
#include "crypto/context.h"

/*
 * The shard count was determined through performance testing with the evp_fetch
 * tool on an Intel Xeon Gold 6248R CPU @ 3.00GHz. Testing showed that 4 shards
 * delivered the best performance for 16 or
 * more threads, and close to best performance at below 16 threads.
 */
#ifndef NUM_SHARDS_BITS
#define NUM_SHARDS_BITS 2
#endif
#define NUM_SHARDS (1 << NUM_SHARDS_BITS)

#ifndef MAX_CACHE_LINES_BITS
#define MAX_CACHE_LINES_BITS 3
#endif
#define MAX_CACHE_LINES (1 << MAX_CACHE_LINES_BITS)

typedef struct {
    void *method;
    int (*up_ref)(void *);
    void (*free)(void *);
} METHOD;

typedef struct {
    int archived;
    const OSSL_PROVIDER *provider;
    OSSL_PROPERTY_LIST *properties;
    METHOD method;
} IMPLEMENTATION;

DEFINE_STACK_OF(IMPLEMENTATION)

typedef struct query_st {
    struct query_st *next; /* list pointer for lookup table */
    void *saptr; /* pointer to our owning STORED_ALGORITHM */
    int nid; /* nid of this query */
    int archived; /* Mark entry as no longer findable */
    OSSL_PROVIDER *prov; /*provider this belongs to */
    char *prop_query; /* query string */
    METHOD method; /* METHOD for this query */
} QUERY;

typedef struct {
    int nid;
    STACK_OF(IMPLEMENTATION) *impls;
} ALGORITHM;

typedef struct {
    SPARSE_ARRAY_OF(ALGORITHM) *algs;

    QUERY *cache_lists[MAX_CACHE_LINES];
    QUERY *archive;

    /*
     * Lock to protect each shard of |algs| from concurrent writing,
     * when individual implementations or queries are inserted.  This is used
     * by the appropriate functions here.
     */
    CRYPTO_RWLOCK *lock;
    CRYPTO_RWLOCK *alock;

    /* query cache specific values */

} STORED_ALGORITHMS;

static int ossl_method_store_atomic_insert_to_list(STORED_ALGORITHMS *sa, QUERY *new);
static int ossl_method_store_atomic_archive(STORED_ALGORITHMS *sa, QUERY *old);
static QUERY *ossl_method_store_atomic_find_in_list(STORED_ALGORITHMS *sa, int nid,
    OSSL_PROVIDER *prov, const char *prop_query);
static void ossl_cache_lists_flush(STORED_ALGORITHMS *sa);
static void ossl_cache_lists_free(STORED_ALGORITHMS *sa);
static void ossl_method_store_atomic_clean_archive(STORED_ALGORITHMS *sa);

struct ossl_method_store_st {
    OSSL_LIB_CTX *ctx;
    STORED_ALGORITHMS *algs;
    /*
     * Lock to reserve the whole store.  This is used when fetching a set
     * of algorithms, via these functions, found in crypto/core_fetch.c:
     * ossl_method_construct_reserve_store()
     * ossl_method_construct_unreserve_store()
     */
    CRYPTO_RWLOCK *biglock;
};

DEFINE_SPARSE_ARRAY_OF(ALGORITHM);

DEFINE_STACK_OF(ALGORITHM)

typedef struct ossl_global_properties_st {
    OSSL_PROPERTY_LIST *list;
#ifndef FIPS_MODULE
    unsigned int no_mirrored : 1;
#endif
} OSSL_GLOBAL_PROPERTIES;

#define stored_algs_shard(store, nid) (&(store)->algs[(nid) & (NUM_SHARDS - 1)])

static void ossl_method_cache_flush_alg(STORED_ALGORITHMS *sa,
    ALGORITHM *alg);
static void ossl_method_cache_flush(STORED_ALGORITHMS *sa, int nid);

static ossl_inline QUERY *QUERY_new(size_t prop_query_len)
{
    /*
     * allocate a new QUERY with the associated property query buffer
     * immediately following it
     */
    QUERY *new = OPENSSL_malloc(sizeof(QUERY) + prop_query_len + 1);
    if (new != NULL)
        new->prop_query = (char *)(new + 1);
    return new;
}

static ossl_inline void QUERY_free(QUERY *q)
{
    /*
     * because we allocate the QUERY with its property query string
     * as one contiguous chunk, this frees both
     */
    OPENSSL_free(q);
}

/* Global properties are stored per library context */
void ossl_ctx_global_properties_free(void *vglobp)
{
    OSSL_GLOBAL_PROPERTIES *globp = vglobp;

    if (globp != NULL) {
        ossl_property_free(globp->list);
        OPENSSL_free(globp);
    }
}

void *ossl_ctx_global_properties_new(OSSL_LIB_CTX *ctx)
{
    return OPENSSL_zalloc(sizeof(OSSL_GLOBAL_PROPERTIES));
}

OSSL_PROPERTY_LIST **ossl_ctx_global_properties(OSSL_LIB_CTX *libctx,
    ossl_unused int loadconfig)
{
    OSSL_GLOBAL_PROPERTIES *globp;

#if !defined(FIPS_MODULE) && !defined(OPENSSL_NO_AUTOLOAD_CONFIG)
    if (loadconfig && !OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, NULL))
        return NULL;
#endif
    globp = ossl_lib_ctx_get_data(libctx, OSSL_LIB_CTX_GLOBAL_PROPERTIES);

    return globp != NULL ? &globp->list : NULL;
}

#ifndef FIPS_MODULE
int ossl_global_properties_no_mirrored(OSSL_LIB_CTX *libctx)
{
    OSSL_GLOBAL_PROPERTIES *globp
        = ossl_lib_ctx_get_data(libctx, OSSL_LIB_CTX_GLOBAL_PROPERTIES);

    return globp != NULL && globp->no_mirrored ? 1 : 0;
}

void ossl_global_properties_stop_mirroring(OSSL_LIB_CTX *libctx)
{
    OSSL_GLOBAL_PROPERTIES *globp
        = ossl_lib_ctx_get_data(libctx, OSSL_LIB_CTX_GLOBAL_PROPERTIES);

    if (globp != NULL)
        globp->no_mirrored = 1;
}
#endif

static int ossl_method_up_ref(METHOD *method)
{
    return (*method->up_ref)(method->method);
}

static void ossl_method_free(METHOD *method)
{
    (*method->free)(method->method);
}

static __owur int ossl_property_read_lock(STORED_ALGORITHMS *p)
{
    return p != NULL ? CRYPTO_THREAD_read_lock(p->lock) : 0;
}

static __owur int ossl_property_write_lock(STORED_ALGORITHMS *p)
{
    return p != NULL ? CRYPTO_THREAD_write_lock(p->lock) : 0;
}

static int ossl_property_unlock(STORED_ALGORITHMS *p)
{
    return p != 0 ? CRYPTO_THREAD_unlock(p->lock) : 0;
}

static void impl_free(IMPLEMENTATION *impl)
{
    if (impl != NULL) {
        ossl_method_free(&impl->method);
        OPENSSL_free(impl);
    }
}

static ossl_inline void impl_cache_free_unlinked(QUERY *elem)
{
    if (elem != NULL) {
        ossl_method_free(&elem->method);
        QUERY_free(elem);
    }
}

static void impl_cache_flush_alg(ALGORITHM *alg, STORED_ALGORITHMS *sa)
{
    QUERY *q;
    int i;

    /*
     * Instead of iterating over the hashtable with the
     * ossl_ht_foreach_until function, we just traverse the
     * linked list, as it much faster this way, as we avoid having
     * to visit lots of potentially empty nodes
     */
    for (i = 0; i < MAX_CACHE_LINES; i++) {
        if (!CRYPTO_atomic_load_ptr((void **)&sa->cache_lists[i], (void **)&q, sa->alock))
            return;
        while (q != NULL) {
            if (q->nid == alg->nid)
                ossl_method_store_atomic_archive(sa, q);
            if (!CRYPTO_atomic_load_ptr((void **)&q->next, (void **)&q, sa->alock))
                return;
        }
    }
}

static void alg_cleanup(ossl_uintmax_t idx, ALGORITHM *a, void *arg)
{
    STORED_ALGORITHMS *sa = arg;

    if (a != NULL) {
        sk_IMPLEMENTATION_pop_free(a->impls, &impl_free);
        OPENSSL_free(a);
    }
    if (sa != NULL)
        ossl_sa_ALGORITHM_set(sa->algs, idx, NULL);
}

static void stored_algs_free(STORED_ALGORITHMS *sa)
{
    if (sa == NULL)
        return;

    for (int i = 0; i < NUM_SHARDS; ++i) {
        ossl_sa_ALGORITHM_doall_arg(sa[i].algs, &alg_cleanup, &sa[i]);
        ossl_sa_ALGORITHM_free(sa[i].algs);
        ossl_cache_lists_free(&sa[i]);
        CRYPTO_THREAD_lock_free(sa[i].lock);
        CRYPTO_THREAD_lock_free(sa[i].alock);
    }

    OPENSSL_free(sa);
}

static STORED_ALGORITHMS *stored_algs_new(OSSL_LIB_CTX *ctx)
{
    STORED_ALGORITHMS *ret;

    ret = OPENSSL_calloc(NUM_SHARDS, sizeof(STORED_ALGORITHMS));
    if (ret == NULL)
        return NULL;

    for (int i = 0; i < NUM_SHARDS; ++i) {
        ret[i].algs = ossl_sa_ALGORITHM_new();
        if (ret[i].algs == NULL)
            goto err;

        ret[i].lock = CRYPTO_THREAD_lock_new();
        if (ret[i].lock == NULL)
            goto err;
        ret[i].alock = CRYPTO_THREAD_lock_new();
        if (ret[i].alock == NULL)
            goto err;
    }

    return ret;

err:
    stored_algs_free(ret);

    return NULL;
}

/*
 * The OSSL_LIB_CTX param here allows access to underlying property data needed
 * for computation
 */
OSSL_METHOD_STORE *ossl_method_store_new(OSSL_LIB_CTX *ctx)
{
    OSSL_METHOD_STORE *res;

    res = OPENSSL_zalloc(sizeof(*res));
    if (res != NULL) {
        res->ctx = ctx;
        if ((res->algs = stored_algs_new(ctx)) == NULL
            || (res->biglock = CRYPTO_THREAD_lock_new()) == NULL) {
            ossl_method_store_free(res);
            return NULL;
        }
    }
    return res;
}

void ossl_method_store_free(OSSL_METHOD_STORE *store)
{
    if (store == NULL)
        return;

    stored_algs_free(store->algs);
    CRYPTO_THREAD_lock_free(store->biglock);
    OPENSSL_free(store);
}

int ossl_method_lock_store(OSSL_METHOD_STORE *store)
{
    return store != NULL ? CRYPTO_THREAD_write_lock(store->biglock) : 0;
}

int ossl_method_unlock_store(OSSL_METHOD_STORE *store)
{
    return store != NULL ? CRYPTO_THREAD_unlock(store->biglock) : 0;
}

static ALGORITHM *ossl_method_store_retrieve(STORED_ALGORITHMS *sa, int nid)
{
    return ossl_sa_ALGORITHM_get(sa->algs, nid);
}

static int ossl_method_store_insert(STORED_ALGORITHMS *sa, ALGORITHM *alg)
{
    return ossl_sa_ALGORITHM_set(sa->algs, alg->nid, alg);
}

/**
 * @brief Adds a method to the specified method store.
 *
 * This function adds a new method to the provided method store, associating it
 * with a specified id, properties, and provider. The method is stored with
 * reference count and destruction callbacks.
 *
 * @param store Pointer to the OSSL_METHOD_STORE where the method will be added.
 *              Must be non-null.
 * @param prov Pointer to the OSSL_PROVIDER for the provider of the method.
 *             Must be non-null.
 * @param nid (identifier) associated with the method, must be > 0
 * @param properties String containing properties of the method.
 * @param method Pointer to the method to be added.
 * @param method_up_ref Function pointer for incrementing the method ref count.
 * @param method_destruct Function pointer for destroying the method.
 *
 * @return 1 if the method is successfully added, 0 on failure.
 *
 * If tracing is enabled, a message is printed indicating that the method is
 * being added to the method store.
 *
 * NOTE: The nid parameter here is _not_ a nid in the sense of the NID_* macros.
 * It is an internal unique identifier.
 */
int ossl_method_store_add(OSSL_METHOD_STORE *store, const OSSL_PROVIDER *prov,
    int nid, const char *properties, void *method,
    int (*method_up_ref)(void *),
    void (*method_destruct)(void *))
{
    STORED_ALGORITHMS *sa;
    ALGORITHM *alg = NULL;
    IMPLEMENTATION *impl;
    int ret = 0;
    int i;

    if (nid <= 0 || method == NULL || store == NULL)
        return 0;

    if (properties == NULL)
        properties = "";

    if (!ossl_assert(prov != NULL))
        return 0;

    /* Create new entry */
    impl = OPENSSL_malloc(sizeof(*impl));
    if (impl == NULL)
        return 0;
    impl->archived = 0;
    impl->method.method = method;
    impl->method.up_ref = method_up_ref;
    impl->method.free = method_destruct;
    if (!ossl_method_up_ref(&impl->method)) {
        OPENSSL_free(impl);
        return 0;
    }
    impl->provider = prov;

    sa = stored_algs_shard(store, nid);

    /* Insert into the hash table if required */
    if (!ossl_property_write_lock(sa)) {
        impl_free(impl);
        return 0;
    }

    /*
     * Flush the alg cache of any implementation that already exists
     * for this id.
     * This is done to ensure that on the next lookup we go through the
     * provider comparison in ossl_method_store_fetch.  If we don't do this
     * then this new method won't be given a chance to get selected.
     * NOTE: This doesn't actually remove the method from the backing store
     * It just ensures that we query the backing store when (re)-adding a
     * method to the algorithm cache, in case the one selected by the next
     * query selects a different implementation
     */
    ossl_method_cache_flush(sa, nid);

    /*
     * Parse the properties associated with this method, and convert it to a
     * property list stored against the implementation for later comparison
     * during fetch operations
     */
    if ((impl->properties = ossl_prop_defn_get(store->ctx, properties)) == NULL) {
        impl->properties = ossl_parse_property(store->ctx, properties);
        if (impl->properties == NULL)
            goto err;
        if (!ossl_prop_defn_set(store->ctx, properties, &impl->properties)) {
            ossl_property_free(impl->properties);
            impl->properties = NULL;
            goto err;
        }
    }

    /*
     * Check if we have an algorithm cache already for this nid.  If so use
     * it, otherwise, create it, and insert it into the store
     */
    alg = ossl_method_store_retrieve(sa, nid);
    if (alg == NULL) {
        if ((alg = OPENSSL_zalloc(sizeof(*alg))) == NULL
            || (alg->impls = sk_IMPLEMENTATION_new_null()) == NULL)
            goto err;
        alg->nid = nid;
        if (!ossl_method_store_insert(sa, alg))
            goto err;
        OSSL_TRACE2(QUERY, "Inserted an alg with nid %d into the stored algorithms %p\n",
            nid, (void *)sa);
    }

    /* Push onto stack if there isn't one there already */
    for (i = 0; i < sk_IMPLEMENTATION_num(alg->impls); i++) {
        const IMPLEMENTATION *tmpimpl = sk_IMPLEMENTATION_value(alg->impls, i);

        if (tmpimpl->archived == 1)
            continue;

        if (tmpimpl->provider == impl->provider
            && tmpimpl->properties == impl->properties)
            break;
    }

    if (i == sk_IMPLEMENTATION_num(alg->impls)
        && sk_IMPLEMENTATION_push(alg->impls, impl)) {
        ret = 1;
#ifndef FIPS_MODULE
        OSSL_TRACE_BEGIN(QUERY)
        {
            BIO_printf(trc_out, "Adding to method store "
                                "nid: %d\nproperties: %s\nprovider: %s\n",
                nid, properties,
                ossl_provider_name(prov) == NULL ? "none" : ossl_provider_name(prov));
        }
        OSSL_TRACE_END(QUERY);
#endif
    }
    ossl_property_unlock(sa);
    if (ret == 0)
        impl_free(impl);
    return ret;

err:
    ossl_property_unlock(sa);
    alg_cleanup(0, alg, NULL);
    impl_free(impl);
    return 0;
}

int ossl_method_store_remove(OSSL_METHOD_STORE *store, int nid,
    const void *method)
{
    ALGORITHM *alg = NULL;
    STORED_ALGORITHMS *sa;
    int i;

    if (nid <= 0 || method == NULL || store == NULL)
        return 0;

    sa = stored_algs_shard(store, nid);
    if (!ossl_property_write_lock(sa))
        return 0;
    ossl_method_cache_flush(sa, nid);
    ossl_method_store_atomic_clean_archive(sa);
    alg = ossl_method_store_retrieve(sa, nid);
    if (alg == NULL) {
        ossl_property_unlock(sa);
        return 0;
    }

    /*
     * A sorting find then a delete could be faster but these stacks should be
     * relatively small, so we avoid the overhead.  Sorting could also surprise
     * users when result orderings change (even though they are not guaranteed).
     */
    for (i = 0; i < sk_IMPLEMENTATION_num(alg->impls); i++) {
        IMPLEMENTATION *impl = sk_IMPLEMENTATION_value(alg->impls, i);

        if (impl->method.method == method) {
            impl_free(impl);
            (void)sk_IMPLEMENTATION_delete(alg->impls, i);
            ossl_property_unlock(sa);
            return 1;
        }
    }
    ossl_property_unlock(sa);
    return 0;
}

struct alg_cleanup_by_provider_data_st {
    STORED_ALGORITHMS *sa;
    const OSSL_PROVIDER *prov;
};

/**
 * @brief Cleans up implementations of an algorithm associated with a provider.
 *
 * This function removes all implementations of a specified algorithm that are
 * associated with a given provider. The function walks through the stack of
 * implementations backwards to handle deletions without affecting indexing.
 *
 * @param idx Index of the algorithm (unused in this function).
 * @param alg Pointer to the ALGORITHM structure containing the implementations.
 * @param arg Pointer to the data containing the provider information.
 *
 * If tracing is enabled, messages are printed indicating the removal of each
 * implementation and its properties. If any implementation is removed, the
 * associated cache is flushed.
 */
static void
alg_cleanup_by_provider(ossl_uintmax_t idx, ALGORITHM *alg, void *arg)
{
    struct alg_cleanup_by_provider_data_st *data = arg;
    int i, count;

    /*
     * We walk the stack backwards, to avoid having to deal with stack shifts
     * caused by deletion
     */
    for (count = 0, i = sk_IMPLEMENTATION_num(alg->impls); i-- > 0;) {
        IMPLEMENTATION *impl = sk_IMPLEMENTATION_value(alg->impls, i);

        if (impl->provider == data->prov) {
#ifndef FIPS_MODULE
            OSSL_TRACE_BEGIN(QUERY)
            {
                char buf[512];
                size_t size;

                size = ossl_property_list_to_string(NULL, impl->properties, buf,
                    sizeof(buf));
                BIO_printf(trc_out, "Removing implementation from "
                                    "query cache\nproperties %s\nprovider %s\n",
                    size == 0 ? "none" : buf,
                    ossl_provider_name(impl->provider) == NULL ? "none" : ossl_provider_name(impl->provider));
            }
            OSSL_TRACE_END(QUERY);
#endif
            impl->archived = 1;
            count++;
        }
    }

    /*
     * If we removed any implementation, we also clear the whole associated
     * cache, 'cause that's the sensible thing to do.
     * There's no point flushing the cache entries where we didn't remove
     * any implementation, though.
     */
    if (count > 0)
        ossl_method_cache_flush_alg(data->sa, alg);
}

int ossl_method_store_remove_all_provided(OSSL_METHOD_STORE *store,
    const OSSL_PROVIDER *prov)
{
    struct alg_cleanup_by_provider_data_st data;

    for (int k = 0; k < NUM_SHARDS; ++k) {
        STORED_ALGORITHMS *sa = &store->algs[k];

        if (!ossl_property_write_lock(sa))
            return 0;
        data.prov = prov;
        data.sa = sa;
        ossl_sa_ALGORITHM_doall_arg(sa->algs, &alg_cleanup_by_provider, &data);
        ossl_method_store_atomic_clean_archive(sa);
        ossl_property_unlock(sa);
    }
    return 1;
}

static void alg_do_one(ALGORITHM *alg, IMPLEMENTATION *impl,
    void (*fn)(int id, void *method, void *fnarg),
    void *fnarg)
{
    fn(alg->nid, impl->method.method, fnarg);
}

static void alg_copy(ossl_uintmax_t idx, ALGORITHM *alg, void *arg)
{
    STACK_OF(ALGORITHM) *newalg = arg;
    int i;
    IMPLEMENTATION *impl;

    alg = OPENSSL_memdup(alg, sizeof(ALGORITHM));
    if (alg == NULL)
        return;

    alg->impls = sk_IMPLEMENTATION_dup(alg->impls);

    /*
     * Remove any archived items while we're under lock
     * note we don't have to free the implementation here
     * as its still tracked in the alg struct we're cloning from
     */
    for (i = sk_IMPLEMENTATION_num(alg->impls); i-- > 0;) {
        impl = sk_IMPLEMENTATION_value(alg->impls, i);
        if (impl->archived == 1)
            (void)sk_IMPLEMENTATION_delete(alg->impls, i);
    }

    (void)sk_ALGORITHM_push(newalg, alg);
}

static void del_tmpalg(ALGORITHM *alg)
{
    sk_IMPLEMENTATION_free(alg->impls);
    OPENSSL_free(alg);
}

void ossl_method_store_do_all(OSSL_METHOD_STORE *store,
    void (*fn)(int id, void *method, void *fnarg),
    void *fnarg)
{
    int i, j;
    int numalgs, numimps;
    STACK_OF(ALGORITHM) *tmpalgs;
    ALGORITHM *alg;

    if (store == NULL)
        return;

    for (int k = 0; k < NUM_SHARDS; ++k) {
        STORED_ALGORITHMS *sa = &store->algs[k];

        if (!ossl_property_read_lock(sa))
            return;

        tmpalgs = sk_ALGORITHM_new_reserve(NULL,
            (int)ossl_sa_ALGORITHM_num(sa->algs));
        if (tmpalgs == NULL) {
            ossl_property_unlock(sa);
            return;
        }

        ossl_sa_ALGORITHM_doall_arg(sa->algs, alg_copy, tmpalgs);
        ossl_property_unlock(sa);
        numalgs = sk_ALGORITHM_num(tmpalgs);
        for (i = 0; i < numalgs; i++) {
            alg = sk_ALGORITHM_value(tmpalgs, i);
            numimps = sk_IMPLEMENTATION_num(alg->impls);
            for (j = 0; j < numimps; j++)
                alg_do_one(alg, sk_IMPLEMENTATION_value(alg->impls, j), fn, fnarg);
        }
        sk_ALGORITHM_pop_free(tmpalgs, del_tmpalg);
    }
}

/**
 * @brief Fetches a method from the method store matching the given properties.
 *
 * This function searches the method store for an implementation of a specified
 * method, identified by its id (nid), and matching the given property query. If
 * successful, it returns the method and its associated provider.
 *
 * @param store Pointer to the OSSL_METHOD_STORE from which to fetch the method.
 *              Must be non-null.
 * @param nid (identifier) of the method to be fetched. Must be > 0
 * @param prop_query String containing the property query to match against.
 * @param prov_rw Pointer to the OSSL_PROVIDER to restrict the search to, or
 *                to receive the matched provider.
 * @param method Pointer to receive the fetched method. Must be non-null.
 *
 * @return 1 if the method is successfully fetched, 0 on failure.
 *
 * If tracing is enabled, a message is printed indicating the property query and
 * the resolved provider.
 *
 * NOTE: The nid parameter here is _not_ a NID in the sense of the NID_* macros.
 * It is a unique internal identifier value.
 */
int ossl_method_store_fetch(OSSL_METHOD_STORE *store,
    int nid, const char *prop_query,
    const OSSL_PROVIDER **prov_rw, void **method)
{
    OSSL_PROPERTY_LIST **plp;
    ALGORITHM *alg;
    IMPLEMENTATION *impl, *best_impl = NULL;
    OSSL_PROPERTY_LIST *pq = NULL, *p2 = NULL;
    const OSSL_PROVIDER *prov = prov_rw != NULL ? *prov_rw : NULL;
    int ret = 0;
    int j, best = -1, score, optional;
    STORED_ALGORITHMS *sa;

    if (nid <= 0 || method == NULL || store == NULL)
        return 0;

#if !defined(FIPS_MODULE) && !defined(OPENSSL_NO_AUTOLOAD_CONFIG)
    if (ossl_lib_ctx_is_default(store->ctx)
        && !OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, NULL))
        return 0;
#endif

    sa = stored_algs_shard(store, nid);

    /* This only needs to be a read lock, because the query won't create anything */
    if (!ossl_property_read_lock(sa))
        return 0;

    OSSL_TRACE2(QUERY, "Retrieving by nid %d from stored algorithms %p\n",
        nid, (void *)sa);
    alg = ossl_method_store_retrieve(sa, nid);
    if (alg == NULL) {
        ossl_property_unlock(sa);
        OSSL_TRACE2(QUERY, "Failed to retrieve by nid %d from stored algorithms %p\n",
            nid, (void *)sa);
        return 0;
    }
    OSSL_TRACE2(QUERY, "Retrieved by nid %d from stored algorithms %p\n",
        nid, (void *)sa);

    /*
     * If a property query string is provided, convert it to an
     * OSSL_PROPERTY_LIST structure
     */
    if (prop_query != NULL)
        p2 = pq = ossl_parse_query(store->ctx, prop_query, 0);

    /*
     * If the library context has default properties specified
     * then merge those with the properties passed to this function
     */
    plp = ossl_ctx_global_properties(store->ctx, 0);
    if (plp != NULL && *plp != NULL) {
        if (pq == NULL) {
            pq = *plp;
        } else {
            p2 = ossl_property_merge(pq, *plp);
            ossl_property_free(pq);
            if (p2 == NULL)
                goto fin;
            pq = p2;
        }
    }

    /*
     * Search for a provider that provides this implementation.
     * If the requested provider is NULL, then any provider will do,
     * otherwise we should try to find the one that matches the requested
     * provider.  Note that providers are given implicit preference via the
     * ordering of the implementation stack
     */
    if (pq == NULL) {
        for (j = 0; j < sk_IMPLEMENTATION_num(alg->impls); j++) {
            impl = sk_IMPLEMENTATION_value(alg->impls, j);
            if (impl == NULL || impl->archived == 1)
                continue;

            if (prov == NULL || impl->provider == prov) {
                best_impl = impl;
                ret = 1;
                break;
            }
        }
        goto fin;
    }

    /*
     * If there are optional properties specified
     * then run the search again, and select the provider that matches the
     * most options
     */
    optional = ossl_property_has_optional(pq);
    for (j = 0; j < sk_IMPLEMENTATION_num(alg->impls); j++) {
        impl = sk_IMPLEMENTATION_value(alg->impls, j);
        if (impl != NULL
            && impl->archived == 0
            && (prov == NULL || impl->provider == prov)) {
            score = ossl_property_match_count(pq, impl->properties);
            if (score > best) {
                best_impl = impl;
                best = score;
                ret = 1;
                if (!optional)
                    goto fin;
            }
        }
    }
fin:
    if (ret) {
        *method = best_impl->method.method;
        if (prov_rw != NULL)
            *prov_rw = best_impl->provider;
#ifdef OPENSSL_NO_CACHED_FETCH
        if (!ossl_method_up_ref(&best_impl->method)) {
            ret = 0;
            *method = NULL;
            if (prov_rw != NULL)
                *prov_rw = NULL;
        }
#endif
    } else {
        ret = 0;
    }

#ifndef FIPS_MODULE
    OSSL_TRACE_BEGIN(QUERY)
    {
        char buf[512];
        size_t size;

        size = ossl_property_list_to_string(NULL, pq, buf, 512);
        BIO_printf(trc_out, "method store query with properties %s "
                            "resolves to provider %s\n",
            size == 0 ? "none" : buf,
            best_impl == NULL ? "none" : ossl_provider_name(best_impl->provider));
    }
    OSSL_TRACE_END(QUERY);
#endif

    ossl_property_unlock(sa);
    ossl_property_free(p2);
    return ret;
}

static void ossl_method_cache_flush_alg(STORED_ALGORITHMS *sa,
    ALGORITHM *alg)
{
    impl_cache_flush_alg(alg, sa);
}

static void ossl_method_cache_flush(STORED_ALGORITHMS *sa, int nid)
{
    ALGORITHM *alg = ossl_method_store_retrieve(sa, nid);

    if (alg != NULL)
        ossl_method_cache_flush_alg(sa, alg);
}

static void ossl_cache_lists_flush(STORED_ALGORITHMS *sa)
{
    int i;
    QUERY *idx, *idxn;

    for (i = 0; i < MAX_CACHE_LINES; i++) {
        if (!CRYPTO_atomic_load_ptr((void **)&sa->cache_lists[i], (void **)&idx, sa->alock))
            break;
        while (idx != NULL) {
            if (!CRYPTO_atomic_load_ptr((void **)&idx->next, (void **)&idxn, sa->alock))
                break;
            ossl_method_store_atomic_archive(sa, idx);
            idx = idxn;
        }
    }
}

static void ossl_cache_lists_free(STORED_ALGORITHMS *sa)
{
    int i;
    QUERY *idx, *idxn;

    for (i = 0; i < MAX_CACHE_LINES; i++) {
        if (!CRYPTO_atomic_load_ptr((void **)&sa->cache_lists[i], (void **)&idx, sa->alock))
            return;
        while (idx != NULL) {
            if (!CRYPTO_atomic_load_ptr((void **)&idx->next, (void **)&idxn, sa->alock))
                return;
            impl_cache_free_unlinked(idx);
            idx = idxn;
        }
    }

    if (!CRYPTO_atomic_load_ptr((void **)&sa->archive, (void **)&idx, sa->alock))
        return;
    while (idx != NULL) {
        if (!CRYPTO_atomic_load_ptr((void **)&idx->next, (void **)&idxn, sa->alock))
            return;
        impl_cache_free_unlinked(idx);
        idx = idxn;
    }
}

int ossl_method_store_cache_flush_all(OSSL_METHOD_STORE *store)
{
    for (int i = 0; i < NUM_SHARDS; ++i) {
        STORED_ALGORITHMS *sa = &store->algs[i];

        if (!ossl_property_write_lock(sa))
            return 0;
        ossl_cache_lists_flush(sa);
        ossl_method_store_atomic_clean_archive(sa);
        ossl_property_unlock(sa);
    }

    return 1;
}

static ossl_inline int ossl_method_store_cache_get_atomic(OSSL_METHOD_STORE *store, OSSL_PROVIDER *prov,
    int nid, const char *prop_query, STORED_ALGORITHMS *sa, void **method)
{
    QUERY *r = NULL;
    int res = 0;

    r = ossl_method_store_atomic_find_in_list(sa, nid, prov, prop_query);

    if (r != NULL) {
        *method = r->method.method;
        res = 1;
#ifdef OPENSSL_NO_CACHED_FETCH
        if (!ossl_method_up_ref(&r->method)) {
            *method = NULL;
            res = 0;
        }
#endif
    }

    return res;
}

int ossl_method_store_cache_get(OSSL_METHOD_STORE *store, OSSL_PROVIDER *prov,
    int nid, const char *prop_query, void **method)
{
    int ret;
    STORED_ALGORITHMS *sa;

    if (nid <= 0 || store == NULL || prop_query == NULL)
        return 0;

    sa = stored_algs_shard(store, nid);

    /*
     * Do an atomic linked list walk to search for our entry
     */
    ret = ossl_method_store_cache_get_atomic(store, prov, nid, prop_query, sa,
        method);

    return ret;
}

static int ossl_method_store_atomic_archive(STORED_ALGORITHMS *sa, QUERY *old)
{
    if (!CRYPTO_atomic_store_int(&old->archived, 1, sa->alock))
        return 0;
    return 1;
}

static ossl_inline int ossl_method_store_put_in_archive(STORED_ALGORITHMS *sa, QUERY *old)
{
    /*
     * point the item we're removing's next pointer to the top of the archive list
     * Note: We're writing to the old->next here which is shared, so that's suspicious, but
     * because we've already removed old from the cache_list in ossl_method_store_clean_archive
     * this is safe for the following reasons:
     * 1) the clean path is done under a write lock, so sa->archive is guaranteed stable
     * 2) any concurrent reader (ie ossl_method_store_cache_set|get, if visiting the old node
     * while we're moving it, will either read the true next value (pointing to the next element
     * in the cache_list), or the one we write here (the next list in the archive)
     *
     * Reading the true next value is fine, as that's the normal traversal anyway.
     * Reading the next pointer as pointing into the archive list is not great, but in the worst
     * case this results in a transient failed cache lookup, which just means a temporary slow path
     * retrieval of an algorithm.
     */
    if (!CRYPTO_atomic_load_ptr((void **)&sa->archive, (void **)&old->next, sa->alock))
        return 0;
    /*
     * And update the head of the archive list to be our new entry
     */
    if (!CRYPTO_atomic_store_ptr((void **)&sa->archive, (void **)&old, sa->alock))
        return 0;
    return 1;
}

/*
 * Migrate archived items to the archive list.  Must be done with the property write
 * lock held
 */
static void ossl_method_store_atomic_clean_archive(STORED_ALGORITHMS *sa)
{
    QUERY *idx, *idxn, *tmp;
    int archived;
    int i;
    int lock_failed;

    /*
     * For each of our linked lists
     */
    for (i = 0; i < MAX_CACHE_LINES; i++) {
    restart_list:
        /*
         * Get the head of the list
         */
        if (!CRYPTO_atomic_load_ptr((void **)&sa->cache_lists[i], (void **)&idx, sa->alock))
            continue;
        /*
         * If its NULL, the list is currently empty, move on to the next one
         */
        if (idx == NULL)
            continue;
        /*
         * Get its archived value
         */
        if (!CRYPTO_atomic_load_int(&idx->archived, &archived, sa->alock))
            continue;
        /*
         * Also fetch its next pointer to idxn
         */
        if (!CRYPTO_atomic_load_ptr((void **)&idx->next, (void **)&idxn, sa->alock))
            continue;
        /*
         * If its been archived, we want to move it to the archive list
         */
        if (archived == 1) {
            /*
             * We know this is the current list head we're working with
             * so store the next pointer to be the new list head
             */
            if (!CRYPTO_atomic_cmp_exch_ptr((void **)&sa->cache_lists[i], (void **)&idx, idxn, sa->alock,
                    &lock_failed)) {
                if (lock_failed)
                    continue;
                else
                    goto restart_list;
            }

            if (!ossl_method_store_put_in_archive(sa, idx))
                continue;
            goto restart_list;
        }

        /*
         * At this point our state is:
         * idx - points to an element in cache_lists[i]
         * idxn points to the next entry (i.e. idx->next)
         */
        while (idx != NULL) {
            /*
             * We know idx isn't archived, so we start looking at idxn
             */
            if (idxn != NULL) {
                /*
                 * if its not NULL, see if its archived
                 */
                if (!CRYPTO_atomic_load_int(&idxn->archived, &archived, sa->alock))
                    break;
                /*
                 * If it is, remove it
                 */
                if (archived == 1) {
                    /*
                     * Start by making idx skip idxn in the list
                     * First load the expected next value of idx->next
                     */
                    if (!CRYPTO_atomic_load_ptr((void **)&idx->next, (void **)&tmp, sa->alock))
                        break;

                    /*
                     * Now compare the value of idx->next to what we just loaded to tmp above
                     * if they match, we can safely update idx->next to skip the idxn entry
                     * by pointing idx->next to idxn->next.
                     * If the comparison fails, then we need to start the list traversal over again.
                     * Note: This should never happen, as once an item is in the list, this is the
                     * only path in which an in-list item has its next pointer mutated, and this
                     * occurs under a write lock, but we should be safe here
                     */
                    if (!CRYPTO_atomic_cmp_exch_ptr((void **)&idx->next,
                            (void **)&tmp, (void *)idxn->next,
                            sa->alock, &lock_failed)) {
                        if (lock_failed)
                            break;
                        /*
                         * The list was mutated while we were trying to mutate it
                         * Normally we would just use the reloaded value of tmp here to re-attempt
                         * the removal, but since idx was changed underneath us, we don't know where
                         * we are in the list anymore.  Its safer to just restart the whole traversal
                         */
                        goto restart_list;
                    }

                    if (!ossl_method_store_put_in_archive(sa, idxn))
                        break;

                    /*
                     * Idx just got a new next pointer above, so just update idxn, so we are sure that idx
                     * still isn't archived
                     */
                    if (!CRYPTO_atomic_load_ptr((void **)&idx->next, (void **)&idxn, sa->alock))
                        break;
                } else {
                    /*
                     * idxn wasn't archived, so we need to advance both pointers here
                     */
                    idx = idxn;
                    if (!CRYPTO_atomic_load_ptr((void **)&idx->next, (void **)&idxn, sa->alock))
                        break;
                }
            } else {
                /*
                 * idxn is NULL, that means we're at the end of the list.
                 * Just advance idx to idxn and the loop will break on the next iteration
                 */
                idx = idxn;
            }
        }
    }
}

static QUERY *ossl_method_store_atomic_find_in_list(STORED_ALGORITHMS *sa, int nid,
    OSSL_PROVIDER *prov, const char *prop_query)
{
    int nididx = (nid >> NUM_SHARDS_BITS) & (MAX_CACHE_LINES - 1);
    int archived;
    QUERY *idx;
    QUERY *ret = NULL;

    if (!CRYPTO_atomic_load_ptr((void **)&sa->cache_lists[nididx], (void **)&idx, sa->alock))
        goto out;

    while (idx != NULL) {
        if (!CRYPTO_atomic_load_int(&idx->archived, &archived, sa->alock))
            goto out;
        if (archived == 0 && idx->nid == nid && idx->prov == prov
            && (strcmp(idx->prop_query, prop_query) == 0)) {
            ret = idx;
            break;
        }
        if (!CRYPTO_atomic_load_ptr((void **)&idx->next, (void **)&idx, sa->alock))
            goto out;
    }
out:
    return ret;
}

static int ossl_method_store_atomic_insert_to_list(STORED_ALGORITHMS *sa, QUERY *new)
{
    int nid = (new->nid >> NUM_SHARDS_BITS) & (MAX_CACHE_LINES - 1);
    QUERY *headptr;
    int ret = 0;
    int lock_failed;

    if (!CRYPTO_atomic_load_ptr((void **)&sa->cache_lists[nid], (void **)&headptr, sa->alock))
        goto out;
try_again:
    if (!CRYPTO_atomic_store_ptr((void **)&new->next, (void **)&headptr, sa->alock))
        goto out;
    if (!CRYPTO_atomic_cmp_exch_ptr((void **)&sa->cache_lists[nid], (void **)&headptr, new, sa->alock,
            &lock_failed)) {
        if (lock_failed == 1)
            goto out;
        goto try_again;
    }
    ret = 1;
out:
    return ret;
}

static ossl_inline int ossl_method_store_cache_set_atomic(OSSL_METHOD_STORE *store, OSSL_PROVIDER *prov,
    int nid, const char *prop_query, STORED_ALGORITHMS *sa, void *method,
    int (*method_up_ref)(void *),
    void (*method_destruct)(void *))
{
    QUERY *p = NULL;
    int res = 1;

    if (method == NULL) {
        p = ossl_method_store_atomic_find_in_list(sa, nid, prov, prop_query);
        if (p != NULL)
            ossl_method_store_atomic_archive(sa, p);
        goto end;
    }

    p = ossl_method_store_atomic_find_in_list(sa, nid, prov, prop_query);
    if (p != NULL)
        ossl_method_store_atomic_archive(sa, p);

    p = QUERY_new(strlen(prop_query));
    if (p != NULL) {
        TSAN_BENIGN(p, "Unpublished value is safe on subsequent read");
        p->saptr = sa;
        p->nid = nid;
        p->prov = prov;
        p->archived = 0;
        strcpy(p->prop_query, prop_query);
        p->method.method = method;
        p->method.up_ref = method_up_ref;
        p->method.free = method_destruct;
        if (!ossl_method_up_ref(&p->method))
            goto err;

        if (!ossl_method_store_atomic_insert_to_list(sa, p)) {
            ossl_method_free(&p->method);
            goto err;
        }

        /*
         * We also want to add this method into the cache against a key computed
         * _only_ from nid and property query.  This lets us match in the event
         * someone does a lookup against a NULL provider (i.e. the "any provided
         * alg will do" match).
         *
         * Only insert it if no NULL-provider entry exists yet for this nid and
         * property query.  The first provider to cache this nid owns that
         * entry, which matches the provider ossl_method_store_fetch would pick
         * by implementation order.  Without this check, a later cache_set from
         * a different provider would overwrite it and change which provider an
         * "any provider" lookup resolves to.
         */
        if (ossl_method_store_atomic_find_in_list(sa, nid, NULL, prop_query) == NULL) {
            p = QUERY_new(strlen(prop_query));
            if (p == NULL)
                goto err;
            TSAN_BENIGN(p, "Unpublished value is safe on subsequent read");
            p->saptr = sa;
            p->nid = nid;
            p->prov = NULL;
            p->archived = 0;
            strcpy(p->prop_query, prop_query);
            p->method.method = method;
            p->method.up_ref = method_up_ref;
            p->method.free = method_destruct;
            if (!ossl_method_up_ref(&p->method))
                goto err;
            if (!ossl_method_store_atomic_insert_to_list(sa, p)) {
                ossl_method_free(&p->method);
                goto err;
            }
        }

        goto end;
    }
err:
    res = 0;
    QUERY_free(p);
end:
    return res;
}

int ossl_method_store_cache_set(OSSL_METHOD_STORE *store, OSSL_PROVIDER *prov,
    int nid, const char *prop_query, void *method,
    int (*method_up_ref)(void *),
    void (*method_destruct)(void *))
{
    STORED_ALGORITHMS *sa;
    int res = 1;

    if (nid <= 0 || store == NULL || prop_query == NULL)
        return 0;

    if (!ossl_assert(prov != NULL))
        return 0;

    sa = stored_algs_shard(store, nid);

    /*
     * Do an atomic insert into the appropriate cache linked list
     */
    res = ossl_method_store_cache_set_atomic(store, prov, nid, prop_query, sa, method,
        method_up_ref, method_destruct);

    return res;
}

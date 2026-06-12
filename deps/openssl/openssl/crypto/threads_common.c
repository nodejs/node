/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/**
 * @file
 * @brief Thread-local context-specific data management for OpenSSL
 *
 * This file implements a mechanism to store and retrieve context-specific
 * data using OpenSSL's thread-local storage (TLS) system. It provides a way
 * to associate and manage data based on a combination of a thread-local key
 * and an `OSSL_LIB_CTX *` context.
 *
 * NOTE: This differs from the CRYPTO_THREAD_[get|set]_local api set in that
 * this api stores a single OS level thread-local key per-process, and manages
 * subsequent keys using a series of arrays and sparse arrays stored against
 * that aforementioned thread local key
 *
 * Data Design:
 *
 * per-thread master key data  -> +--------------+-+
 *                                |              | |
 *                                |              | |
 *                                +--------------+ |
 *                                +--------------+ |
 *                                |              | |
 *                                |              | |
 *                                +--------------+ |  fixed  array indexed
 *                                       .         |      by key id
 *                                       .         |
 *                                       .         |
 *                                +--------------+ |
 *                                |              | |
 *                                |       |      | |
 *                                +-------+------+-+
 *                                        |
 *           ++---------------+           |
 *           ||               |<----------+
 *           ||               |
 *           |+---------------+
 *           |+---------------+
 *           ||               |
 *           ||               |  sparse array indexed
 *           |+---------------+     by libctx pointer
 *           |        .              cast to uintptr_t
 *           |        .
 *           |        .
 *           |+---------------+
 *           ||        +------+----> +-----------------+
 *           ||        |      |      |                 |
 *           ++--------+------+      +-----------------+
 *                                  per-<thread*ctx> data
 *
 * It uses the following lookup pattern:
 *   1) A global os defined key to a per-thread fixed array
 *   2) A libcrypto defined key id as an index to (1) to get a sparse array
 *   3) A Library context pointer as an index to (2) to produce a per
 *      thread*context data pointer
 *
 * Two primary functions are provided:
 *   - CRYPTO_THREAD_get_local_ex() retrieves data associated with a key and
 *     context.
 *   - CRYPTO_THREAD_set_local_ex() associates data with a given key and
 *     context, allocating tables as needed.
 *
 * Internal structures:
 *   - CTX_TABLE_ENTRY: wraps a context-specific data pointer.
 *   - MASTER_KEY_ENTRY: maintains a table of CTX_TABLE_ENTRY and an optional
 *     cleanup function.
 *
 * The implementation ensures:
 *   - Lazy initialization of master key data using CRYPTO_ONCE.
 *   - Automatic cleanup of all context and key mappings when a thread exits.
 *
 * Cleanup routines:
 *   - clean_ctx_entry: releases context-specific entries.
 *   - clean_master_key_id: releases all entries for a specific key ID.
 *   - clean_master_key: top-level cleanup for the thread-local master key.
 *
 */

#include <openssl/crypto.h>
#include <crypto/cryptlib.h>
#include <crypto/sparse_array.h>
#include "internal/cryptlib.h"
#include "internal/threads_common.h"

/**
 * @struct CTX_TABLE_ENTRY
 * @brief Represents a wrapper for context-specific data.
 *
 * This structure is used to hold a pointer to data that is associated
 * with a particular `OSSL_LIB_CTX` instance in a thread-local context
 * mapping. It is stored within a sparse array, allowing efficient
 * per-context data lookup keyed by a context identifier.
 *
 * @var CTX_TABLE_ENTRY::ctx_data
 * Pointer to the data associated with a given library context.
 */
typedef void *CTX_TABLE_ENTRY;

/*
 * define our sparse array of CTX_TABLE_ENTRY functions
 */
DEFINE_SPARSE_ARRAY_OF(CTX_TABLE_ENTRY);

/**
 * @struct MASTER_KEY_ENTRY
 * @brief Represents a mapping of context-specific data for a TLS key ID.
 *
 * This structure manages a collection of `CTX_TABLE_ENTRY` items, each
 * associated with a different `OSSL_LIB_CTX` instance. It supports
 * cleanup of stored data when the thread or key is being destroyed.
 *
 * @var MASTER_KEY_ENTRY::ctx_table
 * Sparse array mapping `OSSL_LIB_CTX` pointers (cast to uintptr_t) to
 * `CTX_TABLE_ENTRY` structures that hold context-specific data.
 *
 */
typedef struct master_key_entry {
    SPARSE_ARRAY_OF(CTX_TABLE_ENTRY) *ctx_table;
} MASTER_KEY_ENTRY;

/**
 * @brief holds our per thread data with the operating system
 *
 * Global thread local storage pointer, used to create a platform
 * specific thread-local key
 */
static CRYPTO_THREAD_LOCAL master_key;

/**
 * @brief Informs the library if the master key has been set up
 *
 * State variable to track if we have initialized the master_key
 * If this isn't set to 1, then we need to skip any cleanup
 * in CRYPTO_THREAD_clean_for_fips, as the uninitialized key
 * will return garbage data
 */
static uint8_t master_key_init = 0;

/**
 * @brief gate variable to do one time init of the master key
 *
 * Run once gate for doing one-time initialization
 */
static CRYPTO_ONCE master_once = CRYPTO_ONCE_STATIC_INIT;

/**
 * @brief Cleans up all context-specific entries for a given key ID.
 *
 * This function is used to release all context data associated with a
 * specific thread-local key (identified by `idx`). It iterates over the
 * context table in the given `MASTER_KEY_ENTRY`, invoking cleanup for each
 * `CTX_TABLE_ENTRY`, then frees the context table and the entry itself.
 *
 * @param idx
 *        The key ID associated with the `MASTER_KEY_ENTRY`. Unused.
 *
 * @param entry
 *        Pointer to the `MASTER_KEY_ENTRY` containing the context table
 *        to be cleaned up.
 *
 * @param arg
 *        Unused parameter.
 */
static void clean_master_key_id(MASTER_KEY_ENTRY *entry)
{
    ossl_sa_CTX_TABLE_ENTRY_free(entry->ctx_table);
}

/**
 * @brief Cleans up all master key entries for the current thread.
 *
 * This function is the top-level cleanup routine for the thread-local
 * storage associated with OpenSSL master keys. It is typically registered
 * as the thread-local storage destructor. It iterates over all
 * `MASTER_KEY_ENTRY` items in the sparse array, releasing associated
 * context data and structures.
 *
 * @param data
 *        Pointer to the thread-local `SPARSE_ARRAY_OF(MASTER_KEY_ENTRY)`
 *        structure to be cleaned up.
 */
static void clean_master_key(void *data)
{
    MASTER_KEY_ENTRY *mkey = data;
    int i;

    if (data == NULL)
        return;

    for (i = 0; i < CRYPTO_THREAD_LOCAL_KEY_MAX; i++) {
        if (mkey[i].ctx_table != NULL)
            clean_master_key_id(&mkey[i]);
    }
    OPENSSL_free(mkey);
}

/**
 * @brief Initializes the thread-local storage for master key data.
 *
 * This function sets up the thread-local key used to store per-thread
 * master key tables. It also registers the `clean_master_key` function
 * as the destructor to be called when the thread exits.
 *
 * This function is intended to be called once using `CRYPTO_THREAD_run_once`
 * to ensure thread-safe initialization.
 */
static void init_master_key(void)
{
    /*
     * Note: We assign a cleanup function here, which is atypical for
     * uses of CRYPTO_THREAD_init_local.  This is because, nominally
     * we expect that the use of ossl_init_thread_start will be used
     * to notify openssl of exiting threads.  However, in this case
     * we want the metadata for this interface (the sparse arrays) to
     * stay valid until the thread actually exits, which is what the
     * clean_master_key function does.  Data held in the sparse arrays
     * (that is assigned via CRYPTO_THREAD_set_local_ex), are still expected
     * to be cleaned via the ossl_init_thread_start/stop api.
     */
    if (!CRYPTO_THREAD_init_local(&master_key, clean_master_key))
        return;

    /*
     * Indicate that the key has been set up.
     */
    master_key_init = 1;
}

/**
 * @brief Retrieves context-specific data from thread-local storage.
 *
 * This function looks up and returns the data associated with a given
 * thread-local key ID and `OSSL_LIB_CTX` context. The data must have
 * previously been stored using `CRYPTO_THREAD_set_local_ex()`.
 *
 * If the master key table is not yet initialized, it will be lazily
 * initialized via `init_master_key()`. If the requested key or context
 * entry does not exist, `NULL` is returned.
 *
 * @param id
 *        The thread-local key ID used to identify the master key entry.
 *
 * @param ctx
 *        Pointer to the `OSSL_LIB_CTX` used to index into the context
 *        table for the specified key.
 *
 * @return A pointer to the stored context-specific data, or NULL if no
 *         entry is found or initialization fails.
 */
void *CRYPTO_THREAD_get_local_ex(CRYPTO_THREAD_LOCAL_KEY_ID id, OSSL_LIB_CTX *ctx)
{
    MASTER_KEY_ENTRY *mkey;
    CTX_TABLE_ENTRY ctxd;

    ctx = (ctx == CRYPTO_THREAD_NO_CONTEXT) ? NULL : ossl_lib_ctx_get_concrete(ctx);
    /*
     * Make sure the master key has been initialized
     * NOTE: We use CRYPTO_THREAD_run_once here, rather than the
     * RUN_ONCE macros.  We do this because this code is included both in
     * libcrypto, and in fips.[dll|dylib|so].  FIPS attempts to avoid doing
     * one time initialization of global data, and so suppresses the definition
     * of RUN_ONCE, etc, meaning the build breaks if we were to use that with
     * fips-enabled.  However, this is a special case in which we want/need
     * this one bit of global data to be initialized in both the fips provider
     * and in libcrypto, so we use CRYPTO_THREAD_run_one directly, which is
     * always defined.
     */
    if (!CRYPTO_THREAD_run_once(&master_once, init_master_key))
        return NULL;

    if (!ossl_assert(id < CRYPTO_THREAD_LOCAL_KEY_MAX))
        return NULL;

    /*
     * Get our master table sparse array, indexed by key id
     */
    mkey = CRYPTO_THREAD_get_local(&master_key);
    if (mkey == NULL)
        return NULL;

    /*
     * Get the specific data entry in the master key
     * table for the key id we are searching for
     */
    if (mkey[id].ctx_table == NULL)
        return NULL;

    /*
     * If we find an entry above, that will be a sparse array,
     * indexed by OSSL_LIB_CTX.
     * Note: Because we're using sparse arrays here, we can do an easy
     * trick, since we know all OSSL_LIB_CTX pointers are unique.  By casting
     * the pointer to a unitptr_t, we can use that as an ordinal index into
     * the sparse array.
     */
    ctxd = ossl_sa_CTX_TABLE_ENTRY_get(mkey[id].ctx_table, (uintptr_t)ctx);

    /*
     * If we find an entry for the passed in context, return its data pointer
     */
    return ctxd;
}

/**
 * @brief Associates context-specific data with a thread-local key.
 *
 * This function stores a pointer to data associated with a specific
 * thread-local key ID and `OSSL_LIB_CTX` context. It ensures that the
 * internal thread-local master key table and all necessary sparse array
 * structures are initialized and allocated as needed.
 *
 * If the key or context-specific entry does not already exist, they will
 * be created. This function allows each thread to maintain separate data
 * for different library contexts under a shared key identifier.
 *
 * @param id
 *        The thread-local key ID to associate the data with.
 *
 * @param ctx
 *        Pointer to the `OSSL_LIB_CTX` used as a secondary key for storing
 *        the data.
 *
 * @param data
 *        Pointer to the user-defined context-specific data to store.
 *
 * @return 1 on success, or 0 if allocation or initialization fails.
 */
int CRYPTO_THREAD_set_local_ex(CRYPTO_THREAD_LOCAL_KEY_ID id,
                               OSSL_LIB_CTX *ctx, void *data)
{
    MASTER_KEY_ENTRY *mkey;

    ctx = (ctx == CRYPTO_THREAD_NO_CONTEXT) ? NULL : ossl_lib_ctx_get_concrete(ctx);
    /*
     * Make sure our master key is initialized
     * See notes above on the use of CRYPTO_THREAD_run_once here
     */
    if (!CRYPTO_THREAD_run_once(&master_once, init_master_key))
        return 0;

    if (!ossl_assert(id < CRYPTO_THREAD_LOCAL_KEY_MAX))
        return 0;

    /*
     * Get our local master key data, which will be
     * a sparse array indexed by the id parameter
     */
    mkey = CRYPTO_THREAD_get_local(&master_key);
    if (mkey == NULL) {
        /*
         * we didn't find one, but that's ok, just initialize it now
         */
        mkey = OPENSSL_calloc(CRYPTO_THREAD_LOCAL_KEY_MAX,
                              sizeof(MASTER_KEY_ENTRY));
        if (mkey == NULL)
            return 0;
        /*
         * make sure to assign it to our master key thread-local storage
         */
        if (!CRYPTO_THREAD_set_local(&master_key, mkey)) {
            OPENSSL_free(mkey);
            return 0;
        }
    }

    /*
     * Find the entry that we are looking for using our id index
     */
    if (mkey[id].ctx_table == NULL) {

        /*
         * Didn't find it, that's ok, just add it now
         */
        mkey[id].ctx_table = ossl_sa_CTX_TABLE_ENTRY_new();
        if (mkey[id].ctx_table == NULL)
            return 0;
    }

    /*
     * Now go look up our per context entry, using the OSSL_LIB_CTX pointer
     * that we've been provided.  Note we cast the pointer to a uintptr_t so
     * as to use it as an index in the sparse array
     *
     * Assign to the entry in the table so that we can find it later
     */
    return ossl_sa_CTX_TABLE_ENTRY_set(mkey[id].ctx_table,
                                       (uintptr_t)ctx, data);
}

void CRYPTO_THREAD_clean_local(void)
{
    MASTER_KEY_ENTRY *mkey;

    /*
     * If we never initialized the master key, there
     * is no data to clean, so we are done here
     */
    if (master_key_init == 0)
        return;

    mkey = CRYPTO_THREAD_get_local(&master_key);
    if (mkey != NULL) {
        clean_master_key(mkey);
        CRYPTO_THREAD_set_local(&master_key, NULL);
    }
}

/*
 * Copyright 2016-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <internal/cryptlib_int.h>
#include <openssl/err.h>
#include <internal/rand.h>
#include <internal/bio.h>
#include <openssl/evp.h>
#include <internal/evp_int.h>
#include <internal/conf.h>
#include <internal/async.h>
#include <internal/engine.h>
#include <internal/comp.h>
#include <internal/err.h>
#include <internal/err_int.h>
#include <internal/objects.h>
#include <stdlib.h>
#include <assert.h>
#include <internal/thread_once.h>
#include <internal/dso.h>

static int stopped = 0;

static void ossl_init_thread_stop(struct thread_local_inits_st *locals);

static CRYPTO_THREAD_LOCAL threadstopkey;

static void ossl_init_thread_stop_wrap(void *local)
{
    ossl_init_thread_stop((struct thread_local_inits_st *)local);
}

static struct thread_local_inits_st *ossl_init_get_thread_local(int alloc)
{
    struct thread_local_inits_st *local =
        CRYPTO_THREAD_get_local(&threadstopkey);

    if (local == NULL && alloc) {
        local = OPENSSL_zalloc(sizeof(*local));
        if (local != NULL && !CRYPTO_THREAD_set_local(&threadstopkey, local)) {
            OPENSSL_free(local);
            return NULL;
        }
    }
    if (!alloc) {
        CRYPTO_THREAD_set_local(&threadstopkey, NULL);
    }

    return local;
}

typedef struct ossl_init_stop_st OPENSSL_INIT_STOP;
struct ossl_init_stop_st {
    void (*handler)(void);
    OPENSSL_INIT_STOP *next;
};

static OPENSSL_INIT_STOP *stop_handlers = NULL;
static CRYPTO_RWLOCK *init_lock = NULL;

static CRYPTO_ONCE base = CRYPTO_ONCE_STATIC_INIT;
static int base_inited = 0;
DEFINE_RUN_ONCE_STATIC(ossl_init_base)
{
#ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_base: Setting up stop handlers\n");
#endif
    /*
     * We use a dummy thread local key here. We use the destructor to detect
     * when the thread is going to stop (where that feature is available)
     */
    CRYPTO_THREAD_init_local(&threadstopkey, ossl_init_thread_stop_wrap);
#ifndef OPENSSL_SYS_UEFI
    atexit(OPENSSL_cleanup);
#endif
    if ((init_lock = CRYPTO_THREAD_lock_new()) == NULL)
        return 0;
    OPENSSL_cpuid_setup();

    /*
     * BIG FAT WARNING!
     * Everything needed to be initialized in this function before threads
     * come along MUST happen before base_inited is set to 1, or we will
     * see race conditions.
     */
    base_inited = 1;

#if !defined(OPENSSL_NO_DSO) && !defined(OPENSSL_USE_NODELETE)
# ifdef DSO_WIN32
    {
        HMODULE handle = NULL;
        BOOL ret;

        /* We don't use the DSO route for WIN32 because there is a better way */
        ret = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                (void *)&base_inited, &handle);

        return (ret == TRUE) ? 1 : 0;
    }
# else
    /*
     * Deliberately leak a reference to ourselves. This will force the library
     * to remain loaded until the atexit() handler is run at process exit.
     */
    {
        DSO *dso = NULL;

        ERR_set_mark();
        dso = DSO_dsobyaddr(&base_inited, DSO_FLAG_NO_UNLOAD_ON_FREE);
        DSO_free(dso);
        ERR_pop_to_mark();
    }
# endif
#endif

    return 1;
}

static CRYPTO_ONCE load_crypto_strings = CRYPTO_ONCE_STATIC_INIT;
static int load_crypto_strings_inited = 0;
DEFINE_RUN_ONCE_STATIC(ossl_init_no_load_crypto_strings)
{
    /* Do nothing in this case */
    return 1;
}

DEFINE_RUN_ONCE_STATIC(ossl_init_load_crypto_strings)
{
    int ret = 1;
    /*
     * OPENSSL_NO_AUTOERRINIT is provided here to prevent at compile time
     * pulling in all the error strings during static linking
     */
#if !defined(OPENSSL_NO_ERR) && !defined(OPENSSL_NO_AUTOERRINIT)
# ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_load_crypto_strings: "
                    "err_load_crypto_strings_int()\n");
# endif
    ret = err_load_crypto_strings_int();
    load_crypto_strings_inited = 1;
#endif    
    return ret;
}

static CRYPTO_ONCE add_all_ciphers = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_add_all_ciphers)
{
    /*
     * OPENSSL_NO_AUTOALGINIT is provided here to prevent at compile time
     * pulling in all the ciphers during static linking
     */
#ifndef OPENSSL_NO_AUTOALGINIT
# ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_add_all_ciphers: "
                    "openssl_add_all_ciphers_int()\n");
# endif
    openssl_add_all_ciphers_int();
#endif
    return 1;
}

static CRYPTO_ONCE add_all_digests = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_add_all_digests)
{
    /*
     * OPENSSL_NO_AUTOALGINIT is provided here to prevent at compile time
     * pulling in all the ciphers during static linking
     */
#ifndef OPENSSL_NO_AUTOALGINIT
# ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_add_all_digests: "
                    "openssl_add_all_digests()\n");
# endif
    openssl_add_all_digests_int();
#endif
    return 1;
}

DEFINE_RUN_ONCE_STATIC(ossl_init_no_add_algs)
{
    /* Do nothing */
    return 1;
}

static CRYPTO_ONCE config = CRYPTO_ONCE_STATIC_INIT;
static int config_inited = 0;
static const char *appname;
DEFINE_RUN_ONCE_STATIC(ossl_init_config)
{
#ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr,
            "OPENSSL_INIT: ossl_init_config: openssl_config(%s)\n",
            appname == NULL ? "NULL" : appname);
#endif
    openssl_config_int(appname);
    config_inited = 1;
    return 1;
}
DEFINE_RUN_ONCE_STATIC(ossl_init_no_config)
{
#ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr,
            "OPENSSL_INIT: ossl_init_config: openssl_no_config_int()\n");
#endif
    openssl_no_config_int();
    config_inited = 1;
    return 1;
}

static CRYPTO_ONCE async = CRYPTO_ONCE_STATIC_INIT;
static int async_inited = 0;
DEFINE_RUN_ONCE_STATIC(ossl_init_async)
{
#ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_async: async_init()\n");
#endif
    if (!async_init())
        return 0;
    async_inited = 1;
    return 1;
}

#ifndef OPENSSL_NO_ENGINE
static CRYPTO_ONCE engine_openssl = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_engine_openssl)
{
# ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_engine_openssl: "
                    "engine_load_openssl_int()\n");
# endif
    engine_load_openssl_int();
    return 1;
}
# if !defined(OPENSSL_NO_HW) && \
    (defined(__OpenBSD__) || defined(__FreeBSD__) || defined(HAVE_CRYPTODEV))
static CRYPTO_ONCE engine_cryptodev = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_engine_cryptodev)
{
#  ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_engine_cryptodev: "
                    "engine_load_cryptodev_int()\n");
#  endif
    engine_load_cryptodev_int();
    return 1;
}
# endif

# ifndef OPENSSL_NO_RDRAND
static CRYPTO_ONCE engine_rdrand = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_engine_rdrand)
{
#  ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_engine_rdrand: "
                    "engine_load_rdrand_int()\n");
#  endif
    engine_load_rdrand_int();
    return 1;
}
# endif
static CRYPTO_ONCE engine_dynamic = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_engine_dynamic)
{
# ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_engine_dynamic: "
                    "engine_load_dynamic_int()\n");
# endif
    engine_load_dynamic_int();
    return 1;
}
# ifndef OPENSSL_NO_STATIC_ENGINE
#  if !defined(OPENSSL_NO_HW) && !defined(OPENSSL_NO_HW_PADLOCK)
static CRYPTO_ONCE engine_padlock = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_engine_padlock)
{
#   ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_engine_padlock: "
                    "engine_load_padlock_int()\n");
#   endif
    engine_load_padlock_int();
    return 1;
}
#  endif
#  if defined(OPENSSL_SYS_WIN32) && !defined(OPENSSL_NO_CAPIENG)
static CRYPTO_ONCE engine_capi = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_engine_capi)
{
#   ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_engine_capi: "
                    "engine_load_capi_int()\n");
#   endif
    engine_load_capi_int();
    return 1;
}
#  endif
#  if !defined(OPENSSL_NO_AFALGENG)
static CRYPTO_ONCE engine_afalg = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_engine_afalg)
{
#   ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: ossl_init_engine_afalg: "
                    "engine_load_afalg_int()\n");
#   endif
    engine_load_afalg_int();
    return 1;
}
#  endif
# endif
#endif

#ifndef OPENSSL_NO_COMP
static CRYPTO_ONCE zlib = CRYPTO_ONCE_STATIC_INIT;

static int zlib_inited = 0;
DEFINE_RUN_ONCE_STATIC(ossl_init_zlib)
{
    /* Do nothing - we need to know about this for the later cleanup */
    zlib_inited = 1;
    return 1;
}
#endif

static void ossl_init_thread_stop(struct thread_local_inits_st *locals)
{
    /* Can't do much about this */
    if (locals == NULL)
        return;

    if (locals->async) {
#ifdef OPENSSL_INIT_DEBUG
        fprintf(stderr, "OPENSSL_INIT: ossl_init_thread_stop: "
                        "ASYNC_cleanup_thread()\n");
#endif
        ASYNC_cleanup_thread();
    }

    if (locals->err_state) {
#ifdef OPENSSL_INIT_DEBUG
        fprintf(stderr, "OPENSSL_INIT: ossl_init_thread_stop: "
                        "err_delete_thread_state()\n");
#endif
        err_delete_thread_state();
    }

    OPENSSL_free(locals);
}

void OPENSSL_thread_stop(void)
{
    ossl_init_thread_stop(
        (struct thread_local_inits_st *)ossl_init_get_thread_local(0));
}

int ossl_init_thread_start(uint64_t opts)
{
    struct thread_local_inits_st *locals;

    if (!OPENSSL_init_crypto(0, NULL))
        return 0;

    locals = ossl_init_get_thread_local(1);

    if (locals == NULL)
        return 0;

    if (opts & OPENSSL_INIT_THREAD_ASYNC) {
#ifdef OPENSSL_INIT_DEBUG
        fprintf(stderr, "OPENSSL_INIT: ossl_init_thread_start: "
                        "marking thread for async\n");
#endif
        locals->async = 1;
    }

    if (opts & OPENSSL_INIT_THREAD_ERR_STATE) {
#ifdef OPENSSL_INIT_DEBUG
        fprintf(stderr, "OPENSSL_INIT: ossl_init_thread_start: "
                        "marking thread for err_state\n");
#endif
        locals->err_state = 1;
    }

    return 1;
}

void OPENSSL_cleanup(void)
{
    OPENSSL_INIT_STOP *currhandler, *lasthandler;

    /* If we've not been inited then no need to deinit */
    if (!base_inited)
        return;

    /* Might be explicitly called and also by atexit */
    if (stopped)
        return;
    stopped = 1;

    /*
     * Thread stop may not get automatically called by the thread library for
     * the very last thread in some situations, so call it directly.
     */
    ossl_init_thread_stop(ossl_init_get_thread_local(0));

    currhandler = stop_handlers;
    while (currhandler != NULL) {
        currhandler->handler();
        lasthandler = currhandler;
        currhandler = currhandler->next;
        OPENSSL_free(lasthandler);
    }
    stop_handlers = NULL;

    CRYPTO_THREAD_lock_free(init_lock);

    /*
     * We assume we are single-threaded for this function, i.e. no race
     * conditions for the various "*_inited" vars below.
     */

#ifndef OPENSSL_NO_COMP
    if (zlib_inited) {
#ifdef OPENSSL_INIT_DEBUG
        fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                        "comp_zlib_cleanup_int()\n");
#endif
        comp_zlib_cleanup_int();
    }
#endif

    if (async_inited) {
# ifdef OPENSSL_INIT_DEBUG
        fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                        "async_deinit()\n");
# endif
        async_deinit();
    }

    if (load_crypto_strings_inited) {
#ifdef OPENSSL_INIT_DEBUG
        fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                        "err_free_strings_int()\n");
#endif
        err_free_strings_int();
    }

    CRYPTO_THREAD_cleanup_local(&threadstopkey);

#ifdef OPENSSL_INIT_DEBUG
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "rand_cleanup_int()\n");
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "conf_modules_free_int()\n");
#ifndef OPENSSL_NO_ENGINE
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "engine_cleanup_int()\n");
#endif
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "crypto_cleanup_all_ex_data_int()\n");
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "bio_sock_cleanup_int()\n");
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "bio_cleanup()\n");
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "evp_cleanup_int()\n");
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "obj_cleanup_int()\n");
    fprintf(stderr, "OPENSSL_INIT: OPENSSL_cleanup: "
                    "err_cleanup()\n");
#endif
    /*
     * Note that cleanup order is important:
     * - rand_cleanup_int could call an ENGINE's RAND cleanup function so
     * must be called before engine_cleanup_int()
     * - ENGINEs use CRYPTO_EX_DATA and therefore, must be cleaned up
     * before the ex data handlers are wiped in CRYPTO_cleanup_all_ex_data().
     * - conf_modules_free_int() can end up in ENGINE code so must be called
     * before engine_cleanup_int()
     * - ENGINEs and additional EVP algorithms might use added OIDs names so
     * obj_cleanup_int() must be called last
     */
    rand_cleanup_int();
    conf_modules_free_int();
#ifndef OPENSSL_NO_ENGINE
    engine_cleanup_int();
#endif
    crypto_cleanup_all_ex_data_int();
    bio_cleanup();
    evp_cleanup_int();
    obj_cleanup_int();
    err_cleanup();

    base_inited = 0;
}

/*
 * If this function is called with a non NULL settings value then it must be
 * called prior to any threads making calls to any OpenSSL functions,
 * i.e. passing a non-null settings value is assumed to be single-threaded.
 */
int OPENSSL_init_crypto(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings)
{
    static int stoperrset = 0;

    if (stopped) {
        if (!stoperrset) {
            /*
             * We only ever set this once to avoid getting into an infinite
             * loop where the error system keeps trying to init and fails so
             * sets an error etc
             */
            stoperrset = 1;
            CRYPTOerr(CRYPTO_F_OPENSSL_INIT_CRYPTO, ERR_R_INIT_FAIL);
        }
        return 0;
    }

    if (!base_inited && !RUN_ONCE(&base, ossl_init_base))
        return 0;

    if ((opts & OPENSSL_INIT_NO_LOAD_CRYPTO_STRINGS)
            && !RUN_ONCE(&load_crypto_strings,
                         ossl_init_no_load_crypto_strings))
        return 0;

    if ((opts & OPENSSL_INIT_LOAD_CRYPTO_STRINGS)
            && !RUN_ONCE(&load_crypto_strings, ossl_init_load_crypto_strings))
        return 0;

    if ((opts & OPENSSL_INIT_NO_ADD_ALL_CIPHERS)
            && !RUN_ONCE(&add_all_ciphers, ossl_init_no_add_algs))
        return 0;

    if ((opts & OPENSSL_INIT_ADD_ALL_CIPHERS)
            && !RUN_ONCE(&add_all_ciphers, ossl_init_add_all_ciphers))
        return 0;

    if ((opts & OPENSSL_INIT_NO_ADD_ALL_DIGESTS)
            && !RUN_ONCE(&add_all_digests, ossl_init_no_add_algs))
        return 0;

    if ((opts & OPENSSL_INIT_ADD_ALL_DIGESTS)
            && !RUN_ONCE(&add_all_digests, ossl_init_add_all_digests))
        return 0;

    if ((opts & OPENSSL_INIT_NO_LOAD_CONFIG)
            && !RUN_ONCE(&config, ossl_init_no_config))
        return 0;

    if (opts & OPENSSL_INIT_LOAD_CONFIG) {
        int ret;
        CRYPTO_THREAD_write_lock(init_lock);
        appname = (settings == NULL) ? NULL : settings->appname;
        ret = RUN_ONCE(&config, ossl_init_config);
        CRYPTO_THREAD_unlock(init_lock);
        if (!ret)
            return 0;
    }

    if ((opts & OPENSSL_INIT_ASYNC)
            && !RUN_ONCE(&async, ossl_init_async))
        return 0;

#ifndef OPENSSL_NO_ENGINE
    if ((opts & OPENSSL_INIT_ENGINE_OPENSSL)
            && !RUN_ONCE(&engine_openssl, ossl_init_engine_openssl))
        return 0;
# if !defined(OPENSSL_NO_HW) && \
    (defined(__OpenBSD__) || defined(__FreeBSD__) || defined(HAVE_CRYPTODEV))
    if ((opts & OPENSSL_INIT_ENGINE_CRYPTODEV)
            && !RUN_ONCE(&engine_cryptodev, ossl_init_engine_cryptodev))
        return 0;
# endif
# ifndef OPENSSL_NO_RDRAND
    if ((opts & OPENSSL_INIT_ENGINE_RDRAND)
            && !RUN_ONCE(&engine_rdrand, ossl_init_engine_rdrand))
        return 0;
# endif
    if ((opts & OPENSSL_INIT_ENGINE_DYNAMIC)
            && !RUN_ONCE(&engine_dynamic, ossl_init_engine_dynamic))
        return 0;
# ifndef OPENSSL_NO_STATIC_ENGINE
#  if !defined(OPENSSL_NO_HW) && !defined(OPENSSL_NO_HW_PADLOCK)
    if ((opts & OPENSSL_INIT_ENGINE_PADLOCK)
            && !RUN_ONCE(&engine_padlock, ossl_init_engine_padlock))
        return 0;
#  endif
#  if defined(OPENSSL_SYS_WIN32) && !defined(OPENSSL_NO_CAPIENG)
    if ((opts & OPENSSL_INIT_ENGINE_CAPI)
            && !RUN_ONCE(&engine_capi, ossl_init_engine_capi))
        return 0;
#  endif
#  if !defined(OPENSSL_NO_AFALGENG)
    if ((opts & OPENSSL_INIT_ENGINE_AFALG)
            && !RUN_ONCE(&engine_afalg, ossl_init_engine_afalg))
        return 0;
#  endif
# endif
    if (opts & (OPENSSL_INIT_ENGINE_ALL_BUILTIN
                | OPENSSL_INIT_ENGINE_OPENSSL
                | OPENSSL_INIT_ENGINE_AFALG)) {
        ENGINE_register_all_complete();
    }
#endif

#ifndef OPENSSL_NO_COMP
    if ((opts & OPENSSL_INIT_ZLIB)
            && !RUN_ONCE(&zlib, ossl_init_zlib))
        return 0;
#endif

    return 1;
}

int OPENSSL_atexit(void (*handler)(void))
{
    OPENSSL_INIT_STOP *newhand;

#if !defined(OPENSSL_NO_DSO) && !defined(OPENSSL_USE_NODELETE)
    {
        union {
            void *sym;
            void (*func)(void);
        } handlersym;

        handlersym.func = handler;
# ifdef DSO_WIN32
        {
            HMODULE handle = NULL;
            BOOL ret;

            /*
             * We don't use the DSO route for WIN32 because there is a better
             * way
             */
            ret = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                    | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                    handlersym.sym, &handle);

            if (!ret)
                return 0;
        }
# else
        /*
         * Deliberately leak a reference to the handler. This will force the
         * library/code containing the handler to remain loaded until we run the
         * atexit handler. If -znodelete has been used then this is
         * unnecessary.
         */
        {
            DSO *dso = NULL;

            ERR_set_mark();
            dso = DSO_dsobyaddr(handlersym.sym, DSO_FLAG_NO_UNLOAD_ON_FREE);
            DSO_free(dso);
            ERR_pop_to_mark();
        }
# endif
    }
#endif

    newhand = OPENSSL_malloc(sizeof(*newhand));
    if (newhand == NULL)
        return 0;

    newhand->handler = handler;
    newhand->next = stop_handlers;
    stop_handlers = newhand;

    return 1;
}

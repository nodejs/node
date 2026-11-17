/*
 * Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/e_os.h"
#include "crypto/cryptlib.h"
#include <openssl/err.h>
#include "crypto/rand.h"
#include "internal/bio.h"
#include <openssl/evp.h>
#include "crypto/evp.h"
#include "internal/conf.h"
#include "crypto/async.h"
#include "internal/comp.h"
#include "internal/err.h"
#include "crypto/err.h"
#include "crypto/objects.h"
#include <stdlib.h>
#include <assert.h>
#include "internal/thread_once.h"
#include "crypto/dso_conf.h"
#include "internal/dso.h"
#include "crypto/store.h"
#include <openssl/cmp_util.h> /* for OSSL_CMP_log_close() */
#include <openssl/trace.h>
#include <openssl/ssl.h> /* for OPENSSL_INIT_(NO_)?LOAD_SSL_STRINGS */
#include "crypto/bn.h"
#include "crypto/ctype.h"
#include "sslerr.h"

#ifdef S390X_MOD_EXP
#include "arch/s390x_arch.h"
#endif

static int stopped = 0;
static uint64_t optsdone = 0;

/* Guards access to the optsdone variable on platforms without atomics */
static CRYPTO_RWLOCK *optsdone_lock = NULL;
/* Guards simultaneous INIT_LOAD_CONFIG calls with non-NULL settings */
static CRYPTO_RWLOCK *init_lock = NULL;
static CRYPTO_THREAD_LOCAL in_init_config_local;

static CRYPTO_ONCE base = CRYPTO_ONCE_STATIC_INIT;
static int base_inited = 0;
static int do_global_cleanup = 0;
DEFINE_RUN_ONCE_STATIC(ossl_init_base)
{
    /* no need to init trace */

    OSSL_TRACE(INIT, "ossl_init_base: setting up stop handlers\n");
#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    ossl_malloc_setup_failures();
#endif

    if ((optsdone_lock = CRYPTO_THREAD_lock_new()) == NULL
        || (init_lock = CRYPTO_THREAD_lock_new()) == NULL)
        goto err;

    OPENSSL_cpuid_setup();

    if (!ossl_init_thread())
        goto err;

    if (!CRYPTO_THREAD_init_local(&in_init_config_local, NULL))
        goto err;

    base_inited = 1;
    return 1;

err:
    OSSL_TRACE(INIT, "ossl_init_base failed!\n");
    CRYPTO_THREAD_lock_free(optsdone_lock);
    optsdone_lock = NULL;
    CRYPTO_THREAD_lock_free(init_lock);
    init_lock = NULL;

    return 0;
}

static CRYPTO_ONCE load_crypto_strings = CRYPTO_ONCE_STATIC_INIT;

DEFINE_RUN_ONCE_STATIC(ossl_init_load_crypto_strings)
{
    int ret = 1;
    /*
     * OPENSSL_NO_AUTOERRINIT is provided here to prevent at compile time
     * pulling in all the error strings during static linking
     */
#if !defined(OPENSSL_NO_ERR) && !defined(OPENSSL_NO_AUTOERRINIT)
    void *err;

    if (!err_shelve_state(&err))
        return 0;

    OSSL_TRACE(INIT, "ossl_err_load_crypto_strings()\n");
    ret = ossl_err_load_crypto_strings();

    err_unshelve_state(err);
#endif
    return ret;
}

DEFINE_RUN_ONCE_STATIC_ALT(ossl_init_no_load_crypto_strings,
    ossl_init_load_crypto_strings)
{
    /* Do nothing in this case */
    return 1;
}

static CRYPTO_ONCE ssl_strings = CRYPTO_ONCE_STATIC_INIT;

DEFINE_RUN_ONCE_STATIC(ossl_init_load_ssl_strings)
{
    /*
     * OPENSSL_NO_AUTOERRINIT is provided here to prevent at compile time
     * pulling in all the error strings during static linking
     */
#if !defined(OPENSSL_NO_ERR) && !defined(OPENSSL_NO_AUTOERRINIT)
    OSSL_TRACE(INIT, "ossl_init_load_ssl_strings: ossl_err_load_SSL_strings()\n");
    ossl_err_load_SSL_strings();
#endif
    return 1;
}

DEFINE_RUN_ONCE_STATIC_ALT(ossl_init_no_load_ssl_strings,
    ossl_init_load_ssl_strings)
{
    /* Do nothing in this case */
    return 1;
}

static CRYPTO_ONCE add_all_ciphers = CRYPTO_ONCE_STATIC_INIT;
DEFINE_RUN_ONCE_STATIC(ossl_init_add_all_ciphers)
{
    /*
     * OPENSSL_NO_AUTOALGINIT is provided here to prevent at compile time
     * pulling in all the ciphers during static linking
     */
#ifndef OPENSSL_NO_AUTOALGINIT
    OSSL_TRACE(INIT, "openssl_add_all_ciphers_int()\n");
    openssl_add_all_ciphers_int();
#endif
    return 1;
}

DEFINE_RUN_ONCE_STATIC_ALT(ossl_init_no_add_all_ciphers,
    ossl_init_add_all_ciphers)
{
    /* Do nothing */
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
    OSSL_TRACE(INIT, "openssl_add_all_digests()\n");
    openssl_add_all_digests_int();
#endif
    return 1;
}

DEFINE_RUN_ONCE_STATIC_ALT(ossl_init_no_add_all_digests,
    ossl_init_add_all_digests)
{
    /* Do nothing */
    return 1;
}

static CRYPTO_ONCE config = CRYPTO_ONCE_STATIC_INIT;
static const OPENSSL_INIT_SETTINGS *conf_settings = NULL;
DEFINE_RUN_ONCE_STATIC(ossl_init_config)
{
    int ret = ossl_config_int(NULL);

    return ret;
}
DEFINE_RUN_ONCE_STATIC_ALT(ossl_init_config_settings, ossl_init_config)
{
    int ret = ossl_config_int(conf_settings);

    return ret;
}
DEFINE_RUN_ONCE_STATIC_ALT(ossl_init_no_config, ossl_init_config)
{
    OSSL_TRACE(INIT, "ossl_no_config_int()\n");
    ossl_no_config_int();

    return 1;
}

static CRYPTO_ONCE async = CRYPTO_ONCE_STATIC_INIT;
static int async_inited = 0;
DEFINE_RUN_ONCE_STATIC(ossl_init_async)
{
    OSSL_TRACE(INIT, "async_init()\n");
    if (!async_init())
        return 0;
    async_inited = 1;
    return 1;
}

/*
 * Global cleanup function. This is optional, and not strictly
 * necessary to run. Operating systems have successfully been
 * recovering memory from exiting tasks since the days when I amused
 * myself by drawing dinosaurs in crayon on used punch cards.
 *
 * If we have destructor support, this function is installed and
 * always run as a global destructor. It only does anything if
 * someone has called OPENSSL_cleanup() before it is run.
 *
 * This ensures that we do the actual cleanup requested by an
 * OPENSSL_cleanup() only after subordinate library destructors which
 * may call into OpenSSL have run.
 *
 * If we do not have destructor support, then this function is not
 * normally run, and OPENSSL_cleanup() will do nothing. If we are
 * compiled with the compile time define of
 * DO_NOT_SKIP_OPENSSL_CLEANUP, this function will be called
 * directly from OPENSSL_cleanup() so that cleanup will happen
 * when OPENSSL_cleanup() is called.
 */
void ossl_cleanup_destructor(void)
{
    /*
     * At some point we should consider looking at this function with a view to
     * moving most/all of this into onfree handlers in OSSL_LIB_CTX.
     */

    /* If we've not been inited then no need to deinit */
    if (!base_inited)
        return;

    /* If we have not been told to clean up, and we are invoked, return */
    if (!do_global_cleanup)
        return;

    /* Might be explicitly called */
    if (stopped)
        return;
    stopped = 1;

    /*
     * Thread stop may not get automatically called by the thread library for
     * the very last thread in some situations, so call it directly.
     */
    OPENSSL_thread_stop();

    CRYPTO_THREAD_lock_free(optsdone_lock);
    optsdone_lock = NULL;
    CRYPTO_THREAD_lock_free(init_lock);
    init_lock = NULL;

    CRYPTO_THREAD_cleanup_local(&in_init_config_local);

    /*
     * We assume we are single-threaded for this function, i.e. no race
     * conditions for the various "*_inited" vars below.
     */

#ifndef OPENSSL_NO_COMP
    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_comp_zlib_cleanup()\n");
    ossl_comp_zlib_cleanup();
    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_comp_brotli_cleanup()\n");
    ossl_comp_brotli_cleanup();
    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_comp_zstd_cleanup()\n");
    ossl_comp_zstd_cleanup();
#endif

    if (async_inited) {
        OSSL_TRACE(INIT, "OPENSSL_cleanup: async_deinit()\n");
        async_deinit();
    }

    /*
     * Note that cleanup order is important:
     * - ossl_rand_cleanup_int could call an ENGINE's RAND cleanup function so
     * must be called before engine_cleanup_int()
     * - ENGINEs use CRYPTO_EX_DATA and therefore, must be cleaned up
     * before the ex data handlers are wiped during default ossl_lib_ctx deinit.
     * - ossl_config_modules_free() can end up in ENGINE code so must be called
     * before engine_cleanup_int()
     * - ENGINEs and additional EVP algorithms might use added OIDs names so
     * ossl_obj_cleanup_int() must be called last
     */
    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_rand_cleanup_int()\n");
    ossl_rand_cleanup_int();

    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_config_modules_free()\n");
    ossl_config_modules_free();

#ifndef OPENSSL_NO_DEPRECATED_3_0
    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_store_cleanup_int()\n");
    ossl_store_cleanup_int();
#endif

    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_lib_ctx_default_deinit()\n");
    ossl_lib_ctx_default_deinit();

    ossl_cleanup_thread();

    OSSL_TRACE(INIT, "OPENSSL_cleanup: bio_cleanup()\n");
    bio_cleanup();

    OSSL_TRACE(INIT, "OPENSSL_cleanup: evp_cleanup_int()\n");
    evp_cleanup_int();

    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_obj_cleanup_int()\n");
    ossl_obj_cleanup_int();

    OSSL_TRACE(INIT, "OPENSSL_cleanup: err_int()\n");
    err_cleanup();

    OSSL_TRACE(INIT, "OPENSSL_cleanup: CRYPTO_secure_malloc_done()\n");
    CRYPTO_secure_malloc_done();

#ifndef OPENSSL_NO_CMP
    OSSL_TRACE(INIT, "OPENSSL_cleanup: OSSL_CMP_log_close()\n");
    OSSL_CMP_log_close();
#endif

    OSSL_TRACE(INIT, "OPENSSL_cleanup: ossl_trace_cleanup()\n");
    ossl_trace_cleanup();

#ifdef S390X_MOD_EXP
    OPENSSL_s390x_cleanup();
#endif

    base_inited = 0;
}

/*
 * If this function is called with a non NULL settings value then it must be
 * called prior to any threads making calls to any OpenSSL functions,
 * i.e. passing a non-null settings value is assumed to be single-threaded.
 */
int OPENSSL_init_crypto(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings)
{
    uint64_t tmp;
    uint64_t optsdone_bits = opts;
    int aloaddone = 0;

    /* Applications depend on 0 being returned when cleanup was already done */
    if (ossl_unlikely(stopped)) {
        if (!(opts & OPENSSL_INIT_BASE_ONLY))
            ERR_raise(ERR_LIB_CRYPTO, ERR_R_INIT_FAIL);
        return 0;
    }

    /*
     * We ignore failures from this function. It is probably because we are
     * on a platform that doesn't support lockless atomic loads (we may not
     * have created optsdone_lock yet so we can't use it). This is just an
     * optimisation to skip the full checks in this function if we don't need
     * to, so we carry on regardless in the event of failure.
     *
     * There could be a race here with other threads, so that optsdone has not
     * been updated yet, even though the options have in fact been initialised.
     * This doesn't matter - it just means we will run the full function
     * unnecessarily - but all the critical code is contained in RUN_ONCE
     * functions anyway so we are safe.
     */
    if (ossl_likely(CRYPTO_atomic_load(&optsdone, &tmp, NULL))) {
        if ((tmp & opts) == opts)
            return 1;
        aloaddone = 1;
    }

    /*
     * At some point we should look at this function with a view to moving
     * most/all of this into OSSL_LIB_CTX.
     *
     * When the caller specifies OPENSSL_INIT_BASE_ONLY, that should be the
     * *only* option specified.  With that option we return immediately after
     * doing the requested limited initialization.
     *
     * If we remain the only caller of err_shelve_state() the recursion should
     * perhaps be removed, but if in doubt, it can be left in place.
     */
    if (ossl_unlikely(!RUN_ONCE(&base, ossl_init_base)))
        return 0;

    if (opts & OPENSSL_INIT_BASE_ONLY)
        return 1;

    /*
     * optsdone_lock should definitely be set up now, so we can now repeat the
     * same check from above but be sure that it will work even on platforms
     * without lockless CRYPTO_atomic_load
     */
    if (!aloaddone) {
        if (!CRYPTO_atomic_load(&optsdone, &tmp, optsdone_lock))
            return 0;
        if ((tmp & opts) == opts)
            return 1;
    }

    if ((opts & OPENSSL_INIT_NO_LOAD_CRYPTO_STRINGS)
        && !RUN_ONCE_ALT(&load_crypto_strings,
            ossl_init_no_load_crypto_strings,
            ossl_init_load_crypto_strings))
        return 0;

    if ((opts & OPENSSL_INIT_LOAD_CRYPTO_STRINGS)
        && !RUN_ONCE(&load_crypto_strings, ossl_init_load_crypto_strings))
        return 0;

    if ((opts & OPENSSL_INIT_NO_LOAD_SSL_STRINGS)
        && !RUN_ONCE_ALT(&ssl_strings, ossl_init_no_load_ssl_strings,
            ossl_init_load_ssl_strings))
        return 0;

    if ((opts & OPENSSL_INIT_LOAD_SSL_STRINGS)
        && !RUN_ONCE(&ssl_strings, ossl_init_load_ssl_strings))
        return 0;

    if ((opts & OPENSSL_INIT_NO_ADD_ALL_CIPHERS)
        && !RUN_ONCE_ALT(&add_all_ciphers, ossl_init_no_add_all_ciphers,
            ossl_init_add_all_ciphers))
        return 0;

    if ((opts & OPENSSL_INIT_ADD_ALL_CIPHERS)
        && !RUN_ONCE(&add_all_ciphers, ossl_init_add_all_ciphers))
        return 0;

    if ((opts & OPENSSL_INIT_NO_ADD_ALL_DIGESTS)
        && !RUN_ONCE_ALT(&add_all_digests, ossl_init_no_add_all_digests,
            ossl_init_add_all_digests))
        return 0;

    if ((opts & OPENSSL_INIT_ADD_ALL_DIGESTS)
        && !RUN_ONCE(&add_all_digests, ossl_init_add_all_digests))
        return 0;

    if ((opts & OPENSSL_INIT_ATFORK)
        && !openssl_init_fork_handlers())
        return 0;

    if ((opts & OPENSSL_INIT_NO_LOAD_CONFIG)
        && !RUN_ONCE_ALT(&config, ossl_init_no_config, ossl_init_config))
        return 0;

    if (opts & OPENSSL_INIT_LOAD_CONFIG) {
        int loading = CRYPTO_THREAD_get_local(&in_init_config_local) != NULL;

        /* If called recursively from OBJ_ calls during config loading,
         * we have to mask OPENSSL_INIT_LOAD_CONFIG in optsdone to not
         * advertise that config loading is complete, otherwise other
         * threads may proceed, e.g., to fetching from a provider that
         * is not yet loaded.
         */
        if (loading) {
            optsdone_bits &= ~(uint64_t)OPENSSL_INIT_LOAD_CONFIG;
        } else {
            int ret;

            if (!CRYPTO_THREAD_set_local(&in_init_config_local, (void *)-1))
                return 0;
            if (settings == NULL) {
                ret = RUN_ONCE(&config, ossl_init_config);
            } else {
                if (!CRYPTO_THREAD_write_lock(init_lock))
                    return 0;
                conf_settings = settings;
                ret = RUN_ONCE_ALT(&config, ossl_init_config_settings,
                    ossl_init_config);
                conf_settings = NULL;
                CRYPTO_THREAD_unlock(init_lock);
            }

            if (ret <= 0)
                return 0;
        }
    }

    if ((opts & OPENSSL_INIT_ASYNC)
        && !RUN_ONCE(&async, ossl_init_async))
        return 0;

    if (!CRYPTO_atomic_or(&optsdone, optsdone_bits, &tmp, optsdone_lock))
        return 0;

    return 1;
}

void OPENSSL_cleanup(void)
{
    do_global_cleanup = 1;
#if defined(OSSL_CLEANUP_USING_DESTRUCTOR)
    return;
#endif /* defined(OSSL_CLEANUP_USING_DESTRUCTOR) */

#if defined(DO_NOT_SKIP_OPENSSL_CLEANUP)
    ossl_cleanup_destructor();
#endif /* defined(DO_NOT_SKIP_OPENSSL_CLEANUP) */
}

/*
 * Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* test_multi below tests the thread safety of a deprecated function */
#define OPENSSL_SUPPRESS_DEPRECATED

#if defined(_WIN32)
# include <windows.h>
#endif

#include <string.h>
#include <openssl/crypto.h>
#include <openssl/rsa.h>
#include <openssl/aes.h>
#include <openssl/rsa.h>
#include "testutil.h"
#include "threadstest.h"

/* Limit the maximum number of threads */
#define MAXIMUM_THREADS     10

/* Limit the maximum number of providers loaded into a library context */
#define MAXIMUM_PROVIDERS   4

static int do_fips = 0;
static char *privkey;
static char *config_file = NULL;
static int multidefault_run = 0;
static const char *default_provider[] = { "default", NULL };

static int test_lock(void)
{
    CRYPTO_RWLOCK *lock = CRYPTO_THREAD_lock_new();
    int res;

    res = TEST_true(CRYPTO_THREAD_read_lock(lock))
          && TEST_true(CRYPTO_THREAD_unlock(lock))
          && TEST_true(CRYPTO_THREAD_write_lock(lock))
          && TEST_true(CRYPTO_THREAD_unlock(lock));

    CRYPTO_THREAD_lock_free(lock);

    return res;
}

static CRYPTO_ONCE once_run = CRYPTO_ONCE_STATIC_INIT;
static unsigned once_run_count = 0;

static void once_do_run(void)
{
    once_run_count++;
}

static void once_run_thread_cb(void)
{
    CRYPTO_THREAD_run_once(&once_run, once_do_run);
}

static int test_once(void)
{
    thread_t thread;

    if (!TEST_true(run_thread(&thread, once_run_thread_cb))
        || !TEST_true(wait_for_thread(thread))
        || !CRYPTO_THREAD_run_once(&once_run, once_do_run)
        || !TEST_int_eq(once_run_count, 1))
        return 0;
    return 1;
}

static CRYPTO_THREAD_LOCAL thread_local_key;
static unsigned destructor_run_count = 0;
static int thread_local_thread_cb_ok = 0;

static void thread_local_destructor(void *arg)
{
    unsigned *count;

    if (arg == NULL)
        return;

    count = arg;

    (*count)++;
}

static void thread_local_thread_cb(void)
{
    void *ptr;

    ptr = CRYPTO_THREAD_get_local(&thread_local_key);
    if (!TEST_ptr_null(ptr)
        || !TEST_true(CRYPTO_THREAD_set_local(&thread_local_key,
                                              &destructor_run_count)))
        return;

    ptr = CRYPTO_THREAD_get_local(&thread_local_key);
    if (!TEST_ptr_eq(ptr, &destructor_run_count))
        return;

    thread_local_thread_cb_ok = 1;
}

static int test_thread_local(void)
{
    thread_t thread;
    void *ptr = NULL;

    if (!TEST_true(CRYPTO_THREAD_init_local(&thread_local_key,
                                            thread_local_destructor)))
        return 0;

    ptr = CRYPTO_THREAD_get_local(&thread_local_key);
    if (!TEST_ptr_null(ptr)
        || !TEST_true(run_thread(&thread, thread_local_thread_cb))
        || !TEST_true(wait_for_thread(thread))
        || !TEST_int_eq(thread_local_thread_cb_ok, 1))
        return 0;

#if defined(OPENSSL_THREADS) && !defined(CRYPTO_TDEBUG)

    ptr = CRYPTO_THREAD_get_local(&thread_local_key);
    if (!TEST_ptr_null(ptr))
        return 0;

# if !defined(OPENSSL_SYS_WINDOWS)
    if (!TEST_int_eq(destructor_run_count, 1))
        return 0;
# endif
#endif

    if (!TEST_true(CRYPTO_THREAD_cleanup_local(&thread_local_key)))
        return 0;
    return 1;
}

static int test_atomic(void)
{
    int val = 0, ret = 0, testresult = 0;
    uint64_t val64 = 1, ret64 = 0;
    CRYPTO_RWLOCK *lock = CRYPTO_THREAD_lock_new();

    if (!TEST_ptr(lock))
        return 0;

    if (CRYPTO_atomic_add(&val, 1, &ret, NULL)) {
        /* This succeeds therefore we're on a platform with lockless atomics */
        if (!TEST_int_eq(val, 1) || !TEST_int_eq(val, ret))
            goto err;
    } else {
        /* This failed therefore we're on a platform without lockless atomics */
        if (!TEST_int_eq(val, 0) || !TEST_int_eq(val, ret))
            goto err;
    }
    val = 0;
    ret = 0;

    if (!TEST_true(CRYPTO_atomic_add(&val, 1, &ret, lock)))
        goto err;
    if (!TEST_int_eq(val, 1) || !TEST_int_eq(val, ret))
        goto err;

    if (CRYPTO_atomic_or(&val64, 2, &ret64, NULL)) {
        /* This succeeds therefore we're on a platform with lockless atomics */
        if (!TEST_uint_eq((unsigned int)val64, 3)
                || !TEST_uint_eq((unsigned int)val64, (unsigned int)ret64))
            goto err;
    } else {
        /* This failed therefore we're on a platform without lockless atomics */
        if (!TEST_uint_eq((unsigned int)val64, 1)
                || !TEST_int_eq((unsigned int)ret64, 0))
            goto err;
    }
    val64 = 1;
    ret64 = 0;

    if (!TEST_true(CRYPTO_atomic_or(&val64, 2, &ret64, lock)))
        goto err;

    if (!TEST_uint_eq((unsigned int)val64, 3)
            || !TEST_uint_eq((unsigned int)val64, (unsigned int)ret64))
        goto err;

    ret64 = 0;
    if (CRYPTO_atomic_load(&val64, &ret64, NULL)) {
        /* This succeeds therefore we're on a platform with lockless atomics */
        if (!TEST_uint_eq((unsigned int)val64, 3)
                || !TEST_uint_eq((unsigned int)val64, (unsigned int)ret64))
            goto err;
    } else {
        /* This failed therefore we're on a platform without lockless atomics */
        if (!TEST_uint_eq((unsigned int)val64, 3)
                || !TEST_int_eq((unsigned int)ret64, 0))
            goto err;
    }

    ret64 = 0;
    if (!TEST_true(CRYPTO_atomic_load(&val64, &ret64, lock)))
        goto err;

    if (!TEST_uint_eq((unsigned int)val64, 3)
            || !TEST_uint_eq((unsigned int)val64, (unsigned int)ret64))
        goto err;

    testresult = 1;
 err:
    CRYPTO_THREAD_lock_free(lock);
    return testresult;
}

static OSSL_LIB_CTX *multi_libctx = NULL;
static int multi_success;
static OSSL_PROVIDER *multi_provider[MAXIMUM_PROVIDERS + 1];
static size_t multi_num_threads;
static thread_t multi_threads[MAXIMUM_THREADS];

static void multi_intialise(void)
{
    multi_success = 1;
    multi_libctx = NULL;
    multi_num_threads = 0;
    memset(multi_threads, 0, sizeof(multi_threads));
    memset(multi_provider, 0, sizeof(multi_provider));
}

static void thead_teardown_libctx(void)
{
    OSSL_PROVIDER **p;

    for (p = multi_provider; *p != NULL; p++)
        OSSL_PROVIDER_unload(*p);
    OSSL_LIB_CTX_free(multi_libctx);
    multi_intialise();
}

static int thread_setup_libctx(int libctx, const char *providers[])
{
    size_t n;

    if (libctx && !TEST_true(test_get_libctx(&multi_libctx, NULL, config_file,
                                             NULL, NULL)))
        return 0;

    if (providers != NULL)
        for (n = 0; providers[n] != NULL; n++)
            if (!TEST_size_t_lt(n, MAXIMUM_PROVIDERS)
                || !TEST_ptr(multi_provider[n] = OSSL_PROVIDER_load(multi_libctx,
                                                                    providers[n]))) {
                thead_teardown_libctx();
                return 0;
            }
    return 1;
}

static int teardown_threads(void)
{
    size_t i;

    for (i = 0; i < multi_num_threads; i++)
        if (!TEST_true(wait_for_thread(multi_threads[i])))
            return 0;
    return 1;
}

static int start_threads(size_t n, void (*thread_func)(void))
{
    size_t i;

    if (!TEST_size_t_le(multi_num_threads + n, MAXIMUM_THREADS))
        return 0;

    for (i = 0 ; i < n; i++)
        if (!TEST_true(run_thread(multi_threads + multi_num_threads++, thread_func)))
            return 0;
    return 1;
}

/* Template multi-threaded test function */
static int thread_run_test(void (*main_func)(void),
                           size_t num_threads, void (*thread_func)(void),
                           int libctx, const char *providers[])
{
    int testresult = 0;

    multi_intialise();
    if (!thread_setup_libctx(libctx, providers)
            || !start_threads(num_threads, thread_func))
        goto err;

    if (main_func != NULL)
        main_func();

    if (!teardown_threads()
            || !TEST_true(multi_success))
        goto err;
    testresult = 1;
 err:
    thead_teardown_libctx();
    return testresult;
}

static void thread_general_worker(void)
{
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    EVP_MD *md = EVP_MD_fetch(multi_libctx, "SHA2-256", NULL);
    EVP_CIPHER_CTX *cipherctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER *ciph = EVP_CIPHER_fetch(multi_libctx, "AES-128-CBC", NULL);
    const char *message = "Hello World";
    size_t messlen = strlen(message);
    /* Should be big enough for encryption output too */
    unsigned char out[EVP_MAX_MD_SIZE];
    const unsigned char key[AES_BLOCK_SIZE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f
    };
    const unsigned char iv[AES_BLOCK_SIZE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f
    };
    unsigned int mdoutl;
    int ciphoutl;
    EVP_PKEY *pkey = NULL;
    int testresult = 0;
    int i, isfips;

    isfips = OSSL_PROVIDER_available(multi_libctx, "fips");

    if (!TEST_ptr(mdctx)
            || !TEST_ptr(md)
            || !TEST_ptr(cipherctx)
            || !TEST_ptr(ciph))
        goto err;

    /* Do some work */
    for (i = 0; i < 5; i++) {
        if (!TEST_true(EVP_DigestInit_ex(mdctx, md, NULL))
                || !TEST_true(EVP_DigestUpdate(mdctx, message, messlen))
                || !TEST_true(EVP_DigestFinal(mdctx, out, &mdoutl)))
            goto err;
    }
    for (i = 0; i < 5; i++) {
        if (!TEST_true(EVP_EncryptInit_ex(cipherctx, ciph, NULL, key, iv))
                || !TEST_true(EVP_EncryptUpdate(cipherctx, out, &ciphoutl,
                                                (unsigned char *)message,
                                                messlen))
                || !TEST_true(EVP_EncryptFinal(cipherctx, out, &ciphoutl)))
            goto err;
    }

    /*
     * We want the test to run quickly - not securely.
     * Therefore we use an insecure bit length where we can (512).
     * In the FIPS module though we must use a longer length.
     */
    pkey = EVP_PKEY_Q_keygen(multi_libctx, NULL, "RSA", isfips ? 2048 : 512);
    if (!TEST_ptr(pkey))
        goto err;

    testresult = 1;
 err:
    EVP_MD_CTX_free(mdctx);
    EVP_MD_free(md);
    EVP_CIPHER_CTX_free(cipherctx);
    EVP_CIPHER_free(ciph);
    EVP_PKEY_free(pkey);
    if (!testresult)
        multi_success = 0;
}

static void thread_multi_simple_fetch(void)
{
    EVP_MD *md = EVP_MD_fetch(multi_libctx, "SHA2-256", NULL);

    if (md != NULL)
        EVP_MD_free(md);
    else
        multi_success = 0;
}

static EVP_PKEY *shared_evp_pkey = NULL;

static void thread_shared_evp_pkey(void)
{
    char *msg = "Hello World";
    unsigned char ctbuf[256];
    unsigned char ptbuf[256];
    size_t ptlen, ctlen = sizeof(ctbuf);
    EVP_PKEY_CTX *ctx = NULL;
    int success = 0;
    int i;

    for (i = 0; i < 1 + do_fips; i++) {
        if (i > 0)
            EVP_PKEY_CTX_free(ctx);
        ctx = EVP_PKEY_CTX_new_from_pkey(multi_libctx, shared_evp_pkey,
                                         i == 0 ? "provider=default"
                                                : "provider=fips");
        if (!TEST_ptr(ctx))
            goto err;

        if (!TEST_int_ge(EVP_PKEY_encrypt_init(ctx), 0)
                || !TEST_int_ge(EVP_PKEY_encrypt(ctx, ctbuf, &ctlen,
                                                (unsigned char *)msg, strlen(msg)),
                                                0))
            goto err;

        EVP_PKEY_CTX_free(ctx);
        ctx = EVP_PKEY_CTX_new_from_pkey(multi_libctx, shared_evp_pkey, NULL);

        if (!TEST_ptr(ctx))
            goto err;

        ptlen = sizeof(ptbuf);
        if (!TEST_int_ge(EVP_PKEY_decrypt_init(ctx), 0)
                || !TEST_int_gt(EVP_PKEY_decrypt(ctx, ptbuf, &ptlen, ctbuf, ctlen),
                                                0)
                || !TEST_mem_eq(msg, strlen(msg), ptbuf, ptlen))
            goto err;
    }

    success = 1;

 err:
    EVP_PKEY_CTX_free(ctx);
    if (!success)
        multi_success = 0;
}

static void thread_downgrade_shared_evp_pkey(void)
{
#ifndef OPENSSL_NO_DEPRECATED_3_0
    /*
     * This test is only relevant for deprecated functions that perform
     * downgrading
     */
    if (EVP_PKEY_get0_RSA(shared_evp_pkey) == NULL)
        multi_success = 0;
#else
    /* Shouldn't ever get here */
    multi_success = 0;
#endif
}

static void thread_provider_load_unload(void)
{
    OSSL_PROVIDER *deflt = OSSL_PROVIDER_load(multi_libctx, "default");

    if (!TEST_ptr(deflt)
            || !TEST_true(OSSL_PROVIDER_available(multi_libctx, "default")))
        multi_success = 0;

    OSSL_PROVIDER_unload(deflt);
}

/*
 * Do work in multiple worker threads at the same time.
 * Test 0: General worker, using the default provider
 * Test 1: General worker, using the fips provider
 * Test 2: Simple fetch worker
 * Test 3: Worker downgrading a shared EVP_PKEY
 * Test 4: Worker using a shared EVP_PKEY
 * Test 5: Worker loading and unloading a provider
 */
static int test_multi(int idx)
{
    thread_t thread1, thread2;
    int testresult = 0;
    OSSL_PROVIDER *prov = NULL, *prov2 = NULL;
    void (*worker)(void) = NULL;
    void (*worker2)(void) = NULL;
    EVP_MD *sha256 = NULL;

    if (idx == 1 && !do_fips)
        return TEST_skip("FIPS not supported");

#ifdef OPENSSL_NO_DEPRECATED_3_0
    if (idx == 3)
        return TEST_skip("Skipping tests for deprected functions");
#endif

    multi_success = 1;
    if (!TEST_true(test_get_libctx(&multi_libctx, NULL, config_file,
                                   NULL, NULL)))
        return 0;

    prov = OSSL_PROVIDER_load(multi_libctx, (idx == 1) ? "fips" : "default");
    if (!TEST_ptr(prov))
        goto err;

    switch (idx) {
    case 0:
    case 1:
        worker = thread_general_worker;
        break;
    case 2:
        worker = thread_multi_simple_fetch;
        break;
    case 3:
        worker2 = thread_downgrade_shared_evp_pkey;
        /* fall through */
    case 4:
        /*
         * If available we have both the default and fips providers for this
         * test
         */
        if (do_fips
                && !TEST_ptr(prov2 = OSSL_PROVIDER_load(multi_libctx, "fips")))
            goto err;
        if (!TEST_ptr(shared_evp_pkey = load_pkey_pem(privkey, multi_libctx)))
            goto err;
        worker = thread_shared_evp_pkey;
        break;
    case 5:
        /*
         * We ensure we get an md from the default provider, and then unload the
         * provider. This ensures the provider remains around but in a
         * deactivated state.
         */
        sha256 = EVP_MD_fetch(multi_libctx, "SHA2-256", NULL);
        OSSL_PROVIDER_unload(prov);
        prov = NULL;
        worker = thread_provider_load_unload;
        break;
    default:
        TEST_error("Invalid test index");
        goto err;
    }
    if (worker2 == NULL)
        worker2 = worker;

    if (!TEST_true(run_thread(&thread1, worker))
            || !TEST_true(run_thread(&thread2, worker2)))
        goto err;

    worker();

    testresult = 1;
    /*
     * Don't combine these into one if statement; must wait for both threads.
     */
    if (!TEST_true(wait_for_thread(thread1)))
        testresult = 0;
    if (!TEST_true(wait_for_thread(thread2)))
        testresult = 0;
    if (!TEST_true(multi_success))
        testresult = 0;

 err:
    EVP_MD_free(sha256);
    OSSL_PROVIDER_unload(prov);
    OSSL_PROVIDER_unload(prov2);
    OSSL_LIB_CTX_free(multi_libctx);
    EVP_PKEY_free(shared_evp_pkey);
    shared_evp_pkey = NULL;
    multi_libctx = NULL;
    return testresult;
}

static char *multi_load_provider = "legacy";
/*
 * This test attempts to load several providers at the same time, and if
 * run with a thread sanitizer, should crash if the core provider code
 * doesn't synchronize well enough.
 */
#define MULTI_LOAD_THREADS 10
static void test_multi_load_worker(void)
{
    OSSL_PROVIDER *prov;

    if (!TEST_ptr(prov = OSSL_PROVIDER_load(NULL, multi_load_provider))
            || !TEST_true(OSSL_PROVIDER_unload(prov)))
        multi_success = 0;
}

static int test_multi_default(void)
{
    thread_t thread1, thread2;
    int testresult = 0;
    OSSL_PROVIDER *prov = NULL;

    /* Avoid running this test twice */
    if (multidefault_run) {
        TEST_skip("multi default test already run");
        return 1;
    }
    multidefault_run = 1;

    multi_success = 1;
    multi_libctx = NULL;
    prov = OSSL_PROVIDER_load(multi_libctx, "default");
    if (!TEST_ptr(prov))
        goto err;

    if (!TEST_true(run_thread(&thread1, thread_multi_simple_fetch))
            || !TEST_true(run_thread(&thread2, thread_multi_simple_fetch)))
        goto err;

    thread_multi_simple_fetch();

    if (!TEST_true(wait_for_thread(thread1))
            || !TEST_true(wait_for_thread(thread2))
            || !TEST_true(multi_success))
        goto err;

    testresult = 1;

 err:
    OSSL_PROVIDER_unload(prov);
    return testresult;
}

static int test_multi_load(void)
{
    thread_t threads[MULTI_LOAD_THREADS];
    int i, res = 1;
    OSSL_PROVIDER *prov;

    /* The multidefault test must run prior to this test */
    if (!multidefault_run) {
        TEST_info("Running multi default test first");
        res = test_multi_default();
    }

    /*
     * We use the legacy provider in test_multi_load_worker because it uses a
     * child libctx that might hit more codepaths that might be sensitive to
     * threading issues. But in a no-legacy build that won't be loadable so
     * we use the default provider instead.
     */
    prov = OSSL_PROVIDER_load(NULL, "legacy");
    if (prov == NULL) {
        TEST_info("Cannot load legacy provider - assuming this is a no-legacy build");
        multi_load_provider = "default";
    }
    OSSL_PROVIDER_unload(prov);

    multi_success = 1;
    for (i = 0; i < MULTI_LOAD_THREADS; i++)
        (void)TEST_true(run_thread(&threads[i], test_multi_load_worker));

    for (i = 0; i < MULTI_LOAD_THREADS; i++)
        (void)TEST_true(wait_for_thread(threads[i]));

    return res && multi_success;
}

static void test_lib_ctx_load_config_worker(void)
{
    if (!TEST_int_eq(OSSL_LIB_CTX_load_config(multi_libctx, config_file), 1))
        multi_success = 0;
}

static int test_lib_ctx_load_config(void)
{
    return thread_run_test(&test_lib_ctx_load_config_worker,
                           MAXIMUM_THREADS, &test_lib_ctx_load_config_worker,
                           1, default_provider);
}

typedef enum OPTION_choice {
    OPT_ERR = -1,
    OPT_EOF = 0,
    OPT_FIPS, OPT_CONFIG_FILE,
    OPT_TEST_ENUM
} OPTION_CHOICE;

const OPTIONS *test_get_options(void)
{
    static const OPTIONS options[] = {
        OPT_TEST_OPTIONS_DEFAULT_USAGE,
        { "fips", OPT_FIPS, '-', "Test the FIPS provider" },
        { "config", OPT_CONFIG_FILE, '<',
          "The configuration file to use for the libctx" },
        { NULL }
    };
    return options;
}

int setup_tests(void)
{
    OPTION_CHOICE o;
    char *datadir;

    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_FIPS:
            do_fips = 1;
            break;
        case OPT_CONFIG_FILE:
            config_file = opt_arg();
            break;
        case OPT_TEST_CASES:
            break;
        default:
            return 0;
        }
    }

    if (!TEST_ptr(datadir = test_get_argument(0)))
        return 0;

    privkey = test_mk_file_path(datadir, "rsakey.pem");
    if (!TEST_ptr(privkey))
        return 0;

    /* Keep first to validate auto creation of default library context */
    ADD_TEST(test_multi_default);

    ADD_TEST(test_lock);
    ADD_TEST(test_once);
    ADD_TEST(test_thread_local);
    ADD_TEST(test_atomic);
    ADD_TEST(test_multi_load);
    ADD_ALL_TESTS(test_multi, 6);
    ADD_TEST(test_lib_ctx_load_config);
    return 1;
}

void cleanup_tests(void)
{
    OPENSSL_free(privkey);
}

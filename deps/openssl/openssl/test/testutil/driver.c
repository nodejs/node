/*
 * Copyright 2016-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../testutil.h"
#include "output.h"
#include "tu_local.h"

#include <string.h>
#include <assert.h>

#include "internal/nelem.h"
#include <openssl/bio.h>

#ifdef _WIN32
# define strdup _strdup
#endif

/*
 * Declares the structures needed to register each test case function.
 */
typedef struct test_info {
    const char *test_case_name;
    int (*test_fn) (void);
    int (*param_test_fn)(int idx);
    int num;

    /* flags */
    int subtest:1;
} TEST_INFO;

static TEST_INFO all_tests[1024];
static int num_tests = 0;
static int seed = 0;
/*
 * A parameterised tests runs a loop of test cases.
 * |num_test_cases| counts the total number of test cases
 * across all tests.
 */
static int num_test_cases = 0;

void add_test(const char *test_case_name, int (*test_fn) (void))
{
    assert(num_tests != OSSL_NELEM(all_tests));
    all_tests[num_tests].test_case_name = test_case_name;
    all_tests[num_tests].test_fn = test_fn;
    all_tests[num_tests].num = -1;
    ++num_tests;
    ++num_test_cases;
}

void add_all_tests(const char *test_case_name, int(*test_fn)(int idx),
                   int num, int subtest)
{
    assert(num_tests != OSSL_NELEM(all_tests));
    all_tests[num_tests].test_case_name = test_case_name;
    all_tests[num_tests].param_test_fn = test_fn;
    all_tests[num_tests].num = num;
    all_tests[num_tests].subtest = subtest;
    ++num_tests;
    num_test_cases += num;
}

static int level = 0;

int subtest_level(void)
{
    return level;
}

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
static int should_report_leaks(void)
{
    /*
     * When compiled with enable-crypto-mdebug, OPENSSL_DEBUG_MEMORY=0
     * can be used to disable leak checking at runtime.
     * Note this only works when running the test binary manually;
     * the test harness always enables OPENSSL_DEBUG_MEMORY.
     */
    char *mem_debug_env = getenv("OPENSSL_DEBUG_MEMORY");

    return mem_debug_env == NULL
        || (strcmp(mem_debug_env, "0") && strcmp(mem_debug_env, ""));
}
#endif

static int gcd(int a, int b)
{
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

void setup_test_framework()
{
    char *TAP_levels = getenv("HARNESS_OSSL_LEVEL");
    char *test_seed = getenv("OPENSSL_TEST_RAND_ORDER");

    level = TAP_levels != NULL ? 4 * atoi(TAP_levels) : 0;

    if (test_seed != NULL) {
        seed = atoi(test_seed);
        if (seed <= 0)
            seed = (int)time(NULL);
        test_printf_stdout("%*s# RAND SEED %d\n", subtest_level(), "", seed);
        test_flush_stdout();
        test_random_seed(seed);
    }

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (should_report_leaks()) {
        CRYPTO_set_mem_debug(1);
        CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
    }
#endif
}

int pulldown_test_framework(int ret)
{
    set_test_title(NULL);
#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (should_report_leaks()
        && CRYPTO_mem_leaks_cb(openssl_error_cb, NULL) <= 0)
        return EXIT_FAILURE;
#endif

    return ret;
}

static void finalize(int success)
{
    if (success)
        ERR_clear_error();
    else
        ERR_print_errors_cb(openssl_error_cb, NULL);
}

static char *test_title = NULL;

void set_test_title(const char *title)
{
    free(test_title);
    test_title = title == NULL ? NULL : strdup(title);
}

PRINTF_FORMAT(2, 3) static void test_verdict(int pass, const char *extra, ...)
{
    va_list ap;

    test_flush_stdout();
    test_flush_stderr();

    test_printf_stdout("%*s%s", level, "", pass ? "ok" : "not ok");
    if (extra != NULL) {
        test_printf_stdout(" ");
        va_start(ap, extra);
        test_vprintf_stdout(extra, ap);
        va_end(ap);
    }
    test_printf_stdout("\n");
    test_flush_stdout();
}

int run_tests(const char *test_prog_name)
{
    int num_failed = 0;
    int verdict = 1;
    int ii, i, jj, j, jstep;
    int permute[OSSL_NELEM(all_tests)];

    if (num_tests < 1) {
        test_printf_stdout("%*s1..0 # Skipped: %s\n", level, "",
                           test_prog_name);
    } else {
        if (level > 0)
            test_printf_stdout("%*s# Subtest: %s\n", level, "", test_prog_name);
        test_printf_stdout("%*s1..%d\n", level, "", num_tests);
    }
    test_flush_stdout();

    for (i = 0; i < num_tests; i++)
        permute[i] = i;
    if (seed != 0)
        for (i = num_tests - 1; i >= 1; i--) {
            j = test_random() % (1 + i);
            ii = permute[j];
            permute[j] = permute[i];
            permute[i] = ii;
        }

    for (ii = 0; ii != num_tests; ++ii) {
        i = permute[ii];
        if (all_tests[i].num == -1) {
            int ret = 0;

            set_test_title(all_tests[i].test_case_name);
            ret = all_tests[i].test_fn();

            verdict = 1;
            if (!ret) {
                verdict = 0;
                ++num_failed;
            }
            test_verdict(verdict, "%d - %s", ii + 1, test_title);
            finalize(ret);
        } else {
            int num_failed_inner = 0;

            level += 4;
            if (all_tests[i].subtest) {
                test_printf_stdout("%*s# Subtest: %s\n", level, "",
                                   all_tests[i].test_case_name);
                test_printf_stdout("%*s%d..%d\n", level, "", 1,
                                   all_tests[i].num);
                test_flush_stdout();
            }

            j = -1;
            if (seed == 0 || all_tests[i].num < 3)
                jstep = 1;
            else
                do
                    jstep = test_random() % all_tests[i].num;
                while (jstep == 0 || gcd(all_tests[i].num, jstep) != 1);

            for (jj = 0; jj < all_tests[i].num; jj++) {
                int ret;

                j = (j + jstep) % all_tests[i].num;
                set_test_title(NULL);
                ret = all_tests[i].param_test_fn(j);

                if (!ret)
                    ++num_failed_inner;

                finalize(ret);

                if (all_tests[i].subtest) {
                    verdict = 1;
                    if (!ret) {
                        verdict = 0;
                        ++num_failed_inner;
                    }
                    if (test_title != NULL)
                        test_verdict(verdict, "%d - %s", jj + 1, test_title);
                    else
                        test_verdict(verdict, "%d - iteration %d",
                                     jj + 1, j + 1);
                }
            }

            level -= 4;
            verdict = 1;
            if (num_failed_inner) {
                verdict = 0;
                ++num_failed;
            }
            test_verdict(verdict, "%d - %s", ii + 1,
                         all_tests[i].test_case_name);
        }
    }
    if (num_failed != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

/*
 * Glue an array of strings together and return it as an allocated string.
 * Optionally return the whole length of this string in |out_len|
 */
char *glue_strings(const char *list[], size_t *out_len)
{
    size_t len = 0;
    char *p, *ret;
    int i;

    for (i = 0; list[i] != NULL; i++)
        len += strlen(list[i]);

    if (out_len != NULL)
        *out_len = len;

    if (!TEST_ptr(ret = p = OPENSSL_malloc(len + 1)))
        return NULL;

    for (i = 0; list[i] != NULL; i++)
        p += strlen(strcpy(p, list[i]));

    return ret;
}

char *test_mk_file_path(const char *dir, const char *file)
{
# ifndef OPENSSL_SYS_VMS
    const char *sep = "/";
# else
    const char *sep = "";
# endif
    size_t len = strlen(dir) + strlen(sep) + strlen(file) + 1;
    char *full_file = OPENSSL_zalloc(len);

    if (full_file != NULL) {
        OPENSSL_strlcpy(full_file, dir, len);
        OPENSSL_strlcat(full_file, sep, len);
        OPENSSL_strlcat(full_file, file, len);
    }

    return full_file;
}

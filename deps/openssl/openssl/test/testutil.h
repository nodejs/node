/*
 * Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_TESTUTIL_H
# define HEADER_TESTUTIL_H

#include <openssl/err.h>

/*-
 * SETUP_TEST_FIXTURE and EXECUTE_TEST macros for test case functions.
 *
 * SETUP_TEST_FIXTURE will call set_up() to create a new TEST_FIXTURE_TYPE
 * object called "fixture". It will also allocate the "result" variable used
 * by EXECUTE_TEST. set_up() should take a const char* specifying the test
 * case name and return a TEST_FIXTURE_TYPE by value.
 *
 * EXECUTE_TEST will pass fixture to execute_func() by value, call
 * tear_down(), and return the result of execute_func(). execute_func() should
 * take a TEST_FIXTURE_TYPE by value and return 1 on success and 0 on
 * failure.
 *
 * Unit tests can define their own SETUP_TEST_FIXTURE and EXECUTE_TEST
 * variations like so:
 *
 * #define SETUP_FOOBAR_TEST_FIXTURE()\
 *   SETUP_TEST_FIXTURE(FOOBAR_TEST_FIXTURE, set_up_foobar)
 *
 * #define EXECUTE_FOOBAR_TEST()\
 *   EXECUTE_TEST(execute_foobar, tear_down_foobar)
 *
 * Then test case functions can take the form:
 *
 * static int test_foobar_feature()
 *      {
 *      SETUP_FOOBAR_TEST_FIXTURE();
 *      [...set individual members of fixture...]
 *      EXECUTE_FOOBAR_TEST();
 *      }
 */
# define SETUP_TEST_FIXTURE(TEST_FIXTURE_TYPE, set_up)\
    TEST_FIXTURE_TYPE fixture = set_up(TEST_CASE_NAME); \
    int result = 0

# define EXECUTE_TEST(execute_func, tear_down)\
        result = execute_func(fixture);\
        tear_down(fixture);\
        return result

/*
 * TEST_CASE_NAME is defined as the name of the test case function where
 * possible; otherwise we get by with the file name and line number.
 */
# if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#  if defined(_MSC_VER)
#   define TEST_CASE_NAME __FUNCTION__
#  else
#   define testutil_stringify_helper(s) #s
#   define testutil_stringify(s) testutil_stringify_helper(s)
#   define TEST_CASE_NAME __FILE__ ":" testutil_stringify(__LINE__)
#  endif                        /* _MSC_VER */
# else
#  define TEST_CASE_NAME __func__
# endif                         /* __STDC_VERSION__ */

/*
 * In main(), call ADD_TEST to register each test case function, then call
 * run_tests() to execute all tests and report the results. The result
 * returned from run_tests() should be used as the return value for main().
 */
# define ADD_TEST(test_function) add_test(#test_function, test_function)

/*
 * Simple parameterized tests. Adds a test_function(idx) test for each
 * 0 <= idx < num.
 */
# define ADD_ALL_TESTS(test_function, num) \
  add_all_tests(#test_function, test_function, num)

void add_test(const char *test_case_name, int (*test_fn) ());
void add_all_tests(const char *test_case_name, int (*test_fn)(int idx), int num);
int run_tests(const char *test_prog_name);

/*
 *  Test assumption verification helpers.
 */

/*
 * Returns 1 if |s1| and |s2| are both NULL or equal.
 * Otherwise, returns 0 and pretty-prints diagnostics using |desc|.
 */
int strings_equal(const char *desc, const char *s1, const char *s2);
#endif                          /* HEADER_TESTUTIL_H */

/*
 * For "impossible" conditions such as malloc failures or bugs in test code,
 * where continuing the test would be meaningless. Note that OPENSSL_assert
 * is fatal, and is never compiled out.
 */
#define TEST_check(condition)                   \
    do {                                        \
        if (!(condition)) {                     \
            ERR_print_errors_fp(stderr);        \
            OPENSSL_assert(!#condition);        \
        }                                       \
    } while (0);

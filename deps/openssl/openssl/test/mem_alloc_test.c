/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <openssl/err.h>
#include <openssl/types.h>
#include "testutil.h"

#ifndef USE_CUSTOM_ALLOC_FNS
# define USE_CUSTOM_ALLOC_FNS 0
#endif

/* Change to 1 to see every call of the custom allocator functions */
#define CUSTOM_FN_PRINT_CALLS 0

enum exp_ret_flags { EXP_FAIL = 0x10 };

enum exp_ret {
    /** Expecting success */
    EXP_NONNULL,
    /** Zero-size special case: can either return NULL or a special pointer */
    EXP_ZERO_SIZE,
    /** Expecting an error due to insufficient memory */
    EXP_OOM = EXP_FAIL,
    /** Expecting error due to invalid arguments */
    EXP_INVAL,
    /** Expecting error due to integer overflow */
    EXP_INT_OF,
};

#define IS_FAIL(exp_) (!!((int) (exp) & (int) EXP_FAIL))

static const char test_fn[] = "test_file_name";
enum { test_line = 31415926 };

#define SQRT_SIZE_T ((size_t) 1 << (sizeof(size_t) * (CHAR_BIT / 2)))
#define SQSQRT_SIZE_T ((size_t) 1 << (sizeof(size_t) * (CHAR_BIT / 4)))

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#if !defined(OPENSSL_NO_CRYPTO_MDEBUG) || USE_CUSTOM_ALLOC_FNS
struct call_counts {
    int malloc;
    int realloc;
    int free;
};
#endif
#if !defined(OPENSSL_NO_CRYPTO_MDEBUG)
static struct call_counts mdebug_counts;
#endif
#if USE_CUSTOM_ALLOC_FNS
static struct call_counts saved_custom_counts, cur_custom_counts;
#endif

static const struct array_alloc_vector {
    size_t nmemb;
    size_t size;
    enum exp_ret exp_malloc;
    enum exp_ret exp_calloc;
} array_alloc_vectors[] = {
    { 0,                0,                 EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0,                1,                 EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0,                SQRT_SIZE_T - 1,   EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0,                SQRT_SIZE_T,       EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0,                SIZE_MAX,          EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 1,                0,                 EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { SQRT_SIZE_T - 1,  0,                 EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { SIZE_MAX,         0,                 EXP_ZERO_SIZE, EXP_ZERO_SIZE },

    { 1,                1,                 EXP_NONNULL,   EXP_NONNULL   },

    { SQRT_SIZE_T / 2,  SQRT_SIZE_T,       EXP_OOM,       EXP_OOM       },

    { SQRT_SIZE_T,      SQRT_SIZE_T,       EXP_ZERO_SIZE, EXP_INT_OF    },

    /* Some magic numbers */
#if SIZE_MAX == 4294967295U
    { 641, 6700417, EXP_NONNULL, EXP_INT_OF },
#else /* Of course there are no archutectures other than 32- and 64-bit ones */
    { 274177, 67280421310721LLU, EXP_NONNULL, EXP_INT_OF },
#endif

    { SIZE_MAX / 4 * 3, SIZE_MAX / 2, EXP_OOM, EXP_INT_OF },
};

static const struct array_realloc_vector {
    size_t size;
    size_t orig_nmemb;
    size_t new_nmemb;
    enum exp_ret exp_orig;
    enum exp_ret exp_new;
    enum exp_ret exp_orig_array;
    enum exp_ret exp_new_array;
} array_realloc_vectors[] = {
    { 0, 0, 0,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, 0, 1,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, 0, SIZE_MAX,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, 1, 0,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, SIZE_MAX, 0,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, 1, SIZE_MAX,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, SIZE_MAX, 1,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, SIZE_MAX, SIZE_MAX,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },

    { 1, 0, 0,
      EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 1, 0, 1,
      EXP_ZERO_SIZE, EXP_NONNULL,   EXP_ZERO_SIZE, EXP_NONNULL },
    { 1, 0, SIZE_MAX,
      EXP_ZERO_SIZE, EXP_OOM,       EXP_ZERO_SIZE, EXP_OOM },
    { 1, 1, 0,
      EXP_NONNULL,   EXP_ZERO_SIZE, EXP_NONNULL,   EXP_ZERO_SIZE },
    { 1, SIZE_MAX, 0,
      EXP_OOM,       EXP_ZERO_SIZE, EXP_OOM,       EXP_ZERO_SIZE },

    { 1, 123, 345,
      EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL },
    { 1, 345, 123,
      EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL },
    { 12, 34, 56,
      EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL },
    { 12, 56, 34,
      EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL },

    { SQSQRT_SIZE_T, SIZE_MAX / SQSQRT_SIZE_T + 1, SIZE_MAX / SQSQRT_SIZE_T + 2,
      EXP_ZERO_SIZE, EXP_NONNULL,   EXP_INT_OF,    EXP_INT_OF },
    { SQSQRT_SIZE_T, SIZE_MAX / SQSQRT_SIZE_T + 2, SIZE_MAX / SQSQRT_SIZE_T + 1,
      EXP_NONNULL,   EXP_ZERO_SIZE, EXP_INT_OF,    EXP_INT_OF },

    { 123, 12, SIZE_MAX / 123 + 12,
      EXP_NONNULL,   EXP_NONNULL,   EXP_NONNULL,   EXP_INT_OF },
    { 123, SIZE_MAX / 123 + 12, 12,
      EXP_NONNULL,   EXP_NONNULL,   EXP_INT_OF,    EXP_NONNULL },
};

static const struct array_aligned_alloc_vector {
    size_t nmemb;
    size_t size;
    size_t align;
    enum exp_ret exp;
    enum exp_ret exp_array;
} array_aligned_alloc_vectors[] = {
    { 0, 0, 0, EXP_INVAL, EXP_INVAL },
    { 0, 0, 1, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, 0, 2, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, 0, 3, EXP_INVAL, EXP_INVAL },
    { 0, 0, 4, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, 0, 64, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    { 0, 0, SQSQRT_SIZE_T, EXP_ZERO_SIZE, EXP_ZERO_SIZE },
    /*
     * This one gets mem_alloc_custom_fns_test killed with SIGKILL
     * on the linux-arm64 github runner.
     */
    /* { 0, 0, SQRT_SIZE_T, EXP_ZERO_SIZE, EXP_ZERO_SIZE }, */

    { 0, 0, 64, EXP_ZERO_SIZE, EXP_ZERO_SIZE },

    { 8, 8, 63, EXP_INVAL, EXP_INVAL },
    { 8, 8, 64, EXP_NONNULL, EXP_NONNULL },
    { 3, 4, 65536, EXP_NONNULL, EXP_NONNULL },
    { 8, 8, 131072, EXP_INVAL, EXP_INVAL },
    { SIZE_MAX / 8 + 9, 8, 64, EXP_NONNULL, EXP_INT_OF },

    /*
     * the open-coded implementation tries to alloc size + alignment,
     * which should fail on integer overflow.
     */
    { 1, SIZE_MAX - 32767, 65536, EXP_INT_OF, EXP_INT_OF },
};

static int secure_memory_is_secure;

#if USE_CUSTOM_ALLOC_FNS
static void *my_malloc(const size_t num,
                       const char * const file, const int line)
{
    void * const p = malloc(num);

# if CUSTOM_FN_PRINT_CALLS
    if (file == test_fn || file == NULL
        || (strcmp(file, OPENSSL_FILE) == 0 && file[0] != '\0'))
        TEST_note("[%s:%d]: malloc(%#zx) -> %p", file, line, num, p);
# endif

    if (cur_custom_counts.malloc < INT_MAX)
        cur_custom_counts.malloc++;

    return p;
}
static void *my_realloc(void * const addr, const size_t num,
                        const char * const file, const int line)
{
# if CUSTOM_FN_PRINT_CALLS
    const uintptr_t old_addr = (uintptr_t) addr;
# endif
    void * const p = realloc(addr, num);

# if CUSTOM_FN_PRINT_CALLS
    if (file == test_fn || file == NULL
        || (strcmp(file, OPENSSL_FILE) == 0 && file[0] != '\0'))
        TEST_note("[%s:%d]: realloc(%#" PRIxPTR ", %#zx) -> %p",
                  file, line, old_addr, num, p);
# endif

    if (cur_custom_counts.realloc < INT_MAX)
        cur_custom_counts.realloc++;

    return p;
}

static void my_free(void * const addr, const char * const file, const int line)
{
# if CUSTOM_FN_PRINT_CALLS
    if (file == test_fn || file == NULL
        || (strcmp(file, OPENSSL_FILE) == 0 && file[0] != '\0'))
        TEST_note("[%s:%d]: free(%p)", file, line, addr);
# endif

    if (cur_custom_counts.free < INT_MAX)
        cur_custom_counts.free++;

    free(addr);
}
#endif /* USE_CUSTOM_ALLOC_FNS */

static bool check_zero_mem(char *p, size_t sz)
{
    for (size_t i = 0; i < sz; i++) {
        if (p[i] != 0) {
            TEST_error("Non-zero byte %zu of %zu (%#04hhx)", i, sz, p[i]);

            return false;
        }
    }

    return true;
}

static void save_counts(void)
{
#if !defined(OPENSSL_NO_CRYPTO_MDEBUG)
    CRYPTO_get_alloc_counts(&mdebug_counts.malloc,
                            &mdebug_counts.realloc,
                            &mdebug_counts.free);
#endif

#if USE_CUSTOM_ALLOC_FNS
    saved_custom_counts = cur_custom_counts;
#endif
}

static void check_exp_prep(void)
{
    ERR_set_mark();

    save_counts();
}

/*
 * Retrieve fresh call counts and check against the expected ones,
 * when the latter are no less than zero.
 */
static bool check_counts(int exp_mallocs, int exp_reallocs, int exp_frees)
{
    int test_result = 1;

#if !defined(OPENSSL_NO_CRYPTO_MDEBUG)
    {
        struct call_counts cur;

        CRYPTO_get_alloc_counts(&cur.malloc, &cur.realloc, &cur.free);
        if (exp_mallocs >= 0
            && !TEST_int_eq(cur.malloc - mdebug_counts.malloc, exp_mallocs))
            test_result = 0;
        if (exp_reallocs >= 0
            && !TEST_int_eq(cur.realloc - mdebug_counts.realloc, exp_reallocs))
            test_result = 0;
        if (exp_frees >= 0
            && !TEST_int_eq(cur.free - mdebug_counts.free, exp_frees))
            test_result = 0;
    }
#endif

#if USE_CUSTOM_ALLOC_FNS
    if (exp_mallocs >= 0
        && !TEST_int_eq(cur_custom_counts.malloc - saved_custom_counts.malloc,
                        exp_mallocs))
        test_result = 0;
    if (exp_reallocs >= 0
        && !TEST_int_eq(cur_custom_counts.realloc - saved_custom_counts.realloc,
                        exp_reallocs))
        test_result = 0;
    if (exp_frees >= 0
        && !TEST_int_eq(cur_custom_counts.free - saved_custom_counts.free,
                        exp_frees))
        test_result = 0;
#endif

    return test_result;
}

static int check_exp(const char * const fn, const int ln, const size_t sz,
                     const bool secure, const bool zero, char * const ret,
                     const enum exp_ret exp, int exp_mallocs, int exp_reallocs)
{
    int num_errs;
    unsigned long err_code = 0;
    const char *err_file = NULL;
    int err_line = 0;
    const char *err_func = NULL;
    const char *err_data = NULL;
    int err_flags = 0;
    int test_result = 1;
    unsigned long oom_err;

    num_errs = ERR_count_to_mark();
    if (num_errs > 0) {
        err_code = ERR_peek_last_error_all(&err_file, &err_line, &err_func,
                                           &err_data, &err_flags);
    }

    switch (exp) {
    case EXP_OOM:
        oom_err = secure ? CRYPTO_R_SECURE_MALLOC_FAILURE
                         : ERR_R_MALLOC_FAILURE;
        if (!TEST_ptr_null(ret)
            || !TEST_int_eq(num_errs, 1)
            || !TEST_ulong_eq(err_code, ERR_PACK(ERR_LIB_CRYPTO, 0, oom_err))
            || !TEST_str_eq(err_file, fn)
            || !TEST_int_eq(err_line, ln)
            || !TEST_str_eq(err_func, "")
            || !TEST_str_eq(err_data, "")
            || !TEST_int_eq(err_flags, 0))
            test_result = 0;

        break;

    case EXP_INVAL:
        if (!TEST_ptr_null(ret)
            || !TEST_int_eq(num_errs, 1)
            || !TEST_ulong_eq(err_code, ERR_PACK(ERR_LIB_CRYPTO, 0,
                                                 ERR_R_PASSED_INVALID_ARGUMENT))
            || !TEST_str_eq(err_file, fn)
            || !TEST_int_eq(err_line, ln)
            || !TEST_str_eq(err_func, "")
            || !TEST_str_eq(err_data, "")
            || !TEST_int_eq(err_flags, 0))
            test_result = 0;

        break;

    case EXP_INT_OF:
        if (!TEST_ptr_null(ret)
            || !TEST_int_eq(num_errs, 1)
            || !TEST_ulong_eq(err_code, ERR_PACK(ERR_LIB_CRYPTO, 0,
                                                 CRYPTO_R_INTEGER_OVERFLOW))
            || !TEST_str_eq(err_file, fn)
            || !TEST_int_eq(err_line, ln)
            || !TEST_str_eq(err_func, "")
            || !TEST_str_eq(err_data, "")
            || !TEST_int_eq(err_flags, 0))
            test_result = 0;

        break;

    case EXP_NONNULL:
        if (!TEST_ptr(ret)
            || !TEST_int_eq(num_errs, 0)) {
            test_result = 0;
        } else if (zero) {
            if (!check_zero_mem(ret, sz))
                test_result = 0;
        }

        break;

    case EXP_ZERO_SIZE:
        /*
         * Since the pointer ca either be NULL or non-NULL, depending
         * on implementation, we can only check for the absence of errors.
         */
        if (!TEST_int_eq(num_errs, 0))
            test_result = 0;

        break;

    default:
        TEST_error("Unexpected expected result");
        test_result = 0;
    }

    ERR_pop_to_mark();

    /*
     * We don't check for frees here as there's a non-trivial amount
     * of free calls in the error handling routines.
     */
    test_result &= check_counts(exp_mallocs, exp_reallocs, -1);

    return test_result;
}

static int test_xalloc(const bool secure, const bool array, const bool zero,
                       const bool macro, const struct array_alloc_vector *td)
{
    char *ret;
    int ln = test_line;
    size_t sz = td->nmemb * td->size;
    enum exp_ret exp = array ? td->exp_calloc : td->exp_malloc;
    bool really_secure = secure && secure_memory_is_secure;
    int exp_cnt = 0;
    int res;

    check_exp_prep();

    if (macro) {
        if (secure) {
            if (array) {
                if (zero)
                    ln = OPENSSL_LINE, ret = OPENSSL_secure_calloc(td->nmemb, td->size);
                else
                    ln = OPENSSL_LINE, ret = OPENSSL_secure_malloc_array(td->nmemb, td->size);
            } else {
                if (zero)
                    ln = OPENSSL_LINE, ret = OPENSSL_secure_zalloc(sz);
                else
                    ln = OPENSSL_LINE, ret = OPENSSL_secure_malloc(sz);
            }
        } else {
            if (array) {
                if (zero)
                    ln = OPENSSL_LINE, ret = OPENSSL_calloc(td->nmemb, td->size);
                else
                    ln = OPENSSL_LINE, ret = OPENSSL_malloc_array(td->nmemb, td->size);
            } else {
                if (zero)
                    ln = OPENSSL_LINE, ret = OPENSSL_zalloc(sz);
                else
                    ln = OPENSSL_LINE, ret = OPENSSL_malloc(sz);
            }
        }
    } else {
        if (array) {
            ret = (secure ? (zero ? CRYPTO_secure_calloc
                                  : CRYPTO_secure_malloc_array)
                          : (zero ? CRYPTO_calloc
                                  : CRYPTO_malloc_array))(td->nmemb, td->size,
                                                          test_fn, test_line);
        } else {
            ret = (secure ? (zero ? CRYPTO_secure_zalloc
                                  : CRYPTO_secure_malloc)
                          : (zero ? CRYPTO_zalloc
                                  : CRYPTO_malloc))(sz, test_fn, test_line);
        }
    }

    /*
     * There is an OPENSSL_calloc in ERR_set_debug, triggered
     * from ossl_report_alloc_err_ex.
     */
    exp_cnt += IS_FAIL(exp) && (!macro || (bool) OPENSSL_FILE[0]);
    /*
     * Secure allocations don't trigger alloc counting.
     * EXP_OOM is special as it comes on return from the (called and counted)
     * allocation function.
     */
    if (!really_secure)
        exp_cnt += !!(exp == EXP_OOM || !IS_FAIL(exp));
    res = check_exp(macro ? OPENSSL_FILE : test_fn, ln, sz, really_secure, zero,
                    ret, exp, exp_cnt, 0);

    if (really_secure)
        OPENSSL_secure_free(ret);
    else
        OPENSSL_free(ret);

    return res;
}

static int test_xrealloc(const bool clear, const bool array, const bool macro,
                         const struct array_realloc_vector *td)
{
    char *ret = NULL;
    char *old_ret = NULL;
    int exp_malloc_cnt, exp_realloc_cnt;
    int res = 1;
    size_t i;

    /*
     * Do two passes, first with NULL ptr, then with the result of the first
     * call.
     */
    for (i = 0; i < 2; i++) {
        size_t nmemb = i ? td->new_nmemb : td->orig_nmemb;
        size_t old_nmemb = i ? td->orig_nmemb : 0;
        size_t sz = nmemb * td->size;
        size_t old_sz = old_nmemb * td->size;
        int ln = test_line;
        enum exp_ret exp = i ? (array ? td->exp_new_array  : td->exp_new)
                             : (array ? td->exp_orig_array : td->exp_orig);
        enum exp_ret exp2 = !i ? (array ? td->exp_new_array  : td->exp_new)
                               : (array ? td->exp_orig_array : td->exp_orig);

        exp_malloc_cnt = exp_realloc_cnt = 0;

        /* clear_realloc_array checks both new and old sizes */
        if (clear && array && i && exp2 == EXP_INT_OF)
            exp = EXP_INT_OF;

        if (exp != EXP_INT_OF) {
            if (clear) {
                /*
                 * clear_alloc just calls cleanse if contraction has been
                 * requested.
                 */
                if (ret == NULL || sz > old_sz)
                    exp_malloc_cnt++;
            } else {
                exp_realloc_cnt++;
#if !USE_CUSTOM_ALLOC_FNS
                /* CRYPTO_malloc() is called explicitly when p is NULL. */
                if (ret == NULL)
                    exp_malloc_cnt++;
#endif
            }
        } else {
            if (!macro || OPENSSL_FILE[0] != '\0')
                exp_malloc_cnt++;
        }

        check_exp_prep();

        if (macro) {
            if (array) {
                if (clear)
                    ln = OPENSSL_LINE, ret = OPENSSL_clear_realloc_array(ret, old_nmemb, nmemb, td->size);
                else
                    ln = OPENSSL_LINE, ret = OPENSSL_realloc_array(ret, nmemb, td->size);
            } else {
                if (clear)
                    ln = OPENSSL_LINE, ret = OPENSSL_clear_realloc(ret, old_sz, sz);
                else
                    ln = OPENSSL_LINE, ret = OPENSSL_realloc(ret, sz);
            }
        } else {
            if (array) {
                if (clear)
                    ret = CRYPTO_clear_realloc_array(ret, old_nmemb, nmemb,
                                                     td->size,
                                                     test_fn, test_line);
                else
                    ret = CRYPTO_realloc_array(ret, nmemb, td->size,
                                               test_fn, test_line);
            } else {
                if (clear)
                    ret = CRYPTO_clear_realloc(ret, old_sz, sz,
                                               test_fn, test_line);
                else
                    ret = CRYPTO_realloc(ret, sz, test_fn, test_line);
            }
        }

        /*
         * There is an OPENSSL_calloc in ERR_set_debug, triggered
         * from ossl_report_alloc_err_ex.
         */
        exp_malloc_cnt += !!(exp == EXP_OOM
                             && (!macro || (bool) OPENSSL_FILE[0]));

        res = check_exp(macro ? OPENSSL_FILE : test_fn, ln, sz, false, false,
                        ret, exp, exp_malloc_cnt, exp_realloc_cnt);
        if (res == 0)
            TEST_error("realloc return code check fail with i = %zu, ret = %p"
                       ", old_nmemb = %#zx, nmemb = %#zx, size = %#zx",
                       i, (void *) ret, old_nmemb, nmemb, td->size);

        /* Write data on the first pass and check it on the second */
        if (res != 0 && exp == EXP_NONNULL && exp2 == EXP_NONNULL) {
            size_t check_sz = MIN(td->orig_nmemb * td->size,
                                  td->new_nmemb * td->size);
            size_t j;
            size_t num_err = 0;

            if (i != 0) {
                for (j = 0; j < check_sz; j++) {
                    char exp_val = (uint8_t) ((uintptr_t) td * 253 + j * 17);

                    if (ret[j] != exp_val) {
                        if (!num_err)
                            TEST_error("Memory mismatch at byte %zu of %zu: "
                                       "%#04hhx != %#04hhx",
                                       j, check_sz, ret[j], exp_val);

                        res = 0;
                        num_err++;
                    }
                }

                if (num_err != 0)
                    TEST_error("Total errors: %zu", num_err);
            } else {
                for (j = 0; j < check_sz; j++)
                    ret[j] = (uint8_t) ((uintptr_t) td * 253 + j * 17);
            }
        }

        /* Freeing the old allocation if realloc has failed */
        if (ret == NULL && exp != EXP_ZERO_SIZE)
            OPENSSL_free(old_ret);

        old_ret = ret;
    }

    OPENSSL_free(ret);

    return res;
}

static int test_xaligned_alloc(const bool array, const bool macro,
                               const struct array_aligned_alloc_vector *td)
{
    char *ret;
    int ln = test_line;
    size_t sz = td->nmemb * td->size;
    enum exp_ret exp = array ? td->exp_array : td->exp;
    int exp_cnt = 0;
    void *freeptr = &freeptr;
    int res = 1;

    check_exp_prep();

    if (macro) {
        if (array) {
            ln = OPENSSL_LINE, ret = OPENSSL_aligned_alloc_array(td->nmemb, td->size, td->align, &freeptr);
        } else {
            ln = OPENSSL_LINE, ret = OPENSSL_aligned_alloc(sz, td->align, &freeptr);
        }
    } else {
        if (array)
            ret = CRYPTO_aligned_alloc_array(td->nmemb, td->size, td->align,
                                             &freeptr, test_fn, test_line);
        else
            ret = CRYPTO_aligned_alloc(sz, td->align, &freeptr,
                                       test_fn, test_line);
    }

    /*
     * aligned_alloc doesn't increment the call counts by itself, and
     * OPENSSL_malloc is only called when the open-coded implementation
     * is used.
     */
#if USE_CUSTOM_ALLOC_FNS \
    || !(defined(_BSD_SOURCE) || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L))
    exp_cnt += !!(exp != EXP_INT_OF && exp != EXP_INVAL);
#endif

    /*
     * There is an OPENSSL_calloc in ERR_set_debug, triggered
     * from ossl_report_alloc_err_ex.
     */
    exp_cnt += IS_FAIL(exp) && (!macro || (bool) OPENSSL_FILE[0]);
    res &= check_exp(macro ? OPENSSL_FILE : test_fn, ln, sz, false, false,
                     ret, exp, exp_cnt, 0);

    /* Check the pointer's alignment */
    if (exp == EXP_NONNULL) {
        if (!TEST_uint64_t_eq((uintptr_t) ret & (td->align - 1), 0))
            res = 0;
    }

    if (IS_FAIL(exp) && !TEST_ptr_null(freeptr))
        res = 0;
    if ((exp == EXP_NONNULL) && !TEST_ptr(freeptr))
        res = 0;

    OPENSSL_free(freeptr);

    return res;
}

static int test_malloc(const int i)
{
    return test_xalloc(false, false, false, false, array_alloc_vectors + i)
        && test_xalloc(false, false, false, true,  array_alloc_vectors + i);
}

static int test_zalloc(const int i)
{
    return test_xalloc(false, false, true, false, array_alloc_vectors + i)
        && test_xalloc(false, false, true, true,  array_alloc_vectors + i);
}

static int test_malloc_array(const int i)
{
    return test_xalloc(false, true, false, false, array_alloc_vectors + i)
        && test_xalloc(false, true, false, true,  array_alloc_vectors + i);
}

static int test_calloc(const int i)
{
    return test_xalloc(false, true, true, false, array_alloc_vectors + i)
        && test_xalloc(false, true, true, true,  array_alloc_vectors + i);
}

static int test_secure_malloc(const int i)
{
    return test_xalloc(true, false, false, false, array_alloc_vectors + i)
        && test_xalloc(true, false, false, true,  array_alloc_vectors + i);
}

static int test_secure_zalloc(const int i)
{
    return test_xalloc(true, false, true, false, array_alloc_vectors + i)
        && test_xalloc(true, false, true, true,  array_alloc_vectors + i);
}

static int test_secure_malloc_array(const int i)
{
    return test_xalloc(true, true, false, false, array_alloc_vectors + i)
        && test_xalloc(true, true, false, true,  array_alloc_vectors + i);
}

static int test_secure_calloc(const int i)
{
    return test_xalloc(true, true, true, false, array_alloc_vectors + i)
        && test_xalloc(true, true, true, true,  array_alloc_vectors + i);
}

static int test_realloc(const int i)
{
    return test_xrealloc(false, false, false, array_realloc_vectors + i)
        && test_xrealloc(false, false, true,  array_realloc_vectors + i);
}

static int test_clear_realloc(const int i)
{
    return test_xrealloc(true, false, false, array_realloc_vectors + i)
        && test_xrealloc(true, false, true,  array_realloc_vectors + i);
}

static int test_realloc_array(const int i)
{
    return test_xrealloc(false, true, false, array_realloc_vectors + i)
        && test_xrealloc(false, true, true,  array_realloc_vectors + i);
}

static int test_clear_realloc_array(const int i)
{
    return test_xrealloc(true, true, false, array_realloc_vectors + i)
        && test_xrealloc(true, true, true,  array_realloc_vectors + i);
}

static int test_aligned_alloc(const int i)
{
    return test_xaligned_alloc(false, false, array_aligned_alloc_vectors + i)
        && test_xaligned_alloc(false, true,  array_aligned_alloc_vectors + i);
}

static int test_aligned_alloc_array(const int i)
{
    return test_xaligned_alloc(true, false, array_aligned_alloc_vectors + i)
        && test_xaligned_alloc(true, true,  array_aligned_alloc_vectors + i);
}

static int test_free(void)
{
    int test_result = 1;
    void *p;

    save_counts();
    OPENSSL_free(NULL);
    if (!TEST_int_eq(check_counts(0, 0, 1), 1))
        test_result = 0;

    save_counts();
    CRYPTO_free(NULL, test_fn, test_line);
    if (!TEST_int_eq(check_counts(0, 0, 1), 1))
        test_result = 0;

    save_counts();
    p = OPENSSL_malloc(42);
    OPENSSL_free(p);
    if (!TEST_int_eq(check_counts(1, 0, 1), 1))
        test_result = 0;

    save_counts();
    p = CRYPTO_calloc(23, 69, test_fn, test_line);
    CRYPTO_free(p, test_fn, test_line);
    if (!TEST_int_eq(check_counts(1, 0, 1), 1))
        test_result = 0;

    return test_result;
}

int setup_tests(void)
{
    secure_memory_is_secure = CRYPTO_secure_malloc_init(65536, 4);
    TEST_info("secure memory init: %d", secure_memory_is_secure);

    ADD_ALL_TESTS(test_malloc, OSSL_NELEM(array_alloc_vectors));
    ADD_ALL_TESTS(test_zalloc, OSSL_NELEM(array_alloc_vectors));
    ADD_ALL_TESTS(test_malloc_array, OSSL_NELEM(array_alloc_vectors));
    ADD_ALL_TESTS(test_calloc, OSSL_NELEM(array_alloc_vectors));

    ADD_ALL_TESTS(test_secure_malloc, OSSL_NELEM(array_alloc_vectors));
    ADD_ALL_TESTS(test_secure_zalloc, OSSL_NELEM(array_alloc_vectors));
    ADD_ALL_TESTS(test_secure_malloc_array, OSSL_NELEM(array_alloc_vectors));
    ADD_ALL_TESTS(test_secure_calloc, OSSL_NELEM(array_alloc_vectors));

    ADD_ALL_TESTS(test_realloc, OSSL_NELEM(array_realloc_vectors));
    ADD_ALL_TESTS(test_clear_realloc, OSSL_NELEM(array_realloc_vectors));
    ADD_ALL_TESTS(test_realloc_array, OSSL_NELEM(array_realloc_vectors));
    ADD_ALL_TESTS(test_clear_realloc_array, OSSL_NELEM(array_realloc_vectors));

    ADD_ALL_TESTS(test_aligned_alloc, OSSL_NELEM(array_aligned_alloc_vectors));
    ADD_ALL_TESTS(test_aligned_alloc_array,
                  OSSL_NELEM(array_aligned_alloc_vectors));

    ADD_TEST(test_free);

    return 1;
}

#if USE_CUSTOM_ALLOC_FNS
int global_init(void)
{
    if (!CRYPTO_set_mem_functions(my_malloc, my_realloc, my_free)) {
        fprintf(stderr, "Failed to override allocator functions");

        return 0;
    }

    return 1;
}
#endif

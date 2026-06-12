/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Utility overflow checking and reporting functions
 */

#ifndef OSSL_INTERNAL_CHECK_SIZE_OVERFLOW_H
# define OSSL_INTERNAL_CHECK_SIZE_OVERFLOW_H

# include <limits.h>
# include <stdbool.h>
# include <stdint.h>

# include "internal/common.h"
# include "internal/safe_math.h"

# include <openssl/cryptoerr.h>
# include <openssl/err.h>

OSSL_SAFE_MATH_UNSIGNED(size_t, size_t)

/*
 * A helper open-coded aligned alloc implementation around CRYPTO_malloc(),
 * for use in cases where non-standard malloc implementation is (or may be,
 * as it is the case of the FIPS module) used, or posix_memalign
 * is not available.
 */
void *ossl_malloc_align(size_t num, size_t alignment, void **freeptr,
                        const char *file, int line);

/*
 * A helper routine to report memory allocation errors.
 * Similar to the ERR_raise() macro, but accepts explicit file/line arguments,
 * pre-defines the library to ERR_LIB_CRYPTO, and avoids emitting an error
 * if both file set to NULL and line set to 0.
 */
static ossl_inline ossl_unused void
ossl_report_alloc_err_ex(const char * const file, const int line,
                         const int reason)
{
    /*
     * ossl_err_get_state_int() in err.c uses CRYPTO_zalloc(num, NULL, 0) for
     * ERR_STATE allocation. Prevent mem alloc error loop while reporting error.
     */
    if (file != NULL || line != 0) {
        ERR_new();
        ERR_set_debug(file, line, NULL);
        ERR_set_error(ERR_LIB_CRYPTO, reason, NULL);
    }
}

/* Report a memory allocation failure. */
static ossl_inline ossl_unused void
ossl_report_alloc_err(const char * const file, const int line)
{
    ossl_report_alloc_err_ex(file, line, ERR_R_MALLOC_FAILURE);
}

/* Report an integer overflow during allocation size calculation. */
static ossl_inline ossl_unused void
ossl_report_alloc_err_of(const char * const file, const int line)
{
    ossl_report_alloc_err_ex(file, line, CRYPTO_R_INTEGER_OVERFLOW);
}

/* Report invalid memory allocation call arguments. */
static ossl_inline ossl_unused void
ossl_report_alloc_err_inv(const char * const file, const int line)
{
    ossl_report_alloc_err_ex(file, line, ERR_R_PASSED_INVALID_ARGUMENT);
}

/*
 * Check the result of num and size multiplication for overflow
 * and set error if it is the case;  return true if there was no overflow,
 * false if there was.
 */
static ossl_inline ossl_unused bool
ossl_size_mul(const size_t num, const size_t size, size_t *bytes,
              const char * const file, const int line)
{
    int err = 0;
    *bytes = safe_mul_size_t(num, size, &err);

    if (ossl_unlikely(err != 0)) {
        ossl_report_alloc_err_of(file, line);

        return false;
    }

    return true;
}

/*
 * Check the result of size1 and size2 addition for overflow
 * and set error if it is the case;  returns true if there was no overflow,
 * false if there was.
 */
static ossl_inline ossl_unused bool
ossl_size_add(const size_t size1, const size_t size2, size_t *bytes,
              const char * const file, const int line)
{
    int err = 0;
    *bytes = safe_add_size_t(size1, size2, &err);

    if (ossl_unlikely(err != 0)) {
        ossl_report_alloc_err_of(file, line);

        return false;
    }

    return true;
}

#endif /* OSSL_INTERNAL_CHECK_SIZE_OVERFLOW_H */

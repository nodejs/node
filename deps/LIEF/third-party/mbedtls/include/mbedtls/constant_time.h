/**
 *  Constant-time functions
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CONSTANT_TIME_H
#define MBEDTLS_CONSTANT_TIME_H

#include <stddef.h>

/** Constant-time buffer comparison without branches.
 *
 * This is equivalent to the standard memcmp function, but is likely to be
 * compiled to code using bitwise operations rather than a branch, such that
 * the time taken is constant w.r.t. the data pointed to by \p a and \p b,
 * and w.r.t. whether \p a and \p b are equal or not. It is not constant-time
 * w.r.t. \p n .
 *
 * This function can be used to write constant-time code by replacing branches
 * with bit operations using masks.
 *
 * \param a     Pointer to the first buffer, containing at least \p n bytes. May not be NULL.
 * \param b     Pointer to the second buffer, containing at least \p n bytes. May not be NULL.
 * \param n     The number of bytes to compare.
 *
 * \return      Zero if the contents of the two buffers are the same,
 *              otherwise non-zero.
 */
int mbedtls_ct_memcmp(const void *a,
                      const void *b,
                      size_t n);

#endif /* MBEDTLS_CONSTANT_TIME_H */

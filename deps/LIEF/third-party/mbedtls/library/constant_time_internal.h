/**
 *  Constant-time functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CONSTANT_TIME_INTERNAL_H
#define MBEDTLS_CONSTANT_TIME_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

#include "common.h"

#if defined(MBEDTLS_BIGNUM_C)
#include "mbedtls/bignum.h"
#endif

/* The constant-time interface provides various operations that are likely
 * to result in constant-time code that does not branch or use conditional
 * instructions for secret data (for secret pointers, this also applies to
 * the data pointed to).
 *
 * It has three main parts:
 *
 * - boolean operations
 *   These are all named mbedtls_ct_<type>_<operation>.
 *   They operate over <type> and return mbedtls_ct_condition_t.
 *   All arguments are considered secret.
 *   example: bool x = y | z          =>    x = mbedtls_ct_bool_or(y, z)
 *   example: bool x = y == z         =>    x = mbedtls_ct_uint_eq(y, z)
 *
 * - conditional data selection
 *   These are all named mbedtls_ct_<type>_if and mbedtls_ct_<type>_if_else_0
 *   All arguments are considered secret.
 *   example: size_t a = x ? b : c    =>    a = mbedtls_ct_size_if(x, b, c)
 *   example: unsigned a = x ? b : 0  =>    a = mbedtls_ct_uint_if_else_0(x, b)
 *
 * - block memory operations
 *   Only some arguments are considered secret, as documented for each
 *   function.
 *   example: if (x) memcpy(...)      =>    mbedtls_ct_memcpy_if(x, ...)
 *
 * mbedtls_ct_condition_t must be treated as opaque and only created and
 * manipulated via the functions in this header. The compiler should never
 * be able to prove anything about its value at compile-time.
 *
 * mbedtls_ct_uint_t is an unsigned integer type over which constant time
 * operations may be performed via the functions in this header. It is as big
 * as the larger of size_t and mbedtls_mpi_uint, i.e. it is safe to cast
 * to/from "unsigned int", "size_t", and "mbedtls_mpi_uint" (and any other
 * not-larger integer types).
 *
 * For Arm (32-bit, 64-bit and Thumb), x86 and x86-64, assembly implementations
 * are used to ensure that the generated code is constant time. For other
 * architectures, it uses a plain C fallback designed to yield constant-time code
 * (this has been observed to be constant-time on latest gcc, clang and MSVC
 * as of May 2023).
 *
 * For readability, the static inline definitions are separated out into
 * constant_time_impl.h.
 */

#if (SIZE_MAX > 0xffffffffffffffffULL)
/* Pointer size > 64-bit */
typedef size_t    mbedtls_ct_condition_t;
typedef size_t    mbedtls_ct_uint_t;
typedef ptrdiff_t mbedtls_ct_int_t;
#define MBEDTLS_CT_TRUE  ((mbedtls_ct_condition_t) mbedtls_ct_compiler_opaque(SIZE_MAX))
#elif (SIZE_MAX > 0xffffffff) || defined(MBEDTLS_HAVE_INT64)
/* 32-bit < pointer size <= 64-bit, or 64-bit MPI */
typedef uint64_t  mbedtls_ct_condition_t;
typedef uint64_t  mbedtls_ct_uint_t;
typedef int64_t   mbedtls_ct_int_t;
#define MBEDTLS_CT_SIZE_64
#define MBEDTLS_CT_TRUE  ((mbedtls_ct_condition_t) mbedtls_ct_compiler_opaque(UINT64_MAX))
#else
/* Pointer size <= 32-bit, and no 64-bit MPIs */
typedef uint32_t  mbedtls_ct_condition_t;
typedef uint32_t  mbedtls_ct_uint_t;
typedef int32_t   mbedtls_ct_int_t;
#define MBEDTLS_CT_SIZE_32
#define MBEDTLS_CT_TRUE  ((mbedtls_ct_condition_t) mbedtls_ct_compiler_opaque(UINT32_MAX))
#endif
#define MBEDTLS_CT_FALSE ((mbedtls_ct_condition_t) mbedtls_ct_compiler_opaque(0))

/* ============================================================================
 * Boolean operations
 */

/** Convert a number into a mbedtls_ct_condition_t.
 *
 * \param x Number to convert.
 *
 * \return MBEDTLS_CT_TRUE if \p x != 0, or MBEDTLS_CT_FALSE if \p x == 0
 *
 */
static inline mbedtls_ct_condition_t mbedtls_ct_bool(mbedtls_ct_uint_t x);

/** Boolean "not equal" operation.
 *
 * Functionally equivalent to:
 *
 * \p x != \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \return      MBEDTLS_CT_TRUE if \p x != \p y, otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_uint_ne(mbedtls_ct_uint_t x, mbedtls_ct_uint_t y);

/** Boolean "equals" operation.
 *
 * Functionally equivalent to:
 *
 * \p x == \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \return      MBEDTLS_CT_TRUE if \p x == \p y, otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_uint_eq(mbedtls_ct_uint_t x,
                                                        mbedtls_ct_uint_t y);

/** Boolean "less than" operation.
 *
 * Functionally equivalent to:
 *
 * \p x < \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \return      MBEDTLS_CT_TRUE if \p x < \p y, otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_uint_lt(mbedtls_ct_uint_t x, mbedtls_ct_uint_t y);

/** Boolean "greater than" operation.
 *
 * Functionally equivalent to:
 *
 * \p x > \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \return      MBEDTLS_CT_TRUE if \p x > \p y, otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_uint_gt(mbedtls_ct_uint_t x,
                                                        mbedtls_ct_uint_t y);

/** Boolean "greater or equal" operation.
 *
 * Functionally equivalent to:
 *
 * \p x >= \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \return      MBEDTLS_CT_TRUE if \p x >= \p y,
 *              otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_uint_ge(mbedtls_ct_uint_t x,
                                                        mbedtls_ct_uint_t y);

/** Boolean "less than or equal" operation.
 *
 * Functionally equivalent to:
 *
 * \p x <= \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \return      MBEDTLS_CT_TRUE if \p x <= \p y,
 *              otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_uint_le(mbedtls_ct_uint_t x,
                                                        mbedtls_ct_uint_t y);

/** Boolean not-equals operation.
 *
 * Functionally equivalent to:
 *
 * \p x != \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \note        This is more efficient than mbedtls_ct_uint_ne if both arguments are
 *              mbedtls_ct_condition_t.
 *
 * \return      MBEDTLS_CT_TRUE if \p x != \p y,
 *              otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_bool_ne(mbedtls_ct_condition_t x,
                                                        mbedtls_ct_condition_t y);

/** Boolean "and" operation.
 *
 * Functionally equivalent to:
 *
 * \p x && \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \return      MBEDTLS_CT_TRUE if \p x && \p y,
 *              otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_bool_and(mbedtls_ct_condition_t x,
                                                         mbedtls_ct_condition_t y);

/** Boolean "or" operation.
 *
 * Functionally equivalent to:
 *
 * \p x || \p y
 *
 * \param x     The first value to analyze.
 * \param y     The second value to analyze.
 *
 * \return      MBEDTLS_CT_TRUE if \p x || \p y,
 *              otherwise MBEDTLS_CT_FALSE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_bool_or(mbedtls_ct_condition_t x,
                                                        mbedtls_ct_condition_t y);

/** Boolean "not" operation.
 *
 * Functionally equivalent to:
 *
 * ! \p x
 *
 * \param x     The value to invert
 *
 * \return      MBEDTLS_CT_FALSE if \p x, otherwise MBEDTLS_CT_TRUE.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_bool_not(mbedtls_ct_condition_t x);


/* ============================================================================
 * Data selection operations
 */

/** Choose between two size_t values.
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : if0.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 * \param if0           Value to use if \p condition == MBEDTLS_CT_FALSE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise \c if0.
 */
static inline size_t mbedtls_ct_size_if(mbedtls_ct_condition_t condition,
                                        size_t if1,
                                        size_t if0);

/** Choose between two unsigned values.
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : if0.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 * \param if0           Value to use if \p condition == MBEDTLS_CT_FALSE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise \c if0.
 */
static inline unsigned mbedtls_ct_uint_if(mbedtls_ct_condition_t condition,
                                          unsigned if1,
                                          unsigned if0);

/** Choose between two mbedtls_ct_condition_t values.
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : if0.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 * \param if0           Value to use if \p condition == MBEDTLS_CT_FALSE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise \c if0.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_bool_if(mbedtls_ct_condition_t condition,
                                                        mbedtls_ct_condition_t if1,
                                                        mbedtls_ct_condition_t if0);

#if defined(MBEDTLS_BIGNUM_C)

/** Choose between two mbedtls_mpi_uint values.
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : if0.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 * \param if0           Value to use if \p condition == MBEDTLS_CT_FALSE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise \c if0.
 */
static inline mbedtls_mpi_uint mbedtls_ct_mpi_uint_if(mbedtls_ct_condition_t condition, \
                                                      mbedtls_mpi_uint if1, \
                                                      mbedtls_mpi_uint if0);

#endif

/** Choose between an unsigned value and 0.
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : 0.
 *
 * Functionally equivalent to mbedtls_ct_uint_if(condition, if1, 0) but
 * results in smaller code size.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise 0.
 */
static inline unsigned mbedtls_ct_uint_if_else_0(mbedtls_ct_condition_t condition, unsigned if1);

/** Choose between an mbedtls_ct_condition_t and 0.
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : 0.
 *
 * Functionally equivalent to mbedtls_ct_bool_if(condition, if1, 0) but
 * results in smaller code size.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise 0.
 */
static inline mbedtls_ct_condition_t mbedtls_ct_bool_if_else_0(mbedtls_ct_condition_t condition,
                                                               mbedtls_ct_condition_t if1);

/** Choose between a size_t value and 0.
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : 0.
 *
 * Functionally equivalent to mbedtls_ct_size_if(condition, if1, 0) but
 * results in smaller code size.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise 0.
 */
static inline size_t mbedtls_ct_size_if_else_0(mbedtls_ct_condition_t condition, size_t if1);

#if defined(MBEDTLS_BIGNUM_C)

/** Choose between an mbedtls_mpi_uint value and 0.
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : 0.
 *
 * Functionally equivalent to mbedtls_ct_mpi_uint_if(condition, if1, 0) but
 * results in smaller code size.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise 0.
 */
static inline mbedtls_mpi_uint mbedtls_ct_mpi_uint_if_else_0(mbedtls_ct_condition_t condition,
                                                             mbedtls_mpi_uint if1);

#endif

/** Constant-flow char selection
 *
 * \param low   Secret. Bottom of range
 * \param high  Secret. Top of range
 * \param c     Secret. Value to compare to range
 * \param t     Secret. Value to return, if in range
 *
 * \return      \p t if \p low <= \p c <= \p high, 0 otherwise.
 */
static inline unsigned char mbedtls_ct_uchar_in_range_if(unsigned char low,
                                                         unsigned char high,
                                                         unsigned char c,
                                                         unsigned char t);

/** Choose between two error values. The values must be in the range [-32767..0].
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : if0.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 * \param if0           Value to use if \p condition == MBEDTLS_CT_FALSE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise \c if0.
 */
static inline int mbedtls_ct_error_if(mbedtls_ct_condition_t condition, int if1, int if0);

/** Choose between an error value and 0. The error value must be in the range [-32767..0].
 *
 * Functionally equivalent to:
 *
 * condition ? if1 : 0.
 *
 * Functionally equivalent to mbedtls_ct_error_if(condition, if1, 0) but
 * results in smaller code size.
 *
 * \param condition     Condition to test.
 * \param if1           Value to use if \p condition == MBEDTLS_CT_TRUE.
 *
 * \return  \c if1 if \p condition == MBEDTLS_CT_TRUE, otherwise 0.
 */
static inline int mbedtls_ct_error_if_else_0(mbedtls_ct_condition_t condition, int if1);

/* ============================================================================
 * Block memory operations
 */

#if defined(MBEDTLS_PKCS1_V15) && defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_RSA_ALT)

/** Conditionally set a block of memory to zero.
 *
 * Regardless of the condition, every byte will be read once and written to
 * once.
 *
 * \param condition     Secret. Condition to test.
 * \param buf           Secret. Pointer to the start of the buffer.
 * \param len           Number of bytes to set to zero.
 *
 * \warning Unlike mbedtls_platform_zeroize, this does not have the same guarantees
 * about not being optimised away if the memory is never read again.
 */
void mbedtls_ct_zeroize_if(mbedtls_ct_condition_t condition, void *buf, size_t len);

/** Shift some data towards the left inside a buffer.
 *
 * Functionally equivalent to:
 *
 * memmove(start, start + offset, total - offset);
 * memset(start + (total - offset), 0, offset);
 *
 * Timing independence comes at the expense of performance.
 *
 * \param start     Secret. Pointer to the start of the buffer.
 * \param total     Total size of the buffer.
 * \param offset    Secret. Offset from which to copy \p total - \p offset bytes.
 */
void mbedtls_ct_memmove_left(void *start,
                             size_t total,
                             size_t offset);

#endif /* defined(MBEDTLS_PKCS1_V15) && defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_RSA_ALT) */

/** Conditional memcpy.
 *
 * Functionally equivalent to:
 *
 * if (condition) {
 *      memcpy(dest, src1, len);
 * } else {
 *      if (src2 != NULL)
 *          memcpy(dest, src2, len);
 * }
 *
 * It will always read len bytes from src1.
 * If src2 != NULL, it will always read len bytes from src2.
 * If src2 == NULL, it will instead read len bytes from dest (as if src2 == dest).
 *
 * \param condition The condition
 * \param dest      Secret. Destination pointer.
 * \param src1      Secret. Pointer to copy from (if \p condition == MBEDTLS_CT_TRUE).
 *                  This may be equal to \p dest, but may not overlap in other ways.
 * \param src2      Secret (contents only - may branch to determine if this parameter is NULL).
 *                  Pointer to copy from (if \p condition == MBEDTLS_CT_FALSE and \p src2 is not NULL). May be NULL.
 *                  This may be equal to \p dest, but may not overlap it in other ways. It may overlap with \p src1.
 * \param len       Number of bytes to copy.
 */
void mbedtls_ct_memcpy_if(mbedtls_ct_condition_t condition,
                          unsigned char *dest,
                          const unsigned char *src1,
                          const unsigned char *src2,
                          size_t len
                          );

/** Copy data from a secret position.
 *
 * Functionally equivalent to:
 *
 * memcpy(dst, src + offset, len)
 *
 * This function copies \p len bytes from \p src + \p offset to
 * \p dst, with a code flow and memory access pattern that does not depend on
 * \p offset, but only on \p offset_min, \p offset_max and \p len.
 *
 * \note                This function reads from \p dest, but the value that
 *                      is read does not influence the result and this
 *                      function's behavior is well-defined regardless of the
 *                      contents of the buffers. This may result in false
 *                      positives from static or dynamic analyzers, especially
 *                      if \p dest is not initialized.
 *
 * \param dest          Secret. The destination buffer. This must point to a writable
 *                      buffer of at least \p len bytes.
 * \param src           Secret. The base of the source buffer. This must point to a
 *                      readable buffer of at least \p offset_max + \p len
 *                      bytes. Shouldn't overlap with \p dest
 * \param offset        Secret. The offset in the source buffer from which to copy.
 *                      This must be no less than \p offset_min and no greater
 *                      than \p offset_max.
 * \param offset_min    The minimal value of \p offset.
 * \param offset_max    The maximal value of \p offset.
 * \param len           The number of bytes to copy.
 */
void mbedtls_ct_memcpy_offset(unsigned char *dest,
                              const unsigned char *src,
                              size_t offset,
                              size_t offset_min,
                              size_t offset_max,
                              size_t len);

/* Documented in include/mbedtls/constant_time.h. a and b are secret.

   int mbedtls_ct_memcmp(const void *a,
                         const void *b,
                         size_t n);
 */

#if defined(MBEDTLS_NIST_KW_C)

/** Constant-time buffer comparison without branches.
 *
 * Similar to mbedtls_ct_memcmp, except that the result only depends on part of
 * the input data - differences in the head or tail are ignored. Functionally equivalent to:
 *
 * memcmp(a + skip_head, b + skip_head, size - skip_head - skip_tail)
 *
 * Time taken depends on \p n, but not on \p skip_head or \p skip_tail .
 *
 * Behaviour is undefined if ( \p skip_head + \p skip_tail) > \p n.
 *
 * \param a         Secret. Pointer to the first buffer, containing at least \p n bytes. May not be NULL.
 * \param b         Secret. Pointer to the second buffer, containing at least \p n bytes. May not be NULL.
 * \param n         The number of bytes to examine (total size of the buffers).
 * \param skip_head Secret. The number of bytes to treat as non-significant at the start of the buffer.
 *                  These bytes will still be read.
 * \param skip_tail Secret. The number of bytes to treat as non-significant at the end of the buffer.
 *                  These bytes will still be read.
 *
 * \return          Zero if the contents of the two buffers are the same, otherwise non-zero.
 */
int mbedtls_ct_memcmp_partial(const void *a,
                              const void *b,
                              size_t n,
                              size_t skip_head,
                              size_t skip_tail);

#endif

/* Include the implementation of static inline functions above. */
#include "constant_time_impl.h"

#endif /* MBEDTLS_CONSTANT_TIME_INTERNAL_H */

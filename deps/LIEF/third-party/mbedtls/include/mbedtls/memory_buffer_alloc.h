/**
 * \file memory_buffer_alloc.h
 *
 * \brief Buffer-based memory allocator
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_MEMORY_BUFFER_ALLOC_H
#define MBEDTLS_MEMORY_BUFFER_ALLOC_H

#include "mbedtls/build_info.h"

#include <stddef.h>

/**
 * \name SECTION: Module settings
 *
 * The configuration options you can set for this module are in this section.
 * Either change them in mbedtls_config.h or define them on the compiler command line.
 * \{
 */

#if !defined(MBEDTLS_MEMORY_ALIGN_MULTIPLE)
#define MBEDTLS_MEMORY_ALIGN_MULTIPLE       4 /**< Align on multiples of this value */
#endif

/** \} name SECTION: Module settings */

#define MBEDTLS_MEMORY_VERIFY_NONE         0
#define MBEDTLS_MEMORY_VERIFY_ALLOC        (1 << 0)
#define MBEDTLS_MEMORY_VERIFY_FREE         (1 << 1)
#define MBEDTLS_MEMORY_VERIFY_ALWAYS       (MBEDTLS_MEMORY_VERIFY_ALLOC | \
                                            MBEDTLS_MEMORY_VERIFY_FREE)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Initialize use of stack-based memory allocator.
 *          The stack-based allocator does memory management inside the
 *          presented buffer and does not call calloc() and free().
 *          It sets the global mbedtls_calloc() and mbedtls_free() pointers
 *          to its own functions.
 *          (Provided mbedtls_calloc() and mbedtls_free() are thread-safe if
 *           MBEDTLS_THREADING_C is defined)
 *
 * \note    This code is not optimized and provides a straight-forward
 *          implementation of a stack-based memory allocator.
 *
 * \param buf   buffer to use as heap
 * \param len   size of the buffer
 */
void mbedtls_memory_buffer_alloc_init(unsigned char *buf, size_t len);

/**
 * \brief   Free the mutex for thread-safety and clear remaining memory
 */
void mbedtls_memory_buffer_alloc_free(void);

/**
 * \brief   Determine when the allocator should automatically verify the state
 *          of the entire chain of headers / meta-data.
 *          (Default: MBEDTLS_MEMORY_VERIFY_NONE)
 *
 * \param verify    One of MBEDTLS_MEMORY_VERIFY_NONE, MBEDTLS_MEMORY_VERIFY_ALLOC,
 *                  MBEDTLS_MEMORY_VERIFY_FREE or MBEDTLS_MEMORY_VERIFY_ALWAYS
 */
void mbedtls_memory_buffer_set_verify(int verify);

#if defined(MBEDTLS_MEMORY_DEBUG)
/**
 * \brief   Print out the status of the allocated memory (primarily for use
 *          after a program should have de-allocated all memory)
 *          Prints out a list of 'still allocated' blocks and their stack
 *          trace if MBEDTLS_MEMORY_BACKTRACE is defined.
 */
void mbedtls_memory_buffer_alloc_status(void);

/**
 * \brief   Get the number of alloc/free so far.
 *
 * \param alloc_count   Number of allocations.
 * \param free_count    Number of frees.
 */
void mbedtls_memory_buffer_alloc_count_get(size_t *alloc_count, size_t *free_count);

/**
 * \brief   Get the peak heap usage so far
 *
 * \param max_used      Peak number of bytes in use or committed. This
 *                      includes bytes in allocated blocks too small to split
 *                      into smaller blocks but larger than the requested size.
 * \param max_blocks    Peak number of blocks in use, including free and used
 */
void mbedtls_memory_buffer_alloc_max_get(size_t *max_used, size_t *max_blocks);

/**
 * \brief   Reset peak statistics
 */
void mbedtls_memory_buffer_alloc_max_reset(void);

/**
 * \brief   Get the current heap usage
 *
 * \param cur_used      Current number of bytes in use or committed. This
 *                      includes bytes in allocated blocks too small to split
 *                      into smaller blocks but larger than the requested size.
 * \param cur_blocks    Current number of blocks in use, including free and used
 */
void mbedtls_memory_buffer_alloc_cur_get(size_t *cur_used, size_t *cur_blocks);
#endif /* MBEDTLS_MEMORY_DEBUG */

/**
 * \brief   Verifies that all headers in the memory buffer are correct
 *          and contain sane values. Helps debug buffer-overflow errors.
 *
 *          Prints out first failure if MBEDTLS_MEMORY_DEBUG is defined.
 *          Prints out full header information if MBEDTLS_MEMORY_DEBUG
 *          is defined. (Includes stack trace information for each block if
 *          MBEDTLS_MEMORY_BACKTRACE is defined as well).
 *
 * \return             0 if verified, 1 otherwise
 */
int mbedtls_memory_buffer_alloc_verify(void);

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if a test failed
 */
int mbedtls_memory_buffer_alloc_self_test(int verbose);
#endif

#ifdef __cplusplus
}
#endif

#endif /* memory_buffer_alloc.h */

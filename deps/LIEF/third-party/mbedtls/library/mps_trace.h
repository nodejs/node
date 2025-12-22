/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/**
 * \file mps_trace.h
 *
 * \brief Tracing module for MPS
 */

#ifndef MBEDTLS_MPS_MBEDTLS_MPS_TRACE_H
#define MBEDTLS_MPS_MBEDTLS_MPS_TRACE_H

#include "common.h"
#include "mps_common.h"
#include "mps_trace.h"

#include "mbedtls/platform.h"

#if defined(MBEDTLS_MPS_ENABLE_TRACE)

/*
 * Adapt this to enable/disable tracing output
 * from the various layers of the MPS.
 */

#define MBEDTLS_MPS_TRACE_ENABLE_LAYER_1
#define MBEDTLS_MPS_TRACE_ENABLE_LAYER_2
#define MBEDTLS_MPS_TRACE_ENABLE_LAYER_3
#define MBEDTLS_MPS_TRACE_ENABLE_LAYER_4
#define MBEDTLS_MPS_TRACE_ENABLE_READER
#define MBEDTLS_MPS_TRACE_ENABLE_WRITER

/*
 * To use the existing trace module, only change
 * MBEDTLS_MPS_TRACE_ENABLE_XXX above, but don't modify the
 * rest of this file.
 */

typedef enum {
    MBEDTLS_MPS_TRACE_TYPE_COMMENT,
    MBEDTLS_MPS_TRACE_TYPE_CALL,
    MBEDTLS_MPS_TRACE_TYPE_ERROR,
    MBEDTLS_MPS_TRACE_TYPE_RETURN
} mbedtls_mps_trace_type;

#define MBEDTLS_MPS_TRACE_BIT_LAYER_1 1
#define MBEDTLS_MPS_TRACE_BIT_LAYER_2 2
#define MBEDTLS_MPS_TRACE_BIT_LAYER_3 3
#define MBEDTLS_MPS_TRACE_BIT_LAYER_4 4
#define MBEDTLS_MPS_TRACE_BIT_WRITER  5
#define MBEDTLS_MPS_TRACE_BIT_READER  6

#if defined(MBEDTLS_MPS_TRACE_ENABLE_LAYER_1)
#define MBEDTLS_MPS_TRACE_MASK_LAYER_1 (1u << MBEDTLS_MPS_TRACE_BIT_LAYER_1)
#else
#define MBEDTLS_MPS_TRACE_MASK_LAYER_1 0
#endif

#if defined(MBEDTLS_MPS_TRACE_ENABLE_LAYER_2)
#define MBEDTLS_MPS_TRACE_MASK_LAYER_2 (1u << MBEDTLS_MPS_TRACE_BIT_LAYER_2)
#else
#define MBEDTLS_MPS_TRACE_MASK_LAYER_2 0
#endif

#if defined(MBEDTLS_MPS_TRACE_ENABLE_LAYER_3)
#define MBEDTLS_MPS_TRACE_MASK_LAYER_3 (1u << MBEDTLS_MPS_TRACE_BIT_LAYER_3)
#else
#define MBEDTLS_MPS_TRACE_MASK_LAYER_3 0
#endif

#if defined(MBEDTLS_MPS_TRACE_ENABLE_LAYER_4)
#define MBEDTLS_MPS_TRACE_MASK_LAYER_4 (1u << MBEDTLS_MPS_TRACE_BIT_LAYER_4)
#else
#define MBEDTLS_MPS_TRACE_MASK_LAYER_4 0
#endif

#if defined(MBEDTLS_MPS_TRACE_ENABLE_READER)
#define MBEDTLS_MPS_TRACE_MASK_READER (1u << MBEDTLS_MPS_TRACE_BIT_READER)
#else
#define MBEDTLS_MPS_TRACE_MASK_READER 0
#endif

#if defined(MBEDTLS_MPS_TRACE_ENABLE_WRITER)
#define MBEDTLS_MPS_TRACE_MASK_WRITER (1u << MBEDTLS_MPS_TRACE_BIT_WRITER)
#else
#define MBEDTLS_MPS_TRACE_MASK_WRITER 0
#endif

#define MBEDTLS_MPS_TRACE_MASK (MBEDTLS_MPS_TRACE_MASK_LAYER_1 |       \
                                MBEDTLS_MPS_TRACE_MASK_LAYER_2 |       \
                                MBEDTLS_MPS_TRACE_MASK_LAYER_3 |       \
                                MBEDTLS_MPS_TRACE_MASK_LAYER_4 |       \
                                MBEDTLS_MPS_TRACE_MASK_READER  |       \
                                MBEDTLS_MPS_TRACE_MASK_WRITER)

/* We have to avoid globals because E-ACSL chokes on them...
 * Wrap everything in stub functions. */
int  mbedtls_mps_trace_get_depth(void);
void mbedtls_mps_trace_inc_depth(void);
void mbedtls_mps_trace_dec_depth(void);

void mbedtls_mps_trace_color(int id);
void mbedtls_mps_trace_indent(int level, mbedtls_mps_trace_type ty);

void mbedtls_mps_trace_print_msg(int id, int line, const char *format, ...);

#define MBEDTLS_MPS_TRACE(type, ...)                                              \
    do {                                                                            \
        if (!(MBEDTLS_MPS_TRACE_MASK & (1u << mbedtls_mps_trace_id)))         \
        break;                                                                  \
        mbedtls_mps_trace_indent(mbedtls_mps_trace_get_depth(), type);            \
        mbedtls_mps_trace_color(mbedtls_mps_trace_id);                            \
        mbedtls_mps_trace_print_msg(mbedtls_mps_trace_id, __LINE__, __VA_ARGS__); \
        mbedtls_mps_trace_color(0);                                               \
    } while (0)

#define MBEDTLS_MPS_TRACE_INIT(...)                                         \
    do {                                                                      \
        if (!(MBEDTLS_MPS_TRACE_MASK & (1u << mbedtls_mps_trace_id)))   \
        break;                                                            \
        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_CALL, __VA_ARGS__);        \
        mbedtls_mps_trace_inc_depth();                                        \
    } while (0)

#define MBEDTLS_MPS_TRACE_END(val)                                        \
    do {                                                                    \
        if (!(MBEDTLS_MPS_TRACE_MASK & (1u << mbedtls_mps_trace_id))) \
        break;                                                          \
        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_RETURN, "%d (-%#04x)",    \
                          (int) (val), -((unsigned) (val)));                           \
        mbedtls_mps_trace_dec_depth();                                      \
    } while (0)

#define MBEDTLS_MPS_TRACE_RETURN(val)         \
    do {                                        \
        /* Breaks tail recursion. */            \
        int ret__ = val;                        \
        MBEDTLS_MPS_TRACE_END(ret__);         \
        return ret__;                        \
    } while (0)

#else /* MBEDTLS_MPS_TRACE */

#define MBEDTLS_MPS_TRACE(type, ...) do { } while (0)
#define MBEDTLS_MPS_TRACE_INIT(...)  do { } while (0)
#define MBEDTLS_MPS_TRACE_END          do { } while (0)

#define MBEDTLS_MPS_TRACE_RETURN(val) return val;

#endif /* MBEDTLS_MPS_TRACE */

#endif /* MBEDTLS_MPS_MBEDTLS_MPS_TRACE_H */

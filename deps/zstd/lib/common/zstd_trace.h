/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_TRACE_H
#define ZSTD_TRACE_H

#include <stddef.h>

/* weak symbol support
 * For now, enable conservatively:
 * - Only GNUC
 * - Only ELF
 * - Only x86-64, i386, aarch64 and risc-v.
 * Also, explicitly disable on platforms known not to work so they aren't
 * forgotten in the future.
 */
#if !defined(ZSTD_HAVE_WEAK_SYMBOLS) && \
    defined(__GNUC__) && defined(__ELF__) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
     defined(_M_IX86) || defined(__aarch64__) || defined(__riscv)) && \
    !defined(__APPLE__) && !defined(_WIN32) && !defined(__MINGW32__) && \
    !defined(__CYGWIN__) && !defined(_AIX)
#  define ZSTD_HAVE_WEAK_SYMBOLS 1
#else
#  define ZSTD_HAVE_WEAK_SYMBOLS 0
#endif
#if ZSTD_HAVE_WEAK_SYMBOLS
#  define ZSTD_WEAK_ATTR __attribute__((__weak__))
#else
#  define ZSTD_WEAK_ATTR
#endif

/* Only enable tracing when weak symbols are available. */
#ifndef ZSTD_TRACE
#  define ZSTD_TRACE ZSTD_HAVE_WEAK_SYMBOLS
#endif

#if ZSTD_TRACE

struct ZSTD_CCtx_s;
struct ZSTD_DCtx_s;
struct ZSTD_CCtx_params_s;

typedef struct {
    /**
     * ZSTD_VERSION_NUMBER
     *
     * This is guaranteed to be the first member of ZSTD_trace.
     * Otherwise, this struct is not stable between versions. If
     * the version number does not match your expectation, you
     * should not interpret the rest of the struct.
     */
    unsigned version;
    /**
     * Non-zero if streaming (de)compression is used.
     */
    int streaming;
    /**
     * The dictionary ID.
     */
    unsigned dictionaryID;
    /**
     * Is the dictionary cold?
     * Only set on decompression.
     */
    int dictionaryIsCold;
    /**
     * The dictionary size or zero if no dictionary.
     */
    size_t dictionarySize;
    /**
     * The uncompressed size of the data.
     */
    size_t uncompressedSize;
    /**
     * The compressed size of the data.
     */
    size_t compressedSize;
    /**
     * The fully resolved CCtx parameters (NULL on decompression).
     */
    struct ZSTD_CCtx_params_s const* params;
    /**
     * The ZSTD_CCtx pointer (NULL on decompression).
     */
    struct ZSTD_CCtx_s const* cctx;
    /**
     * The ZSTD_DCtx pointer (NULL on compression).
     */
    struct ZSTD_DCtx_s const* dctx;
} ZSTD_Trace;

/**
 * A tracing context. It must be 0 when tracing is disabled.
 * Otherwise, any non-zero value returned by a tracing begin()
 * function is presented to any subsequent calls to end().
 *
 * Any non-zero value is treated as tracing is enabled and not
 * interpreted by the library.
 *
 * Two possible uses are:
 * * A timestamp for when the begin() function was called.
 * * A unique key identifying the (de)compression, like the
 *   address of the [dc]ctx pointer if you need to track
 *   more information than just a timestamp.
 */
typedef unsigned long long ZSTD_TraceCtx;

/**
 * Trace the beginning of a compression call.
 * @param cctx The dctx pointer for the compression.
 *             It can be used as a key to map begin() to end().
 * @returns Non-zero if tracing is enabled. The return value is
 *          passed to ZSTD_trace_compress_end().
 */
ZSTD_WEAK_ATTR ZSTD_TraceCtx ZSTD_trace_compress_begin(
    struct ZSTD_CCtx_s const* cctx);

/**
 * Trace the end of a compression call.
 * @param ctx The return value of ZSTD_trace_compress_begin().
 * @param trace The zstd tracing info.
 */
ZSTD_WEAK_ATTR void ZSTD_trace_compress_end(
    ZSTD_TraceCtx ctx,
    ZSTD_Trace const* trace);

/**
 * Trace the beginning of a decompression call.
 * @param dctx The dctx pointer for the decompression.
 *             It can be used as a key to map begin() to end().
 * @returns Non-zero if tracing is enabled. The return value is
 *          passed to ZSTD_trace_compress_end().
 */
ZSTD_WEAK_ATTR ZSTD_TraceCtx ZSTD_trace_decompress_begin(
    struct ZSTD_DCtx_s const* dctx);

/**
 * Trace the end of a decompression call.
 * @param ctx The return value of ZSTD_trace_decompress_begin().
 * @param trace The zstd tracing info.
 */
ZSTD_WEAK_ATTR void ZSTD_trace_decompress_end(
    ZSTD_TraceCtx ctx,
    ZSTD_Trace const* trace);

#endif /* ZSTD_TRACE */

#endif /* ZSTD_TRACE_H */

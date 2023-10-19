/* crc32_simd.h
 *
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the Chromium source repository LICENSE file.
 */

#include <stdint.h>

#include "zconf.h"
#include "zutil.h"
#include "deflate.h"

/*
 * crc32_sse42_simd_(): compute the crc32 of the buffer, where the buffer
 * length must be at least 64, and a multiple of 16.
 */
uint32_t ZLIB_INTERNAL crc32_sse42_simd_(const unsigned char* buf,
                                         z_size_t len,
                                         uint32_t crc);

uint32_t ZLIB_INTERNAL crc32_avx512_simd_(const unsigned char* buf,
                                          z_size_t len,
                                          uint32_t crc);

/*
 * crc32_sse42_simd_ buffer size constraints: see the use in zlib/crc32.c
 * for computing the crc32 of an arbitrary length buffer.
 */
#define Z_CRC32_SSE42_MINIMUM_LENGTH 64
#define Z_CRC32_SSE42_CHUNKSIZE_MASK 15
#define Z_CRC32_AVX512_MINIMUM_LENGTH 256
#define Z_CRC32_AVX512_CHUNKSIZE_MASK 63

/*
 * CRC32 checksums using ARMv8-a crypto instructions.
 */
uint32_t ZLIB_INTERNAL armv8_crc32_little(const unsigned char* buf,
                                          z_size_t len,
                                          uint32_t crc);

/* aarch64 specific code. */
#if defined(__aarch64__)

/* 128 is the sweet spot at the time of coding (late 2020). */
#define Z_CRC32_PMULL_MINIMUM_LENGTH 128
#define Z_CRC32_PMULL_CHUNKSIZE_MASK 15

/*
 * CRC32 checksums using ARMv8-a PMULL instructions, where the buffer
 * length must be at least 64, and a multiple of 16.
 */
uint32_t ZLIB_INTERNAL armv8_crc32_pmull_little(const unsigned char* buf,
                                                z_size_t len,
                                                uint32_t crc);

#endif

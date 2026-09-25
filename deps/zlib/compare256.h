/* compare256.h
 *
 * This does 256-byte match comparison for deflate's longest_match.
 *
 * Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the Chromium source repository LICENSE file.
 */
#ifndef COMPARE256_H
#define COMPARE256_H

#include <stdint.h>
#include <string.h>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

/* Safe unaligned 16-bit load. Compilers optimize this into a single load
 * instruction. */
static inline uint16_t read16(const void* p) {
    uint16_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

/* Finds the byte offset (0..7) of the first difference in non-zero XOR mask
 * `x`:
 */
static inline int compare256_diff(uint64_t x) {
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long i;
    _BitScanForward64(&i, x);
    return (int)i / 8;
#else
    return __builtin_ctzll(x) / 8;
#endif
}

/* Returns the number of matching leading bytes (0 to 256) between src0 and
 * src1. Compares 8 bytes per iteration with early exit. */
static inline int compare256(const unsigned char* src0,
                             const unsigned char* src1) {
    int len = 0;
    do {
        uint64_t a, b, x;
        memcpy(&a, src0 + len, sizeof(a));
        memcpy(&b, src1 + len, sizeof(b));
        x = a ^ b;
        if (x)
            return len + compare256_diff(x);
        len += (int)sizeof(a);
    } while (len < 256);
    return 256;
}

#endif /* COMPARE256_H */

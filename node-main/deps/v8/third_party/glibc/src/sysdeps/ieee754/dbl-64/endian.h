// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// glibc has a couple of endian.h files. This defines the macros expected by
// the code in this directory using macros defined by clang.
#if (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BIG_ENDI 1
#undef LITTLE_ENDI
#define HIGH_HALF 0
#define  LOW_HALF 1
#elif (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#undef BIG_ENDI
#define LITTLE_ENDI 1
#define HIGH_HALF 1
#define  LOW_HALF 0
#else
#error
#endif

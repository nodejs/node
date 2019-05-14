// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AddressSanitizer support.

#ifndef V8_ASAN_H_
#define V8_ASAN_H_

#include "src/base/macros.h"
#include "src/globals.h"

#ifdef V8_USE_ADDRESS_SANITIZER

#include <sanitizer/asan_interface.h>

#else  // !V8_USE_ADDRESS_SANITIZER

#define ASAN_POISON_MEMORY_REGION(start, size)                          \
  static_assert(std::is_pointer<decltype(start)>::value &&              \
                    std::is_convertible<decltype(size), size_t>::value, \
                "static type violation")
#define ASAN_UNPOISON_MEMORY_REGION(start, size) \
  ASAN_POISON_MEMORY_REGION(start, size)

#endif  // V8_USE_ADDRESS_SANITIZER

#endif  // V8_ASAN_H_

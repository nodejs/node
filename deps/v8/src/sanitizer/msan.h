// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemorySanitizer support.

#ifndef V8_SANITIZER_MSAN_H_
#define V8_SANITIZER_MSAN_H_

#include "src/base/macros.h"
#include "src/common/globals.h"

#ifdef V8_USE_MEMORY_SANITIZER

#include <sanitizer/msan_interface.h>

// Marks a memory range as uninitialized, as if it was allocated here.
#define MSAN_ALLOCATED_UNINITIALIZED_MEMORY(p, s) \
  __msan_allocated_memory(reinterpret_cast<const void*>(p), (s))
// Marks a memory range as initialized.
#define MSAN_MEMORY_IS_INITIALIZED(p, s) \
  __msan_unpoison(reinterpret_cast<const void*>(p), (s))

#else  // !V8_USE_MEMORY_SANITIZER

#define MSAN_ALLOCATED_UNINITIALIZED_MEMORY(p, s)                            \
  static_assert((std::is_pointer<decltype(p)>::value ||                      \
                 std::is_same<v8::internal::Address, decltype(p)>::value) && \
                    std::is_convertible<decltype(s), size_t>::value,         \
                "static type violation")
#define MSAN_MEMORY_IS_INITIALIZED(p, s) \
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(p, s)

#endif  // V8_USE_MEMORY_SANITIZER

#endif  // V8_SANITIZER_MSAN_H_

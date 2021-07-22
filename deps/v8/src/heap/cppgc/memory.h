// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MEMORY_H_
#define V8_HEAP_CPPGC_MEMORY_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/base/macros.h"
#include "src/base/sanitizer/asan.h"
#include "src/base/sanitizer/msan.h"

namespace cppgc {
namespace internal {

V8_NOINLINE DISABLE_ASAN void NoSanitizeMemset(void* address, char c,
                                               size_t bytes);

inline void ZapMemory(void* address, size_t size) {
  // The lowest bit of the zapped value should be 0 so that zapped object are
  // never viewed as fully constructed objects.
  static constexpr uint8_t kZappedValue = 0xdc;
  memset(address, kZappedValue, size);
}

// Together `SetMemoryAccessible()` and `SetMemoryInaccessible()` form the
// memory access model for allocation and free.
V8_INLINE void SetMemoryAccessible(void* address, size_t size) {
#if defined(V8_USE_MEMORY_SANITIZER)

  MSAN_MEMORY_IS_INITIALIZED(address, size);

#elif defined(V8_USE_ADDRESS_SANITIZER)

  ASAN_UNPOISON_MEMORY_REGION(address, size);

#elif DEBUG

  memset(address, 0, size);

#else  // Release builds.

  // Nothing to be done for release builds.

#endif  // Release builds.
}

V8_INLINE void SetMemoryInaccessible(void* address, size_t size) {
#if defined(V8_USE_MEMORY_SANITIZER)

  memset(address, 0, size);
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(address, size);

#elif defined(V8_USE_ADDRESS_SANITIZER)

  NoSanitizeMemset(address, 0, size);
  ASAN_POISON_MEMORY_REGION(address, size);

#elif DEBUG

  ::cppgc::internal::ZapMemory(address, size);

#else  // Release builds.

  memset(address, 0, size);

#endif  // Release builds.
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MEMORY_H_

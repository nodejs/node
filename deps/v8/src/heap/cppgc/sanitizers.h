// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_SANITIZERS_H_
#define V8_HEAP_CPPGC_SANITIZERS_H_

#include <stdint.h>
#include <string.h>

#include "src/base/macros.h"

//
// TODO(chromium:1056170): Find a place in base for sanitizer support.
//

#ifdef V8_USE_ADDRESS_SANITIZER

#include <sanitizer/asan_interface.h>

#define NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#if !defined(ASAN_POISON_MEMORY_REGION) || !defined(ASAN_UNPOISON_MEMORY_REGION)
#error "ASAN_POISON_MEMORY_REGION must be defined"
#endif

#else  // !V8_USE_ADDRESS_SANITIZER

#define NO_SANITIZE_ADDRESS
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))

#endif  // V8_USE_ADDRESS_SANITIZER

#ifdef V8_USE_MEMORY_SANITIZER

#include <sanitizer/msan_interface.h>

#define MSAN_POISON(addr, size) __msan_allocated_memory(addr, size)
#define MSAN_UNPOISON(addr, size) __msan_unpoison(addr, size)

#else  // !V8_USE_MEMORY_SANITIZER

#define MSAN_POISON(addr, size) ((void)(addr), (void)(size))
#define MSAN_UNPOISON(addr, size) ((void)(addr), (void)(size))

#endif  // V8_USE_MEMORY_SANITIZER

// API for newly allocated or reclaimed memory.
#if defined(V8_USE_MEMORY_SANITIZER)
#define SET_MEMORY_ACCESSIBLE(address, size) MSAN_UNPOISON(address, size);
#define SET_MEMORY_INACCESSIBLE(address, size) \
  memset((address), 0, (size));                \
  MSAN_POISON((address), (size))
#elif defined(V8_USE_ADDRESS_SANITIZER)
#define SET_MEMORY_ACCESSIBLE(address, size) \
  ASAN_UNPOISON_MEMORY_REGION(address, size);
#define SET_MEMORY_INACCESSIBLE(address, size) \
  memset((address), 0, (size));                \
  ASAN_POISON_MEMORY_REGION(address, size)
#elif DEBUG
#define SET_MEMORY_ACCESSIBLE(address, size) memset((address), 0, (size))
#define SET_MEMORY_INACCESSIBLE(address, size) \
  ::cppgc::internal::ZapMemory((address), (size));
#else
#define SET_MEMORY_ACCESSIBLE(address, size) ((void)(address), (void)(size))
#define SET_MEMORY_INACCESSIBLE(address, size) memset((address), 0, (size))
#endif

namespace cppgc {
namespace internal {

inline void ZapMemory(void* address, size_t size) {
  // The lowest bit of the zapped value should be 0 so that zapped object
  // are never viewed as fully constructed objects.
  static constexpr uint8_t kZappedValue = 0xdc;
  memset(address, kZappedValue, size);
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_SANITIZERS_H_

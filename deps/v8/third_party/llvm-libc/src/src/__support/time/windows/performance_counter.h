//===--- Cached Performance Counter Frequency  ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/atomic.h"
#include "src/__support/common.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace LIBC_NAMESPACE_DECL {
namespace performance_counter {
LIBC_INLINE long long get_ticks_per_second() {
  static cpp::Atomic<long long> frequency = 0;
  // Relaxed ordering is enough. It is okay to record the frequency multiple
  // times. The store operation itself is atomic and the value must propagate
  // as required by cache coherence.
  auto freq = frequency.load(cpp::MemoryOrder::RELAXED);
  if (!freq) {
    [[clang::uninitialized]] LARGE_INTEGER buffer;
    // On systems that run Windows XP or later, the function will always
    // succeed and will thus never return zero.
    ::QueryPerformanceFrequency(&buffer);
    frequency.store(buffer.QuadPart, cpp::MemoryOrder::RELAXED);
    return buffer.QuadPart;
  }
  return freq;
}
} // namespace performance_counter
} // namespace LIBC_NAMESPACE_DECL

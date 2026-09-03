//===-- A lock-free fixed capacity buffer for caching values ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_GPU_FIXEDBUFFER_H
#define LLVM_LIBC_SRC___SUPPORT_GPU_FIXEDBUFFER_H

#include "src/__support/CPP/atomic.h"

#include <stdint.h>

namespace LIBC_NAMESPACE_DECL {

// A lock-free fixed capacity buffer for caching pointer-like values. Uses a
// flat array where a null/zero value indicates an empty slot.
template <typename T, uint32_t CAPACITY> struct alignas(16) FixedBuffer {
  T slots[CAPACITY] = {};

  LIBC_INLINE bool push(const T &val) {
    for (uint32_t i = 0; i < CAPACITY; ++i) {
      cpp::AtomicRef<T> slot(slots[i]);
      if (slot.load(cpp::MemoryOrder::RELAXED) != T{})
        continue;
      T expected{};
      if (slot.compare_exchange_strong(expected, val, cpp::MemoryOrder::RELEASE,
                                       cpp::MemoryOrder::RELAXED))
        return true;
    }
    return false;
  }

  LIBC_INLINE bool pop(T &val) {
    for (uint32_t i = 0; i < CAPACITY; ++i) {
      cpp::AtomicRef<T> slot(slots[i]);
      if (slot.load(cpp::MemoryOrder::RELAXED) == T{})
        continue;
      val = slot.exchange(T{}, cpp::MemoryOrder::ACQUIRE);
      if (val != T{})
        return true;
    }
    return false;
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_GPU_FIXEDBUFFER_H

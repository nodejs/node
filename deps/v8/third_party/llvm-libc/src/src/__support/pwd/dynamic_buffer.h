//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Growable byte buffer with explicit lifetime management.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PWD_DYNAMIC_BUFFER_H
#define LLVM_LIBC_SRC___SUPPORT_PWD_DYNAMIC_BUFFER_H

#include "hdr/func/free.h"
#include "hdr/func/realloc.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/type_traits/is_trivially_destructible.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace pwd {

// A heap-backed, growable byte buffer for the flat-file database engine.
//
// This type is trivially destructible by design, so an instance can be a
// constant-initialised process global. The owner releases the storage
// explicitly when done.
class DynamicBuffer {
  static constexpr size_t INITIAL_CAPACITY = 1024;

  char *ptr = nullptr;
  size_t cap = 0;

public:
  LIBC_INLINE constexpr DynamicBuffer() = default;

  DynamicBuffer(const DynamicBuffer &) = delete;
  DynamicBuffer &operator=(const DynamicBuffer &) = delete;

  // Grows the buffer to hold at least new_capacity bytes, preserving the
  // existing contents. Capacity doubles so that repeated growth stays linear
  // in the number of bytes read. Returns false if allocation failed, in which
  // case the buffer is left untouched.
  [[nodiscard]] LIBC_INLINE bool reserve(size_t new_capacity) {
    if (new_capacity <= cap)
      return true;

    size_t next = cap == 0 ? INITIAL_CAPACITY : cap;
    while (next < new_capacity) {
      if (next > cpp::numeric_limits<size_t>::max() / 2)
        return false;
      next *= 2;
    }

    void *new_ptr = ::realloc(ptr, next);
    if (new_ptr == nullptr)
      return false;

    ptr = static_cast<char *>(new_ptr);
    cap = next;
    return true;
  }

  // Doubles the current capacity, or allocates the initial capacity when the
  // buffer is empty. Returns false if allocation failed.
  [[nodiscard]] LIBC_INLINE bool grow() {
    if (cap == cpp::numeric_limits<size_t>::max())
      return false;
    return reserve(cap == 0 ? INITIAL_CAPACITY : cap + 1);
  }

  // Frees the storage and returns the buffer to its empty state. Safe to call
  // more than once.
  LIBC_INLINE void release() {
    ::free(ptr);
    ptr = nullptr;
    cap = 0;
  }

  [[nodiscard]] LIBC_INLINE cpp::span<char> span() const { return {ptr, cap}; }
  [[nodiscard]] LIBC_INLINE size_t capacity() const { return cap; }
};

static_assert(cpp::is_trivially_destructible<DynamicBuffer>::value,
              "DynamicBuffer must be trivially destructible");

// RAII wrapper around DynamicBuffer for stack-local buffers.
class ScopedDynamicBuffer : public DynamicBuffer {
public:
  using DynamicBuffer::DynamicBuffer;

  LIBC_INLINE ~ScopedDynamicBuffer() { this->release(); }
};

} // namespace pwd
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PWD_DYNAMIC_BUFFER_H

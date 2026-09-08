//===-- Memory Size ---------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MEMORY_SIZE_H
#define LLVM_LIBC_SRC___SUPPORT_MEMORY_SIZE_H

#include "src/__support/CPP/bit.h" // has_single_bit
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math_extras.h"
#include "src/string/memory_utils/utils.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// Limit memory size to the max of ssize_t
class SafeMemSize {
private:
  using type = cpp::make_signed_t<size_t>;
  type value;
  LIBC_INLINE explicit SafeMemSize(type value) : value(value) {}

public:
  LIBC_INLINE_VAR static constexpr size_t MAX_MEM_SIZE =
      static_cast<size_t>(cpp::numeric_limits<type>::max());

  LIBC_INLINE explicit SafeMemSize(size_t value)
      : value(value <= MAX_MEM_SIZE ? static_cast<type>(value) : -1) {}

  LIBC_INLINE static constexpr size_t offset_to(size_t val, size_t align) {
    return (-val) & (align - 1);
  }

  LIBC_INLINE operator size_t() { return static_cast<size_t>(value); }

  LIBC_INLINE bool valid() { return value >= 0; }

  LIBC_INLINE SafeMemSize operator+(const SafeMemSize &other) {
    type result;
    if (LIBC_UNLIKELY((value | other.value) < 0)) {
      result = -1;
    } else {
      result = value + other.value;
    }
    return SafeMemSize{result};
  }

  LIBC_INLINE SafeMemSize operator*(const SafeMemSize &other) {
    type result;
    if (LIBC_UNLIKELY((value | other.value) < 0))
      result = -1;
    if (LIBC_UNLIKELY(mul_overflow(value, other.value, result)))
      result = -1;
    return SafeMemSize{result};
  }

  LIBC_INLINE SafeMemSize align_up(size_t alignment) {
    if (!cpp::has_single_bit(alignment) || alignment > MAX_MEM_SIZE || !valid())
      return SafeMemSize{type{-1}};

    type offset =
        static_cast<type>(offset_to(static_cast<size_t>(value), alignment));

    if (LIBC_UNLIKELY(offset > static_cast<type>(MAX_MEM_SIZE) - value))
      return SafeMemSize{type{-1}};

    return SafeMemSize{value + offset};
  }
};
} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MEMORY_SIZE_H

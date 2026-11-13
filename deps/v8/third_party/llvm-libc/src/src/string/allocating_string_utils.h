//===-- Allocating string utils ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_ALLOCATING_STRING_UTILS_H
#define LLVM_LIBC_SRC_STRING_ALLOCATING_STRING_UTILS_H

#include "src/__support/CPP/new.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/macros/config.h" // LIBC_NAMESPACE_DECL
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/string_utils.h"

#include <stddef.h> // For size_t

namespace LIBC_NAMESPACE_DECL {
namespace internal {

template <typename T> LIBC_INLINE cpp::optional<T *> strdup(const T *src) {
  if (src == nullptr)
    return cpp::nullopt;
  size_t len = string_length(src) + 1;
  AllocChecker ac;
  T *newstr = new (ac) T[len];
  if (!ac)
    return cpp::nullopt;
  inline_memcpy(newstr, src, len * sizeof(T));
  return newstr;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_ALLOCATING_STRING_UTILS_H

//===-- Libc specific custom operator new and delete ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_ALLOC_CHECKER_H
#define LLVM_LIBC_SRC___SUPPORT_ALLOC_CHECKER_H

#include "hdr/func/aligned_alloc.h"
#include "hdr/func/malloc.h"
#include "src/__support/CPP/new.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/os.h"

namespace LIBC_NAMESPACE_DECL {

class AllocChecker {
  bool success = false;

  LIBC_INLINE AllocChecker &operator=(bool status) {
    success = status;
    return *this;
  }

public:
  LIBC_INLINE AllocChecker() = default;

  LIBC_INLINE operator bool() const { return success; }

  LIBC_INLINE static void *alloc(size_t s, AllocChecker &ac) {
    void *mem = ::malloc(s);
    ac = (mem != nullptr);
    return mem;
  }

  LIBC_INLINE static void *aligned_alloc(size_t s, std::align_val_t align,
                                         AllocChecker &ac) {
#ifdef LIBC_TARGET_OS_IS_WINDOWS
    // std::aligned_alloc is not available on Windows because std::free on
    // Windows cannot deallocate any over-aligned memory. Microsoft provides an
    // alternative for std::aligned_alloc named _aligned_malloc, but it must be
    // paired with _aligned_free instead of std::free.
    void *mem = ::_aligned_malloc(static_cast<size_t>(align), s);
#else
    void *mem = ::aligned_alloc(static_cast<size_t>(align), s);
#endif
    ac = (mem != nullptr);
    return mem;
  }
};

} // namespace LIBC_NAMESPACE_DECL

LIBC_INLINE void *operator new(size_t size,
                               LIBC_NAMESPACE::AllocChecker &ac) noexcept {
  return LIBC_NAMESPACE::AllocChecker::alloc(size, ac);
}

LIBC_INLINE void *operator new(size_t size, std::align_val_t align,
                               LIBC_NAMESPACE::AllocChecker &ac) noexcept {
  return LIBC_NAMESPACE::AllocChecker::aligned_alloc(size, align, ac);
}

LIBC_INLINE void *operator new[](size_t size,
                                 LIBC_NAMESPACE::AllocChecker &ac) noexcept {
  return LIBC_NAMESPACE::AllocChecker::alloc(size, ac);
}

LIBC_INLINE void *operator new[](size_t size, std::align_val_t align,
                                 LIBC_NAMESPACE::AllocChecker &ac) noexcept {
  return LIBC_NAMESPACE::AllocChecker::aligned_alloc(size, align, ac);
}

#endif // LLVM_LIBC_SRC___SUPPORT_ALLOC_CHECKER_H

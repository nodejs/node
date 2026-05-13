//===-- Convenient sanitizer macros -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_SANITIZER_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_SANITIZER_H

#include "src/__support/macros/config.h" //LIBC_HAS_FEATURE

//-----------------------------------------------------------------------------
// Functions to unpoison memory
//-----------------------------------------------------------------------------

#if LIBC_HAS_FEATURE(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#define LIBC_HAS_ADDRESS_SANITIZER
#endif

#if LIBC_HAS_FEATURE(memory_sanitizer)
#define LIBC_HAS_MEMORY_SANITIZER
#endif

#ifdef LIBC_HAS_MEMORY_SANITIZER
// Only perform MSAN unpoison in non-constexpr context.
#include <sanitizer/msan_interface.h>
#define MSAN_UNPOISON(addr, size)                                              \
  do {                                                                         \
    if (!__builtin_is_constant_evaluated())                                    \
      __msan_unpoison(addr, size);                                             \
  } while (0)
#else
#define MSAN_UNPOISON(ptr, size)
#endif

#ifdef LIBC_HAS_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#define ASAN_POISON_MEMORY_REGION(addr, size)                                  \
  __asan_poison_memory_region((addr), (size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size)                                \
  __asan_unpoison_memory_region((addr), (size))
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_SANITIZER_H

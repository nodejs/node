//===-- Common internal contructs -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_COMMON_H
#define LLVM_LIBC_SRC___SUPPORT_COMMON_H

#ifndef LIBC_NAMESPACE
#error "LIBC_NAMESPACE macro is not defined."
#endif

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/hardening.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/os.h"

#if defined(__APPLE__) || (defined(LIBC_TARGET_OS_IS_WINDOWS) &&               \
                           defined(LIBC_TARGET_ARCH_IS_X86_32))
#define LIBC_TARGET_USES_LEADING_UNDERSCORE
#endif

#ifndef LLVM_LIBC_FUNCTION_ATTR
#define LLVM_LIBC_FUNCTION_ATTR
#endif

#ifndef LLVM_LIBC_VARIABLE_ATTR
#define LLVM_LIBC_VARIABLE_ATTR
#endif

// clang-format off
// Allow each function `func` to have extra attributes specified by defining:
// `LLVM_LIBC_FUNCTION_ATTR_func` macro, which should always start with
// "LLVM_LIBC_EMPTY, "
//
// For examples:
// #define LLVM_LIBC_FUNCTION_ATTR_memcpy LLVM_LIBC_EMPTY, [[gnu::weak]]
// #define LLVM_LIBC_FUNCTION_ATTR_memchr LLVM_LIBC_EMPTY, [[gnu::weak]] [[gnu::visibility("default")]]
// clang-format on
#define LLVM_LIBC_EMPTY

#define GET_NOTHING(...) 0
#define GET_SECOND(first, second, ...) second
#define GET_FIFTH(first, second, third, fourth, fifth, ...) fifth
#define EXPAND_THEN_SECOND(name) GET_SECOND(name, LLVM_LIBC_EMPTY)

#define LLVM_LIBC_ATTR(name) EXPAND_THEN_SECOND(LLVM_LIBC_FUNCTION_ATTR_##name)

// Export both `func` and `LIBC_NAMESPACE::func` using an alias symbol.
// This does not work on platfors with LIBC_TARGET_USES_LEADING_UNDERSCORE
// so there this only exports `_func`.
#if defined(LIBC_COPT_PUBLIC_PACKAGING) && !defined(LIBC_COMPILER_IS_MSVC)
#ifndef LIBC_TARGET_USES_LEADING_UNDERSCORE
#define LLVM_LIBC_FUNCTION_IMPL_4(type, name, arglist, c_alias)                \
  LLVM_LIBC_ATTR(name)                                                         \
  LLVM_LIBC_FUNCTION_ATTR decltype(LIBC_NAMESPACE::name)                       \
      __##name##_impl__ asm(c_alias);                                          \
  decltype(LIBC_NAMESPACE::name) name [[gnu::alias(c_alias)]];                 \
  type __##name##_impl__ arglist

#define LLVM_LIBC_ADD_FUNCTION_C_ALIAS(name, c_alias)                          \
  extern "C" decltype(LIBC_NAMESPACE::name) c_alias [[gnu::alias(#name)]]

#else // LIBC_TARGET_USES_LEADING_UNDERSCORE
#define LLVM_LIBC_FUNCTION_IMPL_4(type, name, arglist, c_alias)                \
  LLVM_LIBC_ATTR(name)                                                         \
  LLVM_LIBC_FUNCTION_ATTR decltype(LIBC_NAMESPACE::name) name asm(             \
      "_" c_alias);                                                            \
  type name arglist

// clang-format off
#define LLVM_LIBC_ADD_FUNCTION_C_ALIAS(name, c_alias)                          \
  asm(".text\n"                                                                \
      ".global " "_" #c_alias "\n"                                             \
      ".type " "_" #c_alias ", @function\n"                                    \
      "_" #c_alias " = " "_" #name "\n")
// clang-format on
#endif // LIBC_TARGET_USES_LEADING_UNDERSCORE

#else  // LIBC_COPT_PUBLIC_PACKAGING
#define LLVM_LIBC_FUNCTION_IMPL_4(type, name, arglist, c_alias)                \
  type name arglist

#define LLVM_LIBC_ADD_FUNCTION_C_ALIAS(name, c_alias)                          \
  static_assert(true, "Require semicolon.")

#endif // LIBC_COPT_PUBLIC_PACKAGING

#define LLVM_LIBC_FUNCTION_IMPL_3(type, name, arglist)                         \
  LLVM_LIBC_FUNCTION_IMPL_4(type, name, arglist, #name)

// LLVM_LIBC_FUNCTION(type, name, arglist) is equivalent to
// LLVM_LIBC_FUNCTION(type, name, arglist, #name)
#define LLVM_LIBC_FUNCTION(...)                                                \
  GET_FIFTH(__VA_ARGS__, LLVM_LIBC_FUNCTION_IMPL_4, LLVM_LIBC_FUNCTION_IMPL_3, \
            GET_NOTHING)(__VA_ARGS__)

// See comment on LLVM_LIBC_FUNCTION_IMPL_4 for why this checks
// LIBC_TARGET_USES_LEADING_UNDERSCORE.
#if defined(LIBC_COPT_PUBLIC_PACKAGING) && !defined(LIBC_COMPILER_IS_MSVC)
#ifndef LIBC_TARGET_USES_LEADING_UNDERSCORE
#define LLVM_LIBC_VARIABLE_IMPL(type, name)                                    \
  LLVM_LIBC_ATTR(name)                                                         \
  extern LLVM_LIBC_VARIABLE_ATTR decltype(LIBC_NAMESPACE::name)                \
      __##name##_impl__ asm(#name);                                            \
  extern decltype(LIBC_NAMESPACE::name) name [[gnu::alias(#name)]];            \
  type __##name##_impl__
#else // LIBC_TARGET_USES_LEADING_UNDERSCORE
#define LLVM_LIBC_VARIABLE_IMPL(type, name)                                    \
  LLVM_LIBC_ATTR(name)                                                         \
  extern LLVM_LIBC_VARIABLE_ATTR decltype(LIBC_NAMESPACE::name) name asm(      \
      "_" #name);                                                              \
  type name
#endif // LIBC_TARGET_USES_LEADING_UNDERSCORE
#else  // LIBC_COPT_PUBLIC_PACKAGING
#define LLVM_LIBC_VARIABLE_IMPL(type, name) type name
#endif // LIBC_COPT_PUBLIC_PACKAGING

#define LLVM_LIBC_VARIABLE(type, name) LLVM_LIBC_VARIABLE_IMPL(type, name)

#endif // LLVM_LIBC_SRC___SUPPORT_COMMON_H

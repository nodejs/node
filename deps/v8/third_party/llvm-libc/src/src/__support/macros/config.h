//===-- Portable attributes -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This header file defines a set of macros for checking the presence of
// important compiler and platform features. Such macros can be used to
// produce portable code by parameterizing compilation based on the presence or
// lack of a given feature.

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_CONFIG_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_CONFIG_H

#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"

#ifdef LIBC_COMPILER_IS_MSVC
#include <intrin.h>
#endif // LIBC_COMPILER_IS_MSVC

// Workaround for compilers that do not support builtin detection.
// FIXME: This is only required for the GPU portion which should be moved.
#ifndef __has_builtin
#define __has_builtin(b) 0
#endif

// Compiler feature-detection.
// clang.llvm.org/docs/LanguageExtensions.html#has-feature-and-has-extension
#ifdef __has_feature
#define LIBC_HAS_FEATURE(f) __has_feature(f)
#else
#define LIBC_HAS_FEATURE(f) 0
#endif

#ifdef LIBC_COMPILER_IS_MSVC

// __builtin_trap replacement
#ifdef LIBC_TARGET_ARCH_IS_X86
#define __builtin_trap __ud2
#else // arm64
#define __builtin_trap() __break(1)
#endif

#define __builtin_expect(value, expectation) (value)
#define __builtin_unreachable() __assume(0)

#define __builtin_prefetch(X, Y, Z)

#define LIBC_HAS_BUILTIN_IS_CONSTANT_EVALUATED 1
#define LIBC_HAS_BUILTIN_IS_ASSIGNABLE 1
#define LIBC_HAS_BUILTIN_IS_CONSTRUCTIBLE 1

#endif // LIBC_COMPILER_IS_MSVC

#ifdef __clang__
// Declare a LIBC_NAMESPACE with hidden visibility. `namespace
// LIBC_NAMESPACE_DECL {` should be used around all declarations and definitions
// for libc internals as opposed to just `namespace LIBC_NAMESPACE {`. This
// ensures that all declarations within this namespace have hidden
// visibility, which optimizes codegen for uses of symbols defined in other
// translation units in ways that can be necessary for correctness by avoiding
// dynamic relocations. This does not affect the public C symbols which are
// controlled independently via `LLVM_LIBC_FUNCTION_ATTR`.
#define LIBC_NAMESPACE_DECL [[gnu::visibility("hidden")]] LIBC_NAMESPACE
#else
// TODO(#98548): GCC emits a warning when using the visibility attribute which
// needs to be diagnosed and addressed.
#define LIBC_NAMESPACE_DECL LIBC_NAMESPACE
#endif

// IMPORTANT (USE WITH CAUTION): This macro is intended to be used at the top of
// the file and set to 1. It alters the signatures of some functions to have
// constexpr qualifier and forces the use of constexpr-compatible
// implementation, which might be a completely different code path or
// instructions. Some of these functions exploit platform-specific non-constexpr
// implementations to achieve certain goals, thus it is disabled by default.
#ifndef LIBC_ENABLE_CONSTEXPR
#define LIBC_ENABLE_CONSTEXPR 0
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_CONFIG_H

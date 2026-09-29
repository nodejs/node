//===-- Common definitions for LLVM-libc public header files --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LLVM_LIBC_COMMON_H
#define _LLVM_LIBC_COMMON_H

#define __LLVM_LIBC__ 1

#ifdef __cplusplus

#undef __BEGIN_C_DECLS
#define __BEGIN_C_DECLS extern "C" {

#undef __END_C_DECLS
#define __END_C_DECLS }

// Standard C++ doesn't have C99 restrict but GNU C++ has it with __ spelling.
#undef __restrict
#ifndef __GNUC__
#define __restrict
#endif

#undef _Noreturn
#define _Noreturn [[noreturn]]

#undef _Alignas
#define _Alignas alignas

#undef _Static_assert
#define _Static_assert static_assert

#undef _Alignof
#define _Alignof alignof

#undef __NOEXCEPT
#if __cplusplus >= 201103L
#define __NOEXCEPT noexcept
#else
#define __NOEXCEPT throw()
#endif

#undef _Returns_twice
#if __cplusplus >= 201103L
#define _Returns_twice [[gnu::returns_twice]]
#else
#define _Returns_twice __attribute__((returns_twice))
#endif

// This macro serves as a generic cast implementation for use in both C and C++,
// similar to `__BIONIC_CAST` in Android.
#undef __LLVM_LIBC_CAST
#define __LLVM_LIBC_CAST(cast, type, value) (cast<type>(value))

#else // not __cplusplus

#undef __BEGIN_C_DECLS
#define __BEGIN_C_DECLS

#undef __END_C_DECLS
#define __END_C_DECLS

#undef __restrict
#if __STDC_VERSION__ >= 199901L
// C99 and above support the restrict keyword.
#define __restrict restrict
#elif !defined(__GNUC__)
// GNU-compatible compilers accept the __ spelling in all modes.
// Otherwise, omit the qualifier for pure C89 compatibility.
#define __restrict
#endif

#undef _Noreturn
#if __STDC_VERSION__ >= 201112L
// In C11 and later, _Noreturn is a keyword.
#elif defined(__GNUC__)
// GNU-compatible compilers have an equivalent attribute.
#define _Noreturn __attribute__((__noreturn__))
#else
#define _Noreturn
#endif

#undef __NOEXCEPT
#ifdef __GNUC__
#define __NOEXCEPT __attribute__((__nothrow__))
#else
#define __NOEXCEPT
#endif

#undef _Returns_twice
#define _Returns_twice __attribute__((returns_twice))

#undef __LLVM_LIBC_CAST
#define __LLVM_LIBC_CAST(cast, type, value) ((type)(value))

#endif // __cplusplus

#endif // _LLVM_LIBC_COMMON_H

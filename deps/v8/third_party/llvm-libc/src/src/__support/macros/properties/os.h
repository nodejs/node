//===-- Target OS detection -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_OS_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_OS_H

#if (defined(__freebsd__) || defined(__FreeBSD__))
#define LIBC_TARGET_OS_IS_FREEBSD
#endif

#if defined(__ANDROID__)
#define LIBC_TARGET_OS_IS_ANDROID
#endif

#if defined(__linux__) && !defined(LIBC_TARGET_OS_IS_FREEBSD) &&               \
    !defined(LIBC_TARGET_OS_IS_ANDROID)
#define LIBC_TARGET_OS_IS_LINUX
#endif

#if (defined(_WIN64) || defined(_WIN32))
#define LIBC_TARGET_OS_IS_WINDOWS
#endif

#if defined(__Fuchsia__)
#define LIBC_TARGET_OS_IS_FUCHSIA
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_OS_H

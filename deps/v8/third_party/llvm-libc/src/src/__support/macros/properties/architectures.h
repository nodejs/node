//===-- Compile time architecture detection ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_ARCHITECTURES_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_ARCHITECTURES_H

#if defined(__AMDGPU__)
#define LIBC_TARGET_ARCH_IS_AMDGPU
#endif

#if defined(__SPIRV__)
#define LIBC_TARGET_ARCH_IS_SPIRV
#endif

#if defined(__NVPTX__)
#define LIBC_TARGET_ARCH_IS_NVPTX
#endif

#if defined(LIBC_TARGET_ARCH_IS_NVPTX) ||                                      \
    defined(LIBC_TARGET_ARCH_IS_AMDGPU) || defined(LIBC_TARGET_ARCH_IS_SPIRV)
#define LIBC_TARGET_ARCH_IS_GPU
#endif

#if defined(__CLR_VER) || defined(LIBC_TARGET_ARCH_IS_GPU)
#define LIBC_TARGET_ARCH_IS_VM
#endif

#if (defined(_M_IX86) || defined(__i386__)) && !defined(LIBC_TARGET_ARCH_IS_VM)
#define LIBC_TARGET_ARCH_IS_X86_32
#endif

#if (defined(_M_X64) || defined(__x86_64__)) && !defined(LIBC_TARGET_ARCH_IS_VM)
#define LIBC_TARGET_ARCH_IS_X86_64
#endif

#if defined(LIBC_TARGET_ARCH_IS_X86_32) || defined(LIBC_TARGET_ARCH_IS_X86_64)
#define LIBC_TARGET_ARCH_IS_X86
#endif

#if (defined(__arm__) || defined(_M_ARM))
#define LIBC_TARGET_ARCH_IS_ARM
#endif

#if defined(__wasm__)
#define LIBC_TARGET_ARCH_IS_WASM
#endif

#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#define LIBC_TARGET_ARCH_IS_AARCH64
#endif

#if defined(LIBC_TARGET_ARCH_IS_AARCH64) || defined(LIBC_TARGET_ARCH_IS_ARM)
#define LIBC_TARGET_ARCH_IS_ANY_ARM
#endif

#if defined(__riscv) && (__riscv_xlen == 64)
#define LIBC_TARGET_ARCH_IS_RISCV64
#endif

#if defined(__riscv) && (__riscv_xlen == 32)
#define LIBC_TARGET_ARCH_IS_RISCV32
#endif

#if defined(LIBC_TARGET_ARCH_IS_RISCV64) || defined(LIBC_TARGET_ARCH_IS_RISCV32)
#define LIBC_TARGET_ARCH_IS_ANY_RISCV
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_ARCHITECTURES_H

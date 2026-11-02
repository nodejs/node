//===-- Compile time cpu feature detection ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This file lists target cpu features by introspecting compiler enabled
// preprocessor definitions.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_CPU_FEATURES_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_CPU_FEATURES_H

#include "architectures.h"
#include "compiler.h"

#if defined(__ARM_FEATURE_FP16_SCALAR_ARITHMETIC)
#define LIBC_TARGET_CPU_HAS_FULLFP16
#endif

#if defined(__ARM_FEATURE_SVE)
#define LIBC_TARGET_CPU_HAS_SVE
#endif

#if defined(__ARM_FEATURE_SVE2)
#define LIBC_TARGET_CPU_HAS_SVE2
#endif

// __ARM_FEATURE_MVE is a bitfield
#if defined(__ARM_FEATURE_MVE)
#if (__ARM_FEATURE_MVE & 0x1)
#define LIBC_TARGET_CPU_HAS_MVE
#endif // LIBC_TARGET_CPU_HAS_MVE
#if (__ARM_FEATURE_MVE & 0x2)
#define LIBC_TARGET_CPU_HAS_MVE_FP
#endif // LIBC_TARGET_CPU_HAS_MVE_FP
#endif // __ARM_FEATURE_MVE

#if defined(__ARM_FEATURE_MOPS)
#define LIBC_TARGET_CPU_HAS_MOPS
#endif

#if defined(__SSE2__) || defined(_M_X64) ||                                    \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define LIBC_TARGET_CPU_HAS_SSE2
#define LIBC_TARGET_CPU_HAS_FPU_FLOAT
#define LIBC_TARGET_CPU_HAS_FPU_DOUBLE
#if defined(__SSE2__)
#define LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX 128
#endif
#endif

#if defined(__SSE4_2__) || defined(__AVX__)
#define LIBC_TARGET_CPU_HAS_SSE4_2
#endif

#if defined(__AVX__)
#define LIBC_TARGET_CPU_HAS_AVX
#ifndef LIBC_COMPILER_IS_MSVC
#undef LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX
#define LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX 256
#endif
#endif

#if defined(__AVX2__)
#define LIBC_TARGET_CPU_HAS_AVX2
#endif

#if defined(__AVX512F__)
#define LIBC_TARGET_CPU_HAS_AVX512F
#ifndef LIBC_COMPILER_IS_MSVC
#undef LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX
#define LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX 512
#endif
#endif

#if defined(__AVX512BW__)
#define LIBC_TARGET_CPU_HAS_AVX512BW
#endif

#if defined(__AVX512F__) || defined(__AVX2__)
#define LIBC_TARGET_CPU_HAS_GATHER
#endif

#if defined(__ARM_FP)
#if (__ARM_FP & 0x2)
#define LIBC_TARGET_CPU_HAS_ARM_FPU_HALF
#define LIBC_TARGET_CPU_HAS_FPU_HALF
#endif // LIBC_TARGET_CPU_HAS_ARM_FPU_HALF
#if (__ARM_FP & 0x4)
#define LIBC_TARGET_CPU_HAS_ARM_FPU_FLOAT
#define LIBC_TARGET_CPU_HAS_FPU_FLOAT
#endif // LIBC_TARGET_CPU_HAS_ARM_FPU_FLOAT
#if (__ARM_FP & 0x8)
#define LIBC_TARGET_CPU_HAS_ARM_FPU_DOUBLE
#define LIBC_TARGET_CPU_HAS_FPU_DOUBLE
#endif // LIBC_TARGET_CPU_HAS_ARM_FPU_DOUBLE
#elif defined(_M_ARM64) || defined(_M_ARM64EC)
#define LIBC_TARGET_CPU_HAS_ARM_FPU_FLOAT
#define LIBC_TARGET_CPU_HAS_FPU_FLOAT
#define LIBC_TARGET_CPU_HAS_ARM_FPU_DOUBLE
#define LIBC_TARGET_CPU_HAS_FPU_DOUBLE
#endif // __ARM_FP

#if defined(__ARM_NEON) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define LIBC_TARGET_CPU_HAS_ARM_NEON
#if defined(__ARM_NEON)
#define LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX 512
#endif
#endif

#if defined(__riscv_flen)
// https://github.com/riscv-non-isa/riscv-c-api-doc/blob/main/src/c-api.adoc
#if defined(__riscv_zfhmin)
#define LIBC_TARGET_CPU_HAS_RISCV_FPU_HALF
#define LIBC_TARGET_CPU_HAS_FPU_HALF
#endif // LIBC_TARGET_CPU_HAS_RISCV_FPU_HALF
#if (__riscv_flen >= 32)
#define LIBC_TARGET_CPU_HAS_RISCV_FPU_FLOAT
#define LIBC_TARGET_CPU_HAS_FPU_FLOAT
#endif // LIBC_TARGET_CPU_HAS_RISCV_FPU_FLOAT
#if (__riscv_flen >= 64)
#define LIBC_TARGET_CPU_HAS_RISCV_FPU_DOUBLE
#define LIBC_TARGET_CPU_HAS_FPU_DOUBLE
#endif // LIBC_TARGET_CPU_HAS_RISCV_FPU_DOUBLE
#endif // __riscv_flen

#if defined(__riscv_v_min_vlen)
#define LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX __riscv_v_min_vlen
#endif // __riscv_v_min_vlen

#if defined(__NVPTX__) || defined(__AMDGPU__) || defined(__SPIRV__)
#define LIBC_TARGET_CPU_HAS_FPU_FLOAT
#define LIBC_TARGET_CPU_HAS_FPU_DOUBLE
#endif

#if defined(__ARM_FEATURE_FMA) || defined(_M_ARM64) || defined(_M_ARM64EC) ||  \
    (defined(__AVX2__) && defined(__FMA__)) ||                                 \
    (defined(LIBC_COMPILER_IS_MSVC) && defined(__AVX2__)) ||                   \
    defined(__NVPTX__) || defined(__AMDGPU__) || defined(__riscv_flen) ||      \
    defined(__SPIRV__)
#define LIBC_TARGET_CPU_HAS_FMA
// Provide a more fine-grained control of FMA instruction for ARM targets.
#if defined(LIBC_TARGET_CPU_HAS_FPU_HALF)
#define LIBC_TARGET_CPU_HAS_FMA_HALF
#endif // LIBC_TARGET_CPU_HAS_FMA_HALF
#if defined(LIBC_TARGET_CPU_HAS_FPU_FLOAT)
#define LIBC_TARGET_CPU_HAS_FMA_FLOAT
#endif // LIBC_TARGET_CPU_HAS_FMA_FLOAT
#if defined(LIBC_TARGET_CPU_HAS_FPU_DOUBLE)
#define LIBC_TARGET_CPU_HAS_FMA_DOUBLE
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
#endif

#if defined(LIBC_TARGET_ARCH_IS_AARCH64) ||                                    \
    (defined(LIBC_TARGET_ARCH_IS_X86_64) &&                                    \
     defined(LIBC_TARGET_CPU_HAS_SSE4_2))
#define LIBC_TARGET_CPU_HAS_NEAREST_INT
#endif

#if defined(LIBC_TARGET_ARCH_IS_AARCH64) || defined(LIBC_TARGET_ARCH_IS_GPU)
#define LIBC_TARGET_CPU_HAS_FAST_FLOAT16_OPS
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_CPU_FEATURES_H

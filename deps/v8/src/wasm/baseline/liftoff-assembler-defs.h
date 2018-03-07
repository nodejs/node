// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_DEFS_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_DEFS_H_

#include "src/reglist.h"

#if V8_TARGET_ARCH_IA32
#include "src/ia32/assembler-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/x64/assembler-x64.h"
#endif

namespace v8 {
namespace internal {
namespace wasm {

#if V8_TARGET_ARCH_IA32

constexpr bool kLiftoffAssemblerImplementedOnThisPlatform = true;

constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<eax, ecx, edx, ebx, esi, edi>();

// Omit xmm7, which is the kScratchDoubleReg.
constexpr RegList kLiftoffAssemblerFpCacheRegs =
    DoubleRegister::ListOf<xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6>();

#elif V8_TARGET_ARCH_X64

constexpr bool kLiftoffAssemblerImplementedOnThisPlatform = true;

constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<rax, rcx, rdx, rbx, rsi, rdi>();

constexpr RegList kLiftoffAssemblerFpCacheRegs =
    DoubleRegister::ListOf<xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7>();

#else

constexpr bool kLiftoffAssemblerImplementedOnThisPlatform = false;

constexpr RegList kLiftoffAssemblerGpCacheRegs = 0xff;

constexpr RegList kLiftoffAssemblerFpCacheRegs = 0xff;

#endif

#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
constexpr Condition kEqual = equal;
constexpr Condition kUnsignedGreaterEqual = above_equal;
#else
// On unimplemented platforms, just make this compile.
constexpr Condition kEqual = static_cast<Condition>(0);
constexpr Condition kUnsignedGreaterEqual = static_cast<Condition>(0);
#endif

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_DEFS_H_

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
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/assembler-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/assembler-mips64.h"
#endif

namespace v8 {
namespace internal {
namespace wasm {

#if V8_TARGET_ARCH_IA32

constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<eax, ecx, edx, ebx, esi, edi>();

// Omit xmm7, which is the kScratchDoubleReg.
constexpr RegList kLiftoffAssemblerFpCacheRegs =
    DoubleRegister::ListOf<xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6>();

#elif V8_TARGET_ARCH_X64

constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<rax, rcx, rdx, rbx, rsi, rdi>();

constexpr RegList kLiftoffAssemblerFpCacheRegs =
    DoubleRegister::ListOf<xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7>();

#elif V8_TARGET_ARCH_MIPS

constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<a0, a1, a2, a3, t0, t1, t2, t3, t4, t5, t6, s7, v0, v1>();

constexpr RegList kLiftoffAssemblerFpCacheRegs =
    DoubleRegister::ListOf<f0, f2, f4, f6, f8, f10, f12, f14, f16, f18, f20,
                           f22, f24>();

#elif V8_TARGET_ARCH_MIPS64

constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<a0, a1, a2, a3, a4, a5, a6, a7, t0, t1, t2, s7, v0, v1>();

constexpr RegList kLiftoffAssemblerFpCacheRegs =
    DoubleRegister::ListOf<f0, f2, f4, f6, f8, f10, f12, f14, f16, f18, f20,
                           f22, f24, f26>();

#else

constexpr RegList kLiftoffAssemblerGpCacheRegs = 0xff;

constexpr RegList kLiftoffAssemblerFpCacheRegs = 0xff;

#endif

#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64

constexpr Condition kEqual = equal;
constexpr Condition kUnequal = not_equal;
constexpr Condition kSignedLessThan = less;
constexpr Condition kSignedLessEqual = less_equal;
constexpr Condition kSignedGreaterThan = greater;
constexpr Condition kSignedGreaterEqual = greater_equal;
constexpr Condition kUnsignedLessThan = below;
constexpr Condition kUnsignedLessEqual = below_equal;
constexpr Condition kUnsignedGreaterThan = above;
constexpr Condition kUnsignedGreaterEqual = above_equal;

#elif V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64

constexpr Condition kEqual = eq;
constexpr Condition kUnequal = ne;
constexpr Condition kSignedLessThan = lt;
constexpr Condition kSignedLessEqual = le;
constexpr Condition kSignedGreaterThan = gt;
constexpr Condition kSignedGreaterEqual = ge;
constexpr Condition kUnsignedLessThan = ult;
constexpr Condition kUnsignedLessEqual = ule;
constexpr Condition kUnsignedGreaterThan = ugt;
constexpr Condition kUnsignedGreaterEqual = uge;

#else

// On unimplemented platforms, just make this compile.
constexpr Condition kEqual = static_cast<Condition>(0);
constexpr Condition kUnequal = static_cast<Condition>(0);
constexpr Condition kSignedLessThan = static_cast<Condition>(0);
constexpr Condition kSignedLessEqual = static_cast<Condition>(0);
constexpr Condition kSignedGreaterThan = static_cast<Condition>(0);
constexpr Condition kSignedGreaterEqual = static_cast<Condition>(0);
constexpr Condition kUnsignedLessThan = static_cast<Condition>(0);
constexpr Condition kUnsignedLessEqual = static_cast<Condition>(0);
constexpr Condition kUnsignedGreaterThan = static_cast<Condition>(0);
constexpr Condition kUnsignedGreaterEqual = static_cast<Condition>(0);

#endif

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_DEFS_H_

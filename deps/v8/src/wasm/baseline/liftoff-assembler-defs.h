// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_DEFS_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_DEFS_H_

#include "src/assembler-arch.h"
#include "src/reglist.h"

namespace v8 {
namespace internal {
namespace wasm {

#if V8_TARGET_ARCH_IA32

constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<eax, ecx, edx, esi, edi>();

// Omit xmm7, which is the kScratchDoubleReg.
constexpr RegList kLiftoffAssemblerFpCacheRegs =
    DoubleRegister::ListOf<xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6>();

#elif V8_TARGET_ARCH_X64

constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<rax, rcx, rdx, rbx, rsi, rdi, r9>();

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

#elif V8_TARGET_ARCH_ARM

// r7: cp, r10: root, r11: fp, r12: ip, r13: sp, r14: lr, r15: pc.
constexpr RegList kLiftoffAssemblerGpCacheRegs =
    Register::ListOf<r0, r1, r2, r3, r4, r5, r6, r8, r9>();

// d13: zero, d14-d15: scratch
constexpr RegList kLiftoffAssemblerFpCacheRegs =
    LowDwVfpRegister::ListOf<d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11,
                             d12>();

#elif V8_TARGET_ARCH_ARM64

// x16: ip0, x17: ip1, x18: platform register, x26: root, x27: cp, x29: fp,
// x30: lr, x31: xzr.
constexpr RegList kLiftoffAssemblerGpCacheRegs =
    CPURegister::ListOf<x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12,
                        x13, x14, x15, x19, x20, x21, x22, x23, x24, x25,
                        x28>();

// d15: fp_zero, d30-d31: macro-assembler scratch V Registers.
constexpr RegList kLiftoffAssemblerFpCacheRegs =
    CPURegister::ListOf<d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12,
                        d13, d14, d16, d17, d18, d19, d20, d21, d22, d23, d24,
                        d25, d26, d27, d28, d29>();

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

#elif V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64

constexpr Condition kEqual = eq;
constexpr Condition kUnequal = ne;
constexpr Condition kSignedLessThan = lt;
constexpr Condition kSignedLessEqual = le;
constexpr Condition kSignedGreaterThan = gt;
constexpr Condition kSignedGreaterEqual = ge;
constexpr Condition kUnsignedLessThan = lo;
constexpr Condition kUnsignedLessEqual = ls;
constexpr Condition kUnsignedGreaterThan = hi;
constexpr Condition kUnsignedGreaterEqual = hs;

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

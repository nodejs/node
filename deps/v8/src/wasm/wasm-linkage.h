// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_LINKAGE_H_
#define V8_WASM_WASM_LINKAGE_H_

#include "src/assembler-arch.h"
#include "src/machine-type.h"
#include "src/signature.h"
#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {
namespace wasm {

#if V8_TARGET_ARCH_IA32
// ===========================================================================
// == ia32 ===================================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {esi, eax, edx, ecx, ebx};
constexpr Register kGpReturnRegisters[] = {eax, edx};
constexpr DoubleRegister kFpParamRegisters[] = {xmm1, xmm2, xmm3,
                                                xmm4, xmm5, xmm6};
constexpr DoubleRegister kFpReturnRegisters[] = {xmm1, xmm2};

#elif V8_TARGET_ARCH_X64
// ===========================================================================
// == x64 ====================================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {rsi, rax, rdx, rcx, rbx, rdi};
constexpr Register kGpReturnRegisters[] = {rax, rdx};
constexpr DoubleRegister kFpParamRegisters[] = {xmm1, xmm2, xmm3,
                                                xmm4, xmm5, xmm6};
constexpr DoubleRegister kFpReturnRegisters[] = {xmm1, xmm2};

#elif V8_TARGET_ARCH_ARM
// ===========================================================================
// == arm ====================================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {r3, r0, r1, r2};
constexpr Register kGpReturnRegisters[] = {r0, r1};
constexpr DoubleRegister kFpParamRegisters[] = {d0, d1, d2, d3, d4, d5, d6, d7};
constexpr DoubleRegister kFpReturnRegisters[] = {d0, d1};

#elif V8_TARGET_ARCH_ARM64
// ===========================================================================
// == arm64 ====================================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {x7, x0, x1, x2, x3, x4, x5, x6};
constexpr Register kGpReturnRegisters[] = {x0, x1};
constexpr DoubleRegister kFpParamRegisters[] = {d0, d1, d2, d3, d4, d5, d6, d7};
constexpr DoubleRegister kFpReturnRegisters[] = {d0, d1};

#elif V8_TARGET_ARCH_MIPS
// ===========================================================================
// == mips ===================================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {a0, a1, a2, a3};
constexpr Register kGpReturnRegisters[] = {v0, v1};
constexpr DoubleRegister kFpParamRegisters[] = {f2, f4, f6, f8, f10, f12, f14};
constexpr DoubleRegister kFpReturnRegisters[] = {f2, f4};

#elif V8_TARGET_ARCH_MIPS64
// ===========================================================================
// == mips64 =================================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {a0, a1, a2, a3, a4, a5, a6, a7};
constexpr Register kGpReturnRegisters[] = {v0, v1};
constexpr DoubleRegister kFpParamRegisters[] = {f2, f4, f6, f8, f10, f12, f14};
constexpr DoubleRegister kFpReturnRegisters[] = {f2, f4};

#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
// ===========================================================================
// == ppc & ppc64 ============================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {r10, r3, r4, r5, r6, r7, r8, r9};
constexpr Register kGpReturnRegisters[] = {r3, r4};
constexpr DoubleRegister kFpParamRegisters[] = {d1, d2, d3, d4, d5, d6, d7, d8};
constexpr DoubleRegister kFpReturnRegisters[] = {d1, d2};

#elif V8_TARGET_ARCH_S390X
// ===========================================================================
// == s390x ==================================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {r6, r2, r3, r4, r5};
constexpr Register kGpReturnRegisters[] = {r2, r3};
constexpr DoubleRegister kFpParamRegisters[] = {d0, d2, d4, d6};
constexpr DoubleRegister kFpReturnRegisters[] = {d0, d2, d4, d6};

#elif V8_TARGET_ARCH_S390
// ===========================================================================
// == s390 ===================================================================
// ===========================================================================
constexpr Register kGpParamRegisters[] = {r6, r2, r3, r4, r5};
constexpr Register kGpReturnRegisters[] = {r2, r3};
constexpr DoubleRegister kFpParamRegisters[] = {d0, d2};
constexpr DoubleRegister kFpReturnRegisters[] = {d0, d2};

#else
// ===========================================================================
// == unknown ================================================================
// ===========================================================================
// Do not use any registers, we will just always use the stack.
constexpr Register kGpParamRegisters[] = {};
constexpr Register kGpReturnRegisters[] = {};
constexpr DoubleRegister kFpParamRegisters[] = {};
constexpr DoubleRegister kFpReturnRegisters[] = {};

#endif

// The parameter index where the instance parameter should be placed in wasm
// call descriptors. This is used by the Int64Lowering::LowerNode method.
constexpr int kWasmInstanceParameterIndex = 0;

class LinkageAllocator {
 public:
  template <size_t kNumGpRegs, size_t kNumFpRegs>
  constexpr LinkageAllocator(const Register (&gp)[kNumGpRegs],
                             const DoubleRegister (&fp)[kNumFpRegs])
      : LinkageAllocator(gp, kNumGpRegs, fp, kNumFpRegs) {}

  constexpr LinkageAllocator(const Register* gp, int gpc,
                             const DoubleRegister* fp, int fpc)
      : gp_count_(gpc), gp_regs_(gp), fp_count_(fpc), fp_regs_(fp) {}

  bool has_more_gp_regs() const { return gp_offset_ < gp_count_; }
  bool has_more_fp_regs() const { return fp_offset_ < fp_count_; }

  Register NextGpReg() {
    DCHECK_LT(gp_offset_, gp_count_);
    return gp_regs_[gp_offset_++];
  }

  DoubleRegister NextFpReg() {
    DCHECK_LT(fp_offset_, fp_count_);
    return fp_regs_[fp_offset_++];
  }

  // Stackslots are counted upwards starting from 0 (or the offset set by
  // {SetStackOffset}.
  int NumStackSlots(MachineRepresentation type) {
    return std::max(1, ElementSizeInBytes(type) / kPointerSize);
  }

  // Stackslots are counted upwards starting from 0 (or the offset set by
  // {SetStackOffset}. If {type} needs more than
  // one stack slot, the lowest used stack slot is returned.
  int NextStackSlot(MachineRepresentation type) {
    int num_stack_slots = NumStackSlots(type);
    int offset = stack_offset_;
    stack_offset_ += num_stack_slots;
    return offset;
  }

  // Set an offset for the stack slots returned by {NextStackSlot} and
  // {NumStackSlots}. Can only be called before any call to {NextStackSlot}.
  void SetStackOffset(int num) {
    DCHECK_LE(0, num);
    DCHECK_EQ(0, stack_offset_);
    stack_offset_ = num;
  }

  int NumStackSlots() const { return stack_offset_; }

 private:
  const int gp_count_;
  int gp_offset_ = 0;
  const Register* const gp_regs_;

  const int fp_count_;
  int fp_offset_ = 0;
  const DoubleRegister* const fp_regs_;

  int stack_offset_ = 0;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_LINKAGE_H_

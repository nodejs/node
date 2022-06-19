// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_RISCV64_FRAME_CONSTANTS_RISCV64_H_
#define V8_EXECUTION_RISCV64_FRAME_CONSTANTS_RISCV64_H_

#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/execution/frame-constants.h"
#include "src/wasm/baseline/liftoff-assembler-defs.h"
#include "src/wasm/wasm-linkage.h"

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  static constexpr int kCallerFPOffset = -3 * kSystemPointerSize;
};

class WasmCompileLazyFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kNumberOfSavedGpParamRegs =
      arraysize(wasm::kGpParamRegisters) + 1;
  static constexpr int kNumberOfSavedFpParamRegs =
      arraysize(wasm::kFpParamRegisters);
  static constexpr int kNumberOfSavedAllParamRegs =
      kNumberOfSavedGpParamRegs + kNumberOfSavedFpParamRegs;

  // FP-relative.
  // See Generate_WasmCompileLazy in builtins-mips64.cc.
  static constexpr int kWasmInstanceOffset =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(kNumberOfSavedAllParamRegs);
  static constexpr int kFixedFrameSizeFromFp =
      TypedFrameConstants::kFixedFrameSizeFromFp +
      kNumberOfSavedGpParamRegs * kSystemPointerSize +
      kNumberOfSavedFpParamRegs * kDoubleSize;
};

// Frame constructed by the {WasmDebugBreak} builtin.
// After pushing the frame type marker, the builtin pushes all Liftoff cache
// registers (see liftoff-assembler-defs.h).
class WasmDebugBreakFrameConstants : public TypedFrameConstants {
 public:
  static constexpr RegList kPushedGpRegs = wasm::kLiftoffAssemblerGpCacheRegs;

  static constexpr DoubleRegList kPushedFpRegs =
      wasm::kLiftoffAssemblerFpCacheRegs;

  static constexpr int kNumPushedGpRegisters = kPushedGpRegs.Count();
  static constexpr int kNumPushedFpRegisters = kPushedFpRegs.Count();

  static constexpr int kLastPushedGpRegisterOffset =
      -kFixedFrameSizeFromFp - kNumPushedGpRegisters * kSystemPointerSize;
  static constexpr int kLastPushedFpRegisterOffset =
      kLastPushedGpRegisterOffset - kNumPushedFpRegisters * kDoubleSize;

  // Offsets are fp-relative.
  static int GetPushedGpRegisterOffset(int reg_code) {
    DCHECK_NE(0, kPushedGpRegs.bits() & (1 << reg_code));
    uint32_t lower_regs =
        kPushedGpRegs.bits() & ((uint32_t{1} << reg_code) - 1);
    return kLastPushedGpRegisterOffset +
           base::bits::CountPopulation(lower_regs) * kSystemPointerSize;
  }

  static int GetPushedFpRegisterOffset(int reg_code) {
    DCHECK_NE(0, kPushedFpRegs.bits() & (1 << reg_code));
    uint32_t lower_regs =
        kPushedFpRegs.bits() & ((uint32_t{1} << reg_code) - 1);
    return kLastPushedFpRegisterOffset +
           base::bits::CountPopulation(lower_regs) * kDoubleSize;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_RISCV64_FRAME_CONSTANTS_RISCV64_H_

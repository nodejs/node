// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ARM_FRAME_CONSTANTS_ARM_H_
#define V8_EXECUTION_ARM_FRAME_CONSTANTS_ARM_H_

#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/codegen/arm/register-arm.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {

// The layout of an EntryFrame is as follows:
//            TOP OF THE STACK     LOWEST ADDRESS
//         +---------------------+-----------------------
//   0     |  bad frame pointer  |  <-- frame ptr
//         |   (0xFFF.. FF)      |
//         |- - - - - - - - - - -|
//  1..2   | saved register d8   |
//  ...    |        ...          |
//  15..16 | saved register d15  |
//         |- - - - - - - - - - -|
//  17     | saved register r4   |
//  ...    |        ...          |
//  23     | saved register r10  |
//         |- - - - - - - - - - -|
//  24     |   saved fp (r11)    |
//         |- - - - - - - - - - -|
//  25     |   saved lr (r14)    |
//    -----+---------------------+-----------------------
//           BOTTOM OF THE STACK   HIGHEST ADDRESS
class EntryFrameConstants : public AllStatic {
 public:
  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  static constexpr int kCallerFPOffset = -3 * kSystemPointerSize;

  // Stack offsets for arguments passed to JSEntry.
  static constexpr int kArgcOffset = +0 * kSystemPointerSize;
  static constexpr int kArgvOffset = +1 * kSystemPointerSize;

  // These offsets refer to the immediate caller (i.e a native frame).
  static constexpr int kDirectCallerFPOffset =
      /* bad frame pointer (-1) */
      kPointerSize +
      /* d8...d15 */
      kNumDoubleCalleeSaved * kDoubleSize +
      /* r4...r10 (i.e callee saved without fp) */
      (kNumCalleeSaved - 1) * kPointerSize;
  static constexpr int kDirectCallerPCOffset =
      kDirectCallerFPOffset + 1 * kSystemPointerSize;
  static constexpr int kDirectCallerSPOffset =
      kDirectCallerPCOffset + 1 * kSystemPointerSize;
};

class WasmCompileLazyFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kNumberOfSavedGpParamRegs = 4;
  static constexpr int kNumberOfSavedFpParamRegs = 8;

  // FP-relative.
  static constexpr int kWasmInstanceOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kFixedFrameSizeFromFp =
      TypedFrameConstants::kFixedFrameSizeFromFp +
      kNumberOfSavedGpParamRegs * kPointerSize +
      kNumberOfSavedFpParamRegs * kDoubleSize;
};

// Frame constructed by the {WasmDebugBreak} builtin.
// After pushing the frame type marker, the builtin pushes all Liftoff cache
// registers (see liftoff-assembler-defs.h).
class WasmDebugBreakFrameConstants : public TypedFrameConstants {
 public:
  // {r0, r1, r2, r3, r4, r5, r6, r8, r9}
  static constexpr uint32_t kPushedGpRegs = 0b1101111111;
  // {d0 .. d12}
  static constexpr int kFirstPushedFpReg = 0;
  static constexpr int kLastPushedFpReg = 12;

  static constexpr int kNumPushedGpRegisters =
      base::bits::CountPopulation(kPushedGpRegs);
  static constexpr int kNumPushedFpRegisters =
      kLastPushedFpReg - kFirstPushedFpReg + 1;

  static constexpr int kLastPushedGpRegisterOffset =
      -TypedFrameConstants::kFixedFrameSizeFromFp -
      kSystemPointerSize * kNumPushedGpRegisters;
  static constexpr int kLastPushedFpRegisterOffset =
      kLastPushedGpRegisterOffset - kDoubleSize * kNumPushedFpRegisters;

  // Offsets are fp-relative.
  static int GetPushedGpRegisterOffset(int reg_code) {
    DCHECK_NE(0, kPushedGpRegs & (1 << reg_code));
    uint32_t lower_regs = kPushedGpRegs & ((uint32_t{1} << reg_code) - 1);
    return kLastPushedGpRegisterOffset +
           base::bits::CountPopulation(lower_regs) * kSystemPointerSize;
  }

  static int GetPushedFpRegisterOffset(int reg_code) {
    DCHECK_LE(kFirstPushedFpReg, reg_code);
    DCHECK_GE(kLastPushedFpReg, reg_code);
    return kLastPushedFpRegisterOffset +
           (reg_code - kFirstPushedFpReg) * kDoubleSize;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ARM_FRAME_CONSTANTS_ARM_H_

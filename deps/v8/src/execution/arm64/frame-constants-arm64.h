// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ARM64_FRAME_CONSTANTS_ARM64_H_
#define V8_EXECUTION_ARM64_FRAME_CONSTANTS_ARM64_H_

#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {

// The layout of an EntryFrame is as follows:
//
//  slot      Entry frame
//       +---------------------+-----------------------
// -20   | saved register d15  |
// ...   |        ...          |
// -13   | saved register d8   |
//       |- - - - - - - - - - -|
// -12   |   saved lr (x30)    |
//       |- - - - - - - - - - -|
// -11   |   saved fp (x29)    |
//       |- - - - - - - - - - -|
// -10   | saved register x28  |
// ...   |        ...          |
//  -1   | saved register x19  |
//       |- - - - - - - - - - -|
//   0   |  bad frame pointer  |  <-- frame ptr
//       |   (0xFFF.. FF)      |
//       |- - - - - - - - - - -|
//   1   | stack frame marker  |
//       |      (ENTRY)        |
//       |- - - - - - - - - - -|
//   2   | stack frame marker  |
//       |        (0)          |
//       |- - - - - - - - - - -|
//   3   |     C entry FP      |
//       |- - - - - - - - - - -|
//   4   |   JS entry frame    |
//       |       marker        |
//       |- - - - - - - - - - -|
//   5   |      padding        |  <-- stack ptr
//  -----+---------------------+-----------------------
//
class EntryFrameConstants : public AllStatic {
 public:
  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  static constexpr int kCallerFPOffset = -3 * kSystemPointerSize;
  static constexpr int kFixedFrameSize = 6 * kSystemPointerSize;

  // The following constants are defined so we can static-assert their values
  // near the relevant JSEntry assembly code, not because they're actually very
  // useful.
  static constexpr int kCalleeSavedRegisterBytesPushedBeforeFpLrPair =
      8 * kSystemPointerSize;
  static constexpr int kCalleeSavedRegisterBytesPushedAfterFpLrPair =
      10 * kSystemPointerSize;
  static constexpr int kOffsetToCalleeSavedRegisters = 1 * kSystemPointerSize;

  // These offsets refer to the immediate caller (a native frame), not to the
  // previous JS exit frame like kCallerFPOffset above.
  static constexpr int kDirectCallerFPOffset =
      kCalleeSavedRegisterBytesPushedAfterFpLrPair +
      kOffsetToCalleeSavedRegisters;
  static constexpr int kDirectCallerPCOffset =
      kDirectCallerFPOffset + 1 * kSystemPointerSize;
  static constexpr int kDirectCallerSPOffset =
      kDirectCallerPCOffset + 1 * kSystemPointerSize +
      kCalleeSavedRegisterBytesPushedBeforeFpLrPair;
};

class WasmCompileLazyFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kNumberOfSavedGpParamRegs = 8;
  static constexpr int kNumberOfSavedFpParamRegs = 8;

  // FP-relative.
  static constexpr int kWasmInstanceOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  static constexpr int kFixedFrameSizeFromFp =
      // Header is padded to 16 byte (see {MacroAssembler::EnterFrame}).
      RoundUp<16>(TypedFrameConstants::kFixedFrameSizeFromFp) +
      kNumberOfSavedGpParamRegs * kSystemPointerSize +
      kNumberOfSavedFpParamRegs * kDoubleSize;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ARM64_FRAME_CONSTANTS_ARM64_H_

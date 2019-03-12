// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_FRAME_CONSTANTS_ARM64_H_
#define V8_ARM64_FRAME_CONSTANTS_ARM64_H_

#include "src/base/macros.h"
#include "src/frame-constants.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// The layout of an EntryFrame is as follows:
//
//  slot      Entry frame
//       +---------------------+-----------------------
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
  static constexpr int kCallerFPOffset = -3 * kPointerSize;
  static constexpr int kFixedFrameSize = 6 * kPointerSize;
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  static constexpr int kPaddingOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(2);
  DEFINE_TYPED_FRAME_SIZES(3);
  static constexpr int kLastExitFrameField = kPaddingOffset;

  static constexpr int kConstantPoolOffset = 0;  // Not used
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
      kNumberOfSavedGpParamRegs * kPointerSize +
      kNumberOfSavedFpParamRegs * kDoubleSize;
};

class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static constexpr int kLocal0Offset =
      StandardFrameConstants::kExpressionsOffset;

  // There are two words on the stack (saved fp and saved lr) between fp and
  // the arguments.
  static constexpr int kLastParameterOffset = 2 * kPointerSize;

  static constexpr int kFunctionOffset =
      StandardFrameConstants::kFunctionOffset;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_FRAME_CONSTANTS_ARM64_H_

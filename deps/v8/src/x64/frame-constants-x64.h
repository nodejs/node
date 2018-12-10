// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X64_FRAME_CONSTANTS_X64_H_
#define V8_X64_FRAME_CONSTANTS_X64_H_

#include "src/base/macros.h"
#include "src/frame-constants.h"

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
#ifdef _WIN64
  static constexpr int kCalleeSaveXMMRegisters = 10;
  static constexpr int kXMMRegisterSize = 16;
  static constexpr int kXMMRegistersBlockSize =
      kXMMRegisterSize * kCalleeSaveXMMRegisters;

  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  // On x64, there are 7 pushq() and 3 Push() calls between setting up rbp and
  // pushing the c_entry_fp, plus we manually allocate kXMMRegistersBlockSize
  // bytes on the stack.
  static constexpr int kCallerFPOffset =
      -3 * kPointerSize + -7 * kRegisterSize - kXMMRegistersBlockSize;

  // Stack offsets for arguments passed to JSEntry.
  static constexpr int kArgvOffset = 6 * kPointerSize;
  static constexpr int kRootRegisterValueOffset = 7 * kPointerSize;
#else
  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  // On x64, there are 5 pushq() and 3 Push() calls between setting up rbp and
  // pushing the c_entry_fp.
  static constexpr int kCallerFPOffset = -3 * kPointerSize + -5 * kRegisterSize;
#endif
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);

  static constexpr int kCallerFPOffset = +0 * kPointerSize;
  static constexpr int kCallerPCOffset = kFPOnStackSize;

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static constexpr int kCallerSPDisplacement = kCallerPCOffset + kPCOnStackSize;

  static constexpr int kConstantPoolOffset = 0;  // Not used
};

class WasmCompileLazyFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kNumberOfSavedGpParamRegs = 6;
  static constexpr int kNumberOfSavedFpParamRegs = 6;

  // FP-relative.
  static constexpr int kWasmInstanceOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kFixedFrameSizeFromFp =
      TypedFrameConstants::kFixedFrameSizeFromFp +
      kNumberOfSavedGpParamRegs * kPointerSize +
      kNumberOfSavedFpParamRegs * kSimd128Size;
};

class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static constexpr int kLocal0Offset =
      StandardFrameConstants::kExpressionsOffset;
  static constexpr int kLastParameterOffset = kFPOnStackSize + kPCOnStackSize;
  static constexpr int kFunctionOffset =
      StandardFrameConstants::kFunctionOffset;

  // Caller SP-relative.
  static constexpr int kParam0Offset = -2 * kPointerSize;
  static constexpr int kReceiverOffset = -1 * kPointerSize;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_X64_FRAME_CONSTANTS_X64_H_

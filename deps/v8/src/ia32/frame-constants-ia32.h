// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_FRAME_CONSTANTS_IA32_H_
#define V8_IA32_FRAME_CONSTANTS_IA32_H_

#include "src/base/macros.h"
#include "src/frame-constants.h"

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  static constexpr int kCallerFPOffset = -6 * kPointerSize;

  // EntryFrame is used by JSEntry, JSConstructEntry and JSRunMicrotasksEntry.
  // All of them take |root_register_value| as the first parameter.
  static constexpr int kRootRegisterValueOffset = +2 * kPointerSize;

  // Rest of parameters passed to JSEntry and JSConstructEntry.
  static constexpr int kNewTargetArgOffset = +3 * kPointerSize;
  static constexpr int kFunctionArgOffset = +4 * kPointerSize;
  static constexpr int kReceiverArgOffset = +5 * kPointerSize;
  static constexpr int kArgcOffset = +6 * kPointerSize;
  static constexpr int kArgvOffset = +7 * kPointerSize;

  // Rest of parameters passed to JSRunMicrotasksEntry.
  static constexpr int kMicrotaskQueueArgOffset = +3 * kPointerSize;
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);

  static constexpr int kCallerFPOffset = 0 * kPointerSize;
  static constexpr int kCallerPCOffset = +1 * kPointerSize;

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static constexpr int kCallerSPDisplacement = +2 * kPointerSize;

  static constexpr int kConstantPoolOffset = 0;  // Not used
};

class WasmCompileLazyFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kNumberOfSavedGpParamRegs = 4;
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
  static constexpr int kLastParameterOffset = +2 * kPointerSize;
  static constexpr int kFunctionOffset =
      StandardFrameConstants::kFunctionOffset;

  // Caller SP-relative.
  static constexpr int kParam0Offset = -2 * kPointerSize;
  static constexpr int kReceiverOffset = -1 * kPointerSize;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IA32_FRAME_CONSTANTS_IA32_H_

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_IA32_FRAME_CONSTANTS_IA32_H_
#define V8_EXECUTION_IA32_FRAME_CONSTANTS_IA32_H_

#include "src/base/macros.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  static constexpr int kCallerFPOffset = -6 * kSystemPointerSize;

  // EntryFrame is used by JSEntry, JSConstructEntry and JSRunMicrotasksEntry.
  // All of them take |root_register_value| as the first parameter.
  static constexpr int kRootRegisterValueOffset = +2 * kSystemPointerSize;

  // Rest of parameters passed to JSEntry and JSConstructEntry.
  static constexpr int kNewTargetArgOffset = +3 * kSystemPointerSize;
  static constexpr int kFunctionArgOffset = +4 * kSystemPointerSize;
  static constexpr int kReceiverArgOffset = +5 * kSystemPointerSize;
  static constexpr int kArgcOffset = +6 * kSystemPointerSize;
  static constexpr int kArgvOffset = +7 * kSystemPointerSize;

  // Rest of parameters passed to JSRunMicrotasksEntry.
  static constexpr int kMicrotaskQueueArgOffset = +3 * kSystemPointerSize;
};

class WasmCompileLazyFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kNumberOfSavedGpParamRegs = 4;
  static constexpr int kNumberOfSavedFpParamRegs = 6;

  // FP-relative.
  static constexpr int kWasmInstanceOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kFixedFrameSizeFromFp =
      TypedFrameConstants::kFixedFrameSizeFromFp +
      kNumberOfSavedGpParamRegs * kSystemPointerSize +
      kNumberOfSavedFpParamRegs * kSimd128Size;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_IA32_FRAME_CONSTANTS_IA32_H_

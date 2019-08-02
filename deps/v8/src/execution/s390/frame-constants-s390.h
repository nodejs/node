// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_S390_FRAME_CONSTANTS_S390_H_
#define V8_EXECUTION_S390_FRAME_CONSTANTS_S390_H_

#include "src/base/macros.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
  static constexpr int kCallerFPOffset =
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
  // Stack offsets for arguments passed to JSEntry.
  static constexpr int kArgvOffset = 20 * kSystemPointerSize;
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  DEFINE_TYPED_FRAME_SIZES(1);

  // The caller fields are below the frame pointer on the stack.
  static constexpr int kCallerFPOffset = 0 * kPointerSize;
  // The calling JS function is below FP.
  static constexpr int kCallerPCOffset = 1 * kPointerSize;

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static constexpr int kCallerSPDisplacement = 2 * kPointerSize;
};

class WasmCompileLazyFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kNumberOfSavedGpParamRegs = 4;
#ifdef V8_TARGET_ARCH_S390X
  static constexpr int kNumberOfSavedFpParamRegs = 4;
#else
  static constexpr int kNumberOfSavedFpParamRegs = 2;
#endif

  // FP-relative.
  static constexpr int kWasmInstanceOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kFixedFrameSizeFromFp =
      TypedFrameConstants::kFixedFrameSizeFromFp +
      kNumberOfSavedGpParamRegs * kPointerSize +
      kNumberOfSavedFpParamRegs * kDoubleSize;
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

#endif  // V8_EXECUTION_S390_FRAME_CONSTANTS_S390_H_

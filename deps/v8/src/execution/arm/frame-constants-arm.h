// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ARM_FRAME_CONSTANTS_ARM_H_
#define V8_EXECUTION_ARM_FRAME_CONSTANTS_ARM_H_

#include "src/base/macros.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  static constexpr int kCallerFPOffset =
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);

  // Stack offsets for arguments passed to JSEntry.
  static constexpr int kArgcOffset = +0 * kSystemPointerSize;
  static constexpr int kArgvOffset = +1 * kSystemPointerSize;
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

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ARM_FRAME_CONSTANTS_ARM_H_

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
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kSystemPointerSize);
  // Stack offsets for arguments passed to JSEntry.
  static constexpr int kArgvOffset = 20 * kSystemPointerSize;
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
      kNumberOfSavedGpParamRegs * kSystemPointerSize +
      kNumberOfSavedFpParamRegs * kDoubleSize;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_S390_FRAME_CONSTANTS_S390_H_

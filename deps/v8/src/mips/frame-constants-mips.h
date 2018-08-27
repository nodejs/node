// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MIPS_FRAME_CONSTANTS_MIPS_H_
#define V8_MIPS_FRAME_CONSTANTS_MIPS_H_

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
  static constexpr int kCallerFPOffset =
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);

  // The caller fields are below the frame pointer on the stack.
  static constexpr int kCallerFPOffset = +0 * kPointerSize;
  // The calling JS function is between FP and PC.
  static constexpr int kCallerPCOffset = +1 * kPointerSize;

  // MIPS-specific: a pointer to the old sp to avoid unnecessary calculations.
  static constexpr int kCallerSPOffset = +2 * kPointerSize;

  // FP-relative displacement of the caller's SP.
  static constexpr int kCallerSPDisplacement = +2 * kPointerSize;

  static constexpr int kConstantPoolOffset = 0;  // Not used.
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

#endif  // V8_MIPS_FRAME_CONSTANTS_MIPS_H_

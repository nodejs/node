// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_FRAMES_IA32_H_
#define V8_IA32_FRAMES_IA32_H_

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset = -6 * kPointerSize;

  static const int kNewTargetArgOffset = +2 * kPointerSize;
  static const int kFunctionArgOffset = +3 * kPointerSize;
  static const int kReceiverArgOffset = +4 * kPointerSize;
  static const int kArgcOffset = +5 * kPointerSize;
  static const int kArgvOffset = +6 * kPointerSize;
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static const int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static const int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);

  static const int kCallerFPOffset = 0 * kPointerSize;
  static const int kCallerPCOffset = +1 * kPointerSize;

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static const int kCallerSPDisplacement = +2 * kPointerSize;

  static const int kConstantPoolOffset = 0;  // Not used
};

class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kLocal0Offset = StandardFrameConstants::kExpressionsOffset;
  static const int kLastParameterOffset = +2 * kPointerSize;
  static const int kFunctionOffset = StandardFrameConstants::kFunctionOffset;

  // Caller SP-relative.
  static const int kParam0Offset = -2 * kPointerSize;
  static const int kReceiverOffset = -1 * kPointerSize;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IA32_FRAMES_IA32_H_

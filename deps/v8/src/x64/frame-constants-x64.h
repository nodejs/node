// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X64_FRAME_CONSTANTS_X64_H_
#define V8_X64_FRAME_CONSTANTS_X64_H_

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
#ifdef _WIN64
  static constexpr int kCalleeSaveXMMRegisters = 10;
  static constexpr int kXMMRegisterSize = 16;
  static constexpr int kXMMRegistersBlockSize =
      kXMMRegisterSize * kCalleeSaveXMMRegisters;
  static constexpr int kCallerFPOffset =
      -3 * kPointerSize + -7 * kRegisterSize - kXMMRegistersBlockSize;
#else
  // We have 3 Push and 5 pushq in the JSEntryStub::GenerateBody.
  static constexpr int kCallerFPOffset = -3 * kPointerSize + -5 * kRegisterSize;
#endif
  static constexpr int kArgvOffset = 6 * kPointerSize;
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

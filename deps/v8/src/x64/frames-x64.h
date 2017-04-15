// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X64_FRAMES_X64_H_
#define V8_X64_FRAMES_X64_H_

namespace v8 {
namespace internal {

const int kNumRegs = 16;
const RegList kJSCallerSaved =
    1 << 0 |  // rax
    1 << 1 |  // rcx
    1 << 2 |  // rdx
    1 << 3 |  // rbx - used as a caller-saved register in JavaScript code
    1 << 7;   // rdi - callee function

const int kNumJSCallerSaved = 5;

// Number of registers for which space is reserved in safepoints.
const int kNumSafepointRegisters = 16;

// ----------------------------------------------------

class EntryFrameConstants : public AllStatic {
 public:
#ifdef _WIN64
  static const int kCalleeSaveXMMRegisters = 10;
  static const int kXMMRegisterSize = 16;
  static const int kXMMRegistersBlockSize =
      kXMMRegisterSize * kCalleeSaveXMMRegisters;
  static const int kCallerFPOffset =
      -3 * kPointerSize + -7 * kRegisterSize - kXMMRegistersBlockSize;
#else
  // We have 3 Push and 5 pushq in the JSEntryStub::GenerateBody.
  static const int kCallerFPOffset = -3 * kPointerSize + -5 * kRegisterSize;
#endif
  static const int kArgvOffset     = 6 * kPointerSize;
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static const int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static const int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);

  static const int kCallerFPOffset  = +0 * kPointerSize;
  static const int kCallerPCOffset  = kFPOnStackSize;

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static const int kCallerSPDisplacement = kCallerPCOffset + kPCOnStackSize;

  static const int kConstantPoolOffset   = 0;  // Not used
};


class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kLocal0Offset = StandardFrameConstants::kExpressionsOffset;
  static const int kLastParameterOffset = kFPOnStackSize + kPCOnStackSize;
  static const int kFunctionOffset = StandardFrameConstants::kFunctionOffset;

  // Caller SP-relative.
  static const int kParam0Offset   = -2 * kPointerSize;
  static const int kReceiverOffset = -1 * kPointerSize;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_X64_FRAMES_X64_H_

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


class ExitFrameConstants : public AllStatic {
 public:
  static const int kFrameSize       = 2 * kPointerSize;

  static const int kCodeOffset      = -2 * kPointerSize;
  static const int kSPOffset        = -1 * kPointerSize;

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
  static const int kFunctionOffset = StandardFrameConstants::kMarkerOffset;

  // Caller SP-relative.
  static const int kParam0Offset   = -2 * kPointerSize;
  static const int kReceiverOffset = -1 * kPointerSize;
};


class ArgumentsAdaptorFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kLengthOffset = StandardFrameConstants::kExpressionsOffset;

  static const int kFrameSize =
      StandardFrameConstants::kFixedFrameSize + kPointerSize;
};


class ConstructFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kImplicitReceiverOffset = -5 * kPointerSize;
  static const int kConstructorOffset      = kMinInt;
  static const int kLengthOffset           = -4 * kPointerSize;
  static const int kCodeOffset = StandardFrameConstants::kExpressionsOffset;

  static const int kFrameSize =
      StandardFrameConstants::kFixedFrameSize + 3 * kPointerSize;
};


class InternalFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kCodeOffset = StandardFrameConstants::kExpressionsOffset;
};


inline Object* JavaScriptFrame::function_slot_object() const {
  const int offset = JavaScriptFrameConstants::kFunctionOffset;
  return Memory::Object_at(fp() + offset);
}


} }  // namespace v8::internal

#endif  // V8_X64_FRAMES_X64_H_

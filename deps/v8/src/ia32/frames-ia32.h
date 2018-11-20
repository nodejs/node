// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_FRAMES_IA32_H_
#define V8_IA32_FRAMES_IA32_H_

namespace v8 {
namespace internal {


// Register lists
// Note that the bit values must match those used in actual instruction encoding
const int kNumRegs = 8;


// Caller-saved registers
const RegList kJSCallerSaved =
  1 << 0 |  // eax
  1 << 1 |  // ecx
  1 << 2 |  // edx
  1 << 3 |  // ebx - used as a caller-saved register in JavaScript code
  1 << 7;   // edi - callee function

const int kNumJSCallerSaved = 5;


// Number of registers for which space is reserved in safepoints.
const int kNumSafepointRegisters = 8;

const int kNoAlignmentPadding = 0;
const int kAlignmentPaddingPushed = 2;
const int kAlignmentZapValue = 0x12345678;  // Not heap object tagged.

// ----------------------------------------------------


class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset      = -6 * kPointerSize;

  static const int kFunctionArgOffset   = +3 * kPointerSize;
  static const int kReceiverArgOffset   = +4 * kPointerSize;
  static const int kArgcOffset          = +5 * kPointerSize;
  static const int kArgvOffset          = +6 * kPointerSize;
};


class ExitFrameConstants : public AllStatic {
 public:
  static const int kFrameSize      = 2 * kPointerSize;

  static const int kCodeOffset     = -2 * kPointerSize;
  static const int kSPOffset       = -1 * kPointerSize;

  static const int kCallerFPOffset =  0 * kPointerSize;
  static const int kCallerPCOffset = +1 * kPointerSize;

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static const int kCallerSPDisplacement = +2 * kPointerSize;

  static const int kConstantPoolOffset   = 0;  // Not used
};


class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kLocal0Offset = StandardFrameConstants::kExpressionsOffset;
  static const int kLastParameterOffset = +2 * kPointerSize;
  static const int kFunctionOffset = StandardFrameConstants::kMarkerOffset;

  // Caller SP-relative.
  static const int kParam0Offset   = -2 * kPointerSize;
  static const int kReceiverOffset = -1 * kPointerSize;

  static const int kDynamicAlignmentStateOffset = kLocal0Offset;
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


inline void StackHandler::SetFp(Address slot, Address fp) {
  Memory::Address_at(slot) = fp;
}


} }  // namespace v8::internal

#endif  // V8_IA32_FRAMES_IA32_H_

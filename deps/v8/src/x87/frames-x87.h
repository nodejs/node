// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X87_FRAMES_X87_H_
#define V8_X87_FRAMES_X87_H_

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

// ----------------------------------------------------


class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset      = -6 * kPointerSize;

  static const int kNewTargetArgOffset = +2 * kPointerSize;
  static const int kFunctionArgOffset   = +3 * kPointerSize;
  static const int kReceiverArgOffset   = +4 * kPointerSize;
  static const int kArgcOffset          = +5 * kPointerSize;
  static const int kArgvOffset          = +6 * kPointerSize;
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static const int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static const int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);

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
  static const int kFunctionOffset = StandardFrameConstants::kFunctionOffset;

  // Caller SP-relative.
  static const int kParam0Offset   = -2 * kPointerSize;
  static const int kReceiverOffset = -1 * kPointerSize;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_X87_FRAMES_X87_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PPC_FRAMES_PPC_H_
#define V8_PPC_FRAMES_PPC_H_

namespace v8 {
namespace internal {


// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding
const int kNumRegs = 32;


// Caller-saved/arguments registers
const RegList kJSCallerSaved = 1 << 3 |   // r3  a1
                               1 << 4 |   // r4  a2
                               1 << 5 |   // r5  a3
                               1 << 6 |   // r6  a4
                               1 << 7 |   // r7  a5
                               1 << 8 |   // r8  a6
                               1 << 9 |   // r9  a7
                               1 << 10 |  // r10 a8
                               1 << 11;

const int kNumJSCallerSaved = 9;

// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns r0.code() == 0
int JSCallerSavedCode(int n);


// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved = 1 << 14 |  // r14
                             1 << 15 |  // r15
                             1 << 16 |  // r16
                             1 << 17 |  // r17
                             1 << 18 |  // r18
                             1 << 19 |  // r19
                             1 << 20 |  // r20
                             1 << 21 |  // r21
                             1 << 22 |  // r22
                             1 << 23 |  // r23
                             1 << 24 |  // r24
                             1 << 25 |  // r25
                             1 << 26 |  // r26
                             1 << 27 |  // r27
                             1 << 28 |  // r28
                             1 << 29 |  // r29
                             1 << 30 |  // r20
                             1 << 31;   // r31


const int kNumCalleeSaved = 18;

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
const int kNumSafepointRegisters = 32;

// The following constants describe the stack frame linkage area as
// defined by the ABI.  Note that kNumRequiredStackFrameSlots must
// satisfy alignment requirements (rounding up if required).
#if V8_TARGET_ARCH_PPC64 && V8_TARGET_LITTLE_ENDIAN
// [0] back chain
// [1] condition register save area
// [2] link register save area
// [3] TOC save area
// [4] Parameter1 save area
// ...
// [11] Parameter8 save area
// [12] Parameter9 slot (if necessary)
// ...
const int kNumRequiredStackFrameSlots = 12;
const int kStackFrameLRSlot = 2;
const int kStackFrameExtraParamSlot = 12;
#elif V8_OS_AIX || V8_TARGET_ARCH_PPC64
// [0] back chain
// [1] condition register save area
// [2] link register save area
// [3] reserved for compiler
// [4] reserved by binder
// [5] TOC save area
// [6] Parameter1 save area
// ...
// [13] Parameter8 save area
// [14] Parameter9 slot (if necessary)
// ...
#if V8_TARGET_ARCH_PPC64
const int kNumRequiredStackFrameSlots = 14;
#else
const int kNumRequiredStackFrameSlots = 16;
#endif
const int kStackFrameLRSlot = 2;
const int kStackFrameExtraParamSlot = 14;
#else
// [0] back chain
// [1] link register save area
// [2] Parameter9 slot (if necessary)
// ...
const int kNumRequiredStackFrameSlots = 4;
const int kStackFrameLRSlot = 1;
const int kStackFrameExtraParamSlot = 2;
#endif

// ----------------------------------------------------


class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset =
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
};


class ExitFrameConstants : public AllStatic {
 public:
  static const int kFrameSize = 2 * kPointerSize;
  static const int kConstantPoolOffset = 0;  // Not used.
  static const int kCodeOffset = -2 * kPointerSize;
  static const int kSPOffset = -1 * kPointerSize;

  // The caller fields are below the frame pointer on the stack.
  static const int kCallerFPOffset = 0 * kPointerSize;
  // The calling JS function is below FP.
  static const int kCallerPCOffset = 1 * kPointerSize;

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static const int kCallerSPDisplacement = 2 * kPointerSize;
};


class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kLocal0Offset = StandardFrameConstants::kExpressionsOffset;
  static const int kLastParameterOffset = +2 * kPointerSize;
  static const int kFunctionOffset = StandardFrameConstants::kMarkerOffset;

  // Caller SP-relative.
  static const int kParam0Offset = -2 * kPointerSize;
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
  static const int kImplicitReceiverOffset = -6 * kPointerSize;
  static const int kConstructorOffset = -5 * kPointerSize;
  static const int kLengthOffset = -4 * kPointerSize;
  static const int kCodeOffset = StandardFrameConstants::kExpressionsOffset;

  static const int kFrameSize =
      StandardFrameConstants::kFixedFrameSize + 4 * kPointerSize;
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


}
}  // namespace v8::internal

#endif  // V8_PPC_FRAMES_PPC_H_

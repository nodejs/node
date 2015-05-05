// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



#ifndef V8_MIPS_FRAMES_MIPS_H_
#define V8_MIPS_FRAMES_MIPS_H_

namespace v8 {
namespace internal {

// Register lists.
// Note that the bit values must match those used in actual instruction
// encoding.
const int kNumRegs = 32;

const RegList kJSCallerSaved =
  1 << 2  |  // v0
  1 << 3  |  // v1
  1 << 4  |  // a0
  1 << 5  |  // a1
  1 << 6  |  // a2
  1 << 7  |  // a3
  1 << 8  |  // a4
  1 << 9  |  // a5
  1 << 10 |  // a6
  1 << 11 |  // a7
  1 << 12 |  // t0
  1 << 13 |  // t1
  1 << 14 |  // t2
  1 << 15;   // t3

const int kNumJSCallerSaved = 14;


// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns a0.code() == 4.
int JSCallerSavedCode(int n);


// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved =
  1 << 16 |  // s0
  1 << 17 |  // s1
  1 << 18 |  // s2
  1 << 19 |  // s3
  1 << 20 |  // s4
  1 << 21 |  // s5
  1 << 22 |  // s6 (roots in Javascript code)
  1 << 23 |  // s7 (cp in Javascript code)
  1 << 30;   // fp/s8

const int kNumCalleeSaved = 9;

const RegList kCalleeSavedFPU =
  1 << 20 |  // f20
  1 << 22 |  // f22
  1 << 24 |  // f24
  1 << 26 |  // f26
  1 << 28 |  // f28
  1 << 30;   // f30

const int kNumCalleeSavedFPU = 6;

const RegList kCallerSavedFPU =
  1 << 0  |  // f0
  1 << 2  |  // f2
  1 << 4  |  // f4
  1 << 6  |  // f6
  1 << 8  |  // f8
  1 << 10 |  // f10
  1 << 12 |  // f12
  1 << 14 |  // f14
  1 << 16 |  // f16
  1 << 18;   // f18


// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
const int kNumSafepointRegisters = 24;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
const RegList kSafepointSavedRegisters = kJSCallerSaved | kCalleeSaved;
const int kNumSafepointSavedRegisters =
    kNumJSCallerSaved + kNumCalleeSaved;

const int kUndefIndex = -1;
// Map with indexes on stack that corresponds to codes of saved registers.
const int kSafepointRegisterStackIndexMap[kNumRegs] = {
  kUndefIndex,  // zero_reg
  kUndefIndex,  // at
  0,   // v0
  1,   // v1
  2,   // a0
  3,   // a1
  4,   // a2
  5,   // a3
  6,   // a4
  7,   // a5
  8,   // a6
  9,   // a7
  10,  // t0
  11,  // t1
  12,  // t2
  13,  // t3
  14,  // s0
  15,  // s1
  16,  // s2
  17,  // s3
  18,  // s4
  19,  // s5
  20,  // s6
  21,  // s7
  kUndefIndex,  // t8
  kUndefIndex,  // t9
  kUndefIndex,  // k0
  kUndefIndex,  // k1
  kUndefIndex,  // gp
  kUndefIndex,  // sp
  22,  // fp
  kUndefIndex
};


// ----------------------------------------------------

class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset =
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
};


class ExitFrameConstants : public AllStatic {
 public:
  static const int kFrameSize = 2 * kPointerSize;

  static const int kCodeOffset = -2 * kPointerSize;
  static const int kSPOffset = -1 * kPointerSize;

  // The caller fields are below the frame pointer on the stack.
  static const int kCallerFPOffset = +0 * kPointerSize;
  // The calling JS function is between FP and PC.
  static const int kCallerPCOffset = +1 * kPointerSize;

  // MIPS-specific: a pointer to the old sp to avoid unnecessary calculations.
  static const int kCallerSPOffset = +2 * kPointerSize;

  // FP-relative displacement of the caller's SP.
  static const int kCallerSPDisplacement = +2 * kPointerSize;

  static const int kConstantPoolOffset = 0;  // Not used.
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
  static const int kConstructorOffset      = -5 * kPointerSize;
  static const int kLengthOffset           = -4 * kPointerSize;
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


} }  // namespace v8::internal

#endif

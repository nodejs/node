// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



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
  1 << 8  |  // t0
  1 << 9  |  // t1
  1 << 10 |  // t2
  1 << 11 |  // t3
  1 << 12 |  // t4
  1 << 13 |  // t5
  1 << 14 |  // t6
  1 << 15;   // t7

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

typedef Object* JSCallerSavedBuffer[kNumJSCallerSaved];

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
  6,   // t0
  7,   // t1
  8,   // t2
  9,   // t3
  10,  // t4
  11,  // t5
  12,  // t6
  13,  // t7
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
  static const int kCallerFPOffset      = -3 * kPointerSize;
};


class ExitFrameConstants : public AllStatic {
 public:
  // See some explanation in MacroAssembler::EnterExitFrame.
  // This marks the top of the extra allocated stack space.
  static const int kStackSpaceOffset = -3 * kPointerSize;

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

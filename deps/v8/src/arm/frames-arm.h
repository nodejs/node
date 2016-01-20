// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM_FRAMES_ARM_H_
#define V8_ARM_FRAMES_ARM_H_

namespace v8 {
namespace internal {


// The ARM ABI does not specify the usage of register r9, which may be reserved
// as the static base or thread register on some platforms, in which case we
// leave it alone. Adjust the value of kR9Available accordingly:
const int kR9Available = 1;  // 1 if available to us, 0 if reserved


// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding
const int kNumRegs = 16;


// Caller-saved/arguments registers
const RegList kJSCallerSaved =
  1 << 0 |  // r0 a1
  1 << 1 |  // r1 a2
  1 << 2 |  // r2 a3
  1 << 3;   // r3 a4

const int kNumJSCallerSaved = 4;

// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns r0.code() == 0
int JSCallerSavedCode(int n);


// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved =
  1 <<  4 |  //  r4 v1
  1 <<  5 |  //  r5 v2
  1 <<  6 |  //  r6 v3
  1 <<  7 |  //  r7 v4 (cp in JavaScript code)
  1 <<  8 |  //  r8 v5 (pp in JavaScript code)
  kR9Available <<  9 |  //  r9 v6
  1 << 10 |  // r10 v7
  1 << 11;   // r11 v8 (fp in JavaScript code)

// When calling into C++ (only for C++ calls that can't cause a GC).
// The call code will take care of lr, fp, etc.
const RegList kCallerSaved =
  1 <<  0 |  // r0
  1 <<  1 |  // r1
  1 <<  2 |  // r2
  1 <<  3 |  // r3
  1 <<  9;   // r9


const int kNumCalleeSaved = 7 + kR9Available;

// Double registers d8 to d15 are callee-saved.
const int kNumDoubleCalleeSaved = 8;


// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
// TODO(regis): Only 8 registers may actually be sufficient. Revisit.
const int kNumSafepointRegisters = 16;

// The embedded constant pool pointer (r8/pp) is not included in the safepoint
// since it is not tagged.  This register is preserved in the stack frame where
// its value will be updated if GC code movement occurs.  Including it in the
// safepoint (where it will not be relocated) would cause a stale value to be
// restored.
const RegList kConstantPointerRegMask =
    FLAG_enable_embedded_constant_pool ? (1 << 8) : 0;
const int kNumConstantPoolPointerReg =
    FLAG_enable_embedded_constant_pool ? 1 : 0;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
const RegList kSafepointSavedRegisters =
    kJSCallerSaved | (kCalleeSaved & ~kConstantPointerRegMask);
const int kNumSafepointSavedRegisters =
    kNumJSCallerSaved + kNumCalleeSaved - kNumConstantPoolPointerReg;

// ----------------------------------------------------


class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset =
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
};


class ExitFrameConstants : public AllStatic {
 public:
  static const int kFrameSize =
      FLAG_enable_embedded_constant_pool ? 3 * kPointerSize : 2 * kPointerSize;

  static const int kConstantPoolOffset =
      FLAG_enable_embedded_constant_pool ? -3 * kPointerSize : 0;
  static const int kCodeOffset         = -2 * kPointerSize;
  static const int kSPOffset           = -1 * kPointerSize;

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
  static const int kParam0Offset   = -2 * kPointerSize;
  static const int kReceiverOffset = -1 * kPointerSize;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_ARM_FRAMES_ARM_H_

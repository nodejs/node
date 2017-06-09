// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_S390_FRAMES_S390_H_
#define V8_S390_FRAMES_S390_H_

namespace v8 {
namespace internal {

// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding
const int kNumRegs = 16;

// Caller-saved/arguments registers
const RegList kJSCallerSaved = 1 << 1 | 1 << 2 |  // r2  a1
                               1 << 3 |           // r3  a2
                               1 << 4 |           // r4  a3
                               1 << 5;            // r5  a4

const int kNumJSCallerSaved = 5;

// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns r0.code() == 0
int JSCallerSavedCode(int n);

// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved =
    1 << 6 |   // r6 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 7 |   // r7 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 8 |   // r8 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 9 |   // r9 (HandleScope logic in MacroAssembler)
    1 << 10 |  // r10 (Roots register in Javascript)
    1 << 11 |  // r11 (fp in Javascript)
    1 << 12 |  // r12 (ip in Javascript)
    1 << 13;   // r13 (cp in Javascript)
// 1 << 15;   // r15 (sp in Javascript)

const int kNumCalleeSaved = 8;

#ifdef V8_TARGET_ARCH_S390X

const RegList kCallerSavedDoubles = 1 << 0 |  // d0
                                    1 << 1 |  // d1
                                    1 << 2 |  // d2
                                    1 << 3 |  // d3
                                    1 << 4 |  // d4
                                    1 << 5 |  // d5
                                    1 << 6 |  // d6
                                    1 << 7;   // d7

const int kNumCallerSavedDoubles = 8;

const RegList kCalleeSavedDoubles = 1 << 8 |   // d8
                                    1 << 9 |   // d9
                                    1 << 10 |  // d10
                                    1 << 11 |  // d11
                                    1 << 12 |  // d12
                                    1 << 13 |  // d12
                                    1 << 14 |  // d12
                                    1 << 15;   // d13

const int kNumCalleeSavedDoubles = 8;

#else

const RegList kCallerSavedDoubles = 1 << 14 |  // d14
                                    1 << 15 |  // d15
                                    1 << 0 |   // d0
                                    1 << 1 |   // d1
                                    1 << 2 |   // d2
                                    1 << 3 |   // d3
                                    1 << 5 |   // d5
                                    1 << 7 |   // d7
                                    1 << 8 |   // d8
                                    1 << 9 |   // d9
                                    1 << 10 |  // d10
                                    1 << 11 |  // d10
                                    1 << 12 |  // d10
                                    1 << 13;   // d11

const int kNumCallerSavedDoubles = 14;

const RegList kCalleeSavedDoubles = 1 << 4 |  // d4
                                    1 << 6;   // d6

const int kNumCalleeSavedDoubles = 2;

#endif

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
// TODO(regis): Only 8 registers may actually be sufficient. Revisit.
const int kNumSafepointRegisters = 16;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
// const RegList kSafepointSavedRegisters = kJSCallerSaved | kCalleeSaved;
// const int kNumSafepointSavedRegisters = kNumJSCallerSaved + kNumCalleeSaved;

// The following constants describe the stack frame linkage area as
// defined by the ABI.

#if V8_TARGET_ARCH_S390X
// [0] Back Chain
// [1] Reserved for compiler use
// [2] GPR 2
// [3] GPR 3
// ...
// [15] GPR 15
// [16] FPR 0
// [17] FPR 2
// [18] FPR 4
// [19] FPR 6
const int kNumRequiredStackFrameSlots = 20;
const int kStackFrameRASlot = 14;
const int kStackFrameSPSlot = 15;
const int kStackFrameExtraParamSlot = 20;
#else
// [0] Back Chain
// [1] Reserved for compiler use
// [2] GPR 2
// [3] GPR 3
// ...
// [15] GPR 15
// [16..17] FPR 0
// [18..19] FPR 2
// [20..21] FPR 4
// [22..23] FPR 6
const int kNumRequiredStackFrameSlots = 24;
const int kStackFrameRASlot = 14;
const int kStackFrameSPSlot = 15;
const int kStackFrameExtraParamSlot = 24;
#endif

// zLinux ABI requires caller frames to include sufficient space for
// callee preserved register save area.
#if V8_TARGET_ARCH_S390X
const int kCalleeRegisterSaveAreaSize = 160;
#elif V8_TARGET_ARCH_S390
const int kCalleeRegisterSaveAreaSize = 96;
#else
const int kCalleeRegisterSaveAreaSize = 0;
#endif

// ----------------------------------------------------

class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset =
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static const int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static const int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);

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
  static const int kFunctionOffset = StandardFrameConstants::kFunctionOffset;

  // Caller SP-relative.
  static const int kParam0Offset = -2 * kPointerSize;
  static const int kReceiverOffset = -1 * kPointerSize;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_S390_FRAMES_S390_H_

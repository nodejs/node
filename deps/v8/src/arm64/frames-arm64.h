// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm64/assembler-arm64.h"
#include "src/arm64/constants-arm64.h"

#ifndef V8_ARM64_FRAMES_ARM64_H_
#define V8_ARM64_FRAMES_ARM64_H_

namespace v8 {
namespace internal {

const int kNumRegs = kNumberOfRegisters;
// Registers x0-x17 are caller-saved.
const int kNumJSCallerSaved = 18;
const RegList kJSCallerSaved = 0x3ffff;

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of eight.
// TODO(all): Refine this number.
const int kNumSafepointRegisters = 32;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
#define kSafepointSavedRegisters CPURegList::GetSafepointSavedRegisters().list()
#define kNumSafepointSavedRegisters \
  CPURegList::GetSafepointSavedRegisters().Count();

class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset =
      -(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
};


class ExitFrameConstants : public AllStatic {
 public:
  static const int kFrameSize            =  2 * kPointerSize;

  static const int kCallerSPDisplacement =  2 * kPointerSize;
  static const int kCallerPCOffset       =  1 * kPointerSize;
  static const int kCallerFPOffset       =  0 * kPointerSize;   // <- fp
  static const int kSPOffset             = -1 * kPointerSize;
  static const int kCodeOffset           = -2 * kPointerSize;
  static const int kLastExitFrameField   = kCodeOffset;

  static const int kConstantPoolOffset   = 0;  // Not used
};


class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kLocal0Offset = StandardFrameConstants::kExpressionsOffset;

  // There are two words on the stack (saved fp and saved lr) between fp and
  // the arguments.
  static const int kLastParameterOffset = 2 * kPointerSize;

  static const int kFunctionOffset = StandardFrameConstants::kMarkerOffset;
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
  static const int kCodeOffset = StandardFrameConstants::kExpressionsOffset;
  static const int kLengthOffset           = -4 * kPointerSize;
  static const int kConstructorOffset      = -5 * kPointerSize;
  static const int kImplicitReceiverOffset = -6 * kPointerSize;

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


inline void StackHandler::SetFp(Address slot, Address fp) {
  Memory::Address_at(slot) = fp;
}


} }  // namespace v8::internal

#endif  // V8_ARM64_FRAMES_ARM64_H_

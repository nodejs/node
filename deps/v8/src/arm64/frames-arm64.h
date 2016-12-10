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

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static const int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static const int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);
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

  static const int kFunctionOffset = StandardFrameConstants::kFunctionOffset;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_FRAMES_ARM64_H_

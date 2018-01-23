// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_FRAMES_ARM64_H_
#define V8_ARM64_FRAMES_ARM64_H_

namespace v8 {
namespace internal {

// The layout of an EntryFrame is as follows:
//
//  slot      Entry frame
//       +---------------------+-----------------------
//   0   |  bad frame pointer  |  <-- frame ptr
//       |   (0xFFF.. FF)      |
//       |- - - - - - - - - - -|
//   1   | stack frame marker  |
//       |      (ENTRY)        |
//       |- - - - - - - - - - -|
//   2   | stack frame marker  |
//       |        (0)          |
//       |- - - - - - - - - - -|
//   3   |     C entry FP      |
//       |- - - - - - - - - - -|
//   4   |   JS entry frame    |
//       |       marker        |
//       |- - - - - - - - - - -|
//   5   |      padding        |  <-- stack ptr
//  -----+---------------------+-----------------------
//
class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset = -3 * kPointerSize;
  static const int kFixedFrameSize = 6 * kPointerSize;
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static const int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static const int kCodeOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  static const int kPaddingOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(2);
  DEFINE_TYPED_FRAME_SIZES(3);
  static const int kLastExitFrameField = kPaddingOffset;

  static const int kConstantPoolOffset = 0;  // Not used
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

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OSR_H_
#define V8_COMPILER_OSR_H_

#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class OptimizedCompilationInfo;

namespace compiler {

class Frame;

// Encapsulates logic relating to OSR compilations as well has handles some
// details of the frame layout.
class OsrHelper {
 public:
  explicit OsrHelper(OptimizedCompilationInfo* info);

  // Prepares the frame w.r.t. OSR.
  void SetupFrame(Frame* frame);

  // Returns the number of unoptimized frame slots for this OSR.
  size_t UnoptimizedFrameSlots() { return stack_slot_count_; }

  // Returns the environment index of the first stack slot.
  static int FirstStackSlotIndex(int parameter_count) {
    // TurboFan environments do not contain the context.
    return 1 + parameter_count;  // receiver + params
  }

 private:
  size_t parameter_count_;
  size_t stack_slot_count_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OSR_H_

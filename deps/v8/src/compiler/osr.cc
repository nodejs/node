// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/osr.h"

#include "src/compilation-info.h"
#include "src/compiler/frame.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {
namespace compiler {

OsrHelper::OsrHelper(CompilationInfo* info)
    : parameter_count_(
          info->shared_info()->bytecode_array()->parameter_count()),
      stack_slot_count_(
          InterpreterFrameConstants::RegisterStackSlotCount(
              info->shared_info()->bytecode_array()->register_count()) +
          InterpreterFrameConstants::kExtraSlotCount) {}

void OsrHelper::SetupFrame(Frame* frame) {
  // The optimized frame will subsume the unoptimized frame. Do so by reserving
  // the first spill slots.
  frame->ReserveSpillSlots(UnoptimizedFrameSlots());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

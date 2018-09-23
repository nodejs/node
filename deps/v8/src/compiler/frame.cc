// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/frame.h"

#include "src/compiler/linkage.h"
#include "src/compiler/register-allocator.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {
namespace compiler {

Frame::Frame(int fixed_frame_size_in_slots)
    : frame_slot_count_(fixed_frame_size_in_slots),
      spilled_callee_register_slot_count_(0),
      stack_slot_count_(0),
      allocated_registers_(NULL),
      allocated_double_registers_(NULL) {}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

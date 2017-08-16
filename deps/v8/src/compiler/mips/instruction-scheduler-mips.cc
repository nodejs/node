// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

bool InstructionScheduler::SchedulerSupported() { return false; }


int InstructionScheduler::GetTargetInstructionFlags(
    const Instruction* instr) const {
  UNIMPLEMENTED();
}


int InstructionScheduler::GetInstructionLatency(const Instruction* instr) {
  UNIMPLEMENTED();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

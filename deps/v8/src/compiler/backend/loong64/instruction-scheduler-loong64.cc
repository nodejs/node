// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/macro-assembler.h"
#include "src/compiler/backend/instruction-scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

// TODO(LOONG_dev): LOONG64 Support instruction scheduler.
bool InstructionScheduler::SchedulerSupported() { return false; }

int InstructionScheduler::GetTargetInstructionFlags(
    const Instruction* instr) const {
  UNREACHABLE();
}

int InstructionScheduler::GetInstructionLatency(const Instruction* instr) {
  UNREACHABLE();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

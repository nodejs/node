// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/control-flow-builders.h"

namespace v8 {
namespace internal {
namespace interpreter {


BreakableControlFlowBuilder::~BreakableControlFlowBuilder() {
  DCHECK(break_sites_.empty());
}


void BreakableControlFlowBuilder::SetBreakTarget(const BytecodeLabel& target) {
  BindLabels(target, &break_sites_);
}


void BreakableControlFlowBuilder::EmitJump(ZoneVector<BytecodeLabel>* sites) {
  sites->push_back(BytecodeLabel());
  builder()->Jump(&sites->back());
}


void BreakableControlFlowBuilder::EmitJumpIfTrue(
    ZoneVector<BytecodeLabel>* sites) {
  sites->push_back(BytecodeLabel());
  builder()->JumpIfTrue(&sites->back());
}


void BreakableControlFlowBuilder::EmitJumpIfUndefined(
    ZoneVector<BytecodeLabel>* sites) {
  sites->push_back(BytecodeLabel());
  builder()->JumpIfUndefined(&sites->back());
}


void BreakableControlFlowBuilder::EmitJumpIfNull(
    ZoneVector<BytecodeLabel>* sites) {
  sites->push_back(BytecodeLabel());
  builder()->JumpIfNull(&sites->back());
}


void BreakableControlFlowBuilder::EmitJump(ZoneVector<BytecodeLabel>* sites,
                                           int index) {
  builder()->Jump(&sites->at(index));
}


void BreakableControlFlowBuilder::EmitJumpIfTrue(
    ZoneVector<BytecodeLabel>* sites, int index) {
  builder()->JumpIfTrue(&sites->at(index));
}


void BreakableControlFlowBuilder::BindLabels(const BytecodeLabel& target,
                                             ZoneVector<BytecodeLabel>* sites) {
  for (size_t i = 0; i < sites->size(); i++) {
    BytecodeLabel& site = sites->at(i);
    builder()->Bind(target, &site);
  }
  sites->clear();
}


LoopBuilder::~LoopBuilder() { DCHECK(continue_sites_.empty()); }


void LoopBuilder::SetContinueTarget(const BytecodeLabel& target) {
  BindLabels(target, &continue_sites_);
}


SwitchBuilder::~SwitchBuilder() {
#ifdef DEBUG
  for (auto site : case_sites_) {
    DCHECK(site.is_bound());
  }
#endif
}


void SwitchBuilder::SetCaseTarget(int index) {
  BytecodeLabel& site = case_sites_.at(index);
  builder()->Bind(&site);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

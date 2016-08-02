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


void BreakableControlFlowBuilder::EmitJumpIfFalse(
    ZoneVector<BytecodeLabel>* sites) {
  sites->push_back(BytecodeLabel());
  builder()->JumpIfFalse(&sites->back());
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


void BreakableControlFlowBuilder::EmitJumpIfFalse(
    ZoneVector<BytecodeLabel>* sites, int index) {
  builder()->JumpIfFalse(&sites->at(index));
}


void BreakableControlFlowBuilder::BindLabels(const BytecodeLabel& target,
                                             ZoneVector<BytecodeLabel>* sites) {
  for (size_t i = 0; i < sites->size(); i++) {
    BytecodeLabel& site = sites->at(i);
    builder()->Bind(target, &site);
  }
  sites->clear();
}


void BlockBuilder::EndBlock() {
  builder()->Bind(&block_end_);
  SetBreakTarget(block_end_);
}


LoopBuilder::~LoopBuilder() { DCHECK(continue_sites_.empty()); }


void LoopBuilder::LoopHeader(ZoneVector<BytecodeLabel>* additional_labels) {
  // Jumps from before the loop header into the loop violate ordering
  // requirements of bytecode basic blocks. The only entry into a loop
  // must be the loop header. Surely breaks is okay? Not if nested
  // and misplaced between the headers.
  DCHECK(break_sites_.empty() && continue_sites_.empty());
  builder()->Bind(&loop_header_);
  for (auto& label : *additional_labels) {
    builder()->Bind(loop_header_, &label);
  }
}


void LoopBuilder::EndLoop() {
  // Loop must have closed form, i.e. all loop elements are within the loop,
  // the loop header precedes the body and next elements in the loop.
  DCHECK(loop_header_.is_bound());
  builder()->Bind(&loop_end_);
  SetBreakTarget(loop_end_);
}

void LoopBuilder::SetContinueTarget() {
  BytecodeLabel target;
  builder()->Bind(&target);
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


void TryCatchBuilder::BeginTry(Register context) {
  builder()->MarkTryBegin(handler_id_, context);
}


void TryCatchBuilder::EndTry() {
  builder()->MarkTryEnd(handler_id_);
  builder()->Jump(&exit_);
  builder()->Bind(&handler_);
  builder()->MarkHandler(handler_id_, true);
}


void TryCatchBuilder::EndCatch() { builder()->Bind(&exit_); }


void TryFinallyBuilder::BeginTry(Register context) {
  builder()->MarkTryBegin(handler_id_, context);
}


void TryFinallyBuilder::LeaveTry() {
  finalization_sites_.push_back(BytecodeLabel());
  builder()->Jump(&finalization_sites_.back());
}


void TryFinallyBuilder::EndTry() {
  builder()->MarkTryEnd(handler_id_);
}


void TryFinallyBuilder::BeginHandler() {
  builder()->Bind(&handler_);
  builder()->MarkHandler(handler_id_, will_catch_);
}


void TryFinallyBuilder::BeginFinally() {
  for (size_t i = 0; i < finalization_sites_.size(); i++) {
    BytecodeLabel& site = finalization_sites_.at(i);
    builder()->Bind(&site);
  }
}


void TryFinallyBuilder::EndFinally() {
  // Nothing to be done here.
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

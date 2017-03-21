// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/control-flow-builders.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {


BreakableControlFlowBuilder::~BreakableControlFlowBuilder() {
  DCHECK(break_labels_.empty() || break_labels_.is_bound());
}

void BreakableControlFlowBuilder::BindBreakTarget() {
  break_labels_.Bind(builder());
}

void BreakableControlFlowBuilder::EmitJump(BytecodeLabels* sites) {
  builder()->Jump(sites->New());
}

void BreakableControlFlowBuilder::EmitJumpIfTrue(BytecodeLabels* sites) {
  builder()->JumpIfTrue(sites->New());
}

void BreakableControlFlowBuilder::EmitJumpIfFalse(BytecodeLabels* sites) {
  builder()->JumpIfFalse(sites->New());
}

void BreakableControlFlowBuilder::EmitJumpIfUndefined(BytecodeLabels* sites) {
  builder()->JumpIfUndefined(sites->New());
}

void BreakableControlFlowBuilder::EmitJumpIfNull(BytecodeLabels* sites) {
  builder()->JumpIfNull(sites->New());
}


void BlockBuilder::EndBlock() {
  builder()->Bind(&block_end_);
  BindBreakTarget();
}

LoopBuilder::~LoopBuilder() {
  DCHECK(continue_labels_.empty() || continue_labels_.is_bound());
  DCHECK(header_labels_.empty() || header_labels_.is_bound());
}

void LoopBuilder::LoopHeader(ZoneVector<BytecodeLabel>* additional_labels) {
  // Jumps from before the loop header into the loop violate ordering
  // requirements of bytecode basic blocks. The only entry into a loop
  // must be the loop header. Surely breaks is okay? Not if nested
  // and misplaced between the headers.
  DCHECK(break_labels_.empty() && continue_labels_.empty());
  builder()->Bind(&loop_header_);
  if (additional_labels != nullptr) {
    for (auto& label : *additional_labels) {
      builder()->Bind(&label);
    }
  }
}

void LoopBuilder::JumpToHeader(int loop_depth) {
  // Pass the proper loop nesting level to the backwards branch, to trigger
  // on-stack replacement when armed for the given loop nesting depth.
  int level = Min(loop_depth, AbstractCode::kMaxLoopNestingMarker - 1);
  // Loop must have closed form, i.e. all loop elements are within the loop,
  // the loop header precedes the body and next elements in the loop.
  DCHECK(loop_header_.is_bound());
  builder()->JumpLoop(&loop_header_, level);
}

void LoopBuilder::EndLoop() {
  BindBreakTarget();
  header_labels_.BindToLabel(builder(), loop_header_);
}

void LoopBuilder::BindContinueTarget() { continue_labels_.Bind(builder()); }

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
  builder()->MarkHandler(handler_id_, catch_prediction_);
}


void TryCatchBuilder::EndCatch() { builder()->Bind(&exit_); }


void TryFinallyBuilder::BeginTry(Register context) {
  builder()->MarkTryBegin(handler_id_, context);
}


void TryFinallyBuilder::LeaveTry() {
  builder()->Jump(finalization_sites_.New());
}


void TryFinallyBuilder::EndTry() {
  builder()->MarkTryEnd(handler_id_);
}


void TryFinallyBuilder::BeginHandler() {
  builder()->Bind(&handler_);
  builder()->MarkHandler(handler_id_, catch_prediction_);
}

void TryFinallyBuilder::BeginFinally() { finalization_sites_.Bind(builder()); }

void TryFinallyBuilder::EndFinally() {
  // Nothing to be done here.
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

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

void BreakableControlFlowBuilder::EmitJumpIfTrue(
    BytecodeArrayBuilder::ToBooleanMode mode, BytecodeLabels* sites) {
  builder()->JumpIfTrue(mode, sites->New());
}

void BreakableControlFlowBuilder::EmitJumpIfFalse(
    BytecodeArrayBuilder::ToBooleanMode mode, BytecodeLabels* sites) {
  builder()->JumpIfFalse(mode, sites->New());
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
  BindBreakTarget();
  // Restore the parent jump table.
  if (generator_jump_table_location_ != nullptr) {
    *generator_jump_table_location_ = parent_generator_jump_table_;
  }
}

void LoopBuilder::LoopHeader() {
  // Jumps from before the loop header into the loop violate ordering
  // requirements of bytecode basic blocks. The only entry into a loop
  // must be the loop header. Surely breaks is okay? Not if nested
  // and misplaced between the headers.
  DCHECK(break_labels_.empty() && continue_labels_.empty());
  builder()->Bind(&loop_header_);
}

void LoopBuilder::LoopHeaderInGenerator(
    BytecodeJumpTable** generator_jump_table, int first_resume_id,
    int resume_count) {
  // Bind all the resume points that are inside the loop to be at the loop
  // header.
  for (int id = first_resume_id; id < first_resume_id + resume_count; ++id) {
    builder()->Bind(*generator_jump_table, id);
  }

  // Create the loop header.
  LoopHeader();

  // Create a new jump table for after the loop header for only these
  // resume points.
  generator_jump_table_location_ = generator_jump_table;
  parent_generator_jump_table_ = *generator_jump_table;
  *generator_jump_table =
      builder()->AllocateJumpTable(resume_count, first_resume_id);
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

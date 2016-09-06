// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/control-builders.h"

namespace v8 {
namespace internal {
namespace compiler {


void IfBuilder::If(Node* condition, BranchHint hint) {
  builder_->NewBranch(condition, hint);
  else_environment_ = environment()->CopyForConditional();
}


void IfBuilder::Then() { builder_->NewIfTrue(); }


void IfBuilder::Else() {
  builder_->NewMerge();
  then_environment_ = environment();
  set_environment(else_environment_);
  builder_->NewIfFalse();
}


void IfBuilder::End() {
  then_environment_->Merge(environment());
  set_environment(then_environment_);
}


void LoopBuilder::BeginLoop(BitVector* assigned, bool is_osr) {
  loop_environment_ = environment()->CopyForLoop(assigned, is_osr);
  continue_environment_ = environment()->CopyAsUnreachable();
  break_environment_ = environment()->CopyAsUnreachable();
  assigned_ = assigned;
}


void LoopBuilder::Continue() {
  continue_environment_->Merge(environment());
  environment()->MarkAsUnreachable();
}


void LoopBuilder::Break() {
  break_environment_->Merge(environment());
  environment()->MarkAsUnreachable();
}


void LoopBuilder::EndBody() {
  continue_environment_->Merge(environment());
  set_environment(continue_environment_);
}


void LoopBuilder::EndLoop() {
  loop_environment_->Merge(environment());
  set_environment(break_environment_);
  ExitLoop();
}


void LoopBuilder::BreakUnless(Node* condition) {
  IfBuilder control_if(builder_);
  control_if.If(condition);
  control_if.Then();
  control_if.Else();
  Break();
  control_if.End();
}


void LoopBuilder::BreakWhen(Node* condition) {
  IfBuilder control_if(builder_);
  control_if.If(condition);
  control_if.Then();
  Break();
  control_if.Else();
  control_if.End();
}

void LoopBuilder::ExitLoop(Node** extra_value_to_rename) {
  if (extra_value_to_rename) {
    environment()->Push(*extra_value_to_rename);
  }
  environment()->PrepareForLoopExit(loop_environment_->GetControlDependency(),
                                    assigned_);
  if (extra_value_to_rename) {
    *extra_value_to_rename = environment()->Pop();
  }
}

void SwitchBuilder::BeginSwitch() {
  body_environment_ = environment()->CopyAsUnreachable();
  label_environment_ = environment()->CopyAsUnreachable();
  break_environment_ = environment()->CopyAsUnreachable();
}


void SwitchBuilder::BeginLabel(int index, Node* condition) {
  builder_->NewBranch(condition);
  label_environment_ = environment()->CopyForConditional();
  builder_->NewIfTrue();
  body_environments_[index] = environment();
}


void SwitchBuilder::EndLabel() {
  set_environment(label_environment_);
  builder_->NewIfFalse();
}


void SwitchBuilder::DefaultAt(int index) {
  label_environment_ = environment()->CopyAsUnreachable();
  body_environments_[index] = environment();
}


void SwitchBuilder::BeginCase(int index) {
  set_environment(body_environments_[index]);
  environment()->Merge(body_environment_);
}


void SwitchBuilder::Break() {
  break_environment_->Merge(environment());
  environment()->MarkAsUnreachable();
}


void SwitchBuilder::EndCase() { body_environment_ = environment(); }


void SwitchBuilder::EndSwitch() {
  break_environment_->Merge(label_environment_);
  break_environment_->Merge(environment());
  set_environment(break_environment_);
}


void BlockBuilder::BeginBlock() {
  break_environment_ = environment()->CopyAsUnreachable();
}


void BlockBuilder::Break() {
  break_environment_->Merge(environment());
  environment()->MarkAsUnreachable();
}


void BlockBuilder::BreakWhen(Node* condition, BranchHint hint) {
  IfBuilder control_if(builder_);
  control_if.If(condition, hint);
  control_if.Then();
  Break();
  control_if.Else();
  control_if.End();
}


void BlockBuilder::BreakUnless(Node* condition, BranchHint hint) {
  IfBuilder control_if(builder_);
  control_if.If(condition, hint);
  control_if.Then();
  control_if.Else();
  Break();
  control_if.End();
}


void BlockBuilder::EndBlock() {
  break_environment_->Merge(environment());
  set_environment(break_environment_);
}


void TryCatchBuilder::BeginTry() {
  exit_environment_ = environment()->CopyAsUnreachable();
  catch_environment_ = environment()->CopyAsUnreachable();
  catch_environment_->Push(the_hole());
}


void TryCatchBuilder::Throw(Node* exception) {
  environment()->Push(exception);
  catch_environment_->Merge(environment());
  environment()->Pop();
  environment()->MarkAsUnreachable();
}


void TryCatchBuilder::EndTry() {
  exit_environment_->Merge(environment());
  exception_node_ = catch_environment_->Pop();
  set_environment(catch_environment_);
}


void TryCatchBuilder::EndCatch() {
  exit_environment_->Merge(environment());
  set_environment(exit_environment_);
}


void TryFinallyBuilder::BeginTry() {
  finally_environment_ = environment()->CopyAsUnreachable();
  finally_environment_->Push(the_hole());
  finally_environment_->Push(the_hole());
}


void TryFinallyBuilder::LeaveTry(Node* token, Node* value) {
  environment()->Push(value);
  environment()->Push(token);
  finally_environment_->Merge(environment());
  environment()->Drop(2);
}


void TryFinallyBuilder::EndTry(Node* fallthrough_token, Node* value) {
  environment()->Push(value);
  environment()->Push(fallthrough_token);
  finally_environment_->Merge(environment());
  environment()->Drop(2);
  token_node_ = finally_environment_->Pop();
  value_node_ = finally_environment_->Pop();
  set_environment(finally_environment_);
}


void TryFinallyBuilder::EndFinally() {
  // Nothing to be done here.
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

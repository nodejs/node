// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "control-builders.h"

namespace v8 {
namespace internal {
namespace compiler {


void IfBuilder::If(Node* condition) {
  builder_->NewBranch(condition);
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


void LoopBuilder::BeginLoop() {
  builder_->NewLoop();
  loop_environment_ = environment()->CopyForLoop();
  continue_environment_ = environment()->CopyAsUnreachable();
  break_environment_ = environment()->CopyAsUnreachable();
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
}


void LoopBuilder::BreakUnless(Node* condition) {
  IfBuilder control_if(builder_);
  control_if.If(condition);
  control_if.Then();
  control_if.Else();
  Break();
  control_if.End();
}


void SwitchBuilder::BeginSwitch() {
  body_environment_ = environment()->CopyAsUnreachable();
  label_environment_ = environment()->CopyAsUnreachable();
  break_environment_ = environment()->CopyAsUnreachable();
  body_environments_.AddBlock(NULL, case_count(), zone());
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


void BlockBuilder::EndBlock() {
  break_environment_->Merge(environment());
  set_environment(break_environment_);
}
}
}
}  // namespace v8::internal::compiler

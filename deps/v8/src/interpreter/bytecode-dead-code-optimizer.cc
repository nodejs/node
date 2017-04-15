// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-dead-code-optimizer.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeDeadCodeOptimizer::BytecodeDeadCodeOptimizer(
    BytecodePipelineStage* next_stage)
    : next_stage_(next_stage), exit_seen_in_block_(false) {}

// override
Handle<BytecodeArray> BytecodeDeadCodeOptimizer::ToBytecodeArray(
    Isolate* isolate, int register_count, int parameter_count,
    Handle<FixedArray> handler_table) {
  return next_stage_->ToBytecodeArray(isolate, register_count, parameter_count,
                                      handler_table);
}

// override
void BytecodeDeadCodeOptimizer::Write(BytecodeNode* node) {
  // Don't emit dead code.
  if (exit_seen_in_block_) return;

  switch (node->bytecode()) {
    case Bytecode::kReturn:
    case Bytecode::kThrow:
    case Bytecode::kReThrow:
      exit_seen_in_block_ = true;
      break;
    default:
      break;
  }

  next_stage_->Write(node);
}

// override
void BytecodeDeadCodeOptimizer::WriteJump(BytecodeNode* node,
                                          BytecodeLabel* label) {
  // Don't emit dead code.
  // TODO(rmcilroy): For forward jumps we could mark the label as dead, thereby
  // avoiding emitting dead code when we bind the label.
  if (exit_seen_in_block_) return;

  switch (node->bytecode()) {
    case Bytecode::kJump:
    case Bytecode::kJumpConstant:
      exit_seen_in_block_ = true;
      break;
    default:
      break;
  }

  next_stage_->WriteJump(node, label);
}

// override
void BytecodeDeadCodeOptimizer::BindLabel(BytecodeLabel* label) {
  next_stage_->BindLabel(label);
  exit_seen_in_block_ = false;
}

// override
void BytecodeDeadCodeOptimizer::BindLabel(const BytecodeLabel& target,
                                          BytecodeLabel* label) {
  next_stage_->BindLabel(target, label);
  // exit_seen_in_block_ was reset when target was bound, so shouldn't be
  // changed here.
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

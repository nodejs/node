// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-peephole-optimizer.h"

#include "src/interpreter/constant-array-builder.h"
#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodePeepholeOptimizer::BytecodePeepholeOptimizer(
    ConstantArrayBuilder* constant_array_builder,
    BytecodePipelineStage* next_stage)
    : constant_array_builder_(constant_array_builder),
      next_stage_(next_stage),
      last_is_discardable_(false) {
  InvalidateLast();
}

void BytecodePeepholeOptimizer::InvalidateLast() {
  last_.set_bytecode(Bytecode::kIllegal);
}

bool BytecodePeepholeOptimizer::LastIsValid() const {
  return last_.bytecode() != Bytecode::kIllegal;
}

void BytecodePeepholeOptimizer::SetLast(const BytecodeNode* const node) {
  last_.Clone(node);
  last_is_discardable_ = true;
}

// override
size_t BytecodePeepholeOptimizer::FlushForOffset() {
  size_t buffered_size = next_stage_->FlushForOffset();
  if (LastIsValid()) {
    if (last_.bytecode() == Bytecode::kNop &&
        !last_.source_info().is_statement()) {
      // The Nop can be dropped as it doesn't have a statement
      // position for the debugger and doesn't have any effects by
      // definition.
      InvalidateLast();
    } else {
      buffered_size += last_.Size();
      last_is_discardable_ = false;
    }
  }
  return buffered_size;
}

// override
void BytecodePeepholeOptimizer::FlushBasicBlock() {
  if (LastIsValid()) {
    next_stage_->Write(&last_);
    InvalidateLast();
  }
  next_stage_->FlushBasicBlock();
}

// override
void BytecodePeepholeOptimizer::Write(BytecodeNode* node) {
  // Attempt optimization if there is an earlier node to optimize with.
  if (LastIsValid()) {
    node = Optimize(node);
    // Only output the last node if it wasn't invalidated by the optimization.
    if (LastIsValid()) {
      next_stage_->Write(&last_);
      InvalidateLast();
    }
  }

  if (node != nullptr) {
    SetLast(node);
  }
}

Handle<Object> BytecodePeepholeOptimizer::GetConstantForIndexOperand(
    const BytecodeNode* const node, int index) const {
  DCHECK_LE(index, node->operand_count());
  DCHECK_EQ(Bytecodes::GetOperandType(node->bytecode(), 0), OperandType::kIdx);
  uint32_t index_operand = node->operand(0);
  return constant_array_builder_->At(index_operand);
}

bool BytecodePeepholeOptimizer::LastBytecodePutsNameInAccumulator() const {
  DCHECK(LastIsValid());
  return (last_.bytecode() == Bytecode::kTypeOf ||
          last_.bytecode() == Bytecode::kToName ||
          (last_.bytecode() == Bytecode::kLdaConstant &&
           GetConstantForIndexOperand(&last_, 0)->IsName()));
}

void BytecodePeepholeOptimizer::UpdateCurrentBytecode(BytecodeNode* current) {
  if (Bytecodes::IsJumpIfToBoolean(current->bytecode()) &&
      Bytecodes::WritesBooleanToAccumulator(last_.bytecode())) {
    // Conditional jumps with boolean conditions are emitted in
    // ToBoolean form by the bytecode array builder,
    // i.e. JumpIfToBooleanTrue rather JumpIfTrue. The ToBoolean element
    // can be removed if the previous bytecode put a boolean value in
    // the accumulator.
    Bytecode jump = Bytecodes::GetJumpWithoutToBoolean(current->bytecode());
    current->set_bytecode(jump, current->operand(0), current->operand_scale());
  } else if (current->bytecode() == Bytecode::kToBooleanLogicalNot &&
             Bytecodes::WritesBooleanToAccumulator(last_.bytecode())) {
    // Logical-nots are emitted in ToBoolean form by the bytecode array
    // builder, The ToBoolean element can be removed if the previous bytecode
    // put a boolean value in the accumulator.
    current->set_bytecode(Bytecode::kLogicalNot);
  }
}

bool BytecodePeepholeOptimizer::CanElideCurrent(
    const BytecodeNode* const current) const {
  if (Bytecodes::IsLdarOrStar(last_.bytecode()) &&
      Bytecodes::IsLdarOrStar(current->bytecode()) &&
      current->operand(0) == last_.operand(0)) {
    // Ldar and Star make the accumulator and register hold equivalent
    // values. Only the first bytecode is needed if there's a sequence
    // of back-to-back Ldar and Star bytecodes with the same operand.
    return true;
  } else if (current->bytecode() == Bytecode::kToName &&
             LastBytecodePutsNameInAccumulator()) {
    // If the previous bytecode ensured a name was in the accumulator,
    // the type coercion ToName() can be elided.
    return true;
  } else {
    // Additional candidates for eliding current:
    // (i) ToNumber if the last puts a number in the accumulator.
    return false;
  }
}

bool BytecodePeepholeOptimizer::CanElideLast(
    const BytecodeNode* const current) const {
  if (!last_is_discardable_) {
    return false;
  }

  if (last_.bytecode() == Bytecode::kNop) {
    // Nop are placeholders for holding source position information
    // and can be elided.
    return true;
  } else if (Bytecodes::IsAccumulatorLoadWithoutEffects(current->bytecode()) &&
             Bytecodes::IsAccumulatorLoadWithoutEffects(last_.bytecode())) {
    // The accumulator is invisible to the debugger. If there is a sequence of
    // consecutive accumulator loads (that don't have side effects) then only
    // the final load is potentially visible.
    return true;
  } else {
    return false;
  }
}

BytecodeNode* BytecodePeepholeOptimizer::Optimize(BytecodeNode* current) {
  UpdateCurrentBytecode(current);

  if (CanElideCurrent(current)) {
    if (current->source_info().is_valid()) {
      current->set_bytecode(Bytecode::kNop);
    } else {
      current = nullptr;
    }
  } else if (CanElideLast(current)) {
    if (last_.source_info().is_valid()) {
      current->source_info().Update(last_.source_info());
    }
    InvalidateLast();
  }
  return current;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

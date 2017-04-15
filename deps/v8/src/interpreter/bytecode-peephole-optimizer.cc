// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-peephole-optimizer.h"

#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodePeepholeOptimizer::BytecodePeepholeOptimizer(
    BytecodePipelineStage* next_stage)
    : next_stage_(next_stage),
      last_(BytecodeNode::Illegal(BytecodeSourceInfo())) {
  InvalidateLast();
}

// override
Handle<BytecodeArray> BytecodePeepholeOptimizer::ToBytecodeArray(
    Isolate* isolate, int register_count, int parameter_count,
    Handle<FixedArray> handler_table) {
  Flush();
  return next_stage_->ToBytecodeArray(isolate, register_count, parameter_count,
                                      handler_table);
}

// override
void BytecodePeepholeOptimizer::BindLabel(BytecodeLabel* label) {
  Flush();
  next_stage_->BindLabel(label);
}

// override
void BytecodePeepholeOptimizer::BindLabel(const BytecodeLabel& target,
                                          BytecodeLabel* label) {
  // There is no need to flush here, it will have been flushed when
  // |target| was bound.
  next_stage_->BindLabel(target, label);
}

// override
void BytecodePeepholeOptimizer::WriteJump(BytecodeNode* node,
                                          BytecodeLabel* label) {
  // Handlers for jump bytecodes do not emit |node| as WriteJump()
  // requires the |label| and having a label argument in all action
  // handlers results in dead work in the non-jump case.
  ApplyPeepholeAction(node);
  next_stage()->WriteJump(node, label);
}

// override
void BytecodePeepholeOptimizer::Write(BytecodeNode* node) {
  // Handlers for non-jump bytecodes run to completion emitting
  // bytecode to next stage as appropriate.
  ApplyPeepholeAction(node);
}

void BytecodePeepholeOptimizer::Flush() {
  if (LastIsValid()) {
    next_stage_->Write(&last_);
    InvalidateLast();
  }
}

void BytecodePeepholeOptimizer::InvalidateLast() {
  last_ = BytecodeNode::Illegal(BytecodeSourceInfo());
}

bool BytecodePeepholeOptimizer::LastIsValid() const {
  return last_.bytecode() != Bytecode::kIllegal;
}

void BytecodePeepholeOptimizer::SetLast(const BytecodeNode* const node) {
  // An action shouldn't leave a NOP as last bytecode unless it has
  // source position information. NOP without source information can
  // always be elided.
  DCHECK(node->bytecode() != Bytecode::kNop || node->source_info().is_valid());
  last_ = *node;
}

bool BytecodePeepholeOptimizer::CanElideLastBasedOnSourcePosition(
    const BytecodeNode* const current) const {
  //
  // The rules for allowing the elision of the last bytecode based
  // on source position are:
  //
  //                     C U R R E N T
  //              +--------+--------+--------+
  //              |  None  |  Expr  |  Stmt  |
  //  L  +--------+--------+--------+--------+
  //     |  None  |  YES   |  YES   |  YES   |
  //  A  +--------+--------+--------+--------+
  //     |  Expr  |  YES   | MAYBE  |  MAYBE |
  //  S  +--------+--------+--------+--------+
  //     |  Stmt  |  YES   |   NO   |   NO   |
  //  T  +--------+--------+--------+--------+
  //
  // The goal is not lose any statement positions and not lose useful
  // expression positions. Whenever the last bytecode is elided it's
  // source position information is applied to the current node
  // updating it if necessary.
  //
  // The last bytecode could be elided for the MAYBE cases if the last
  // bytecode is known not to throw. If it throws, the system would
  // not have correct stack trace information. The appropriate check
  // for this would be Bytecodes::IsWithoutExternalSideEffects(). By
  // default, the upstream bytecode generator filters out unneeded
  // expression position information so there is neglible benefit to
  // handling MAYBE specially. Hence MAYBE is treated the same as NO.
  //
  return (!last_.source_info().is_valid() ||
          !current->source_info().is_valid());
}

namespace {

BytecodeNode TransformLdaSmiBinaryOpToBinaryOpWithSmi(
    Bytecode new_bytecode, BytecodeNode* const last,
    BytecodeNode* const current) {
  DCHECK_EQ(last->bytecode(), Bytecode::kLdaSmi);
  BytecodeNode node(new_bytecode, last->operand(0), current->operand(0),
                    current->operand(1), current->source_info());
  if (last->source_info().is_valid()) {
    node.set_source_info(last->source_info());
  }
  return node;
}

BytecodeNode TransformLdaZeroBinaryOpToBinaryOpWithZero(
    Bytecode new_bytecode, BytecodeNode* const last,
    BytecodeNode* const current) {
  DCHECK_EQ(last->bytecode(), Bytecode::kLdaZero);
  BytecodeNode node(new_bytecode, 0, current->operand(0), current->operand(1),
                    current->source_info());
  if (last->source_info().is_valid()) {
    node.set_source_info(last->source_info());
  }
  return node;
}

BytecodeNode TransformEqualityWithNullOrUndefined(Bytecode new_bytecode,
                                                  BytecodeNode* const last,
                                                  BytecodeNode* const current) {
  DCHECK((last->bytecode() == Bytecode::kLdaNull) ||
         (last->bytecode() == Bytecode::kLdaUndefined));
  DCHECK((current->bytecode() == Bytecode::kTestEqual) ||
         (current->bytecode() == Bytecode::kTestEqualStrict));
  BytecodeNode node(new_bytecode, current->operand(0), current->source_info());
  if (last->source_info().is_valid()) {
    node.set_source_info(last->source_info());
  }
  return node;
}

}  // namespace

void BytecodePeepholeOptimizer::DefaultAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  next_stage()->Write(last());
  SetLast(node);
}

void BytecodePeepholeOptimizer::UpdateLastAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(!LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  SetLast(node);
}

void BytecodePeepholeOptimizer::UpdateLastIfSourceInfoPresentAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(!LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  if (node->source_info().is_valid()) {
    SetLast(node);
  }
}

void BytecodePeepholeOptimizer::ElideCurrentAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  if (node->source_info().is_valid()) {
    // Preserve the source information by replacing the node bytecode
    // with a no op bytecode.
    BytecodeNode new_node(BytecodeNode::Nop(node->source_info()));
    DefaultAction(&new_node);
  } else {
    // Nothing to do, keep last and wait for next bytecode to pair with it.
  }
}

void BytecodePeepholeOptimizer::ElideCurrentIfOperand0MatchesAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  if (last()->operand(0) == node->operand(0)) {
    ElideCurrentAction(node);
  } else {
    DefaultAction(node);
  }
}

void BytecodePeepholeOptimizer::ElideLastAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  if (CanElideLastBasedOnSourcePosition(node)) {
    if (last()->source_info().is_valid()) {
      // |node| can not have a valid source position if the source
      // position of last() is valid (per rules in
      // CanElideLastBasedOnSourcePosition()).
      node->set_source_info(last()->source_info());
    }
    SetLast(node);
  } else {
    DefaultAction(node);
  }
}

void BytecodePeepholeOptimizer::ChangeBytecodeAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  node->replace_bytecode(action_data->bytecode);
  DefaultAction(node);
}

void BytecodePeepholeOptimizer::TransformLdaSmiBinaryOpToBinaryOpWithSmiAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  if (!node->source_info().is_valid() || !last()->source_info().is_valid()) {
    // Fused last and current into current.
    BytecodeNode new_node(TransformLdaSmiBinaryOpToBinaryOpWithSmi(
        action_data->bytecode, last(), node));
    SetLast(&new_node);
  } else {
    DefaultAction(node);
  }
}

void BytecodePeepholeOptimizer::
    TransformLdaZeroBinaryOpToBinaryOpWithZeroAction(
        BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));
  if (!node->source_info().is_valid() || !last()->source_info().is_valid()) {
    // Fused last and current into current.
    BytecodeNode new_node(TransformLdaZeroBinaryOpToBinaryOpWithZero(
        action_data->bytecode, last(), node));
    SetLast(&new_node);
  } else {
    DefaultAction(node);
  }
}

void BytecodePeepholeOptimizer::TransformEqualityWithNullOrUndefinedAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));
  // Fused last and current into current.
  BytecodeNode new_node(TransformEqualityWithNullOrUndefined(
      action_data->bytecode, last(), node));
  SetLast(&new_node);
}

void BytecodePeepholeOptimizer::DefaultJumpAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(Bytecodes::IsJump(node->bytecode()));

  next_stage()->Write(last());
  InvalidateLast();
}

void BytecodePeepholeOptimizer::UpdateLastJumpAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(!LastIsValid());
  DCHECK(Bytecodes::IsJump(node->bytecode()));
}

void BytecodePeepholeOptimizer::ChangeJumpBytecodeAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(Bytecodes::IsJump(node->bytecode()));

  next_stage()->Write(last());
  InvalidateLast();
  node->replace_bytecode(action_data->bytecode);
}

void BytecodePeepholeOptimizer::ElideLastBeforeJumpAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(Bytecodes::IsJump(node->bytecode()));

  if (!CanElideLastBasedOnSourcePosition(node)) {
    next_stage()->Write(last());
  } else if (!node->source_info().is_valid()) {
    node->set_source_info(last()->source_info());
  }
  InvalidateLast();
}

void BytecodePeepholeOptimizer::ApplyPeepholeAction(BytecodeNode* const node) {
  // A single table is used for looking up peephole optimization
  // matches as it is observed to have better performance. This is
  // inspite of the fact that jump bytecodes and non-jump bytecodes
  // have different processing logic, in particular a jump bytecode
  // always needs to emit the jump via WriteJump().
  const PeepholeActionAndData* const action_data =
      PeepholeActionTable::Lookup(last()->bytecode(), node->bytecode());
  switch (action_data->action) {
#define CASE(Action)              \
  case PeepholeAction::k##Action: \
    Action(node, action_data);    \
    break;
    PEEPHOLE_ACTION_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
      break;
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

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
    : next_stage_(next_stage), last_(Bytecode::kIllegal) {
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
  last_.set_bytecode(Bytecode::kIllegal);
}

bool BytecodePeepholeOptimizer::LastIsValid() const {
  return last_.bytecode() != Bytecode::kIllegal;
}

void BytecodePeepholeOptimizer::SetLast(const BytecodeNode* const node) {
  // An action shouldn't leave a NOP as last bytecode unless it has
  // source position information. NOP without source information can
  // always be elided.
  DCHECK(node->bytecode() != Bytecode::kNop || node->source_info().is_valid());

  last_.Clone(node);
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

void TransformLdaStarToLdrLdar(Bytecode new_bytecode, BytecodeNode* const last,
                               BytecodeNode* const current) {
  DCHECK_EQ(current->bytecode(), Bytecode::kStar);

  //
  // An example transformation here would be:
  //
  //   LdaGlobal i0, i1  ____\  LdrGlobal i0, i1, R
  //   Star R            ====/  Ldar R
  //
  // which loads a global value into both a register and the
  // accumulator. However, in the second form the Ldar can often be
  // peephole optimized away unlike the Star in the first form.
  //
  last->Transform(new_bytecode, current->operand(0));
  current->set_bytecode(Bytecode::kLdar, current->operand(0));
}

void TransformLdaSmiBinaryOpToBinaryOpWithSmi(Bytecode new_bytecode,
                                              BytecodeNode* const last,
                                              BytecodeNode* const current) {
  DCHECK_EQ(last->bytecode(), Bytecode::kLdaSmi);
  current->set_bytecode(new_bytecode, last->operand(0), current->operand(0),
                        current->operand(1));
  if (last->source_info().is_valid()) {
    current->source_info_ptr()->Clone(last->source_info());
  }
}

void TransformLdaZeroBinaryOpToBinaryOpWithZero(Bytecode new_bytecode,
                                                BytecodeNode* const last,
                                                BytecodeNode* const current) {
  DCHECK_EQ(last->bytecode(), Bytecode::kLdaZero);
  current->set_bytecode(new_bytecode, 0, current->operand(0),
                        current->operand(1));
  if (last->source_info().is_valid()) {
    current->source_info_ptr()->Clone(last->source_info());
  }
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
    node->set_bytecode(Bytecode::kNop);
    DefaultAction(node);
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
      node->source_info_ptr()->Clone(last()->source_info());
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

void BytecodePeepholeOptimizer::TransformLdaStarToLdrLdarAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  if (!node->source_info().is_statement()) {
    TransformLdaStarToLdrLdar(action_data->bytecode, last(), node);
  }
  DefaultAction(node);
}

void BytecodePeepholeOptimizer::TransformLdaSmiBinaryOpToBinaryOpWithSmiAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(!Bytecodes::IsJump(node->bytecode()));

  if (!node->source_info().is_valid() || !last()->source_info().is_valid()) {
    // Fused last and current into current.
    TransformLdaSmiBinaryOpToBinaryOpWithSmi(action_data->bytecode, last(),
                                             node);
    SetLast(node);
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
    TransformLdaZeroBinaryOpToBinaryOpWithZero(action_data->bytecode, last(),
                                               node);
    SetLast(node);
  } else {
    DefaultAction(node);
  }
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
  node->set_bytecode(action_data->bytecode, node->operand(0));
}

void BytecodePeepholeOptimizer::ElideLastBeforeJumpAction(
    BytecodeNode* const node, const PeepholeActionAndData* action_data) {
  DCHECK(LastIsValid());
  DCHECK(Bytecodes::IsJump(node->bytecode()));

  if (!CanElideLastBasedOnSourcePosition(node)) {
    next_stage()->Write(last());
  } else if (!node->source_info().is_valid()) {
    node->source_info_ptr()->Clone(last()->source_info());
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

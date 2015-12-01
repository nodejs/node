// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/bytecode-graph-builder.h"

#include "src/compiler/linkage.h"
#include "src/compiler/operator-properties.h"
#include "src/interpreter/bytecode-array-iterator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Issues:
// - Need to deal with FrameState / FrameStateBeforeAndAfter / StateValue.
// - Scopes - intimately tied to AST. Need to eval what is needed.
// - Need to resolve closure parameter treatment.
BytecodeGraphBuilder::Environment::Environment(BytecodeGraphBuilder* builder,
                                               int register_count,
                                               int parameter_count,
                                               Node* control_dependency,
                                               Node* context)
    : builder_(builder),
      register_count_(register_count),
      parameter_count_(parameter_count),
      context_(context),
      control_dependency_(control_dependency),
      effect_dependency_(control_dependency),
      values_(builder->local_zone()) {
  // The layout of values_ is:
  //
  // [receiver] [parameters] [registers]
  //
  // parameter[0] is the receiver (this), parameters 1..N are the
  // parameters supplied to the method (arg0..argN-1). The accumulator
  // is stored separately.

  // Parameters including the receiver
  for (int i = 0; i < parameter_count; i++) {
    const char* debug_name = (i == 0) ? "%this" : nullptr;
    const Operator* op = common()->Parameter(i, debug_name);
    Node* parameter = builder->graph()->NewNode(op, graph()->start());
    values()->push_back(parameter);
  }

  // Registers
  register_base_ = static_cast<int>(values()->size());
  Node* undefined_constant = builder->jsgraph()->UndefinedConstant();
  values()->insert(values()->end(), register_count, undefined_constant);

  // Accumulator
  accumulator_ = undefined_constant;
}


int BytecodeGraphBuilder::Environment::RegisterToValuesIndex(
    interpreter::Register the_register) const {
  if (the_register.is_parameter()) {
    return the_register.ToParameterIndex(parameter_count());
  } else {
    return the_register.index() + register_base();
  }
}


void BytecodeGraphBuilder::Environment::BindRegister(
    interpreter::Register the_register, Node* node) {
  int values_index = RegisterToValuesIndex(the_register);
  values()->at(values_index) = node;
}


Node* BytecodeGraphBuilder::Environment::LookupRegister(
    interpreter::Register the_register) const {
  int values_index = RegisterToValuesIndex(the_register);
  return values()->at(values_index);
}


void BytecodeGraphBuilder::Environment::BindAccumulator(Node* node) {
  accumulator_ = node;
}


Node* BytecodeGraphBuilder::Environment::LookupAccumulator() const {
  return accumulator_;
}


bool BytecodeGraphBuilder::Environment::IsMarkedAsUnreachable() const {
  return GetControlDependency()->opcode() == IrOpcode::kDead;
}


void BytecodeGraphBuilder::Environment::MarkAsUnreachable() {
  UpdateControlDependency(builder()->jsgraph()->Dead());
}


BytecodeGraphBuilder::BytecodeGraphBuilder(Zone* local_zone,
                                           CompilationInfo* compilation_info,
                                           JSGraph* jsgraph)
    : local_zone_(local_zone),
      info_(compilation_info),
      jsgraph_(jsgraph),
      input_buffer_size_(0),
      input_buffer_(nullptr),
      exit_controls_(local_zone) {
  bytecode_array_ = handle(info()->shared_info()->bytecode_array());
}


Node* BytecodeGraphBuilder::GetFunctionContext() {
  if (!function_context_.is_set()) {
    // Parameter (arity + 1) is special for the outer context of the function
    const Operator* op =
        common()->Parameter(bytecode_array()->parameter_count(), "%context");
    Node* node = NewNode(op, graph()->start());
    function_context_.set(node);
  }
  return function_context_.get();
}


bool BytecodeGraphBuilder::CreateGraph(bool stack_check) {
  // Set up the basic structure of the graph. Outputs for {Start} are
  // the formal parameters (including the receiver) plus context and
  // closure.

  // The additional count items are for the context and closure.
  int actual_parameter_count = bytecode_array()->parameter_count() + 2;
  graph()->SetStart(graph()->NewNode(common()->Start(actual_parameter_count)));

  Environment env(this, bytecode_array()->register_count(),
                  bytecode_array()->parameter_count(), graph()->start(),
                  GetFunctionContext());
  set_environment(&env);

  // Build function context only if there are context allocated variables.
  if (info()->num_heap_slots() > 0) {
    UNIMPLEMENTED();  // TODO(oth): Write ast-graph-builder equivalent.
  } else {
    // Simply use the outer function context in building the graph.
    CreateGraphBody(stack_check);
  }

  // Finish the basic structure of the graph.
  DCHECK_NE(0u, exit_controls_.size());
  int const input_count = static_cast<int>(exit_controls_.size());
  Node** const inputs = &exit_controls_.front();
  Node* end = graph()->NewNode(common()->End(input_count), input_count, inputs);
  graph()->SetEnd(end);

  return true;
}


void BytecodeGraphBuilder::CreateGraphBody(bool stack_check) {
  // TODO(oth): Review ast-graph-builder equivalent, i.e. arguments
  // object setup, this function variable if used, tracing hooks.
  VisitBytecodes();
}


void BytecodeGraphBuilder::VisitBytecodes() {
  interpreter::BytecodeArrayIterator iterator(bytecode_array());
  while (!iterator.done()) {
    switch (iterator.current_bytecode()) {
#define BYTECODE_CASE(name, ...)       \
  case interpreter::Bytecode::k##name: \
    Visit##name(iterator);             \
    break;
      BYTECODE_LIST(BYTECODE_CASE)
#undef BYTECODE_CODE
    }
    iterator.Advance();
  }
}


void BytecodeGraphBuilder::VisitLdaZero(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->ZeroConstant();
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaSmi8(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->Constant(iterator.GetSmi8Operand(0));
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->Constant(iterator.GetConstantForIndexOperand(0));
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaUndefined(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->UndefinedConstant();
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaNull(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->NullConstant();
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaTheHole(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->TheHoleConstant();
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaTrue(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->TrueConstant();
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaFalse(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->FalseConstant();
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdar(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* value = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  environment()->BindAccumulator(value);
}


void BytecodeGraphBuilder::VisitStar(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* value = environment()->LookupAccumulator();
  environment()->BindRegister(iterator.GetRegisterOperand(0), value);
}


void BytecodeGraphBuilder::VisitLdaGlobal(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitLoadIC(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitKeyedLoadIC(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitStoreIC(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitKeyedStoreIC(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitCall(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::BuildBinaryOp(
    const Operator* js_op, const interpreter::BytecodeArrayIterator& iterator) {
  Node* left = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* right = environment()->LookupAccumulator();
  Node* node = NewNode(js_op, left, right);

  // TODO(oth): Real frame state and environment check pointing.
  int frame_state_count =
      OperatorProperties::GetFrameStateInputCount(node->op());
  for (int i = 0; i < frame_state_count; i++) {
    NodeProperties::ReplaceFrameStateInput(node, i,
                                           jsgraph()->EmptyFrameState());
  }
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitAdd(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildBinaryOp(javascript()->Add(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitSub(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildBinaryOp(javascript()->Subtract(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitMul(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildBinaryOp(javascript()->Multiply(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitDiv(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildBinaryOp(javascript()->Divide(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitMod(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildBinaryOp(javascript()->Modulus(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitTestEqual(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestNotEqual(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestEqualStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestNotEqualStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestLessThan(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestGreaterThan(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestLessThanOrEqual(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestGreaterThanOrEqual(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestIn(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitTestInstanceOf(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitToBoolean(
    const interpreter::BytecodeArrayIterator& ToBoolean) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitJump(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitJumpConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitJumpIfTrue(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitJumpIfTrueConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitJumpIfFalse(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitJumpIfFalseConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  UNIMPLEMENTED();
}


void BytecodeGraphBuilder::VisitReturn(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* control =
      NewNode(common()->Return(), environment()->LookupAccumulator());
  UpdateControlDependencyToLeaveFunction(control);
}


Node** BytecodeGraphBuilder::EnsureInputBufferSize(int size) {
  if (size > input_buffer_size_) {
    size = size + kInputBufferSizeIncrement + input_buffer_size_;
    input_buffer_ = local_zone()->NewArray<Node*>(size);
    input_buffer_size_ = size;
  }
  return input_buffer_;
}


Node* BytecodeGraphBuilder::MakeNode(const Operator* op, int value_input_count,
                                     Node** value_inputs, bool incomplete) {
  DCHECK_EQ(op->ValueInputCount(), value_input_count);

  bool has_context = OperatorProperties::HasContextInput(op);
  int frame_state_count = OperatorProperties::GetFrameStateInputCount(op);
  bool has_control = op->ControlInputCount() == 1;
  bool has_effect = op->EffectInputCount() == 1;

  DCHECK_LT(op->ControlInputCount(), 2);
  DCHECK_LT(op->EffectInputCount(), 2);

  Node* result = NULL;
  if (!has_context && frame_state_count == 0 && !has_control && !has_effect) {
    result = graph()->NewNode(op, value_input_count, value_inputs, incomplete);
  } else {
    int input_count_with_deps = value_input_count;
    if (has_context) ++input_count_with_deps;
    input_count_with_deps += frame_state_count;
    if (has_control) ++input_count_with_deps;
    if (has_effect) ++input_count_with_deps;
    Node** buffer = EnsureInputBufferSize(input_count_with_deps);
    memcpy(buffer, value_inputs, kPointerSize * value_input_count);
    Node** current_input = buffer + value_input_count;
    if (has_context) {
      *current_input++ = environment()->Context();
    }
    for (int i = 0; i < frame_state_count; i++) {
      // The frame state will be inserted later. Here we misuse
      // the {Dead} node as a sentinel to be later overwritten
      // with the real frame state.
      *current_input++ = jsgraph()->Dead();
    }
    if (has_effect) {
      *current_input++ = environment()->GetEffectDependency();
    }
    if (has_control) {
      *current_input++ = environment()->GetControlDependency();
    }
    result = graph()->NewNode(op, input_count_with_deps, buffer, incomplete);
    if (!environment()->IsMarkedAsUnreachable()) {
      // Update the current control dependency for control-producing nodes.
      if (NodeProperties::IsControl(result)) {
        environment()->UpdateControlDependency(result);
      }
      // Update the current effect dependency for effect-producing nodes.
      if (result->op()->EffectOutputCount() > 0) {
        environment()->UpdateEffectDependency(result);
      }
      // Add implicit success continuation for throwing nodes.
      if (!result->op()->HasProperty(Operator::kNoThrow)) {
        const Operator* if_success = common()->IfSuccess();
        Node* on_success = graph()->NewNode(if_success, result);
        environment_->UpdateControlDependency(on_success);
      }
    }
  }

  return result;
}


Node* BytecodeGraphBuilder::MergeControl(Node* control, Node* other) {
  int inputs = control->op()->ControlInputCount() + 1;
  if (control->opcode() == IrOpcode::kLoop) {
    // Control node for loop exists, add input.
    const Operator* op = common()->Loop(inputs);
    control->AppendInput(graph_zone(), other);
    NodeProperties::ChangeOp(control, op);
  } else if (control->opcode() == IrOpcode::kMerge) {
    // Control node for merge exists, add input.
    const Operator* op = common()->Merge(inputs);
    control->AppendInput(graph_zone(), other);
    NodeProperties::ChangeOp(control, op);
  } else {
    // Control node is a singleton, introduce a merge.
    const Operator* op = common()->Merge(inputs);
    Node* merge_inputs[] = {control, other};
    control = graph()->NewNode(op, arraysize(merge_inputs), merge_inputs, true);
  }
  return control;
}


void BytecodeGraphBuilder::UpdateControlDependencyToLeaveFunction(Node* exit) {
  if (environment()->IsMarkedAsUnreachable()) return;
  environment()->MarkAsUnreachable();
  exit_controls_.push_back(exit);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

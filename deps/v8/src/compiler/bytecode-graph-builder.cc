// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/bytecode-graph-builder.h"

#include "src/compiler/bytecode-branch-analysis.h"
#include "src/compiler/linkage.h"
#include "src/compiler/operator-properties.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace compiler {

// Helper for generating frame states for before and after a bytecode.
class BytecodeGraphBuilder::FrameStateBeforeAndAfter {
 public:
  FrameStateBeforeAndAfter(BytecodeGraphBuilder* builder,
                           const interpreter::BytecodeArrayIterator& iterator)
      : builder_(builder),
        id_after_(BailoutId::None()),
        added_to_node_(false),
        output_poke_offset_(0),
        output_poke_count_(0) {
    BailoutId id_before(iterator.current_offset());
    frame_state_before_ = builder_->environment()->Checkpoint(
        id_before, OutputFrameStateCombine::Ignore());
    id_after_ = BailoutId(id_before.ToInt() + iterator.current_bytecode_size());
  }

  ~FrameStateBeforeAndAfter() {
    DCHECK(added_to_node_);
    DCHECK(builder_->environment()->StateValuesAreUpToDate(output_poke_offset_,
                                                           output_poke_count_));
  }

 private:
  friend class Environment;

  void AddToNode(Node* node, OutputFrameStateCombine combine) {
    DCHECK(!added_to_node_);
    int count = OperatorProperties::GetFrameStateInputCount(node->op());
    DCHECK_LE(count, 2);
    if (count >= 1) {
      // Add the frame state for after the operation.
      DCHECK_EQ(IrOpcode::kDead,
                NodeProperties::GetFrameStateInput(node, 0)->opcode());
      Node* frame_state_after =
          builder_->environment()->Checkpoint(id_after_, combine);
      NodeProperties::ReplaceFrameStateInput(node, 0, frame_state_after);
    }

    if (count >= 2) {
      // Add the frame state for before the operation.
      DCHECK_EQ(IrOpcode::kDead,
                NodeProperties::GetFrameStateInput(node, 1)->opcode());
      NodeProperties::ReplaceFrameStateInput(node, 1, frame_state_before_);
    }

    if (!combine.IsOutputIgnored()) {
      output_poke_offset_ = static_cast<int>(combine.GetOffsetToPokeAt());
      output_poke_count_ = node->op()->ValueOutputCount();
    }
    added_to_node_ = true;
  }

  BytecodeGraphBuilder* builder_;
  Node* frame_state_before_;
  BailoutId id_after_;

  bool added_to_node_;
  int output_poke_offset_;
  int output_poke_count_;
};


// Issues:
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
      values_(builder->local_zone()),
      parameters_state_values_(nullptr),
      registers_state_values_(nullptr),
      accumulator_state_values_(nullptr) {
  // The layout of values_ is:
  //
  // [receiver] [parameters] [registers] [accumulator]
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
  accumulator_base_ = static_cast<int>(values()->size());
  values()->push_back(undefined_constant);
}


BytecodeGraphBuilder::Environment::Environment(
    const BytecodeGraphBuilder::Environment* other)
    : builder_(other->builder_),
      register_count_(other->register_count_),
      parameter_count_(other->parameter_count_),
      context_(other->context_),
      control_dependency_(other->control_dependency_),
      effect_dependency_(other->effect_dependency_),
      values_(other->zone()),
      parameters_state_values_(nullptr),
      registers_state_values_(nullptr),
      accumulator_state_values_(nullptr),
      register_base_(other->register_base_),
      accumulator_base_(other->accumulator_base_) {
  values_ = other->values_;
}


int BytecodeGraphBuilder::Environment::RegisterToValuesIndex(
    interpreter::Register the_register) const {
  if (the_register.is_parameter()) {
    return the_register.ToParameterIndex(parameter_count());
  } else {
    return the_register.index() + register_base();
  }
}


Node* BytecodeGraphBuilder::Environment::LookupAccumulator() const {
  return values()->at(accumulator_base_);
}


Node* BytecodeGraphBuilder::Environment::LookupRegister(
    interpreter::Register the_register) const {
  if (the_register.is_function_context()) {
    return builder()->GetFunctionContext();
  } else if (the_register.is_function_closure()) {
    return builder()->GetFunctionClosure();
  } else if (the_register.is_new_target()) {
    return builder()->GetNewTarget();
  } else {
    int values_index = RegisterToValuesIndex(the_register);
    return values()->at(values_index);
  }
}


void BytecodeGraphBuilder::Environment::ExchangeRegisters(
    interpreter::Register reg0, interpreter::Register reg1) {
  int reg0_index = RegisterToValuesIndex(reg0);
  int reg1_index = RegisterToValuesIndex(reg1);
  Node* saved_reg0_value = values()->at(reg0_index);
  values()->at(reg0_index) = values()->at(reg1_index);
  values()->at(reg1_index) = saved_reg0_value;
}


void BytecodeGraphBuilder::Environment::BindAccumulator(
    Node* node, FrameStateBeforeAndAfter* states) {
  if (states) {
    states->AddToNode(node, OutputFrameStateCombine::PokeAt(0));
  }
  values()->at(accumulator_base_) = node;
}


void BytecodeGraphBuilder::Environment::BindRegister(
    interpreter::Register the_register, Node* node,
    FrameStateBeforeAndAfter* states) {
  int values_index = RegisterToValuesIndex(the_register);
  if (states) {
    states->AddToNode(node, OutputFrameStateCombine::PokeAt(accumulator_base_ -
                                                            values_index));
  }
  values()->at(values_index) = node;
}


void BytecodeGraphBuilder::Environment::BindRegistersToProjections(
    interpreter::Register first_reg, Node* node,
    FrameStateBeforeAndAfter* states) {
  int values_index = RegisterToValuesIndex(first_reg);
  if (states) {
    states->AddToNode(node, OutputFrameStateCombine::PokeAt(accumulator_base_ -
                                                            values_index));
  }
  for (int i = 0; i < node->op()->ValueOutputCount(); i++) {
    values()->at(values_index + i) =
        builder()->NewNode(common()->Projection(i), node);
  }
}


void BytecodeGraphBuilder::Environment::RecordAfterState(
    Node* node, FrameStateBeforeAndAfter* states) {
  states->AddToNode(node, OutputFrameStateCombine::Ignore());
}


bool BytecodeGraphBuilder::Environment::IsMarkedAsUnreachable() const {
  return GetControlDependency()->opcode() == IrOpcode::kDead;
}


void BytecodeGraphBuilder::Environment::MarkAsUnreachable() {
  UpdateControlDependency(builder()->jsgraph()->Dead());
}


BytecodeGraphBuilder::Environment*
BytecodeGraphBuilder::Environment::CopyForLoop() {
  PrepareForLoop();
  return new (zone()) Environment(this);
}


BytecodeGraphBuilder::Environment*
BytecodeGraphBuilder::Environment::CopyForConditional() const {
  return new (zone()) Environment(this);
}


void BytecodeGraphBuilder::Environment::Merge(
    BytecodeGraphBuilder::Environment* other) {
  // Nothing to do if the other environment is dead.
  if (other->IsMarkedAsUnreachable()) {
    return;
  }

  // Create a merge of the control dependencies of both environments and update
  // the current environment's control dependency accordingly.
  Node* control = builder()->MergeControl(GetControlDependency(),
                                          other->GetControlDependency());
  UpdateControlDependency(control);

  // Create a merge of the effect dependencies of both environments and update
  // the current environment's effect dependency accordingly.
  Node* effect = builder()->MergeEffect(GetEffectDependency(),
                                        other->GetEffectDependency(), control);
  UpdateEffectDependency(effect);

  // Introduce Phi nodes for values that have differing input at merge points,
  // potentially extending an existing Phi node if possible.
  context_ = builder()->MergeValue(context_, other->context_, control);
  for (size_t i = 0; i < values_.size(); i++) {
    values_[i] = builder()->MergeValue(values_[i], other->values_[i], control);
  }
}


void BytecodeGraphBuilder::Environment::PrepareForLoop() {
  // Create a control node for the loop header.
  Node* control = builder()->NewLoop();

  // Create a Phi for external effects.
  Node* effect = builder()->NewEffectPhi(1, GetEffectDependency(), control);
  UpdateEffectDependency(effect);

  // Assume everything in the loop is updated.
  context_ = builder()->NewPhi(1, context_, control);
  int size = static_cast<int>(values()->size());
  for (int i = 0; i < size; i++) {
    values()->at(i) = builder()->NewPhi(1, values()->at(i), control);
  }

  // Connect to the loop end.
  Node* terminate = builder()->graph()->NewNode(
      builder()->common()->Terminate(), effect, control);
  builder()->exit_controls_.push_back(terminate);
}


bool BytecodeGraphBuilder::Environment::StateValuesRequireUpdate(
    Node** state_values, int offset, int count) {
  if (!builder()->info()->is_deoptimization_enabled()) {
    return false;
  }
  if (*state_values == nullptr) {
    return true;
  }
  DCHECK_EQ((*state_values)->InputCount(), count);
  DCHECK_LE(static_cast<size_t>(offset + count), values()->size());
  Node** env_values = (count == 0) ? nullptr : &values()->at(offset);
  for (int i = 0; i < count; i++) {
    if ((*state_values)->InputAt(i) != env_values[i]) {
      return true;
    }
  }
  return false;
}


void BytecodeGraphBuilder::Environment::UpdateStateValues(Node** state_values,
                                                          int offset,
                                                          int count) {
  if (StateValuesRequireUpdate(state_values, offset, count)) {
    const Operator* op = common()->StateValues(count);
    (*state_values) = graph()->NewNode(op, count, &values()->at(offset));
  }
}


Node* BytecodeGraphBuilder::Environment::Checkpoint(
    BailoutId bailout_id, OutputFrameStateCombine combine) {
  if (!builder()->info()->is_deoptimization_enabled()) {
    return builder()->jsgraph()->EmptyFrameState();
  }

  // TODO(rmcilroy): Consider using StateValuesCache for some state values.
  UpdateStateValues(&parameters_state_values_, 0, parameter_count());
  UpdateStateValues(&registers_state_values_, register_base(),
                    register_count());
  UpdateStateValues(&accumulator_state_values_, accumulator_base(), 1);

  const Operator* op = common()->FrameState(
      bailout_id, combine, builder()->frame_state_function_info());
  Node* result = graph()->NewNode(
      op, parameters_state_values_, registers_state_values_,
      accumulator_state_values_, Context(), builder()->GetFunctionClosure(),
      builder()->graph()->start());

  return result;
}


bool BytecodeGraphBuilder::Environment::StateValuesAreUpToDate(
    Node** state_values, int offset, int count, int output_poke_start,
    int output_poke_end) {
  DCHECK_LE(static_cast<size_t>(offset + count), values()->size());
  for (int i = 0; i < count; i++, offset++) {
    if (offset < output_poke_start || offset >= output_poke_end) {
      if ((*state_values)->InputAt(i) != values()->at(offset)) {
        return false;
      }
    }
  }
  return true;
}


bool BytecodeGraphBuilder::Environment::StateValuesAreUpToDate(
    int output_poke_offset, int output_poke_count) {
  // Poke offset is relative to the top of the stack (i.e., the accumulator).
  int output_poke_start = accumulator_base() - output_poke_offset;
  int output_poke_end = output_poke_start + output_poke_count;
  return StateValuesAreUpToDate(&parameters_state_values_, 0, parameter_count(),
                                output_poke_start, output_poke_end) &&
         StateValuesAreUpToDate(&registers_state_values_, register_base(),
                                register_count(), output_poke_start,
                                output_poke_end) &&
         StateValuesAreUpToDate(&accumulator_state_values_, accumulator_base(),
                                1, output_poke_start, output_poke_end);
}


BytecodeGraphBuilder::BytecodeGraphBuilder(Zone* local_zone,
                                           CompilationInfo* compilation_info,
                                           JSGraph* jsgraph)
    : local_zone_(local_zone),
      info_(compilation_info),
      jsgraph_(jsgraph),
      bytecode_array_(handle(info()->shared_info()->bytecode_array())),
      frame_state_function_info_(common()->CreateFrameStateFunctionInfo(
          FrameStateType::kInterpretedFunction,
          bytecode_array()->parameter_count(),
          bytecode_array()->register_count(), info()->shared_info(),
          CALL_MAINTAINS_NATIVE_CONTEXT)),
      merge_environments_(local_zone),
      loop_header_environments_(local_zone),
      input_buffer_size_(0),
      input_buffer_(nullptr),
      exit_controls_(local_zone) {}


Node* BytecodeGraphBuilder::GetNewTarget() {
  if (!new_target_.is_set()) {
    int params = bytecode_array()->parameter_count();
    int index = Linkage::GetJSCallNewTargetParamIndex(params);
    const Operator* op = common()->Parameter(index, "%new.target");
    Node* node = NewNode(op, graph()->start());
    new_target_.set(node);
  }
  return new_target_.get();
}


Node* BytecodeGraphBuilder::GetFunctionContext() {
  if (!function_context_.is_set()) {
    int params = bytecode_array()->parameter_count();
    int index = Linkage::GetJSCallContextParamIndex(params);
    const Operator* op = common()->Parameter(index, "%context");
    Node* node = NewNode(op, graph()->start());
    function_context_.set(node);
  }
  return function_context_.get();
}


Node* BytecodeGraphBuilder::GetFunctionClosure() {
  if (!function_closure_.is_set()) {
    int index = Linkage::kJSCallClosureParamIndex;
    const Operator* op = common()->Parameter(index, "%closure");
    Node* node = NewNode(op, graph()->start());
    function_closure_.set(node);
  }
  return function_closure_.get();
}


Node* BytecodeGraphBuilder::BuildLoadObjectField(Node* object, int offset) {
  return NewNode(jsgraph()->machine()->Load(MachineType::AnyTagged()), object,
                 jsgraph()->IntPtrConstant(offset - kHeapObjectTag));
}


Node* BytecodeGraphBuilder::BuildLoadImmutableObjectField(Node* object,
                                                          int offset) {
  return graph()->NewNode(jsgraph()->machine()->Load(MachineType::AnyTagged()),
                          object,
                          jsgraph()->IntPtrConstant(offset - kHeapObjectTag),
                          graph()->start(), graph()->start());
}


Node* BytecodeGraphBuilder::BuildLoadNativeContextField(int index) {
  const Operator* op =
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true);
  Node* native_context = NewNode(op, environment()->Context());
  return NewNode(javascript()->LoadContext(0, index, true), native_context);
}


Node* BytecodeGraphBuilder::BuildLoadFeedbackVector() {
  if (!feedback_vector_.is_set()) {
    Node* closure = GetFunctionClosure();
    Node* shared = BuildLoadImmutableObjectField(
        closure, JSFunction::kSharedFunctionInfoOffset);
    Node* vector = BuildLoadImmutableObjectField(
        shared, SharedFunctionInfo::kFeedbackVectorOffset);
    feedback_vector_.set(vector);
  }
  return feedback_vector_.get();
}


VectorSlotPair BytecodeGraphBuilder::CreateVectorSlotPair(int slot_id) {
  Handle<TypeFeedbackVector> feedback_vector = info()->feedback_vector();
  FeedbackVectorSlot slot;
  if (slot_id >= TypeFeedbackVector::kReservedIndexCount) {
    slot = feedback_vector->ToSlot(slot_id);
  }
  return VectorSlotPair(feedback_vector, slot);
}


bool BytecodeGraphBuilder::CreateGraph(bool stack_check) {
  // Set up the basic structure of the graph. Outputs for {Start} are
  // the formal parameters (including the receiver) plus context and
  // closure.

  // Set up the basic structure of the graph. Outputs for {Start} are the formal
  // parameters (including the receiver) plus new target, number of arguments,
  // context and closure.
  int actual_parameter_count = bytecode_array()->parameter_count() + 4;
  graph()->SetStart(graph()->NewNode(common()->Start(actual_parameter_count)));

  Environment env(this, bytecode_array()->register_count(),
                  bytecode_array()->parameter_count(), graph()->start(),
                  GetFunctionContext());
  set_environment(&env);

  CreateGraphBody(stack_check);

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

  if (stack_check) {
    Node* node = NewNode(javascript()->StackCheck());
    PrepareEntryFrameState(node);
  }

  VisitBytecodes();
}


void BytecodeGraphBuilder::VisitBytecodes() {
  BytecodeBranchAnalysis analysis(bytecode_array(), local_zone());
  analysis.Analyze();
  set_branch_analysis(&analysis);
  interpreter::BytecodeArrayIterator iterator(bytecode_array());
  set_bytecode_iterator(&iterator);
  while (!iterator.done()) {
    int current_offset = iterator.current_offset();
    if (analysis.is_reachable(current_offset)) {
      MergeEnvironmentsOfForwardBranches(current_offset);
      BuildLoopHeaderForBackwardBranches(current_offset);

      switch (iterator.current_bytecode()) {
#define BYTECODE_CASE(name, ...)       \
  case interpreter::Bytecode::k##name: \
    Visit##name(iterator);             \
    break;
        BYTECODE_LIST(BYTECODE_CASE)
#undef BYTECODE_CODE
      }
    }
    iterator.Advance();
  }
  set_branch_analysis(nullptr);
  set_bytecode_iterator(nullptr);
}


void BytecodeGraphBuilder::VisitLdaZero(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->ZeroConstant();
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaSmi8(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->Constant(iterator.GetImmediateOperand(0));
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaConstantWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node = jsgraph()->Constant(iterator.GetConstantForIndexOperand(0));
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


void BytecodeGraphBuilder::VisitMov(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* value = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  environment()->BindRegister(iterator.GetRegisterOperand(1), value);
}


void BytecodeGraphBuilder::VisitExchange(
    const interpreter::BytecodeArrayIterator& iterator) {
  environment()->ExchangeRegisters(iterator.GetRegisterOperand(0),
                                   iterator.GetRegisterOperand(1));
}


void BytecodeGraphBuilder::VisitExchangeWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  environment()->ExchangeRegisters(iterator.GetRegisterOperand(0),
                                   iterator.GetRegisterOperand(1));
}


void BytecodeGraphBuilder::BuildLoadGlobal(
    const interpreter::BytecodeArrayIterator& iterator,
    TypeofMode typeof_mode) {
  FrameStateBeforeAndAfter states(this, iterator);
  Handle<Name> name =
      Handle<Name>::cast(iterator.GetConstantForIndexOperand(0));
  VectorSlotPair feedback = CreateVectorSlotPair(iterator.GetIndexOperand(1));

  const Operator* op = javascript()->LoadGlobal(name, feedback, typeof_mode);
  Node* node = NewNode(op, BuildLoadFeedbackVector());
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitLdaGlobalSloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildLoadGlobal(iterator, TypeofMode::NOT_INSIDE_TYPEOF);
}


void BytecodeGraphBuilder::VisitLdaGlobalStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildLoadGlobal(iterator, TypeofMode::NOT_INSIDE_TYPEOF);
}


void BytecodeGraphBuilder::VisitLdaGlobalInsideTypeofSloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildLoadGlobal(iterator, TypeofMode::INSIDE_TYPEOF);
}


void BytecodeGraphBuilder::VisitLdaGlobalInsideTypeofStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildLoadGlobal(iterator, TypeofMode::INSIDE_TYPEOF);
}


void BytecodeGraphBuilder::VisitLdaGlobalSloppyWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildLoadGlobal(iterator, TypeofMode::NOT_INSIDE_TYPEOF);
}


void BytecodeGraphBuilder::VisitLdaGlobalStrictWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildLoadGlobal(iterator, TypeofMode::NOT_INSIDE_TYPEOF);
}


void BytecodeGraphBuilder::VisitLdaGlobalInsideTypeofSloppyWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildLoadGlobal(iterator, TypeofMode::INSIDE_TYPEOF);
}


void BytecodeGraphBuilder::VisitLdaGlobalInsideTypeofStrictWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildLoadGlobal(iterator, TypeofMode::INSIDE_TYPEOF);
}


void BytecodeGraphBuilder::BuildStoreGlobal(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Handle<Name> name =
      Handle<Name>::cast(iterator.GetConstantForIndexOperand(0));
  VectorSlotPair feedback = CreateVectorSlotPair(iterator.GetIndexOperand(1));
  Node* value = environment()->LookupAccumulator();

  const Operator* op =
      javascript()->StoreGlobal(language_mode(), name, feedback);
  Node* node = NewNode(op, value, BuildLoadFeedbackVector());
  environment()->RecordAfterState(node, &states);
}


void BytecodeGraphBuilder::VisitStaGlobalSloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildStoreGlobal(iterator);
}


void BytecodeGraphBuilder::VisitStaGlobalStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildStoreGlobal(iterator);
}

void BytecodeGraphBuilder::VisitStaGlobalSloppyWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildStoreGlobal(iterator);
}


void BytecodeGraphBuilder::VisitStaGlobalStrictWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildStoreGlobal(iterator);
}


void BytecodeGraphBuilder::VisitLdaContextSlot(
    const interpreter::BytecodeArrayIterator& iterator) {
  // TODO(mythria): LoadContextSlots are unrolled by the required depth when
  // generating bytecode. Hence the value of depth is always 0. Update this
  // code, when the implementation changes.
  // TODO(mythria): immutable flag is also set to false. This information is not
  // available in bytecode array. update this code when the implementation
  // changes.
  const Operator* op =
      javascript()->LoadContext(0, iterator.GetIndexOperand(1), false);
  Node* context = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* node = NewNode(op, context);
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitLdaContextSlotWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  VisitLdaContextSlot(iterator);
}


void BytecodeGraphBuilder::VisitStaContextSlot(
    const interpreter::BytecodeArrayIterator& iterator) {
  // TODO(mythria): LoadContextSlots are unrolled by the required depth when
  // generating bytecode. Hence the value of depth is always 0. Update this
  // code, when the implementation changes.
  const Operator* op =
      javascript()->StoreContext(0, iterator.GetIndexOperand(1));
  Node* context = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* value = environment()->LookupAccumulator();
  NewNode(op, context, value);
}


void BytecodeGraphBuilder::VisitStaContextSlotWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  VisitStaContextSlot(iterator);
}


void BytecodeGraphBuilder::BuildLdaLookupSlot(
    TypeofMode typeof_mode,
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Handle<String> name =
      Handle<String>::cast(iterator.GetConstantForIndexOperand(0));
  const Operator* op = javascript()->LoadDynamic(name, typeof_mode);
  Node* value =
      NewNode(op, BuildLoadFeedbackVector(), environment()->Context());
  environment()->BindAccumulator(value, &states);
}


void BytecodeGraphBuilder::VisitLdaLookupSlot(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildLdaLookupSlot(TypeofMode::NOT_INSIDE_TYPEOF, iterator);
}


void BytecodeGraphBuilder::VisitLdaLookupSlotInsideTypeof(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildLdaLookupSlot(TypeofMode::INSIDE_TYPEOF, iterator);
}


void BytecodeGraphBuilder::BuildStaLookupSlot(
    LanguageMode language_mode,
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* value = environment()->LookupAccumulator();
  Node* name = jsgraph()->Constant(iterator.GetConstantForIndexOperand(0));
  Node* language = jsgraph()->Constant(language_mode);
  const Operator* op = javascript()->CallRuntime(Runtime::kStoreLookupSlot, 4);
  Node* store = NewNode(op, value, environment()->Context(), name, language);
  environment()->BindAccumulator(store, &states);
}


void BytecodeGraphBuilder::VisitLdaLookupSlotWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  VisitLdaLookupSlot(iterator);
}


void BytecodeGraphBuilder::VisitLdaLookupSlotInsideTypeofWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  VisitLdaLookupSlotInsideTypeof(iterator);
}


void BytecodeGraphBuilder::VisitStaLookupSlotSloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildStaLookupSlot(LanguageMode::SLOPPY, iterator);
}


void BytecodeGraphBuilder::VisitStaLookupSlotStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildStaLookupSlot(LanguageMode::STRICT, iterator);
}


void BytecodeGraphBuilder::VisitStaLookupSlotSloppyWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  VisitStaLookupSlotSloppy(iterator);
}


void BytecodeGraphBuilder::VisitStaLookupSlotStrictWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  VisitStaLookupSlotStrict(iterator);
}


void BytecodeGraphBuilder::BuildNamedLoad(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* object = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Handle<Name> name =
      Handle<Name>::cast(iterator.GetConstantForIndexOperand(1));
  VectorSlotPair feedback = CreateVectorSlotPair(iterator.GetIndexOperand(2));

  const Operator* op = javascript()->LoadNamed(language_mode(), name, feedback);
  Node* node = NewNode(op, object, BuildLoadFeedbackVector());
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitLoadICSloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildNamedLoad(iterator);
}


void BytecodeGraphBuilder::VisitLoadICStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildNamedLoad(iterator);
}


void BytecodeGraphBuilder::VisitLoadICSloppyWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildNamedLoad(iterator);
}


void BytecodeGraphBuilder::VisitLoadICStrictWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildNamedLoad(iterator);
}


void BytecodeGraphBuilder::BuildKeyedLoad(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* key = environment()->LookupAccumulator();
  Node* object = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  VectorSlotPair feedback = CreateVectorSlotPair(iterator.GetIndexOperand(1));

  const Operator* op = javascript()->LoadProperty(language_mode(), feedback);
  Node* node = NewNode(op, object, key, BuildLoadFeedbackVector());
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitKeyedLoadICSloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildKeyedLoad(iterator);
}


void BytecodeGraphBuilder::VisitKeyedLoadICStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildKeyedLoad(iterator);
}


void BytecodeGraphBuilder::VisitKeyedLoadICSloppyWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildKeyedLoad(iterator);
}


void BytecodeGraphBuilder::VisitKeyedLoadICStrictWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildKeyedLoad(iterator);
}


void BytecodeGraphBuilder::BuildNamedStore(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* value = environment()->LookupAccumulator();
  Node* object = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Handle<Name> name =
      Handle<Name>::cast(iterator.GetConstantForIndexOperand(1));
  VectorSlotPair feedback = CreateVectorSlotPair(iterator.GetIndexOperand(2));

  const Operator* op =
      javascript()->StoreNamed(language_mode(), name, feedback);
  Node* node = NewNode(op, object, value, BuildLoadFeedbackVector());
  environment()->RecordAfterState(node, &states);
}


void BytecodeGraphBuilder::VisitStoreICSloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildNamedStore(iterator);
}


void BytecodeGraphBuilder::VisitStoreICStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildNamedStore(iterator);
}


void BytecodeGraphBuilder::VisitStoreICSloppyWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildNamedStore(iterator);
}


void BytecodeGraphBuilder::VisitStoreICStrictWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildNamedStore(iterator);
}


void BytecodeGraphBuilder::BuildKeyedStore(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* value = environment()->LookupAccumulator();
  Node* object = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* key = environment()->LookupRegister(iterator.GetRegisterOperand(1));
  VectorSlotPair feedback = CreateVectorSlotPair(iterator.GetIndexOperand(2));

  const Operator* op = javascript()->StoreProperty(language_mode(), feedback);
  Node* node = NewNode(op, object, key, value, BuildLoadFeedbackVector());
  environment()->RecordAfterState(node, &states);
}


void BytecodeGraphBuilder::VisitKeyedStoreICSloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildKeyedStore(iterator);
}


void BytecodeGraphBuilder::VisitKeyedStoreICStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildKeyedStore(iterator);
}


void BytecodeGraphBuilder::VisitKeyedStoreICSloppyWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildKeyedStore(iterator);
}


void BytecodeGraphBuilder::VisitKeyedStoreICStrictWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildKeyedStore(iterator);
}


void BytecodeGraphBuilder::VisitPushContext(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* context = environment()->LookupAccumulator();
  environment()->BindRegister(iterator.GetRegisterOperand(0), context);
  environment()->SetContext(context);
}


void BytecodeGraphBuilder::VisitPopContext(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* context = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  environment()->SetContext(context);
}


void BytecodeGraphBuilder::VisitCreateClosure(
    const interpreter::BytecodeArrayIterator& iterator) {
  Handle<SharedFunctionInfo> shared_info =
      Handle<SharedFunctionInfo>::cast(iterator.GetConstantForIndexOperand(0));
  PretenureFlag tenured =
      iterator.GetImmediateOperand(1) ? TENURED : NOT_TENURED;
  const Operator* op = javascript()->CreateClosure(shared_info, tenured);
  Node* closure = NewNode(op);
  environment()->BindAccumulator(closure);
}


void BytecodeGraphBuilder::VisitCreateClosureWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  VisitCreateClosure(iterator);
}


void BytecodeGraphBuilder::BuildCreateArguments(
    CreateArgumentsParameters::Type type,
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  const Operator* op = javascript()->CreateArguments(type, 0);
  Node* object = NewNode(op, GetFunctionClosure());
  environment()->BindAccumulator(object, &states);
}


void BytecodeGraphBuilder::VisitCreateMappedArguments(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCreateArguments(CreateArgumentsParameters::kMappedArguments, iterator);
}


void BytecodeGraphBuilder::VisitCreateUnmappedArguments(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCreateArguments(CreateArgumentsParameters::kUnmappedArguments, iterator);
}


void BytecodeGraphBuilder::BuildCreateLiteral(
    const Operator* op, const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* literal = NewNode(op, GetFunctionClosure());
  environment()->BindAccumulator(literal, &states);
}


void BytecodeGraphBuilder::BuildCreateRegExpLiteral(
    const interpreter::BytecodeArrayIterator& iterator) {
  Handle<String> constant_pattern =
      Handle<String>::cast(iterator.GetConstantForIndexOperand(0));
  int literal_index = iterator.GetIndexOperand(1);
  int literal_flags = iterator.GetImmediateOperand(2);
  const Operator* op = javascript()->CreateLiteralRegExp(
      constant_pattern, literal_flags, literal_index);
  BuildCreateLiteral(op, iterator);
}


void BytecodeGraphBuilder::VisitCreateRegExpLiteral(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCreateRegExpLiteral(iterator);
}


void BytecodeGraphBuilder::VisitCreateRegExpLiteralWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCreateRegExpLiteral(iterator);
}


void BytecodeGraphBuilder::BuildCreateArrayLiteral(
    const interpreter::BytecodeArrayIterator& iterator) {
  Handle<FixedArray> constant_elements =
      Handle<FixedArray>::cast(iterator.GetConstantForIndexOperand(0));
  int literal_index = iterator.GetIndexOperand(1);
  int literal_flags = iterator.GetImmediateOperand(2);
  const Operator* op = javascript()->CreateLiteralArray(
      constant_elements, literal_flags, literal_index);
  BuildCreateLiteral(op, iterator);
}


void BytecodeGraphBuilder::VisitCreateArrayLiteral(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCreateArrayLiteral(iterator);
}


void BytecodeGraphBuilder::VisitCreateArrayLiteralWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCreateArrayLiteral(iterator);
}


void BytecodeGraphBuilder::BuildCreateObjectLiteral(
    const interpreter::BytecodeArrayIterator& iterator) {
  Handle<FixedArray> constant_properties =
      Handle<FixedArray>::cast(iterator.GetConstantForIndexOperand(0));
  int literal_index = iterator.GetIndexOperand(1);
  int literal_flags = iterator.GetImmediateOperand(2);
  const Operator* op = javascript()->CreateLiteralObject(
      constant_properties, literal_flags, literal_index);
  BuildCreateLiteral(op, iterator);
}


void BytecodeGraphBuilder::VisitCreateObjectLiteral(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCreateObjectLiteral(iterator);
}


void BytecodeGraphBuilder::VisitCreateObjectLiteralWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCreateObjectLiteral(iterator);
}


Node* BytecodeGraphBuilder::ProcessCallArguments(const Operator* call_op,
                                                 Node* callee,
                                                 interpreter::Register receiver,
                                                 size_t arity) {
  Node** all = info()->zone()->NewArray<Node*>(static_cast<int>(arity));
  all[0] = callee;
  all[1] = environment()->LookupRegister(receiver);
  int receiver_index = receiver.index();
  for (int i = 2; i < static_cast<int>(arity); ++i) {
    all[i] = environment()->LookupRegister(
        interpreter::Register(receiver_index + i - 1));
  }
  Node* value = MakeNode(call_op, static_cast<int>(arity), all, false);
  return value;
}


void BytecodeGraphBuilder::BuildCall(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  // TODO(rmcilroy): Set receiver_hint correctly based on whether the receiver
  // register has been loaded with null / undefined explicitly or we are sure it
  // is not null / undefined.
  ConvertReceiverMode receiver_hint = ConvertReceiverMode::kAny;
  Node* callee = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  interpreter::Register receiver = iterator.GetRegisterOperand(1);
  size_t arg_count = iterator.GetCountOperand(2);
  VectorSlotPair feedback = CreateVectorSlotPair(iterator.GetIndexOperand(3));

  const Operator* call = javascript()->CallFunction(
      arg_count + 2, language_mode(), feedback, receiver_hint);
  Node* value = ProcessCallArguments(call, callee, receiver, arg_count + 2);
  environment()->BindAccumulator(value, &states);
}


void BytecodeGraphBuilder::VisitCall(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCall(iterator);
}


void BytecodeGraphBuilder::VisitCallWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCall(iterator);
}


void BytecodeGraphBuilder::VisitCallJSRuntime(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* callee = BuildLoadNativeContextField(iterator.GetIndexOperand(0));
  interpreter::Register receiver = iterator.GetRegisterOperand(1);
  size_t arg_count = iterator.GetCountOperand(2);

  // Create node to perform the JS runtime call.
  const Operator* call =
      javascript()->CallFunction(arg_count + 2, language_mode());
  Node* value = ProcessCallArguments(call, callee, receiver, arg_count + 2);
  environment()->BindAccumulator(value, &states);
}


Node* BytecodeGraphBuilder::ProcessCallRuntimeArguments(
    const Operator* call_runtime_op, interpreter::Register first_arg,
    size_t arity) {
  Node** all = info()->zone()->NewArray<Node*>(arity);
  int first_arg_index = first_arg.index();
  for (int i = 0; i < static_cast<int>(arity); ++i) {
    all[i] = environment()->LookupRegister(
        interpreter::Register(first_arg_index + i));
  }
  Node* value = MakeNode(call_runtime_op, static_cast<int>(arity), all, false);
  return value;
}


void BytecodeGraphBuilder::VisitCallRuntime(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Runtime::FunctionId functionId =
      static_cast<Runtime::FunctionId>(iterator.GetIndexOperand(0));
  interpreter::Register first_arg = iterator.GetRegisterOperand(1);
  size_t arg_count = iterator.GetCountOperand(2);

  // Create node to perform the runtime call.
  const Operator* call = javascript()->CallRuntime(functionId, arg_count);
  Node* value = ProcessCallRuntimeArguments(call, first_arg, arg_count);
  environment()->BindAccumulator(value, &states);
}


void BytecodeGraphBuilder::VisitCallRuntimeForPair(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Runtime::FunctionId functionId =
      static_cast<Runtime::FunctionId>(iterator.GetIndexOperand(0));
  interpreter::Register first_arg = iterator.GetRegisterOperand(1);
  size_t arg_count = iterator.GetCountOperand(2);
  interpreter::Register first_return = iterator.GetRegisterOperand(3);

  // Create node to perform the runtime call.
  const Operator* call = javascript()->CallRuntime(functionId, arg_count);
  Node* return_pair = ProcessCallRuntimeArguments(call, first_arg, arg_count);
  environment()->BindRegistersToProjections(first_return, return_pair, &states);
}


Node* BytecodeGraphBuilder::ProcessCallNewArguments(
    const Operator* call_new_op, interpreter::Register callee,
    interpreter::Register first_arg, size_t arity) {
  Node** all = info()->zone()->NewArray<Node*>(arity);
  all[0] = environment()->LookupRegister(callee);
  int first_arg_index = first_arg.index();
  for (int i = 1; i < static_cast<int>(arity) - 1; ++i) {
    all[i] = environment()->LookupRegister(
        interpreter::Register(first_arg_index + i - 1));
  }
  // Original constructor is the same as the callee.
  all[arity - 1] = environment()->LookupRegister(callee);
  Node* value = MakeNode(call_new_op, static_cast<int>(arity), all, false);
  return value;
}


void BytecodeGraphBuilder::VisitNew(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  interpreter::Register callee = iterator.GetRegisterOperand(0);
  interpreter::Register first_arg = iterator.GetRegisterOperand(1);
  size_t arg_count = iterator.GetCountOperand(2);

  // TODO(turbofan): Pass the feedback here.
  const Operator* call = javascript()->CallConstruct(
      static_cast<int>(arg_count) + 2, VectorSlotPair());
  Node* value = ProcessCallNewArguments(call, callee, first_arg, arg_count + 2);
  environment()->BindAccumulator(value, &states);
}


void BytecodeGraphBuilder::VisitThrow(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* value = environment()->LookupAccumulator();
  // TODO(mythria): Change to Runtime::kThrow when we have deoptimization
  // information support in the interpreter.
  NewNode(javascript()->CallRuntime(Runtime::kReThrow, 1), value);
  Node* control = NewNode(common()->Throw(), value);
  environment()->RecordAfterState(control, &states);
  UpdateControlDependencyToLeaveFunction(control);
}


void BytecodeGraphBuilder::BuildBinaryOp(
    const Operator* js_op, const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* left = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* right = environment()->LookupAccumulator();
  Node* node = NewNode(js_op, left, right);
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitAdd(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->Add(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitSub(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->Subtract(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitMul(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->Multiply(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitDiv(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->Divide(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitMod(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->Modulus(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitBitwiseOr(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->BitwiseOr(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitBitwiseXor(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->BitwiseXor(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitBitwiseAnd(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->BitwiseAnd(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitShiftLeft(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->ShiftLeft(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitShiftRight(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->ShiftRight(language_mode(), hints), iterator);
}


void BytecodeGraphBuilder::VisitShiftRightLogical(
    const interpreter::BytecodeArrayIterator& iterator) {
  BinaryOperationHints hints = BinaryOperationHints::Any();
  BuildBinaryOp(javascript()->ShiftRightLogical(language_mode(), hints),
                iterator);
}


void BytecodeGraphBuilder::VisitInc(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  const Operator* js_op =
      javascript()->Add(language_mode(), BinaryOperationHints::Any());
  Node* node = NewNode(js_op, environment()->LookupAccumulator(),
                       jsgraph()->OneConstant());
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitDec(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  const Operator* js_op =
      javascript()->Subtract(language_mode(), BinaryOperationHints::Any());
  Node* node = NewNode(js_op, environment()->LookupAccumulator(),
                       jsgraph()->OneConstant());
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitLogicalNot(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* value = NewNode(javascript()->ToBoolean(ToBooleanHint::kAny),
                        environment()->LookupAccumulator());
  Node* node = NewNode(common()->Select(MachineRepresentation::kTagged), value,
                       jsgraph()->FalseConstant(), jsgraph()->TrueConstant());
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::VisitTypeOf(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* node =
      NewNode(javascript()->TypeOf(), environment()->LookupAccumulator());
  environment()->BindAccumulator(node);
}


void BytecodeGraphBuilder::BuildDelete(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* key = environment()->LookupAccumulator();
  Node* object = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* node =
      NewNode(javascript()->DeleteProperty(language_mode()), object, key);
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitDeletePropertyStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_strict(language_mode()));
  BuildDelete(iterator);
}


void BytecodeGraphBuilder::VisitDeletePropertySloppy(
    const interpreter::BytecodeArrayIterator& iterator) {
  DCHECK(is_sloppy(language_mode()));
  BuildDelete(iterator);
}


void BytecodeGraphBuilder::VisitDeleteLookupSlot(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* name = environment()->LookupAccumulator();
  const Operator* op = javascript()->CallRuntime(Runtime::kDeleteLookupSlot, 2);
  Node* result = NewNode(op, environment()->Context(), name);
  environment()->BindAccumulator(result, &states);
}


void BytecodeGraphBuilder::BuildCompareOp(
    const Operator* js_op, const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* left = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* right = environment()->LookupAccumulator();
  Node* node = NewNode(js_op, left, right);
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitTestEqual(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->Equal(), iterator);
}


void BytecodeGraphBuilder::VisitTestNotEqual(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->NotEqual(), iterator);
}


void BytecodeGraphBuilder::VisitTestEqualStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->StrictEqual(), iterator);
}


void BytecodeGraphBuilder::VisitTestNotEqualStrict(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->StrictNotEqual(), iterator);
}


void BytecodeGraphBuilder::VisitTestLessThan(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->LessThan(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitTestGreaterThan(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->GreaterThan(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitTestLessThanOrEqual(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->LessThanOrEqual(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitTestGreaterThanOrEqual(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->GreaterThanOrEqual(language_mode()), iterator);
}


void BytecodeGraphBuilder::VisitTestIn(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->HasProperty(), iterator);
}


void BytecodeGraphBuilder::VisitTestInstanceOf(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCompareOp(javascript()->InstanceOf(), iterator);
}


void BytecodeGraphBuilder::BuildCastOperator(
    const Operator* js_op, const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* node = NewNode(js_op, environment()->LookupAccumulator());
  environment()->BindAccumulator(node, &states);
}


void BytecodeGraphBuilder::VisitToName(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCastOperator(javascript()->ToName(), iterator);
}


void BytecodeGraphBuilder::VisitToObject(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCastOperator(javascript()->ToObject(), iterator);
}


void BytecodeGraphBuilder::VisitToNumber(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildCastOperator(javascript()->ToNumber(), iterator);
}


void BytecodeGraphBuilder::VisitJump(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJump();
}


void BytecodeGraphBuilder::VisitJumpConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJump();
}


void BytecodeGraphBuilder::VisitJumpConstantWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJump();
}


void BytecodeGraphBuilder::VisitJumpIfTrue(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->TrueConstant());
}


void BytecodeGraphBuilder::VisitJumpIfTrueConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->TrueConstant());
}


void BytecodeGraphBuilder::VisitJumpIfTrueConstantWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->TrueConstant());
}


void BytecodeGraphBuilder::VisitJumpIfFalse(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->FalseConstant());
}


void BytecodeGraphBuilder::VisitJumpIfFalseConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->FalseConstant());
}


void BytecodeGraphBuilder::VisitJumpIfFalseConstantWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->FalseConstant());
}


void BytecodeGraphBuilder::VisitJumpIfToBooleanTrue(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfToBooleanEqual(jsgraph()->TrueConstant());
}


void BytecodeGraphBuilder::VisitJumpIfToBooleanTrueConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfToBooleanEqual(jsgraph()->TrueConstant());
}


void BytecodeGraphBuilder::VisitJumpIfToBooleanTrueConstantWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfToBooleanEqual(jsgraph()->TrueConstant());
}


void BytecodeGraphBuilder::VisitJumpIfToBooleanFalse(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfToBooleanEqual(jsgraph()->FalseConstant());
}


void BytecodeGraphBuilder::VisitJumpIfToBooleanFalseConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfToBooleanEqual(jsgraph()->FalseConstant());
}


void BytecodeGraphBuilder::VisitJumpIfToBooleanFalseConstantWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfToBooleanEqual(jsgraph()->FalseConstant());
}


void BytecodeGraphBuilder::VisitJumpIfNull(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->NullConstant());
}


void BytecodeGraphBuilder::VisitJumpIfNullConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->NullConstant());
}


void BytecodeGraphBuilder::VisitJumpIfNullConstantWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->NullConstant());
}


void BytecodeGraphBuilder::VisitJumpIfUndefined(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->UndefinedConstant());
}


void BytecodeGraphBuilder::VisitJumpIfUndefinedConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->UndefinedConstant());
}


void BytecodeGraphBuilder::VisitJumpIfUndefinedConstantWide(
    const interpreter::BytecodeArrayIterator& iterator) {
  BuildJumpIfEqual(jsgraph()->UndefinedConstant());
}


void BytecodeGraphBuilder::VisitReturn(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* control =
      NewNode(common()->Return(), environment()->LookupAccumulator());
  UpdateControlDependencyToLeaveFunction(control);
  set_environment(nullptr);
}


void BytecodeGraphBuilder::VisitForInPrepare(
    const interpreter::BytecodeArrayIterator& iterator) {
  Node* prepare = nullptr;
  {
    FrameStateBeforeAndAfter states(this, iterator);
    Node* receiver = environment()->LookupAccumulator();
    prepare = NewNode(javascript()->ForInPrepare(), receiver);
    environment()->RecordAfterState(prepare, &states);
  }
  // Project cache_type, cache_array, cache_length into register
  // operands 1, 2, 3.
  for (int i = 0; i < 3; i++) {
    environment()->BindRegister(iterator.GetRegisterOperand(i),
                                NewNode(common()->Projection(i), prepare));
  }
}


void BytecodeGraphBuilder::VisitForInDone(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* index = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* cache_length =
      environment()->LookupRegister(iterator.GetRegisterOperand(1));
  Node* exit_cond = NewNode(javascript()->ForInDone(), index, cache_length);
  environment()->BindAccumulator(exit_cond, &states);
}


void BytecodeGraphBuilder::VisitForInNext(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* receiver =
      environment()->LookupRegister(iterator.GetRegisterOperand(0));
  Node* cache_type =
      environment()->LookupRegister(iterator.GetRegisterOperand(1));
  Node* cache_array =
      environment()->LookupRegister(iterator.GetRegisterOperand(2));
  Node* index = environment()->LookupRegister(iterator.GetRegisterOperand(3));
  Node* value = NewNode(javascript()->ForInNext(), receiver, cache_array,
                        cache_type, index);
  environment()->BindAccumulator(value, &states);
}


void BytecodeGraphBuilder::VisitForInStep(
    const interpreter::BytecodeArrayIterator& iterator) {
  FrameStateBeforeAndAfter states(this, iterator);
  Node* index = environment()->LookupRegister(iterator.GetRegisterOperand(0));
  index = NewNode(javascript()->ForInStep(), index);
  environment()->BindAccumulator(index, &states);
}


void BytecodeGraphBuilder::MergeEnvironmentsOfBackwardBranches(
    int source_offset, int target_offset) {
  DCHECK_GE(source_offset, target_offset);
  const ZoneVector<int>* branch_sites =
      branch_analysis()->BackwardBranchesTargetting(target_offset);
  if (branch_sites->back() == source_offset) {
    // The set of back branches is complete, merge them.
    DCHECK_GE(branch_sites->at(0), target_offset);
    Environment* merged = merge_environments_[branch_sites->at(0)];
    for (size_t i = 1; i < branch_sites->size(); i++) {
      DCHECK_GE(branch_sites->at(i), target_offset);
      merged->Merge(merge_environments_[branch_sites->at(i)]);
    }
    // And now merge with loop header environment created when loop
    // header was visited.
    loop_header_environments_[target_offset]->Merge(merged);
  }
}


void BytecodeGraphBuilder::MergeEnvironmentsOfForwardBranches(
    int source_offset) {
  if (branch_analysis()->forward_branches_target(source_offset)) {
    // Merge environments of branches that reach this bytecode.
    auto branch_sites =
        branch_analysis()->ForwardBranchesTargetting(source_offset);
    DCHECK_LT(branch_sites->at(0), source_offset);
    Environment* merged = merge_environments_[branch_sites->at(0)];
    for (size_t i = 1; i < branch_sites->size(); i++) {
      DCHECK_LT(branch_sites->at(i), source_offset);
      merged->Merge(merge_environments_[branch_sites->at(i)]);
    }
    if (environment()) {
      merged->Merge(environment());
    }
    set_environment(merged);
  }
}


void BytecodeGraphBuilder::BuildLoopHeaderForBackwardBranches(
    int source_offset) {
  if (branch_analysis()->backward_branches_target(source_offset)) {
    // Add loop header and store a copy so we can connect merged back
    // edge inputs to the loop header.
    loop_header_environments_[source_offset] = environment()->CopyForLoop();
  }
}


void BytecodeGraphBuilder::BuildJump(int source_offset, int target_offset) {
  DCHECK_NULL(merge_environments_[source_offset]);
  merge_environments_[source_offset] = environment();
  if (source_offset >= target_offset) {
    MergeEnvironmentsOfBackwardBranches(source_offset, target_offset);
  }
  set_environment(nullptr);
}


void BytecodeGraphBuilder::BuildJump() {
  int source_offset = bytecode_iterator()->current_offset();
  int target_offset = bytecode_iterator()->GetJumpTargetOffset();
  BuildJump(source_offset, target_offset);
}


void BytecodeGraphBuilder::BuildConditionalJump(Node* condition) {
  int source_offset = bytecode_iterator()->current_offset();
  NewBranch(condition);
  Environment* if_false_environment = environment()->CopyForConditional();
  NewIfTrue();
  BuildJump(source_offset, bytecode_iterator()->GetJumpTargetOffset());
  set_environment(if_false_environment);
  NewIfFalse();
}


void BytecodeGraphBuilder::BuildJumpIfEqual(Node* comperand) {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition =
      NewNode(javascript()->StrictEqual(), accumulator, comperand);
  BuildConditionalJump(condition);
}


void BytecodeGraphBuilder::BuildJumpIfToBooleanEqual(Node* comperand) {
  Node* accumulator = environment()->LookupAccumulator();
  Node* to_boolean =
      NewNode(javascript()->ToBoolean(ToBooleanHint::kAny), accumulator);
  Node* condition = NewNode(javascript()->StrictEqual(), to_boolean, comperand);
  BuildConditionalJump(condition);
}


Node** BytecodeGraphBuilder::EnsureInputBufferSize(int size) {
  if (size > input_buffer_size_) {
    size = size + kInputBufferSizeIncrement + input_buffer_size_;
    input_buffer_ = local_zone()->NewArray<Node*>(size);
    input_buffer_size_ = size;
  }
  return input_buffer_;
}


void BytecodeGraphBuilder::PrepareEntryFrameState(Node* node) {
  DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
  DCHECK_EQ(IrOpcode::kDead,
            NodeProperties::GetFrameStateInput(node, 0)->opcode());
  NodeProperties::ReplaceFrameStateInput(
      node, 0, environment()->Checkpoint(BailoutId(0),
                                         OutputFrameStateCombine::Ignore()));
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

  Node* result = nullptr;
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


Node* BytecodeGraphBuilder::NewPhi(int count, Node* input, Node* control) {
  const Operator* phi_op = common()->Phi(MachineRepresentation::kTagged, count);
  Node** buffer = EnsureInputBufferSize(count + 1);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer, true);
}


Node* BytecodeGraphBuilder::NewEffectPhi(int count, Node* input,
                                         Node* control) {
  const Operator* phi_op = common()->EffectPhi(count);
  Node** buffer = EnsureInputBufferSize(count + 1);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer, true);
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


Node* BytecodeGraphBuilder::MergeEffect(Node* value, Node* other,
                                        Node* control) {
  int inputs = control->op()->ControlInputCount();
  if (value->opcode() == IrOpcode::kEffectPhi &&
      NodeProperties::GetControlInput(value) == control) {
    // Phi already exists, add input.
    value->InsertInput(graph_zone(), inputs - 1, other);
    NodeProperties::ChangeOp(value, common()->EffectPhi(inputs));
  } else if (value != other) {
    // Phi does not exist yet, introduce one.
    value = NewEffectPhi(inputs, value, control);
    value->ReplaceInput(inputs - 1, other);
  }
  return value;
}


Node* BytecodeGraphBuilder::MergeValue(Node* value, Node* other,
                                       Node* control) {
  int inputs = control->op()->ControlInputCount();
  if (value->opcode() == IrOpcode::kPhi &&
      NodeProperties::GetControlInput(value) == control) {
    // Phi already exists, add input.
    value->InsertInput(graph_zone(), inputs - 1, other);
    NodeProperties::ChangeOp(
        value, common()->Phi(MachineRepresentation::kTagged, inputs));
  } else if (value != other) {
    // Phi does not exist yet, introduce one.
    value = NewPhi(inputs, value, control);
    value->ReplaceInput(inputs - 1, other);
  }
  return value;
}


void BytecodeGraphBuilder::UpdateControlDependencyToLeaveFunction(Node* exit) {
  if (environment()->IsMarkedAsUnreachable()) return;
  environment()->MarkAsUnreachable();
  exit_controls_.push_back(exit);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/bytecode-graph-builder.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-generator.h"
#include "src/objects/literal-objects-inl.h"
#include "src/vector-slot-pair.h"

namespace v8 {
namespace internal {
namespace compiler {

// The abstract execution environment simulates the content of the interpreter
// register file. The environment performs SSA-renaming of all tracked nodes at
// split and merge points in the control flow.
class BytecodeGraphBuilder::Environment : public ZoneObject {
 public:
  Environment(BytecodeGraphBuilder* builder, int register_count,
              int parameter_count,
              interpreter::Register incoming_new_target_or_generator,
              Node* control_dependency);

  // Specifies whether environment binding methods should attach frame state
  // inputs to nodes representing the value being bound. This is done because
  // the {OutputFrameStateCombine} is closely related to the binding method.
  enum FrameStateAttachmentMode { kAttachFrameState, kDontAttachFrameState };

  int parameter_count() const { return parameter_count_; }
  int register_count() const { return register_count_; }

  Node* LookupAccumulator() const;
  Node* LookupRegister(interpreter::Register the_register) const;
  Node* LookupGeneratorState() const;

  void BindAccumulator(Node* node,
                       FrameStateAttachmentMode mode = kDontAttachFrameState);
  void BindRegister(interpreter::Register the_register, Node* node,
                    FrameStateAttachmentMode mode = kDontAttachFrameState);
  void BindRegistersToProjections(
      interpreter::Register first_reg, Node* node,
      FrameStateAttachmentMode mode = kDontAttachFrameState);
  void BindGeneratorState(Node* node);
  void RecordAfterState(Node* node,
                        FrameStateAttachmentMode mode = kDontAttachFrameState);

  // Effect dependency tracked by this environment.
  Node* GetEffectDependency() { return effect_dependency_; }
  void UpdateEffectDependency(Node* dependency) {
    effect_dependency_ = dependency;
  }

  // Preserve a checkpoint of the environment for the IR graph. Any
  // further mutation of the environment will not affect checkpoints.
  Node* Checkpoint(BailoutId bytecode_offset, OutputFrameStateCombine combine,
                   const BytecodeLivenessState* liveness);

  // Control dependency tracked by this environment.
  Node* GetControlDependency() const { return control_dependency_; }
  void UpdateControlDependency(Node* dependency) {
    control_dependency_ = dependency;
  }

  Node* Context() const { return context_; }
  void SetContext(Node* new_context) { context_ = new_context; }

  Environment* Copy();
  void Merge(Environment* other, const BytecodeLivenessState* liveness);

  void FillWithOsrValues();
  void PrepareForLoop(const BytecodeLoopAssignments& assignments,
                      const BytecodeLivenessState* liveness);
  void PrepareForLoopExit(Node* loop,
                          const BytecodeLoopAssignments& assignments,
                          const BytecodeLivenessState* liveness);

 private:
  explicit Environment(const Environment* copy);

  bool StateValuesRequireUpdate(Node** state_values, Node** values, int count);
  void UpdateStateValues(Node** state_values, Node** values, int count);
  Node* GetStateValuesFromCache(Node** values, int count,
                                const BitVector* liveness, int liveness_offset);

  int RegisterToValuesIndex(interpreter::Register the_register) const;

  Zone* zone() const { return builder_->local_zone(); }
  Graph* graph() const { return builder_->graph(); }
  CommonOperatorBuilder* common() const { return builder_->common(); }
  BytecodeGraphBuilder* builder() const { return builder_; }
  const NodeVector* values() const { return &values_; }
  NodeVector* values() { return &values_; }
  int register_base() const { return register_base_; }
  int accumulator_base() const { return accumulator_base_; }

  BytecodeGraphBuilder* builder_;
  int register_count_;
  int parameter_count_;
  Node* context_;
  Node* control_dependency_;
  Node* effect_dependency_;
  NodeVector values_;
  Node* parameters_state_values_;
  Node* generator_state_;
  int register_base_;
  int accumulator_base_;
};

// A helper for creating a temporary sub-environment for simple branches.
struct BytecodeGraphBuilder::SubEnvironment final {
 public:
  explicit SubEnvironment(BytecodeGraphBuilder* builder)
      : builder_(builder), parent_(builder->environment()->Copy()) {}

  ~SubEnvironment() { builder_->set_environment(parent_); }

 private:
  BytecodeGraphBuilder* builder_;
  BytecodeGraphBuilder::Environment* parent_;
};

// Issues:
// - Scopes - intimately tied to AST. Need to eval what is needed.
// - Need to resolve closure parameter treatment.
BytecodeGraphBuilder::Environment::Environment(
    BytecodeGraphBuilder* builder, int register_count, int parameter_count,
    interpreter::Register incoming_new_target_or_generator,
    Node* control_dependency)
    : builder_(builder),
      register_count_(register_count),
      parameter_count_(parameter_count),
      control_dependency_(control_dependency),
      effect_dependency_(control_dependency),
      values_(builder->local_zone()),
      parameters_state_values_(nullptr),
      generator_state_(nullptr) {
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

  // Context
  int context_index = Linkage::GetJSCallContextParamIndex(parameter_count);
  const Operator* op = common()->Parameter(context_index, "%context");
  context_ = builder->graph()->NewNode(op, graph()->start());

  // Incoming new.target or generator register
  if (incoming_new_target_or_generator.is_valid()) {
    int new_target_index =
        Linkage::GetJSCallNewTargetParamIndex(parameter_count);
    const Operator* op = common()->Parameter(new_target_index, "%new.target");
    Node* new_target_node = builder->graph()->NewNode(op, graph()->start());

    int values_index = RegisterToValuesIndex(incoming_new_target_or_generator);
    values()->at(values_index) = new_target_node;
  }
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
      parameters_state_values_(other->parameters_state_values_),
      generator_state_(other->generator_state_),
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

Node* BytecodeGraphBuilder::Environment::LookupGeneratorState() const {
  DCHECK_NOT_NULL(generator_state_);
  return generator_state_;
}

Node* BytecodeGraphBuilder::Environment::LookupRegister(
    interpreter::Register the_register) const {
  if (the_register.is_current_context()) {
    return Context();
  } else if (the_register.is_function_closure()) {
    return builder()->GetFunctionClosure();
  } else {
    int values_index = RegisterToValuesIndex(the_register);
    return values()->at(values_index);
  }
}

void BytecodeGraphBuilder::Environment::BindAccumulator(
    Node* node, FrameStateAttachmentMode mode) {
  if (mode == FrameStateAttachmentMode::kAttachFrameState) {
    builder()->PrepareFrameState(node, OutputFrameStateCombine::PokeAt(0));
  }
  values()->at(accumulator_base_) = node;
}

void BytecodeGraphBuilder::Environment::BindGeneratorState(Node* node) {
  generator_state_ = node;
}

void BytecodeGraphBuilder::Environment::BindRegister(
    interpreter::Register the_register, Node* node,
    FrameStateAttachmentMode mode) {
  int values_index = RegisterToValuesIndex(the_register);
  if (mode == FrameStateAttachmentMode::kAttachFrameState) {
    builder()->PrepareFrameState(node, OutputFrameStateCombine::PokeAt(
                                           accumulator_base_ - values_index));
  }
  values()->at(values_index) = node;
}

void BytecodeGraphBuilder::Environment::BindRegistersToProjections(
    interpreter::Register first_reg, Node* node,
    FrameStateAttachmentMode mode) {
  int values_index = RegisterToValuesIndex(first_reg);
  if (mode == FrameStateAttachmentMode::kAttachFrameState) {
    builder()->PrepareFrameState(node, OutputFrameStateCombine::PokeAt(
                                           accumulator_base_ - values_index));
  }
  for (int i = 0; i < node->op()->ValueOutputCount(); i++) {
    values()->at(values_index + i) =
        builder()->NewNode(common()->Projection(i), node);
  }
}

void BytecodeGraphBuilder::Environment::RecordAfterState(
    Node* node, FrameStateAttachmentMode mode) {
  if (mode == FrameStateAttachmentMode::kAttachFrameState) {
    builder()->PrepareFrameState(node, OutputFrameStateCombine::Ignore());
  }
}

BytecodeGraphBuilder::Environment* BytecodeGraphBuilder::Environment::Copy() {
  return new (zone()) Environment(this);
}

void BytecodeGraphBuilder::Environment::Merge(
    BytecodeGraphBuilder::Environment* other,
    const BytecodeLivenessState* liveness) {
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

  // Introduce Phi nodes for values that are live and have differing inputs at
  // the merge point, potentially extending an existing Phi node if possible.
  context_ = builder()->MergeValue(context_, other->context_, control);
  for (int i = 0; i < parameter_count(); i++) {
    values_[i] = builder()->MergeValue(values_[i], other->values_[i], control);
  }
  for (int i = 0; i < register_count(); i++) {
    int index = register_base() + i;
    if (liveness == nullptr || liveness->RegisterIsLive(i)) {
#if DEBUG
      // We only do these DCHECKs when we are not in the resume path of a
      // generator -- this is, when either there is no generator state at all,
      // or the generator state is not the constant "executing" value.
      if (generator_state_ == nullptr ||
          NumberMatcher(generator_state_)
              .Is(JSGeneratorObject::kGeneratorExecuting)) {
        DCHECK_NE(values_[index], builder()->jsgraph()->OptimizedOutConstant());
        DCHECK_NE(other->values_[index],
                  builder()->jsgraph()->OptimizedOutConstant());
      }
#endif

      values_[index] =
          builder()->MergeValue(values_[index], other->values_[index], control);

    } else {
      values_[index] = builder()->jsgraph()->OptimizedOutConstant();
    }
  }

  if (liveness == nullptr || liveness->AccumulatorIsLive()) {
    DCHECK_NE(values_[accumulator_base()],
              builder()->jsgraph()->OptimizedOutConstant());
    DCHECK_NE(other->values_[accumulator_base()],
              builder()->jsgraph()->OptimizedOutConstant());

    values_[accumulator_base()] =
        builder()->MergeValue(values_[accumulator_base()],
                              other->values_[accumulator_base()], control);
  } else {
    values_[accumulator_base()] = builder()->jsgraph()->OptimizedOutConstant();
  }

  if (generator_state_ != nullptr) {
    DCHECK_NOT_NULL(other->generator_state_);
    generator_state_ = builder()->MergeValue(generator_state_,
                                             other->generator_state_, control);
  }
}

void BytecodeGraphBuilder::Environment::PrepareForLoop(
    const BytecodeLoopAssignments& assignments,
    const BytecodeLivenessState* liveness) {
  // Create a control node for the loop header.
  Node* control = builder()->NewLoop();

  // Create a Phi for external effects.
  Node* effect = builder()->NewEffectPhi(1, GetEffectDependency(), control);
  UpdateEffectDependency(effect);

  // Create Phis for any values that are live on entry to the loop and may be
  // updated by the end of the loop.
  context_ = builder()->NewPhi(1, context_, control);
  for (int i = 0; i < parameter_count(); i++) {
    if (assignments.ContainsParameter(i)) {
      values_[i] = builder()->NewPhi(1, values_[i], control);
    }
  }
  for (int i = 0; i < register_count(); i++) {
    if (assignments.ContainsLocal(i) &&
        (liveness == nullptr || liveness->RegisterIsLive(i))) {
      int index = register_base() + i;
      values_[index] = builder()->NewPhi(1, values_[index], control);
    }
  }
  // The accumulator should not be live on entry.
  DCHECK_IMPLIES(liveness != nullptr, !liveness->AccumulatorIsLive());

  if (generator_state_ != nullptr) {
    generator_state_ = builder()->NewPhi(1, generator_state_, control);
  }

  // Connect to the loop end.
  Node* terminate = builder()->graph()->NewNode(
      builder()->common()->Terminate(), effect, control);
  builder()->exit_controls_.push_back(terminate);
}

void BytecodeGraphBuilder::Environment::FillWithOsrValues() {
  Node* start = graph()->start();

  // Create OSR values for each environment value.
  SetContext(graph()->NewNode(
      common()->OsrValue(Linkage::kOsrContextSpillSlotIndex), start));
  int size = static_cast<int>(values()->size());
  for (int i = 0; i < size; i++) {
    int idx = i;  // Indexing scheme follows {StandardFrame}, adapt accordingly.
    if (i >= register_base()) idx += InterpreterFrameConstants::kExtraSlotCount;
    if (i >= accumulator_base()) idx = Linkage::kOsrAccumulatorRegisterIndex;
    values()->at(i) = graph()->NewNode(common()->OsrValue(idx), start);
  }
}

bool BytecodeGraphBuilder::Environment::StateValuesRequireUpdate(
    Node** state_values, Node** values, int count) {
  if (*state_values == nullptr) {
    return true;
  }
  Node::Inputs inputs = (*state_values)->inputs();
  if (inputs.count() != count) return true;
  for (int i = 0; i < count; i++) {
    if (inputs[i] != values[i]) {
      return true;
    }
  }
  return false;
}

void BytecodeGraphBuilder::Environment::PrepareForLoopExit(
    Node* loop, const BytecodeLoopAssignments& assignments,
    const BytecodeLivenessState* liveness) {
  DCHECK_EQ(loop->opcode(), IrOpcode::kLoop);

  Node* control = GetControlDependency();

  // Create the loop exit node.
  Node* loop_exit = graph()->NewNode(common()->LoopExit(), control, loop);
  UpdateControlDependency(loop_exit);

  // Rename the effect.
  Node* effect_rename = graph()->NewNode(common()->LoopExitEffect(),
                                         GetEffectDependency(), loop_exit);
  UpdateEffectDependency(effect_rename);

  // TODO(jarin) We should also rename context here. However, unconditional
  // renaming confuses global object and native context specialization.
  // We should only rename if the context is assigned in the loop.

  // Rename the environment values if they were assigned in the loop and are
  // live after exiting the loop.
  for (int i = 0; i < parameter_count(); i++) {
    if (assignments.ContainsParameter(i)) {
      Node* rename =
          graph()->NewNode(common()->LoopExitValue(), values_[i], loop_exit);
      values_[i] = rename;
    }
  }
  for (int i = 0; i < register_count(); i++) {
    if (assignments.ContainsLocal(i) &&
        (liveness == nullptr || liveness->RegisterIsLive(i))) {
      Node* rename = graph()->NewNode(common()->LoopExitValue(),
                                      values_[register_base() + i], loop_exit);
      values_[register_base() + i] = rename;
    }
  }
  if (liveness == nullptr || liveness->AccumulatorIsLive()) {
    Node* rename = graph()->NewNode(common()->LoopExitValue(),
                                    values_[accumulator_base()], loop_exit);
    values_[accumulator_base()] = rename;
  }

  if (generator_state_ != nullptr) {
    generator_state_ = graph()->NewNode(common()->LoopExitValue(),
                                        generator_state_, loop_exit);
  }
}

void BytecodeGraphBuilder::Environment::UpdateStateValues(Node** state_values,
                                                          Node** values,
                                                          int count) {
  if (StateValuesRequireUpdate(state_values, values, count)) {
    const Operator* op = common()->StateValues(count, SparseInputMask::Dense());
    (*state_values) = graph()->NewNode(op, count, values);
  }
}

Node* BytecodeGraphBuilder::Environment::GetStateValuesFromCache(
    Node** values, int count, const BitVector* liveness, int liveness_offset) {
  return builder_->state_values_cache_.GetNodeForValues(
      values, static_cast<size_t>(count), liveness, liveness_offset);
}

Node* BytecodeGraphBuilder::Environment::Checkpoint(
    BailoutId bailout_id, OutputFrameStateCombine combine,
    const BytecodeLivenessState* liveness) {
  if (parameter_count() == register_count()) {
    // Re-use the state-value cache if the number of local registers happens
    // to match the parameter count.
    parameters_state_values_ = GetStateValuesFromCache(
        &values()->at(0), parameter_count(), nullptr, 0);
  } else {
    UpdateStateValues(&parameters_state_values_, &values()->at(0),
                      parameter_count());
  }

  Node* registers_state_values =
      GetStateValuesFromCache(&values()->at(register_base()), register_count(),
                              liveness ? &liveness->bit_vector() : nullptr, 0);

  bool accumulator_is_live = !liveness || liveness->AccumulatorIsLive();
  Node* accumulator_state_value =
      accumulator_is_live && combine != OutputFrameStateCombine::PokeAt(0)
          ? values()->at(accumulator_base())
          : builder()->jsgraph()->OptimizedOutConstant();

  const Operator* op = common()->FrameState(
      bailout_id, combine, builder()->frame_state_function_info());
  Node* result = graph()->NewNode(
      op, parameters_state_values_, registers_state_values,
      accumulator_state_value, Context(), builder()->GetFunctionClosure(),
      builder()->graph()->start());

  return result;
}

BytecodeGraphBuilder::BytecodeGraphBuilder(
    Zone* local_zone, Handle<SharedFunctionInfo> shared_info,
    Handle<FeedbackVector> feedback_vector, BailoutId osr_offset,
    JSGraph* jsgraph, CallFrequency invocation_frequency,
    SourcePositionTable* source_positions, Handle<Context> native_context,
    int inlining_id, JSTypeHintLowering::Flags flags, bool stack_check,
    bool analyze_environment_liveness)
    : local_zone_(local_zone),
      jsgraph_(jsgraph),
      invocation_frequency_(invocation_frequency),
      bytecode_array_(
          handle(shared_info->GetBytecodeArray(), jsgraph->isolate())),
      feedback_vector_(feedback_vector),
      type_hint_lowering_(jsgraph, feedback_vector, flags),
      frame_state_function_info_(common()->CreateFrameStateFunctionInfo(
          FrameStateType::kInterpretedFunction,
          bytecode_array()->parameter_count(),
          bytecode_array()->register_count(), shared_info)),
      bytecode_iterator_(nullptr),
      bytecode_analysis_(nullptr),
      environment_(nullptr),
      osr_offset_(osr_offset),
      currently_peeled_loop_offset_(-1),
      stack_check_(stack_check),
      analyze_environment_liveness_(analyze_environment_liveness),
      merge_environments_(local_zone),
      generator_merge_environments_(local_zone),
      exception_handlers_(local_zone),
      current_exception_handler_(0),
      input_buffer_size_(0),
      input_buffer_(nullptr),
      needs_eager_checkpoint_(true),
      exit_controls_(local_zone),
      state_values_cache_(jsgraph),
      source_positions_(source_positions),
      start_position_(shared_info->StartPosition(), inlining_id),
      native_context_(native_context) {}

Node* BytecodeGraphBuilder::GetFunctionClosure() {
  if (!function_closure_.is_set()) {
    int index = Linkage::kJSCallClosureParamIndex;
    const Operator* op = common()->Parameter(index, "%closure");
    Node* node = NewNode(op, graph()->start());
    function_closure_.set(node);
  }
  return function_closure_.get();
}

Node* BytecodeGraphBuilder::BuildLoadNativeContextField(int index) {
  const Operator* op =
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true);
  Node* native_context = NewNode(op);
  Node* result = NewNode(javascript()->LoadContext(0, index, true));
  NodeProperties::ReplaceContextInput(result, native_context);
  return result;
}

VectorSlotPair BytecodeGraphBuilder::CreateVectorSlotPair(int slot_id) {
  return VectorSlotPair(feedback_vector(), FeedbackVector::ToSlot(slot_id));
}

void BytecodeGraphBuilder::CreateGraph() {
  SourcePositionTable::Scope pos_scope(source_positions_, start_position_);

  // Set up the basic structure of the graph. Outputs for {Start} are the formal
  // parameters (including the receiver) plus new target, number of arguments,
  // context and closure.
  int actual_parameter_count = bytecode_array()->parameter_count() + 4;
  graph()->SetStart(graph()->NewNode(common()->Start(actual_parameter_count)));

  Environment env(this, bytecode_array()->register_count(),
                  bytecode_array()->parameter_count(),
                  bytecode_array()->incoming_new_target_or_generator_register(),
                  graph()->start());
  set_environment(&env);

  VisitBytecodes();

  // Finish the basic structure of the graph.
  DCHECK_NE(0u, exit_controls_.size());
  int const input_count = static_cast<int>(exit_controls_.size());
  Node** const inputs = &exit_controls_.front();
  Node* end = graph()->NewNode(common()->End(input_count), input_count, inputs);
  graph()->SetEnd(end);
}

void BytecodeGraphBuilder::PrepareEagerCheckpoint() {
  if (needs_eager_checkpoint()) {
    // Create an explicit checkpoint node for before the operation. This only
    // needs to happen if we aren't effect-dominated by a {Checkpoint} already.
    mark_as_needing_eager_checkpoint(false);
    Node* node = NewNode(common()->Checkpoint());
    DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
    DCHECK_EQ(IrOpcode::kDead,
              NodeProperties::GetFrameStateInput(node)->opcode());
    BailoutId bailout_id(bytecode_iterator().current_offset());

    const BytecodeLivenessState* liveness_before =
        bytecode_analysis()->GetInLivenessFor(
            bytecode_iterator().current_offset());

    Node* frame_state_before = environment()->Checkpoint(
        bailout_id, OutputFrameStateCombine::Ignore(), liveness_before);
    NodeProperties::ReplaceFrameStateInput(node, frame_state_before);
#ifdef DEBUG
  } else {
    // In case we skipped checkpoint creation above, we must be able to find an
    // existing checkpoint that effect-dominates the nodes about to be created.
    // Starting a search from the current effect-dependency has to succeed.
    Node* effect = environment()->GetEffectDependency();
    while (effect->opcode() != IrOpcode::kCheckpoint) {
      DCHECK(effect->op()->HasProperty(Operator::kNoWrite));
      DCHECK_EQ(1, effect->op()->EffectInputCount());
      effect = NodeProperties::GetEffectInput(effect);
    }
  }
#else
  }
#endif  // DEBUG
}

void BytecodeGraphBuilder::PrepareFrameState(Node* node,
                                             OutputFrameStateCombine combine) {
  if (OperatorProperties::HasFrameStateInput(node->op())) {
    // Add the frame state for after the operation. The node in question has
    // already been created and had a {Dead} frame state input up until now.
    DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
    DCHECK_EQ(IrOpcode::kDead,
              NodeProperties::GetFrameStateInput(node)->opcode());
    BailoutId bailout_id(bytecode_iterator().current_offset());

    const BytecodeLivenessState* liveness_after =
        bytecode_analysis()->GetOutLivenessFor(
            bytecode_iterator().current_offset());

    Node* frame_state_after =
        environment()->Checkpoint(bailout_id, combine, liveness_after);
    NodeProperties::ReplaceFrameStateInput(node, frame_state_after);
  }
}

// Stores the state of the SourcePosition iterator, and the index to the
// current exception handlers stack. We need, during the OSR graph generation,
// to backup the states of these iterators at the LoopHeader offset of each
// outer loop which contains the OSR loop. The iterators are then restored when
// peeling the loops, so that both exception handling and synchronisation with
// the source position can be achieved.
class BytecodeGraphBuilder::OsrIteratorState {
 public:
  OsrIteratorState(interpreter::BytecodeArrayIterator* iterator,
                   SourcePositionTableIterator* source_position_iterator,
                   BytecodeGraphBuilder* graph_builder)
      : iterator_(iterator),
        source_position_iterator_(source_position_iterator),
        graph_builder_(graph_builder),
        saved_states_(graph_builder->local_zone()) {}

  void ProcessOsrPrelude() {
    ZoneVector<int> outer_loop_offsets(graph_builder_->local_zone());

    const BytecodeAnalysis& bytecode_analysis =
        *(graph_builder_->bytecode_analysis());
    int osr_offset = bytecode_analysis.osr_entry_point();

    // We find here the outermost loop which contains the OSR loop.
    int outermost_loop_offset = osr_offset;
    while ((outermost_loop_offset =
                bytecode_analysis.GetLoopInfoFor(outermost_loop_offset)
                    .parent_offset()) != -1) {
      outer_loop_offsets.push_back(outermost_loop_offset);
    }
    outermost_loop_offset =
        outer_loop_offsets.empty() ? osr_offset : outer_loop_offsets.back();

    // We will not processs any bytecode before the outermost_loop_offset, but
    // the source_position_iterator needs to be advanced step by step through
    // the bytecode.
    for (; iterator_->current_offset() != outermost_loop_offset;
         iterator_->Advance()) {
      graph_builder_->UpdateSourcePosition(source_position_iterator_,
                                           iterator_->current_offset());
    }

    // We save some iterators states at the offsets of the loop headers of the
    // outer loops (the ones containing the OSR loop). They will be used for
    // jumping back in the bytecode.
    for (ZoneVector<int>::const_reverse_iterator it =
             outer_loop_offsets.crbegin();
         it != outer_loop_offsets.crend(); ++it) {
      int next_loop_offset = *it;
      for (; iterator_->current_offset() != next_loop_offset;
           iterator_->Advance()) {
        graph_builder_->UpdateSourcePosition(source_position_iterator_,
                                             iterator_->current_offset());
      }
      graph_builder_->ExitThenEnterExceptionHandlers(
          iterator_->current_offset());
      saved_states_.push(
          IteratorsStates(graph_builder_->current_exception_handler(),
                          source_position_iterator_->GetState()));
    }

    // Finishing by advancing to the OSR entry
    for (; iterator_->current_offset() != osr_offset; iterator_->Advance()) {
      graph_builder_->UpdateSourcePosition(source_position_iterator_,
                                           iterator_->current_offset());
    }

    // Enters all remaining exception handler which end before the OSR loop
    // so that on next call of VisitSingleBytecode they will get popped from
    // the exception handlers stack.
    graph_builder_->ExitThenEnterExceptionHandlers(osr_offset);
    graph_builder_->set_currently_peeled_loop_offset(
        bytecode_analysis.GetLoopInfoFor(osr_offset).parent_offset());
  }

  void RestoreState(int target_offset, int new_parent_offset) {
    iterator_->SetOffset(target_offset);
    // In case of a return, we must not build loop exits for
    // not-yet-built outer loops.
    graph_builder_->set_currently_peeled_loop_offset(new_parent_offset);
    IteratorsStates saved_state = saved_states_.top();
    source_position_iterator_->RestoreState(saved_state.source_iterator_state_);
    graph_builder_->set_current_exception_handler(
        saved_state.exception_handler_index_);
    saved_states_.pop();
  }

 private:
  struct IteratorsStates {
    int exception_handler_index_;
    SourcePositionTableIterator::IndexAndPosition source_iterator_state_;

    IteratorsStates(
        int exception_handler_index,
        SourcePositionTableIterator::IndexAndPosition source_iterator_state)
        : exception_handler_index_(exception_handler_index),
          source_iterator_state_(source_iterator_state) {}
  };

  interpreter::BytecodeArrayIterator* iterator_;
  SourcePositionTableIterator* source_position_iterator_;
  BytecodeGraphBuilder* graph_builder_;
  ZoneStack<IteratorsStates> saved_states_;
};

void BytecodeGraphBuilder::RemoveMergeEnvironmentsBeforeOffset(
    int limit_offset) {
  if (!merge_environments_.empty()) {
    ZoneMap<int, Environment*>::iterator it = merge_environments_.begin();
    ZoneMap<int, Environment*>::iterator stop_it = merge_environments_.end();
    while (it != stop_it && it->first <= limit_offset) {
      it = merge_environments_.erase(it);
    }
  }
}

// We will iterate through the OSR loop, then its parent, and so on
// until we have reached the outmost loop containing the OSR loop. We do
// not generate nodes for anything before the outermost loop.
void BytecodeGraphBuilder::AdvanceToOsrEntryAndPeelLoops(
    interpreter::BytecodeArrayIterator* iterator,
    SourcePositionTableIterator* source_position_iterator) {
  const BytecodeAnalysis& analysis = *(bytecode_analysis());
  int osr_offset = analysis.osr_entry_point();
  OsrIteratorState iterator_states(iterator, source_position_iterator, this);

  iterator_states.ProcessOsrPrelude();
  DCHECK_EQ(iterator->current_offset(), osr_offset);

  environment()->FillWithOsrValues();

  // Suppose we have n nested loops, loop_0 being the outermost one, and
  // loop_n being the OSR loop. We start iterating the bytecode at the header
  // of loop_n (the OSR loop), and then we peel the part of the the body of
  // loop_{n-1} following the end of loop_n. We then rewind the iterator to
  // the header of loop_{n-1}, and so on until we have partly peeled loop 0.
  // The full loop_0 body will be generating with the rest of the function,
  // outside the OSR generation.

  // To do so, if we are visiting a loop, we continue to visit what's left
  // of its parent, and then when reaching the parent's JumpLoop, we do not
  // create any jump for that but rewind the bytecode iterator to visit the
  // parent loop entirely, and so on.

  int current_parent_offset =
      analysis.GetLoopInfoFor(osr_offset).parent_offset();
  while (current_parent_offset != -1) {
    LoopInfo current_parent_loop =
        analysis.GetLoopInfoFor(current_parent_offset);
    // We iterate until the back edge of the parent loop, which we detect by
    // the offset that the JumpLoop targets.
    for (; !iterator->done(); iterator->Advance()) {
      if (iterator->current_bytecode() == interpreter::Bytecode::kJumpLoop &&
          iterator->GetJumpTargetOffset() == current_parent_offset) {
        // Reached the end of the current parent loop.
        break;
      }
      VisitSingleBytecode(source_position_iterator);
    }
    DCHECK(!iterator->done());  // Should have found the loop's jump target.

    // We also need to take care of the merge environments and exceptions
    // handlers here because the omitted JumpLoop bytecode can still be the
    // target of jumps or the first bytecode after a try block.
    ExitThenEnterExceptionHandlers(iterator->current_offset());
    SwitchToMergeEnvironment(iterator->current_offset());

    // This jump is the jump of our parent loop, which is not yet created.
    // So we do not build the jump nodes, but restore the bytecode and the
    // SourcePosition iterators to the values they had when we were visiting
    // the offset pointed at by the JumpLoop we've just reached.
    // We have already built nodes for inner loops, but now we will
    // iterate again over them and build new nodes corresponding to the same
    // bytecode offsets. Any jump or reference to this inner loops must now
    // point to the new nodes we will build, hence we clear the relevant part
    // of the environment.
    // Completely clearing the environment is not possible because merge
    // environments for forward jumps out of the loop need to be preserved
    // (e.g. a return or a labeled break in the middle of a loop).
    RemoveMergeEnvironmentsBeforeOffset(iterator->current_offset());
    iterator_states.RestoreState(current_parent_offset,
                                 current_parent_loop.parent_offset());
    current_parent_offset = current_parent_loop.parent_offset();
  }
}

void BytecodeGraphBuilder::VisitSingleBytecode(
    SourcePositionTableIterator* source_position_iterator) {
  const interpreter::BytecodeArrayIterator& iterator = bytecode_iterator();
  int current_offset = iterator.current_offset();
  UpdateSourcePosition(source_position_iterator, current_offset);
  ExitThenEnterExceptionHandlers(current_offset);
  DCHECK_GE(exception_handlers_.empty() ? current_offset
                                        : exception_handlers_.top().end_offset_,
            current_offset);
  SwitchToMergeEnvironment(current_offset);

  if (environment() != nullptr) {
    BuildLoopHeaderEnvironment(current_offset);

    // Skip the first stack check if stack_check is false
    if (!stack_check() &&
        iterator.current_bytecode() == interpreter::Bytecode::kStackCheck) {
      set_stack_check(true);
      return;
    }

    switch (iterator.current_bytecode()) {
#define BYTECODE_CASE(name, ...)       \
  case interpreter::Bytecode::k##name: \
    Visit##name();                     \
    break;
      BYTECODE_LIST(BYTECODE_CASE)
#undef BYTECODE_CODE
    }
  }
}

void BytecodeGraphBuilder::VisitBytecodes() {
  BytecodeAnalysis bytecode_analysis(bytecode_array(), local_zone(),
                                     analyze_environment_liveness());
  bytecode_analysis.Analyze(osr_offset_);
  set_bytecode_analysis(&bytecode_analysis);

  interpreter::BytecodeArrayIterator iterator(bytecode_array());
  set_bytecode_iterator(&iterator);
  SourcePositionTableIterator source_position_iterator(
      handle(bytecode_array()->SourcePositionTable(), isolate()));

  if (analyze_environment_liveness() && FLAG_trace_environment_liveness) {
    StdoutStream of;
    bytecode_analysis.PrintLivenessTo(of);
  }

  if (!bytecode_analysis.resume_jump_targets().empty()) {
    environment()->BindGeneratorState(
        jsgraph()->SmiConstant(JSGeneratorObject::kGeneratorExecuting));
  }

  if (bytecode_analysis.HasOsrEntryPoint()) {
    // We peel the OSR loop and any outer loop containing it except that we
    // leave the nodes corresponding to the whole outermost loop (including
    // the last copies of the loops it contains) to be generated by the normal
    // bytecode iteration below.
    AdvanceToOsrEntryAndPeelLoops(&iterator, &source_position_iterator);
  }

  for (; !iterator.done(); iterator.Advance()) {
    VisitSingleBytecode(&source_position_iterator);
  }
  set_bytecode_analysis(nullptr);
  set_bytecode_iterator(nullptr);
  DCHECK(exception_handlers_.empty());
}

void BytecodeGraphBuilder::VisitLdaZero() {
  Node* node = jsgraph()->ZeroConstant();
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaSmi() {
  Node* node = jsgraph()->Constant(bytecode_iterator().GetImmediateOperand(0));
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaConstant() {
  Node* node = jsgraph()->Constant(
      handle(bytecode_iterator().GetConstantForIndexOperand(0), isolate()));
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaUndefined() {
  Node* node = jsgraph()->UndefinedConstant();
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaNull() {
  Node* node = jsgraph()->NullConstant();
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaTheHole() {
  Node* node = jsgraph()->TheHoleConstant();
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaTrue() {
  Node* node = jsgraph()->TrueConstant();
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaFalse() {
  Node* node = jsgraph()->FalseConstant();
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdar() {
  Node* value =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  environment()->BindAccumulator(value);
}

void BytecodeGraphBuilder::VisitStar() {
  Node* value = environment()->LookupAccumulator();
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(0), value);
}

void BytecodeGraphBuilder::VisitMov() {
  Node* value =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(1), value);
}

Node* BytecodeGraphBuilder::BuildLoadGlobal(Handle<Name> name,
                                            uint32_t feedback_slot_index,
                                            TypeofMode typeof_mode) {
  VectorSlotPair feedback = CreateVectorSlotPair(feedback_slot_index);
  DCHECK(IsLoadGlobalICKind(feedback_vector()->GetKind(feedback.slot())));
  const Operator* op = javascript()->LoadGlobal(name, feedback, typeof_mode);
  return NewNode(op);
}

void BytecodeGraphBuilder::VisitLdaGlobal() {
  PrepareEagerCheckpoint();
  Handle<Name> name(
      Name::cast(bytecode_iterator().GetConstantForIndexOperand(0)), isolate());
  uint32_t feedback_slot_index = bytecode_iterator().GetIndexOperand(1);
  Node* node =
      BuildLoadGlobal(name, feedback_slot_index, TypeofMode::NOT_INSIDE_TYPEOF);
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitLdaGlobalInsideTypeof() {
  PrepareEagerCheckpoint();
  Handle<Name> name(
      Name::cast(bytecode_iterator().GetConstantForIndexOperand(0)), isolate());
  uint32_t feedback_slot_index = bytecode_iterator().GetIndexOperand(1);
  Node* node =
      BuildLoadGlobal(name, feedback_slot_index, TypeofMode::INSIDE_TYPEOF);
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitStaGlobal() {
  PrepareEagerCheckpoint();
  Handle<Name> name(
      Name::cast(bytecode_iterator().GetConstantForIndexOperand(0)), isolate());
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(1));
  Node* value = environment()->LookupAccumulator();

  LanguageMode language_mode =
      feedback.vector()->GetLanguageMode(feedback.slot());
  const Operator* op = javascript()->StoreGlobal(language_mode, name, feedback);
  Node* node = NewNode(op, value);
  environment()->RecordAfterState(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitStaInArrayLiteral() {
  PrepareEagerCheckpoint();
  Node* value = environment()->LookupAccumulator();
  Node* array =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* index =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(2));
  const Operator* op = javascript()->StoreInArrayLiteral(feedback);

  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedStoreKeyed(op, array, index, value, feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, array, index, value);
  }

  environment()->RecordAfterState(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitStaDataPropertyInLiteral() {
  PrepareEagerCheckpoint();

  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* name =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  Node* value = environment()->LookupAccumulator();
  int flags = bytecode_iterator().GetFlagOperand(2);
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(3));

  const Operator* op = javascript()->StoreDataPropertyInLiteral(feedback);
  Node* node = NewNode(op, object, name, value, jsgraph()->Constant(flags));
  environment()->RecordAfterState(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitCollectTypeProfile() {
  PrepareEagerCheckpoint();

  Node* position =
      jsgraph()->Constant(bytecode_iterator().GetImmediateOperand(0));
  Node* value = environment()->LookupAccumulator();
  Node* vector = jsgraph()->Constant(feedback_vector());

  const Operator* op = javascript()->CallRuntime(Runtime::kCollectTypeProfile);

  Node* node = NewNode(op, position, value, vector);
  environment()->RecordAfterState(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitLdaContextSlot() {
  const Operator* op = javascript()->LoadContext(
      bytecode_iterator().GetUnsignedImmediateOperand(2),
      bytecode_iterator().GetIndexOperand(1), false);
  Node* node = NewNode(op);
  Node* context =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  NodeProperties::ReplaceContextInput(node, context);
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaImmutableContextSlot() {
  const Operator* op = javascript()->LoadContext(
      bytecode_iterator().GetUnsignedImmediateOperand(2),
      bytecode_iterator().GetIndexOperand(1), true);
  Node* node = NewNode(op);
  Node* context =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  NodeProperties::ReplaceContextInput(node, context);
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaCurrentContextSlot() {
  const Operator* op = javascript()->LoadContext(
      0, bytecode_iterator().GetIndexOperand(0), false);
  Node* node = NewNode(op);
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaImmutableCurrentContextSlot() {
  const Operator* op = javascript()->LoadContext(
      0, bytecode_iterator().GetIndexOperand(0), true);
  Node* node = NewNode(op);
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitStaContextSlot() {
  const Operator* op = javascript()->StoreContext(
      bytecode_iterator().GetUnsignedImmediateOperand(2),
      bytecode_iterator().GetIndexOperand(1));
  Node* value = environment()->LookupAccumulator();
  Node* node = NewNode(op, value);
  Node* context =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  NodeProperties::ReplaceContextInput(node, context);
}

void BytecodeGraphBuilder::VisitStaCurrentContextSlot() {
  const Operator* op =
      javascript()->StoreContext(0, bytecode_iterator().GetIndexOperand(0));
  Node* value = environment()->LookupAccumulator();
  NewNode(op, value);
}

void BytecodeGraphBuilder::BuildLdaLookupSlot(TypeofMode typeof_mode) {
  PrepareEagerCheckpoint();
  Node* name = jsgraph()->Constant(
      handle(bytecode_iterator().GetConstantForIndexOperand(0), isolate()));
  const Operator* op =
      javascript()->CallRuntime(typeof_mode == TypeofMode::NOT_INSIDE_TYPEOF
                                    ? Runtime::kLoadLookupSlot
                                    : Runtime::kLoadLookupSlotInsideTypeof);
  Node* value = NewNode(op, name);
  environment()->BindAccumulator(value, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitLdaLookupSlot() {
  BuildLdaLookupSlot(TypeofMode::NOT_INSIDE_TYPEOF);
}

void BytecodeGraphBuilder::VisitLdaLookupSlotInsideTypeof() {
  BuildLdaLookupSlot(TypeofMode::INSIDE_TYPEOF);
}

BytecodeGraphBuilder::Environment* BytecodeGraphBuilder::CheckContextExtensions(
    uint32_t depth) {
  // Output environment where the context has an extension
  Environment* slow_environment = nullptr;

  // We only need to check up to the last-but-one depth, because the an eval
  // in the same scope as the variable itself has no way of shadowing it.
  for (uint32_t d = 0; d < depth; d++) {
    Node* extension_slot =
        NewNode(javascript()->LoadContext(d, Context::EXTENSION_INDEX, false));

    Node* check_no_extension =
        NewNode(simplified()->ReferenceEqual(), extension_slot,
                jsgraph()->TheHoleConstant());

    NewBranch(check_no_extension);

    {
      SubEnvironment sub_environment(this);

      NewIfFalse();
      // If there is an extension, merge into the slow path.
      if (slow_environment == nullptr) {
        slow_environment = environment();
        NewMerge();
      } else {
        slow_environment->Merge(environment(),
                                bytecode_analysis()->GetInLivenessFor(
                                    bytecode_iterator().current_offset()));
      }
    }

    NewIfTrue();
    // Do nothing on if there is no extension, eventually falling through to
    // the fast path.
  }

  // The depth can be zero, in which case no slow-path checks are built, and
  // the slow path environment can be null.
  DCHECK(depth == 0 || slow_environment != nullptr);

  return slow_environment;
}

void BytecodeGraphBuilder::BuildLdaLookupContextSlot(TypeofMode typeof_mode) {
  uint32_t depth = bytecode_iterator().GetUnsignedImmediateOperand(2);

  // Check if any context in the depth has an extension.
  Environment* slow_environment = CheckContextExtensions(depth);

  // Fast path, do a context load.
  {
    uint32_t slot_index = bytecode_iterator().GetIndexOperand(1);

    const Operator* op = javascript()->LoadContext(depth, slot_index, false);
    environment()->BindAccumulator(NewNode(op));
  }

  // Only build the slow path if there were any slow-path checks.
  if (slow_environment != nullptr) {
    // Add a merge to the fast environment.
    NewMerge();
    Environment* fast_environment = environment();

    // Slow path, do a runtime load lookup.
    set_environment(slow_environment);
    {
      Node* name = jsgraph()->Constant(
          handle(bytecode_iterator().GetConstantForIndexOperand(0), isolate()));

      const Operator* op =
          javascript()->CallRuntime(typeof_mode == TypeofMode::NOT_INSIDE_TYPEOF
                                        ? Runtime::kLoadLookupSlot
                                        : Runtime::kLoadLookupSlotInsideTypeof);
      Node* value = NewNode(op, name);
      environment()->BindAccumulator(value, Environment::kAttachFrameState);
    }

    fast_environment->Merge(environment(),
                            bytecode_analysis()->GetOutLivenessFor(
                                bytecode_iterator().current_offset()));
    set_environment(fast_environment);
    mark_as_needing_eager_checkpoint(true);
  }
}

void BytecodeGraphBuilder::VisitLdaLookupContextSlot() {
  BuildLdaLookupContextSlot(TypeofMode::NOT_INSIDE_TYPEOF);
}

void BytecodeGraphBuilder::VisitLdaLookupContextSlotInsideTypeof() {
  BuildLdaLookupContextSlot(TypeofMode::INSIDE_TYPEOF);
}

void BytecodeGraphBuilder::BuildLdaLookupGlobalSlot(TypeofMode typeof_mode) {
  uint32_t depth = bytecode_iterator().GetUnsignedImmediateOperand(2);

  // Check if any context in the depth has an extension.
  Environment* slow_environment = CheckContextExtensions(depth);

  // Fast path, do a global load.
  {
    PrepareEagerCheckpoint();
    Handle<Name> name(
        Name::cast(bytecode_iterator().GetConstantForIndexOperand(0)),
        isolate());
    uint32_t feedback_slot_index = bytecode_iterator().GetIndexOperand(1);
    Node* node = BuildLoadGlobal(name, feedback_slot_index, typeof_mode);
    environment()->BindAccumulator(node, Environment::kAttachFrameState);
  }

  // Only build the slow path if there were any slow-path checks.
  if (slow_environment != nullptr) {
    // Add a merge to the fast environment.
    NewMerge();
    Environment* fast_environment = environment();

    // Slow path, do a runtime load lookup.
    set_environment(slow_environment);
    {
      Node* name = jsgraph()->Constant(
          handle(bytecode_iterator().GetConstantForIndexOperand(0), isolate()));

      const Operator* op =
          javascript()->CallRuntime(typeof_mode == TypeofMode::NOT_INSIDE_TYPEOF
                                        ? Runtime::kLoadLookupSlot
                                        : Runtime::kLoadLookupSlotInsideTypeof);
      Node* value = NewNode(op, name);
      environment()->BindAccumulator(value, Environment::kAttachFrameState);
    }

    fast_environment->Merge(environment(),
                            bytecode_analysis()->GetOutLivenessFor(
                                bytecode_iterator().current_offset()));
    set_environment(fast_environment);
    mark_as_needing_eager_checkpoint(true);
  }
}

void BytecodeGraphBuilder::VisitLdaLookupGlobalSlot() {
  BuildLdaLookupGlobalSlot(TypeofMode::NOT_INSIDE_TYPEOF);
}

void BytecodeGraphBuilder::VisitLdaLookupGlobalSlotInsideTypeof() {
  BuildLdaLookupGlobalSlot(TypeofMode::INSIDE_TYPEOF);
}

void BytecodeGraphBuilder::VisitStaLookupSlot() {
  PrepareEagerCheckpoint();
  Node* value = environment()->LookupAccumulator();
  Node* name = jsgraph()->Constant(
      handle(bytecode_iterator().GetConstantForIndexOperand(0), isolate()));
  int bytecode_flags = bytecode_iterator().GetFlagOperand(1);
  LanguageMode language_mode = static_cast<LanguageMode>(
      interpreter::StoreLookupSlotFlags::LanguageModeBit::decode(
          bytecode_flags));
  LookupHoistingMode lookup_hoisting_mode = static_cast<LookupHoistingMode>(
      interpreter::StoreLookupSlotFlags::LookupHoistingModeBit::decode(
          bytecode_flags));
  DCHECK_IMPLIES(lookup_hoisting_mode == LookupHoistingMode::kLegacySloppy,
                 is_sloppy(language_mode));
  const Operator* op = javascript()->CallRuntime(
      is_strict(language_mode)
          ? Runtime::kStoreLookupSlot_Strict
          : lookup_hoisting_mode == LookupHoistingMode::kLegacySloppy
                ? Runtime::kStoreLookupSlot_SloppyHoisting
                : Runtime::kStoreLookupSlot_Sloppy);
  Node* store = NewNode(op, name, value);
  environment()->BindAccumulator(store, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitLdaNamedProperty() {
  PrepareEagerCheckpoint();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Handle<Name> name(
      Name::cast(bytecode_iterator().GetConstantForIndexOperand(1)), isolate());
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(2));
  const Operator* op = javascript()->LoadNamed(name, feedback);

  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedLoadNamed(op, object, feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, object);
  }
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitLdaKeyedProperty() {
  PrepareEagerCheckpoint();
  Node* key = environment()->LookupAccumulator();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(1));
  const Operator* op = javascript()->LoadProperty(feedback);

  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedLoadKeyed(op, object, key, feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, object, key);
  }
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::BuildNamedStore(StoreMode store_mode) {
  PrepareEagerCheckpoint();
  Node* value = environment()->LookupAccumulator();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Handle<Name> name(
      Name::cast(bytecode_iterator().GetConstantForIndexOperand(1)), isolate());
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(2));

  const Operator* op;
  if (store_mode == StoreMode::kOwn) {
    DCHECK_EQ(FeedbackSlotKind::kStoreOwnNamed,
              feedback.vector()->GetKind(feedback.slot()));
    op = javascript()->StoreNamedOwn(name, feedback);
  } else {
    DCHECK_EQ(StoreMode::kNormal, store_mode);
    LanguageMode language_mode =
        feedback.vector()->GetLanguageMode(feedback.slot());
    op = javascript()->StoreNamed(language_mode, name, feedback);
  }

  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedStoreNamed(op, object, value, feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, object, value);
  }
  environment()->RecordAfterState(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitStaNamedProperty() {
  BuildNamedStore(StoreMode::kNormal);
}

void BytecodeGraphBuilder::VisitStaNamedOwnProperty() {
  BuildNamedStore(StoreMode::kOwn);
}

void BytecodeGraphBuilder::VisitStaKeyedProperty() {
  PrepareEagerCheckpoint();
  Node* value = environment()->LookupAccumulator();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* key =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(2));
  LanguageMode language_mode =
      feedback.vector()->GetLanguageMode(feedback.slot());
  const Operator* op = javascript()->StoreProperty(language_mode, feedback);

  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedStoreKeyed(op, object, key, value, feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, object, key, value);
  }

  environment()->RecordAfterState(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitLdaModuleVariable() {
  int32_t cell_index = bytecode_iterator().GetImmediateOperand(0);
  uint32_t depth = bytecode_iterator().GetUnsignedImmediateOperand(1);
  Node* module =
      NewNode(javascript()->LoadContext(depth, Context::EXTENSION_INDEX, true));
  Node* value = NewNode(javascript()->LoadModule(cell_index), module);
  environment()->BindAccumulator(value);
}

void BytecodeGraphBuilder::VisitStaModuleVariable() {
  int32_t cell_index = bytecode_iterator().GetImmediateOperand(0);
  uint32_t depth = bytecode_iterator().GetUnsignedImmediateOperand(1);
  Node* module =
      NewNode(javascript()->LoadContext(depth, Context::EXTENSION_INDEX, true));
  Node* value = environment()->LookupAccumulator();
  NewNode(javascript()->StoreModule(cell_index), module, value);
}

void BytecodeGraphBuilder::VisitPushContext() {
  Node* new_context = environment()->LookupAccumulator();
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(0),
                              environment()->Context());
  environment()->SetContext(new_context);
}

void BytecodeGraphBuilder::VisitPopContext() {
  Node* context =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  environment()->SetContext(context);
}

void BytecodeGraphBuilder::VisitCreateClosure() {
  Handle<SharedFunctionInfo> shared_info(
      SharedFunctionInfo::cast(
          bytecode_iterator().GetConstantForIndexOperand(0)),
      isolate());
  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(1);
  FeedbackNexus nexus(feedback_vector(), slot);
  PretenureFlag tenured =
      interpreter::CreateClosureFlags::PretenuredBit::decode(
          bytecode_iterator().GetFlagOperand(2))
          ? TENURED
          : NOT_TENURED;
  const Operator* op = javascript()->CreateClosure(
      shared_info, nexus.GetFeedbackCell(),
      handle(jsgraph()->isolate()->builtins()->builtin(Builtins::kCompileLazy),
             isolate()),
      tenured);
  Node* closure = NewNode(op);
  environment()->BindAccumulator(closure);
}

void BytecodeGraphBuilder::VisitCreateBlockContext() {
  Handle<ScopeInfo> scope_info(
      ScopeInfo::cast(bytecode_iterator().GetConstantForIndexOperand(0)),
      isolate());

  const Operator* op = javascript()->CreateBlockContext(scope_info);
  Node* context = NewNode(op);
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::VisitCreateFunctionContext() {
  Handle<ScopeInfo> scope_info(
      ScopeInfo::cast(bytecode_iterator().GetConstantForIndexOperand(0)),
      isolate());
  uint32_t slots = bytecode_iterator().GetUnsignedImmediateOperand(1);
  const Operator* op =
      javascript()->CreateFunctionContext(scope_info, slots, FUNCTION_SCOPE);
  Node* context = NewNode(op);
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::VisitCreateEvalContext() {
  Handle<ScopeInfo> scope_info(
      ScopeInfo::cast(bytecode_iterator().GetConstantForIndexOperand(0)),
      isolate());
  uint32_t slots = bytecode_iterator().GetUnsignedImmediateOperand(1);
  const Operator* op =
      javascript()->CreateFunctionContext(scope_info, slots, EVAL_SCOPE);
  Node* context = NewNode(op);
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::VisitCreateCatchContext() {
  interpreter::Register reg = bytecode_iterator().GetRegisterOperand(0);
  Node* exception = environment()->LookupRegister(reg);
  Handle<ScopeInfo> scope_info(
      ScopeInfo::cast(bytecode_iterator().GetConstantForIndexOperand(1)),
      isolate());

  const Operator* op = javascript()->CreateCatchContext(scope_info);
  Node* context = NewNode(op, exception);
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::VisitCreateWithContext() {
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Handle<ScopeInfo> scope_info(
      ScopeInfo::cast(bytecode_iterator().GetConstantForIndexOperand(1)),
      isolate());

  const Operator* op = javascript()->CreateWithContext(scope_info);
  Node* context = NewNode(op, object);
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::BuildCreateArguments(CreateArgumentsType type) {
  const Operator* op = javascript()->CreateArguments(type);
  Node* object = NewNode(op, GetFunctionClosure());
  environment()->BindAccumulator(object, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitCreateMappedArguments() {
  BuildCreateArguments(CreateArgumentsType::kMappedArguments);
}

void BytecodeGraphBuilder::VisitCreateUnmappedArguments() {
  BuildCreateArguments(CreateArgumentsType::kUnmappedArguments);
}

void BytecodeGraphBuilder::VisitCreateRestParameter() {
  BuildCreateArguments(CreateArgumentsType::kRestParameter);
}

void BytecodeGraphBuilder::VisitCreateRegExpLiteral() {
  Handle<String> constant_pattern(
      String::cast(bytecode_iterator().GetConstantForIndexOperand(0)),
      isolate());
  int const slot_id = bytecode_iterator().GetIndexOperand(1);
  VectorSlotPair pair = CreateVectorSlotPair(slot_id);
  int literal_flags = bytecode_iterator().GetFlagOperand(2);
  Node* literal = NewNode(
      javascript()->CreateLiteralRegExp(constant_pattern, pair, literal_flags));
  environment()->BindAccumulator(literal, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitCreateArrayLiteral() {
  Handle<ArrayBoilerplateDescription> array_boilerplate_description(
      ArrayBoilerplateDescription::cast(
          bytecode_iterator().GetConstantForIndexOperand(0)),
      isolate());
  int const slot_id = bytecode_iterator().GetIndexOperand(1);
  VectorSlotPair pair = CreateVectorSlotPair(slot_id);
  int bytecode_flags = bytecode_iterator().GetFlagOperand(2);
  int literal_flags =
      interpreter::CreateArrayLiteralFlags::FlagsBits::decode(bytecode_flags);
  // Disable allocation site mementos. Only unoptimized code will collect
  // feedback about allocation site. Once the code is optimized we expect the
  // data to converge. So, we disable allocation site mementos in optimized
  // code. We can revisit this when we have data to the contrary.
  literal_flags |= ArrayLiteral::kDisableMementos;
  // TODO(mstarzinger): Thread through number of elements. The below number is
  // only an estimate and does not match {ArrayLiteral::values::length}.
  int number_of_elements =
      array_boilerplate_description->constant_elements()->length();
  Node* literal = NewNode(javascript()->CreateLiteralArray(
      array_boilerplate_description, pair, literal_flags, number_of_elements));
  environment()->BindAccumulator(literal, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitCreateEmptyArrayLiteral() {
  int const slot_id = bytecode_iterator().GetIndexOperand(0);
  VectorSlotPair pair = CreateVectorSlotPair(slot_id);
  Node* literal = NewNode(javascript()->CreateEmptyLiteralArray(pair));
  environment()->BindAccumulator(literal);
}

void BytecodeGraphBuilder::VisitCreateObjectLiteral() {
  Handle<ObjectBoilerplateDescription> constant_properties(
      ObjectBoilerplateDescription::cast(
          bytecode_iterator().GetConstantForIndexOperand(0)),
      isolate());
  int const slot_id = bytecode_iterator().GetIndexOperand(1);
  VectorSlotPair pair = CreateVectorSlotPair(slot_id);
  int bytecode_flags = bytecode_iterator().GetFlagOperand(2);
  int literal_flags =
      interpreter::CreateObjectLiteralFlags::FlagsBits::decode(bytecode_flags);
  // TODO(mstarzinger): Thread through number of properties. The below number is
  // only an estimate and does not match {ObjectLiteral::properties_count}.
  int number_of_properties = constant_properties->size();
  Node* literal = NewNode(javascript()->CreateLiteralObject(
      constant_properties, pair, literal_flags, number_of_properties));
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(3),
                              literal, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitCreateEmptyObjectLiteral() {
  Node* literal =
      NewNode(javascript()->CreateEmptyLiteralObject(), GetFunctionClosure());
  environment()->BindAccumulator(literal);
}

void BytecodeGraphBuilder::VisitCloneObject() {
  PrepareEagerCheckpoint();
  Node* source =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  int flags = bytecode_iterator().GetFlagOperand(1);
  int slot = bytecode_iterator().GetIndexOperand(2);
  const Operator* op =
      javascript()->CloneObject(CreateVectorSlotPair(slot), flags);
  Node* value = NewNode(op, source);
  environment()->BindAccumulator(value, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitGetTemplateObject() {
  Handle<TemplateObjectDescription> description(
      TemplateObjectDescription::cast(
          bytecode_iterator().GetConstantForIndexOperand(0)),
      isolate());
  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(1);
  FeedbackNexus nexus(feedback_vector(), slot);

  Handle<JSArray> cached_value;
  if (nexus.GetFeedback() == MaybeObject::FromSmi(Smi::kZero)) {
    // It's not observable when the template object is created, so we
    // can just create it eagerly during graph building and bake in
    // the JSArray constant here.
    cached_value =
        TemplateObjectDescription::CreateTemplateObject(isolate(), description);
    nexus.vector()->Set(slot, *cached_value);
  } else {
    cached_value = handle(
        JSArray::cast(nexus.GetFeedback()->ToStrongHeapObject()), isolate());
  }

  Node* template_object = jsgraph()->HeapConstant(cached_value);
  environment()->BindAccumulator(template_object);
}

Node* const* BytecodeGraphBuilder::GetCallArgumentsFromRegisters(
    Node* callee, Node* receiver, interpreter::Register first_arg,
    int arg_count) {
  // The arity of the Call node -- includes the callee, receiver and function
  // arguments.
  int arity = 2 + arg_count;

  Node** all = local_zone()->NewArray<Node*>(static_cast<size_t>(arity));

  all[0] = callee;
  all[1] = receiver;

  // The function arguments are in consecutive registers.
  int arg_base = first_arg.index();
  for (int i = 0; i < arg_count; ++i) {
    all[2 + i] =
        environment()->LookupRegister(interpreter::Register(arg_base + i));
  }

  return all;
}

Node* BytecodeGraphBuilder::ProcessCallArguments(const Operator* call_op,
                                                 Node* const* args,
                                                 int arg_count) {
  return MakeNode(call_op, arg_count, args, false);
}

Node* BytecodeGraphBuilder::ProcessCallArguments(const Operator* call_op,
                                                 Node* callee,
                                                 interpreter::Register receiver,
                                                 size_t reg_count) {
  Node* receiver_node = environment()->LookupRegister(receiver);
  // The receiver is followed by the arguments in the consecutive registers.
  DCHECK_GE(reg_count, 1);
  interpreter::Register first_arg = interpreter::Register(receiver.index() + 1);
  int arg_count = static_cast<int>(reg_count) - 1;

  Node* const* call_args = GetCallArgumentsFromRegisters(callee, receiver_node,
                                                         first_arg, arg_count);
  return ProcessCallArguments(call_op, call_args, 2 + arg_count);
}

void BytecodeGraphBuilder::BuildCall(ConvertReceiverMode receiver_mode,
                                     Node* const* args, size_t arg_count,
                                     int slot_id) {
  DCHECK_EQ(interpreter::Bytecodes::GetReceiverMode(
                bytecode_iterator().current_bytecode()),
            receiver_mode);
  PrepareEagerCheckpoint();

  VectorSlotPair feedback = CreateVectorSlotPair(slot_id);

  CallFrequency frequency = ComputeCallFrequency(slot_id);
  const Operator* op =
      javascript()->Call(arg_count, frequency, feedback, receiver_mode,
                         GetSpeculationMode(slot_id));
  JSTypeHintLowering::LoweringResult lowering = TryBuildSimplifiedCall(
      op, args, static_cast<int>(arg_count), feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = ProcessCallArguments(op, args, static_cast<int>(arg_count));
  }
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

Node* const* BytecodeGraphBuilder::ProcessCallVarArgs(
    ConvertReceiverMode receiver_mode, Node* callee,
    interpreter::Register first_reg, int arg_count) {
  DCHECK_GE(arg_count, 0);
  Node* receiver_node;
  interpreter::Register first_arg;

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    // The receiver is implicit (and undefined), the arguments are in
    // consecutive registers.
    receiver_node = jsgraph()->UndefinedConstant();
    first_arg = first_reg;
  } else {
    // The receiver is the first register, followed by the arguments in the
    // consecutive registers.
    receiver_node = environment()->LookupRegister(first_reg);
    first_arg = interpreter::Register(first_reg.index() + 1);
  }

  Node* const* call_args = GetCallArgumentsFromRegisters(callee, receiver_node,
                                                         first_arg, arg_count);
  return call_args;
}

void BytecodeGraphBuilder::BuildCallVarArgs(ConvertReceiverMode receiver_mode) {
  DCHECK_EQ(interpreter::Bytecodes::GetReceiverMode(
                bytecode_iterator().current_bytecode()),
            receiver_mode);
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  interpreter::Register first_reg = bytecode_iterator().GetRegisterOperand(1);
  size_t reg_count = bytecode_iterator().GetRegisterCountOperand(2);
  int const slot_id = bytecode_iterator().GetIndexOperand(3);

  int arg_count = receiver_mode == ConvertReceiverMode::kNullOrUndefined
                      ? static_cast<int>(reg_count)
                      : static_cast<int>(reg_count) - 1;
  Node* const* call_args =
      ProcessCallVarArgs(receiver_mode, callee, first_reg, arg_count);
  BuildCall(receiver_mode, call_args, static_cast<size_t>(2 + arg_count),
            slot_id);
}

void BytecodeGraphBuilder::VisitCallAnyReceiver() {
  BuildCallVarArgs(ConvertReceiverMode::kAny);
}

void BytecodeGraphBuilder::VisitCallProperty() {
  BuildCallVarArgs(ConvertReceiverMode::kNotNullOrUndefined);
}

void BytecodeGraphBuilder::VisitCallProperty0() {
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* receiver =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  int const slot_id = bytecode_iterator().GetIndexOperand(2);
  BuildCall(ConvertReceiverMode::kNotNullOrUndefined, {callee, receiver},
            slot_id);
}

void BytecodeGraphBuilder::VisitCallProperty1() {
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* receiver =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  Node* arg0 =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(2));
  int const slot_id = bytecode_iterator().GetIndexOperand(3);
  BuildCall(ConvertReceiverMode::kNotNullOrUndefined, {callee, receiver, arg0},
            slot_id);
}

void BytecodeGraphBuilder::VisitCallProperty2() {
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* receiver =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  Node* arg0 =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(2));
  Node* arg1 =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(3));
  int const slot_id = bytecode_iterator().GetIndexOperand(4);
  BuildCall(ConvertReceiverMode::kNotNullOrUndefined,
            {callee, receiver, arg0, arg1}, slot_id);
}

void BytecodeGraphBuilder::VisitCallUndefinedReceiver() {
  BuildCallVarArgs(ConvertReceiverMode::kNullOrUndefined);
}

void BytecodeGraphBuilder::VisitCallUndefinedReceiver0() {
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* receiver = jsgraph()->UndefinedConstant();
  int const slot_id = bytecode_iterator().GetIndexOperand(1);
  BuildCall(ConvertReceiverMode::kNullOrUndefined, {callee, receiver}, slot_id);
}

void BytecodeGraphBuilder::VisitCallUndefinedReceiver1() {
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* receiver = jsgraph()->UndefinedConstant();
  Node* arg0 =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  int const slot_id = bytecode_iterator().GetIndexOperand(2);
  BuildCall(ConvertReceiverMode::kNullOrUndefined, {callee, receiver, arg0},
            slot_id);
}

void BytecodeGraphBuilder::VisitCallUndefinedReceiver2() {
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* receiver = jsgraph()->UndefinedConstant();
  Node* arg0 =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  Node* arg1 =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(2));
  int const slot_id = bytecode_iterator().GetIndexOperand(3);
  BuildCall(ConvertReceiverMode::kNullOrUndefined,
            {callee, receiver, arg0, arg1}, slot_id);
}

void BytecodeGraphBuilder::VisitCallWithSpread() {
  PrepareEagerCheckpoint();
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  interpreter::Register receiver = bytecode_iterator().GetRegisterOperand(1);
  Node* receiver_node = environment()->LookupRegister(receiver);
  size_t reg_count = bytecode_iterator().GetRegisterCountOperand(2);
  interpreter::Register first_arg = interpreter::Register(receiver.index() + 1);
  int arg_count = static_cast<int>(reg_count) - 1;
  Node* const* args = GetCallArgumentsFromRegisters(callee, receiver_node,
                                                    first_arg, arg_count);
  int const slot_id = bytecode_iterator().GetIndexOperand(3);
  VectorSlotPair feedback = CreateVectorSlotPair(slot_id);

  CallFrequency frequency = ComputeCallFrequency(slot_id);
  const Operator* op = javascript()->CallWithSpread(
      static_cast<int>(reg_count + 1), frequency, feedback);

  JSTypeHintLowering::LoweringResult lowering = TryBuildSimplifiedCall(
      op, args, static_cast<int>(arg_count), feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = ProcessCallArguments(op, args, 2 + arg_count);
  }
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitCallJSRuntime() {
  PrepareEagerCheckpoint();
  Node* callee = BuildLoadNativeContextField(
      bytecode_iterator().GetNativeContextIndexOperand(0));
  interpreter::Register first_reg = bytecode_iterator().GetRegisterOperand(1);
  size_t reg_count = bytecode_iterator().GetRegisterCountOperand(2);
  int arg_count = static_cast<int>(reg_count);

  const Operator* call = javascript()->Call(2 + arg_count);
  Node* const* call_args = ProcessCallVarArgs(
      ConvertReceiverMode::kNullOrUndefined, callee, first_reg, arg_count);
  Node* value = ProcessCallArguments(call, call_args, 2 + arg_count);
  environment()->BindAccumulator(value, Environment::kAttachFrameState);
}

Node* BytecodeGraphBuilder::ProcessCallRuntimeArguments(
    const Operator* call_runtime_op, interpreter::Register receiver,
    size_t reg_count) {
  int arg_count = static_cast<int>(reg_count);
  // arity is args.
  int arity = arg_count;
  Node** all = local_zone()->NewArray<Node*>(static_cast<size_t>(arity));
  int first_arg_index = receiver.index();
  for (int i = 0; i < static_cast<int>(reg_count); ++i) {
    all[i] = environment()->LookupRegister(
        interpreter::Register(first_arg_index + i));
  }
  Node* value = MakeNode(call_runtime_op, arity, all, false);
  return value;
}

void BytecodeGraphBuilder::VisitCallRuntime() {
  PrepareEagerCheckpoint();
  Runtime::FunctionId function_id = bytecode_iterator().GetRuntimeIdOperand(0);
  interpreter::Register receiver = bytecode_iterator().GetRegisterOperand(1);
  size_t reg_count = bytecode_iterator().GetRegisterCountOperand(2);

  // Create node to perform the runtime call.
  const Operator* call = javascript()->CallRuntime(function_id, reg_count);
  Node* value = ProcessCallRuntimeArguments(call, receiver, reg_count);
  environment()->BindAccumulator(value, Environment::kAttachFrameState);

  // Connect to the end if {function_id} is non-returning.
  if (Runtime::IsNonReturning(function_id)) {
    // TODO(7099): Investigate if we need LoopExit node here.
    Node* control = NewNode(common()->Throw());
    MergeControlToLeaveFunction(control);
  }
}

void BytecodeGraphBuilder::VisitCallRuntimeForPair() {
  PrepareEagerCheckpoint();
  Runtime::FunctionId functionId = bytecode_iterator().GetRuntimeIdOperand(0);
  interpreter::Register receiver = bytecode_iterator().GetRegisterOperand(1);
  size_t reg_count = bytecode_iterator().GetRegisterCountOperand(2);
  interpreter::Register first_return =
      bytecode_iterator().GetRegisterOperand(3);

  // Create node to perform the runtime call.
  const Operator* call = javascript()->CallRuntime(functionId, reg_count);
  Node* return_pair = ProcessCallRuntimeArguments(call, receiver, reg_count);
  environment()->BindRegistersToProjections(first_return, return_pair,
                                            Environment::kAttachFrameState);
}

Node* const* BytecodeGraphBuilder::GetConstructArgumentsFromRegister(
    Node* target, Node* new_target, interpreter::Register first_arg,
    int arg_count) {
  // arity is args + callee and new target.
  int arity = arg_count + 2;
  Node** all = local_zone()->NewArray<Node*>(static_cast<size_t>(arity));
  all[0] = target;
  int first_arg_index = first_arg.index();
  for (int i = 0; i < arg_count; ++i) {
    all[1 + i] = environment()->LookupRegister(
        interpreter::Register(first_arg_index + i));
  }
  all[arity - 1] = new_target;
  return all;
}

Node* BytecodeGraphBuilder::ProcessConstructArguments(const Operator* op,
                                                      Node* const* args,
                                                      int arg_count) {
  return MakeNode(op, arg_count, args, false);
}

void BytecodeGraphBuilder::VisitConstruct() {
  PrepareEagerCheckpoint();
  interpreter::Register callee_reg = bytecode_iterator().GetRegisterOperand(0);
  interpreter::Register first_reg = bytecode_iterator().GetRegisterOperand(1);
  size_t reg_count = bytecode_iterator().GetRegisterCountOperand(2);
  int const slot_id = bytecode_iterator().GetIndexOperand(3);
  VectorSlotPair feedback = CreateVectorSlotPair(slot_id);

  Node* new_target = environment()->LookupAccumulator();
  Node* callee = environment()->LookupRegister(callee_reg);

  CallFrequency frequency = ComputeCallFrequency(slot_id);
  const Operator* op = javascript()->Construct(
      static_cast<uint32_t>(reg_count + 2), frequency, feedback);
  int arg_count = static_cast<int>(reg_count);
  Node* const* args = GetConstructArgumentsFromRegister(callee, new_target,
                                                        first_reg, arg_count);
  JSTypeHintLowering::LoweringResult lowering = TryBuildSimplifiedConstruct(
      op, args, static_cast<int>(arg_count), feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = ProcessConstructArguments(op, args, 2 + arg_count);
  }
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitConstructWithSpread() {
  PrepareEagerCheckpoint();
  interpreter::Register callee_reg = bytecode_iterator().GetRegisterOperand(0);
  interpreter::Register first_reg = bytecode_iterator().GetRegisterOperand(1);
  size_t reg_count = bytecode_iterator().GetRegisterCountOperand(2);
  int const slot_id = bytecode_iterator().GetIndexOperand(3);
  VectorSlotPair feedback = CreateVectorSlotPair(slot_id);

  Node* new_target = environment()->LookupAccumulator();
  Node* callee = environment()->LookupRegister(callee_reg);

  CallFrequency frequency = ComputeCallFrequency(slot_id);
  const Operator* op = javascript()->ConstructWithSpread(
      static_cast<uint32_t>(reg_count + 2), frequency, feedback);
  int arg_count = static_cast<int>(reg_count);
  Node* const* args = GetConstructArgumentsFromRegister(callee, new_target,
                                                        first_reg, arg_count);
  JSTypeHintLowering::LoweringResult lowering = TryBuildSimplifiedConstruct(
      op, args, static_cast<int>(arg_count), feedback.slot());
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = ProcessConstructArguments(op, args, 2 + arg_count);
  }
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitInvokeIntrinsic() {
  PrepareEagerCheckpoint();
  Runtime::FunctionId functionId = bytecode_iterator().GetIntrinsicIdOperand(0);
  interpreter::Register receiver = bytecode_iterator().GetRegisterOperand(1);
  size_t reg_count = bytecode_iterator().GetRegisterCountOperand(2);

  // Create node to perform the runtime call. Turbofan will take care of the
  // lowering.
  const Operator* call = javascript()->CallRuntime(functionId, reg_count);
  Node* value = ProcessCallRuntimeArguments(call, receiver, reg_count);
  environment()->BindAccumulator(value, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitThrow() {
  BuildLoopExitsForFunctionExit(bytecode_analysis()->GetInLivenessFor(
      bytecode_iterator().current_offset()));
  Node* value = environment()->LookupAccumulator();
  Node* call = NewNode(javascript()->CallRuntime(Runtime::kThrow), value);
  environment()->BindAccumulator(call, Environment::kAttachFrameState);
  Node* control = NewNode(common()->Throw());
  MergeControlToLeaveFunction(control);
}

void BytecodeGraphBuilder::VisitAbort() {
  BuildLoopExitsForFunctionExit(bytecode_analysis()->GetInLivenessFor(
      bytecode_iterator().current_offset()));
  AbortReason reason =
      static_cast<AbortReason>(bytecode_iterator().GetIndexOperand(0));
  NewNode(simplified()->RuntimeAbort(reason));
  Node* control = NewNode(common()->Throw());
  MergeControlToLeaveFunction(control);
}

void BytecodeGraphBuilder::VisitReThrow() {
  BuildLoopExitsForFunctionExit(bytecode_analysis()->GetInLivenessFor(
      bytecode_iterator().current_offset()));
  Node* value = environment()->LookupAccumulator();
  NewNode(javascript()->CallRuntime(Runtime::kReThrow), value);
  Node* control = NewNode(common()->Throw());
  MergeControlToLeaveFunction(control);
}

void BytecodeGraphBuilder::BuildHoleCheckAndThrow(
    Node* condition, Runtime::FunctionId runtime_id, Node* name) {
  Node* accumulator = environment()->LookupAccumulator();
  NewBranch(condition, BranchHint::kFalse);
  {
    SubEnvironment sub_environment(this);

    NewIfTrue();
    BuildLoopExitsForFunctionExit(bytecode_analysis()->GetInLivenessFor(
        bytecode_iterator().current_offset()));
    Node* node;
    const Operator* op = javascript()->CallRuntime(runtime_id);
    if (runtime_id == Runtime::kThrowReferenceError) {
      DCHECK_NOT_NULL(name);
      node = NewNode(op, name);
    } else {
      DCHECK(runtime_id == Runtime::kThrowSuperAlreadyCalledError ||
             runtime_id == Runtime::kThrowSuperNotCalled);
      node = NewNode(op);
    }
    environment()->RecordAfterState(node, Environment::kAttachFrameState);
    Node* control = NewNode(common()->Throw());
    MergeControlToLeaveFunction(control);
  }
  NewIfFalse();
  environment()->BindAccumulator(accumulator);
}

void BytecodeGraphBuilder::VisitThrowReferenceErrorIfHole() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* check_for_hole = NewNode(simplified()->ReferenceEqual(), accumulator,
                                 jsgraph()->TheHoleConstant());
  Node* name = jsgraph()->Constant(
      handle(bytecode_iterator().GetConstantForIndexOperand(0), isolate()));
  BuildHoleCheckAndThrow(check_for_hole, Runtime::kThrowReferenceError, name);
}

void BytecodeGraphBuilder::VisitThrowSuperNotCalledIfHole() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* check_for_hole = NewNode(simplified()->ReferenceEqual(), accumulator,
                                 jsgraph()->TheHoleConstant());
  BuildHoleCheckAndThrow(check_for_hole, Runtime::kThrowSuperNotCalled);
}

void BytecodeGraphBuilder::VisitThrowSuperAlreadyCalledIfNotHole() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* check_for_hole = NewNode(simplified()->ReferenceEqual(), accumulator,
                                 jsgraph()->TheHoleConstant());
  Node* check_for_not_hole =
      NewNode(simplified()->BooleanNot(), check_for_hole);
  BuildHoleCheckAndThrow(check_for_not_hole,
                         Runtime::kThrowSuperAlreadyCalledError);
}

void BytecodeGraphBuilder::BuildUnaryOp(const Operator* op) {
  PrepareEagerCheckpoint();
  Node* operand = environment()->LookupAccumulator();

  FeedbackSlot slot =
      bytecode_iterator().GetSlotOperand(kUnaryOperationHintIndex);
  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedUnaryOp(op, operand, slot);
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, operand);
  }

  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::BuildBinaryOp(const Operator* op) {
  PrepareEagerCheckpoint();
  Node* left =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* right = environment()->LookupAccumulator();

  FeedbackSlot slot =
      bytecode_iterator().GetSlotOperand(kBinaryOperationHintIndex);
  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedBinaryOp(op, left, right, slot);
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, left, right);
  }

  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

// Helper function to create binary operation hint from the recorded type
// feedback.
BinaryOperationHint BytecodeGraphBuilder::GetBinaryOperationHint(
    int operand_index) {
  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(operand_index);
  FeedbackNexus nexus(feedback_vector(), slot);
  return nexus.GetBinaryOperationFeedback();
}

// Helper function to create compare operation hint from the recorded type
// feedback.
CompareOperationHint BytecodeGraphBuilder::GetCompareOperationHint() {
  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(1);
  FeedbackNexus nexus(feedback_vector(), slot);
  return nexus.GetCompareOperationFeedback();
}

// Helper function to create for-in mode from the recorded type feedback.
ForInMode BytecodeGraphBuilder::GetForInMode(int operand_index) {
  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(operand_index);
  FeedbackNexus nexus(feedback_vector(), slot);
  switch (nexus.GetForInFeedback()) {
    case ForInHint::kNone:
    case ForInHint::kEnumCacheKeysAndIndices:
      return ForInMode::kUseEnumCacheKeysAndIndices;
    case ForInHint::kEnumCacheKeys:
      return ForInMode::kUseEnumCacheKeys;
    case ForInHint::kAny:
      return ForInMode::kGeneric;
  }
  UNREACHABLE();
}

CallFrequency BytecodeGraphBuilder::ComputeCallFrequency(int slot_id) const {
  if (invocation_frequency_.IsUnknown()) return CallFrequency();
  FeedbackNexus nexus(feedback_vector(), FeedbackVector::ToSlot(slot_id));
  return CallFrequency(nexus.ComputeCallFrequency() *
                       invocation_frequency_.value());
}

SpeculationMode BytecodeGraphBuilder::GetSpeculationMode(int slot_id) const {
  FeedbackNexus nexus(feedback_vector(), FeedbackVector::ToSlot(slot_id));
  return nexus.GetSpeculationMode();
}

void BytecodeGraphBuilder::VisitBitwiseNot() {
  BuildUnaryOp(javascript()->BitwiseNot());
}

void BytecodeGraphBuilder::VisitDec() {
  BuildUnaryOp(javascript()->Decrement());
}

void BytecodeGraphBuilder::VisitInc() {
  BuildUnaryOp(javascript()->Increment());
}

void BytecodeGraphBuilder::VisitNegate() {
  BuildUnaryOp(javascript()->Negate());
}

void BytecodeGraphBuilder::VisitAdd() {
  BuildBinaryOp(
      javascript()->Add(GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitSub() {
  BuildBinaryOp(javascript()->Subtract());
}

void BytecodeGraphBuilder::VisitMul() {
  BuildBinaryOp(javascript()->Multiply());
}

void BytecodeGraphBuilder::VisitDiv() { BuildBinaryOp(javascript()->Divide()); }

void BytecodeGraphBuilder::VisitMod() {
  BuildBinaryOp(javascript()->Modulus());
}

void BytecodeGraphBuilder::VisitExp() {
  BuildBinaryOp(javascript()->Exponentiate());
}

void BytecodeGraphBuilder::VisitBitwiseOr() {
  BuildBinaryOp(javascript()->BitwiseOr());
}

void BytecodeGraphBuilder::VisitBitwiseXor() {
  BuildBinaryOp(javascript()->BitwiseXor());
}

void BytecodeGraphBuilder::VisitBitwiseAnd() {
  BuildBinaryOp(javascript()->BitwiseAnd());
}

void BytecodeGraphBuilder::VisitShiftLeft() {
  BuildBinaryOp(javascript()->ShiftLeft());
}

void BytecodeGraphBuilder::VisitShiftRight() {
  BuildBinaryOp(javascript()->ShiftRight());
}

void BytecodeGraphBuilder::VisitShiftRightLogical() {
  BuildBinaryOp(javascript()->ShiftRightLogical());
}

void BytecodeGraphBuilder::BuildBinaryOpWithImmediate(const Operator* op) {
  PrepareEagerCheckpoint();
  Node* left = environment()->LookupAccumulator();
  Node* right = jsgraph()->Constant(bytecode_iterator().GetImmediateOperand(0));

  FeedbackSlot slot =
      bytecode_iterator().GetSlotOperand(kBinaryOperationSmiHintIndex);
  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedBinaryOp(op, left, right, slot);
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, left, right);
  }
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitAddSmi() {
  BuildBinaryOpWithImmediate(
      javascript()->Add(GetBinaryOperationHint(kBinaryOperationSmiHintIndex)));
}

void BytecodeGraphBuilder::VisitSubSmi() {
  BuildBinaryOpWithImmediate(javascript()->Subtract());
}

void BytecodeGraphBuilder::VisitMulSmi() {
  BuildBinaryOpWithImmediate(javascript()->Multiply());
}

void BytecodeGraphBuilder::VisitDivSmi() {
  BuildBinaryOpWithImmediate(javascript()->Divide());
}

void BytecodeGraphBuilder::VisitModSmi() {
  BuildBinaryOpWithImmediate(javascript()->Modulus());
}

void BytecodeGraphBuilder::VisitExpSmi() {
  BuildBinaryOpWithImmediate(javascript()->Exponentiate());
}

void BytecodeGraphBuilder::VisitBitwiseOrSmi() {
  BuildBinaryOpWithImmediate(javascript()->BitwiseOr());
}

void BytecodeGraphBuilder::VisitBitwiseXorSmi() {
  BuildBinaryOpWithImmediate(javascript()->BitwiseXor());
}

void BytecodeGraphBuilder::VisitBitwiseAndSmi() {
  BuildBinaryOpWithImmediate(javascript()->BitwiseAnd());
}

void BytecodeGraphBuilder::VisitShiftLeftSmi() {
  BuildBinaryOpWithImmediate(javascript()->ShiftLeft());
}

void BytecodeGraphBuilder::VisitShiftRightSmi() {
  BuildBinaryOpWithImmediate(javascript()->ShiftRight());
}

void BytecodeGraphBuilder::VisitShiftRightLogicalSmi() {
  BuildBinaryOpWithImmediate(javascript()->ShiftRightLogical());
}

void BytecodeGraphBuilder::VisitLogicalNot() {
  Node* value = environment()->LookupAccumulator();
  Node* node = NewNode(simplified()->BooleanNot(), value);
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitToBooleanLogicalNot() {
  Node* value =
      NewNode(simplified()->ToBoolean(), environment()->LookupAccumulator());
  Node* node = NewNode(simplified()->BooleanNot(), value);
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitTypeOf() {
  Node* node =
      NewNode(simplified()->TypeOf(), environment()->LookupAccumulator());
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::BuildDelete(LanguageMode language_mode) {
  PrepareEagerCheckpoint();
  Node* key = environment()->LookupAccumulator();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* mode = jsgraph()->Constant(static_cast<int32_t>(language_mode));
  Node* node = NewNode(javascript()->DeleteProperty(), object, key, mode);
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitDeletePropertyStrict() {
  BuildDelete(LanguageMode::kStrict);
}

void BytecodeGraphBuilder::VisitDeletePropertySloppy() {
  BuildDelete(LanguageMode::kSloppy);
}

void BytecodeGraphBuilder::VisitGetSuperConstructor() {
  Node* node = NewNode(javascript()->GetSuperConstructor(),
                       environment()->LookupAccumulator());
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(0), node,
                              Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::BuildCompareOp(const Operator* op) {
  PrepareEagerCheckpoint();
  Node* left =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* right = environment()->LookupAccumulator();

  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(1);
  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedBinaryOp(op, left, right, slot);
  if (lowering.IsExit()) return;

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(op, left, right);
  }
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitTestEqual() {
  BuildCompareOp(javascript()->Equal(GetCompareOperationHint()));
}

void BytecodeGraphBuilder::VisitTestEqualStrict() {
  BuildCompareOp(javascript()->StrictEqual(GetCompareOperationHint()));
}

void BytecodeGraphBuilder::VisitTestLessThan() {
  BuildCompareOp(javascript()->LessThan(GetCompareOperationHint()));
}

void BytecodeGraphBuilder::VisitTestGreaterThan() {
  BuildCompareOp(javascript()->GreaterThan(GetCompareOperationHint()));
}

void BytecodeGraphBuilder::VisitTestLessThanOrEqual() {
  BuildCompareOp(javascript()->LessThanOrEqual(GetCompareOperationHint()));
}

void BytecodeGraphBuilder::VisitTestGreaterThanOrEqual() {
  BuildCompareOp(javascript()->GreaterThanOrEqual(GetCompareOperationHint()));
}

void BytecodeGraphBuilder::VisitTestReferenceEqual() {
  Node* left =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* right = environment()->LookupAccumulator();
  Node* result = NewNode(simplified()->ReferenceEqual(), left, right);
  environment()->BindAccumulator(result);
}

void BytecodeGraphBuilder::VisitTestIn() {
  PrepareEagerCheckpoint();
  Node* object = environment()->LookupAccumulator();
  Node* key =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* node = NewNode(javascript()->HasProperty(), object, key);
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitTestInstanceOf() {
  int const slot_index = bytecode_iterator().GetIndexOperand(1);
  BuildCompareOp(javascript()->InstanceOf(CreateVectorSlotPair(slot_index)));
}

void BytecodeGraphBuilder::VisitTestUndetectable() {
  Node* object = environment()->LookupAccumulator();
  Node* node = NewNode(jsgraph()->simplified()->ObjectIsUndetectable(), object);
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitTestNull() {
  Node* object = environment()->LookupAccumulator();
  Node* result = NewNode(simplified()->ReferenceEqual(), object,
                         jsgraph()->NullConstant());
  environment()->BindAccumulator(result);
}

void BytecodeGraphBuilder::VisitTestUndefined() {
  Node* object = environment()->LookupAccumulator();
  Node* result = NewNode(simplified()->ReferenceEqual(), object,
                         jsgraph()->UndefinedConstant());
  environment()->BindAccumulator(result);
}

void BytecodeGraphBuilder::VisitTestTypeOf() {
  Node* object = environment()->LookupAccumulator();
  auto literal_flag = interpreter::TestTypeOfFlags::Decode(
      bytecode_iterator().GetFlagOperand(0));
  Node* result;
  switch (literal_flag) {
    case interpreter::TestTypeOfFlags::LiteralFlag::kNumber:
      result = NewNode(simplified()->ObjectIsNumber(), object);
      break;
    case interpreter::TestTypeOfFlags::LiteralFlag::kString:
      result = NewNode(simplified()->ObjectIsString(), object);
      break;
    case interpreter::TestTypeOfFlags::LiteralFlag::kSymbol:
      result = NewNode(simplified()->ObjectIsSymbol(), object);
      break;
    case interpreter::TestTypeOfFlags::LiteralFlag::kBigInt:
      result = NewNode(simplified()->ObjectIsBigInt(), object);
      break;
    case interpreter::TestTypeOfFlags::LiteralFlag::kBoolean:
      result = NewNode(common()->Select(MachineRepresentation::kTagged),
                       NewNode(simplified()->ReferenceEqual(), object,
                               jsgraph()->TrueConstant()),
                       jsgraph()->TrueConstant(),
                       NewNode(simplified()->ReferenceEqual(), object,
                               jsgraph()->FalseConstant()));
      break;
    case interpreter::TestTypeOfFlags::LiteralFlag::kUndefined:
      result = graph()->NewNode(
          common()->Select(MachineRepresentation::kTagged),
          graph()->NewNode(simplified()->ReferenceEqual(), object,
                           jsgraph()->NullConstant()),
          jsgraph()->FalseConstant(),
          graph()->NewNode(simplified()->ObjectIsUndetectable(), object));
      break;
    case interpreter::TestTypeOfFlags::LiteralFlag::kFunction:
      result =
          graph()->NewNode(simplified()->ObjectIsDetectableCallable(), object);
      break;
    case interpreter::TestTypeOfFlags::LiteralFlag::kObject:
      result = graph()->NewNode(
          common()->Select(MachineRepresentation::kTagged),
          graph()->NewNode(simplified()->ObjectIsNonCallable(), object),
          jsgraph()->TrueConstant(),
          graph()->NewNode(simplified()->ReferenceEqual(), object,
                           jsgraph()->NullConstant()));
      break;
    case interpreter::TestTypeOfFlags::LiteralFlag::kOther:
      UNREACHABLE();  // Should never be emitted.
      break;
  }
  environment()->BindAccumulator(result);
}

void BytecodeGraphBuilder::BuildCastOperator(const Operator* js_op) {
  Node* value = NewNode(js_op, environment()->LookupAccumulator());
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(0), value,
                              Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitToName() {
  BuildCastOperator(javascript()->ToName());
}

void BytecodeGraphBuilder::VisitToObject() {
  BuildCastOperator(javascript()->ToObject());
}

void BytecodeGraphBuilder::VisitToString() {
  Node* value =
      NewNode(javascript()->ToString(), environment()->LookupAccumulator());
  environment()->BindAccumulator(value, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitToNumber() {
  PrepareEagerCheckpoint();
  Node* object = environment()->LookupAccumulator();

  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(0);
  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedToNumber(object, slot);

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(javascript()->ToNumber(), object);
  }

  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitToNumeric() {
  PrepareEagerCheckpoint();
  Node* object = environment()->LookupAccumulator();

  // If we have some kind of Number feedback, we do the same lowering as for
  // ToNumber.
  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(0);
  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedToNumber(object, slot);

  Node* node = nullptr;
  if (lowering.IsSideEffectFree()) {
    node = lowering.value();
  } else {
    DCHECK(!lowering.Changed());
    node = NewNode(javascript()->ToNumeric(), object);
  }

  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitJump() { BuildJump(); }

void BytecodeGraphBuilder::VisitJumpConstant() { BuildJump(); }

void BytecodeGraphBuilder::VisitJumpIfTrue() { BuildJumpIfTrue(); }

void BytecodeGraphBuilder::VisitJumpIfTrueConstant() { BuildJumpIfTrue(); }

void BytecodeGraphBuilder::VisitJumpIfFalse() { BuildJumpIfFalse(); }

void BytecodeGraphBuilder::VisitJumpIfFalseConstant() { BuildJumpIfFalse(); }

void BytecodeGraphBuilder::VisitJumpIfToBooleanTrue() {
  BuildJumpIfToBooleanTrue();
}

void BytecodeGraphBuilder::VisitJumpIfToBooleanTrueConstant() {
  BuildJumpIfToBooleanTrue();
}

void BytecodeGraphBuilder::VisitJumpIfToBooleanFalse() {
  BuildJumpIfToBooleanFalse();
}

void BytecodeGraphBuilder::VisitJumpIfToBooleanFalseConstant() {
  BuildJumpIfToBooleanFalse();
}

void BytecodeGraphBuilder::VisitJumpIfJSReceiver() { BuildJumpIfJSReceiver(); }

void BytecodeGraphBuilder::VisitJumpIfJSReceiverConstant() {
  BuildJumpIfJSReceiver();
}

void BytecodeGraphBuilder::VisitJumpIfNull() {
  BuildJumpIfEqual(jsgraph()->NullConstant());
}

void BytecodeGraphBuilder::VisitJumpIfNullConstant() {
  BuildJumpIfEqual(jsgraph()->NullConstant());
}

void BytecodeGraphBuilder::VisitJumpIfNotNull() {
  BuildJumpIfNotEqual(jsgraph()->NullConstant());
}

void BytecodeGraphBuilder::VisitJumpIfNotNullConstant() {
  BuildJumpIfNotEqual(jsgraph()->NullConstant());
}

void BytecodeGraphBuilder::VisitJumpIfUndefined() {
  BuildJumpIfEqual(jsgraph()->UndefinedConstant());
}

void BytecodeGraphBuilder::VisitJumpIfUndefinedConstant() {
  BuildJumpIfEqual(jsgraph()->UndefinedConstant());
}

void BytecodeGraphBuilder::VisitJumpIfNotUndefined() {
  BuildJumpIfNotEqual(jsgraph()->UndefinedConstant());
}

void BytecodeGraphBuilder::VisitJumpIfNotUndefinedConstant() {
  BuildJumpIfNotEqual(jsgraph()->UndefinedConstant());
}

void BytecodeGraphBuilder::VisitJumpLoop() { BuildJump(); }

void BytecodeGraphBuilder::BuildSwitchOnSmi(Node* condition) {
  interpreter::JumpTableTargetOffsets offsets =
      bytecode_iterator().GetJumpTableTargetOffsets();

  NewSwitch(condition, offsets.size() + 1);
  for (const auto& entry : offsets) {
    SubEnvironment sub_environment(this);
    NewIfValue(entry.case_value);
    MergeIntoSuccessorEnvironment(entry.target_offset);
  }
  NewIfDefault();
}

void BytecodeGraphBuilder::VisitSwitchOnSmiNoFeedback() {
  PrepareEagerCheckpoint();

  Node* acc = environment()->LookupAccumulator();
  Node* acc_smi = NewNode(simplified()->CheckSmi(VectorSlotPair()), acc);
  BuildSwitchOnSmi(acc_smi);
}

void BytecodeGraphBuilder::VisitStackCheck() {
  PrepareEagerCheckpoint();
  Node* node = NewNode(javascript()->StackCheck());
  environment()->RecordAfterState(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitSetPendingMessage() {
  Node* previous_message = NewNode(javascript()->LoadMessage());
  NewNode(javascript()->StoreMessage(), environment()->LookupAccumulator());
  environment()->BindAccumulator(previous_message);
}

void BytecodeGraphBuilder::BuildReturn(const BytecodeLivenessState* liveness) {
  BuildLoopExitsForFunctionExit(liveness);
  Node* pop_node = jsgraph()->ZeroConstant();
  Node* control =
      NewNode(common()->Return(), pop_node, environment()->LookupAccumulator());
  MergeControlToLeaveFunction(control);
}

void BytecodeGraphBuilder::VisitReturn() {
  BuildReturn(bytecode_analysis()->GetInLivenessFor(
      bytecode_iterator().current_offset()));
}

void BytecodeGraphBuilder::VisitDebugger() {
  PrepareEagerCheckpoint();
  Node* call = NewNode(javascript()->Debugger());
  environment()->RecordAfterState(call, Environment::kAttachFrameState);
}

// We cannot create a graph from the debugger copy of the bytecode array.
#define DEBUG_BREAK(Name, ...) \
  void BytecodeGraphBuilder::Visit##Name() { UNREACHABLE(); }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK);
#undef DEBUG_BREAK

void BytecodeGraphBuilder::VisitIncBlockCounter() {
  Node* closure = GetFunctionClosure();
  Node* coverage_array_slot =
      jsgraph()->Constant(bytecode_iterator().GetIndexOperand(0));

  const Operator* op = javascript()->CallRuntime(Runtime::kIncBlockCounter);

  NewNode(op, closure, coverage_array_slot);
}

void BytecodeGraphBuilder::VisitForInEnumerate() {
  Node* receiver =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* enumerator = NewNode(javascript()->ForInEnumerate(), receiver);
  environment()->BindAccumulator(enumerator, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitForInPrepare() {
  PrepareEagerCheckpoint();
  Node* enumerator = environment()->LookupAccumulator();

  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(1);
  JSTypeHintLowering::LoweringResult lowering =
      TryBuildSimplifiedForInPrepare(enumerator, slot);
  if (lowering.IsExit()) return;
  DCHECK(!lowering.Changed());
  Node* node = NewNode(javascript()->ForInPrepare(GetForInMode(1)), enumerator);
  environment()->BindRegistersToProjections(
      bytecode_iterator().GetRegisterOperand(0), node);
}

void BytecodeGraphBuilder::VisitForInContinue() {
  PrepareEagerCheckpoint();
  Node* index =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* cache_length =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  Node* exit_cond = NewNode(simplified()->SpeculativeNumberLessThan(
                                NumberOperationHint::kSignedSmall),
                            index, cache_length);
  environment()->BindAccumulator(exit_cond);
}

void BytecodeGraphBuilder::VisitForInNext() {
  PrepareEagerCheckpoint();
  Node* receiver =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* index =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  int catch_reg_pair_index = bytecode_iterator().GetRegisterOperand(2).index();
  Node* cache_type = environment()->LookupRegister(
      interpreter::Register(catch_reg_pair_index));
  Node* cache_array = environment()->LookupRegister(
      interpreter::Register(catch_reg_pair_index + 1));

  // We need to rename the {index} here, as in case of OSR we loose the
  // information that the {index} is always a valid unsigned Smi value.
  index = graph()->NewNode(common()->TypeGuard(Type::UnsignedSmall()), index,
                           environment()->GetEffectDependency(),
                           environment()->GetControlDependency());
  environment()->UpdateEffectDependency(index);

  FeedbackSlot slot = bytecode_iterator().GetSlotOperand(3);
  JSTypeHintLowering::LoweringResult lowering = TryBuildSimplifiedForInNext(
      receiver, cache_array, cache_type, index, slot);
  if (lowering.IsExit()) return;

  DCHECK(!lowering.Changed());
  Node* node = NewNode(javascript()->ForInNext(GetForInMode(3)), receiver,
                       cache_array, cache_type, index);
  environment()->BindAccumulator(node, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitForInStep() {
  PrepareEagerCheckpoint();
  Node* index =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  index = NewNode(simplified()->SpeculativeSafeIntegerAdd(
                      NumberOperationHint::kSignedSmall),
                  index, jsgraph()->OneConstant());
  environment()->BindAccumulator(index, Environment::kAttachFrameState);
}

void BytecodeGraphBuilder::VisitSuspendGenerator() {
  Node* generator = environment()->LookupRegister(
      bytecode_iterator().GetRegisterOperand(0));
  interpreter::Register first_reg = bytecode_iterator().GetRegisterOperand(1);
  // We assume we are storing a range starting from index 0.
  CHECK_EQ(0, first_reg.index());
  int register_count =
      static_cast<int>(bytecode_iterator().GetRegisterCountOperand(2));
  int parameter_count_without_receiver =
      bytecode_array()->parameter_count() - 1;

  Node* suspend_id = jsgraph()->SmiConstant(
      bytecode_iterator().GetUnsignedImmediateOperand(3));

  // The offsets used by the bytecode iterator are relative to a different base
  // than what is used in the interpreter, hence the addition.
  Node* offset =
      jsgraph()->Constant(bytecode_iterator().current_offset() +
                          (BytecodeArray::kHeaderSize - kHeapObjectTag));

  const BytecodeLivenessState* liveness = bytecode_analysis()->GetInLivenessFor(
      bytecode_iterator().current_offset());

  // Maybe overallocate the value list since we don't know how many registers
  // are live.
  // TODO(leszeks): We could get this count from liveness rather than the
  // register list.
  int value_input_count = 3 + parameter_count_without_receiver + register_count;

  Node** value_inputs = local_zone()->NewArray<Node*>(value_input_count);
  value_inputs[0] = generator;
  value_inputs[1] = suspend_id;
  value_inputs[2] = offset;

  int count_written = 0;
  // Store the parameters.
  for (int i = 0; i < parameter_count_without_receiver; i++) {
    value_inputs[3 + count_written++] =
        environment()->LookupRegister(interpreter::Register::FromParameterIndex(
            i, parameter_count_without_receiver));
  }

  // Store the registers.
  for (int i = 0; i < register_count; ++i) {
    if (liveness == nullptr || liveness->RegisterIsLive(i)) {
      int index_in_parameters_and_registers =
          parameter_count_without_receiver + i;
      while (count_written < index_in_parameters_and_registers) {
        value_inputs[3 + count_written++] = jsgraph()->OptimizedOutConstant();
      }
      value_inputs[3 + count_written++] =
          environment()->LookupRegister(interpreter::Register(i));
      DCHECK_EQ(count_written, index_in_parameters_and_registers + 1);
    }
  }

  // Use the actual written count rather than the register count to create the
  // node.
  MakeNode(javascript()->GeneratorStore(count_written), 3 + count_written,
           value_inputs, false);

  // TODO(leszeks): This over-approximates the liveness at exit, only the
  // accumulator should be live by this point.
  BuildReturn(bytecode_analysis()->GetInLivenessFor(
      bytecode_iterator().current_offset()));
}

void BytecodeGraphBuilder::BuildSwitchOnGeneratorState(
    const ZoneVector<ResumeJumpTarget>& resume_jump_targets,
    bool allow_fallthrough_on_executing) {
  Node* generator_state = environment()->LookupGeneratorState();

  int extra_cases = allow_fallthrough_on_executing ? 2 : 1;
  NewSwitch(generator_state,
            static_cast<int>(resume_jump_targets.size() + extra_cases));
  for (const ResumeJumpTarget& target : resume_jump_targets) {
    SubEnvironment sub_environment(this);
    NewIfValue(target.suspend_id());
    if (target.is_leaf()) {
      // Mark that we are resuming executing.
      environment()->BindGeneratorState(
          jsgraph()->SmiConstant(JSGeneratorObject::kGeneratorExecuting));
    }
    // Jump to the target offset, whether it's a loop header or the resume.
    MergeIntoSuccessorEnvironment(target.target_offset());
  }

  {
    SubEnvironment sub_environment(this);
    // We should never hit the default case (assuming generator state cannot be
    // corrupted), so abort if we do.
    // TODO(leszeks): Maybe only check this in debug mode, and otherwise use
    // the default to represent one of the cases above/fallthrough below?
    NewIfDefault();
    NewNode(simplified()->RuntimeAbort(AbortReason::kInvalidJumpTableIndex));
    // TODO(7099): Investigate if we need LoopExit here.
    Node* control = NewNode(common()->Throw());
    MergeControlToLeaveFunction(control);
  }

  if (allow_fallthrough_on_executing) {
    // If we are executing (rather than resuming), and we allow it, just fall
    // through to the actual loop body.
    NewIfValue(JSGeneratorObject::kGeneratorExecuting);
  } else {
    // Otherwise, this environment is dead.
    set_environment(nullptr);
  }
}

void BytecodeGraphBuilder::VisitSwitchOnGeneratorState() {
  Node* generator =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));

  Node* generator_is_undefined =
      NewNode(simplified()->ReferenceEqual(), generator,
              jsgraph()->UndefinedConstant());

  NewBranch(generator_is_undefined);
  {
    SubEnvironment resume_env(this);
    NewIfFalse();

    Node* generator_state =
        NewNode(javascript()->GeneratorRestoreContinuation(), generator);
    environment()->BindGeneratorState(generator_state);

    Node* generator_context =
        NewNode(javascript()->GeneratorRestoreContext(), generator);
    environment()->SetContext(generator_context);

    BuildSwitchOnGeneratorState(bytecode_analysis()->resume_jump_targets(),
                                false);
  }

  // Fallthrough for the first-call case.
  NewIfTrue();
}

void BytecodeGraphBuilder::VisitResumeGenerator() {
  Node* generator =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  interpreter::Register first_reg = bytecode_iterator().GetRegisterOperand(1);
  // We assume we are restoring registers starting fromm index 0.
  CHECK_EQ(0, first_reg.index());

  const BytecodeLivenessState* liveness =
      bytecode_analysis()->GetOutLivenessFor(
          bytecode_iterator().current_offset());

  int parameter_count_without_receiver =
      bytecode_array()->parameter_count() - 1;

  // Mapping between registers and array indices must match that used in
  // InterpreterAssembler::ExportParametersAndRegisterFile.
  for (int i = 0; i < environment()->register_count(); ++i) {
    if (liveness == nullptr || liveness->RegisterIsLive(i)) {
      Node* value = NewNode(javascript()->GeneratorRestoreRegister(
                                parameter_count_without_receiver + i),
                            generator);
      environment()->BindRegister(interpreter::Register(i), value);
    }
  }

  // Update the accumulator with the generator's input_or_debug_pos.
  Node* input_or_debug_pos =
      NewNode(javascript()->GeneratorRestoreInputOrDebugPos(), generator);
  environment()->BindAccumulator(input_or_debug_pos);
}

void BytecodeGraphBuilder::VisitWide() {
  // Consumed by the BytecodeArrayIterator.
  UNREACHABLE();
}

void BytecodeGraphBuilder::VisitExtraWide() {
  // Consumed by the BytecodeArrayIterator.
  UNREACHABLE();
}

void BytecodeGraphBuilder::VisitIllegal() {
  // Not emitted in valid bytecode.
  UNREACHABLE();
}

void BytecodeGraphBuilder::SwitchToMergeEnvironment(int current_offset) {
  auto it = merge_environments_.find(current_offset);
  if (it != merge_environments_.end()) {
    mark_as_needing_eager_checkpoint(true);
    if (environment() != nullptr) {
      it->second->Merge(environment(),
                        bytecode_analysis()->GetInLivenessFor(current_offset));
    }
    set_environment(it->second);
  }
}

void BytecodeGraphBuilder::BuildLoopHeaderEnvironment(int current_offset) {
  if (bytecode_analysis()->IsLoopHeader(current_offset)) {
    mark_as_needing_eager_checkpoint(true);
    const LoopInfo& loop_info =
        bytecode_analysis()->GetLoopInfoFor(current_offset);
    const BytecodeLivenessState* liveness =
        bytecode_analysis()->GetInLivenessFor(current_offset);

    const auto& resume_jump_targets = loop_info.resume_jump_targets();
    bool generate_suspend_switch = !resume_jump_targets.empty();

    // Add loop header.
    environment()->PrepareForLoop(loop_info.assignments(), liveness);

    // Store a copy of the environment so we can connect merged back edge inputs
    // to the loop header.
    merge_environments_[current_offset] = environment()->Copy();

    // If this loop contains resumes, create a new switch just after the loop
    // for those resumes.
    if (generate_suspend_switch) {
      BuildSwitchOnGeneratorState(loop_info.resume_jump_targets(), true);

      // TODO(leszeks): At this point we know we are executing rather than
      // resuming, so we should be able to prune off the phis in the environment
      // related to the resume path.

      // Set the generator state to a known constant.
      environment()->BindGeneratorState(
          jsgraph()->SmiConstant(JSGeneratorObject::kGeneratorExecuting));
    }
  }
}

void BytecodeGraphBuilder::MergeIntoSuccessorEnvironment(int target_offset) {
  BuildLoopExitsForBranch(target_offset);
  Environment*& merge_environment = merge_environments_[target_offset];

  if (merge_environment == nullptr) {
    // Append merge nodes to the environment. We may merge here with another
    // environment. So add a place holder for merge nodes. We may add redundant
    // but will be eliminated in a later pass.
    // TODO(mstarzinger): Be smarter about this!
    NewMerge();
    merge_environment = environment();
  } else {
    // Merge any values which are live coming into the successor.
    merge_environment->Merge(
        environment(), bytecode_analysis()->GetInLivenessFor(target_offset));
  }
  set_environment(nullptr);
}

void BytecodeGraphBuilder::MergeControlToLeaveFunction(Node* exit) {
  exit_controls_.push_back(exit);
  set_environment(nullptr);
}

void BytecodeGraphBuilder::BuildLoopExitsForBranch(int target_offset) {
  int origin_offset = bytecode_iterator().current_offset();
  // Only build loop exits for forward edges.
  if (target_offset > origin_offset) {
    BuildLoopExitsUntilLoop(
        bytecode_analysis()->GetLoopOffsetFor(target_offset),
        bytecode_analysis()->GetInLivenessFor(target_offset));
  }
}

void BytecodeGraphBuilder::BuildLoopExitsUntilLoop(
    int loop_offset, const BytecodeLivenessState* liveness) {
  int origin_offset = bytecode_iterator().current_offset();
  int current_loop = bytecode_analysis()->GetLoopOffsetFor(origin_offset);
  // The limit_offset is the stop offset for building loop exists, used for OSR.
  // It prevents the creations of loopexits for loops which do not exist.
  loop_offset = std::max(loop_offset, currently_peeled_loop_offset_);

  while (loop_offset < current_loop) {
    Node* loop_node = merge_environments_[current_loop]->GetControlDependency();
    const LoopInfo& loop_info =
        bytecode_analysis()->GetLoopInfoFor(current_loop);
    environment()->PrepareForLoopExit(loop_node, loop_info.assignments(),
                                      liveness);
    current_loop = loop_info.parent_offset();
  }
}

void BytecodeGraphBuilder::BuildLoopExitsForFunctionExit(
    const BytecodeLivenessState* liveness) {
  BuildLoopExitsUntilLoop(-1, liveness);
}

void BytecodeGraphBuilder::BuildJump() {
  MergeIntoSuccessorEnvironment(bytecode_iterator().GetJumpTargetOffset());
}

void BytecodeGraphBuilder::BuildJumpIf(Node* condition) {
  NewBranch(condition, BranchHint::kNone, IsSafetyCheck::kNoSafetyCheck);
  {
    SubEnvironment sub_environment(this);
    NewIfTrue();
    MergeIntoSuccessorEnvironment(bytecode_iterator().GetJumpTargetOffset());
  }
  NewIfFalse();
}

void BytecodeGraphBuilder::BuildJumpIfNot(Node* condition) {
  NewBranch(condition, BranchHint::kNone, IsSafetyCheck::kNoSafetyCheck);
  {
    SubEnvironment sub_environment(this);
    NewIfFalse();
    MergeIntoSuccessorEnvironment(bytecode_iterator().GetJumpTargetOffset());
  }
  NewIfTrue();
}

void BytecodeGraphBuilder::BuildJumpIfEqual(Node* comperand) {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition =
      NewNode(simplified()->ReferenceEqual(), accumulator, comperand);
  BuildJumpIf(condition);
}

void BytecodeGraphBuilder::BuildJumpIfNotEqual(Node* comperand) {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition =
      NewNode(simplified()->ReferenceEqual(), accumulator, comperand);
  BuildJumpIfNot(condition);
}

void BytecodeGraphBuilder::BuildJumpIfFalse() {
  NewBranch(environment()->LookupAccumulator(), BranchHint::kNone,
            IsSafetyCheck::kNoSafetyCheck);
  {
    SubEnvironment sub_environment(this);
    NewIfFalse();
    environment()->BindAccumulator(jsgraph()->FalseConstant());
    MergeIntoSuccessorEnvironment(bytecode_iterator().GetJumpTargetOffset());
  }
  NewIfTrue();
  environment()->BindAccumulator(jsgraph()->TrueConstant());
}

void BytecodeGraphBuilder::BuildJumpIfTrue() {
  NewBranch(environment()->LookupAccumulator(), BranchHint::kNone,
            IsSafetyCheck::kNoSafetyCheck);
  {
    SubEnvironment sub_environment(this);
    NewIfTrue();
    environment()->BindAccumulator(jsgraph()->TrueConstant());
    MergeIntoSuccessorEnvironment(bytecode_iterator().GetJumpTargetOffset());
  }
  NewIfFalse();
  environment()->BindAccumulator(jsgraph()->FalseConstant());
}

void BytecodeGraphBuilder::BuildJumpIfToBooleanTrue() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition = NewNode(simplified()->ToBoolean(), accumulator);
  BuildJumpIf(condition);
}

void BytecodeGraphBuilder::BuildJumpIfToBooleanFalse() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition = NewNode(simplified()->ToBoolean(), accumulator);
  BuildJumpIfNot(condition);
}

void BytecodeGraphBuilder::BuildJumpIfNotHole() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition = NewNode(simplified()->ReferenceEqual(), accumulator,
                            jsgraph()->TheHoleConstant());
  BuildJumpIfNot(condition);
}

void BytecodeGraphBuilder::BuildJumpIfJSReceiver() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition = NewNode(simplified()->ObjectIsReceiver(), accumulator);
  BuildJumpIf(condition);
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedUnaryOp(const Operator* op,
                                                Node* operand,
                                                FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceUnaryOperation(op, operand, effect, control,
                                                slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedBinaryOp(const Operator* op, Node* left,
                                                 Node* right,
                                                 FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceBinaryOperation(op, left, right, effect,
                                                 control, slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedForInNext(Node* receiver,
                                                  Node* cache_array,
                                                  Node* cache_type, Node* index,
                                                  FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceForInNextOperation(
          receiver, cache_array, cache_type, index, effect, control, slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedForInPrepare(Node* enumerator,
                                                     FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceForInPrepareOperation(enumerator, effect,
                                                       control, slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedToNumber(Node* value,
                                                 FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceToNumberOperation(value, effect, control,
                                                   slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult BytecodeGraphBuilder::TryBuildSimplifiedCall(
    const Operator* op, Node* const* args, int arg_count, FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceCallOperation(op, args, arg_count, effect,
                                               control, slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedConstruct(const Operator* op,
                                                  Node* const* args,
                                                  int arg_count,
                                                  FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceConstructOperation(op, args, arg_count, effect,
                                                    control, slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedLoadNamed(const Operator* op,
                                                  Node* receiver,
                                                  FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult early_reduction =
      type_hint_lowering().ReduceLoadNamedOperation(op, receiver, effect,
                                                    control, slot);
  ApplyEarlyReduction(early_reduction);
  return early_reduction;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedLoadKeyed(const Operator* op,
                                                  Node* receiver, Node* key,
                                                  FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceLoadKeyedOperation(op, receiver, key, effect,
                                                    control, slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedStoreNamed(const Operator* op,
                                                   Node* receiver, Node* value,
                                                   FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceStoreNamedOperation(op, receiver, value,
                                                     effect, control, slot);
  ApplyEarlyReduction(result);
  return result;
}

JSTypeHintLowering::LoweringResult
BytecodeGraphBuilder::TryBuildSimplifiedStoreKeyed(const Operator* op,
                                                   Node* receiver, Node* key,
                                                   Node* value,
                                                   FeedbackSlot slot) {
  Node* effect = environment()->GetEffectDependency();
  Node* control = environment()->GetControlDependency();
  JSTypeHintLowering::LoweringResult result =
      type_hint_lowering().ReduceStoreKeyedOperation(op, receiver, key, value,
                                                     effect, control, slot);
  ApplyEarlyReduction(result);
  return result;
}

void BytecodeGraphBuilder::ApplyEarlyReduction(
    JSTypeHintLowering::LoweringResult reduction) {
  if (reduction.IsExit()) {
    MergeControlToLeaveFunction(reduction.control());
  } else if (reduction.IsSideEffectFree()) {
    environment()->UpdateEffectDependency(reduction.effect());
    environment()->UpdateControlDependency(reduction.control());
  } else {
    DCHECK(!reduction.Changed());
    // At the moment, we assume side-effect free reduction. To support
    // side-effects, we would have to invalidate the eager checkpoint,
    // so that deoptimization does not repeat the side effect.
  }
}

Node** BytecodeGraphBuilder::EnsureInputBufferSize(int size) {
  if (size > input_buffer_size_) {
    size = size + kInputBufferSizeIncrement + input_buffer_size_;
    input_buffer_ = local_zone()->NewArray<Node*>(size);
    input_buffer_size_ = size;
  }
  return input_buffer_;
}

void BytecodeGraphBuilder::ExitThenEnterExceptionHandlers(int current_offset) {
  HandlerTable table(*bytecode_array());

  // Potentially exit exception handlers.
  while (!exception_handlers_.empty()) {
    int current_end = exception_handlers_.top().end_offset_;
    if (current_offset < current_end) break;  // Still covered by range.
    exception_handlers_.pop();
  }

  // Potentially enter exception handlers.
  int num_entries = table.NumberOfRangeEntries();
  while (current_exception_handler_ < num_entries) {
    int next_start = table.GetRangeStart(current_exception_handler_);
    if (current_offset < next_start) break;  // Not yet covered by range.
    int next_end = table.GetRangeEnd(current_exception_handler_);
    int next_handler = table.GetRangeHandler(current_exception_handler_);
    int context_register = table.GetRangeData(current_exception_handler_);
    exception_handlers_.push(
        {next_start, next_end, next_handler, context_register});
    current_exception_handler_++;
  }
}

Node* BytecodeGraphBuilder::MakeNode(const Operator* op, int value_input_count,
                                     Node* const* value_inputs,
                                     bool incomplete) {
  DCHECK_EQ(op->ValueInputCount(), value_input_count);

  bool has_context = OperatorProperties::HasContextInput(op);
  bool has_frame_state = OperatorProperties::HasFrameStateInput(op);
  bool has_control = op->ControlInputCount() == 1;
  bool has_effect = op->EffectInputCount() == 1;

  DCHECK_LT(op->ControlInputCount(), 2);
  DCHECK_LT(op->EffectInputCount(), 2);

  Node* result = nullptr;
  if (!has_context && !has_frame_state && !has_control && !has_effect) {
    result = graph()->NewNode(op, value_input_count, value_inputs, incomplete);
  } else {
    bool inside_handler = !exception_handlers_.empty();
    int input_count_with_deps = value_input_count;
    if (has_context) ++input_count_with_deps;
    if (has_frame_state) ++input_count_with_deps;
    if (has_control) ++input_count_with_deps;
    if (has_effect) ++input_count_with_deps;
    Node** buffer = EnsureInputBufferSize(input_count_with_deps);
    memcpy(buffer, value_inputs, kPointerSize * value_input_count);
    Node** current_input = buffer + value_input_count;
    if (has_context) {
      *current_input++ = environment()->Context();
    }
    if (has_frame_state) {
      // The frame state will be inserted later. Here we misuse the {Dead} node
      // as a sentinel to be later overwritten with the real frame state by the
      // calls to {PrepareFrameState} within individual visitor methods.
      *current_input++ = jsgraph()->Dead();
    }
    if (has_effect) {
      *current_input++ = environment()->GetEffectDependency();
    }
    if (has_control) {
      *current_input++ = environment()->GetControlDependency();
    }
    result = graph()->NewNode(op, input_count_with_deps, buffer, incomplete);
    // Update the current control dependency for control-producing nodes.
    if (result->op()->ControlOutputCount() > 0) {
      environment()->UpdateControlDependency(result);
    }
    // Update the current effect dependency for effect-producing nodes.
    if (result->op()->EffectOutputCount() > 0) {
      environment()->UpdateEffectDependency(result);
    }
    // Add implicit exception continuation for throwing nodes.
    if (!result->op()->HasProperty(Operator::kNoThrow) && inside_handler) {
      int handler_offset = exception_handlers_.top().handler_offset_;
      int context_index = exception_handlers_.top().context_register_;
      interpreter::Register context_register(context_index);
      Environment* success_env = environment()->Copy();
      const Operator* op = common()->IfException();
      Node* effect = environment()->GetEffectDependency();
      Node* on_exception = graph()->NewNode(op, effect, result);
      Node* context = environment()->LookupRegister(context_register);
      environment()->UpdateControlDependency(on_exception);
      environment()->UpdateEffectDependency(on_exception);
      environment()->BindAccumulator(on_exception);
      environment()->SetContext(context);
      MergeIntoSuccessorEnvironment(handler_offset);
      set_environment(success_env);
    }
    // Add implicit success continuation for throwing nodes.
    if (!result->op()->HasProperty(Operator::kNoThrow) && inside_handler) {
      const Operator* if_success = common()->IfSuccess();
      Node* on_success = graph()->NewNode(if_success, result);
      environment()->UpdateControlDependency(on_success);
    }
    // Ensure checkpoints are created after operations with side-effects.
    if (has_effect && !result->op()->HasProperty(Operator::kNoWrite)) {
      mark_as_needing_eager_checkpoint(true);
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

void BytecodeGraphBuilder::UpdateSourcePosition(SourcePositionTableIterator* it,
                                                int offset) {
  if (it->done()) return;
  if (it->code_offset() == offset) {
    source_positions_->SetCurrentPosition(SourcePosition(
        it->source_position().ScriptOffset(), start_position_.InliningId()));
    it->Advance();
  } else {
    DCHECK_GT(it->code_offset(), offset);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

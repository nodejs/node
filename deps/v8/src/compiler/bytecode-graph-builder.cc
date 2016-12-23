// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/bytecode-graph-builder.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/compilation-info.h"
#include "src/compiler/bytecode-branch-analysis.h"
#include "src/compiler/linkage.h"
#include "src/compiler/operator-properties.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// The abstract execution environment simulates the content of the interpreter
// register file. The environment performs SSA-renaming of all tracked nodes at
// split and merge points in the control flow.
class BytecodeGraphBuilder::Environment : public ZoneObject {
 public:
  Environment(BytecodeGraphBuilder* builder, int register_count,
              int parameter_count, Node* control_dependency, Node* context);

  int parameter_count() const { return parameter_count_; }
  int register_count() const { return register_count_; }

  Node* LookupAccumulator() const;
  Node* LookupRegister(interpreter::Register the_register) const;
  void MarkAllRegistersLive();

  void BindAccumulator(Node* node, FrameStateBeforeAndAfter* states = nullptr);
  void BindRegister(interpreter::Register the_register, Node* node,
                    FrameStateBeforeAndAfter* states = nullptr);
  void BindRegistersToProjections(interpreter::Register first_reg, Node* node,
                                  FrameStateBeforeAndAfter* states = nullptr);
  void RecordAfterState(Node* node, FrameStateBeforeAndAfter* states);

  // Effect dependency tracked by this environment.
  Node* GetEffectDependency() { return effect_dependency_; }
  void UpdateEffectDependency(Node* dependency) {
    effect_dependency_ = dependency;
  }

  // Preserve a checkpoint of the environment for the IR graph. Any
  // further mutation of the environment will not affect checkpoints.
  Node* Checkpoint(BailoutId bytecode_offset, OutputFrameStateCombine combine,
                   bool owner_has_exception);

  // Returns true if the state values are up to date with the current
  // environment.
  bool StateValuesAreUpToDate(int output_poke_offset, int output_poke_count);

  // Control dependency tracked by this environment.
  Node* GetControlDependency() const { return control_dependency_; }
  void UpdateControlDependency(Node* dependency) {
    control_dependency_ = dependency;
  }

  Node* Context() const { return context_; }
  void SetContext(Node* new_context) { context_ = new_context; }

  Environment* CopyForConditional();
  Environment* CopyForLoop();
  Environment* CopyForOsrEntry();
  void Merge(Environment* other);
  void PrepareForOsrEntry();

  void PrepareForLoopExit(Node* loop);

 private:
  Environment(const Environment* copy, LivenessAnalyzerBlock* liveness_block);
  void PrepareForLoop();

  enum { kNotCached, kCached };

  bool StateValuesAreUpToDate(Node** state_values, int offset, int count,
                              int output_poke_start, int output_poke_end,
                              int cached = kNotCached);
  bool StateValuesRequireUpdate(Node** state_values, int offset, int count);
  void UpdateStateValues(Node** state_values, int offset, int count);
  void UpdateStateValuesWithCache(Node** state_values, int offset, int count);

  int RegisterToValuesIndex(interpreter::Register the_register) const;

  bool IsLivenessBlockConsistent() const;

  Zone* zone() const { return builder_->local_zone(); }
  Graph* graph() const { return builder_->graph(); }
  CommonOperatorBuilder* common() const { return builder_->common(); }
  BytecodeGraphBuilder* builder() const { return builder_; }
  LivenessAnalyzerBlock* liveness_block() const { return liveness_block_; }
  const NodeVector* values() const { return &values_; }
  NodeVector* values() { return &values_; }
  int register_base() const { return register_base_; }
  int accumulator_base() const { return accumulator_base_; }

  BytecodeGraphBuilder* builder_;
  int register_count_;
  int parameter_count_;
  LivenessAnalyzerBlock* liveness_block_;
  Node* context_;
  Node* control_dependency_;
  Node* effect_dependency_;
  NodeVector values_;
  Node* parameters_state_values_;
  Node* registers_state_values_;
  Node* accumulator_state_values_;
  int register_base_;
  int accumulator_base_;
};

// Helper for generating frame states for before and after a bytecode.
class BytecodeGraphBuilder::FrameStateBeforeAndAfter {
 public:
  explicit FrameStateBeforeAndAfter(BytecodeGraphBuilder* builder)
      : builder_(builder),
        id_after_(BailoutId::None()),
        added_to_node_(false),
        frame_states_unused_(false),
        output_poke_offset_(0),
        output_poke_count_(0) {
    BailoutId id_before(builder->bytecode_iterator().current_offset());
    frame_state_before_ = builder_->environment()->Checkpoint(
        id_before, OutputFrameStateCombine::Ignore(), false);
    id_after_ = BailoutId(id_before.ToInt() +
                          builder->bytecode_iterator().current_bytecode_size());
    // Create an explicit checkpoint node for before the operation.
    Node* node = builder_->NewNode(builder_->common()->Checkpoint());
    DCHECK_EQ(IrOpcode::kDead,
              NodeProperties::GetFrameStateInput(node)->opcode());
    NodeProperties::ReplaceFrameStateInput(node, frame_state_before_);
  }

  ~FrameStateBeforeAndAfter() {
    DCHECK(added_to_node_);
    DCHECK(frame_states_unused_ ||
           builder_->environment()->StateValuesAreUpToDate(output_poke_offset_,
                                                           output_poke_count_));
  }

 private:
  friend class Environment;

  void AddToNode(Node* node, OutputFrameStateCombine combine) {
    DCHECK(!added_to_node_);
    bool has_frame_state = OperatorProperties::HasFrameStateInput(node->op());
    if (has_frame_state) {
      // Add the frame state for after the operation.
      DCHECK_EQ(IrOpcode::kDead,
                NodeProperties::GetFrameStateInput(node)->opcode());
      bool has_exception = NodeProperties::IsExceptionalCall(node);
      Node* frame_state_after = builder_->environment()->Checkpoint(
          id_after_, combine, has_exception);
      NodeProperties::ReplaceFrameStateInput(node, frame_state_after);
    }

    if (!combine.IsOutputIgnored()) {
      output_poke_offset_ = static_cast<int>(combine.GetOffsetToPokeAt());
      output_poke_count_ = node->op()->ValueOutputCount();
    }
    frame_states_unused_ = !has_frame_state;
    added_to_node_ = true;
  }

  BytecodeGraphBuilder* builder_;
  Node* frame_state_before_;
  BailoutId id_after_;

  bool added_to_node_;
  bool frame_states_unused_;
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
      liveness_block_(builder->is_liveness_analysis_enabled_
                          ? builder_->liveness_analyzer()->NewBlock()
                          : nullptr),
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
    const BytecodeGraphBuilder::Environment* other,
    LivenessAnalyzerBlock* liveness_block)
    : builder_(other->builder_),
      register_count_(other->register_count_),
      parameter_count_(other->parameter_count_),
      liveness_block_(liveness_block),
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

bool BytecodeGraphBuilder::Environment::IsLivenessBlockConsistent() const {
  return !builder_->IsLivenessAnalysisEnabled() ==
         (liveness_block() == nullptr);
}

Node* BytecodeGraphBuilder::Environment::LookupAccumulator() const {
  return values()->at(accumulator_base_);
}


Node* BytecodeGraphBuilder::Environment::LookupRegister(
    interpreter::Register the_register) const {
  if (the_register.is_current_context()) {
    return Context();
  } else if (the_register.is_function_closure()) {
    return builder()->GetFunctionClosure();
  } else if (the_register.is_new_target()) {
    return builder()->GetNewTarget();
  } else {
    int values_index = RegisterToValuesIndex(the_register);
    if (liveness_block() != nullptr && !the_register.is_parameter()) {
      DCHECK(IsLivenessBlockConsistent());
      liveness_block()->Lookup(the_register.index());
    }
    return values()->at(values_index);
  }
}

void BytecodeGraphBuilder::Environment::MarkAllRegistersLive() {
  DCHECK(IsLivenessBlockConsistent());
  if (liveness_block() != nullptr) {
    for (int i = 0; i < register_count(); ++i) {
      liveness_block()->Lookup(i);
    }
  }
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
  if (liveness_block() != nullptr && !the_register.is_parameter()) {
    DCHECK(IsLivenessBlockConsistent());
    liveness_block()->Bind(the_register.index());
  }
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


BytecodeGraphBuilder::Environment*
BytecodeGraphBuilder::Environment::CopyForLoop() {
  PrepareForLoop();
  if (liveness_block() != nullptr) {
    // Finish the current block before copying.
    liveness_block_ = builder_->liveness_analyzer()->NewBlock(liveness_block());
  }
  return new (zone()) Environment(this, liveness_block());
}

BytecodeGraphBuilder::Environment*
BytecodeGraphBuilder::Environment::CopyForOsrEntry() {
  return new (zone())
      Environment(this, builder_->liveness_analyzer()->NewBlock());
}

BytecodeGraphBuilder::Environment*
BytecodeGraphBuilder::Environment::CopyForConditional() {
  LivenessAnalyzerBlock* copy_liveness_block = nullptr;
  if (liveness_block() != nullptr) {
    copy_liveness_block =
        builder_->liveness_analyzer()->NewBlock(liveness_block());
    liveness_block_ = builder_->liveness_analyzer()->NewBlock(liveness_block());
  }
  return new (zone()) Environment(this, copy_liveness_block);
}


void BytecodeGraphBuilder::Environment::Merge(
    BytecodeGraphBuilder::Environment* other) {
  if (builder_->is_liveness_analysis_enabled_) {
    if (GetControlDependency()->opcode() != IrOpcode::kLoop) {
      liveness_block_ =
          builder()->liveness_analyzer()->NewBlock(liveness_block());
    }
    liveness_block()->AddPredecessor(other->liveness_block());
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

void BytecodeGraphBuilder::Environment::PrepareForOsrEntry() {
  DCHECK_EQ(IrOpcode::kLoop, GetControlDependency()->opcode());
  DCHECK_EQ(1, GetControlDependency()->InputCount());

  Node* start = graph()->start();

  // Create a control node for the OSR entry point and update the current
  // environment's dependencies accordingly.
  Node* entry = graph()->NewNode(common()->OsrLoopEntry(), start, start);
  UpdateControlDependency(entry);
  UpdateEffectDependency(entry);

  // Create OSR values for each environment value.
  SetContext(graph()->NewNode(
      common()->OsrValue(Linkage::kOsrContextSpillSlotIndex), entry));
  int size = static_cast<int>(values()->size());
  for (int i = 0; i < size; i++) {
    int idx = i;  // Indexing scheme follows {StandardFrame}, adapt accordingly.
    if (i >= register_base()) idx += InterpreterFrameConstants::kExtraSlotCount;
    if (i >= accumulator_base()) idx = Linkage::kOsrAccumulatorRegisterIndex;
    values()->at(i) = graph()->NewNode(common()->OsrValue(idx), entry);
  }
}

bool BytecodeGraphBuilder::Environment::StateValuesRequireUpdate(
    Node** state_values, int offset, int count) {
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

void BytecodeGraphBuilder::Environment::PrepareForLoopExit(Node* loop) {
  DCHECK_EQ(loop->opcode(), IrOpcode::kLoop);

  Node* control = GetControlDependency();

  // Create the loop exit node.
  Node* loop_exit = graph()->NewNode(common()->LoopExit(), control, loop);
  UpdateControlDependency(loop_exit);

  // Rename the effect.
  Node* effect_rename = graph()->NewNode(common()->LoopExitEffect(),
                                         GetEffectDependency(), loop_exit);
  UpdateEffectDependency(effect_rename);

  // TODO(jarin) We should also rename context here. However, uncoditional
  // renaming confuses global object and native context specialization.
  // We should only rename if the context is assigned in the loop.

  // Rename the environmnent values.
  for (size_t i = 0; i < values_.size(); i++) {
    Node* rename =
        graph()->NewNode(common()->LoopExitValue(), values_[i], loop_exit);
    values_[i] = rename;
  }
}

void BytecodeGraphBuilder::Environment::UpdateStateValues(Node** state_values,
                                                          int offset,
                                                          int count) {
  if (StateValuesRequireUpdate(state_values, offset, count)) {
    const Operator* op = common()->StateValues(count);
    (*state_values) = graph()->NewNode(op, count, &values()->at(offset));
  }
}

void BytecodeGraphBuilder::Environment::UpdateStateValuesWithCache(
    Node** state_values, int offset, int count) {
  Node** env_values = (count == 0) ? nullptr : &values()->at(offset);
  *state_values = builder_->state_values_cache_.GetNodeForValues(
      env_values, static_cast<size_t>(count));
}

Node* BytecodeGraphBuilder::Environment::Checkpoint(
    BailoutId bailout_id, OutputFrameStateCombine combine,
    bool owner_has_exception) {
  UpdateStateValues(&parameters_state_values_, 0, parameter_count());
  UpdateStateValuesWithCache(&registers_state_values_, register_base(),
                             register_count());
  UpdateStateValues(&accumulator_state_values_, accumulator_base(), 1);

  const Operator* op = common()->FrameState(
      bailout_id, combine, builder()->frame_state_function_info());
  Node* result = graph()->NewNode(
      op, parameters_state_values_, registers_state_values_,
      accumulator_state_values_, Context(), builder()->GetFunctionClosure(),
      builder()->graph()->start());

  if (liveness_block() != nullptr) {
    // If the owning node has an exception, register the checkpoint to the
    // predecessor so that the checkpoint is used for both the normal and the
    // exceptional paths. Yes, this is a terrible hack and we might want
    // to use an explicit frame state for the exceptional path.
    if (owner_has_exception) {
      liveness_block()->GetPredecessor()->Checkpoint(result);
    } else {
      liveness_block()->Checkpoint(result);
    }
  }

  return result;
}

bool BytecodeGraphBuilder::Environment::StateValuesAreUpToDate(
    Node** state_values, int offset, int count, int output_poke_start,
    int output_poke_end, int cached) {
  DCHECK_LE(static_cast<size_t>(offset + count), values()->size());
  if (cached == kNotCached) {
    for (int i = 0; i < count; i++, offset++) {
      if (offset < output_poke_start || offset >= output_poke_end) {
        if ((*state_values)->InputAt(i) != values()->at(offset)) {
          return false;
        }
      }
    }
  } else {
    for (StateValuesAccess::TypedNode state_value :
         StateValuesAccess(*state_values)) {
      if (offset < output_poke_start || offset >= output_poke_end) {
        if (state_value.node != values()->at(offset)) {
          return false;
        }
      }
      ++offset;
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
                                output_poke_end, kCached) &&
         StateValuesAreUpToDate(&accumulator_state_values_, accumulator_base(),
                                1, output_poke_start, output_poke_end);
}

BytecodeGraphBuilder::BytecodeGraphBuilder(Zone* local_zone,
                                           CompilationInfo* info,
                                           JSGraph* jsgraph,
                                           float invocation_frequency)
    : local_zone_(local_zone),
      jsgraph_(jsgraph),
      invocation_frequency_(invocation_frequency),
      bytecode_array_(handle(info->shared_info()->bytecode_array())),
      exception_handler_table_(
          handle(HandlerTable::cast(bytecode_array()->handler_table()))),
      feedback_vector_(handle(info->closure()->feedback_vector())),
      frame_state_function_info_(common()->CreateFrameStateFunctionInfo(
          FrameStateType::kInterpretedFunction,
          bytecode_array()->parameter_count(),
          bytecode_array()->register_count(), info->shared_info())),
      osr_ast_id_(info->osr_ast_id()),
      merge_environments_(local_zone),
      exception_handlers_(local_zone),
      current_exception_handler_(0),
      input_buffer_size_(0),
      input_buffer_(nullptr),
      exit_controls_(local_zone),
      is_liveness_analysis_enabled_(FLAG_analyze_environment_liveness &&
                                    info->is_deoptimization_enabled()),
      state_values_cache_(jsgraph),
      liveness_analyzer_(
          static_cast<size_t>(bytecode_array()->register_count()), local_zone) {
}

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


Node* BytecodeGraphBuilder::BuildLoadNativeContextField(int index) {
  const Operator* op =
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true);
  Node* native_context = NewNode(op, environment()->Context());
  return NewNode(javascript()->LoadContext(0, index, true), native_context);
}


VectorSlotPair BytecodeGraphBuilder::CreateVectorSlotPair(int slot_id) {
  FeedbackVectorSlot slot;
  if (slot_id >= TypeFeedbackVector::kReservedIndexCount) {
    slot = feedback_vector()->ToSlot(slot_id);
  }
  return VectorSlotPair(feedback_vector(), slot);
}

bool BytecodeGraphBuilder::CreateGraph() {
  // Set up the basic structure of the graph. Outputs for {Start} are the formal
  // parameters (including the receiver) plus new target, number of arguments,
  // context and closure.
  int actual_parameter_count = bytecode_array()->parameter_count() + 4;
  graph()->SetStart(graph()->NewNode(common()->Start(actual_parameter_count)));

  Environment env(this, bytecode_array()->register_count(),
                  bytecode_array()->parameter_count(), graph()->start(),
                  GetFunctionContext());
  set_environment(&env);

  VisitBytecodes();

  // Finish the basic structure of the graph.
  DCHECK_NE(0u, exit_controls_.size());
  int const input_count = static_cast<int>(exit_controls_.size());
  Node** const inputs = &exit_controls_.front();
  Node* end = graph()->NewNode(common()->End(input_count), input_count, inputs);
  graph()->SetEnd(end);

  ClearNonLiveSlotsInFrameStates();

  return true;
}

void BytecodeGraphBuilder::ClearNonLiveSlotsInFrameStates() {
  if (!IsLivenessAnalysisEnabled()) {
    return;
  }
  NonLiveFrameStateSlotReplacer replacer(
      &state_values_cache_, jsgraph()->OptimizedOutConstant(),
      liveness_analyzer()->local_count(), local_zone());
  liveness_analyzer()->Run(&replacer);
  if (FLAG_trace_environment_liveness) {
    OFStream os(stdout);
    liveness_analyzer()->Print(os);
  }
}

void BytecodeGraphBuilder::VisitBytecodes() {
  BytecodeBranchAnalysis analysis(bytecode_array(), local_zone());
  BytecodeLoopAnalysis loop_analysis(bytecode_array(), &analysis, local_zone());
  analysis.Analyze();
  loop_analysis.Analyze();
  set_branch_analysis(&analysis);
  set_loop_analysis(&loop_analysis);
  interpreter::BytecodeArrayIterator iterator(bytecode_array());
  set_bytecode_iterator(&iterator);
  BuildOSRNormalEntryPoint();
  while (!iterator.done()) {
    int current_offset = iterator.current_offset();
    EnterAndExitExceptionHandlers(current_offset);
    SwitchToMergeEnvironment(current_offset);
    if (environment() != nullptr) {
      BuildLoopHeaderEnvironment(current_offset);
      BuildOSRLoopEntryPoint(current_offset);

      switch (iterator.current_bytecode()) {
#define BYTECODE_CASE(name, ...)       \
  case interpreter::Bytecode::k##name: \
    Visit##name();                     \
    break;
        BYTECODE_LIST(BYTECODE_CASE)
#undef BYTECODE_CODE
      }
    }
    iterator.Advance();
  }
  set_branch_analysis(nullptr);
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
  Node* node =
      jsgraph()->Constant(bytecode_iterator().GetConstantForIndexOperand(0));
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdaUndefined() {
  Node* node = jsgraph()->UndefinedConstant();
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdrUndefined() {
  Node* node = jsgraph()->UndefinedConstant();
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(0), node);
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

Node* BytecodeGraphBuilder::BuildLoadGlobal(uint32_t feedback_slot_index,
                                            TypeofMode typeof_mode) {
  VectorSlotPair feedback = CreateVectorSlotPair(feedback_slot_index);
  DCHECK_EQ(FeedbackVectorSlotKind::LOAD_GLOBAL_IC,
            feedback_vector()->GetKind(feedback.slot()));
  Handle<Name> name(feedback_vector()->GetName(feedback.slot()));
  const Operator* op = javascript()->LoadGlobal(name, feedback, typeof_mode);
  return NewNode(op, GetFunctionClosure());
}

void BytecodeGraphBuilder::VisitLdaGlobal() {
  FrameStateBeforeAndAfter states(this);
  Node* node = BuildLoadGlobal(bytecode_iterator().GetIndexOperand(0),
                               TypeofMode::NOT_INSIDE_TYPEOF);
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::VisitLdrGlobal() {
  FrameStateBeforeAndAfter states(this);
  Node* node = BuildLoadGlobal(bytecode_iterator().GetIndexOperand(0),
                               TypeofMode::NOT_INSIDE_TYPEOF);
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(1), node,
                              &states);
}

void BytecodeGraphBuilder::VisitLdaGlobalInsideTypeof() {
  FrameStateBeforeAndAfter states(this);
  Node* node = BuildLoadGlobal(bytecode_iterator().GetIndexOperand(0),
                               TypeofMode::INSIDE_TYPEOF);
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::BuildStoreGlobal(LanguageMode language_mode) {
  FrameStateBeforeAndAfter states(this);
  Handle<Name> name =
      Handle<Name>::cast(bytecode_iterator().GetConstantForIndexOperand(0));
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(1));
  Node* value = environment()->LookupAccumulator();

  const Operator* op = javascript()->StoreGlobal(language_mode, name, feedback);
  Node* node = NewNode(op, value, GetFunctionClosure());
  environment()->RecordAfterState(node, &states);
}

void BytecodeGraphBuilder::VisitStaGlobalSloppy() {
  BuildStoreGlobal(LanguageMode::SLOPPY);
}

void BytecodeGraphBuilder::VisitStaGlobalStrict() {
  BuildStoreGlobal(LanguageMode::STRICT);
}

Node* BytecodeGraphBuilder::BuildLoadContextSlot() {
  // TODO(mythria): immutable flag is also set to false. This information is not
  // available in bytecode array. update this code when the implementation
  // changes.
  const Operator* op = javascript()->LoadContext(
      bytecode_iterator().GetUnsignedImmediateOperand(2),
      bytecode_iterator().GetIndexOperand(1), false);
  Node* context =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  return NewNode(op, context);
}

void BytecodeGraphBuilder::VisitLdaContextSlot() {
  Node* node = BuildLoadContextSlot();
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitLdrContextSlot() {
  Node* node = BuildLoadContextSlot();
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(3), node);
}

void BytecodeGraphBuilder::VisitStaContextSlot() {
  const Operator* op = javascript()->StoreContext(
      bytecode_iterator().GetUnsignedImmediateOperand(2),
      bytecode_iterator().GetIndexOperand(1));
  Node* context =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* value = environment()->LookupAccumulator();
  NewNode(op, context, value);
}

void BytecodeGraphBuilder::BuildLdaLookupSlot(TypeofMode typeof_mode) {
  FrameStateBeforeAndAfter states(this);
  Node* name =
      jsgraph()->Constant(bytecode_iterator().GetConstantForIndexOperand(0));
  const Operator* op =
      javascript()->CallRuntime(typeof_mode == TypeofMode::NOT_INSIDE_TYPEOF
                                    ? Runtime::kLoadLookupSlot
                                    : Runtime::kLoadLookupSlotInsideTypeof);
  Node* value = NewNode(op, name);
  environment()->BindAccumulator(value, &states);
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

  // We only need to check up to the last-but-one depth, because the an eval in
  // the same scope as the variable itself has no way of shadowing it.
  for (uint32_t d = 0; d < depth; d++) {
    Node* extension_slot =
        NewNode(javascript()->LoadContext(d, Context::EXTENSION_INDEX, false),
                environment()->Context());

    Node* check_no_extension =
        NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                extension_slot, jsgraph()->TheHoleConstant());

    NewBranch(check_no_extension);
    Environment* true_environment = environment()->CopyForConditional();

    {
      NewIfFalse();
      // If there is an extension, merge into the slow path.
      if (slow_environment == nullptr) {
        slow_environment = environment();
        NewMerge();
      } else {
        slow_environment->Merge(environment());
      }
    }

    {
      set_environment(true_environment);
      NewIfTrue();
      // Do nothing on if there is no extension, eventually falling through to
      // the fast path.
    }
  }

  // The depth can be zero, in which case no slow-path checks are built, and the
  // slow path environment can be null.
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
    Node* context = environment()->Context();
    environment()->BindAccumulator(NewNode(op, context));
  }

  // Only build the slow path if there were any slow-path checks.
  if (slow_environment != nullptr) {
    // Add a merge to the fast environment.
    NewMerge();
    Environment* fast_environment = environment();

    // Slow path, do a runtime load lookup.
    set_environment(slow_environment);
    {
      FrameStateBeforeAndAfter states(this);

      Node* name = jsgraph()->Constant(
          bytecode_iterator().GetConstantForIndexOperand(0));

      const Operator* op =
          javascript()->CallRuntime(typeof_mode == TypeofMode::NOT_INSIDE_TYPEOF
                                        ? Runtime::kLoadLookupSlot
                                        : Runtime::kLoadLookupSlotInsideTypeof);
      Node* value = NewNode(op, name);
      environment()->BindAccumulator(value, &states);
    }

    fast_environment->Merge(environment());
    set_environment(fast_environment);
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
    FrameStateBeforeAndAfter states(this);
    Node* node =
        BuildLoadGlobal(bytecode_iterator().GetIndexOperand(1), typeof_mode);
    environment()->BindAccumulator(node, &states);
  }

  // Only build the slow path if there were any slow-path checks.
  if (slow_environment != nullptr) {
    // Add a merge to the fast environment.
    NewMerge();
    Environment* fast_environment = environment();

    // Slow path, do a runtime load lookup.
    set_environment(slow_environment);
    {
      FrameStateBeforeAndAfter states(this);

      Node* name = jsgraph()->Constant(
          bytecode_iterator().GetConstantForIndexOperand(0));

      const Operator* op =
          javascript()->CallRuntime(typeof_mode == TypeofMode::NOT_INSIDE_TYPEOF
                                        ? Runtime::kLoadLookupSlot
                                        : Runtime::kLoadLookupSlotInsideTypeof);
      Node* value = NewNode(op, name);
      environment()->BindAccumulator(value, &states);
    }

    fast_environment->Merge(environment());
    set_environment(fast_environment);
  }
}

void BytecodeGraphBuilder::VisitLdaLookupGlobalSlot() {
  BuildLdaLookupGlobalSlot(TypeofMode::NOT_INSIDE_TYPEOF);
}

void BytecodeGraphBuilder::VisitLdaLookupGlobalSlotInsideTypeof() {
  BuildLdaLookupGlobalSlot(TypeofMode::INSIDE_TYPEOF);
}

void BytecodeGraphBuilder::BuildStaLookupSlot(LanguageMode language_mode) {
  FrameStateBeforeAndAfter states(this);
  Node* value = environment()->LookupAccumulator();
  Node* name =
      jsgraph()->Constant(bytecode_iterator().GetConstantForIndexOperand(0));
  const Operator* op = javascript()->CallRuntime(
      is_strict(language_mode) ? Runtime::kStoreLookupSlot_Strict
                               : Runtime::kStoreLookupSlot_Sloppy);
  Node* store = NewNode(op, name, value);
  environment()->BindAccumulator(store, &states);
}

void BytecodeGraphBuilder::VisitStaLookupSlotSloppy() {
  BuildStaLookupSlot(LanguageMode::SLOPPY);
}

void BytecodeGraphBuilder::VisitStaLookupSlotStrict() {
  BuildStaLookupSlot(LanguageMode::STRICT);
}

Node* BytecodeGraphBuilder::BuildNamedLoad() {
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Handle<Name> name =
      Handle<Name>::cast(bytecode_iterator().GetConstantForIndexOperand(1));
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(2));

  const Operator* op = javascript()->LoadNamed(name, feedback);
  return NewNode(op, object, GetFunctionClosure());
}

void BytecodeGraphBuilder::VisitLdaNamedProperty() {
  FrameStateBeforeAndAfter states(this);
  Node* node = BuildNamedLoad();
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::VisitLdrNamedProperty() {
  FrameStateBeforeAndAfter states(this);
  Node* node = BuildNamedLoad();
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(3), node,
                              &states);
}

Node* BytecodeGraphBuilder::BuildKeyedLoad() {
  Node* key = environment()->LookupAccumulator();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(1));

  const Operator* op = javascript()->LoadProperty(feedback);
  return NewNode(op, object, key, GetFunctionClosure());
}

void BytecodeGraphBuilder::VisitLdaKeyedProperty() {
  FrameStateBeforeAndAfter states(this);
  Node* node = BuildKeyedLoad();
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::VisitLdrKeyedProperty() {
  FrameStateBeforeAndAfter states(this);
  Node* node = BuildKeyedLoad();
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(2), node,
                              &states);
}

void BytecodeGraphBuilder::BuildNamedStore(LanguageMode language_mode) {
  FrameStateBeforeAndAfter states(this);
  Node* value = environment()->LookupAccumulator();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Handle<Name> name =
      Handle<Name>::cast(bytecode_iterator().GetConstantForIndexOperand(1));
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(2));

  const Operator* op = javascript()->StoreNamed(language_mode, name, feedback);
  Node* node = NewNode(op, object, value, GetFunctionClosure());
  environment()->RecordAfterState(node, &states);
}

void BytecodeGraphBuilder::VisitStaNamedPropertySloppy() {
  BuildNamedStore(LanguageMode::SLOPPY);
}

void BytecodeGraphBuilder::VisitStaNamedPropertyStrict() {
  BuildNamedStore(LanguageMode::STRICT);
}

void BytecodeGraphBuilder::BuildKeyedStore(LanguageMode language_mode) {
  FrameStateBeforeAndAfter states(this);
  Node* value = environment()->LookupAccumulator();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* key =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  VectorSlotPair feedback =
      CreateVectorSlotPair(bytecode_iterator().GetIndexOperand(2));

  const Operator* op = javascript()->StoreProperty(language_mode, feedback);
  Node* node = NewNode(op, object, key, value, GetFunctionClosure());
  environment()->RecordAfterState(node, &states);
}

void BytecodeGraphBuilder::VisitStaKeyedPropertySloppy() {
  BuildKeyedStore(LanguageMode::SLOPPY);
}

void BytecodeGraphBuilder::VisitStaKeyedPropertyStrict() {
  BuildKeyedStore(LanguageMode::STRICT);
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
  Handle<SharedFunctionInfo> shared_info = Handle<SharedFunctionInfo>::cast(
      bytecode_iterator().GetConstantForIndexOperand(0));
  PretenureFlag tenured =
      interpreter::CreateClosureFlags::PretenuredBit::decode(
          bytecode_iterator().GetFlagOperand(1))
          ? TENURED
          : NOT_TENURED;
  const Operator* op = javascript()->CreateClosure(shared_info, tenured);
  Node* closure = NewNode(op);
  environment()->BindAccumulator(closure);
}

void BytecodeGraphBuilder::VisitCreateBlockContext() {
  Handle<ScopeInfo> scope_info = Handle<ScopeInfo>::cast(
      bytecode_iterator().GetConstantForIndexOperand(0));

  const Operator* op = javascript()->CreateBlockContext(scope_info);
  Node* context = NewNode(op, environment()->LookupAccumulator());
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::VisitCreateFunctionContext() {
  uint32_t slots = bytecode_iterator().GetUnsignedImmediateOperand(0);
  const Operator* op = javascript()->CreateFunctionContext(slots);
  Node* context = NewNode(op, GetFunctionClosure());
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::VisitCreateCatchContext() {
  interpreter::Register reg = bytecode_iterator().GetRegisterOperand(0);
  Node* exception = environment()->LookupRegister(reg);
  Handle<String> name =
      Handle<String>::cast(bytecode_iterator().GetConstantForIndexOperand(1));
  Handle<ScopeInfo> scope_info = Handle<ScopeInfo>::cast(
      bytecode_iterator().GetConstantForIndexOperand(2));
  Node* closure = environment()->LookupAccumulator();

  const Operator* op = javascript()->CreateCatchContext(name, scope_info);
  Node* context = NewNode(op, exception, closure);
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::VisitCreateWithContext() {
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Handle<ScopeInfo> scope_info = Handle<ScopeInfo>::cast(
      bytecode_iterator().GetConstantForIndexOperand(1));

  const Operator* op = javascript()->CreateWithContext(scope_info);
  Node* context = NewNode(op, object, environment()->LookupAccumulator());
  environment()->BindAccumulator(context);
}

void BytecodeGraphBuilder::BuildCreateArguments(CreateArgumentsType type) {
  FrameStateBeforeAndAfter states(this);
  const Operator* op = javascript()->CreateArguments(type);
  Node* object = NewNode(op, GetFunctionClosure());
  environment()->BindAccumulator(object, &states);
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

void BytecodeGraphBuilder::BuildCreateLiteral(const Operator* op) {
  FrameStateBeforeAndAfter states(this);
  Node* literal = NewNode(op, GetFunctionClosure());
  environment()->BindAccumulator(literal, &states);
}

void BytecodeGraphBuilder::VisitCreateRegExpLiteral() {
  Handle<String> constant_pattern =
      Handle<String>::cast(bytecode_iterator().GetConstantForIndexOperand(0));
  int literal_index = bytecode_iterator().GetIndexOperand(1);
  int literal_flags = bytecode_iterator().GetFlagOperand(2);
  const Operator* op = javascript()->CreateLiteralRegExp(
      constant_pattern, literal_flags, literal_index);
  BuildCreateLiteral(op);
}

void BytecodeGraphBuilder::VisitCreateArrayLiteral() {
  Handle<FixedArray> constant_elements = Handle<FixedArray>::cast(
      bytecode_iterator().GetConstantForIndexOperand(0));
  int literal_index = bytecode_iterator().GetIndexOperand(1);
  int literal_flags = bytecode_iterator().GetFlagOperand(2);
  // Disable allocation site mementos. Only unoptimized code will collect
  // feedback about allocation site. Once the code is optimized we expect the
  // data to converge. So, we disable allocation site mementos in optimized
  // code. We can revisit this when we have data to the contrary.
  literal_flags |= ArrayLiteral::kDisableMementos;
  int number_of_elements = constant_elements->length();
  const Operator* op = javascript()->CreateLiteralArray(
      constant_elements, literal_flags, literal_index, number_of_elements);
  BuildCreateLiteral(op);
}

void BytecodeGraphBuilder::VisitCreateObjectLiteral() {
  FrameStateBeforeAndAfter states(this);
  Handle<FixedArray> constant_properties = Handle<FixedArray>::cast(
      bytecode_iterator().GetConstantForIndexOperand(0));
  int literal_index = bytecode_iterator().GetIndexOperand(1);
  int bytecode_flags = bytecode_iterator().GetFlagOperand(2);
  int literal_flags =
      interpreter::CreateObjectLiteralFlags::FlagsBits::decode(bytecode_flags);
  // TODO(mstarzinger): Thread through number of properties.
  int number_of_properties = constant_properties->length() / 2;
  Node* literal = NewNode(
      javascript()->CreateLiteralObject(constant_properties, literal_flags,
                                        literal_index, number_of_properties),
      GetFunctionClosure());
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(3),
                              literal, &states);
}

Node* BytecodeGraphBuilder::ProcessCallArguments(const Operator* call_op,
                                                 Node* callee,
                                                 interpreter::Register receiver,
                                                 size_t arity) {
  Node** all = local_zone()->NewArray<Node*>(static_cast<int>(arity));
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

void BytecodeGraphBuilder::BuildCall(TailCallMode tail_call_mode) {
  FrameStateBeforeAndAfter states(this);
  ConvertReceiverMode receiver_hint = ConvertReceiverMode::kAny;
  Node* callee =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  interpreter::Register receiver = bytecode_iterator().GetRegisterOperand(1);
  size_t arg_count = bytecode_iterator().GetRegisterCountOperand(2);

  // Slot index of 0 is used indicate no feedback slot is available. Assert
  // the assumption that slot index 0 is never a valid feedback slot.
  STATIC_ASSERT(TypeFeedbackVector::kReservedIndexCount > 0);
  int const slot_id = bytecode_iterator().GetIndexOperand(3);
  VectorSlotPair feedback = CreateVectorSlotPair(slot_id);

  float const frequency = ComputeCallFrequency(slot_id);
  const Operator* call = javascript()->CallFunction(
      arg_count + 1, frequency, feedback, receiver_hint, tail_call_mode);
  Node* value = ProcessCallArguments(call, callee, receiver, arg_count + 1);
  environment()->BindAccumulator(value, &states);
}

void BytecodeGraphBuilder::VisitCall() { BuildCall(TailCallMode::kDisallow); }

void BytecodeGraphBuilder::VisitTailCall() {
  TailCallMode tail_call_mode =
      bytecode_array_->GetIsolate()->is_tail_call_elimination_enabled()
          ? TailCallMode::kAllow
          : TailCallMode::kDisallow;
  BuildCall(tail_call_mode);
}

void BytecodeGraphBuilder::VisitCallJSRuntime() {
  FrameStateBeforeAndAfter states(this);
  Node* callee =
      BuildLoadNativeContextField(bytecode_iterator().GetIndexOperand(0));
  interpreter::Register receiver = bytecode_iterator().GetRegisterOperand(1);
  size_t arg_count = bytecode_iterator().GetRegisterCountOperand(2);

  // Create node to perform the JS runtime call.
  const Operator* call = javascript()->CallFunction(arg_count + 1);
  Node* value = ProcessCallArguments(call, callee, receiver, arg_count + 1);
  environment()->BindAccumulator(value, &states);
}

Node* BytecodeGraphBuilder::ProcessCallRuntimeArguments(
    const Operator* call_runtime_op, interpreter::Register first_arg,
    size_t arity) {
  Node** all = local_zone()->NewArray<Node*>(arity);
  int first_arg_index = first_arg.index();
  for (int i = 0; i < static_cast<int>(arity); ++i) {
    all[i] = environment()->LookupRegister(
        interpreter::Register(first_arg_index + i));
  }
  Node* value = MakeNode(call_runtime_op, static_cast<int>(arity), all, false);
  return value;
}

void BytecodeGraphBuilder::VisitCallRuntime() {
  FrameStateBeforeAndAfter states(this);
  Runtime::FunctionId functionId = bytecode_iterator().GetRuntimeIdOperand(0);
  interpreter::Register first_arg = bytecode_iterator().GetRegisterOperand(1);
  size_t arg_count = bytecode_iterator().GetRegisterCountOperand(2);

  // Create node to perform the runtime call.
  const Operator* call = javascript()->CallRuntime(functionId, arg_count);
  Node* value = ProcessCallRuntimeArguments(call, first_arg, arg_count);
  environment()->BindAccumulator(value, &states);
}

void BytecodeGraphBuilder::VisitCallRuntimeForPair() {
  FrameStateBeforeAndAfter states(this);
  Runtime::FunctionId functionId = bytecode_iterator().GetRuntimeIdOperand(0);
  interpreter::Register first_arg = bytecode_iterator().GetRegisterOperand(1);
  size_t arg_count = bytecode_iterator().GetRegisterCountOperand(2);
  interpreter::Register first_return =
      bytecode_iterator().GetRegisterOperand(3);

  // Create node to perform the runtime call.
  const Operator* call = javascript()->CallRuntime(functionId, arg_count);
  Node* return_pair = ProcessCallRuntimeArguments(call, first_arg, arg_count);
  environment()->BindRegistersToProjections(first_return, return_pair, &states);
}

void BytecodeGraphBuilder::VisitInvokeIntrinsic() {
  FrameStateBeforeAndAfter states(this);
  Runtime::FunctionId functionId = bytecode_iterator().GetIntrinsicIdOperand(0);
  interpreter::Register first_arg = bytecode_iterator().GetRegisterOperand(1);
  size_t arg_count = bytecode_iterator().GetRegisterCountOperand(2);

  // Create node to perform the runtime call. Turbofan will take care of the
  // lowering.
  const Operator* call = javascript()->CallRuntime(functionId, arg_count);
  Node* value = ProcessCallRuntimeArguments(call, first_arg, arg_count);
  environment()->BindAccumulator(value, &states);
}

Node* BytecodeGraphBuilder::ProcessCallNewArguments(
    const Operator* call_new_op, Node* callee, Node* new_target,
    interpreter::Register first_arg, size_t arity) {
  Node** all = local_zone()->NewArray<Node*>(arity);
  all[0] = callee;
  int first_arg_index = first_arg.index();
  for (int i = 1; i < static_cast<int>(arity) - 1; ++i) {
    all[i] = environment()->LookupRegister(
        interpreter::Register(first_arg_index + i - 1));
  }
  all[arity - 1] = new_target;
  Node* value = MakeNode(call_new_op, static_cast<int>(arity), all, false);
  return value;
}

void BytecodeGraphBuilder::VisitNew() {
  FrameStateBeforeAndAfter states(this);
  interpreter::Register callee_reg = bytecode_iterator().GetRegisterOperand(0);
  interpreter::Register first_arg = bytecode_iterator().GetRegisterOperand(1);
  size_t arg_count = bytecode_iterator().GetRegisterCountOperand(2);
  // Slot index of 0 is used indicate no feedback slot is available. Assert
  // the assumption that slot index 0 is never a valid feedback slot.
  STATIC_ASSERT(TypeFeedbackVector::kReservedIndexCount > 0);
  int const slot_id = bytecode_iterator().GetIndexOperand(3);
  VectorSlotPair feedback = CreateVectorSlotPair(slot_id);

  Node* new_target = environment()->LookupAccumulator();
  Node* callee = environment()->LookupRegister(callee_reg);

  float const frequency = ComputeCallFrequency(slot_id);
  const Operator* call = javascript()->CallConstruct(
      static_cast<int>(arg_count) + 2, frequency, feedback);
  Node* value = ProcessCallNewArguments(call, callee, new_target, first_arg,
                                        arg_count + 2);
  environment()->BindAccumulator(value, &states);
}

void BytecodeGraphBuilder::BuildThrow() {
  FrameStateBeforeAndAfter states(this);
  Node* value = environment()->LookupAccumulator();
  Node* call = NewNode(javascript()->CallRuntime(Runtime::kThrow), value);
  environment()->BindAccumulator(call, &states);
}

void BytecodeGraphBuilder::VisitThrow() {
  BuildLoopExitsForFunctionExit();
  BuildThrow();
  Node* call = environment()->LookupAccumulator();
  Node* control = NewNode(common()->Throw(), call);
  MergeControlToLeaveFunction(control);
}

void BytecodeGraphBuilder::VisitReThrow() {
  BuildLoopExitsForFunctionExit();
  Node* value = environment()->LookupAccumulator();
  Node* call = NewNode(javascript()->CallRuntime(Runtime::kReThrow), value);
  Node* control = NewNode(common()->Throw(), call);
  MergeControlToLeaveFunction(control);
}

void BytecodeGraphBuilder::BuildBinaryOp(const Operator* js_op) {
  FrameStateBeforeAndAfter states(this);
  Node* left =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* right = environment()->LookupAccumulator();
  Node* node = NewNode(js_op, left, right);
  environment()->BindAccumulator(node, &states);
}

// Helper function to create binary operation hint from the recorded type
// feedback.
BinaryOperationHint BytecodeGraphBuilder::GetBinaryOperationHint(
    int operand_index) {
  FeedbackVectorSlot slot = feedback_vector()->ToSlot(
      bytecode_iterator().GetIndexOperand(operand_index));
  DCHECK_EQ(FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC,
            feedback_vector()->GetKind(slot));
  BinaryOpICNexus nexus(feedback_vector(), slot);
  return nexus.GetBinaryOperationFeedback();
}

// Helper function to create compare operation hint from the recorded type
// feedback.
CompareOperationHint BytecodeGraphBuilder::GetCompareOperationHint() {
  int slot_index = bytecode_iterator().GetIndexOperand(1);
  if (slot_index == 0) {
    return CompareOperationHint::kAny;
  }
  FeedbackVectorSlot slot =
      feedback_vector()->ToSlot(bytecode_iterator().GetIndexOperand(1));
  DCHECK_EQ(FeedbackVectorSlotKind::INTERPRETER_COMPARE_IC,
            feedback_vector()->GetKind(slot));
  CompareICNexus nexus(feedback_vector(), slot);
  return nexus.GetCompareOperationFeedback();
}

float BytecodeGraphBuilder::ComputeCallFrequency(int slot_id) const {
  if (slot_id >= TypeFeedbackVector::kReservedIndexCount) {
    CallICNexus nexus(feedback_vector(), feedback_vector()->ToSlot(slot_id));
    return nexus.ComputeCallFrequency() * invocation_frequency_;
  }
  return 0.0f;
}

void BytecodeGraphBuilder::VisitAdd() {
  BuildBinaryOp(
      javascript()->Add(GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitSub() {
  BuildBinaryOp(javascript()->Subtract(
      GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitMul() {
  BuildBinaryOp(javascript()->Multiply(
      GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitDiv() {
  BuildBinaryOp(
      javascript()->Divide(GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitMod() {
  BuildBinaryOp(
      javascript()->Modulus(GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitBitwiseOr() {
  BuildBinaryOp(javascript()->BitwiseOr(
      GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitBitwiseXor() {
  BuildBinaryOp(javascript()->BitwiseXor(
      GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitBitwiseAnd() {
  BuildBinaryOp(javascript()->BitwiseAnd(
      GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitShiftLeft() {
  BuildBinaryOp(javascript()->ShiftLeft(
      GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitShiftRight() {
  BuildBinaryOp(javascript()->ShiftRight(
      GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::VisitShiftRightLogical() {
  BuildBinaryOp(javascript()->ShiftRightLogical(
      GetBinaryOperationHint(kBinaryOperationHintIndex)));
}

void BytecodeGraphBuilder::BuildBinaryOpWithImmediate(const Operator* js_op) {
  FrameStateBeforeAndAfter states(this);
  Node* left =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  Node* right = jsgraph()->Constant(bytecode_iterator().GetImmediateOperand(0));
  Node* node = NewNode(js_op, left, right);
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::VisitAddSmi() {
  BuildBinaryOpWithImmediate(
      javascript()->Add(GetBinaryOperationHint(kBinaryOperationSmiHintIndex)));
}

void BytecodeGraphBuilder::VisitSubSmi() {
  BuildBinaryOpWithImmediate(javascript()->Subtract(
      GetBinaryOperationHint(kBinaryOperationSmiHintIndex)));
}

void BytecodeGraphBuilder::VisitBitwiseOrSmi() {
  BuildBinaryOpWithImmediate(javascript()->BitwiseOr(
      GetBinaryOperationHint(kBinaryOperationSmiHintIndex)));
}

void BytecodeGraphBuilder::VisitBitwiseAndSmi() {
  BuildBinaryOpWithImmediate(javascript()->BitwiseAnd(
      GetBinaryOperationHint(kBinaryOperationSmiHintIndex)));
}

void BytecodeGraphBuilder::VisitShiftLeftSmi() {
  BuildBinaryOpWithImmediate(javascript()->ShiftLeft(
      GetBinaryOperationHint(kBinaryOperationSmiHintIndex)));
}

void BytecodeGraphBuilder::VisitShiftRightSmi() {
  BuildBinaryOpWithImmediate(javascript()->ShiftRight(
      GetBinaryOperationHint(kBinaryOperationSmiHintIndex)));
}

void BytecodeGraphBuilder::VisitInc() {
  FrameStateBeforeAndAfter states(this);
  // Note: Use subtract -1 here instead of add 1 to ensure we always convert to
  // a number, not a string.
  const Operator* js_op =
      javascript()->Subtract(GetBinaryOperationHint(kCountOperationHintIndex));
  Node* node = NewNode(js_op, environment()->LookupAccumulator(),
                       jsgraph()->Constant(-1));
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::VisitDec() {
  FrameStateBeforeAndAfter states(this);
  const Operator* js_op =
      javascript()->Subtract(GetBinaryOperationHint(kCountOperationHintIndex));
  Node* node = NewNode(js_op, environment()->LookupAccumulator(),
                       jsgraph()->OneConstant());
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::VisitLogicalNot() {
  Node* value = environment()->LookupAccumulator();
  Node* node = NewNode(common()->Select(MachineRepresentation::kTagged), value,
                       jsgraph()->FalseConstant(), jsgraph()->TrueConstant());
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitToBooleanLogicalNot() {
  Node* value = NewNode(javascript()->ToBoolean(ToBooleanHint::kAny),
                        environment()->LookupAccumulator());
  Node* node = NewNode(common()->Select(MachineRepresentation::kTagged), value,
                       jsgraph()->FalseConstant(), jsgraph()->TrueConstant());
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::VisitTypeOf() {
  Node* node =
      NewNode(javascript()->TypeOf(), environment()->LookupAccumulator());
  environment()->BindAccumulator(node);
}

void BytecodeGraphBuilder::BuildDelete(LanguageMode language_mode) {
  FrameStateBeforeAndAfter states(this);
  Node* key = environment()->LookupAccumulator();
  Node* object =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* node =
      NewNode(javascript()->DeleteProperty(language_mode), object, key);
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::VisitDeletePropertyStrict() {
  BuildDelete(LanguageMode::STRICT);
}

void BytecodeGraphBuilder::VisitDeletePropertySloppy() {
  BuildDelete(LanguageMode::SLOPPY);
}

void BytecodeGraphBuilder::BuildCompareOp(const Operator* js_op) {
  FrameStateBeforeAndAfter states(this);
  Node* left =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* right = environment()->LookupAccumulator();
  Node* node = NewNode(js_op, left, right);
  environment()->BindAccumulator(node, &states);
}

void BytecodeGraphBuilder::VisitTestEqual() {
  BuildCompareOp(javascript()->Equal(GetCompareOperationHint()));
}

void BytecodeGraphBuilder::VisitTestNotEqual() {
  BuildCompareOp(javascript()->NotEqual(GetCompareOperationHint()));
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

void BytecodeGraphBuilder::VisitTestIn() {
  BuildCompareOp(javascript()->HasProperty());
}

void BytecodeGraphBuilder::VisitTestInstanceOf() {
  BuildCompareOp(javascript()->InstanceOf());
}

void BytecodeGraphBuilder::BuildCastOperator(const Operator* js_op) {
  FrameStateBeforeAndAfter states(this);
  Node* value = NewNode(js_op, environment()->LookupAccumulator());
  environment()->BindRegister(bytecode_iterator().GetRegisterOperand(0), value,
                              &states);
}

void BytecodeGraphBuilder::VisitToName() {
  BuildCastOperator(javascript()->ToName());
}

void BytecodeGraphBuilder::VisitToObject() {
  BuildCastOperator(javascript()->ToObject());
}

void BytecodeGraphBuilder::VisitToNumber() {
  BuildCastOperator(javascript()->ToNumber());
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

void BytecodeGraphBuilder::VisitJumpIfNotHole() { BuildJumpIfNotHole(); }

void BytecodeGraphBuilder::VisitJumpIfNotHoleConstant() {
  BuildJumpIfNotHole();
}

void BytecodeGraphBuilder::VisitJumpIfNull() {
  BuildJumpIfEqual(jsgraph()->NullConstant());
}

void BytecodeGraphBuilder::VisitJumpIfNullConstant() {
  BuildJumpIfEqual(jsgraph()->NullConstant());
}

void BytecodeGraphBuilder::VisitJumpIfUndefined() {
  BuildJumpIfEqual(jsgraph()->UndefinedConstant());
}

void BytecodeGraphBuilder::VisitJumpIfUndefinedConstant() {
  BuildJumpIfEqual(jsgraph()->UndefinedConstant());
}

void BytecodeGraphBuilder::VisitJumpLoop() { BuildJump(); }

void BytecodeGraphBuilder::VisitStackCheck() {
  FrameStateBeforeAndAfter states(this);
  Node* node = NewNode(javascript()->StackCheck());
  environment()->RecordAfterState(node, &states);
}

void BytecodeGraphBuilder::VisitReturn() {
  BuildLoopExitsForFunctionExit();
  Node* control =
      NewNode(common()->Return(), environment()->LookupAccumulator());
  MergeControlToLeaveFunction(control);
}

void BytecodeGraphBuilder::VisitDebugger() {
  FrameStateBeforeAndAfter states(this);
  Node* call =
      NewNode(javascript()->CallRuntime(Runtime::kHandleDebuggerStatement));
  environment()->BindAccumulator(call, &states);
  environment()->MarkAllRegistersLive();
}

// We cannot create a graph from the debugger copy of the bytecode array.
#define DEBUG_BREAK(Name, ...) \
  void BytecodeGraphBuilder::Visit##Name() { UNREACHABLE(); }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK);
#undef DEBUG_BREAK

void BytecodeGraphBuilder::BuildForInPrepare() {
  FrameStateBeforeAndAfter states(this);
  Node* receiver =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* prepare = NewNode(javascript()->ForInPrepare(), receiver);
  environment()->BindRegistersToProjections(
      bytecode_iterator().GetRegisterOperand(1), prepare, &states);
}

void BytecodeGraphBuilder::VisitForInPrepare() { BuildForInPrepare(); }

void BytecodeGraphBuilder::VisitForInContinue() {
  FrameStateBeforeAndAfter states(this);
  Node* index =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* cache_length =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  Node* exit_cond =
      NewNode(javascript()->LessThan(CompareOperationHint::kSignedSmall), index,
              cache_length);
  environment()->BindAccumulator(exit_cond, &states);
}

void BytecodeGraphBuilder::BuildForInNext() {
  FrameStateBeforeAndAfter states(this);
  Node* receiver =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  Node* index =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(1));
  int catch_reg_pair_index = bytecode_iterator().GetRegisterOperand(2).index();
  Node* cache_type = environment()->LookupRegister(
      interpreter::Register(catch_reg_pair_index));
  Node* cache_array = environment()->LookupRegister(
      interpreter::Register(catch_reg_pair_index + 1));

  Node* value = NewNode(javascript()->ForInNext(), receiver, cache_array,
                        cache_type, index);
  environment()->BindAccumulator(value, &states);
}

void BytecodeGraphBuilder::VisitForInNext() { BuildForInNext(); }

void BytecodeGraphBuilder::VisitForInStep() {
  FrameStateBeforeAndAfter states(this);
  Node* index =
      environment()->LookupRegister(bytecode_iterator().GetRegisterOperand(0));
  index = NewNode(javascript()->Add(BinaryOperationHint::kSignedSmall), index,
                  jsgraph()->OneConstant());
  environment()->BindAccumulator(index, &states);
}

void BytecodeGraphBuilder::VisitSuspendGenerator() {
  Node* state = environment()->LookupAccumulator();
  Node* generator = environment()->LookupRegister(
      bytecode_iterator().GetRegisterOperand(0));
  // The offsets used by the bytecode iterator are relative to a different base
  // than what is used in the interpreter, hence the addition.
  Node* offset =
      jsgraph()->Constant(bytecode_iterator().current_offset() +
                          (BytecodeArray::kHeaderSize - kHeapObjectTag));

  int register_count = environment()->register_count();
  int value_input_count = 3 + register_count;

  Node** value_inputs = local_zone()->NewArray<Node*>(value_input_count);
  value_inputs[0] = generator;
  value_inputs[1] = state;
  value_inputs[2] = offset;
  for (int i = 0; i < register_count; ++i) {
    value_inputs[3 + i] =
        environment()->LookupRegister(interpreter::Register(i));
  }

  MakeNode(javascript()->GeneratorStore(register_count), value_input_count,
           value_inputs, false);
}

void BytecodeGraphBuilder::VisitResumeGenerator() {
  FrameStateBeforeAndAfter states(this);

  Node* generator = environment()->LookupRegister(
      bytecode_iterator().GetRegisterOperand(0));

  // Bijection between registers and array indices must match that used in
  // InterpreterAssembler::ExportRegisterFile.
  for (int i = 0; i < environment()->register_count(); ++i) {
    Node* value = NewNode(javascript()->GeneratorRestoreRegister(i), generator);
    environment()->BindRegister(interpreter::Register(i), value);
  }

  Node* state =
      NewNode(javascript()->GeneratorRestoreContinuation(), generator);

  environment()->BindAccumulator(state, &states);
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

void BytecodeGraphBuilder::VisitNop() {}

void BytecodeGraphBuilder::SwitchToMergeEnvironment(int current_offset) {
  if (merge_environments_[current_offset] != nullptr) {
    if (environment() != nullptr) {
      merge_environments_[current_offset]->Merge(environment());
    }
    set_environment(merge_environments_[current_offset]);
  }
}

void BytecodeGraphBuilder::BuildLoopHeaderEnvironment(int current_offset) {
  if (branch_analysis()->backward_branches_target(current_offset)) {
    // Add loop header and store a copy so we can connect merged back
    // edge inputs to the loop header.
    merge_environments_[current_offset] = environment()->CopyForLoop();
  }
}

void BytecodeGraphBuilder::MergeIntoSuccessorEnvironment(int target_offset) {
  BuildLoopExitsForBranch(target_offset);
  if (merge_environments_[target_offset] == nullptr) {
    // Append merge nodes to the environment. We may merge here with another
    // environment. So add a place holder for merge nodes. We may add redundant
    // but will be eliminated in a later pass.
    // TODO(mstarzinger): Be smarter about this!
    NewMerge();
    merge_environments_[target_offset] = environment();
  } else {
    merge_environments_[target_offset]->Merge(environment());
  }
  set_environment(nullptr);
}

void BytecodeGraphBuilder::MergeControlToLeaveFunction(Node* exit) {
  exit_controls_.push_back(exit);
  set_environment(nullptr);
}

void BytecodeGraphBuilder::BuildOSRLoopEntryPoint(int current_offset) {
  if (!osr_ast_id_.IsNone() && osr_ast_id_.ToInt() == current_offset) {
    // For OSR add a special {OsrLoopEntry} node into the current loop header.
    // It will be turned into a usable entry by the OSR deconstruction.
    Environment* loop_env = merge_environments_[current_offset];
    Environment* osr_env = loop_env->CopyForOsrEntry();
    osr_env->PrepareForOsrEntry();
    loop_env->Merge(osr_env);
  }
}

void BytecodeGraphBuilder::BuildOSRNormalEntryPoint() {
  if (!osr_ast_id_.IsNone()) {
    // For OSR add an {OsrNormalEntry} as the the top-level environment start.
    // It will be replaced with {Dead} by the OSR deconstruction.
    NewNode(common()->OsrNormalEntry());
    // Note that the requested OSR entry point must be the target of a backward
    // branch, otherwise there will not be a proper loop header available.
    DCHECK(branch_analysis()->backward_branches_target(osr_ast_id_.ToInt()));
  }
}

void BytecodeGraphBuilder::BuildLoopExitsForBranch(int target_offset) {
  int origin_offset = bytecode_iterator().current_offset();
  // Only build loop exits for forward edges.
  if (target_offset > origin_offset) {
    BuildLoopExitsUntilLoop(loop_analysis()->GetLoopOffsetFor(target_offset));
  }
}

void BytecodeGraphBuilder::BuildLoopExitsUntilLoop(int loop_offset) {
  int origin_offset = bytecode_iterator().current_offset();
  int current_loop = loop_analysis()->GetLoopOffsetFor(origin_offset);
  while (loop_offset < current_loop) {
    Node* loop_node = merge_environments_[current_loop]->GetControlDependency();
    environment()->PrepareForLoopExit(loop_node);
    current_loop = loop_analysis()->GetParentLoopFor(current_loop);
  }
}

void BytecodeGraphBuilder::BuildLoopExitsForFunctionExit() {
  BuildLoopExitsUntilLoop(-1);
}

void BytecodeGraphBuilder::BuildJump() {
  MergeIntoSuccessorEnvironment(bytecode_iterator().GetJumpTargetOffset());
}

void BytecodeGraphBuilder::BuildJumpIf(Node* condition) {
  NewBranch(condition);
  Environment* if_false_environment = environment()->CopyForConditional();
  NewIfTrue();
  MergeIntoSuccessorEnvironment(bytecode_iterator().GetJumpTargetOffset());
  set_environment(if_false_environment);
  NewIfFalse();
}

void BytecodeGraphBuilder::BuildJumpIfNot(Node* condition) {
  NewBranch(condition);
  Environment* if_true_environment = environment()->CopyForConditional();
  NewIfFalse();
  MergeIntoSuccessorEnvironment(bytecode_iterator().GetJumpTargetOffset());
  set_environment(if_true_environment);
  NewIfTrue();
}

void BytecodeGraphBuilder::BuildJumpIfEqual(Node* comperand) {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition =
      NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
              accumulator, comperand);
  BuildJumpIf(condition);
}

void BytecodeGraphBuilder::BuildJumpIfFalse() {
  BuildJumpIfNot(environment()->LookupAccumulator());
}

void BytecodeGraphBuilder::BuildJumpIfTrue() {
  BuildJumpIf(environment()->LookupAccumulator());
}

void BytecodeGraphBuilder::BuildJumpIfToBooleanTrue() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition =
      NewNode(javascript()->ToBoolean(ToBooleanHint::kAny), accumulator);
  BuildJumpIf(condition);
}

void BytecodeGraphBuilder::BuildJumpIfToBooleanFalse() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition =
      NewNode(javascript()->ToBoolean(ToBooleanHint::kAny), accumulator);
  BuildJumpIfNot(condition);
}

void BytecodeGraphBuilder::BuildJumpIfNotHole() {
  Node* accumulator = environment()->LookupAccumulator();
  Node* condition =
      NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
              accumulator, jsgraph()->TheHoleConstant());
  BuildJumpIfNot(condition);
}

Node** BytecodeGraphBuilder::EnsureInputBufferSize(int size) {
  if (size > input_buffer_size_) {
    size = size + kInputBufferSizeIncrement + input_buffer_size_;
    input_buffer_ = local_zone()->NewArray<Node*>(size);
    input_buffer_size_ = size;
  }
  return input_buffer_;
}

void BytecodeGraphBuilder::EnterAndExitExceptionHandlers(int current_offset) {
  Handle<HandlerTable> table = exception_handler_table();
  int num_entries = table->NumberOfRangeEntries();

  // Potentially exit exception handlers.
  while (!exception_handlers_.empty()) {
    int current_end = exception_handlers_.top().end_offset_;
    if (current_offset < current_end) break;  // Still covered by range.
    exception_handlers_.pop();
  }

  // Potentially enter exception handlers.
  while (current_exception_handler_ < num_entries) {
    int next_start = table->GetRangeStart(current_exception_handler_);
    if (current_offset < next_start) break;  // Not yet covered by range.
    int next_end = table->GetRangeEnd(current_exception_handler_);
    int next_handler = table->GetRangeHandler(current_exception_handler_);
    int context_register = table->GetRangeData(current_exception_handler_);
    exception_handlers_.push(
        {next_start, next_end, next_handler, context_register});
    current_exception_handler_++;
  }
}

Node* BytecodeGraphBuilder::MakeNode(const Operator* op, int value_input_count,
                                     Node** value_inputs, bool incomplete) {
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
    // Update the current control dependency for control-producing nodes.
    if (NodeProperties::IsControl(result)) {
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
      Environment* success_env = environment()->CopyForConditional();
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
    if (!result->op()->HasProperty(Operator::kNoThrow)) {
      const Operator* if_success = common()->IfSuccess();
      Node* on_success = graph()->NewNode(if_success, result);
      environment()->UpdateControlDependency(on_success);
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

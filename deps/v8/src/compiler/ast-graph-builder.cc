// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/ast-graph-builder.h"

#include "src/ast/compile-time-value.h"
#include "src/ast/scopes.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/compiler/ast-loop-assignment-analyzer.h"
#include "src/compiler/control-builders.h"
#include "src/compiler/linkage.h"
#include "src/compiler/liveness-analyzer.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/state-values-utils.h"
#include "src/feedback-vector.h"
#include "src/objects-inl.h"
#include "src/objects/literal-objects.h"

namespace v8 {
namespace internal {
namespace compiler {


// Each expression in the AST is evaluated in a specific context. This context
// decides how the evaluation result is passed up the visitor.
class AstGraphBuilder::AstContext BASE_EMBEDDED {
 public:
  bool IsEffect() const { return kind_ == Expression::kEffect; }
  bool IsValue() const { return kind_ == Expression::kValue; }
  bool IsTest() const { return kind_ == Expression::kTest; }

  // Determines how to combine the frame state with the value
  // that is about to be plugged into this AstContext.
  OutputFrameStateCombine GetStateCombine() {
    return IsEffect() ? OutputFrameStateCombine::Ignore()
                      : OutputFrameStateCombine::Push();
  }

  // Plug a node into this expression context.  Call this function in tail
  // position in the Visit functions for expressions.
  virtual void ProduceValue(Expression* expr, Node* value) = 0;

  // Unplugs a node from this expression context.  Call this to retrieve the
  // result of another Visit function that already plugged the context.
  virtual Node* ConsumeValue() = 0;

  // Shortcut for "context->ProduceValue(context->ConsumeValue())".
  void ReplaceValue(Expression* expr) { ProduceValue(expr, ConsumeValue()); }

 protected:
  AstContext(AstGraphBuilder* owner, Expression::Context kind);
  virtual ~AstContext();

  AstGraphBuilder* owner() const { return owner_; }
  Environment* environment() const { return owner_->environment(); }

// We want to be able to assert, in a context-specific way, that the stack
// height makes sense when the context is filled.
#ifdef DEBUG
  int original_height_;
#endif

 private:
  Expression::Context kind_;
  AstGraphBuilder* owner_;
  AstContext* outer_;
};


// Context to evaluate expression for its side effects only.
class AstGraphBuilder::AstEffectContext final : public AstContext {
 public:
  explicit AstEffectContext(AstGraphBuilder* owner)
      : AstContext(owner, Expression::kEffect) {}
  ~AstEffectContext() final;
  void ProduceValue(Expression* expr, Node* value) final;
  Node* ConsumeValue() final;
};


// Context to evaluate expression for its value (and side effects).
class AstGraphBuilder::AstValueContext final : public AstContext {
 public:
  explicit AstValueContext(AstGraphBuilder* owner)
      : AstContext(owner, Expression::kValue) {}
  ~AstValueContext() final;
  void ProduceValue(Expression* expr, Node* value) final;
  Node* ConsumeValue() final;
};


// Context to evaluate expression for a condition value (and side effects).
class AstGraphBuilder::AstTestContext final : public AstContext {
 public:
  AstTestContext(AstGraphBuilder* owner, TypeFeedbackId feedback_id)
      : AstContext(owner, Expression::kTest), feedback_id_(feedback_id) {}
  ~AstTestContext() final;
  void ProduceValue(Expression* expr, Node* value) final;
  Node* ConsumeValue() final;

 private:
  TypeFeedbackId const feedback_id_;
};


// Scoped class tracking context objects created by the visitor. Represents
// mutations of the context chain within the function body and allows to
// change the current {scope} and {context} during visitation.
class AstGraphBuilder::ContextScope BASE_EMBEDDED {
 public:
  ContextScope(AstGraphBuilder* builder, Scope* scope, Node* context)
      : builder_(builder),
        outer_(builder->execution_context()),
        scope_(scope),
        depth_(builder_->environment()->context_chain_length()) {
    builder_->environment()->PushContext(context);  // Push.
    builder_->set_execution_context(this);
  }

  ~ContextScope() {
    builder_->set_execution_context(outer_);  // Pop.
    builder_->environment()->PopContext();
    CHECK_EQ(depth_, builder_->environment()->context_chain_length());
  }

  // Current scope during visitation.
  Scope* scope() const { return scope_; }

 private:
  AstGraphBuilder* builder_;
  ContextScope* outer_;
  Scope* scope_;
  int depth_;
};


// Scoped class tracking control statements entered by the visitor. There are
// different types of statements participating in this stack to properly track
// local as well as non-local control flow:
//  - IterationStatement : Allows proper 'break' and 'continue' behavior.
//  - BreakableStatement : Allows 'break' from block and switch statements.
//  - TryCatchStatement  : Intercepts 'throw' and implicit exceptional edges.
//  - TryFinallyStatement: Intercepts 'break', 'continue', 'throw' and 'return'.
class AstGraphBuilder::ControlScope BASE_EMBEDDED {
 public:
  explicit ControlScope(AstGraphBuilder* builder)
      : builder_(builder),
        outer_(builder->execution_control()),
        context_length_(builder->environment()->context_chain_length()),
        stack_height_(builder->environment()->stack_height()) {
    builder_->set_execution_control(this);  // Push.
  }

  virtual ~ControlScope() {
    builder_->set_execution_control(outer_);  // Pop.
  }

  // Either 'break' or 'continue' to the target statement.
  void BreakTo(BreakableStatement* target);
  void ContinueTo(BreakableStatement* target);

  // Either 'return' or 'throw' the given value.
  void ReturnValue(Node* return_value);
  void ThrowValue(Node* exception_value);

 protected:
  enum Command { CMD_BREAK, CMD_CONTINUE, CMD_RETURN, CMD_THROW };

  // Performs one of the above commands on this stack of control scopes. This
  // walks through the stack giving each scope a chance to execute or defer the
  // given command by overriding the {Execute} method appropriately. Note that
  // this also drops extra operands from the environment for each skipped scope.
  void PerformCommand(Command cmd, Statement* target, Node* value);

  // Interface to execute a given command in this scope. Returning {true} here
  // indicates successful execution whereas {false} requests to skip scope.
  virtual bool Execute(Command cmd, Statement* target, Node** value) {
    // For function-level control.
    switch (cmd) {
      case CMD_THROW:
        builder()->BuildThrow(*value);
        return true;
      case CMD_RETURN:
        builder()->BuildReturn(*value);
        return true;
      case CMD_BREAK:
      case CMD_CONTINUE:
        break;
    }
    return false;
  }

  Environment* environment() { return builder_->environment(); }
  AstGraphBuilder* builder() const { return builder_; }
  int context_length() const { return context_length_; }
  int stack_height() const { return stack_height_; }

 private:
  AstGraphBuilder* builder_;
  ControlScope* outer_;
  int context_length_;
  int stack_height_;
};


// Control scope implementation for a BreakableStatement.
class AstGraphBuilder::ControlScopeForBreakable : public ControlScope {
 public:
  ControlScopeForBreakable(AstGraphBuilder* owner, BreakableStatement* target,
                           ControlBuilder* control)
      : ControlScope(owner), target_(target), control_(control) {}

 protected:
  bool Execute(Command cmd, Statement* target, Node** value) override {
    if (target != target_) return false;  // We are not the command target.
    switch (cmd) {
      case CMD_BREAK:
        control_->Break();
        return true;
      case CMD_CONTINUE:
      case CMD_THROW:
      case CMD_RETURN:
        break;
    }
    return false;
  }

 private:
  BreakableStatement* target_;
  ControlBuilder* control_;
};


// Control scope implementation for an IterationStatement.
class AstGraphBuilder::ControlScopeForIteration : public ControlScope {
 public:
  ControlScopeForIteration(AstGraphBuilder* owner, IterationStatement* target,
                           LoopBuilder* control)
      : ControlScope(owner), target_(target), control_(control) {}

 protected:
  bool Execute(Command cmd, Statement* target, Node** value) override {
    if (target != target_) {
      control_->ExitLoop(value);
      return false;
    }
    switch (cmd) {
      case CMD_BREAK:
        control_->Break();
        return true;
      case CMD_CONTINUE:
        control_->Continue();
        return true;
      case CMD_THROW:
      case CMD_RETURN:
        break;
    }
    return false;
  }

 private:
  BreakableStatement* target_;
  LoopBuilder* control_;
};

AstGraphBuilder::AstGraphBuilder(Zone* local_zone, CompilationInfo* info,
                                 JSGraph* jsgraph,
                                 CallFrequency invocation_frequency,
                                 LoopAssignmentAnalysis* loop)
    : isolate_(info->isolate()),
      local_zone_(local_zone),
      info_(info),
      jsgraph_(jsgraph),
      invocation_frequency_(invocation_frequency),
      environment_(nullptr),
      ast_context_(nullptr),
      globals_(0, local_zone),
      execution_control_(nullptr),
      execution_context_(nullptr),
      input_buffer_size_(0),
      input_buffer_(nullptr),
      exit_controls_(local_zone),
      loop_assignment_analysis_(loop),
      state_values_cache_(jsgraph),
      liveness_analyzer_(static_cast<size_t>(info->scope()->num_stack_slots()),
                         false, local_zone),
      frame_state_function_info_(common()->CreateFrameStateFunctionInfo(
          FrameStateType::kJavaScriptFunction, info->num_parameters() + 1,
          info->scope()->num_stack_slots(), info->shared_info())) {
  InitializeAstVisitor(info->isolate());
}


Node* AstGraphBuilder::GetFunctionClosureForContext() {
  DeclarationScope* closure_scope = current_scope()->GetClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function as
    // their closure, not the anonymous closure containing the global code.
    return BuildLoadNativeContextField(Context::CLOSURE_INDEX);
  } else if (closure_scope->is_eval_scope()) {
    // Contexts nested inside eval code have the same closure as the context
    // calling eval, not the anonymous closure containing the eval code.
    const Operator* op =
        javascript()->LoadContext(0, Context::CLOSURE_INDEX, false);
    return NewNode(op);
  } else {
    DCHECK(closure_scope->is_function_scope());
    return GetFunctionClosure();
  }
}


Node* AstGraphBuilder::GetFunctionClosure() {
  if (!function_closure_.is_set()) {
    int index = Linkage::kJSCallClosureParamIndex;
    const Operator* op = common()->Parameter(index, "%closure");
    Node* node = NewNode(op, graph()->start());
    function_closure_.set(node);
  }
  return function_closure_.get();
}


Node* AstGraphBuilder::GetFunctionContext() {
  if (!function_context_.is_set()) {
    int params = info()->num_parameters_including_this();
    int index = Linkage::GetJSCallContextParamIndex(params);
    const Operator* op = common()->Parameter(index, "%context");
    Node* node = NewNode(op, graph()->start());
    function_context_.set(node);
  }
  return function_context_.get();
}

Node* AstGraphBuilder::GetEmptyFrameState() {
  if (!empty_frame_state_.is_set()) {
    const Operator* op = common()->FrameState(
        BailoutId::None(), OutputFrameStateCombine::Ignore(), nullptr);
    Node* node = graph()->NewNode(
        op, jsgraph()->EmptyStateValues(), jsgraph()->EmptyStateValues(),
        jsgraph()->EmptyStateValues(), jsgraph()->NoContextConstant(),
        jsgraph()->UndefinedConstant(), graph()->start());
    empty_frame_state_.set(node);
  }
  return empty_frame_state_.get();
}

bool AstGraphBuilder::CreateGraph(bool stack_check) {
  DeclarationScope* scope = info()->scope();
  DCHECK_NOT_NULL(graph());

  // Set up the basic structure of the graph. Outputs for {Start} are the formal
  // parameters (including the receiver) plus new target, number of arguments,
  // context and closure.
  int actual_parameter_count = info()->num_parameters_including_this() + 4;
  graph()->SetStart(graph()->NewNode(common()->Start(actual_parameter_count)));

  // Initialize the top-level environment.
  Environment env(this, scope, graph()->start());
  set_environment(&env);

  if (info()->is_osr()) {
    // Use OSR normal entry as the start of the top-level environment.
    // It will be replaced with {Dead} after typing and optimizations.
    NewNode(common()->OsrNormalEntry());
  }

  // Initialize the incoming context.
  ContextScope incoming(this, scope, GetFunctionContext());

  // Initialize control scope.
  ControlScope control(this);

  // TODO(mstarzinger): For now we cannot assume that the {this} parameter is
  // not {the_hole}, because for derived classes {this} has a TDZ and the
  // JSConstructStubForDerived magically passes {the_hole} as a receiver.
  if (scope->has_this_declaration() && scope->receiver()->mode() == CONST) {
    env.RawParameterBind(0, jsgraph()->TheHoleConstant());
  }

  if (scope->NeedsContext()) {
    // Push a new inner context scope for the current activation.
    Node* inner_context = BuildLocalActivationContext(GetFunctionContext());
    ContextScope top_context(this, scope, inner_context);
    CreateGraphBody(stack_check);
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

  // Compute local variable liveness information and use it to relax
  // frame states.
  ClearNonLiveSlotsInFrameStates();

  // Failures indicated by stack overflow.
  return !HasStackOverflow();
}


void AstGraphBuilder::CreateGraphBody(bool stack_check) {
  DeclarationScope* scope = info()->scope();

  // Build the arguments object if it is used.
  BuildArgumentsObject(scope->arguments());

  // We don't support new.target and rest parameters here.
  DCHECK_NULL(scope->new_target_var());
  DCHECK_NULL(scope->rest_parameter());
  DCHECK_NULL(scope->this_function_var());

  // Emit tracing call if requested to do so.
  if (FLAG_trace) {
    NewNode(javascript()->CallRuntime(Runtime::kTraceEnter));
  }

  // Visit declarations within the function scope.
  VisitDeclarations(scope->declarations());

  // Build a stack-check before the body.
  if (stack_check) {
    Node* node = NewNode(javascript()->StackCheck());
    PrepareFrameState(node, BailoutId::FunctionEntry());
  }

  // Visit statements in the function body.
  VisitStatements(info()->literal()->body());

  // Return 'undefined' in case we can fall off the end.
  BuildReturn(jsgraph()->UndefinedConstant());
}


void AstGraphBuilder::ClearNonLiveSlotsInFrameStates() {
  if (!FLAG_analyze_environment_liveness ||
      !info()->is_deoptimization_enabled()) {
    return;
  }

  NonLiveFrameStateSlotReplacer replacer(
      &state_values_cache_, jsgraph()->OptimizedOutConstant(),
      liveness_analyzer()->local_count(), false, local_zone());
  Variable* arguments = info()->scope()->arguments();
  if (arguments != nullptr && arguments->IsStackAllocated()) {
    replacer.MarkPermanentlyLive(arguments->index());
  }
  liveness_analyzer()->Run(&replacer);
  if (FLAG_trace_environment_liveness) {
    OFStream os(stdout);
    liveness_analyzer()->Print(os);
  }
}


// Gets the bailout id just before reading a variable proxy, but only for
// unallocated variables.
static BailoutId BeforeId(VariableProxy* proxy) {
  return proxy->var()->IsUnallocated() ? proxy->BeforeId() : BailoutId::None();
}

static const char* GetDebugParameterName(Zone* zone, DeclarationScope* scope,
                                         int index) {
#if DEBUG
  const AstRawString* name = scope->parameter(index)->raw_name();
  if (name && name->length() > 0) {
    char* data = zone->NewArray<char>(name->length() + 1);
    data[name->length()] = 0;
    memcpy(data, name->raw_data(), name->length());
    return data;
  }
#endif
  return nullptr;
}

AstGraphBuilder::Environment::Environment(AstGraphBuilder* builder,
                                          DeclarationScope* scope,
                                          Node* control_dependency)
    : builder_(builder),
      parameters_count_(scope->num_parameters() + 1),
      locals_count_(scope->num_stack_slots()),
      liveness_block_(IsLivenessAnalysisEnabled()
                          ? builder_->liveness_analyzer()->NewBlock()
                          : nullptr),
      values_(builder_->local_zone()),
      contexts_(builder_->local_zone()),
      control_dependency_(control_dependency),
      effect_dependency_(control_dependency),
      parameters_node_(nullptr),
      locals_node_(nullptr),
      stack_node_(nullptr) {
  DCHECK_EQ(scope->num_parameters() + 1, parameters_count());

  // Bind the receiver variable.
  int param_num = 0;
  if (builder->info()->is_this_defined()) {
    const Operator* op = common()->Parameter(param_num++, "%this");
    Node* receiver = builder->graph()->NewNode(op, builder->graph()->start());
    values()->push_back(receiver);
  } else {
    values()->push_back(builder->jsgraph()->UndefinedConstant());
  }

  // Bind all parameter variables. The parameter indices are shifted by 1
  // (receiver is variable index -1 but {Parameter} node index 0 and located at
  // index 0 in the environment).
  for (int i = 0; i < scope->num_parameters(); ++i) {
    const char* debug_name = GetDebugParameterName(graph()->zone(), scope, i);
    const Operator* op = common()->Parameter(param_num++, debug_name);
    Node* parameter = builder->graph()->NewNode(op, builder->graph()->start());
    values()->push_back(parameter);
  }

  // Bind all local variables to undefined.
  Node* undefined_constant = builder->jsgraph()->UndefinedConstant();
  values()->insert(values()->end(), locals_count(), undefined_constant);
}


AstGraphBuilder::Environment::Environment(AstGraphBuilder::Environment* copy,
                                          LivenessAnalyzerBlock* liveness_block)
    : builder_(copy->builder_),
      parameters_count_(copy->parameters_count_),
      locals_count_(copy->locals_count_),
      liveness_block_(liveness_block),
      values_(copy->zone()),
      contexts_(copy->zone()),
      control_dependency_(copy->control_dependency_),
      effect_dependency_(copy->effect_dependency_),
      parameters_node_(copy->parameters_node_),
      locals_node_(copy->locals_node_),
      stack_node_(copy->stack_node_) {
  const size_t kStackEstimate = 7;  // optimum from experimentation!
  values_.reserve(copy->values_.size() + kStackEstimate);
  values_.insert(values_.begin(), copy->values_.begin(), copy->values_.end());
  contexts_.reserve(copy->contexts_.size());
  contexts_.insert(contexts_.begin(), copy->contexts_.begin(),
                   copy->contexts_.end());
}


void AstGraphBuilder::Environment::Bind(Variable* variable, Node* node) {
  DCHECK(variable->IsStackAllocated());
  if (variable->IsParameter()) {
    // The parameter indices are shifted by 1 (receiver is variable
    // index -1 but located at index 0 in the environment).
    values()->at(variable->index() + 1) = node;
  } else {
    DCHECK(variable->IsStackLocal());
    values()->at(variable->index() + parameters_count_) = node;
    DCHECK(IsLivenessBlockConsistent());
    if (liveness_block() != nullptr) {
      liveness_block()->Bind(variable->index());
    }
  }
}


Node* AstGraphBuilder::Environment::Lookup(Variable* variable) {
  DCHECK(variable->IsStackAllocated());
  if (variable->IsParameter()) {
    // The parameter indices are shifted by 1 (receiver is variable
    // index -1 but located at index 0 in the environment).
    return values()->at(variable->index() + 1);
  } else {
    DCHECK(variable->IsStackLocal());
    DCHECK(IsLivenessBlockConsistent());
    if (liveness_block() != nullptr) {
      liveness_block()->Lookup(variable->index());
    }
    return values()->at(variable->index() + parameters_count_);
  }
}


void AstGraphBuilder::Environment::MarkAllLocalsLive() {
  DCHECK(IsLivenessBlockConsistent());
  if (liveness_block() != nullptr) {
    for (int i = 0; i < locals_count_; i++) {
      liveness_block()->Lookup(i);
    }
  }
}


void AstGraphBuilder::Environment::RawParameterBind(int index, Node* node) {
  DCHECK_LT(index, parameters_count());
  values()->at(index) = node;
}


Node* AstGraphBuilder::Environment::RawParameterLookup(int index) {
  DCHECK_LT(index, parameters_count());
  return values()->at(index);
}


AstGraphBuilder::Environment*
AstGraphBuilder::Environment::CopyForConditional() {
  LivenessAnalyzerBlock* copy_liveness_block = nullptr;
  if (liveness_block() != nullptr) {
    copy_liveness_block =
        builder_->liveness_analyzer()->NewBlock(liveness_block());
    liveness_block_ = builder_->liveness_analyzer()->NewBlock(liveness_block());
  }
  return new (zone()) Environment(this, copy_liveness_block);
}


AstGraphBuilder::Environment*
AstGraphBuilder::Environment::CopyAsUnreachable() {
  Environment* env = new (zone()) Environment(this, nullptr);
  env->MarkAsUnreachable();
  return env;
}

AstGraphBuilder::Environment* AstGraphBuilder::Environment::CopyForOsrEntry() {
  LivenessAnalyzerBlock* copy_block =
      liveness_block() == nullptr ? nullptr
                                  : builder_->liveness_analyzer()->NewBlock();
  return new (zone()) Environment(this, copy_block);
}

AstGraphBuilder::Environment*
AstGraphBuilder::Environment::CopyAndShareLiveness() {
  if (liveness_block() != nullptr) {
    // Finish the current liveness block before copying.
    liveness_block_ = builder_->liveness_analyzer()->NewBlock(liveness_block());
  }
  Environment* env = new (zone()) Environment(this, liveness_block());
  return env;
}


AstGraphBuilder::Environment* AstGraphBuilder::Environment::CopyForLoop(
    BitVector* assigned, bool is_osr) {
  PrepareForLoop(assigned);
  Environment* loop = CopyAndShareLiveness();
  if (is_osr) {
    // Create and merge the OSR entry if necessary.
    Environment* osr_env = CopyForOsrEntry();
    osr_env->PrepareForOsrEntry();
    loop->Merge(osr_env);
  }
  return loop;
}


void AstGraphBuilder::Environment::UpdateStateValues(Node** state_values,
                                                     int offset, int count) {
  bool should_update = false;
  Node** env_values = (count == 0) ? nullptr : &values()->at(offset);
  if (*state_values == nullptr || (*state_values)->InputCount() != count) {
    should_update = true;
  } else {
    DCHECK(static_cast<size_t>(offset + count) <= values()->size());
    for (int i = 0; i < count; i++) {
      if ((*state_values)->InputAt(i) != env_values[i]) {
        should_update = true;
        break;
      }
    }
  }
  if (should_update) {
    const Operator* op = common()->StateValues(count, SparseInputMask::Dense());
    (*state_values) = graph()->NewNode(op, count, env_values);
  }
}


Node* AstGraphBuilder::Environment::Checkpoint(BailoutId ast_id,
                                               OutputFrameStateCombine combine,
                                               bool owner_has_exception) {
  if (!builder()->info()->is_deoptimization_enabled()) {
    return builder()->GetEmptyFrameState();
  }

  UpdateStateValues(&parameters_node_, 0, parameters_count());
  UpdateStateValues(&locals_node_, parameters_count(), locals_count());
  UpdateStateValues(&stack_node_, parameters_count() + locals_count(),
                    stack_height());

  const Operator* op = common()->FrameState(
      ast_id, combine, builder()->frame_state_function_info());

  Node* result = graph()->NewNode(op, parameters_node_, locals_node_,
                                  stack_node_, builder()->current_context(),
                                  builder()->GetFunctionClosure(),
                                  builder()->graph()->start());

  DCHECK(IsLivenessBlockConsistent());
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

void AstGraphBuilder::Environment::PrepareForLoopExit(
    Node* loop, BitVector* assigned_variables) {
  if (IsMarkedAsUnreachable()) return;

  DCHECK_EQ(loop->opcode(), IrOpcode::kLoop);

  Node* control = GetControlDependency();

  // Create the loop exit node.
  Node* loop_exit = graph()->NewNode(common()->LoopExit(), control, loop);
  UpdateControlDependency(loop_exit);

  // Rename the environmnent values.
  for (size_t i = 0; i < values()->size(); i++) {
    if (assigned_variables == nullptr ||
        static_cast<int>(i) >= assigned_variables->length() ||
        assigned_variables->Contains(static_cast<int>(i))) {
      Node* rename = graph()->NewNode(common()->LoopExitValue(), (*values())[i],
                                      loop_exit);
      (*values())[i] = rename;
    }
  }

  // Rename the effect.
  Node* effect_rename = graph()->NewNode(common()->LoopExitEffect(),
                                         GetEffectDependency(), loop_exit);
  UpdateEffectDependency(effect_rename);
}

bool AstGraphBuilder::Environment::IsLivenessAnalysisEnabled() {
  return FLAG_analyze_environment_liveness &&
         builder()->info()->is_deoptimization_enabled();
}


bool AstGraphBuilder::Environment::IsLivenessBlockConsistent() {
  return (!IsLivenessAnalysisEnabled() || IsMarkedAsUnreachable()) ==
         (liveness_block() == nullptr);
}


AstGraphBuilder::AstContext::AstContext(AstGraphBuilder* own,
                                        Expression::Context kind)
    : kind_(kind), owner_(own), outer_(own->ast_context()) {
  owner()->set_ast_context(this);  // Push.
#ifdef DEBUG
  original_height_ = environment()->stack_height();
#endif
}


AstGraphBuilder::AstContext::~AstContext() {
  owner()->set_ast_context(outer_);  // Pop.
}


AstGraphBuilder::AstEffectContext::~AstEffectContext() {
  DCHECK(environment()->stack_height() == original_height_);
}


AstGraphBuilder::AstValueContext::~AstValueContext() {
  DCHECK(environment()->stack_height() == original_height_ + 1);
}


AstGraphBuilder::AstTestContext::~AstTestContext() {
  DCHECK(environment()->stack_height() == original_height_ + 1);
}

void AstGraphBuilder::AstEffectContext::ProduceValue(Expression* expr,
                                                     Node* value) {
  // The value is ignored.
  owner()->PrepareEagerCheckpoint(expr->id());
}

void AstGraphBuilder::AstValueContext::ProduceValue(Expression* expr,
                                                    Node* value) {
  environment()->Push(value);
  owner()->PrepareEagerCheckpoint(expr->id());
}

void AstGraphBuilder::AstTestContext::ProduceValue(Expression* expr,
                                                   Node* value) {
  environment()->Push(owner()->BuildToBoolean(value, feedback_id_));
  owner()->PrepareEagerCheckpoint(expr->id());
}


Node* AstGraphBuilder::AstEffectContext::ConsumeValue() { return nullptr; }


Node* AstGraphBuilder::AstValueContext::ConsumeValue() {
  return environment()->Pop();
}


Node* AstGraphBuilder::AstTestContext::ConsumeValue() {
  return environment()->Pop();
}


Scope* AstGraphBuilder::current_scope() const {
  return execution_context_->scope();
}


Node* AstGraphBuilder::current_context() const {
  return environment()->Context();
}


void AstGraphBuilder::ControlScope::PerformCommand(Command command,
                                                   Statement* target,
                                                   Node* value) {
  Environment* env = environment()->CopyAsUnreachable();
  ControlScope* current = this;
  while (current != nullptr) {
    environment()->TrimStack(current->stack_height());
    environment()->TrimContextChain(current->context_length());
    if (current->Execute(command, target, &value)) break;
    current = current->outer_;
  }
  builder()->set_environment(env);
  DCHECK_NOT_NULL(current);  // Always handled (unless stack is malformed).
}


void AstGraphBuilder::ControlScope::BreakTo(BreakableStatement* stmt) {
  PerformCommand(CMD_BREAK, stmt, builder()->jsgraph()->TheHoleConstant());
}


void AstGraphBuilder::ControlScope::ContinueTo(BreakableStatement* stmt) {
  PerformCommand(CMD_CONTINUE, stmt, builder()->jsgraph()->TheHoleConstant());
}


void AstGraphBuilder::ControlScope::ReturnValue(Node* return_value) {
  PerformCommand(CMD_RETURN, nullptr, return_value);
}


void AstGraphBuilder::ControlScope::ThrowValue(Node* exception_value) {
  PerformCommand(CMD_THROW, nullptr, exception_value);
}


void AstGraphBuilder::VisitForValueOrNull(Expression* expr) {
  if (expr == nullptr) {
    return environment()->Push(jsgraph()->NullConstant());
  }
  VisitForValue(expr);
}


void AstGraphBuilder::VisitForValueOrTheHole(Expression* expr) {
  if (expr == nullptr) {
    return environment()->Push(jsgraph()->TheHoleConstant());
  }
  VisitForValue(expr);
}


void AstGraphBuilder::VisitForValues(ZoneList<Expression*>* exprs) {
  for (int i = 0; i < exprs->length(); ++i) {
    VisitForValue(exprs->at(i));
  }
}


void AstGraphBuilder::VisitForValue(Expression* expr) {
  AstValueContext for_value(this);
  if (!CheckStackOverflow()) {
    VisitNoStackOverflowCheck(expr);
  } else {
    ast_context()->ProduceValue(expr, jsgraph()->UndefinedConstant());
  }
}


void AstGraphBuilder::VisitForEffect(Expression* expr) {
  AstEffectContext for_effect(this);
  if (!CheckStackOverflow()) {
    VisitNoStackOverflowCheck(expr);
  } else {
    ast_context()->ProduceValue(expr, jsgraph()->UndefinedConstant());
  }
}


void AstGraphBuilder::VisitForTest(Expression* expr) {
  AstTestContext for_condition(this, expr->test_id());
  if (!CheckStackOverflow()) {
    VisitNoStackOverflowCheck(expr);
  } else {
    ast_context()->ProduceValue(expr, jsgraph()->UndefinedConstant());
  }
}


void AstGraphBuilder::Visit(Expression* expr) {
  // Reuses enclosing AstContext.
  if (!CheckStackOverflow()) {
    VisitNoStackOverflowCheck(expr);
  } else {
    ast_context()->ProduceValue(expr, jsgraph()->UndefinedConstant());
  }
}


void AstGraphBuilder::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      DCHECK(!variable->binding_needs_init());
      globals()->push_back(variable->name());
      FeedbackSlot slot = decl->proxy()->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals()->push_back(handle(Smi::FromInt(slot.ToInt()), isolate()));
      globals()->push_back(isolate()->factory()->undefined_value());
      globals()->push_back(isolate()->factory()->undefined_value());
      break;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      if (variable->binding_needs_init()) {
        Node* value = jsgraph()->TheHoleConstant();
        environment()->Bind(variable, value);
      }
      break;
    case VariableLocation::CONTEXT:
      if (variable->binding_needs_init()) {
        Node* value = jsgraph()->TheHoleConstant();
        const Operator* op = javascript()->StoreContext(0, variable->index());
        NewNode(op, value);
      }
      break;
    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void AstGraphBuilder::VisitFunctionDeclaration(FunctionDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      Handle<SharedFunctionInfo> function = Compiler::GetSharedFunctionInfo(
          decl->fun(), info()->script(), info());
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals()->push_back(variable->name());
      FeedbackSlot slot = decl->proxy()->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals()->push_back(handle(Smi::FromInt(slot.ToInt()), isolate()));

      // We need the slot where the literals array lives, too.
      slot = decl->fun()->LiteralFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals()->push_back(handle(Smi::FromInt(slot.ToInt()), isolate()));

      globals()->push_back(function);
      break;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      VisitForValue(decl->fun());
      Node* value = environment()->Pop();
      environment()->Bind(variable, value);
      break;
    }
    case VariableLocation::CONTEXT: {
      VisitForValue(decl->fun());
      Node* value = environment()->Pop();
      const Operator* op = javascript()->StoreContext(0, variable->index());
      NewNode(op, value);
      break;
    }
    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void AstGraphBuilder::VisitBlock(Block* stmt) {
  BlockBuilder block(this);
  ControlScopeForBreakable scope(this, stmt, &block);
  if (stmt->labels() != nullptr) block.BeginBlock();
  if (stmt->scope() == nullptr) {
    // Visit statements in the same scope, no declarations.
    VisitStatements(stmt->statements());
  } else {
    // Visit declarations and statements in a block scope.
    if (stmt->scope()->NeedsContext()) {
      Node* context = BuildLocalBlockContext(stmt->scope());
      ContextScope scope(this, stmt->scope(), context);
      VisitDeclarations(stmt->scope()->declarations());
      VisitStatements(stmt->statements());
    } else {
      VisitDeclarations(stmt->scope()->declarations());
      VisitStatements(stmt->statements());
    }
  }
  if (stmt->labels() != nullptr) block.EndBlock();
}


void AstGraphBuilder::VisitExpressionStatement(ExpressionStatement* stmt) {
  VisitForEffect(stmt->expression());
}


void AstGraphBuilder::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
}


void AstGraphBuilder::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* stmt) {
  Visit(stmt->statement());
}


void AstGraphBuilder::VisitIfStatement(IfStatement* stmt) {
  IfBuilder compare_if(this);
  VisitForTest(stmt->condition());
  Node* condition = environment()->Pop();
  compare_if.If(condition);
  compare_if.Then();
  Visit(stmt->then_statement());
  compare_if.Else();
  Visit(stmt->else_statement());
  compare_if.End();
}


void AstGraphBuilder::VisitContinueStatement(ContinueStatement* stmt) {
  execution_control()->ContinueTo(stmt->target());
}


void AstGraphBuilder::VisitBreakStatement(BreakStatement* stmt) {
  execution_control()->BreakTo(stmt->target());
}


void AstGraphBuilder::VisitReturnStatement(ReturnStatement* stmt) {
  VisitForValue(stmt->expression());
  Node* result = environment()->Pop();
  execution_control()->ReturnValue(result);
}


void AstGraphBuilder::VisitWithStatement(WithStatement* stmt) {
  // Dynamic scoping is supported only by going through Ignition first.
  UNREACHABLE();
}


void AstGraphBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  ZoneList<CaseClause*>* clauses = stmt->cases();
  SwitchBuilder compare_switch(this, clauses->length());
  ControlScopeForBreakable scope(this, stmt, &compare_switch);
  compare_switch.BeginSwitch();
  int default_index = -1;

  // Keep the switch value on the stack until a case matches.
  VisitForValue(stmt->tag());

  // Iterate over all cases and create nodes for label comparison.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);

    // The default is not a test, remember index.
    if (clause->is_default()) {
      default_index = i;
      continue;
    }

    // Create nodes to perform label comparison as if via '==='. The switch
    // value is still on the operand stack while the label is evaluated.
    VisitForValue(clause->label());
    Node* label = environment()->Pop();
    Node* tag = environment()->Top();

    CompareOperationHint hint = CompareOperationHint::kAny;
    const Operator* op = javascript()->StrictEqual(hint);
    Node* condition = NewNode(op, tag, label);
    compare_switch.BeginLabel(i, condition);

    // Discard the switch value at label match.
    environment()->Pop();
    compare_switch.EndLabel();
  }

  // Discard the switch value and mark the default case.
  environment()->Pop();
  if (default_index >= 0) {
    compare_switch.DefaultAt(default_index);
  }

  // Iterate over all cases and create nodes for case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);
    compare_switch.BeginCase(i);
    VisitStatements(clause->statements());
    compare_switch.EndCase();
  }

  compare_switch.EndSwitch();
}


void AstGraphBuilder::VisitDoWhileStatement(DoWhileStatement* stmt) {
  LoopBuilder while_loop(this);
  while_loop.BeginLoop(GetVariablesAssignedInLoop(stmt), CheckOsrEntry(stmt));
  VisitIterationBody(stmt, &while_loop, stmt->StackCheckId());
  while_loop.EndBody();
  VisitForTest(stmt->cond());
  Node* condition = environment()->Pop();
  while_loop.BreakUnless(condition);
  while_loop.EndLoop();
}


void AstGraphBuilder::VisitWhileStatement(WhileStatement* stmt) {
  LoopBuilder while_loop(this);
  while_loop.BeginLoop(GetVariablesAssignedInLoop(stmt), CheckOsrEntry(stmt));
  VisitForTest(stmt->cond());
  Node* condition = environment()->Pop();
  while_loop.BreakUnless(condition);
  VisitIterationBody(stmt, &while_loop, stmt->StackCheckId());
  while_loop.EndBody();
  while_loop.EndLoop();
}


void AstGraphBuilder::VisitForStatement(ForStatement* stmt) {
  LoopBuilder for_loop(this);
  VisitIfNotNull(stmt->init());
  for_loop.BeginLoop(GetVariablesAssignedInLoop(stmt), CheckOsrEntry(stmt));
  if (stmt->cond() != nullptr) {
    VisitForTest(stmt->cond());
    Node* condition = environment()->Pop();
    for_loop.BreakUnless(condition);
  } else {
    for_loop.BreakUnless(jsgraph()->TrueConstant());
  }
  VisitIterationBody(stmt, &for_loop, stmt->StackCheckId());
  for_loop.EndBody();
  VisitIfNotNull(stmt->next());
  for_loop.EndLoop();
}


void AstGraphBuilder::VisitForInStatement(ForInStatement* stmt) {
  // Only the BytecodeGraphBuilder supports for-in.
  return SetStackOverflow();
}


void AstGraphBuilder::VisitForOfStatement(ForOfStatement* stmt) {
  // Iterator looping is supported only by going through Ignition first.
  UNREACHABLE();
}


void AstGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  // Exception handling is supported only by going through Ignition first.
  UNREACHABLE();
}


void AstGraphBuilder::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  // Exception handling is supported only by going through Ignition first.
  UNREACHABLE();
}


void AstGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  // Debugger statement is supported only by going through Ignition first.
  UNREACHABLE();
}


void AstGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  // Find or build a shared function info.
  Handle<SharedFunctionInfo> shared_info =
      Compiler::GetSharedFunctionInfo(expr, info()->script(), info());
  CHECK(!shared_info.is_null());  // TODO(mstarzinger): Set stack overflow?

  // Create node to instantiate a new closure.
  PretenureFlag pretenure = expr->pretenure() ? TENURED : NOT_TENURED;
  VectorSlotPair pair = CreateVectorSlotPair(expr->LiteralFeedbackSlot());
  const Operator* op =
      javascript()->CreateClosure(shared_info, pair, pretenure);
  Node* value = NewNode(op);
  ast_context()->ProduceValue(expr, value);
}

void AstGraphBuilder::VisitClassLiteral(ClassLiteral* expr) { UNREACHABLE(); }

void AstGraphBuilder::VisitNativeFunctionLiteral(NativeFunctionLiteral* expr) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitDoExpression(DoExpression* expr) {
  VisitBlock(expr->block());
  VisitVariableProxy(expr->result());
  ast_context()->ReplaceValue(expr);
}


void AstGraphBuilder::VisitConditional(Conditional* expr) {
  IfBuilder compare_if(this);
  VisitForTest(expr->condition());
  Node* condition = environment()->Pop();
  compare_if.If(condition);
  compare_if.Then();
  Visit(expr->then_expression());
  compare_if.Else();
  Visit(expr->else_expression());
  compare_if.End();
  // Skip plugging AST evaluation contexts of the test kind. This is to stay in
  // sync with full codegen which doesn't prepare the proper bailout point (see
  // the implementation of FullCodeGenerator::VisitForControl).
  if (ast_context()->IsTest()) return;
  ast_context()->ReplaceValue(expr);
}


void AstGraphBuilder::VisitVariableProxy(VariableProxy* expr) {
  VectorSlotPair pair = CreateVectorSlotPair(expr->VariableFeedbackSlot());
  PrepareEagerCheckpoint(BeforeId(expr));
  Node* value = BuildVariableLoad(expr->var(), expr->id(), pair,
                                  ast_context()->GetStateCombine());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitLiteral(Literal* expr) {
  Node* value = jsgraph()->Constant(expr->value());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to materialize a regular expression literal.
  const Operator* op = javascript()->CreateLiteralRegExp(
      expr->pattern(), expr->flags(),
      FeedbackVector::GetIndex(expr->literal_slot()));
  Node* literal = NewNode(op, closure);
  PrepareFrameState(literal, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(expr, literal);
}


void AstGraphBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to deep-copy the literal boilerplate.
  const Operator* op = javascript()->CreateLiteralObject(
      expr->GetOrBuildConstantProperties(isolate()), expr->ComputeFlags(true),
      FeedbackVector::GetIndex(expr->literal_slot()), expr->properties_count());
  Node* literal = NewNode(op, closure);
  PrepareFrameState(literal, expr->CreateLiteralId(),
                    OutputFrameStateCombine::Push());

  // The object is expected on the operand stack during computation of the
  // property values and is the value of the entire expression.
  environment()->Push(literal);

  // Create nodes to store computed values into the literal.
  AccessorTable accessor_table(local_zone());
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    DCHECK(!property->is_computed_name());
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key()->AsLiteral();
    switch (property->kind()) {
      case ObjectLiteral::Property::SPREAD:
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(property->value()));
      // Fall through.
      case ObjectLiteral::Property::COMPUTED: {
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->IsStringLiteral()) {
          DCHECK(key->IsPropertyName());
          if (property->emit_store()) {
            VisitForValue(property->value());
            Node* value = environment()->Pop();
            Node* literal = environment()->Top();
            Handle<Name> name = key->AsPropertyName();
            VectorSlotPair feedback =
                CreateVectorSlotPair(property->GetSlot(0));
            Node* store = BuildNamedStoreOwn(literal, name, value, feedback);
            PrepareFrameState(store, key->id(),
                              OutputFrameStateCombine::Ignore());
            BuildSetHomeObject(value, literal, property, 1);
          } else {
            VisitForEffect(property->value());
          }
          break;
        }
        environment()->Push(environment()->Top());  // Duplicate receiver.
        VisitForValue(property->key());
        VisitForValue(property->value());
        Node* value = environment()->Pop();
        Node* key = environment()->Pop();
        Node* receiver = environment()->Pop();
        if (property->emit_store()) {
          Node* language = jsgraph()->Constant(SLOPPY);
          const Operator* op = javascript()->CallRuntime(Runtime::kSetProperty);
          Node* set_property = NewNode(op, receiver, key, value, language);
          // SetProperty should not lazy deopt on an object literal.
          PrepareFrameState(set_property, BailoutId::None());
          BuildSetHomeObject(value, receiver, property);
        }
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE: {
        environment()->Push(environment()->Top());  // Duplicate receiver.
        VisitForValue(property->value());
        Node* value = environment()->Pop();
        Node* receiver = environment()->Pop();
        DCHECK(property->emit_store());
        const Operator* op =
            javascript()->CallRuntime(Runtime::kInternalSetPrototype);
        Node* set_prototype = NewNode(op, receiver, value);
        // SetPrototype should not lazy deopt on an object literal.
        PrepareFrameState(set_prototype, expr->GetIdForPropertySet(i));
        break;
      }
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(i);
          it->second->getter = property;
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(i);
          it->second->setter = property;
        }
        break;
    }
  }

  // Create nodes to define accessors, using only a single call to the runtime
  // for each pair of corresponding getters and setters.
  literal = environment()->Top();  // Reload from operand stack.
  for (AccessorTable::Iterator it = accessor_table.begin();
       it != accessor_table.end(); ++it) {
    VisitForValue(it->first);
    VisitObjectLiteralAccessor(literal, it->second->getter);
    VisitObjectLiteralAccessor(literal, it->second->setter);
    Node* setter = environment()->Pop();
    Node* getter = environment()->Pop();
    Node* name = environment()->Pop();
    Node* attr = jsgraph()->Constant(NONE);
    const Operator* op =
        javascript()->CallRuntime(Runtime::kDefineAccessorPropertyUnchecked);
    Node* call = NewNode(op, literal, name, getter, setter, attr);
    PrepareFrameState(call, it->second->bailout_id);
  }
  ast_context()->ProduceValue(expr, environment()->Pop());
}


void AstGraphBuilder::VisitObjectLiteralAccessor(
    Node* home_object, ObjectLiteralProperty* property) {
  if (property == nullptr) {
    VisitForValueOrNull(nullptr);
  } else {
    VisitForValue(property->value());
    BuildSetHomeObject(environment()->Top(), home_object, property);
  }
}


void AstGraphBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to deep-copy the literal boilerplate.
  const Operator* op = javascript()->CreateLiteralArray(
      expr->GetOrBuildConstantElements(isolate()), expr->ComputeFlags(true),
      FeedbackVector::GetIndex(expr->literal_slot()), expr->values()->length());
  Node* literal = NewNode(op, closure);
  PrepareFrameState(literal, expr->CreateLiteralId(),
                    OutputFrameStateCombine::Push());

  // The array is expected on the operand stack during computation of the
  // element values.
  environment()->Push(literal);

  // Create nodes to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  for (int array_index = 0; array_index < expr->values()->length();
       array_index++) {
    Expression* subexpr = expr->values()->at(array_index);
    DCHECK(!subexpr->IsSpread());
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    VisitForValue(subexpr);
    VectorSlotPair pair = CreateVectorSlotPair(expr->LiteralFeedbackSlot());
    Node* value = environment()->Pop();
    Node* index = jsgraph()->Constant(array_index);
    Node* literal = environment()->Top();
    Node* store = BuildKeyedStore(literal, index, value, pair);
    PrepareFrameState(store, expr->GetIdForElement(array_index),
                      OutputFrameStateCombine::Ignore());
  }

  ast_context()->ProduceValue(expr, environment()->Pop());
}

void AstGraphBuilder::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpressionOrThis());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);
  bool needs_frame_state_before = true;

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->target()->AsVariableProxy()->var();
      if (variable->location() == VariableLocation::PARAMETER ||
          variable->location() == VariableLocation::LOCAL ||
          variable->location() == VariableLocation::CONTEXT) {
        needs_frame_state_before = false;
      }
      break;
    }
    case NAMED_PROPERTY:
      VisitForValue(property->obj());
      break;
    case KEYED_PROPERTY:
      VisitForValue(property->obj());
      VisitForValue(property->key());
      break;
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }

  // Evaluate the value and potentially handle compound assignments by loading
  // the left-hand side value and performing a binary operation.
  if (expr->is_compound()) {
    Node* old_value = nullptr;
    switch (assign_type) {
      case VARIABLE: {
        VariableProxy* proxy = expr->target()->AsVariableProxy();
        VectorSlotPair pair =
            CreateVectorSlotPair(proxy->VariableFeedbackSlot());
        PrepareEagerCheckpoint(BeforeId(proxy));
        old_value = BuildVariableLoad(proxy->var(), expr->target()->id(), pair,
                                      OutputFrameStateCombine::Push());
        break;
      }
      case NAMED_PROPERTY: {
        Node* object = environment()->Top();
        Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
        VectorSlotPair pair =
            CreateVectorSlotPair(property->PropertyFeedbackSlot());
        old_value = BuildNamedLoad(object, name, pair);
        PrepareFrameState(old_value, property->LoadId(),
                          OutputFrameStateCombine::Push());
        break;
      }
      case KEYED_PROPERTY: {
        Node* key = environment()->Top();
        Node* object = environment()->Peek(1);
        VectorSlotPair pair =
            CreateVectorSlotPair(property->PropertyFeedbackSlot());
        old_value = BuildKeyedLoad(object, key, pair);
        PrepareFrameState(old_value, property->LoadId(),
                          OutputFrameStateCombine::Push());
        break;
      }
      case NAMED_SUPER_PROPERTY:
      case KEYED_SUPER_PROPERTY:
        UNREACHABLE();
        break;
    }
    environment()->Push(old_value);
    VisitForValue(expr->value());
    Node* right = environment()->Pop();
    Node* left = environment()->Pop();
    Node* value =
        BuildBinaryOp(left, right, expr->binary_op(),
                      expr->binary_operation()->BinaryOperationFeedbackId());
    PrepareFrameState(value, expr->binary_operation()->id(),
                      OutputFrameStateCombine::Push());
    environment()->Push(value);
    if (needs_frame_state_before) {
      PrepareEagerCheckpoint(expr->binary_operation()->id());
    }
  } else {
    VisitForValue(expr->value());
  }

  // Store the value.
  Node* value = environment()->Pop();
  VectorSlotPair feedback = CreateVectorSlotPair(expr->AssignmentSlot());
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->target()->AsVariableProxy()->var();
      BuildVariableAssignment(variable, value, expr->op(), feedback, expr->id(),
                              ast_context()->GetStateCombine());
      break;
    }
    case NAMED_PROPERTY: {
      Node* object = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store = BuildNamedStore(object, name, value, feedback);
      PrepareFrameState(store, expr->AssignmentId(),
                        OutputFrameStateCombine::Push());
      break;
    }
    case KEYED_PROPERTY: {
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      Node* store = BuildKeyedStore(object, key, value, feedback);
      PrepareFrameState(store, expr->AssignmentId(),
                        OutputFrameStateCombine::Push());
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }

  ast_context()->ProduceValue(expr, value);
}

void AstGraphBuilder::VisitSuspend(Suspend* expr) {
  // Generator functions are supported only by going through Ignition first.
  UNREACHABLE();
}


void AstGraphBuilder::VisitThrow(Throw* expr) {
  VisitForValue(expr->exception());
  Node* exception = environment()->Pop();
  Node* value = BuildThrowError(exception, expr->id());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitProperty(Property* expr) {
  Node* value = nullptr;
  LhsKind property_kind = Property::GetAssignType(expr);
  VectorSlotPair pair = CreateVectorSlotPair(expr->PropertyFeedbackSlot());
  switch (property_kind) {
    case VARIABLE:
      UNREACHABLE();
      break;
    case NAMED_PROPERTY: {
      VisitForValue(expr->obj());
      Node* object = environment()->Pop();
      Handle<Name> name = expr->key()->AsLiteral()->AsPropertyName();
      value = BuildNamedLoad(object, name, pair);
      PrepareFrameState(value, expr->LoadId(), OutputFrameStateCombine::Push());
      break;
    }
    case KEYED_PROPERTY: {
      VisitForValue(expr->obj());
      VisitForValue(expr->key());
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      value = BuildKeyedLoad(object, key, pair);
      PrepareFrameState(value, expr->LoadId(), OutputFrameStateCombine::Push());
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitCall(Call* expr) {
  Expression* callee = expr->expression();
  Call::CallType call_type = expr->GetCallType();
  CHECK(!expr->is_possibly_eval());

  // Prepare the callee and the receiver to the function call. This depends on
  // the semantics of the underlying call type.
  ConvertReceiverMode receiver_hint = ConvertReceiverMode::kAny;
  Node* receiver_value = nullptr;
  Node* callee_value = nullptr;
  switch (call_type) {
    case Call::GLOBAL_CALL: {
      VariableProxy* proxy = callee->AsVariableProxy();
      VectorSlotPair pair = CreateVectorSlotPair(proxy->VariableFeedbackSlot());
      PrepareEagerCheckpoint(BeforeId(proxy));
      callee_value = BuildVariableLoad(proxy->var(), expr->expression()->id(),
                                       pair, OutputFrameStateCombine::Push());
      receiver_hint = ConvertReceiverMode::kNullOrUndefined;
      receiver_value = jsgraph()->UndefinedConstant();
      break;
    }
    case Call::NAMED_PROPERTY_CALL: {
      Property* property = callee->AsProperty();
      VectorSlotPair feedback =
          CreateVectorSlotPair(property->PropertyFeedbackSlot());
      VisitForValue(property->obj());
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* object = environment()->Top();
      callee_value = BuildNamedLoad(object, name, feedback);
      PrepareFrameState(callee_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      // Note that a property call requires the receiver to be wrapped into
      // an object for sloppy callees. However the receiver is guaranteed
      // not to be null or undefined at this point.
      receiver_hint = ConvertReceiverMode::kNotNullOrUndefined;
      receiver_value = environment()->Pop();
      break;
    }
    case Call::KEYED_PROPERTY_CALL: {
      Property* property = callee->AsProperty();
      VectorSlotPair feedback =
          CreateVectorSlotPair(property->PropertyFeedbackSlot());
      VisitForValue(property->obj());
      VisitForValue(property->key());
      Node* key = environment()->Pop();
      Node* object = environment()->Top();
      callee_value = BuildKeyedLoad(object, key, feedback);
      PrepareFrameState(callee_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      // Note that a property call requires the receiver to be wrapped into
      // an object for sloppy callees. However the receiver is guaranteed
      // not to be null or undefined at this point.
      receiver_hint = ConvertReceiverMode::kNotNullOrUndefined;
      receiver_value = environment()->Pop();
      break;
    }
    case Call::OTHER_CALL:
      VisitForValue(callee);
      callee_value = environment()->Pop();
      receiver_hint = ConvertReceiverMode::kNullOrUndefined;
      receiver_value = jsgraph()->UndefinedConstant();
      break;
    case Call::NAMED_SUPER_PROPERTY_CALL:
    case Call::KEYED_SUPER_PROPERTY_CALL:
    case Call::SUPER_CALL:
    case Call::WITH_CALL:
      UNREACHABLE();
  }

  // The callee and the receiver both have to be pushed onto the operand stack
  // before arguments are being evaluated.
  environment()->Push(callee_value);
  environment()->Push(receiver_value);

  // Evaluate all arguments to the function call,
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the function call.
  CallFrequency frequency = ComputeCallFrequency(expr->CallFeedbackICSlot());
  VectorSlotPair feedback = CreateVectorSlotPair(expr->CallFeedbackICSlot());
  const Operator* call =
      javascript()->Call(args->length() + 2, frequency, feedback, receiver_hint,
                         expr->tail_call_mode());
  PrepareEagerCheckpoint(expr->CallId());
  Node* value = ProcessArguments(call, args->length() + 2);
  // The callee passed to the call, we just need to push something here to
  // satisfy the bailout location contract. The fullcodegen code will not
  // ever look at this value, so we just push optimized_out here.
  environment()->Push(jsgraph()->OptimizedOutConstant());
  PrepareFrameState(value, expr->ReturnId(), OutputFrameStateCombine::Push());
  environment()->Drop(1);
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitCallNew(CallNew* expr) {
  VisitForValue(expr->expression());

  // Evaluate all arguments to the construct call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // The new target is the same as the callee.
  environment()->Push(environment()->Peek(args->length()));

  // Create node to perform the construct call.
  CallFrequency frequency = ComputeCallFrequency(expr->CallNewFeedbackSlot());
  VectorSlotPair feedback = CreateVectorSlotPair(expr->CallNewFeedbackSlot());
  const Operator* call =
      javascript()->Construct(args->length() + 2, frequency, feedback);
  Node* value = ProcessArguments(call, args->length() + 2);
  PrepareFrameState(value, expr->ReturnId(), OutputFrameStateCombine::Push());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitCallJSRuntime(CallRuntime* expr) {
  // The callee and the receiver both have to be pushed onto the operand stack
  // before arguments are being evaluated.
  Node* callee_value = BuildLoadNativeContextField(expr->context_index());
  Node* receiver_value = jsgraph()->UndefinedConstant();

  environment()->Push(callee_value);
  environment()->Push(receiver_value);

  // Evaluate all arguments to the JS runtime call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the JS runtime call.
  const Operator* call = javascript()->Call(args->length() + 2);
  PrepareEagerCheckpoint(expr->CallId());
  Node* value = ProcessArguments(call, args->length() + 2);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitCallRuntime(CallRuntime* expr) {
  // Handle calls to runtime functions implemented in JavaScript separately as
  // the call follows JavaScript ABI and the callee is statically unknown.
  if (expr->is_jsruntime()) {
    return VisitCallJSRuntime(expr);
  }

  // Evaluate all arguments to the runtime call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the runtime call.
  Runtime::FunctionId functionId = expr->function()->function_id;
  const Operator* call = javascript()->CallRuntime(functionId, args->length());
  if (expr->function()->intrinsic_type == Runtime::IntrinsicType::RUNTIME ||
      expr->function()->function_id == Runtime::kInlineCall) {
    PrepareEagerCheckpoint(expr->CallId());
  }
  Node* value = ProcessArguments(call, args->length());
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::DELETE:
      return VisitDelete(expr);
    case Token::VOID:
      return VisitVoid(expr);
    case Token::TYPEOF:
      return VisitTypeof(expr);
    case Token::NOT:
      return VisitNot(expr);
    default:
      UNREACHABLE();
  }
}


void AstGraphBuilder::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpressionOrThis());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->expression()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  // Reserve space for result of postfix operation.
  bool is_postfix = expr->is_postfix() && !ast_context()->IsEffect();
  if (is_postfix && assign_type != VARIABLE) {
    environment()->Push(jsgraph()->ZeroConstant());
  }

  // Evaluate LHS expression and get old value.
  Node* old_value = nullptr;
  int stack_depth = -1;
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      VectorSlotPair pair = CreateVectorSlotPair(proxy->VariableFeedbackSlot());
      PrepareEagerCheckpoint(BeforeId(proxy));
      old_value = BuildVariableLoad(proxy->var(), expr->expression()->id(),
                                    pair, OutputFrameStateCombine::Push());
      stack_depth = 0;
      break;
    }
    case NAMED_PROPERTY: {
      VisitForValue(property->obj());
      Node* object = environment()->Top();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      VectorSlotPair pair =
          CreateVectorSlotPair(property->PropertyFeedbackSlot());
      old_value = BuildNamedLoad(object, name, pair);
      PrepareFrameState(old_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      stack_depth = 1;
      break;
    }
    case KEYED_PROPERTY: {
      VisitForValue(property->obj());
      VisitForValue(property->key());
      Node* key = environment()->Top();
      Node* object = environment()->Peek(1);
      VectorSlotPair pair =
          CreateVectorSlotPair(property->PropertyFeedbackSlot());
      old_value = BuildKeyedLoad(object, key, pair);
      PrepareFrameState(old_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      stack_depth = 2;
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }

  // Convert old value into a number.
  old_value = NewNode(javascript()->ToNumber(), old_value);
  PrepareFrameState(old_value, expr->ToNumberId(),
                    OutputFrameStateCombine::Push());

  // Create a proper eager frame state for the stores.
  environment()->Push(old_value);
  PrepareEagerCheckpoint(expr->ToNumberId());
  old_value = environment()->Pop();

  // Save result for postfix expressions at correct stack depth.
  if (is_postfix) {
    if (assign_type != VARIABLE) {
      environment()->Poke(stack_depth, old_value);
    } else {
      environment()->Push(old_value);
    }
  }

  // Create node to perform +1/-1 operation.
  Node* value = BuildBinaryOp(old_value, jsgraph()->OneConstant(),
                              expr->binary_op(), expr->CountBinOpFeedbackId());
  // This should never lazy deopt because we have converted to number before.
  PrepareFrameState(value, BailoutId::None());

  // Store the value.
  VectorSlotPair feedback = CreateVectorSlotPair(expr->CountSlot());
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->expression()->AsVariableProxy()->var();
      environment()->Push(value);
      BuildVariableAssignment(variable, value, expr->op(), feedback,
                              expr->AssignmentId());
      environment()->Pop();
      break;
    }
    case NAMED_PROPERTY: {
      Node* object = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store = BuildNamedStore(object, name, value, feedback);
      PrepareFrameState(store, expr->AssignmentId(),
                        OutputFrameStateCombine::Push());
      break;
    }
    case KEYED_PROPERTY: {
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      Node* store = BuildKeyedStore(object, key, value, feedback);
      PrepareFrameState(store, expr->AssignmentId(),
                        OutputFrameStateCombine::Push());
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }

  // Restore old value for postfix expressions.
  if (is_postfix) value = environment()->Pop();

  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitBinaryOperation(BinaryOperation* expr) {
  switch (expr->op()) {
    case Token::COMMA:
      return VisitComma(expr);
    case Token::OR:
    case Token::AND:
      return VisitLogicalExpression(expr);
    default: {
      VisitForValue(expr->left());
      VisitForValue(expr->right());
      Node* right = environment()->Pop();
      Node* left = environment()->Pop();
      Node* value = BuildBinaryOp(left, right, expr->op(),
                                  expr->BinaryOperationFeedbackId());
      PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
      ast_context()->ProduceValue(expr, value);
    }
  }
}

void AstGraphBuilder::VisitLiteralCompareNil(CompareOperation* expr,
                                             Expression* sub_expr,
                                             Node* nil_value) {
  const Operator* op = nullptr;
  switch (expr->op()) {
    case Token::EQ:
      op = javascript()->Equal(CompareOperationHint::kAny);
      break;
    case Token::EQ_STRICT:
      op = javascript()->StrictEqual(CompareOperationHint::kAny);
      break;
    default:
      UNREACHABLE();
  }
  VisitForValue(sub_expr);
  Node* value_to_compare = environment()->Pop();
  Node* value = NewNode(op, value_to_compare, nil_value);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  return ast_context()->ProduceValue(expr, value);
}

void AstGraphBuilder::VisitLiteralCompareTypeof(CompareOperation* expr,
                                                Expression* sub_expr,
                                                Handle<String> check) {
  VisitTypeofExpression(sub_expr);
  Node* typeof_arg = NewNode(javascript()->TypeOf(), environment()->Pop());
  Node* value = NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                        typeof_arg, jsgraph()->Constant(check));
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  return ast_context()->ProduceValue(expr, value);
}

void AstGraphBuilder::VisitCompareOperation(CompareOperation* expr) {
  // Check for a few fast cases. The AST visiting behavior must be in sync
  // with the full codegen: We don't push both left and right values onto
  // the expression stack when one side is a special-case literal.
  Expression* sub_expr = nullptr;
  Literal* literal;
  if (expr->IsLiteralCompareTypeof(&sub_expr, &literal)) {
    return VisitLiteralCompareTypeof(expr, sub_expr,
                                     Handle<String>::cast(literal->value()));
  }
  if (expr->IsLiteralCompareUndefined(&sub_expr)) {
    return VisitLiteralCompareNil(expr, sub_expr,
                                  jsgraph()->UndefinedConstant());
  }
  if (expr->IsLiteralCompareNull(&sub_expr)) {
    return VisitLiteralCompareNil(expr, sub_expr, jsgraph()->NullConstant());
  }

  CompareOperationHint hint = CompareOperationHint::kAny;
  const Operator* op;
  switch (expr->op()) {
    case Token::EQ:
      op = javascript()->Equal(hint);
      break;
    case Token::EQ_STRICT:
      op = javascript()->StrictEqual(hint);
      break;
    case Token::LT:
      op = javascript()->LessThan(hint);
      break;
    case Token::GT:
      op = javascript()->GreaterThan(hint);
      break;
    case Token::LTE:
      op = javascript()->LessThanOrEqual(hint);
      break;
    case Token::GTE:
      op = javascript()->GreaterThanOrEqual(hint);
      break;
    case Token::INSTANCEOF:
      op = javascript()->InstanceOf();
      break;
    case Token::IN:
      op = javascript()->HasProperty();
      break;
    default:
      op = nullptr;
      UNREACHABLE();
  }
  VisitForValue(expr->left());
  VisitForValue(expr->right());
  Node* right = environment()->Pop();
  Node* left = environment()->Pop();
  Node* value = NewNode(op, left, right);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitSpread(Spread* expr) {
  // Handled entirely by the parser itself.
  UNREACHABLE();
}


void AstGraphBuilder::VisitEmptyParentheses(EmptyParentheses* expr) {
  // Handled entirely by the parser itself.
  UNREACHABLE();
}

void AstGraphBuilder::VisitGetIterator(GetIterator* expr) {
  // GetIterator is supported only by going through Ignition first.
  UNREACHABLE();
}

void AstGraphBuilder::VisitImportCallExpression(ImportCallExpression* expr) {
  // ImportCallExpression is supported only by going through Ignition first.
  UNREACHABLE();
}

void AstGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  Node* value = GetFunctionClosure();
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitSuperPropertyReference(
    SuperPropertyReference* expr) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitSuperCallReference(SuperCallReference* expr) {
  // Handled by VisitCall
  UNREACHABLE();
}


void AstGraphBuilder::VisitCaseClause(CaseClause* expr) {
  // Handled entirely in VisitSwitch.
  UNREACHABLE();
}

void AstGraphBuilder::VisitDeclarations(Declaration::List* declarations) {
  DCHECK(globals()->empty());
  AstVisitor<AstGraphBuilder>::VisitDeclarations(declarations);
  if (globals()->empty()) return;
  int array_index = 0;
  Handle<FeedbackVector> feedback_vector(info()->closure()->feedback_vector());
  Handle<FixedArray> data = isolate()->factory()->NewFixedArray(
      static_cast<int>(globals()->size()), TENURED);
  for (Handle<Object> obj : *globals()) data->set(array_index++, *obj);
  int encoded_flags = info()->GetDeclareGlobalsFlags();
  Node* flags = jsgraph()->Constant(encoded_flags);
  Node* decls = jsgraph()->Constant(data);
  Node* vector = jsgraph()->Constant(feedback_vector);
  const Operator* op = javascript()->CallRuntime(Runtime::kDeclareGlobals);
  Node* call = NewNode(op, decls, flags, vector);
  PrepareFrameState(call, BailoutId::Declarations());
  globals()->clear();
}


void AstGraphBuilder::VisitIfNotNull(Statement* stmt) {
  if (stmt == nullptr) return;
  Visit(stmt);
}


void AstGraphBuilder::VisitIterationBody(IterationStatement* stmt,
                                         LoopBuilder* loop,
                                         BailoutId stack_check_id) {
  ControlScopeForIteration scope(this, stmt, loop);
  Node* node = NewNode(javascript()->StackCheck());
  PrepareFrameState(node, stack_check_id);
  Visit(stmt->body());
}


void AstGraphBuilder::VisitDelete(UnaryOperation* expr) {
  Node* value;
  if (expr->expression()->IsVariableProxy()) {
    // Delete of an unqualified identifier is disallowed in strict mode but
    // "delete this" is allowed.
    Variable* variable = expr->expression()->AsVariableProxy()->var();
    DCHECK(is_sloppy(language_mode()) || variable->is_this());
    value = BuildVariableDelete(variable, expr->id(),
                                ast_context()->GetStateCombine());
  } else if (expr->expression()->IsProperty()) {
    Property* property = expr->expression()->AsProperty();
    VisitForValue(property->obj());
    VisitForValue(property->key());
    Node* key = environment()->Pop();
    Node* object = environment()->Pop();
    Node* mode = jsgraph()->Constant(static_cast<int32_t>(language_mode()));
    value = NewNode(javascript()->DeleteProperty(), object, key, mode);
    PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  } else {
    VisitForEffect(expr->expression());
    value = jsgraph()->TrueConstant();
  }
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitVoid(UnaryOperation* expr) {
  VisitForEffect(expr->expression());
  Node* value = jsgraph()->UndefinedConstant();
  ast_context()->ProduceValue(expr, value);
}

void AstGraphBuilder::VisitTypeofExpression(Expression* expr) {
  if (expr->IsVariableProxy()) {
    // Typeof does not throw a reference error on global variables, hence we
    // perform a non-contextual load in case the operand is a variable proxy.
    VariableProxy* proxy = expr->AsVariableProxy();
    VectorSlotPair pair = CreateVectorSlotPair(proxy->VariableFeedbackSlot());
    PrepareEagerCheckpoint(BeforeId(proxy));
    Node* load =
        BuildVariableLoad(proxy->var(), expr->id(), pair,
                          OutputFrameStateCombine::Push(), INSIDE_TYPEOF);
    environment()->Push(load);
  } else {
    VisitForValue(expr);
  }
}

void AstGraphBuilder::VisitTypeof(UnaryOperation* expr) {
  VisitTypeofExpression(expr->expression());
  Node* value = NewNode(javascript()->TypeOf(), environment()->Pop());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitNot(UnaryOperation* expr) {
  VisitForTest(expr->expression());
  Node* input = environment()->Pop();
  Node* value = NewNode(common()->Select(MachineRepresentation::kTagged), input,
                        jsgraph()->FalseConstant(), jsgraph()->TrueConstant());
  // Skip plugging AST evaluation contexts of the test kind. This is to stay in
  // sync with full codegen which doesn't prepare the proper bailout point (see
  // the implementation of FullCodeGenerator::VisitForControl).
  if (ast_context()->IsTest()) return environment()->Push(value);
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitComma(BinaryOperation* expr) {
  VisitForEffect(expr->left());
  Visit(expr->right());
  // Skip plugging AST evaluation contexts of the test kind. This is to stay in
  // sync with full codegen which doesn't prepare the proper bailout point (see
  // the implementation of FullCodeGenerator::VisitForControl).
  if (ast_context()->IsTest()) return;
  ast_context()->ReplaceValue(expr);
}


void AstGraphBuilder::VisitLogicalExpression(BinaryOperation* expr) {
  bool is_logical_and = expr->op() == Token::AND;
  IfBuilder compare_if(this);
  // Only use an AST evaluation context of the value kind when this expression
  // is evaluated as value as well. Otherwise stick to a test context which is
  // in sync with full codegen (see FullCodeGenerator::VisitLogicalExpression).
  Node* condition = nullptr;
  if (ast_context()->IsValue()) {
    VisitForValue(expr->left());
    Node* left = environment()->Top();
    condition = BuildToBoolean(left, expr->left()->test_id());
  } else {
    VisitForTest(expr->left());
    condition = environment()->Top();
  }
  compare_if.If(condition);
  compare_if.Then();
  if (is_logical_and) {
    environment()->Pop();
    Visit(expr->right());
  } else if (ast_context()->IsEffect()) {
    environment()->Pop();
  } else if (ast_context()->IsTest()) {
    environment()->Poke(0, jsgraph()->TrueConstant());
  }
  compare_if.Else();
  if (!is_logical_and) {
    environment()->Pop();
    Visit(expr->right());
  } else if (ast_context()->IsEffect()) {
    environment()->Pop();
  } else if (ast_context()->IsTest()) {
    environment()->Poke(0, jsgraph()->FalseConstant());
  }
  compare_if.End();
  // Skip plugging AST evaluation contexts of the test kind. This is to stay in
  // sync with full codegen which doesn't prepare the proper bailout point (see
  // the implementation of FullCodeGenerator::VisitForControl).
  if (ast_context()->IsTest()) return;
  ast_context()->ReplaceValue(expr);
}


LanguageMode AstGraphBuilder::language_mode() const {
  return current_scope()->language_mode();
}

VectorSlotPair AstGraphBuilder::CreateVectorSlotPair(FeedbackSlot slot) const {
  return VectorSlotPair(handle(info()->closure()->feedback_vector()), slot);
}


void AstGraphBuilder::VisitRewritableExpression(RewritableExpression* node) {
  Visit(node->expression());
}

CallFrequency AstGraphBuilder::ComputeCallFrequency(FeedbackSlot slot) const {
  if (invocation_frequency_.IsUnknown() || slot.IsInvalid()) {
    return CallFrequency();
  }
  Handle<FeedbackVector> feedback_vector(info()->closure()->feedback_vector(),
                                         isolate());
  CallICNexus nexus(feedback_vector, slot);
  return CallFrequency(nexus.ComputeCallFrequency() *
                       invocation_frequency_.value());
}

Node* AstGraphBuilder::ProcessArguments(const Operator* op, int arity) {
  DCHECK(environment()->stack_height() >= arity);
  Node** all = info()->zone()->NewArray<Node*>(arity);
  for (int i = arity - 1; i >= 0; --i) {
    all[i] = environment()->Pop();
  }
  Node* value = NewNode(op, arity, all);
  return value;
}


Node* AstGraphBuilder::BuildLocalActivationContext(Node* context) {
  DeclarationScope* scope = info()->scope();

  // Allocate a new local context.
  Node* local_context = scope->is_script_scope()
                            ? BuildLocalScriptContext(scope)
                            : BuildLocalFunctionContext(scope);

  if (scope->has_this_declaration() && scope->receiver()->IsContextSlot()) {
    Node* receiver = environment()->RawParameterLookup(0);
    // Context variable (at bottom of the context chain).
    Variable* variable = scope->receiver();
    DCHECK_EQ(0, scope->ContextChainLength(variable->scope()));
    const Operator* op = javascript()->StoreContext(0, variable->index());
    Node* node = NewNode(op, receiver);
    NodeProperties::ReplaceContextInput(node, local_context);
  }

  // Copy parameters into context if necessary.
  int num_parameters = scope->num_parameters();
  for (int i = 0; i < num_parameters; i++) {
    Variable* variable = scope->parameter(i);
    if (!variable->IsContextSlot()) continue;
    Node* parameter = environment()->RawParameterLookup(i + 1);
    // Context variable (at bottom of the context chain).
    DCHECK_EQ(0, scope->ContextChainLength(variable->scope()));
    const Operator* op = javascript()->StoreContext(0, variable->index());
    Node* node = NewNode(op, parameter);
    NodeProperties::ReplaceContextInput(node, local_context);
  }

  return local_context;
}


Node* AstGraphBuilder::BuildLocalFunctionContext(Scope* scope) {
  DCHECK(scope->is_function_scope() || scope->is_eval_scope());

  // Allocate a new local context.
  int slot_count = scope->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  const Operator* op =
      javascript()->CreateFunctionContext(slot_count, scope->scope_type());
  Node* local_context = NewNode(op, GetFunctionClosure());

  return local_context;
}


Node* AstGraphBuilder::BuildLocalScriptContext(Scope* scope) {
  DCHECK(scope->is_script_scope());

  // Allocate a new local context.
  Handle<ScopeInfo> scope_info = scope->scope_info();
  const Operator* op = javascript()->CreateScriptContext(scope_info);
  Node* local_context = NewNode(op, GetFunctionClosure());
  PrepareFrameState(local_context, BailoutId::ScriptContext(),
                    OutputFrameStateCombine::Push());

  return local_context;
}


Node* AstGraphBuilder::BuildLocalBlockContext(Scope* scope) {
  DCHECK(scope->is_block_scope());

  // Allocate a new local context.
  Handle<ScopeInfo> scope_info = scope->scope_info();
  const Operator* op = javascript()->CreateBlockContext(scope_info);
  Node* local_context = NewNode(op, GetFunctionClosureForContext());

  return local_context;
}


Node* AstGraphBuilder::BuildArgumentsObject(Variable* arguments) {
  if (arguments == nullptr) return nullptr;

  // Allocate and initialize a new arguments object.
  CreateArgumentsType type =
      is_strict(language_mode()) || !info()->has_simple_parameters()
          ? CreateArgumentsType::kUnmappedArguments
          : CreateArgumentsType::kMappedArguments;
  const Operator* op = javascript()->CreateArguments(type);
  Node* object = NewNode(op, GetFunctionClosure());
  PrepareFrameState(object, BailoutId::None());

  // Assign the object to the {arguments} variable. This should never lazy
  // deopt, so it is fine to send invalid bailout id.
  DCHECK(arguments->IsContextSlot() || arguments->IsStackAllocated());
  BuildVariableAssignment(arguments, object, Token::ASSIGN, VectorSlotPair(),
                          BailoutId::None());
  return object;
}

Node* AstGraphBuilder::BuildHoleCheckThenThrow(Node* value, Variable* variable,
                                               Node* not_hole,
                                               BailoutId bailout_id) {
  IfBuilder hole_check(this);
  Node* the_hole = jsgraph()->TheHoleConstant();
  Node* check = NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                        value, the_hole);
  hole_check.If(check);
  hole_check.Then();
  Node* error = BuildThrowReferenceError(variable, bailout_id);
  environment()->Push(error);
  hole_check.Else();
  environment()->Push(not_hole);
  hole_check.End();
  return environment()->Pop();
}


Node* AstGraphBuilder::BuildHoleCheckElseThrow(Node* value, Variable* variable,
                                               Node* for_hole,
                                               BailoutId bailout_id) {
  IfBuilder hole_check(this);
  Node* the_hole = jsgraph()->TheHoleConstant();
  Node* check = NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                        value, the_hole);
  hole_check.If(check);
  hole_check.Then();
  environment()->Push(for_hole);
  hole_check.Else();
  Node* error = BuildThrowReferenceError(variable, bailout_id);
  environment()->Push(error);
  hole_check.End();
  return environment()->Pop();
}

Node* AstGraphBuilder::BuildVariableLoad(Variable* variable,
                                         BailoutId bailout_id,
                                         const VectorSlotPair& feedback,
                                         OutputFrameStateCombine combine,
                                         TypeofMode typeof_mode) {
  Node* the_hole = jsgraph()->TheHoleConstant();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      // Global var, const, or let variable.
      Handle<Name> name = variable->name();
      if (Node* node = TryLoadGlobalConstant(name)) return node;
      Node* value = BuildGlobalLoad(name, feedback, typeof_mode);
      PrepareFrameState(value, bailout_id, combine);
      return value;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      // Local var, const, or let variable.
      Node* value = environment()->Lookup(variable);
      if (variable->binding_needs_init()) {
        // Perform check for uninitialized let/const variables.
        if (value->op() == the_hole->op()) {
          value = BuildThrowReferenceError(variable, bailout_id);
        } else if (value->opcode() == IrOpcode::kPhi) {
          value = BuildHoleCheckThenThrow(value, variable, value, bailout_id);
        }
      }
      return value;
    }
    case VariableLocation::CONTEXT: {
      // Context variable (potentially up the context chain).
      int depth = current_scope()->ContextChainLength(variable->scope());
      // TODO(mstarzinger): The {maybe_assigned} flag computed during variable
      // resolution is highly inaccurate and cannot be trusted. We are only
      // taking this information into account when asm.js compilation is used.
      bool immutable = variable->maybe_assigned() == kNotAssigned &&
                       info()->is_function_context_specializing();
      const Operator* op =
          javascript()->LoadContext(depth, variable->index(), immutable);
      Node* value = NewNode(op);
      // TODO(titzer): initialization checks are redundant for already
      // initialized immutable context loads, but only specialization knows.
      // Maybe specializer should be a parameter to the graph builder?
      if (variable->binding_needs_init()) {
        // Perform check for uninitialized let/const variables.
        value = BuildHoleCheckThenThrow(value, variable, value, bailout_id);
      }
      return value;
    }
    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
  UNREACHABLE();
  return nullptr;
}


Node* AstGraphBuilder::BuildVariableDelete(Variable* variable,
                                           BailoutId bailout_id,
                                           OutputFrameStateCombine combine) {
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      // Global var, const, or let variable.
      Node* global = BuildLoadGlobalObject();
      Node* name = jsgraph()->Constant(variable->name());
      Node* mode = jsgraph()->Constant(static_cast<int32_t>(language_mode()));
      const Operator* op = javascript()->DeleteProperty();
      Node* result = NewNode(op, global, name, mode);
      PrepareFrameState(result, bailout_id, combine);
      return result;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
    case VariableLocation::CONTEXT: {
      // Local var, const, or let variable or context variable.
      return jsgraph()->BooleanConstant(variable->is_this());
    }
    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
  UNREACHABLE();
  return nullptr;
}

Node* AstGraphBuilder::BuildVariableAssignment(
    Variable* variable, Node* value, Token::Value op,
    const VectorSlotPair& feedback, BailoutId bailout_id,
    OutputFrameStateCombine combine) {
  Node* the_hole = jsgraph()->TheHoleConstant();
  VariableMode mode = variable->mode();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      // Global var, const, or let variable.
      Handle<Name> name = variable->name();
      Node* store = BuildGlobalStore(name, value, feedback);
      PrepareFrameState(store, bailout_id, combine);
      return store;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      // Local var, const, or let variable.
      if (mode == LET && op == Token::INIT) {
        // No initialization check needed because scoping guarantees it. Note
        // that we still perform a lookup to keep the variable live, because
        // baseline code might contain debug code that inspects the variable.
        Node* current = environment()->Lookup(variable);
        CHECK_NOT_NULL(current);
      } else if (mode == LET && op != Token::INIT &&
                 variable->binding_needs_init()) {
        // Perform an initialization check for let declared variables.
        Node* current = environment()->Lookup(variable);
        if (current->op() == the_hole->op()) {
          return BuildThrowReferenceError(variable, bailout_id);
        } else if (current->opcode() == IrOpcode::kPhi) {
          BuildHoleCheckThenThrow(current, variable, value, bailout_id);
        }
      } else if (mode == CONST && op == Token::INIT) {
        // Perform an initialization check for const {this} variables.
        // Note that the {this} variable is the only const variable being able
        // to trigger bind operations outside the TDZ, via {super} calls.
        Node* current = environment()->Lookup(variable);
        if (current->op() != the_hole->op() && variable->is_this()) {
          value = BuildHoleCheckElseThrow(current, variable, value, bailout_id);
        }
      } else if (mode == CONST && op != Token::INIT &&
                 variable->is_sloppy_function_name()) {
        // Non-initializing assignment to sloppy function names is
        // - exception in strict mode.
        // - ignored in sloppy mode.
        DCHECK(!variable->binding_needs_init());
        if (variable->throw_on_const_assignment(language_mode())) {
          return BuildThrowConstAssignError(bailout_id);
        }
        return value;
      } else if (mode == CONST && op != Token::INIT) {
        if (variable->binding_needs_init()) {
          Node* current = environment()->Lookup(variable);
          if (current->op() == the_hole->op()) {
            return BuildThrowReferenceError(variable, bailout_id);
          } else if (current->opcode() == IrOpcode::kPhi) {
            BuildHoleCheckThenThrow(current, variable, value, bailout_id);
          }
        }
        // Assignment to const is exception in all modes.
        return BuildThrowConstAssignError(bailout_id);
      }
      environment()->Bind(variable, value);
      return value;
    case VariableLocation::CONTEXT: {
      // Context variable (potentially up the context chain).
      int depth = current_scope()->ContextChainLength(variable->scope());
      if (mode == LET && op != Token::INIT && variable->binding_needs_init()) {
        // Perform an initialization check for let declared variables.
        const Operator* op =
            javascript()->LoadContext(depth, variable->index(), false);
        Node* current = NewNode(op);
        value = BuildHoleCheckThenThrow(current, variable, value, bailout_id);
      } else if (mode == CONST && op == Token::INIT) {
        // Perform an initialization check for const {this} variables.
        // Note that the {this} variable is the only const variable being able
        // to trigger bind operations outside the TDZ, via {super} calls.
        if (variable->is_this()) {
          const Operator* op =
              javascript()->LoadContext(depth, variable->index(), false);
          Node* current = NewNode(op);
          value = BuildHoleCheckElseThrow(current, variable, value, bailout_id);
        }
      } else if (mode == CONST && op != Token::INIT &&
                 variable->is_sloppy_function_name()) {
        // Non-initializing assignment to sloppy function names is
        // - exception in strict mode.
        // - ignored in sloppy mode.
        DCHECK(!variable->binding_needs_init());
        if (variable->throw_on_const_assignment(language_mode())) {
          return BuildThrowConstAssignError(bailout_id);
        }
        return value;
      } else if (mode == CONST && op != Token::INIT) {
        if (variable->binding_needs_init()) {
          const Operator* op =
              javascript()->LoadContext(depth, variable->index(), false);
          Node* current = NewNode(op);
          BuildHoleCheckThenThrow(current, variable, value, bailout_id);
        }
        // Assignment to const is exception in all modes.
        return BuildThrowConstAssignError(bailout_id);
      }
      const Operator* op = javascript()->StoreContext(depth, variable->index());
      return NewNode(op, value);
    }
    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
  UNREACHABLE();
  return nullptr;
}


Node* AstGraphBuilder::BuildKeyedLoad(Node* object, Node* key,
                                      const VectorSlotPair& feedback) {
  const Operator* op = javascript()->LoadProperty(feedback);
  Node* node = NewNode(op, object, key);
  return node;
}


Node* AstGraphBuilder::BuildNamedLoad(Node* object, Handle<Name> name,
                                      const VectorSlotPair& feedback) {
  const Operator* op = javascript()->LoadNamed(name, feedback);
  Node* node = NewNode(op, object);
  return node;
}


Node* AstGraphBuilder::BuildKeyedStore(Node* object, Node* key, Node* value,
                                       const VectorSlotPair& feedback) {
  DCHECK_EQ(feedback.vector()->GetLanguageMode(feedback.slot()),
            language_mode());
  const Operator* op = javascript()->StoreProperty(language_mode(), feedback);
  Node* node = NewNode(op, object, key, value);
  return node;
}


Node* AstGraphBuilder::BuildNamedStore(Node* object, Handle<Name> name,
                                       Node* value,
                                       const VectorSlotPair& feedback) {
  DCHECK_EQ(feedback.vector()->GetLanguageMode(feedback.slot()),
            language_mode());
  const Operator* op =
      javascript()->StoreNamed(language_mode(), name, feedback);
  Node* node = NewNode(op, object, value);
  return node;
}

Node* AstGraphBuilder::BuildNamedStoreOwn(Node* object, Handle<Name> name,
                                          Node* value,
                                          const VectorSlotPair& feedback) {
  DCHECK_EQ(FeedbackSlotKind::kStoreOwnNamed,
            feedback.vector()->GetKind(feedback.slot()));
  const Operator* op = javascript()->StoreNamedOwn(name, feedback);
  Node* node = NewNode(op, object, value);
  return node;
}

Node* AstGraphBuilder::BuildGlobalLoad(Handle<Name> name,
                                       const VectorSlotPair& feedback,
                                       TypeofMode typeof_mode) {
  DCHECK_EQ(feedback.vector()->GetTypeofMode(feedback.slot()), typeof_mode);
  const Operator* op = javascript()->LoadGlobal(name, feedback, typeof_mode);
  Node* node = NewNode(op);
  return node;
}


Node* AstGraphBuilder::BuildGlobalStore(Handle<Name> name, Node* value,
                                        const VectorSlotPair& feedback) {
  const Operator* op =
      javascript()->StoreGlobal(language_mode(), name, feedback);
  Node* node = NewNode(op, value);
  return node;
}

Node* AstGraphBuilder::BuildLoadGlobalObject() {
  return BuildLoadNativeContextField(Context::EXTENSION_INDEX);
}


Node* AstGraphBuilder::BuildLoadNativeContextField(int index) {
  const Operator* op =
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true);
  Node* native_context = NewNode(op);
  Node* result = NewNode(javascript()->LoadContext(0, index, true));
  NodeProperties::ReplaceContextInput(result, native_context);
  return result;
}


Node* AstGraphBuilder::BuildToBoolean(Node* input, TypeFeedbackId feedback_id) {
  if (Node* node = TryFastToBoolean(input)) return node;
  ToBooleanHints hints = ToBooleanHint::kAny;
  return NewNode(javascript()->ToBoolean(hints), input);
}


Node* AstGraphBuilder::BuildToObject(Node* input, BailoutId bailout_id) {
  Node* object = NewNode(javascript()->ToObject(), input);
  PrepareFrameState(object, bailout_id, OutputFrameStateCombine::Push());
  return object;
}

Node* AstGraphBuilder::BuildSetHomeObject(Node* value, Node* home_object,
                                          LiteralProperty* property,
                                          int slot_number) {
  Expression* expr = property->value();
  if (!FunctionLiteral::NeedsHomeObject(expr)) return value;
  Handle<Name> name = isolate()->factory()->home_object_symbol();
  VectorSlotPair feedback =
      CreateVectorSlotPair(property->GetSlot(slot_number));
  Node* store = BuildNamedStore(value, name, home_object, feedback);
  PrepareFrameState(store, BailoutId::None(),
                    OutputFrameStateCombine::Ignore());
  return store;
}


Node* AstGraphBuilder::BuildThrowError(Node* exception, BailoutId bailout_id) {
  const Operator* op = javascript()->CallRuntime(Runtime::kThrow);
  Node* call = NewNode(op, exception);
  PrepareFrameState(call, bailout_id);
  Node* control = NewNode(common()->Throw());
  UpdateControlDependencyToLeaveFunction(control);
  return call;
}


Node* AstGraphBuilder::BuildThrowReferenceError(Variable* variable,
                                                BailoutId bailout_id) {
  Node* variable_name = jsgraph()->Constant(variable->name());
  const Operator* op = javascript()->CallRuntime(Runtime::kThrowReferenceError);
  Node* call = NewNode(op, variable_name);
  PrepareFrameState(call, bailout_id);
  Node* control = NewNode(common()->Throw());
  UpdateControlDependencyToLeaveFunction(control);
  return call;
}


Node* AstGraphBuilder::BuildThrowConstAssignError(BailoutId bailout_id) {
  const Operator* op =
      javascript()->CallRuntime(Runtime::kThrowConstAssignError);
  Node* call = NewNode(op);
  PrepareFrameState(call, bailout_id);
  Node* control = NewNode(common()->Throw());
  UpdateControlDependencyToLeaveFunction(control);
  return call;
}


Node* AstGraphBuilder::BuildReturn(Node* return_value) {
  // Emit tracing call if requested to do so.
  if (FLAG_trace) {
    return_value =
        NewNode(javascript()->CallRuntime(Runtime::kTraceExit), return_value);
  }
  Node* pop_node = jsgraph()->ZeroConstant();
  Node* control = NewNode(common()->Return(), pop_node, return_value);
  UpdateControlDependencyToLeaveFunction(control);
  return control;
}


Node* AstGraphBuilder::BuildThrow(Node* exception_value) {
  NewNode(javascript()->CallRuntime(Runtime::kReThrow), exception_value);
  Node* control = NewNode(common()->Throw());
  UpdateControlDependencyToLeaveFunction(control);
  return control;
}


Node* AstGraphBuilder::BuildBinaryOp(Node* left, Node* right, Token::Value op,
                                     TypeFeedbackId feedback_id) {
  const Operator* js_op;
  BinaryOperationHint hint = BinaryOperationHint::kAny;
  switch (op) {
    case Token::BIT_OR:
      js_op = javascript()->BitwiseOr();
      break;
    case Token::BIT_AND:
      js_op = javascript()->BitwiseAnd();
      break;
    case Token::BIT_XOR:
      js_op = javascript()->BitwiseXor();
      break;
    case Token::SHL:
      js_op = javascript()->ShiftLeft();
      break;
    case Token::SAR:
      js_op = javascript()->ShiftRight();
      break;
    case Token::SHR:
      js_op = javascript()->ShiftRightLogical();
      break;
    case Token::ADD:
      js_op = javascript()->Add(hint);
      break;
    case Token::SUB:
      js_op = javascript()->Subtract();
      break;
    case Token::MUL:
      js_op = javascript()->Multiply();
      break;
    case Token::DIV:
      js_op = javascript()->Divide();
      break;
    case Token::MOD:
      js_op = javascript()->Modulus();
      break;
    default:
      UNREACHABLE();
      js_op = nullptr;
  }
  return NewNode(js_op, left, right);
}


Node* AstGraphBuilder::TryLoadGlobalConstant(Handle<Name> name) {
  // Optimize global constants like "undefined", "Infinity", and "NaN".
  Handle<Object> constant_value = isolate()->factory()->GlobalConstantFor(name);
  if (!constant_value.is_null()) return jsgraph()->Constant(constant_value);
  return nullptr;
}

Node* AstGraphBuilder::TryFastToBoolean(Node* input) {
  switch (input->opcode()) {
    case IrOpcode::kNumberConstant: {
      NumberMatcher m(input);
      return jsgraph_->BooleanConstant(!m.Is(0) && !m.IsNaN());
    }
    case IrOpcode::kHeapConstant: {
      Handle<HeapObject> object = HeapObjectMatcher(input).Value();
      return jsgraph_->BooleanConstant(object->BooleanValue());
    }
    case IrOpcode::kJSEqual:
    case IrOpcode::kJSStrictEqual:
    case IrOpcode::kJSLessThan:
    case IrOpcode::kJSLessThanOrEqual:
    case IrOpcode::kJSGreaterThan:
    case IrOpcode::kJSGreaterThanOrEqual:
    case IrOpcode::kJSToBoolean:
    case IrOpcode::kJSDeleteProperty:
    case IrOpcode::kJSHasProperty:
    case IrOpcode::kJSInstanceOf:
      return input;
    default:
      break;
  }
  return nullptr;
}


bool AstGraphBuilder::CheckOsrEntry(IterationStatement* stmt) {
  if (info()->osr_ast_id() == stmt->OsrEntryId()) {
    DCHECK_EQ(-1, info()->osr_expr_stack_height());
    info()->set_osr_expr_stack_height(environment()->stack_height());
    return true;
  }
  return false;
}


void AstGraphBuilder::PrepareFrameState(Node* node, BailoutId ast_id,
                                        OutputFrameStateCombine combine) {
  if (OperatorProperties::HasFrameStateInput(node->op())) {
    DCHECK(ast_id.IsNone() || info()->shared_info()->VerifyBailoutId(ast_id));
    DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
    DCHECK_EQ(IrOpcode::kDead,
              NodeProperties::GetFrameStateInput(node)->opcode());
    bool has_exception = NodeProperties::IsExceptionalCall(node);
    Node* state = environment()->Checkpoint(ast_id, combine, has_exception);
    NodeProperties::ReplaceFrameStateInput(node, state);
  }
}

void AstGraphBuilder::PrepareEagerCheckpoint(BailoutId ast_id) {
  if (environment()->GetEffectDependency()->opcode() == IrOpcode::kCheckpoint) {
    // We skip preparing a checkpoint if there already is one the current effect
    // dependency. This is just an optimization and not need for correctness.
    return;
  }
  if (ast_id != BailoutId::None()) {
    DCHECK(info()->shared_info()->VerifyBailoutId(ast_id));
    Node* node = NewNode(common()->Checkpoint());
    DCHECK_EQ(IrOpcode::kDead,
              NodeProperties::GetFrameStateInput(node)->opcode());
    Node* state = environment()->Checkpoint(ast_id);
    NodeProperties::ReplaceFrameStateInput(node, state);
  }
}

BitVector* AstGraphBuilder::GetVariablesAssignedInLoop(
    IterationStatement* stmt) {
  if (loop_assignment_analysis_ == nullptr) return nullptr;
  return loop_assignment_analysis_->GetVariablesAssignedInLoop(stmt);
}


Node** AstGraphBuilder::EnsureInputBufferSize(int size) {
  if (size > input_buffer_size_) {
    size = size + kInputBufferSizeIncrement + input_buffer_size_;
    input_buffer_ = local_zone()->NewArray<Node*>(size);
    input_buffer_size_ = size;
  }
  return input_buffer_;
}


Node* AstGraphBuilder::MakeNode(const Operator* op, int value_input_count,
                                Node** value_inputs, bool incomplete) {
  DCHECK_EQ(op->ValueInputCount(), value_input_count);

  bool has_context = OperatorProperties::HasContextInput(op);
  bool has_frame_state = OperatorProperties::HasFrameStateInput(op);
  bool has_control = op->ControlInputCount() == 1;
  bool has_effect = op->EffectInputCount() == 1;

  DCHECK(op->ControlInputCount() < 2);
  DCHECK(op->EffectInputCount() < 2);

  Node* result = nullptr;
  if (!has_context && !has_frame_state && !has_control && !has_effect) {
    result = graph()->NewNode(op, value_input_count, value_inputs, incomplete);
  } else {
    int input_count_with_deps = value_input_count;
    if (has_context) ++input_count_with_deps;
    if (has_frame_state) ++input_count_with_deps;
    if (has_control) ++input_count_with_deps;
    if (has_effect) ++input_count_with_deps;
    Node** buffer = EnsureInputBufferSize(input_count_with_deps);
    memcpy(buffer, value_inputs, kPointerSize * value_input_count);
    Node** current_input = buffer + value_input_count;
    if (has_context) {
      *current_input++ = current_context();
    }
    if (has_frame_state) {
      // The frame state will be inserted later. Here we misuse
      // the {Dead} node as a sentinel to be later overwritten
      // with the real frame state.
      *current_input++ = jsgraph()->Dead();
    }
    if (has_effect) {
      *current_input++ = environment_->GetEffectDependency();
    }
    if (has_control) {
      *current_input++ = environment_->GetControlDependency();
    }
    result = graph()->NewNode(op, input_count_with_deps, buffer, incomplete);
    if (!environment()->IsMarkedAsUnreachable()) {
      // Update the current control dependency for control-producing nodes.
      if (result->op()->ControlOutputCount() > 0) {
        environment_->UpdateControlDependency(result);
      }
      // Update the current effect dependency for effect-producing nodes.
      if (result->op()->EffectOutputCount() > 0) {
        environment_->UpdateEffectDependency(result);
      }
    }
  }

  return result;
}


void AstGraphBuilder::UpdateControlDependencyToLeaveFunction(Node* exit) {
  if (environment()->IsMarkedAsUnreachable()) return;
  environment()->MarkAsUnreachable();
  exit_controls_.push_back(exit);
}


void AstGraphBuilder::Environment::Merge(Environment* other) {
  DCHECK(values_.size() == other->values_.size());
  DCHECK(contexts_.size() == other->contexts_.size());

  // Nothing to do if the other environment is dead.
  if (other->IsMarkedAsUnreachable()) return;

  // Resurrect a dead environment by copying the contents of the other one and
  // placing a singleton merge as the new control dependency.
  if (this->IsMarkedAsUnreachable()) {
    Node* other_control = other->control_dependency_;
    Node* inputs[] = {other_control};
    control_dependency_ =
        graph()->NewNode(common()->Merge(1), arraysize(inputs), inputs, true);
    effect_dependency_ = other->effect_dependency_;
    values_ = other->values_;
    contexts_ = other->contexts_;
    if (IsLivenessAnalysisEnabled()) {
      liveness_block_ =
          builder_->liveness_analyzer()->NewBlock(other->liveness_block());
    }
    return;
  }

  // Record the merge for the local variable liveness calculation.
  // For loops, we are connecting a back edge into the existing block;
  // for merges, we create a new merged block.
  if (IsLivenessAnalysisEnabled()) {
    if (GetControlDependency()->opcode() != IrOpcode::kLoop) {
      liveness_block_ =
          builder_->liveness_analyzer()->NewBlock(liveness_block());
    }
    liveness_block()->AddPredecessor(other->liveness_block());
  }

  // Create a merge of the control dependencies of both environments and update
  // the current environment's control dependency accordingly.
  Node* control = builder_->MergeControl(this->GetControlDependency(),
                                         other->GetControlDependency());
  UpdateControlDependency(control);

  // Create a merge of the effect dependencies of both environments and update
  // the current environment's effect dependency accordingly.
  Node* effect = builder_->MergeEffect(this->GetEffectDependency(),
                                       other->GetEffectDependency(), control);
  UpdateEffectDependency(effect);

  // Introduce Phi nodes for values that have differing input at merge points,
  // potentially extending an existing Phi node if possible.
  for (int i = 0; i < static_cast<int>(values_.size()); ++i) {
    values_[i] = builder_->MergeValue(values_[i], other->values_[i], control);
  }
  for (int i = 0; i < static_cast<int>(contexts_.size()); ++i) {
    contexts_[i] =
        builder_->MergeValue(contexts_[i], other->contexts_[i], control);
  }
}

void AstGraphBuilder::Environment::PrepareForOsrEntry() {
  int size = static_cast<int>(values()->size());
  Graph* graph = builder_->graph();

  // Set the control and effect to the OSR loop entry.
  Node* osr_loop_entry = graph->NewNode(builder_->common()->OsrLoopEntry(),
                                        graph->start(), graph->start());
  Node* effect = osr_loop_entry;
  UpdateControlDependency(osr_loop_entry);
  UpdateEffectDependency(effect);

  // Set OSR values.
  for (int i = 0; i < size; ++i) {
    values()->at(i) =
        graph->NewNode(builder_->common()->OsrValue(i), osr_loop_entry);
  }

  // Set the innermost context.
  const Operator* op_inner =
      builder_->common()->OsrValue(Linkage::kOsrContextSpillSlotIndex);
  contexts()->back() = graph->NewNode(op_inner, osr_loop_entry);

  // The innermost context is the OSR value, and the outer contexts are
  // reconstructed by dynamically walking up the context chain.
  const Operator* load_op =
      builder_->javascript()->LoadContext(0, Context::PREVIOUS_INDEX, true);
  Node* osr_context = contexts()->back();
  int last = static_cast<int>(contexts()->size() - 1);
  for (int i = last - 1; i >= 0; i--) {
    osr_context = effect = graph->NewNode(load_op, osr_context, effect);
    contexts()->at(i) = osr_context;
  }
  UpdateEffectDependency(effect);
}

void AstGraphBuilder::Environment::PrepareForLoop(BitVector* assigned) {
  int size = static_cast<int>(values()->size());

  Node* control = builder_->NewLoop();
  if (assigned == nullptr) {
    // Assume that everything is updated in the loop.
    for (int i = 0; i < size; ++i) {
      values()->at(i) = builder_->NewPhi(1, values()->at(i), control);
    }
  } else {
    // Only build phis for those locals assigned in this loop.
    for (int i = 0; i < size; ++i) {
      if (i < assigned->length() && !assigned->Contains(i)) continue;
      Node* phi = builder_->NewPhi(1, values()->at(i), control);
      values()->at(i) = phi;
    }
  }
  Node* effect = builder_->NewEffectPhi(1, GetEffectDependency(), control);
  UpdateEffectDependency(effect);

  // Connect the loop to end via Terminate if it's not marked as unreachable.
  if (!IsMarkedAsUnreachable()) {
    // Connect the Loop node to end via a Terminate node.
    Node* terminate = builder_->graph()->NewNode(
        builder_->common()->Terminate(), effect, control);
    builder_->exit_controls_.push_back(terminate);
  }

  if (builder_->info()->is_osr()) {
    // Introduce phis for all context values in the case of an OSR graph.
    for (size_t i = 0; i < contexts()->size(); ++i) {
      Node* context = contexts()->at(i);
      contexts()->at(i) = builder_->NewPhi(1, context, control);
    }
  }
}


Node* AstGraphBuilder::NewPhi(int count, Node* input, Node* control) {
  const Operator* phi_op = common()->Phi(MachineRepresentation::kTagged, count);
  Node** buffer = EnsureInputBufferSize(count + 1);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer, true);
}


Node* AstGraphBuilder::NewEffectPhi(int count, Node* input, Node* control) {
  const Operator* phi_op = common()->EffectPhi(count);
  Node** buffer = EnsureInputBufferSize(count + 1);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer, true);
}


Node* AstGraphBuilder::MergeControl(Node* control, Node* other) {
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
    Node* inputs[] = {control, other};
    control = graph()->NewNode(op, arraysize(inputs), inputs, true);
  }
  return control;
}


Node* AstGraphBuilder::MergeEffect(Node* value, Node* other, Node* control) {
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


Node* AstGraphBuilder::MergeValue(Node* value, Node* other, Node* control) {
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

AstGraphBuilderWithPositions::AstGraphBuilderWithPositions(
    Zone* local_zone, CompilationInfo* info, JSGraph* jsgraph,
    CallFrequency invocation_frequency, LoopAssignmentAnalysis* loop_assignment,
    SourcePositionTable* source_positions, int inlining_id)
    : AstGraphBuilder(local_zone, info, jsgraph, invocation_frequency,
                      loop_assignment),
      source_positions_(source_positions),
      start_position_(info->shared_info()->start_position(), inlining_id) {}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

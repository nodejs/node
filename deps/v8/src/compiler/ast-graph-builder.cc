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
#include "src/compiler/type-hint-analyzer.h"

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

  class DeferredCommands;

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

// Helper class for a try-finally control scope. It can record intercepted
// control-flow commands that cause entry into a finally-block, and re-apply
// them after again leaving that block. Special tokens are used to identify
// paths going through the finally-block to dispatch after leaving the block.
class AstGraphBuilder::ControlScope::DeferredCommands : public ZoneObject {
 public:
  explicit DeferredCommands(AstGraphBuilder* owner)
      : owner_(owner),
        deferred_(owner->local_zone()),
        return_token_(nullptr),
        throw_token_(nullptr) {}

  // One recorded control-flow command.
  struct Entry {
    Command command;       // The command type being applied on this path.
    Statement* statement;  // The target statement for the command or {nullptr}.
    Node* token;           // A token identifying this particular path.
  };

  // Records a control-flow command while entering the finally-block. This also
  // generates a new dispatch token that identifies one particular path.
  Node* RecordCommand(Command cmd, Statement* stmt, Node* value) {
    Node* token = nullptr;
    switch (cmd) {
      case CMD_BREAK:
      case CMD_CONTINUE:
        token = NewPathToken(dispenser_.GetBreakContinueToken());
        break;
      case CMD_THROW:
        if (throw_token_) return throw_token_;
        token = NewPathToken(TokenDispenserForFinally::kThrowToken);
        throw_token_ = token;
        break;
      case CMD_RETURN:
        if (return_token_) return return_token_;
        token = NewPathToken(TokenDispenserForFinally::kReturnToken);
        return_token_ = token;
        break;
    }
    DCHECK_NOT_NULL(token);
    deferred_.push_back({cmd, stmt, token});
    return token;
  }

  // Returns the dispatch token to be used to identify the implicit fall-through
  // path at the end of a try-block into the corresponding finally-block.
  Node* GetFallThroughToken() { return NewPathTokenForImplicitFallThrough(); }

  // Applies all recorded control-flow commands after the finally-block again.
  // This generates a dynamic dispatch on the token from the entry point.
  void ApplyDeferredCommands(Node* token, Node* value) {
    SwitchBuilder dispatch(owner_, static_cast<int>(deferred_.size()));
    dispatch.BeginSwitch();
    for (size_t i = 0; i < deferred_.size(); ++i) {
      Node* condition = NewPathDispatchCondition(token, deferred_[i].token);
      dispatch.BeginLabel(static_cast<int>(i), condition);
      dispatch.EndLabel();
    }
    for (size_t i = 0; i < deferred_.size(); ++i) {
      dispatch.BeginCase(static_cast<int>(i));
      owner_->execution_control()->PerformCommand(
          deferred_[i].command, deferred_[i].statement, value);
      dispatch.EndCase();
    }
    dispatch.EndSwitch();
  }

 protected:
  Node* NewPathToken(int token_id) {
    return owner_->jsgraph()->Constant(token_id);
  }
  Node* NewPathTokenForImplicitFallThrough() {
    return NewPathToken(TokenDispenserForFinally::kFallThroughToken);
  }
  Node* NewPathDispatchCondition(Node* t1, Node* t2) {
    return owner_->NewNode(
        owner_->javascript()->StrictEqual(CompareOperationHint::kAny), t1, t2);
  }

 private:
  TokenDispenserForFinally dispenser_;
  AstGraphBuilder* owner_;
  ZoneVector<Entry> deferred_;
  Node* return_token_;
  Node* throw_token_;
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


// Control scope implementation for a TryCatchStatement.
class AstGraphBuilder::ControlScopeForCatch : public ControlScope {
 public:
  ControlScopeForCatch(AstGraphBuilder* owner, TryCatchStatement* stmt,
                       TryCatchBuilder* control)
      : ControlScope(owner), control_(control) {
    builder()->try_nesting_level_++;  // Increment nesting.
  }
  ~ControlScopeForCatch() {
    builder()->try_nesting_level_--;  // Decrement nesting.
  }

 protected:
  bool Execute(Command cmd, Statement* target, Node** value) override {
    switch (cmd) {
      case CMD_THROW:
        control_->Throw(*value);
        return true;
      case CMD_BREAK:
      case CMD_CONTINUE:
      case CMD_RETURN:
        break;
    }
    return false;
  }

 private:
  TryCatchBuilder* control_;
};


// Control scope implementation for a TryFinallyStatement.
class AstGraphBuilder::ControlScopeForFinally : public ControlScope {
 public:
  ControlScopeForFinally(AstGraphBuilder* owner, TryFinallyStatement* stmt,
                         DeferredCommands* commands, TryFinallyBuilder* control)
      : ControlScope(owner), commands_(commands), control_(control) {
    builder()->try_nesting_level_++;  // Increment nesting.
  }
  ~ControlScopeForFinally() {
    builder()->try_nesting_level_--;  // Decrement nesting.
  }

 protected:
  bool Execute(Command cmd, Statement* target, Node** value) override {
    Node* token = commands_->RecordCommand(cmd, target, *value);
    control_->LeaveTry(token, *value);
    return true;
  }

 private:
  DeferredCommands* commands_;
  TryFinallyBuilder* control_;
};

AstGraphBuilder::AstGraphBuilder(Zone* local_zone, CompilationInfo* info,
                                 JSGraph* jsgraph, float invocation_frequency,
                                 LoopAssignmentAnalysis* loop,
                                 TypeHintAnalysis* type_hint_analysis)
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
      try_nesting_level_(0),
      input_buffer_size_(0),
      input_buffer_(nullptr),
      exit_controls_(local_zone),
      loop_assignment_analysis_(loop),
      type_hint_analysis_(type_hint_analysis),
      state_values_cache_(jsgraph),
      liveness_analyzer_(static_cast<size_t>(info->scope()->num_stack_slots()),
                         local_zone),
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
    return NewNode(op, current_context());
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


Node* AstGraphBuilder::GetNewTarget() {
  if (!new_target_.is_set()) {
    int params = info()->num_parameters_including_this();
    int index = Linkage::GetJSCallNewTargetParamIndex(params);
    const Operator* op = common()->Parameter(index, "%new.target");
    Node* node = NewNode(op, graph()->start());
    new_target_.set(node);
  }
  return new_target_.get();
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

  // Build rest arguments array if it is used.
  Variable* rest_parameter = scope->rest_parameter();
  BuildRestArgumentsArray(rest_parameter);

  // Build assignment to {.this_function} variable if it is used.
  BuildThisFunctionVariable(scope->this_function_var());

  // Build assignment to {new.target} variable if it is used.
  BuildNewTargetVariable(scope->new_target_var());

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
      liveness_analyzer()->local_count(), local_zone());
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
  return new (zone())
      Environment(this, builder_->liveness_analyzer()->NewBlock());
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
    const Operator* op = common()->StateValues(count);
    (*state_values) = graph()->NewNode(op, count, env_values);
  }
}


void AstGraphBuilder::Environment::UpdateStateValuesWithCache(
    Node** state_values, int offset, int count) {
  Node** env_values = (count == 0) ? nullptr : &values()->at(offset);
  *state_values = builder_->state_values_cache_.GetNodeForValues(
      env_values, static_cast<size_t>(count));
}

Node* AstGraphBuilder::Environment::Checkpoint(BailoutId ast_id,
                                               OutputFrameStateCombine combine,
                                               bool owner_has_exception) {
  if (!builder()->info()->is_deoptimization_enabled()) {
    return builder()->GetEmptyFrameState();
  }

  UpdateStateValues(&parameters_node_, 0, parameters_count());
  UpdateStateValuesWithCache(&locals_node_, parameters_count(), locals_count());
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
      FeedbackVectorSlot slot = decl->proxy()->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals()->push_back(handle(Smi::FromInt(slot.ToInt()), isolate()));
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
        NewNode(op, current_context(), value);
      }
      break;
    case VariableLocation::LOOKUP: {
      DCHECK(!variable->binding_needs_init());
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op = javascript()->CallRuntime(Runtime::kDeclareEvalVar);
      Node* store = NewNode(op, name);
      PrepareFrameState(store, decl->proxy()->id());
      break;
    }
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
      FeedbackVectorSlot slot = decl->proxy()->VariableFeedbackSlot();
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
      NewNode(op, current_context(), value);
      break;
    }
    case VariableLocation::LOOKUP: {
      VisitForValue(decl->fun());
      Node* value = environment()->Pop();
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op =
          javascript()->CallRuntime(Runtime::kDeclareEvalFunction);
      Node* store = NewNode(op, name, value);
      PrepareFrameState(store, decl->proxy()->id());
      break;
    }
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
  VisitForValue(stmt->expression());
  Node* value = environment()->Pop();
  Node* object = BuildToObject(value, stmt->ToObjectId());
  Handle<ScopeInfo> scope_info = stmt->scope()->scope_info();
  const Operator* op = javascript()->CreateWithContext(scope_info);
  Node* context = NewNode(op, object, GetFunctionClosureForContext());
  PrepareFrameState(context, stmt->EntryId());
  VisitInScope(stmt->statement(), stmt->scope(), context);
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

    CompareOperationHint hint;
    if (!type_hint_analysis_ ||
        !type_hint_analysis_->GetCompareOperationHint(clause->CompareId(),
                                                      &hint)) {
      hint = CompareOperationHint::kAny;
    }

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
  VisitForValue(stmt->subject());
  Node* object = environment()->Pop();
  BlockBuilder for_block(this);
  for_block.BeginBlock();
  // Check for null or undefined before entering loop.
  Node* is_null_cond =
      NewNode(javascript()->StrictEqual(CompareOperationHint::kAny), object,
              jsgraph()->NullConstant());
  for_block.BreakWhen(is_null_cond, BranchHint::kFalse);
  Node* is_undefined_cond =
      NewNode(javascript()->StrictEqual(CompareOperationHint::kAny), object,
              jsgraph()->UndefinedConstant());
  for_block.BreakWhen(is_undefined_cond, BranchHint::kFalse);
  {
    // Convert object to jsobject.
    object = BuildToObject(object, stmt->ToObjectId());
    environment()->Push(object);

    // Prepare for-in cache.
    Node* prepare = NewNode(javascript()->ForInPrepare(), object);
    PrepareFrameState(prepare, stmt->PrepareId(),
                      OutputFrameStateCombine::Push(3));
    Node* cache_type = NewNode(common()->Projection(0), prepare);
    Node* cache_array = NewNode(common()->Projection(1), prepare);
    Node* cache_length = NewNode(common()->Projection(2), prepare);

    // Construct the rest of the environment.
    environment()->Push(cache_type);
    environment()->Push(cache_array);
    environment()->Push(cache_length);
    environment()->Push(jsgraph()->ZeroConstant());

    // Build the actual loop body.
    LoopBuilder for_loop(this);
    for_loop.BeginLoop(GetVariablesAssignedInLoop(stmt), CheckOsrEntry(stmt));
    {
      // These stack values are renamed in the case of OSR, so reload them
      // from the environment.
      Node* index = environment()->Peek(0);
      Node* cache_length = environment()->Peek(1);
      Node* cache_array = environment()->Peek(2);
      Node* cache_type = environment()->Peek(3);
      Node* object = environment()->Peek(4);

      // Check loop termination condition (we know that the {index} is always
      // in Smi range, so we can just set the hint on the comparison below).
      PrepareEagerCheckpoint(stmt->EntryId());
      Node* exit_cond =
          NewNode(javascript()->LessThan(CompareOperationHint::kSignedSmall),
                  index, cache_length);
      PrepareFrameState(exit_cond, BailoutId::None());
      for_loop.BreakUnless(exit_cond);

      // Compute the next enumerated value.
      Node* value = NewNode(javascript()->ForInNext(), object, cache_array,
                            cache_type, index);
      PrepareFrameState(value, stmt->FilterId(),
                        OutputFrameStateCombine::Push());
      IfBuilder test_value(this);
      Node* test_value_cond =
          NewNode(javascript()->StrictEqual(CompareOperationHint::kAny), value,
                  jsgraph()->UndefinedConstant());
      test_value.If(test_value_cond, BranchHint::kFalse);
      test_value.Then();
      test_value.Else();
      {
        environment()->Push(value);
        PrepareEagerCheckpoint(stmt->FilterId());
        value = environment()->Pop();
        // Bind value and do loop body.
        VectorSlotPair feedback =
            CreateVectorSlotPair(stmt->EachFeedbackSlot());
        VisitForInAssignment(stmt->each(), value, feedback,
                             stmt->AssignmentId());
        VisitIterationBody(stmt, &for_loop, stmt->StackCheckId());
      }
      test_value.End();
      for_loop.EndBody();

      // Increment counter and continue (we know that the {index} is always
      // in Smi range, so we can just set the hint on the increment below).
      index = environment()->Peek(0);
      PrepareEagerCheckpoint(stmt->IncrementId());
      index = NewNode(javascript()->Add(BinaryOperationHint::kSignedSmall),
                      index, jsgraph()->OneConstant());
      PrepareFrameState(index, BailoutId::None());
      environment()->Poke(0, index);
    }
    for_loop.EndLoop();
    environment()->Drop(5);
  }
  for_block.EndBlock();
}


void AstGraphBuilder::VisitForOfStatement(ForOfStatement* stmt) {
  LoopBuilder for_loop(this);
  VisitForEffect(stmt->assign_iterator());
  for_loop.BeginLoop(GetVariablesAssignedInLoop(stmt), CheckOsrEntry(stmt));
  VisitForEffect(stmt->next_result());
  VisitForTest(stmt->result_done());
  Node* condition = environment()->Pop();
  for_loop.BreakWhen(condition);
  VisitForEffect(stmt->assign_each());
  VisitIterationBody(stmt, &for_loop, stmt->StackCheckId());
  for_loop.EndBody();
  for_loop.EndLoop();
}


void AstGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  TryCatchBuilder try_control(this);

  // Evaluate the try-block inside a control scope. This simulates a handler
  // that is intercepting 'throw' control commands.
  try_control.BeginTry();
  {
    ControlScopeForCatch scope(this, stmt, &try_control);
    STATIC_ASSERT(TryBlockConstant::kElementCount == 1);
    environment()->Push(current_context());
    Visit(stmt->try_block());
    environment()->Pop();
  }
  try_control.EndTry();

  // If requested, clear message object as we enter the catch block.
  if (stmt->clear_pending_message()) {
    Node* the_hole = jsgraph()->TheHoleConstant();
    NewNode(javascript()->StoreMessage(), the_hole);
  }

  // Create a catch scope that binds the exception.
  Node* exception = try_control.GetExceptionNode();
  Handle<String> name = stmt->variable()->name();
  Handle<ScopeInfo> scope_info = stmt->scope()->scope_info();
  const Operator* op = javascript()->CreateCatchContext(name, scope_info);
  Node* context = NewNode(op, exception, GetFunctionClosureForContext());

  // Evaluate the catch-block.
  VisitInScope(stmt->catch_block(), stmt->scope(), context);
  try_control.EndCatch();
}


void AstGraphBuilder::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  TryFinallyBuilder try_control(this);

  // We keep a record of all paths that enter the finally-block to be able to
  // dispatch to the correct continuation point after the statements in the
  // finally-block have been evaluated.
  //
  // The try-finally construct can enter the finally-block in three ways:
  // 1. By exiting the try-block normally, falling through at the end.
  // 2. By exiting the try-block with a function-local control flow transfer
  //    (i.e. through break/continue/return statements).
  // 3. By exiting the try-block with a thrown exception.
  Node* fallthrough_result = jsgraph()->TheHoleConstant();
  ControlScope::DeferredCommands* commands =
      new (local_zone()) ControlScope::DeferredCommands(this);

  // Evaluate the try-block inside a control scope. This simulates a handler
  // that is intercepting all control commands.
  try_control.BeginTry();
  {
    ControlScopeForFinally scope(this, stmt, commands, &try_control);
    STATIC_ASSERT(TryBlockConstant::kElementCount == 1);
    environment()->Push(current_context());
    Visit(stmt->try_block());
    environment()->Pop();
  }
  try_control.EndTry(commands->GetFallThroughToken(), fallthrough_result);

  // The result value semantics depend on how the block was entered:
  //  - ReturnStatement: It represents the return value being returned.
  //  - ThrowStatement: It represents the exception being thrown.
  //  - BreakStatement/ContinueStatement: Filled with the hole.
  //  - Falling through into finally-block: Filled with the hole.
  Node* result = try_control.GetResultValueNode();
  Node* token = try_control.GetDispatchTokenNode();

  // The result value, dispatch token and message is expected on the operand
  // stack (this is in sync with FullCodeGenerator::EnterFinallyBlock).
  Node* message = NewNode(javascript()->LoadMessage());
  environment()->Push(token);
  environment()->Push(result);
  environment()->Push(message);

  // Clear message object as we enter the finally block.
  Node* the_hole = jsgraph()->TheHoleConstant();
  NewNode(javascript()->StoreMessage(), the_hole);

  // Evaluate the finally-block.
  Visit(stmt->finally_block());
  try_control.EndFinally();

  // The result value, dispatch token and message is restored from the operand
  // stack (this is in sync with FullCodeGenerator::ExitFinallyBlock).
  message = environment()->Pop();
  result = environment()->Pop();
  token = environment()->Pop();
  NewNode(javascript()->StoreMessage(), message);

  // Dynamic dispatch after the finally-block.
  commands->ApplyDeferredCommands(token, result);
}


void AstGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  Node* node =
      NewNode(javascript()->CallRuntime(Runtime::kHandleDebuggerStatement));
  PrepareFrameState(node, stmt->DebugBreakId());
  environment()->MarkAllLocalsLive();
}


void AstGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  // Find or build a shared function info.
  Handle<SharedFunctionInfo> shared_info =
      Compiler::GetSharedFunctionInfo(expr, info()->script(), info());
  CHECK(!shared_info.is_null());  // TODO(mstarzinger): Set stack overflow?

  // Create node to instantiate a new closure.
  PretenureFlag pretenure = expr->pretenure() ? TENURED : NOT_TENURED;
  const Operator* op = javascript()->CreateClosure(shared_info, pretenure);
  Node* value = NewNode(op);
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitClassLiteral(ClassLiteral* expr) {
  VisitForValueOrTheHole(expr->extends());
  VisitForValue(expr->constructor());

  // Create node to instantiate a new class.
  Node* constructor = environment()->Pop();
  Node* extends = environment()->Pop();
  Node* start = jsgraph()->Constant(expr->start_position());
  Node* end = jsgraph()->Constant(expr->end_position());
  const Operator* opc = javascript()->CallRuntime(Runtime::kDefineClass);
  Node* literal = NewNode(opc, extends, constructor, start, end);
  PrepareFrameState(literal, expr->CreateLiteralId(),
                    OutputFrameStateCombine::Push());
  environment()->Push(literal);

  // Load the "prototype" from the constructor.
  PrepareEagerCheckpoint(expr->CreateLiteralId());
  Handle<Name> name = isolate()->factory()->prototype_string();
  VectorSlotPair pair = CreateVectorSlotPair(expr->PrototypeSlot());
  Node* prototype = BuildNamedLoad(literal, name, pair);
  PrepareFrameState(prototype, expr->PrototypeId(),
                    OutputFrameStateCombine::Push());
  environment()->Push(prototype);

  // Create nodes to store method values into the literal.
  for (int i = 0; i < expr->properties()->length(); i++) {
    ClassLiteral::Property* property = expr->properties()->at(i);
    environment()->Push(environment()->Peek(property->is_static() ? 1 : 0));

    VisitForValue(property->key());
    Node* name = BuildToName(environment()->Pop(), expr->GetIdForProperty(i));
    environment()->Push(name);

    // The static prototype property is read only. We handle the non computed
    // property name case in the parser. Since this is the only case where we
    // need to check for an own read only property we special case this so we do
    // not need to do this for every property.
    if (property->is_static() && property->is_computed_name()) {
      Node* check = BuildThrowIfStaticPrototype(environment()->Pop(),
                                                expr->GetIdForProperty(i));
      environment()->Push(check);
    }

    VisitForValue(property->value());
    Node* value = environment()->Pop();
    Node* key = environment()->Pop();
    Node* receiver = environment()->Pop();

    BuildSetHomeObject(value, receiver, property);

    switch (property->kind()) {
      case ClassLiteral::Property::METHOD: {
        Node* attr = jsgraph()->Constant(DONT_ENUM);
        Node* set_function_name =
            jsgraph()->Constant(property->NeedsSetFunctionName());
        const Operator* op =
            javascript()->CallRuntime(Runtime::kDefineDataPropertyInLiteral);
        Node* call = NewNode(op, receiver, key, value, attr, set_function_name);
        PrepareFrameState(call, BailoutId::None());
        break;
      }
      case ClassLiteral::Property::GETTER: {
        Node* attr = jsgraph()->Constant(DONT_ENUM);
        const Operator* op = javascript()->CallRuntime(
            Runtime::kDefineGetterPropertyUnchecked, 4);
        NewNode(op, receiver, key, value, attr);
        break;
      }
      case ClassLiteral::Property::SETTER: {
        Node* attr = jsgraph()->Constant(DONT_ENUM);
        const Operator* op = javascript()->CallRuntime(
            Runtime::kDefineSetterPropertyUnchecked, 4);
        NewNode(op, receiver, key, value, attr);
        break;
      }
      case ClassLiteral::Property::FIELD: {
        UNREACHABLE();
        break;
      }
    }
  }

  // Set the constructor to have fast properties.
  prototype = environment()->Pop();
  literal = environment()->Pop();
  const Operator* op = javascript()->CallRuntime(Runtime::kToFastProperties);
  literal = NewNode(op, literal);

  // Assign to class variable.
  if (expr->class_variable_proxy() != nullptr) {
    Variable* var = expr->class_variable_proxy()->var();
    VectorSlotPair feedback = CreateVectorSlotPair(
        expr->NeedsProxySlot() ? expr->ProxySlot()
                               : FeedbackVectorSlot::Invalid());
    BuildVariableAssignment(var, literal, Token::INIT, feedback,
                            BailoutId::None());
  }
  ast_context()->ProduceValue(expr, literal);
}


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
      expr->pattern(), expr->flags(), expr->literal_index());
  Node* literal = NewNode(op, closure);
  PrepareFrameState(literal, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(expr, literal);
}


void AstGraphBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to deep-copy the literal boilerplate.
  const Operator* op = javascript()->CreateLiteralObject(
      expr->constant_properties(), expr->ComputeFlags(true),
      expr->literal_index(), expr->properties_count());
  Node* literal = NewNode(op, closure);
  PrepareFrameState(literal, expr->CreateLiteralId(),
                    OutputFrameStateCombine::Push());

  // The object is expected on the operand stack during computation of the
  // property values and is the value of the entire expression.
  environment()->Push(literal);

  // Create nodes to store computed values into the literal.
  int property_index = 0;
  AccessorTable accessor_table(local_zone());
  for (; property_index < expr->properties()->length(); property_index++) {
    ObjectLiteral::Property* property = expr->properties()->at(property_index);
    if (property->is_computed_name()) break;
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key()->AsLiteral();
    switch (property->kind()) {
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
            Node* store = BuildNamedStore(literal, name, value, feedback);
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
        PrepareFrameState(set_prototype,
                          expr->GetIdForPropertySet(property_index));
        break;
      }
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(property_index);
          it->second->getter = property;
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(property_index);
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

  // Object literals have two parts. The "static" part on the left contains no
  // computed property names, and so we can compute its map ahead of time; see
  // Runtime_CreateObjectLiteralBoilerplate. The second "dynamic" part starts
  // with the first computed property name and continues with all properties to
  // its right. All the code from above initializes the static component of the
  // object literal, and arranges for the map of the result to reflect the
  // static order in which the keys appear. For the dynamic properties, we
  // compile them into a series of "SetOwnProperty" runtime calls. This will
  // preserve insertion order.
  for (; property_index < expr->properties()->length(); property_index++) {
    ObjectLiteral::Property* property = expr->properties()->at(property_index);

    if (property->kind() == ObjectLiteral::Property::PROTOTYPE) {
      environment()->Push(environment()->Top());  // Duplicate receiver.
      VisitForValue(property->value());
      Node* value = environment()->Pop();
      Node* receiver = environment()->Pop();
      const Operator* op =
          javascript()->CallRuntime(Runtime::kInternalSetPrototype);
      Node* call = NewNode(op, receiver, value);
      PrepareFrameState(call, expr->GetIdForPropertySet(property_index));
      continue;
    }

    environment()->Push(environment()->Top());  // Duplicate receiver.
    VisitForValue(property->key());
    Node* name = BuildToName(environment()->Pop(),
                             expr->GetIdForPropertyName(property_index));
    environment()->Push(name);
    VisitForValue(property->value());
    Node* value = environment()->Pop();
    Node* key = environment()->Pop();
    Node* receiver = environment()->Pop();
    BuildSetHomeObject(value, receiver, property);
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::COMPUTED:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL: {
        if (!property->emit_store()) continue;
        Node* attr = jsgraph()->Constant(NONE);
        Node* set_function_name =
            jsgraph()->Constant(property->NeedsSetFunctionName());
        const Operator* op =
            javascript()->CallRuntime(Runtime::kDefineDataPropertyInLiteral);
        Node* call = NewNode(op, receiver, key, value, attr, set_function_name);
        PrepareFrameState(call, expr->GetIdForPropertySet(property_index));
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE:
        UNREACHABLE();  // Handled specially above.
        break;
      case ObjectLiteral::Property::GETTER: {
        Node* attr = jsgraph()->Constant(NONE);
        const Operator* op = javascript()->CallRuntime(
            Runtime::kDefineGetterPropertyUnchecked, 4);
        Node* call = NewNode(op, receiver, key, value, attr);
        PrepareFrameState(call, BailoutId::None());
        break;
      }
      case ObjectLiteral::Property::SETTER: {
        Node* attr = jsgraph()->Constant(NONE);
        const Operator* op = javascript()->CallRuntime(
            Runtime::kDefineSetterPropertyUnchecked, 4);
        Node* call = NewNode(op, receiver, key, value, attr);
        PrepareFrameState(call, BailoutId::None());
        break;
      }
    }
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
      expr->constant_elements(), expr->ComputeFlags(true),
      expr->literal_index(), expr->values()->length());
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

void AstGraphBuilder::VisitForInAssignment(Expression* expr, Node* value,
                                           const VectorSlotPair& feedback,
                                           BailoutId bailout_id) {
  DCHECK(expr->IsValidReferenceExpressionOrThis());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  // Evaluate LHS expression and store the value.
  switch (assign_type) {
    case VARIABLE: {
      Variable* var = expr->AsVariableProxy()->var();
      BuildVariableAssignment(var, value, Token::ASSIGN, feedback, bailout_id);
      break;
    }
    case NAMED_PROPERTY: {
      environment()->Push(value);
      VisitForValue(property->obj());
      Node* object = environment()->Pop();
      value = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store = BuildNamedStore(object, name, value, feedback);
      PrepareFrameState(store, bailout_id, OutputFrameStateCombine::Ignore());
      break;
    }
    case KEYED_PROPERTY: {
      environment()->Push(value);
      VisitForValue(property->obj());
      VisitForValue(property->key());
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      value = environment()->Pop();
      Node* store = BuildKeyedStore(object, key, value, feedback);
      PrepareFrameState(store, bailout_id, OutputFrameStateCombine::Ignore());
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      environment()->Push(value);
      VisitForValue(property->obj()->AsSuperPropertyReference()->this_var());
      VisitForValue(property->obj()->AsSuperPropertyReference()->home_object());
      Node* home_object = environment()->Pop();
      Node* receiver = environment()->Pop();
      value = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store = BuildNamedSuperStore(receiver, home_object, name, value);
      PrepareFrameState(store, bailout_id, OutputFrameStateCombine::Ignore());
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      environment()->Push(value);
      VisitForValue(property->obj()->AsSuperPropertyReference()->this_var());
      VisitForValue(property->obj()->AsSuperPropertyReference()->home_object());
      VisitForValue(property->key());
      Node* key = environment()->Pop();
      Node* home_object = environment()->Pop();
      Node* receiver = environment()->Pop();
      value = environment()->Pop();
      Node* store = BuildKeyedSuperStore(receiver, home_object, key, value);
      PrepareFrameState(store, bailout_id, OutputFrameStateCombine::Ignore());
      break;
    }
  }
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
      VisitForValue(property->obj()->AsSuperPropertyReference()->this_var());
      VisitForValue(property->obj()->AsSuperPropertyReference()->home_object());
      break;
    case KEYED_SUPER_PROPERTY:
      VisitForValue(property->obj()->AsSuperPropertyReference()->this_var());
      VisitForValue(property->obj()->AsSuperPropertyReference()->home_object());
      VisitForValue(property->key());
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
      case NAMED_SUPER_PROPERTY: {
        Node* home_object = environment()->Top();
        Node* receiver = environment()->Peek(1);
        Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
        VectorSlotPair pair =
            CreateVectorSlotPair(property->PropertyFeedbackSlot());
        old_value = BuildNamedSuperLoad(receiver, home_object, name, pair);
        PrepareFrameState(old_value, property->LoadId(),
                          OutputFrameStateCombine::Push());
        break;
      }
      case KEYED_SUPER_PROPERTY: {
        Node* key = environment()->Top();
        Node* home_object = environment()->Peek(1);
        Node* receiver = environment()->Peek(2);
        VectorSlotPair pair =
            CreateVectorSlotPair(property->PropertyFeedbackSlot());
        old_value = BuildKeyedSuperLoad(receiver, home_object, key, pair);
        PrepareFrameState(old_value, property->LoadId(),
                          OutputFrameStateCombine::Push());
        break;
      }
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
    case NAMED_SUPER_PROPERTY: {
      Node* home_object = environment()->Pop();
      Node* receiver = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store = BuildNamedSuperStore(receiver, home_object, name, value);
      PrepareFrameState(store, expr->id(), ast_context()->GetStateCombine());
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      Node* key = environment()->Pop();
      Node* home_object = environment()->Pop();
      Node* receiver = environment()->Pop();
      Node* store = BuildKeyedSuperStore(receiver, home_object, key, value);
      PrepareFrameState(store, expr->id(), ast_context()->GetStateCombine());
      break;
    }
  }

  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitYield(Yield* expr) {
  // Generator functions are supported only by going through Ignition first.
  SetStackOverflow();
  ast_context()->ProduceValue(expr, jsgraph()->UndefinedConstant());
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
    case NAMED_SUPER_PROPERTY: {
      VisitForValue(expr->obj()->AsSuperPropertyReference()->this_var());
      VisitForValue(expr->obj()->AsSuperPropertyReference()->home_object());
      Node* home_object = environment()->Pop();
      Node* receiver = environment()->Pop();
      Handle<Name> name = expr->key()->AsLiteral()->AsPropertyName();
      value = BuildNamedSuperLoad(receiver, home_object, name, pair);
      PrepareFrameState(value, expr->LoadId(), OutputFrameStateCombine::Push());
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      VisitForValue(expr->obj()->AsSuperPropertyReference()->this_var());
      VisitForValue(expr->obj()->AsSuperPropertyReference()->home_object());
      VisitForValue(expr->key());
      Node* key = environment()->Pop();
      Node* home_object = environment()->Pop();
      Node* receiver = environment()->Pop();
      value = BuildKeyedSuperLoad(receiver, home_object, key, pair);
      PrepareFrameState(value, expr->LoadId(), OutputFrameStateCombine::Push());
      break;
    }
  }
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitCall(Call* expr) {
  Expression* callee = expr->expression();
  Call::CallType call_type = expr->GetCallType();

  // Prepare the callee and the receiver to the function call. This depends on
  // the semantics of the underlying call type.
  ConvertReceiverMode receiver_hint = ConvertReceiverMode::kAny;
  Node* receiver_value = nullptr;
  Node* callee_value = nullptr;
  bool possibly_eval = false;
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
    case Call::LOOKUP_SLOT_CALL: {
      Variable* variable = callee->AsVariableProxy()->var();
      DCHECK(variable->location() == VariableLocation::LOOKUP);
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op =
          javascript()->CallRuntime(Runtime::kLoadLookupSlotForCall);
      Node* pair = NewNode(op, name);
      callee_value = NewNode(common()->Projection(0), pair);
      receiver_value = NewNode(common()->Projection(1), pair);
      PrepareFrameState(pair, expr->LookupId(),
                        OutputFrameStateCombine::Push(2));
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
    case Call::NAMED_SUPER_PROPERTY_CALL: {
      Property* property = callee->AsProperty();
      SuperPropertyReference* super_ref =
          property->obj()->AsSuperPropertyReference();
      VisitForValue(super_ref->home_object());
      VisitForValue(super_ref->this_var());
      Node* home = environment()->Peek(1);
      Node* object = environment()->Top();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      callee_value = BuildNamedSuperLoad(object, home, name, VectorSlotPair());
      PrepareFrameState(callee_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      // Note that a property call requires the receiver to be wrapped into
      // an object for sloppy callees. Since the receiver is not the target of
      // the load, it could very well be null or undefined at this point.
      receiver_value = environment()->Pop();
      environment()->Drop(1);
      break;
    }
    case Call::KEYED_SUPER_PROPERTY_CALL: {
      Property* property = callee->AsProperty();
      SuperPropertyReference* super_ref =
          property->obj()->AsSuperPropertyReference();
      VisitForValue(super_ref->home_object());
      VisitForValue(super_ref->this_var());
      environment()->Push(environment()->Top());    // Duplicate this_var.
      environment()->Push(environment()->Peek(2));  // Duplicate home_obj.
      VisitForValue(property->key());
      Node* key = environment()->Pop();
      Node* home = environment()->Pop();
      Node* object = environment()->Pop();
      callee_value = BuildKeyedSuperLoad(object, home, key, VectorSlotPair());
      PrepareFrameState(callee_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      // Note that a property call requires the receiver to be wrapped into
      // an object for sloppy callees. Since the receiver is not the target of
      // the load, it could very well be null or undefined at this point.
      receiver_value = environment()->Pop();
      environment()->Drop(1);
      break;
    }
    case Call::SUPER_CALL:
      return VisitCallSuper(expr);
    case Call::POSSIBLY_EVAL_CALL:
      possibly_eval = true;
      if (callee->AsVariableProxy()->var()->IsLookupSlot()) {
        Variable* variable = callee->AsVariableProxy()->var();
        Node* name = jsgraph()->Constant(variable->name());
        const Operator* op =
            javascript()->CallRuntime(Runtime::kLoadLookupSlotForCall);
        Node* pair = NewNode(op, name);
        callee_value = NewNode(common()->Projection(0), pair);
        receiver_value = NewNode(common()->Projection(1), pair);
        PrepareFrameState(pair, expr->LookupId(),
                          OutputFrameStateCombine::Push(2));
        break;
      }
    // Fall through.
    case Call::OTHER_CALL:
      VisitForValue(callee);
      callee_value = environment()->Pop();
      receiver_hint = ConvertReceiverMode::kNullOrUndefined;
      receiver_value = jsgraph()->UndefinedConstant();
      break;
  }

  // The callee and the receiver both have to be pushed onto the operand stack
  // before arguments are being evaluated.
  environment()->Push(callee_value);
  environment()->Push(receiver_value);

  // Evaluate all arguments to the function call,
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Resolve callee for a potential direct eval call. This block will mutate the
  // callee value pushed onto the environment.
  if (possibly_eval && args->length() > 0) {
    int arg_count = args->length();

    // Extract callee and source string from the environment.
    Node* callee = environment()->Peek(arg_count + 1);
    Node* source = environment()->Peek(arg_count - 1);

    // Create node to ask for help resolving potential eval call. This will
    // provide a fully resolved callee to patch into the environment.
    Node* function = GetFunctionClosure();
    Node* language = jsgraph()->Constant(language_mode());
    Node* eval_scope_position =
        jsgraph()->Constant(current_scope()->start_position());
    Node* eval_position = jsgraph()->Constant(expr->position());
    const Operator* op =
        javascript()->CallRuntime(Runtime::kResolvePossiblyDirectEval);
    Node* new_callee = NewNode(op, callee, source, function, language,
                               eval_scope_position, eval_position);
    PrepareFrameState(new_callee, expr->EvalId(),
                      OutputFrameStateCombine::PokeAt(arg_count + 1));

    // Patch callee on the environment.
    environment()->Poke(arg_count + 1, new_callee);
  }

  // Create node to perform the function call.
  float const frequency = ComputeCallFrequency(expr->CallFeedbackICSlot());
  VectorSlotPair feedback = CreateVectorSlotPair(expr->CallFeedbackICSlot());
  const Operator* call =
      javascript()->CallFunction(args->length() + 2, frequency, feedback,
                                 receiver_hint, expr->tail_call_mode());
  PrepareEagerCheckpoint(possibly_eval ? expr->EvalId() : expr->CallId());
  Node* value = ProcessArguments(call, args->length() + 2);
  // The callee passed to the call, we just need to push something here to
  // satisfy the bailout location contract. The fullcodegen code will not
  // ever look at this value, so we just push optimized_out here.
  environment()->Push(jsgraph()->OptimizedOutConstant());
  PrepareFrameState(value, expr->ReturnId(), OutputFrameStateCombine::Push());
  environment()->Drop(1);
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitCallSuper(Call* expr) {
  SuperCallReference* super = expr->expression()->AsSuperCallReference();
  DCHECK_NOT_NULL(super);

  // Prepare the callee to the super call.
  VisitForValue(super->this_function_var());
  Node* this_function = environment()->Pop();
  const Operator* op =
      javascript()->CallRuntime(Runtime::kInlineGetSuperConstructor, 1);
  Node* super_function = NewNode(op, this_function);
  environment()->Push(super_function);

  // Evaluate all arguments to the super call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // The new target is loaded from the {new.target} variable.
  VisitForValue(super->new_target_var());

  // Create node to perform the super call.
  const Operator* call =
      javascript()->CallConstruct(args->length() + 2, 0.0f, VectorSlotPair());
  Node* value = ProcessArguments(call, args->length() + 2);
  PrepareFrameState(value, expr->ReturnId(), OutputFrameStateCombine::Push());
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
  float const frequency = ComputeCallFrequency(expr->CallNewFeedbackSlot());
  VectorSlotPair feedback = CreateVectorSlotPair(expr->CallNewFeedbackSlot());
  const Operator* call =
      javascript()->CallConstruct(args->length() + 2, frequency, feedback);
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
  const Operator* call = javascript()->CallFunction(args->length() + 2);
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
    case NAMED_SUPER_PROPERTY: {
      VisitForValue(property->obj()->AsSuperPropertyReference()->this_var());
      VisitForValue(property->obj()->AsSuperPropertyReference()->home_object());
      Node* home_object = environment()->Top();
      Node* receiver = environment()->Peek(1);
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      VectorSlotPair pair =
          CreateVectorSlotPair(property->PropertyFeedbackSlot());
      old_value = BuildNamedSuperLoad(receiver, home_object, name, pair);
      PrepareFrameState(old_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      stack_depth = 2;
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      VisitForValue(property->obj()->AsSuperPropertyReference()->this_var());
      VisitForValue(property->obj()->AsSuperPropertyReference()->home_object());
      VisitForValue(property->key());
      Node* key = environment()->Top();
      Node* home_object = environment()->Peek(1);
      Node* receiver = environment()->Peek(2);
      VectorSlotPair pair =
          CreateVectorSlotPair(property->PropertyFeedbackSlot());
      old_value = BuildKeyedSuperLoad(receiver, home_object, key, pair);
      PrepareFrameState(old_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      stack_depth = 3;
      break;
    }
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
    case NAMED_SUPER_PROPERTY: {
      Node* home_object = environment()->Pop();
      Node* receiver = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store = BuildNamedSuperStore(receiver, home_object, name, value);
      PrepareFrameState(store, expr->AssignmentId(),
                        OutputFrameStateCombine::Push());
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      Node* key = environment()->Pop();
      Node* home_object = environment()->Pop();
      Node* receiver = environment()->Pop();
      Node* store = BuildKeyedSuperStore(receiver, home_object, key, value);
      PrepareFrameState(store, expr->AssignmentId(),
                        OutputFrameStateCombine::Push());
      break;
    }
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
  Handle<String> check;
  if (expr->IsLiteralCompareTypeof(&sub_expr, &check)) {
    return VisitLiteralCompareTypeof(expr, sub_expr, check);
  }
  if (expr->IsLiteralCompareUndefined(&sub_expr)) {
    return VisitLiteralCompareNil(expr, sub_expr,
                                  jsgraph()->UndefinedConstant());
  }
  if (expr->IsLiteralCompareNull(&sub_expr)) {
    return VisitLiteralCompareNil(expr, sub_expr, jsgraph()->NullConstant());
  }

  CompareOperationHint hint;
  if (!type_hint_analysis_ ||
      !type_hint_analysis_->GetCompareOperationHint(
          expr->CompareOperationFeedbackId(), &hint)) {
    hint = CompareOperationHint::kAny;
  }

  const Operator* op;
  switch (expr->op()) {
    case Token::EQ:
      op = javascript()->Equal(hint);
      break;
    case Token::NE:
      op = javascript()->NotEqual(hint);
      break;
    case Token::EQ_STRICT:
      op = javascript()->StrictEqual(hint);
      break;
    case Token::NE_STRICT:
      op = javascript()->StrictNotEqual(hint);
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


void AstGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  Node* value = GetFunctionClosure();
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitSuperPropertyReference(
    SuperPropertyReference* expr) {
  Node* value = BuildThrowUnsupportedSuperError(expr->id());
  ast_context()->ProduceValue(expr, value);
}


void AstGraphBuilder::VisitSuperCallReference(SuperCallReference* expr) {
  // Handled by VisitCall
  UNREACHABLE();
}


void AstGraphBuilder::VisitCaseClause(CaseClause* expr) {
  // Handled entirely in VisitSwitch.
  UNREACHABLE();
}


void AstGraphBuilder::VisitDeclarations(ZoneList<Declaration*>* declarations) {
  DCHECK(globals()->empty());
  AstVisitor<AstGraphBuilder>::VisitDeclarations(declarations);
  if (globals()->empty()) return;
  int array_index = 0;
  Handle<TypeFeedbackVector> feedback_vector(
      info()->closure()->feedback_vector());
  Handle<FixedArray> data = isolate()->factory()->NewFixedArray(
      static_cast<int>(globals()->size()), TENURED);
  for (Handle<Object> obj : *globals()) data->set(array_index++, *obj);
  int encoded_flags = info()->GetDeclareGlobalsFlags();
  Node* flags = jsgraph()->Constant(encoded_flags);
  Node* pairs = jsgraph()->Constant(data);
  Node* vector = jsgraph()->Constant(feedback_vector);
  const Operator* op = javascript()->CallRuntime(Runtime::kDeclareGlobals);
  Node* call = NewNode(op, pairs, flags, vector);
  PrepareFrameState(call, BailoutId::Declarations());
  globals()->clear();
}


void AstGraphBuilder::VisitIfNotNull(Statement* stmt) {
  if (stmt == nullptr) return;
  Visit(stmt);
}


void AstGraphBuilder::VisitInScope(Statement* stmt, Scope* s, Node* context) {
  ContextScope scope(this, s, context);
  DCHECK(s->declarations()->is_empty());
  Visit(stmt);
}

void AstGraphBuilder::VisitIterationBody(IterationStatement* stmt,
                                         LoopBuilder* loop,
                                         BailoutId stack_check_id) {
  ControlScopeForIteration scope(this, stmt, loop);
  if (FLAG_turbo_loop_stackcheck || !info()->shared_info()->asm_function()) {
    Node* node = NewNode(javascript()->StackCheck());
    PrepareFrameState(node, stack_check_id);
  }
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
    value = NewNode(javascript()->DeleteProperty(language_mode()), object, key);
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


VectorSlotPair AstGraphBuilder::CreateVectorSlotPair(
    FeedbackVectorSlot slot) const {
  return VectorSlotPair(handle(info()->closure()->feedback_vector()), slot);
}


void AstGraphBuilder::VisitRewritableExpression(RewritableExpression* node) {
  Visit(node->expression());
}


namespace {

// Limit of context chain length to which inline check is possible.
const int kMaxCheckDepth = 30;

// Sentinel for {TryLoadDynamicVariable} disabling inline checks.
const uint32_t kFullCheckRequired = -1;

}  // namespace


uint32_t AstGraphBuilder::ComputeBitsetForDynamicGlobal(Variable* variable) {
  DCHECK_EQ(DYNAMIC_GLOBAL, variable->mode());
  uint32_t check_depths = 0;
  for (Scope* s = current_scope(); s != nullptr; s = s->outer_scope()) {
    if (!s->NeedsContext()) continue;
    if (!s->calls_sloppy_eval()) continue;
    int depth = current_scope()->ContextChainLength(s);
    if (depth > kMaxCheckDepth) return kFullCheckRequired;
    check_depths |= 1 << depth;
  }
  return check_depths;
}


uint32_t AstGraphBuilder::ComputeBitsetForDynamicContext(Variable* variable) {
  DCHECK_EQ(DYNAMIC_LOCAL, variable->mode());
  uint32_t check_depths = 0;
  for (Scope* s = current_scope(); s != nullptr; s = s->outer_scope()) {
    if (!s->NeedsContext()) continue;
    if (!s->calls_sloppy_eval() && s != variable->scope()) continue;
    int depth = current_scope()->ContextChainLength(s);
    if (depth > kMaxCheckDepth) return kFullCheckRequired;
    check_depths |= 1 << depth;
    if (s == variable->scope()) break;
  }
  return check_depths;
}

float AstGraphBuilder::ComputeCallFrequency(FeedbackVectorSlot slot) const {
  if (slot.IsInvalid()) return 0.0f;
  Handle<TypeFeedbackVector> feedback_vector(
      info()->closure()->feedback_vector(), isolate());
  CallICNexus nexus(feedback_vector, slot);
  return nexus.ComputeCallFrequency() * invocation_frequency_;
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
    NewNode(op, local_context, receiver);
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
    NewNode(op, local_context, parameter);
  }

  return local_context;
}


Node* AstGraphBuilder::BuildLocalFunctionContext(Scope* scope) {
  DCHECK(scope->is_function_scope() || scope->is_eval_scope());

  // Allocate a new local context.
  int slot_count = scope->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  const Operator* op = javascript()->CreateFunctionContext(slot_count);
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

Node* AstGraphBuilder::BuildRestArgumentsArray(Variable* rest) {
  if (rest == nullptr) return nullptr;

  // Allocate and initialize a new arguments object.
  CreateArgumentsType type = CreateArgumentsType::kRestParameter;
  const Operator* op = javascript()->CreateArguments(type);
  Node* object = NewNode(op, GetFunctionClosure());
  PrepareFrameState(object, BailoutId::None());

  // Assign the object to the {rest} variable. This should never lazy
  // deopt, so it is fine to send invalid bailout id.
  DCHECK(rest->IsContextSlot() || rest->IsStackAllocated());
  BuildVariableAssignment(rest, object, Token::ASSIGN, VectorSlotPair(),
                          BailoutId::None());
  return object;
}


Node* AstGraphBuilder::BuildThisFunctionVariable(Variable* this_function_var) {
  if (this_function_var == nullptr) return nullptr;

  // Retrieve the closure we were called with.
  Node* this_function = GetFunctionClosure();

  // Assign the object to the {.this_function} variable. This should never lazy
  // deopt, so it is fine to send invalid bailout id.
  BuildVariableAssignment(this_function_var, this_function, Token::INIT,
                          VectorSlotPair(), BailoutId::None());
  return this_function;
}


Node* AstGraphBuilder::BuildNewTargetVariable(Variable* new_target_var) {
  if (new_target_var == nullptr) return nullptr;

  // Retrieve the new target we were called with.
  Node* object = GetNewTarget();

  // Assign the object to the {new.target} variable. This should never lazy
  // deopt, so it is fine to send invalid bailout id.
  BuildVariableAssignment(new_target_var, object, Token::INIT, VectorSlotPair(),
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


Node* AstGraphBuilder::BuildThrowIfStaticPrototype(Node* name,
                                                   BailoutId bailout_id) {
  IfBuilder prototype_check(this);
  Node* prototype_string =
      jsgraph()->Constant(isolate()->factory()->prototype_string());
  Node* check = NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                        name, prototype_string);
  prototype_check.If(check);
  prototype_check.Then();
  Node* error = BuildThrowStaticPrototypeError(bailout_id);
  environment()->Push(error);
  prototype_check.Else();
  environment()->Push(name);
  prototype_check.End();
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
      bool immutable = variable->maybe_assigned() == kNotAssigned;
      const Operator* op =
          javascript()->LoadContext(depth, variable->index(), immutable);
      Node* value = NewNode(op, current_context());
      // TODO(titzer): initialization checks are redundant for already
      // initialized immutable context loads, but only specialization knows.
      // Maybe specializer should be a parameter to the graph builder?
      if (variable->binding_needs_init()) {
        // Perform check for uninitialized let/const variables.
        value = BuildHoleCheckThenThrow(value, variable, value, bailout_id);
      }
      return value;
    }
    case VariableLocation::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Handle<String> name = variable->name();
      if (Node* node = TryLoadDynamicVariable(variable, name, bailout_id,
                                              feedback, combine, typeof_mode)) {
        return node;
      }
      Node* value = BuildDynamicLoad(name, typeof_mode);
      PrepareFrameState(value, bailout_id, combine);
      return value;
    }
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
      const Operator* op = javascript()->DeleteProperty(language_mode());
      Node* result = NewNode(op, global, name);
      PrepareFrameState(result, bailout_id, combine);
      return result;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
    case VariableLocation::CONTEXT: {
      // Local var, const, or let variable or context variable.
      return jsgraph()->BooleanConstant(variable->is_this());
    }
    case VariableLocation::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op =
          javascript()->CallRuntime(Runtime::kDeleteLookupSlot);
      Node* result = NewNode(op, name);
      PrepareFrameState(result, bailout_id, combine);
      return result;
    }
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
        Node* current = NewNode(op, current_context());
        value = BuildHoleCheckThenThrow(current, variable, value, bailout_id);
      } else if (mode == CONST && op == Token::INIT) {
        // Perform an initialization check for const {this} variables.
        // Note that the {this} variable is the only const variable being able
        // to trigger bind operations outside the TDZ, via {super} calls.
        if (variable->is_this()) {
          const Operator* op =
              javascript()->LoadContext(depth, variable->index(), false);
          Node* current = NewNode(op, current_context());
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
          Node* current = NewNode(op, current_context());
          BuildHoleCheckThenThrow(current, variable, value, bailout_id);
        }
        // Assignment to const is exception in all modes.
        return BuildThrowConstAssignError(bailout_id);
      }
      const Operator* op = javascript()->StoreContext(depth, variable->index());
      return NewNode(op, current_context(), value);
    }
    case VariableLocation::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Handle<Name> name = variable->name();
      Node* store = BuildDynamicStore(name, value);
      PrepareFrameState(store, bailout_id, combine);
      return store;
    }
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
  UNREACHABLE();
  return nullptr;
}


Node* AstGraphBuilder::BuildKeyedLoad(Node* object, Node* key,
                                      const VectorSlotPair& feedback) {
  const Operator* op = javascript()->LoadProperty(feedback);
  Node* node = NewNode(op, object, key, GetFunctionClosure());
  return node;
}


Node* AstGraphBuilder::BuildNamedLoad(Node* object, Handle<Name> name,
                                      const VectorSlotPair& feedback) {
  const Operator* op = javascript()->LoadNamed(name, feedback);
  Node* node = NewNode(op, object, GetFunctionClosure());
  return node;
}


Node* AstGraphBuilder::BuildKeyedStore(Node* object, Node* key, Node* value,
                                       const VectorSlotPair& feedback) {
  const Operator* op = javascript()->StoreProperty(language_mode(), feedback);
  Node* node = NewNode(op, object, key, value, GetFunctionClosure());
  return node;
}


Node* AstGraphBuilder::BuildNamedStore(Node* object, Handle<Name> name,
                                       Node* value,
                                       const VectorSlotPair& feedback) {
  const Operator* op =
      javascript()->StoreNamed(language_mode(), name, feedback);
  Node* node = NewNode(op, object, value, GetFunctionClosure());
  return node;
}


Node* AstGraphBuilder::BuildNamedSuperLoad(Node* receiver, Node* home_object,
                                           Handle<Name> name,
                                           const VectorSlotPair& feedback) {
  Node* name_node = jsgraph()->Constant(name);
  const Operator* op = javascript()->CallRuntime(Runtime::kLoadFromSuper);
  Node* node = NewNode(op, receiver, home_object, name_node);
  return node;
}


Node* AstGraphBuilder::BuildKeyedSuperLoad(Node* receiver, Node* home_object,
                                           Node* key,
                                           const VectorSlotPair& feedback) {
  const Operator* op = javascript()->CallRuntime(Runtime::kLoadKeyedFromSuper);
  Node* node = NewNode(op, receiver, home_object, key);
  return node;
}


Node* AstGraphBuilder::BuildKeyedSuperStore(Node* receiver, Node* home_object,
                                            Node* key, Node* value) {
  Runtime::FunctionId function_id = is_strict(language_mode())
                                        ? Runtime::kStoreKeyedToSuper_Strict
                                        : Runtime::kStoreKeyedToSuper_Sloppy;
  const Operator* op = javascript()->CallRuntime(function_id, 4);
  Node* node = NewNode(op, receiver, home_object, key, value);
  return node;
}


Node* AstGraphBuilder::BuildNamedSuperStore(Node* receiver, Node* home_object,
                                            Handle<Name> name, Node* value) {
  Node* name_node = jsgraph()->Constant(name);
  Runtime::FunctionId function_id = is_strict(language_mode())
                                        ? Runtime::kStoreToSuper_Strict
                                        : Runtime::kStoreToSuper_Sloppy;
  const Operator* op = javascript()->CallRuntime(function_id, 4);
  Node* node = NewNode(op, receiver, home_object, name_node, value);
  return node;
}


Node* AstGraphBuilder::BuildGlobalLoad(Handle<Name> name,
                                       const VectorSlotPair& feedback,
                                       TypeofMode typeof_mode) {
  const Operator* op = javascript()->LoadGlobal(name, feedback, typeof_mode);
  Node* node = NewNode(op, GetFunctionClosure());
  return node;
}


Node* AstGraphBuilder::BuildGlobalStore(Handle<Name> name, Node* value,
                                        const VectorSlotPair& feedback) {
  const Operator* op =
      javascript()->StoreGlobal(language_mode(), name, feedback);
  Node* node = NewNode(op, value, GetFunctionClosure());
  return node;
}


Node* AstGraphBuilder::BuildDynamicLoad(Handle<Name> name,
                                        TypeofMode typeof_mode) {
  Node* name_node = jsgraph()->Constant(name);
  const Operator* op =
      javascript()->CallRuntime(typeof_mode == TypeofMode::NOT_INSIDE_TYPEOF
                                    ? Runtime::kLoadLookupSlot
                                    : Runtime::kLoadLookupSlotInsideTypeof);
  Node* node = NewNode(op, name_node);
  return node;
}


Node* AstGraphBuilder::BuildDynamicStore(Handle<Name> name, Node* value) {
  Node* name_node = jsgraph()->Constant(name);
  const Operator* op = javascript()->CallRuntime(
      is_strict(language_mode()) ? Runtime::kStoreLookupSlot_Strict
                                 : Runtime::kStoreLookupSlot_Sloppy);
  Node* node = NewNode(op, name_node, value);
  return node;
}


Node* AstGraphBuilder::BuildLoadGlobalObject() {
  return BuildLoadNativeContextField(Context::EXTENSION_INDEX);
}


Node* AstGraphBuilder::BuildLoadNativeContextField(int index) {
  const Operator* op =
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true);
  Node* native_context = NewNode(op, current_context());
  return NewNode(javascript()->LoadContext(0, index, true), native_context);
}


Node* AstGraphBuilder::BuildToBoolean(Node* input, TypeFeedbackId feedback_id) {
  if (Node* node = TryFastToBoolean(input)) return node;
  ToBooleanHints hints;
  if (!type_hint_analysis_ ||
      !type_hint_analysis_->GetToBooleanHints(feedback_id, &hints)) {
    hints = ToBooleanHint::kAny;
  }
  return NewNode(javascript()->ToBoolean(hints), input);
}


Node* AstGraphBuilder::BuildToName(Node* input, BailoutId bailout_id) {
  if (Node* node = TryFastToName(input)) return node;
  Node* name = NewNode(javascript()->ToName(), input);
  PrepareFrameState(name, bailout_id, OutputFrameStateCombine::Push());
  return name;
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
  Node* control = NewNode(common()->Throw(), call);
  UpdateControlDependencyToLeaveFunction(control);
  return call;
}


Node* AstGraphBuilder::BuildThrowReferenceError(Variable* variable,
                                                BailoutId bailout_id) {
  Node* variable_name = jsgraph()->Constant(variable->name());
  const Operator* op = javascript()->CallRuntime(Runtime::kThrowReferenceError);
  Node* call = NewNode(op, variable_name);
  PrepareFrameState(call, bailout_id);
  Node* control = NewNode(common()->Throw(), call);
  UpdateControlDependencyToLeaveFunction(control);
  return call;
}


Node* AstGraphBuilder::BuildThrowConstAssignError(BailoutId bailout_id) {
  const Operator* op =
      javascript()->CallRuntime(Runtime::kThrowConstAssignError);
  Node* call = NewNode(op);
  PrepareFrameState(call, bailout_id);
  Node* control = NewNode(common()->Throw(), call);
  UpdateControlDependencyToLeaveFunction(control);
  return call;
}


Node* AstGraphBuilder::BuildThrowStaticPrototypeError(BailoutId bailout_id) {
  const Operator* op =
      javascript()->CallRuntime(Runtime::kThrowStaticPrototypeError);
  Node* call = NewNode(op);
  PrepareFrameState(call, bailout_id);
  Node* control = NewNode(common()->Throw(), call);
  UpdateControlDependencyToLeaveFunction(control);
  return call;
}


Node* AstGraphBuilder::BuildThrowUnsupportedSuperError(BailoutId bailout_id) {
  const Operator* op =
      javascript()->CallRuntime(Runtime::kThrowUnsupportedSuperError);
  Node* call = NewNode(op);
  PrepareFrameState(call, bailout_id);
  Node* control = NewNode(common()->Throw(), call);
  UpdateControlDependencyToLeaveFunction(control);
  return call;
}


Node* AstGraphBuilder::BuildReturn(Node* return_value) {
  // Emit tracing call if requested to do so.
  if (FLAG_trace) {
    return_value =
        NewNode(javascript()->CallRuntime(Runtime::kTraceExit), return_value);
  }
  Node* control = NewNode(common()->Return(), return_value);
  UpdateControlDependencyToLeaveFunction(control);
  return control;
}


Node* AstGraphBuilder::BuildThrow(Node* exception_value) {
  NewNode(javascript()->CallRuntime(Runtime::kReThrow), exception_value);
  Node* control = NewNode(common()->Throw(), exception_value);
  UpdateControlDependencyToLeaveFunction(control);
  return control;
}


Node* AstGraphBuilder::BuildBinaryOp(Node* left, Node* right, Token::Value op,
                                     TypeFeedbackId feedback_id) {
  const Operator* js_op;
  BinaryOperationHint hint;
  if (!type_hint_analysis_ ||
      !type_hint_analysis_->GetBinaryOperationHint(feedback_id, &hint)) {
    hint = BinaryOperationHint::kAny;
  }
  switch (op) {
    case Token::BIT_OR:
      js_op = javascript()->BitwiseOr(hint);
      break;
    case Token::BIT_AND:
      js_op = javascript()->BitwiseAnd(hint);
      break;
    case Token::BIT_XOR:
      js_op = javascript()->BitwiseXor(hint);
      break;
    case Token::SHL:
      js_op = javascript()->ShiftLeft(hint);
      break;
    case Token::SAR:
      js_op = javascript()->ShiftRight(hint);
      break;
    case Token::SHR:
      js_op = javascript()->ShiftRightLogical(hint);
      break;
    case Token::ADD:
      js_op = javascript()->Add(hint);
      break;
    case Token::SUB:
      js_op = javascript()->Subtract(hint);
      break;
    case Token::MUL:
      js_op = javascript()->Multiply(hint);
      break;
    case Token::DIV:
      js_op = javascript()->Divide(hint);
      break;
    case Token::MOD:
      js_op = javascript()->Modulus(hint);
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

Node* AstGraphBuilder::TryLoadDynamicVariable(Variable* variable,
                                              Handle<String> name,
                                              BailoutId bailout_id,
                                              const VectorSlotPair& feedback,
                                              OutputFrameStateCombine combine,
                                              TypeofMode typeof_mode) {
  VariableMode mode = variable->mode();

  if (mode == DYNAMIC_GLOBAL) {
    uint32_t bitset = ComputeBitsetForDynamicGlobal(variable);
    if (bitset == kFullCheckRequired) return nullptr;

    // We are using two blocks to model fast and slow cases.
    BlockBuilder fast_block(this);
    BlockBuilder slow_block(this);
    environment()->Push(jsgraph()->TheHoleConstant());
    slow_block.BeginBlock();
    environment()->Pop();
    fast_block.BeginBlock();

    // Perform checks whether the fast mode applies, by looking for any
    // extension object which might shadow the optimistic declaration.
    for (int depth = 0; bitset != 0; bitset >>= 1, depth++) {
      if ((bitset & 1) == 0) continue;
      Node* load = NewNode(
          javascript()->LoadContext(depth, Context::EXTENSION_INDEX, false),
          current_context());
      Node* check =
          NewNode(javascript()->StrictEqual(CompareOperationHint::kAny), load,
                  jsgraph()->TheHoleConstant());
      fast_block.BreakUnless(check, BranchHint::kTrue);
    }

    // Fast case, because variable is not shadowed.
    if (Node* constant = TryLoadGlobalConstant(name)) {
      environment()->Push(constant);
    } else {
      // Perform global slot load.
      Node* fast = BuildGlobalLoad(name, feedback, typeof_mode);
      PrepareFrameState(fast, bailout_id, combine);
      environment()->Push(fast);
    }
    slow_block.Break();
    environment()->Pop();
    fast_block.EndBlock();

    // Slow case, because variable potentially shadowed. Perform dynamic lookup.
    Node* slow = BuildDynamicLoad(name, typeof_mode);
    PrepareFrameState(slow, bailout_id, combine);
    environment()->Push(slow);
    slow_block.EndBlock();

    return environment()->Pop();
  }

  if (mode == DYNAMIC_LOCAL) {
    uint32_t bitset = ComputeBitsetForDynamicContext(variable);
    if (bitset == kFullCheckRequired) return nullptr;

    // We are using two blocks to model fast and slow cases.
    BlockBuilder fast_block(this);
    BlockBuilder slow_block(this);
    environment()->Push(jsgraph()->TheHoleConstant());
    slow_block.BeginBlock();
    environment()->Pop();
    fast_block.BeginBlock();

    // Perform checks whether the fast mode applies, by looking for any
    // extension object which might shadow the optimistic declaration.
    for (int depth = 0; bitset != 0; bitset >>= 1, depth++) {
      if ((bitset & 1) == 0) continue;
      Node* load = NewNode(
          javascript()->LoadContext(depth, Context::EXTENSION_INDEX, false),
          current_context());
      Node* check =
          NewNode(javascript()->StrictEqual(CompareOperationHint::kAny), load,
                  jsgraph()->TheHoleConstant());
      fast_block.BreakUnless(check, BranchHint::kTrue);
    }

    // Fast case, because variable is not shadowed. Perform context slot load.
    Variable* local = variable->local_if_not_shadowed();
    DCHECK(local->location() == VariableLocation::CONTEXT);  // Must be context.
    Node* fast =
        BuildVariableLoad(local, bailout_id, feedback, combine, typeof_mode);
    environment()->Push(fast);
    slow_block.Break();
    environment()->Pop();
    fast_block.EndBlock();

    // Slow case, because variable potentially shadowed. Perform dynamic lookup.
    Node* slow = BuildDynamicLoad(name, typeof_mode);
    PrepareFrameState(slow, bailout_id, combine);
    environment()->Push(slow);
    slow_block.EndBlock();

    return environment()->Pop();
  }

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
    case IrOpcode::kJSNotEqual:
    case IrOpcode::kJSStrictEqual:
    case IrOpcode::kJSStrictNotEqual:
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


Node* AstGraphBuilder::TryFastToName(Node* input) {
  switch (input->opcode()) {
    case IrOpcode::kHeapConstant: {
      Handle<HeapObject> object = HeapObjectMatcher(input).Value();
      if (object->IsName()) return input;
      break;
    }
    case IrOpcode::kJSToString:
    case IrOpcode::kJSToName:
    case IrOpcode::kJSTypeOf:
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
    bool inside_try_scope = try_nesting_level_ > 0;
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
      if (NodeProperties::IsControl(result)) {
        environment_->UpdateControlDependency(result);
      }
      // Update the current effect dependency for effect-producing nodes.
      if (result->op()->EffectOutputCount() > 0) {
        environment_->UpdateEffectDependency(result);
      }
      // Add implicit exception continuation for throwing nodes.
      if (!result->op()->HasProperty(Operator::kNoThrow) && inside_try_scope) {
        // Copy the environment for the success continuation.
        Environment* success_env = environment()->CopyForConditional();
        const Operator* op = common()->IfException();
        Node* effect = environment()->GetEffectDependency();
        Node* on_exception = graph()->NewNode(op, effect, result);
        environment_->UpdateControlDependency(on_exception);
        environment_->UpdateEffectDependency(on_exception);
        execution_control()->ThrowValue(on_exception);
        set_environment(success_env);
      }
      // Add implicit success continuation for throwing nodes.
      if (!result->op()->HasProperty(Operator::kNoThrow)) {
        const Operator* op = common()->IfSuccess();
        Node* on_success = graph()->NewNode(op, result);
        environment_->UpdateControlDependency(on_success);
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
  UpdateControlDependency(osr_loop_entry);
  UpdateEffectDependency(osr_loop_entry);
  // Set OSR values.
  for (int i = 0; i < size; ++i) {
    values()->at(i) =
        graph->NewNode(builder_->common()->OsrValue(i), osr_loop_entry);
  }

  // Set the contexts.
  // The innermost context is the OSR value, and the outer contexts are
  // reconstructed by dynamically walking up the context chain.
  Node* osr_context = nullptr;
  const Operator* op =
      builder_->javascript()->LoadContext(0, Context::PREVIOUS_INDEX, true);
  const Operator* op_inner =
      builder_->common()->OsrValue(Linkage::kOsrContextSpillSlotIndex);
  int last = static_cast<int>(contexts()->size() - 1);
  for (int i = last; i >= 0; i--) {
    osr_context = (i == last) ? graph->NewNode(op_inner, osr_loop_entry)
                              : graph->NewNode(op, osr_context, osr_context,
                                               osr_loop_entry);
    contexts()->at(i) = osr_context;
  }
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

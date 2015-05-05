// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/ast-graph-builder.h"

#include "src/compiler.h"
#include "src/compiler/ast-loop-assignment-analyzer.h"
#include "src/compiler/control-builders.h"
#include "src/compiler/js-type-feedback.h"
#include "src/compiler/linkage.h"
#include "src/compiler/liveness-analyzer.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/state-values-utils.h"
#include "src/full-codegen.h"
#include "src/parser.h"
#include "src/scopes.h"

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
  virtual void ProduceValue(Node* value) = 0;

  // Unplugs a node from this expression context.  Call this to retrieve the
  // result of another Visit function that already plugged the context.
  virtual Node* ConsumeValue() = 0;

  // Shortcut for "context->ProduceValue(context->ConsumeValue())".
  void ReplaceValue() { ProduceValue(ConsumeValue()); }

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
class AstGraphBuilder::AstEffectContext FINAL : public AstContext {
 public:
  explicit AstEffectContext(AstGraphBuilder* owner)
      : AstContext(owner, Expression::kEffect) {}
  ~AstEffectContext() FINAL;
  void ProduceValue(Node* value) FINAL;
  Node* ConsumeValue() FINAL;
};


// Context to evaluate expression for its value (and side effects).
class AstGraphBuilder::AstValueContext FINAL : public AstContext {
 public:
  explicit AstValueContext(AstGraphBuilder* owner)
      : AstContext(owner, Expression::kValue) {}
  ~AstValueContext() FINAL;
  void ProduceValue(Node* value) FINAL;
  Node* ConsumeValue() FINAL;
};


// Context to evaluate expression for a condition value (and side effects).
class AstGraphBuilder::AstTestContext FINAL : public AstContext {
 public:
  explicit AstTestContext(AstGraphBuilder* owner)
      : AstContext(owner, Expression::kTest) {}
  ~AstTestContext() FINAL;
  void ProduceValue(Node* value) FINAL;
  Node* ConsumeValue() FINAL;
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
        depth_(builder_->environment()->ContextStackDepth()) {
    builder_->environment()->PushContext(context);  // Push.
    builder_->set_execution_context(this);
  }

  ~ContextScope() {
    builder_->set_execution_context(outer_);  // Pop.
    builder_->environment()->PopContext();
    CHECK_EQ(depth_, builder_->environment()->ContextStackDepth());
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
  virtual bool Execute(Command cmd, Statement* target, Node* value) {
    // For function-level control.
    switch (cmd) {
      case CMD_THROW:
        builder()->BuildThrow(value);
        return true;
      case CMD_RETURN:
        builder()->BuildReturn(value);
        return true;
      case CMD_BREAK:
      case CMD_CONTINUE:
        break;
    }
    return false;
  }

  Environment* environment() { return builder_->environment(); }
  AstGraphBuilder* builder() const { return builder_; }
  int stack_height() const { return stack_height_; }

 private:
  AstGraphBuilder* builder_;
  ControlScope* outer_;
  int stack_height_;
};


// Helper class for a try-finally control scope. It can record intercepted
// control-flow commands that cause entry into a finally-block, and re-apply
// them after again leaving that block. Special tokens are used to identify
// paths going through the finally-block to dispatch after leaving the block.
class AstGraphBuilder::ControlScope::DeferredCommands : public ZoneObject {
 public:
  explicit DeferredCommands(AstGraphBuilder* owner)
      : owner_(owner), deferred_(owner->zone()) {}

  // One recorded control-flow command.
  struct Entry {
    Command command;       // The command type being applied on this path.
    Statement* statement;  // The target statement for the command or {NULL}.
    Node* token;           // A token identifying this particular path.
  };

  // Records a control-flow command while entering the finally-block. This also
  // generates a new dispatch token that identifies one particular path.
  Node* RecordCommand(Command cmd, Statement* stmt, Node* value) {
    Node* token = NewPathTokenForDeferredCommand();
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
  Node* NewPathTokenForDeferredCommand() {
    return owner_->jsgraph()->Constant(static_cast<int>(deferred_.size()));
  }
  Node* NewPathTokenForImplicitFallThrough() {
    return owner_->jsgraph()->Constant(-1);
  }
  Node* NewPathDispatchCondition(Node* t1, Node* t2) {
    // TODO(mstarzinger): This should be machine()->WordEqual(), but our Phi
    // nodes all have kRepTagged|kTypeAny, which causes representation mismatch.
    return owner_->NewNode(owner_->javascript()->StrictEqual(), t1, t2);
  }

 private:
  AstGraphBuilder* owner_;
  ZoneVector<Entry> deferred_;
};


// Control scope implementation for a BreakableStatement.
class AstGraphBuilder::ControlScopeForBreakable : public ControlScope {
 public:
  ControlScopeForBreakable(AstGraphBuilder* owner, BreakableStatement* target,
                           ControlBuilder* control)
      : ControlScope(owner), target_(target), control_(control) {}

 protected:
  virtual bool Execute(Command cmd, Statement* target, Node* value) OVERRIDE {
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
  virtual bool Execute(Command cmd, Statement* target, Node* value) OVERRIDE {
    if (target != target_) return false;  // We are not the command target.
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
  ControlScopeForCatch(AstGraphBuilder* owner, TryCatchBuilder* control)
      : ControlScope(owner), control_(control) {
    builder()->try_nesting_level_++;  // Increment nesting.
  }
  ~ControlScopeForCatch() {
    builder()->try_nesting_level_--;  // Decrement nesting.
  }

 protected:
  virtual bool Execute(Command cmd, Statement* target, Node* value) OVERRIDE {
    switch (cmd) {
      case CMD_THROW:
        control_->Throw(value);
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
  ControlScopeForFinally(AstGraphBuilder* owner, DeferredCommands* commands,
                         TryFinallyBuilder* control)
      : ControlScope(owner), commands_(commands), control_(control) {
    builder()->try_nesting_level_++;  // Increment nesting.
  }
  ~ControlScopeForFinally() {
    builder()->try_nesting_level_--;  // Decrement nesting.
  }

 protected:
  virtual bool Execute(Command cmd, Statement* target, Node* value) OVERRIDE {
    Node* token = commands_->RecordCommand(cmd, target, value);
    control_->LeaveTry(token, value);
    return true;
  }

 private:
  DeferredCommands* commands_;
  TryFinallyBuilder* control_;
};


AstGraphBuilder::AstGraphBuilder(Zone* local_zone, CompilationInfo* info,
                                 JSGraph* jsgraph, LoopAssignmentAnalysis* loop,
                                 JSTypeFeedbackTable* js_type_feedback)
    : local_zone_(local_zone),
      info_(info),
      jsgraph_(jsgraph),
      environment_(nullptr),
      ast_context_(nullptr),
      globals_(0, local_zone),
      execution_control_(nullptr),
      execution_context_(nullptr),
      try_nesting_level_(0),
      input_buffer_size_(0),
      input_buffer_(nullptr),
      exit_control_(nullptr),
      loop_assignment_analysis_(loop),
      state_values_cache_(jsgraph),
      liveness_analyzer_(static_cast<size_t>(info->scope()->num_stack_slots()),
                         local_zone),
      js_type_feedback_(js_type_feedback) {
  InitializeAstVisitor(info->isolate(), local_zone);
}


Node* AstGraphBuilder::GetFunctionClosure() {
  if (!function_closure_.is_set()) {
    const Operator* op =
        common()->Parameter(Linkage::kJSFunctionCallClosureParamIndex);
    Node* node = NewNode(op, graph()->start());
    function_closure_.set(node);
  }
  return function_closure_.get();
}


void AstGraphBuilder::CreateFunctionContext(bool constant_context) {
  function_context_.set(constant_context
                            ? jsgraph()->HeapConstant(info()->context())
                            : NewOuterContextParam());
}


Node* AstGraphBuilder::NewOuterContextParam() {
  // Parameter (arity + 1) is special for the outer context of the function
  const Operator* op = common()->Parameter(info()->num_parameters() + 1);
  return NewNode(op, graph()->start());
}


Node* AstGraphBuilder::NewCurrentContextOsrValue() {
  // TODO(titzer): use a real OSR value here; a parameter works by accident.
  // Parameter (arity + 1) is special for the outer context of the function
  const Operator* op = common()->Parameter(info()->num_parameters() + 1);
  return NewNode(op, graph()->start());
}


bool AstGraphBuilder::CreateGraph(bool constant_context, bool stack_check) {
  Scope* scope = info()->scope();
  DCHECK(graph() != NULL);

  // Set up the basic structure of the graph.
  int parameter_count = info()->num_parameters();
  graph()->SetStart(graph()->NewNode(common()->Start(parameter_count)));

  // Initialize the top-level environment.
  Environment env(this, scope, graph()->start());
  set_environment(&env);

  // Initialize control scope.
  ControlScope control(this);

  if (info()->is_osr()) {
    // Use OSR normal entry as the start of the top-level environment.
    // It will be replaced with {Dead} after typing and optimizations.
    NewNode(common()->OsrNormalEntry());
  }

  // Initialize the incoming context.
  CreateFunctionContext(constant_context);
  ContextScope incoming(this, scope, function_context_.get());

  // Build receiver check for sloppy mode if necessary.
  // TODO(mstarzinger/verwaest): Should this be moved back into the CallIC?
  Node* original_receiver = env.Lookup(scope->receiver());
  Node* patched_receiver = BuildPatchReceiverToGlobalProxy(original_receiver);
  env.Bind(scope->receiver(), patched_receiver);

  // Build function context only if there are context allocated variables.
  int heap_slots = info()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    // Push a new inner context scope for the function.
    Node* closure = GetFunctionClosure();
    Node* inner_context =
        BuildLocalFunctionContext(function_context_.get(), closure);
    ContextScope top_context(this, scope, inner_context);
    CreateGraphBody(stack_check);
  } else {
    // Simply use the outer function context in building the graph.
    CreateGraphBody(stack_check);
  }

  // Finish the basic structure of the graph.
  graph()->SetEnd(graph()->NewNode(common()->End(), exit_control()));

  // Compute local variable liveness information and use it to relax
  // frame states.
  ClearNonLiveSlotsInFrameStates();

  // Failures indicated by stack overflow.
  return !HasStackOverflow();
}


void AstGraphBuilder::CreateGraphBody(bool stack_check) {
  Scope* scope = info()->scope();

  // Build the arguments object if it is used.
  BuildArgumentsObject(scope->arguments());

  // Build rest arguments array if it is used.
  int rest_index;
  Variable* rest_parameter = scope->rest_parameter(&rest_index);
  BuildRestArgumentsArray(rest_parameter, rest_index);

  // Emit tracing call if requested to do so.
  if (FLAG_trace) {
    NewNode(javascript()->CallRuntime(Runtime::kTraceEnter, 0));
  }

  // Visit implicit declaration of the function name.
  if (scope->is_function_scope() && scope->function() != NULL) {
    VisitVariableDeclaration(scope->function());
  }

  // Visit declarations within the function scope.
  VisitDeclarations(scope->declarations());

  // Build a stack-check before the body.
  if (stack_check) {
    Node* node = NewNode(javascript()->StackCheck());
    PrepareFrameState(node, BailoutId::FunctionEntry());
  }

  // Visit statements in the function body.
  VisitStatements(info()->function()->body());

  // Emit tracing call if requested to do so.
  if (FLAG_trace) {
    // TODO(mstarzinger): Only traces implicit return.
    Node* return_value = jsgraph()->UndefinedConstant();
    NewNode(javascript()->CallRuntime(Runtime::kTraceExit, 1), return_value);
  }

  // Return 'undefined' in case we can fall off the end.
  BuildReturn(jsgraph()->UndefinedConstant());
}


void AstGraphBuilder::ClearNonLiveSlotsInFrameStates() {
  if (!FLAG_analyze_environment_liveness) return;

  NonLiveFrameStateSlotReplacer replacer(
      &state_values_cache_, jsgraph()->UndefinedConstant(),
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


// Left-hand side can only be a property, a global or a variable slot.
enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };


// Determine the left-hand side kind of an assignment.
static LhsKind DetermineLhsKind(Expression* expr) {
  Property* property = expr->AsProperty();
  DCHECK(expr->IsValidReferenceExpression());
  LhsKind lhs_kind =
      (property == NULL) ? VARIABLE : (property->key()->IsPropertyName())
                                          ? NAMED_PROPERTY
                                          : KEYED_PROPERTY;
  return lhs_kind;
}


AstGraphBuilder::Environment::Environment(AstGraphBuilder* builder,
                                          Scope* scope,
                                          Node* control_dependency)
    : builder_(builder),
      parameters_count_(scope->num_parameters() + 1),
      locals_count_(scope->num_stack_slots()),
      liveness_block_(builder_->liveness_analyzer()->NewBlock()),
      values_(builder_->local_zone()),
      contexts_(builder_->local_zone()),
      control_dependency_(control_dependency),
      effect_dependency_(control_dependency),
      parameters_node_(nullptr),
      locals_node_(nullptr),
      stack_node_(nullptr) {
  DCHECK_EQ(scope->num_parameters() + 1, parameters_count());

  // Bind the receiver variable.
  Node* receiver = builder->graph()->NewNode(common()->Parameter(0),
                                             builder->graph()->start());
  values()->push_back(receiver);

  // Bind all parameter variables. The parameter indices are shifted by 1
  // (receiver is parameter index -1 but environment index 0).
  for (int i = 0; i < scope->num_parameters(); ++i) {
    Node* parameter = builder->graph()->NewNode(common()->Parameter(i + 1),
                                                builder->graph()->start());
    values()->push_back(parameter);
  }

  // Bind all local variables to undefined.
  Node* undefined_constant = builder->jsgraph()->UndefinedConstant();
  values()->insert(values()->end(), locals_count(), undefined_constant);
}


AstGraphBuilder::Environment::Environment(AstGraphBuilder::Environment* copy)
    : builder_(copy->builder_),
      parameters_count_(copy->parameters_count_),
      locals_count_(copy->locals_count_),
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

  if (FLAG_analyze_environment_liveness) {
    // Split the liveness blocks.
    copy->liveness_block_ =
        builder_->liveness_analyzer()->NewBlock(copy->liveness_block());
    liveness_block_ =
        builder_->liveness_analyzer()->NewBlock(copy->liveness_block());
  }
}


void AstGraphBuilder::Environment::Bind(Variable* variable, Node* node) {
  DCHECK(variable->IsStackAllocated());
  if (variable->IsParameter()) {
    // The parameter indices are shifted by 1 (receiver is parameter
    // index -1 but environment index 0).
    values()->at(variable->index() + 1) = node;
  } else {
    DCHECK(variable->IsStackLocal());
    values()->at(variable->index() + parameters_count_) = node;
    if (FLAG_analyze_environment_liveness) {
      liveness_block()->Bind(variable->index());
    }
  }
}


Node* AstGraphBuilder::Environment::Lookup(Variable* variable) {
  DCHECK(variable->IsStackAllocated());
  if (variable->IsParameter()) {
    // The parameter indices are shifted by 1 (receiver is parameter
    // index -1 but environment index 0).
    return values()->at(variable->index() + 1);
  } else {
    DCHECK(variable->IsStackLocal());
    if (FLAG_analyze_environment_liveness) {
      liveness_block()->Lookup(variable->index());
    }
    return values()->at(variable->index() + parameters_count_);
  }
}


void AstGraphBuilder::Environment::MarkAllLocalsLive() {
  if (FLAG_analyze_environment_liveness) {
    for (int i = 0; i < locals_count_; i++) {
      liveness_block()->Lookup(i);
    }
  }
}


AstGraphBuilder::Environment*
AstGraphBuilder::Environment::CopyAndShareLiveness() {
  Environment* env = new (zone()) Environment(this);
  if (FLAG_analyze_environment_liveness) {
    env->liveness_block_ = liveness_block();
  }
  return env;
}


void AstGraphBuilder::Environment::UpdateStateValues(Node** state_values,
                                                     int offset, int count) {
  bool should_update = false;
  Node** env_values = (count == 0) ? nullptr : &values()->at(offset);
  if (*state_values == NULL || (*state_values)->InputCount() != count) {
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


Node* AstGraphBuilder::Environment::Checkpoint(
    BailoutId ast_id, OutputFrameStateCombine combine) {
  if (!FLAG_turbo_deoptimization) return nullptr;

  UpdateStateValues(&parameters_node_, 0, parameters_count());
  UpdateStateValuesWithCache(&locals_node_, parameters_count(), locals_count());
  UpdateStateValues(&stack_node_, parameters_count() + locals_count(),
                    stack_height());

  const Operator* op = common()->FrameState(JS_FRAME, ast_id, combine);

  Node* result = graph()->NewNode(op, parameters_node_, locals_node_,
                                  stack_node_, builder()->current_context(),
                                  builder()->jsgraph()->UndefinedConstant());
  if (FLAG_analyze_environment_liveness) {
    liveness_block()->Checkpoint(result);
  }
  return result;
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


void AstGraphBuilder::AstEffectContext::ProduceValue(Node* value) {
  // The value is ignored.
}


void AstGraphBuilder::AstValueContext::ProduceValue(Node* value) {
  environment()->Push(value);
}


void AstGraphBuilder::AstTestContext::ProduceValue(Node* value) {
  environment()->Push(owner()->BuildToBoolean(value));
}


Node* AstGraphBuilder::AstEffectContext::ConsumeValue() { return NULL; }


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
  while (current != NULL) {
    environment()->Trim(current->stack_height());
    if (current->Execute(command, target, value)) break;
    current = current->outer_;
  }
  builder()->set_environment(env);
  DCHECK(current != NULL);  // Always handled (unless stack is malformed).
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
  if (expr == NULL) {
    return environment()->Push(jsgraph()->NullConstant());
  }
  VisitForValue(expr);
}


void AstGraphBuilder::VisitForValueOrTheHole(Expression* expr) {
  if (expr == NULL) {
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
    expr->Accept(this);
  } else {
    ast_context()->ProduceValue(jsgraph()->UndefinedConstant());
  }
}


void AstGraphBuilder::VisitForEffect(Expression* expr) {
  AstEffectContext for_effect(this);
  if (!CheckStackOverflow()) {
    expr->Accept(this);
  } else {
    ast_context()->ProduceValue(jsgraph()->UndefinedConstant());
  }
}


void AstGraphBuilder::VisitForTest(Expression* expr) {
  AstTestContext for_condition(this);
  if (!CheckStackOverflow()) {
    expr->Accept(this);
  } else {
    ast_context()->ProduceValue(jsgraph()->UndefinedConstant());
  }
}


void AstGraphBuilder::Visit(Expression* expr) {
  // Reuses enclosing AstContext.
  if (!CheckStackOverflow()) {
    expr->Accept(this);
  } else {
    ast_context()->ProduceValue(jsgraph()->UndefinedConstant());
  }
}


void AstGraphBuilder::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  VariableMode mode = decl->mode();
  bool hole_init = mode == CONST || mode == CONST_LEGACY || mode == LET;
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      Handle<Oddball> value = variable->binding_needs_init()
                                  ? isolate()->factory()->the_hole_value()
                                  : isolate()->factory()->undefined_value();
      globals()->push_back(variable->name());
      globals()->push_back(value);
      break;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL:
      if (hole_init) {
        Node* value = jsgraph()->TheHoleConstant();
        environment()->Bind(variable, value);
      }
      break;
    case Variable::CONTEXT:
      if (hole_init) {
        Node* value = jsgraph()->TheHoleConstant();
        const Operator* op = javascript()->StoreContext(0, variable->index());
        NewNode(op, current_context(), value);
      }
      break;
    case Variable::LOOKUP:
      UNIMPLEMENTED();
  }
}


void AstGraphBuilder::VisitFunctionDeclaration(FunctionDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      Handle<SharedFunctionInfo> function =
          Compiler::BuildFunctionInfo(decl->fun(), info()->script(), info());
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals()->push_back(variable->name());
      globals()->push_back(function);
      break;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL: {
      VisitForValue(decl->fun());
      Node* value = environment()->Pop();
      environment()->Bind(variable, value);
      break;
    }
    case Variable::CONTEXT: {
      VisitForValue(decl->fun());
      Node* value = environment()->Pop();
      const Operator* op = javascript()->StoreContext(0, variable->index());
      NewNode(op, current_context(), value);
      break;
    }
    case Variable::LOOKUP:
      UNIMPLEMENTED();
  }
}


void AstGraphBuilder::VisitModuleDeclaration(ModuleDeclaration* decl) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitImportDeclaration(ImportDeclaration* decl) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitExportDeclaration(ExportDeclaration* decl) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitModuleLiteral(ModuleLiteral* modl) { UNREACHABLE(); }


void AstGraphBuilder::VisitModulePath(ModulePath* modl) { UNREACHABLE(); }


void AstGraphBuilder::VisitModuleUrl(ModuleUrl* modl) { UNREACHABLE(); }


void AstGraphBuilder::VisitBlock(Block* stmt) {
  BlockBuilder block(this);
  ControlScopeForBreakable scope(this, stmt, &block);
  if (stmt->labels() != NULL) block.BeginBlock();
  if (stmt->scope() == NULL) {
    // Visit statements in the same scope, no declarations.
    VisitStatements(stmt->statements());
  } else {
    // Visit declarations and statements in a block scope.
    Node* context = BuildLocalBlockContext(stmt->scope());
    ContextScope scope(this, stmt->scope(), context);
    VisitDeclarations(stmt->scope()->declarations());
    VisitStatements(stmt->statements());
  }
  if (stmt->labels() != NULL) block.EndBlock();
}


void AstGraphBuilder::VisitModuleStatement(ModuleStatement* stmt) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitExpressionStatement(ExpressionStatement* stmt) {
  VisitForEffect(stmt->expression());
}


void AstGraphBuilder::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
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
  const Operator* op = javascript()->CreateWithContext();
  Node* context = NewNode(op, value, GetFunctionClosure());
  PrepareFrameState(context, stmt->EntryId());
  ContextScope scope(this, stmt->scope(), context);
  Visit(stmt->statement());
}


void AstGraphBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  ZoneList<CaseClause*>* clauses = stmt->cases();
  SwitchBuilder compare_switch(this, clauses->length());
  ControlScopeForBreakable scope(this, stmt, &compare_switch);
  compare_switch.BeginSwitch();
  int default_index = -1;

  // Keep the switch value on the stack until a case matches.
  VisitForValue(stmt->tag());
  Node* tag = environment()->Top();

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
    const Operator* op = javascript()->StrictEqual();
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
  VisitIterationBody(stmt, &while_loop);
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
  VisitIterationBody(stmt, &while_loop);
  while_loop.EndBody();
  while_loop.EndLoop();
}


void AstGraphBuilder::VisitForStatement(ForStatement* stmt) {
  LoopBuilder for_loop(this);
  VisitIfNotNull(stmt->init());
  for_loop.BeginLoop(GetVariablesAssignedInLoop(stmt), CheckOsrEntry(stmt));
  if (stmt->cond() != NULL) {
    VisitForTest(stmt->cond());
    Node* condition = environment()->Pop();
    for_loop.BreakUnless(condition);
  } else {
    for_loop.BreakUnless(jsgraph()->TrueConstant());
  }
  VisitIterationBody(stmt, &for_loop);
  for_loop.EndBody();
  VisitIfNotNull(stmt->next());
  for_loop.EndLoop();
}


// TODO(dcarney): this is a big function.  Try to clean up some.
void AstGraphBuilder::VisitForInStatement(ForInStatement* stmt) {
  VisitForValue(stmt->subject());
  Node* obj = environment()->Pop();
  // Check for undefined or null before entering loop.
  IfBuilder is_undefined(this);
  Node* is_undefined_cond =
      NewNode(javascript()->StrictEqual(), obj, jsgraph()->UndefinedConstant());
  is_undefined.If(is_undefined_cond);
  is_undefined.Then();
  is_undefined.Else();
  {
    IfBuilder is_null(this);
    Node* is_null_cond =
        NewNode(javascript()->StrictEqual(), obj, jsgraph()->NullConstant());
    is_null.If(is_null_cond);
    is_null.Then();
    is_null.Else();
    // Convert object to jsobject.
    // PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);
    obj = NewNode(javascript()->ToObject(), obj);
    PrepareFrameState(obj, stmt->ToObjectId(), OutputFrameStateCombine::Push());
    environment()->Push(obj);
    // TODO(dcarney): should do a fast enum cache check here to skip runtime.
    Node* cache_type = NewNode(
        javascript()->CallRuntime(Runtime::kGetPropertyNamesFast, 1), obj);
    PrepareFrameState(cache_type, stmt->EnumId(),
                      OutputFrameStateCombine::Push());
    // TODO(dcarney): these next runtime calls should be removed in favour of
    //                a few simplified instructions.
    Node* cache_pair = NewNode(
        javascript()->CallRuntime(Runtime::kForInInit, 2), obj, cache_type);
    // cache_type may have been replaced.
    Node* cache_array = NewNode(common()->Projection(0), cache_pair);
    cache_type = NewNode(common()->Projection(1), cache_pair);
    Node* cache_length =
        NewNode(javascript()->CallRuntime(Runtime::kForInCacheArrayLength, 2),
                cache_type, cache_array);
    {
      // TODO(dcarney): this check is actually supposed to be for the
      //                empty enum case only.
      IfBuilder have_no_properties(this);
      Node* empty_array_cond = NewNode(javascript()->StrictEqual(),
                                       cache_length, jsgraph()->ZeroConstant());
      have_no_properties.If(empty_array_cond);
      have_no_properties.Then();
      // Pop obj and skip loop.
      environment()->Pop();
      have_no_properties.Else();
      {
        // Construct the rest of the environment.
        environment()->Push(cache_type);
        environment()->Push(cache_array);
        environment()->Push(cache_length);
        environment()->Push(jsgraph()->ZeroConstant());

        // Build the actual loop body.
        VisitForInBody(stmt);
      }
      have_no_properties.End();
    }
    is_null.End();
  }
  is_undefined.End();
}


// TODO(dcarney): this is a big function.  Try to clean up some.
void AstGraphBuilder::VisitForInBody(ForInStatement* stmt) {
  LoopBuilder for_loop(this);
  for_loop.BeginLoop(GetVariablesAssignedInLoop(stmt), CheckOsrEntry(stmt));

  // These stack values are renamed in the case of OSR, so reload them
  // from the environment.
  Node* index = environment()->Peek(0);
  Node* cache_length = environment()->Peek(1);
  Node* cache_array = environment()->Peek(2);
  Node* cache_type = environment()->Peek(3);
  Node* obj = environment()->Peek(4);

  // Check loop termination condition.
  Node* exit_cond = NewNode(javascript()->LessThan(), index, cache_length);
  // TODO(jarin): provide real bailout id.
  PrepareFrameState(exit_cond, BailoutId::None());
  for_loop.BreakUnless(exit_cond);
  Node* pair = NewNode(javascript()->CallRuntime(Runtime::kForInNext, 4), obj,
                       cache_array, cache_type, index);
  Node* value = NewNode(common()->Projection(0), pair);
  Node* should_filter = NewNode(common()->Projection(1), pair);
  environment()->Push(value);
  {
    // Test if FILTER_KEY needs to be called.
    IfBuilder test_should_filter(this);
    Node* should_filter_cond = NewNode(
        javascript()->StrictEqual(), should_filter, jsgraph()->TrueConstant());
    test_should_filter.If(should_filter_cond);
    test_should_filter.Then();
    value = environment()->Pop();
    Node* builtins = BuildLoadBuiltinsObject();
    Node* function = BuildLoadObjectField(
        builtins,
        JSBuiltinsObject::OffsetOfFunctionWithId(Builtins::FILTER_KEY));
    // result is either the string key or Smi(0) indicating the property
    // is gone.
    Node* res = NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                        function, obj, value);
    // TODO(jarin): provide real bailout id.
    PrepareFrameState(res, BailoutId::None());
    Node* property_missing =
        NewNode(javascript()->StrictEqual(), res, jsgraph()->ZeroConstant());
    {
      IfBuilder is_property_missing(this);
      is_property_missing.If(property_missing);
      is_property_missing.Then();
      // Inc counter and continue.
      Node* index_inc =
          NewNode(javascript()->Add(), index, jsgraph()->OneConstant());
      // TODO(jarin): provide real bailout id.
      PrepareFrameStateAfterAndBefore(index_inc, BailoutId::None(),
                                      OutputFrameStateCombine::Ignore(),
                                      jsgraph()->EmptyFrameState());
      environment()->Poke(0, index_inc);
      for_loop.Continue();
      is_property_missing.Else();
      is_property_missing.End();
    }
    // Replace 'value' in environment.
    environment()->Push(res);
    test_should_filter.Else();
    test_should_filter.End();
  }
  value = environment()->Pop();
  // Bind value and do loop body.
  VisitForInAssignment(stmt->each(), value, stmt->AssignmentId());
  VisitIterationBody(stmt, &for_loop);
  index = environment()->Peek(0);
  for_loop.EndBody();

  // Inc counter and continue.
  Node* index_inc =
      NewNode(javascript()->Add(), index, jsgraph()->OneConstant());
  // TODO(jarin): provide real bailout id.
  PrepareFrameStateAfterAndBefore(index_inc, BailoutId::None(),
                                  OutputFrameStateCombine::Ignore(),
                                  jsgraph()->EmptyFrameState());
  environment()->Poke(0, index_inc);
  for_loop.EndLoop();
  environment()->Drop(5);
  // PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
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
  VisitIterationBody(stmt, &for_loop);
  for_loop.EndBody();
  for_loop.EndLoop();
}


void AstGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  TryCatchBuilder try_control(this);

  // Evaluate the try-block inside a control scope. This simulates a handler
  // that is intercepting 'throw' control commands.
  try_control.BeginTry();
  {
    ControlScopeForCatch scope(this, &try_control);
    STATIC_ASSERT(TryBlockConstant::kElementCount == 1);
    environment()->Push(current_context());
    Visit(stmt->try_block());
    environment()->Pop();
  }
  try_control.EndTry();

  // Create a catch scope that binds the exception.
  Node* exception = try_control.GetExceptionNode();
  Unique<String> name = MakeUnique(stmt->variable()->name());
  const Operator* op = javascript()->CreateCatchContext(name);
  Node* context = NewNode(op, exception, GetFunctionClosure());
  PrepareFrameState(context, BailoutId::None());
  {
    ContextScope scope(this, stmt->scope(), context);
    DCHECK(stmt->scope()->declarations()->is_empty());
    // Evaluate the catch-block.
    Visit(stmt->catch_block());
  }
  try_control.EndCatch();

  // TODO(mstarzinger): Remove bailout once everything works.
  if (!FLAG_turbo_exceptions) SetStackOverflow();
}


void AstGraphBuilder::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  TryFinallyBuilder try_control(this);

  ExternalReference message_object =
      ExternalReference::address_of_pending_message_obj(isolate());

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
      new (zone()) ControlScope::DeferredCommands(this);

  // Evaluate the try-block inside a control scope. This simulates a handler
  // that is intercepting all control commands.
  try_control.BeginTry();
  {
    ControlScopeForFinally scope(this, commands, &try_control);
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
  Node* message = BuildLoadExternal(message_object, kMachAnyTagged);
  environment()->Push(token);  // TODO(mstarzinger): Cook token!
  environment()->Push(result);
  environment()->Push(message);

  // Evaluate the finally-block.
  Visit(stmt->finally_block());
  try_control.EndFinally();

  // The result value, dispatch token and message is restored from the operand
  // stack (this is in sync with FullCodeGenerator::ExitFinallyBlock).
  message = environment()->Pop();
  result = environment()->Pop();
  token = environment()->Pop();  // TODO(mstarzinger): Uncook token!
  BuildStoreExternal(message_object, kMachAnyTagged, message);

  // Dynamic dispatch after the finally-block.
  commands->ApplyDeferredCommands(token, result);

  // TODO(mstarzinger): Remove bailout once everything works.
  if (!FLAG_turbo_exceptions) SetStackOverflow();
}


void AstGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  Node* node = NewNode(javascript()->CallRuntime(Runtime::kDebugBreak, 0));
  PrepareFrameState(node, stmt->DebugBreakId());
  environment()->MarkAllLocalsLive();
}


void AstGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  Node* context = current_context();

  // Build a new shared function info if we cannot find one in the baseline
  // code. We also have a stack overflow if the recursive compilation did.
  expr->InitializeSharedInfo(handle(info()->shared_info()->code()));
  Handle<SharedFunctionInfo> shared_info = expr->shared_info();
  if (shared_info.is_null()) {
    shared_info = Compiler::BuildFunctionInfo(expr, info()->script(), info());
    CHECK(!shared_info.is_null());  // TODO(mstarzinger): Set stack overflow?
  }

  // Create node to instantiate a new closure.
  Node* info = jsgraph()->Constant(shared_info);
  Node* pretenure = jsgraph()->BooleanConstant(expr->pretenure());
  const Operator* op = javascript()->CallRuntime(Runtime::kNewClosure, 3);
  Node* value = NewNode(op, context, info, pretenure);
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitClassLiteral(ClassLiteral* expr) {
  if (expr->scope() == NULL) {
    // Visit class literal in the same scope, no declarations.
    VisitClassLiteralContents(expr);
  } else {
    // Visit declarations and class literal in a block scope.
    Node* context = BuildLocalBlockContext(expr->scope());
    ContextScope scope(this, expr->scope(), context);
    VisitDeclarations(expr->scope()->declarations());
    VisitClassLiteralContents(expr);
  }
}


void AstGraphBuilder::VisitClassLiteralContents(ClassLiteral* expr) {
  Node* class_name = expr->raw_name() ? jsgraph()->Constant(expr->name())
                                      : jsgraph()->UndefinedConstant();

  // The class name is expected on the operand stack.
  environment()->Push(class_name);
  VisitForValueOrTheHole(expr->extends());
  VisitForValue(expr->constructor());

  // Create node to instantiate a new class.
  Node* constructor = environment()->Pop();
  Node* extends = environment()->Pop();
  Node* name = environment()->Pop();
  Node* script = jsgraph()->Constant(info()->script());
  Node* start = jsgraph()->Constant(expr->start_position());
  Node* end = jsgraph()->Constant(expr->end_position());
  const Operator* opc = javascript()->CallRuntime(Runtime::kDefineClass, 6);
  Node* literal = NewNode(opc, name, extends, constructor, script, start, end);

  // The prototype is ensured to exist by Runtime_DefineClass. No access check
  // is needed here since the constructor is created by the class literal.
  Node* proto =
      BuildLoadObjectField(literal, JSFunction::kPrototypeOrInitialMapOffset);

  // The class literal and the prototype are both expected on the operand stack
  // during evaluation of the method values.
  environment()->Push(literal);
  environment()->Push(proto);

  // Create nodes to store method values into the literal.
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    environment()->Push(property->is_static() ? literal : proto);

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
    BuildSetHomeObject(value, receiver, property->value());

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
      case ObjectLiteral::Property::PROTOTYPE:
        UNREACHABLE();
      case ObjectLiteral::Property::COMPUTED: {
        const Operator* op =
            javascript()->CallRuntime(Runtime::kDefineClassMethod, 3);
        NewNode(op, receiver, key, value);
        break;
      }
      case ObjectLiteral::Property::GETTER: {
        Node* attr = jsgraph()->Constant(DONT_ENUM);
        const Operator* op = javascript()->CallRuntime(
            Runtime::kDefineGetterPropertyUnchecked, 4);
        NewNode(op, receiver, key, value, attr);
        break;
      }
      case ObjectLiteral::Property::SETTER: {
        Node* attr = jsgraph()->Constant(DONT_ENUM);
        const Operator* op = javascript()->CallRuntime(
            Runtime::kDefineSetterPropertyUnchecked, 4);
        NewNode(op, receiver, key, value, attr);
        break;
      }
    }
  }

  // Transform both the class literal and the prototype to fast properties.
  const Operator* op = javascript()->CallRuntime(Runtime::kToFastProperties, 1);
  NewNode(op, environment()->Pop());  // prototype
  NewNode(op, environment()->Pop());  // literal

  // Assign to class variable.
  if (expr->scope() != NULL) {
    DCHECK_NOT_NULL(expr->class_variable_proxy());
    Variable* var = expr->class_variable_proxy()->var();
    BuildVariableAssignment(var, literal, Token::INIT_CONST, BailoutId::None());
  }

  PrepareFrameState(literal, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(literal);
}


void AstGraphBuilder::VisitNativeFunctionLiteral(NativeFunctionLiteral* expr) {
  UNREACHABLE();
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
  ast_context()->ReplaceValue();
}


void AstGraphBuilder::VisitVariableProxy(VariableProxy* expr) {
  VectorSlotPair pair = CreateVectorSlotPair(expr->VariableFeedbackSlot());
  Node* value = BuildVariableLoad(expr->var(), expr->id(), pair);
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitLiteral(Literal* expr) {
  Node* value = jsgraph()->Constant(expr->value());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to materialize a regular expression literal.
  Node* literals_array =
      BuildLoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* literal_index = jsgraph()->Constant(expr->literal_index());
  Node* pattern = jsgraph()->Constant(expr->pattern());
  Node* flags = jsgraph()->Constant(expr->flags());
  const Operator* op =
      javascript()->CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  Node* literal = NewNode(op, literals_array, literal_index, pattern, flags);
  PrepareFrameState(literal, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(literal);
}


void AstGraphBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to deep-copy the literal boilerplate.
  expr->BuildConstantProperties(isolate());
  Node* literals_array =
      BuildLoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* literal_index = jsgraph()->Constant(expr->literal_index());
  Node* constants = jsgraph()->Constant(expr->constant_properties());
  Node* flags = jsgraph()->Constant(expr->ComputeFlags());
  const Operator* op =
      javascript()->CallRuntime(Runtime::kCreateObjectLiteral, 4);
  Node* literal = NewNode(op, literals_array, literal_index, constants, flags);
  PrepareFrameState(literal, expr->CreateLiteralId(),
                    OutputFrameStateCombine::Push());

  // The object is expected on the operand stack during computation of the
  // property values and is the value of the entire expression.
  environment()->Push(literal);

  // Mark all computed expressions that are bound to a key that is shadowed by
  // a later occurrence of the same key. For the marked expressions, no store
  // code is emitted.
  expr->CalculateEmitStore(zone());

  // Create nodes to store computed values into the literal.
  int property_index = 0;
  AccessorTable accessor_table(zone());
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
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForValue(property->value());
            Node* value = environment()->Pop();
            Handle<Name> name = key->AsPropertyName();
            Node* store =
                BuildNamedStore(literal, name, value, TypeFeedbackId::None());
            PrepareFrameState(store, key->id());
            BuildSetHomeObject(value, literal, property->value());
          } else {
            VisitForEffect(property->value());
          }
          break;
        }
        environment()->Push(literal);  // Duplicate receiver.
        VisitForValue(property->key());
        VisitForValue(property->value());
        Node* value = environment()->Pop();
        Node* key = environment()->Pop();
        Node* receiver = environment()->Pop();
        if (property->emit_store()) {
          Node* language = jsgraph()->Constant(SLOPPY);
          const Operator* op =
              javascript()->CallRuntime(Runtime::kSetProperty, 4);
          NewNode(op, receiver, key, value, language);
          BuildSetHomeObject(value, receiver, property->value());
        }
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE: {
        environment()->Push(literal);  // Duplicate receiver.
        VisitForValue(property->value());
        Node* value = environment()->Pop();
        Node* receiver = environment()->Pop();
        DCHECK(property->emit_store());
        const Operator* op =
            javascript()->CallRuntime(Runtime::kInternalSetPrototype, 2);
        Node* set_prototype = NewNode(op, receiver, value);
        // SetPrototype should not lazy deopt on an object literal.
        PrepareFrameState(set_prototype, BailoutId::None());
        break;
      }
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          accessor_table.lookup(key)->second->getter = property->value();
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          accessor_table.lookup(key)->second->setter = property->value();
        }
        break;
    }
  }

  // Create nodes to define accessors, using only a single call to the runtime
  // for each pair of corresponding getters and setters.
  for (AccessorTable::Iterator it = accessor_table.begin();
       it != accessor_table.end(); ++it) {
    VisitForValue(it->first);
    VisitForValueOrNull(it->second->getter);
    BuildSetHomeObject(environment()->Top(), literal, it->second->getter);
    VisitForValueOrNull(it->second->setter);
    BuildSetHomeObject(environment()->Top(), literal, it->second->setter);
    Node* setter = environment()->Pop();
    Node* getter = environment()->Pop();
    Node* name = environment()->Pop();
    Node* attr = jsgraph()->Constant(NONE);
    const Operator* op =
        javascript()->CallRuntime(Runtime::kDefineAccessorPropertyUnchecked, 5);
    Node* call = NewNode(op, literal, name, getter, setter, attr);
    // This should not lazy deopt on a new literal.
    PrepareFrameState(call, BailoutId::None());
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

    environment()->Push(literal);  // Duplicate receiver.
    VisitForValue(property->key());
    Node* name = BuildToName(environment()->Pop(),
                             expr->GetIdForProperty(property_index));
    environment()->Push(name);
    // TODO(mstarzinger): For ObjectLiteral::Property::PROTOTYPE the key should
    // not be on the operand stack while the value is being evaluated. Come up
    // with a repro for this and fix it. Also find a nice way to do so. :)
    VisitForValue(property->value());
    Node* value = environment()->Pop();
    Node* key = environment()->Pop();
    Node* receiver = environment()->Pop();
    BuildSetHomeObject(value, receiver, property->value());

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::COMPUTED:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL: {
        Node* attr = jsgraph()->Constant(NONE);
        const Operator* op =
            javascript()->CallRuntime(Runtime::kDefineDataPropertyUnchecked, 4);
        Node* call = NewNode(op, receiver, key, value, attr);
        PrepareFrameState(call, BailoutId::None());
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE: {
        const Operator* op =
            javascript()->CallRuntime(Runtime::kInternalSetPrototype, 2);
        Node* call = NewNode(op, receiver, value);
        PrepareFrameState(call, BailoutId::None());
        break;
      }
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

  // Transform literals that contain functions to fast properties.
  if (expr->has_function()) {
    const Operator* op =
        javascript()->CallRuntime(Runtime::kToFastProperties, 1);
    NewNode(op, literal);
  }

  ast_context()->ProduceValue(environment()->Pop());
}


void AstGraphBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to deep-copy the literal boilerplate.
  expr->BuildConstantElements(isolate());
  Node* literals_array =
      BuildLoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* literal_index = jsgraph()->Constant(expr->literal_index());
  Node* constants = jsgraph()->Constant(expr->constant_elements());
  Node* flags = jsgraph()->Constant(expr->ComputeFlags());
  const Operator* op =
      javascript()->CallRuntime(Runtime::kCreateArrayLiteral, 4);
  Node* literal = NewNode(op, literals_array, literal_index, constants, flags);
  PrepareFrameState(literal, expr->CreateLiteralId(),
                    OutputFrameStateCombine::Push());

  // The array and the literal index are both expected on the operand stack
  // during computation of the element values.
  environment()->Push(literal);
  environment()->Push(literal_index);

  // Create nodes to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  for (int i = 0; i < expr->values()->length(); i++) {
    Expression* subexpr = expr->values()->at(i);
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    VisitForValue(subexpr);
    Node* frame_state_before = environment()->Checkpoint(
        subexpr->id(), OutputFrameStateCombine::PokeAt(0));
    Node* value = environment()->Pop();
    Node* index = jsgraph()->Constant(i);
    Node* store =
        BuildKeyedStore(literal, index, value, TypeFeedbackId::None());
    PrepareFrameStateAfterAndBefore(store, expr->GetIdForElement(i),
                                    OutputFrameStateCombine::Ignore(),
                                    frame_state_before);
  }

  environment()->Pop();  // Array literal index.
  ast_context()->ProduceValue(environment()->Pop());
}


void AstGraphBuilder::VisitForInAssignment(Expression* expr, Node* value,
                                           BailoutId bailout_id) {
  DCHECK(expr->IsValidReferenceExpression());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->AsProperty();
  LhsKind assign_type = DetermineLhsKind(expr);

  // Evaluate LHS expression and store the value.
  switch (assign_type) {
    case VARIABLE: {
      Variable* var = expr->AsVariableProxy()->var();
      BuildVariableAssignment(var, value, Token::ASSIGN, bailout_id);
      break;
    }
    case NAMED_PROPERTY: {
      environment()->Push(value);
      VisitForValue(property->obj());
      Node* object = environment()->Pop();
      value = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store =
          BuildNamedStore(object, name, value, TypeFeedbackId::None());
      PrepareFrameState(store, bailout_id);
      break;
    }
    case KEYED_PROPERTY: {
      environment()->Push(value);
      VisitForValue(property->obj());
      VisitForValue(property->key());
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      value = environment()->Pop();
      Node* store = BuildKeyedStore(object, key, value, TypeFeedbackId::None());
      // TODO(jarin) Provide a real frame state before.
      PrepareFrameStateAfterAndBefore(store, bailout_id,
                                      OutputFrameStateCombine::Ignore(),
                                      jsgraph()->EmptyFrameState());
      break;
    }
  }
}


void AstGraphBuilder::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpression());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = DetermineLhsKind(expr->target());

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_PROPERTY:
      VisitForValue(property->obj());
      break;
    case KEYED_PROPERTY: {
      VisitForValue(property->obj());
      VisitForValue(property->key());
      break;
    }
  }

  // Evaluate the value and potentially handle compound assignments by loading
  // the left-hand side value and performing a binary operation.
  Node* frame_state_before_store = nullptr;
  bool needs_frame_state_before = (assign_type == KEYED_PROPERTY);
  if (expr->is_compound()) {
    Node* old_value = NULL;
    switch (assign_type) {
      case VARIABLE: {
        VariableProxy* proxy = expr->target()->AsVariableProxy();
        VectorSlotPair pair =
            CreateVectorSlotPair(proxy->VariableFeedbackSlot());
        old_value = BuildVariableLoad(proxy->var(), expr->target()->id(), pair);
        break;
      }
      case NAMED_PROPERTY: {
        Node* object = environment()->Top();
        Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
        VectorSlotPair pair =
            CreateVectorSlotPair(property->PropertyFeedbackSlot());
        old_value =
            BuildNamedLoad(object, name, pair, property->PropertyFeedbackId());
        PrepareFrameState(old_value, property->LoadId(),
                          OutputFrameStateCombine::Push());
        break;
      }
      case KEYED_PROPERTY: {
        Node* key = environment()->Top();
        Node* object = environment()->Peek(1);
        VectorSlotPair pair =
            CreateVectorSlotPair(property->PropertyFeedbackSlot());
        old_value =
            BuildKeyedLoad(object, key, pair, property->PropertyFeedbackId());
        PrepareFrameState(old_value, property->LoadId(),
                          OutputFrameStateCombine::Push());
        break;
      }
    }
    environment()->Push(old_value);
    VisitForValue(expr->value());
    Node* frame_state_before = environment()->Checkpoint(expr->value()->id());
    Node* right = environment()->Pop();
    Node* left = environment()->Pop();
    Node* value = BuildBinaryOp(left, right, expr->binary_op());
    PrepareFrameStateAfterAndBefore(value, expr->binary_operation()->id(),
                                    OutputFrameStateCombine::Push(),
                                    frame_state_before);
    environment()->Push(value);
    if (needs_frame_state_before) {
      frame_state_before_store = environment()->Checkpoint(
          expr->binary_operation()->id(), OutputFrameStateCombine::PokeAt(0));
    }
  } else {
    VisitForValue(expr->value());
    if (needs_frame_state_before) {
      // This frame state can be used for lazy-deopting from a to-number
      // conversion if we are storing into a typed array. It is important
      // that the frame state is usable for such lazy deopt (i.e., it has
      // to specify how to override the value before the conversion, in this
      // case, it overwrites the stack top).
      frame_state_before_store = environment()->Checkpoint(
          expr->value()->id(), OutputFrameStateCombine::PokeAt(0));
    }
  }

  // Store the value.
  Node* value = environment()->Pop();
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->target()->AsVariableProxy()->var();
      BuildVariableAssignment(variable, value, expr->op(), expr->id(),
                              ast_context()->GetStateCombine());
      break;
    }
    case NAMED_PROPERTY: {
      Node* object = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store =
          BuildNamedStore(object, name, value, expr->AssignmentFeedbackId());
      PrepareFrameState(store, expr->id(), ast_context()->GetStateCombine());
      break;
    }
    case KEYED_PROPERTY: {
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      Node* store =
          BuildKeyedStore(object, key, value, expr->AssignmentFeedbackId());
      PrepareFrameStateAfterAndBefore(store, expr->id(),
                                      ast_context()->GetStateCombine(),
                                      frame_state_before_store);
      break;
    }
  }

  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitYield(Yield* expr) {
  // TODO(turbofan): Implement yield here.
  SetStackOverflow();
  ast_context()->ProduceValue(jsgraph()->UndefinedConstant());
}


void AstGraphBuilder::VisitThrow(Throw* expr) {
  VisitForValue(expr->exception());
  Node* exception = environment()->Pop();
  Node* value = BuildThrowError(exception, expr->id());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitProperty(Property* expr) {
  Node* value;
  VectorSlotPair pair = CreateVectorSlotPair(expr->PropertyFeedbackSlot());
  if (expr->key()->IsPropertyName()) {
    VisitForValue(expr->obj());
    Node* object = environment()->Pop();
    Handle<Name> name = expr->key()->AsLiteral()->AsPropertyName();
    value = BuildNamedLoad(object, name, pair, expr->PropertyFeedbackId());
  } else {
    VisitForValue(expr->obj());
    VisitForValue(expr->key());
    Node* key = environment()->Pop();
    Node* object = environment()->Pop();
    value = BuildKeyedLoad(object, key, pair, expr->PropertyFeedbackId());
  }
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitCall(Call* expr) {
  Expression* callee = expr->expression();
  Call::CallType call_type = expr->GetCallType(isolate());

  // Prepare the callee and the receiver to the function call. This depends on
  // the semantics of the underlying call type.
  CallFunctionFlags flags = NO_CALL_FUNCTION_FLAGS;
  Node* receiver_value = NULL;
  Node* callee_value = NULL;
  bool possibly_eval = false;
  switch (call_type) {
    case Call::GLOBAL_CALL: {
      VariableProxy* proxy = callee->AsVariableProxy();
      VectorSlotPair pair = CreateVectorSlotPair(proxy->VariableFeedbackSlot());
      callee_value =
          BuildVariableLoad(proxy->var(), expr->expression()->id(), pair);
      receiver_value = jsgraph()->UndefinedConstant();
      break;
    }
    case Call::LOOKUP_SLOT_CALL: {
      Variable* variable = callee->AsVariableProxy()->var();
      DCHECK(variable->location() == Variable::LOOKUP);
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op =
          javascript()->CallRuntime(Runtime::kLoadLookupSlot, 2);
      Node* pair = NewNode(op, current_context(), name);
      callee_value = NewNode(common()->Projection(0), pair);
      receiver_value = NewNode(common()->Projection(1), pair);

      PrepareFrameState(pair, expr->EvalOrLookupId(),
                        OutputFrameStateCombine::Push(2));
      break;
    }
    case Call::PROPERTY_CALL: {
      Property* property = callee->AsProperty();
      VisitForValue(property->obj());
      Node* object = environment()->Top();
      VectorSlotPair pair =
          CreateVectorSlotPair(property->PropertyFeedbackSlot());
      if (property->key()->IsPropertyName()) {
        Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
        callee_value =
            BuildNamedLoad(object, name, pair, property->PropertyFeedbackId());
      } else {
        VisitForValue(property->key());
        Node* key = environment()->Pop();
        callee_value =
            BuildKeyedLoad(object, key, pair, property->PropertyFeedbackId());
      }
      PrepareFrameState(callee_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      receiver_value = environment()->Pop();
      // Note that a PROPERTY_CALL requires the receiver to be wrapped into an
      // object for sloppy callees. This could also be modeled explicitly here,
      // thereby obsoleting the need for a flag to the call operator.
      flags = CALL_AS_METHOD;
      break;
    }
    case Call::SUPER_CALL:
      // TODO(dslomov): Implement super calls.
      callee_value = jsgraph()->UndefinedConstant();
      receiver_value = jsgraph()->UndefinedConstant();
      SetStackOverflow();
      break;
    case Call::POSSIBLY_EVAL_CALL:
      possibly_eval = true;
    // Fall through.
    case Call::OTHER_CALL:
      VisitForValue(callee);
      callee_value = environment()->Pop();
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

  // Resolve callee and receiver for a potential direct eval call. This block
  // will mutate the callee and receiver values pushed onto the environment.
  if (possibly_eval && args->length() > 0) {
    int arg_count = args->length();

    // Extract callee and source string from the environment.
    Node* callee = environment()->Peek(arg_count + 1);
    Node* source = environment()->Peek(arg_count - 1);

    // Create node to ask for help resolving potential eval call. This will
    // provide a fully resolved callee and the corresponding receiver.
    Node* function = GetFunctionClosure();
    Node* receiver = environment()->Lookup(info()->scope()->receiver());
    Node* language = jsgraph()->Constant(language_mode());
    Node* position = jsgraph()->Constant(info()->scope()->start_position());
    const Operator* op =
        javascript()->CallRuntime(Runtime::kResolvePossiblyDirectEval, 6);
    Node* pair =
        NewNode(op, callee, source, function, receiver, language, position);
    PrepareFrameState(pair, expr->EvalOrLookupId(),
                      OutputFrameStateCombine::PokeAt(arg_count + 1));
    Node* new_callee = NewNode(common()->Projection(0), pair);
    Node* new_receiver = NewNode(common()->Projection(1), pair);

    // Patch callee and receiver on the environment.
    environment()->Poke(arg_count + 1, new_callee);
    environment()->Poke(arg_count + 0, new_receiver);
  }

  // Create node to perform the function call.
  const Operator* call = javascript()->CallFunction(args->length() + 2, flags);
  Node* value = ProcessArguments(call, args->length() + 2);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitCallNew(CallNew* expr) {
  VisitForValue(expr->expression());

  // Evaluate all arguments to the construct call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the construct call.
  const Operator* call = javascript()->CallConstruct(args->length() + 1);
  Node* value = ProcessArguments(call, args->length() + 1);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitCallJSRuntime(CallRuntime* expr) {
  Handle<String> name = expr->name();

  // The callee and the receiver both have to be pushed onto the operand stack
  // before arguments are being evaluated.
  CallFunctionFlags flags = NO_CALL_FUNCTION_FLAGS;
  Node* receiver_value = BuildLoadBuiltinsObject();
  VectorSlotPair pair = CreateVectorSlotPair(expr->CallRuntimeFeedbackSlot());
  Node* callee_value =
      BuildNamedLoad(receiver_value, name, pair, expr->CallRuntimeFeedbackId());
  // TODO(jarin): Find/create a bailout id to deoptimize to (crankshaft
  // refuses to optimize functions with jsruntime calls).
  PrepareFrameState(callee_value, BailoutId::None(),
                    OutputFrameStateCombine::Push());
  environment()->Push(callee_value);
  environment()->Push(receiver_value);

  // Evaluate all arguments to the JS runtime call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the JS runtime call.
  const Operator* call = javascript()->CallFunction(args->length() + 2, flags);
  Node* value = ProcessArguments(call, args->length() + 2);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitCallRuntime(CallRuntime* expr) {
  const Runtime::Function* function = expr->function();

  // Handle calls to runtime functions implemented in JavaScript separately as
  // the call follows JavaScript ABI and the callee is statically unknown.
  if (expr->is_jsruntime()) {
    DCHECK(function == NULL && expr->name()->length() > 0);
    return VisitCallJSRuntime(expr);
  }

  // Evaluate all arguments to the runtime call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the runtime call.
  Runtime::FunctionId functionId = function->function_id;
  const Operator* call = javascript()->CallRuntime(functionId, args->length());
  Node* value = ProcessArguments(call, args->length());
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
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
  DCHECK(expr->expression()->IsValidReferenceExpression());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->expression()->AsProperty();
  LhsKind assign_type = DetermineLhsKind(expr->expression());

  // Reserve space for result of postfix operation.
  bool is_postfix = expr->is_postfix() && !ast_context()->IsEffect();
  if (is_postfix) environment()->Push(jsgraph()->UndefinedConstant());

  // Evaluate LHS expression and get old value.
  Node* old_value = NULL;
  int stack_depth = -1;
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      VectorSlotPair pair = CreateVectorSlotPair(proxy->VariableFeedbackSlot());
      old_value =
          BuildVariableLoad(proxy->var(), expr->expression()->id(), pair);
      stack_depth = 0;
      break;
    }
    case NAMED_PROPERTY: {
      VisitForValue(property->obj());
      Node* object = environment()->Top();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      VectorSlotPair pair =
          CreateVectorSlotPair(property->PropertyFeedbackSlot());
      old_value =
          BuildNamedLoad(object, name, pair, property->PropertyFeedbackId());
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
      old_value =
          BuildKeyedLoad(object, key, pair, property->PropertyFeedbackId());
      PrepareFrameState(old_value, property->LoadId(),
                        OutputFrameStateCombine::Push());
      stack_depth = 2;
      break;
    }
  }

  // Convert old value into a number.
  old_value = NewNode(javascript()->ToNumber(), old_value);
  PrepareFrameState(old_value, expr->ToNumberId(),
                    OutputFrameStateCombine::Push());

  Node* frame_state_before_store =
      assign_type == KEYED_PROPERTY
          ? environment()->Checkpoint(expr->ToNumberId())
          : nullptr;

  // Save result for postfix expressions at correct stack depth.
  if (is_postfix) environment()->Poke(stack_depth, old_value);

  // Create node to perform +1/-1 operation.
  Node* value =
      BuildBinaryOp(old_value, jsgraph()->OneConstant(), expr->binary_op());
  // This should never deoptimize because we have converted to number
  // before.
  PrepareFrameStateAfterAndBefore(value, BailoutId::None(),
                                  OutputFrameStateCombine::Ignore(),
                                  jsgraph()->EmptyFrameState());

  // Store the value.
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->expression()->AsVariableProxy()->var();
      environment()->Push(value);
      BuildVariableAssignment(variable, value, expr->op(),
                              expr->AssignmentId());
      environment()->Pop();
      break;
    }
    case NAMED_PROPERTY: {
      Node* object = environment()->Pop();
      Handle<Name> name = property->key()->AsLiteral()->AsPropertyName();
      Node* store =
          BuildNamedStore(object, name, value, expr->CountStoreFeedbackId());
      environment()->Push(value);
      PrepareFrameState(store, expr->AssignmentId());
      environment()->Pop();
      break;
    }
    case KEYED_PROPERTY: {
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      Node* store =
          BuildKeyedStore(object, key, value, expr->CountStoreFeedbackId());
      environment()->Push(value);
      PrepareFrameStateAfterAndBefore(store, expr->AssignmentId(),
                                      OutputFrameStateCombine::Ignore(),
                                      frame_state_before_store);
      environment()->Pop();
      break;
    }
  }

  // Restore old value for postfix expressions.
  if (is_postfix) value = environment()->Pop();

  ast_context()->ProduceValue(value);
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
      Node* frame_state_before = environment()->Checkpoint(expr->right()->id());
      Node* right = environment()->Pop();
      Node* left = environment()->Pop();
      Node* value = BuildBinaryOp(left, right, expr->op());
      PrepareFrameStateAfterAndBefore(value, expr->id(),
                                      ast_context()->GetStateCombine(),
                                      frame_state_before);
      ast_context()->ProduceValue(value);
    }
  }
}


void AstGraphBuilder::VisitCompareOperation(CompareOperation* expr) {
  const Operator* op;
  switch (expr->op()) {
    case Token::EQ:
      op = javascript()->Equal();
      break;
    case Token::NE:
      op = javascript()->NotEqual();
      break;
    case Token::EQ_STRICT:
      op = javascript()->StrictEqual();
      break;
    case Token::NE_STRICT:
      op = javascript()->StrictNotEqual();
      break;
    case Token::LT:
      op = javascript()->LessThan();
      break;
    case Token::GT:
      op = javascript()->GreaterThan();
      break;
    case Token::LTE:
      op = javascript()->LessThanOrEqual();
      break;
    case Token::GTE:
      op = javascript()->GreaterThanOrEqual();
      break;
    case Token::INSTANCEOF:
      op = javascript()->InstanceOf();
      break;
    case Token::IN:
      op = javascript()->HasProperty();
      break;
    default:
      op = NULL;
      UNREACHABLE();
  }
  VisitForValue(expr->left());
  VisitForValue(expr->right());
  Node* right = environment()->Pop();
  Node* left = environment()->Pop();
  Node* value = NewNode(op, left, right);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  Node* value = GetFunctionClosure();
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitSuperReference(SuperReference* expr) {
  // TODO(turbofan): Implement super here.
  SetStackOverflow();
  ast_context()->ProduceValue(jsgraph()->UndefinedConstant());
}


void AstGraphBuilder::VisitCaseClause(CaseClause* expr) { UNREACHABLE(); }


void AstGraphBuilder::VisitDeclarations(ZoneList<Declaration*>* declarations) {
  DCHECK(globals()->empty());
  AstVisitor::VisitDeclarations(declarations);
  if (globals()->empty()) return;
  int array_index = 0;
  Handle<FixedArray> data = isolate()->factory()->NewFixedArray(
      static_cast<int>(globals()->size()), TENURED);
  for (Handle<Object> obj : *globals()) data->set(array_index++, *obj);
  int encoded_flags = DeclareGlobalsEvalFlag::encode(info()->is_eval()) |
                      DeclareGlobalsNativeFlag::encode(info()->is_native()) |
                      DeclareGlobalsLanguageMode::encode(language_mode());
  Node* flags = jsgraph()->Constant(encoded_flags);
  Node* pairs = jsgraph()->Constant(data);
  const Operator* op = javascript()->CallRuntime(Runtime::kDeclareGlobals, 3);
  NewNode(op, current_context(), pairs, flags);
  globals()->clear();
}


void AstGraphBuilder::VisitIfNotNull(Statement* stmt) {
  if (stmt == NULL) return;
  Visit(stmt);
}


void AstGraphBuilder::VisitIterationBody(IterationStatement* stmt,
                                         LoopBuilder* loop) {
  ControlScopeForIteration scope(this, stmt, loop);
  Visit(stmt->body());
}


void AstGraphBuilder::VisitDelete(UnaryOperation* expr) {
  Node* value;
  if (expr->expression()->IsVariableProxy()) {
    // Delete of an unqualified identifier is only allowed in classic mode but
    // deleting "this" is allowed in all language modes.
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
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitVoid(UnaryOperation* expr) {
  VisitForEffect(expr->expression());
  Node* value = jsgraph()->UndefinedConstant();
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitTypeof(UnaryOperation* expr) {
  Node* operand;
  if (expr->expression()->IsVariableProxy()) {
    // Typeof does not throw a reference error on global variables, hence we
    // perform a non-contextual load in case the operand is a variable proxy.
    VariableProxy* proxy = expr->expression()->AsVariableProxy();
    VectorSlotPair pair = CreateVectorSlotPair(proxy->VariableFeedbackSlot());
    operand = BuildVariableLoad(proxy->var(), expr->expression()->id(), pair,
                                NOT_CONTEXTUAL);
  } else {
    VisitForValue(expr->expression());
    operand = environment()->Pop();
  }
  Node* value = NewNode(javascript()->TypeOf(), operand);
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitNot(UnaryOperation* expr) {
  VisitForValue(expr->expression());
  Node* operand = environment()->Pop();
  // TODO(mstarzinger): Possible optimization when we are in effect context.
  Node* value = NewNode(javascript()->UnaryNot(), operand);
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitComma(BinaryOperation* expr) {
  VisitForEffect(expr->left());
  Visit(expr->right());
  ast_context()->ReplaceValue();
}


void AstGraphBuilder::VisitLogicalExpression(BinaryOperation* expr) {
  bool is_logical_and = expr->op() == Token::AND;
  IfBuilder compare_if(this);
  VisitForValue(expr->left());
  Node* condition = environment()->Top();
  compare_if.If(BuildToBoolean(condition));
  compare_if.Then();
  if (is_logical_and) {
    environment()->Pop();
    Visit(expr->right());
  } else if (ast_context()->IsEffect()) {
    environment()->Pop();
  }
  compare_if.Else();
  if (!is_logical_and) {
    environment()->Pop();
    Visit(expr->right());
  } else if (ast_context()->IsEffect()) {
    environment()->Pop();
  }
  compare_if.End();
  ast_context()->ReplaceValue();
}


LanguageMode AstGraphBuilder::language_mode() const {
  return info()->language_mode();
}


VectorSlotPair AstGraphBuilder::CreateVectorSlotPair(
    FeedbackVectorICSlot slot) const {
  return VectorSlotPair(handle(info()->shared_info()->feedback_vector()), slot);
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


Node* AstGraphBuilder::BuildPatchReceiverToGlobalProxy(Node* receiver) {
  // Sloppy mode functions and builtins need to replace the receiver with the
  // global proxy when called as functions (without an explicit receiver
  // object). Otherwise there is nothing left to do here.
  if (is_strict(language_mode()) || info()->is_native()) return receiver;

  // There is no need to perform patching if the receiver is never used. Note
  // that scope predicates are purely syntactical, a call to eval might still
  // inspect the receiver value.
  if (!info()->scope()->uses_this() && !info()->scope()->inner_uses_this() &&
      !info()->scope()->calls_sloppy_eval()) {
    return receiver;
  }

  IfBuilder receiver_check(this);
  Node* undefined = jsgraph()->UndefinedConstant();
  Node* check = NewNode(javascript()->StrictEqual(), receiver, undefined);
  receiver_check.If(check);
  receiver_check.Then();
  Node* proxy = BuildLoadGlobalProxy();
  environment()->Push(proxy);
  receiver_check.Else();
  environment()->Push(receiver);
  receiver_check.End();
  return environment()->Pop();
}


Node* AstGraphBuilder::BuildLocalFunctionContext(Node* context, Node* closure) {
  // Allocate a new local context.
  const Operator* op = javascript()->CreateFunctionContext();
  Node* local_context = NewNode(op, closure);

  // Copy parameters into context if necessary.
  int num_parameters = info()->scope()->num_parameters();
  for (int i = 0; i < num_parameters; i++) {
    Variable* variable = info()->scope()->parameter(i);
    if (!variable->IsContextSlot()) continue;
    // Temporary parameter node. The parameter indices are shifted by 1
    // (receiver is parameter index -1 but environment index 0).
    Node* parameter = NewNode(common()->Parameter(i + 1), graph()->start());
    // Context variable (at bottom of the context chain).
    DCHECK_EQ(0, info()->scope()->ContextChainLength(variable->scope()));
    const Operator* op = javascript()->StoreContext(0, variable->index());
    NewNode(op, local_context, parameter);
  }

  return local_context;
}


Node* AstGraphBuilder::BuildLocalBlockContext(Scope* scope) {
  Node* closure = GetFunctionClosure();

  // Allocate a new local context.
  const Operator* op = javascript()->CreateBlockContext();
  Node* scope_info = jsgraph()->Constant(scope->GetScopeInfo(info_->isolate()));
  Node* local_context = NewNode(op, scope_info, closure);

  return local_context;
}


Node* AstGraphBuilder::BuildArgumentsObject(Variable* arguments) {
  if (arguments == NULL) return NULL;

  // Allocate and initialize a new arguments object.
  Node* callee = GetFunctionClosure();
  const Operator* op = javascript()->CallRuntime(Runtime::kNewArguments, 1);
  Node* object = NewNode(op, callee);

  // Assign the object to the arguments variable.
  DCHECK(arguments->IsContextSlot() || arguments->IsStackAllocated());
  // This should never lazy deopt, so it is fine to send invalid bailout id.
  BuildVariableAssignment(arguments, object, Token::ASSIGN, BailoutId::None());

  return object;
}


Node* AstGraphBuilder::BuildRestArgumentsArray(Variable* rest, int index) {
  if (rest == NULL) return NULL;

  DCHECK(index >= 0);
  const Operator* op = javascript()->CallRuntime(Runtime::kNewRestParamSlow, 1);
  Node* object = NewNode(op, jsgraph()->SmiConstant(index));

  // Assign the object to the rest array
  DCHECK(rest->IsContextSlot() || rest->IsStackAllocated());
  // This should never lazy deopt, so it is fine to send invalid bailout id.
  BuildVariableAssignment(rest, object, Token::ASSIGN, BailoutId::None());

  return object;
}


Node* AstGraphBuilder::BuildHoleCheckSilent(Node* value, Node* for_hole,
                                            Node* not_hole) {
  IfBuilder hole_check(this);
  Node* the_hole = jsgraph()->TheHoleConstant();
  Node* check = NewNode(javascript()->StrictEqual(), value, the_hole);
  hole_check.If(check);
  hole_check.Then();
  environment()->Push(for_hole);
  hole_check.Else();
  environment()->Push(not_hole);
  hole_check.End();
  return environment()->Pop();
}


Node* AstGraphBuilder::BuildHoleCheckThrow(Node* value, Variable* variable,
                                           Node* not_hole,
                                           BailoutId bailout_id) {
  IfBuilder hole_check(this);
  Node* the_hole = jsgraph()->TheHoleConstant();
  Node* check = NewNode(javascript()->StrictEqual(), value, the_hole);
  hole_check.If(check);
  hole_check.Then();
  Node* error = BuildThrowReferenceError(variable, bailout_id);
  environment()->Push(error);
  hole_check.Else();
  environment()->Push(not_hole);
  hole_check.End();
  return environment()->Pop();
}


Node* AstGraphBuilder::BuildThrowIfStaticPrototype(Node* name,
                                                   BailoutId bailout_id) {
  IfBuilder prototype_check(this);
  Node* prototype_string =
      jsgraph()->Constant(isolate()->factory()->prototype_string());
  Node* check = NewNode(javascript()->StrictEqual(), name, prototype_string);
  prototype_check.If(check);
  prototype_check.Then();
  {
    const Operator* op =
        javascript()->CallRuntime(Runtime::kThrowStaticPrototypeError, 0);
    Node* call = NewNode(op);
    PrepareFrameState(call, bailout_id);
    environment()->Push(call);
  }
  prototype_check.Else();
  environment()->Push(name);
  prototype_check.End();
  return environment()->Pop();
}


Node* AstGraphBuilder::BuildVariableLoad(Variable* variable,
                                         BailoutId bailout_id,
                                         const VectorSlotPair& feedback,
                                         ContextualMode contextual_mode) {
  Node* the_hole = jsgraph()->TheHoleConstant();
  VariableMode mode = variable->mode();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      // Global var, const, or let variable.
      Node* global = BuildLoadGlobalObject();
      Handle<Name> name = variable->name();
      Node* node = BuildNamedLoad(global, name, feedback,
                                  TypeFeedbackId::None(), contextual_mode);
      PrepareFrameState(node, bailout_id, OutputFrameStateCombine::Push());
      return node;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL: {
      // Local var, const, or let variable.
      Node* value = environment()->Lookup(variable);
      if (mode == CONST_LEGACY) {
        // Perform check for uninitialized legacy const variables.
        if (value->op() == the_hole->op()) {
          value = jsgraph()->UndefinedConstant();
        } else if (value->opcode() == IrOpcode::kPhi) {
          Node* undefined = jsgraph()->UndefinedConstant();
          value = BuildHoleCheckSilent(value, undefined, value);
        }
      } else if (mode == LET || mode == CONST) {
        // Perform check for uninitialized let/const variables.
        if (value->op() == the_hole->op()) {
          value = BuildThrowReferenceError(variable, bailout_id);
        } else if (value->opcode() == IrOpcode::kPhi) {
          value = BuildHoleCheckThrow(value, variable, value, bailout_id);
        }
      }
      return value;
    }
    case Variable::CONTEXT: {
      // Context variable (potentially up the context chain).
      int depth = current_scope()->ContextChainLength(variable->scope());
      bool immutable = variable->maybe_assigned() == kNotAssigned;
      const Operator* op =
          javascript()->LoadContext(depth, variable->index(), immutable);
      Node* value = NewNode(op, current_context());
      // TODO(titzer): initialization checks are redundant for already
      // initialized immutable context loads, but only specialization knows.
      // Maybe specializer should be a parameter to the graph builder?
      if (mode == CONST_LEGACY) {
        // Perform check for uninitialized legacy const variables.
        Node* undefined = jsgraph()->UndefinedConstant();
        value = BuildHoleCheckSilent(value, undefined, value);
      } else if (mode == LET || mode == CONST) {
        // Perform check for uninitialized let/const variables.
        value = BuildHoleCheckThrow(value, variable, value, bailout_id);
      }
      return value;
    }
    case Variable::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Node* name = jsgraph()->Constant(variable->name());
      Runtime::FunctionId function_id =
          (contextual_mode == CONTEXTUAL)
              ? Runtime::kLoadLookupSlot
              : Runtime::kLoadLookupSlotNoReferenceError;
      const Operator* op = javascript()->CallRuntime(function_id, 2);
      Node* pair = NewNode(op, current_context(), name);
      PrepareFrameState(pair, bailout_id, OutputFrameStateCombine::Push(1));
      return NewNode(common()->Projection(0), pair);
    }
  }
  UNREACHABLE();
  return NULL;
}


Node* AstGraphBuilder::BuildVariableDelete(
    Variable* variable, BailoutId bailout_id,
    OutputFrameStateCombine state_combine) {
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      // Global var, const, or let variable.
      Node* global = BuildLoadGlobalObject();
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op = javascript()->DeleteProperty(language_mode());
      Node* result = NewNode(op, global, name);
      PrepareFrameState(result, bailout_id, state_combine);
      return result;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL:
    case Variable::CONTEXT:
      // Local var, const, or let variable or context variable.
      return jsgraph()->BooleanConstant(variable->is_this());
    case Variable::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op =
          javascript()->CallRuntime(Runtime::kDeleteLookupSlot, 2);
      Node* result = NewNode(op, current_context(), name);
      PrepareFrameState(result, bailout_id, state_combine);
      return result;
    }
  }
  UNREACHABLE();
  return NULL;
}


Node* AstGraphBuilder::BuildVariableAssignment(
    Variable* variable, Node* value, Token::Value op, BailoutId bailout_id,
    OutputFrameStateCombine combine) {
  Node* the_hole = jsgraph()->TheHoleConstant();
  VariableMode mode = variable->mode();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      // Global var, const, or let variable.
      Node* global = BuildLoadGlobalObject();
      Handle<Name> name = variable->name();
      Node* store =
          BuildNamedStore(global, name, value, TypeFeedbackId::None());
      PrepareFrameState(store, bailout_id, combine);
      return store;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL:
      // Local var, const, or let variable.
      if (mode == CONST_LEGACY && op == Token::INIT_CONST_LEGACY) {
        // Perform an initialization check for legacy const variables.
        Node* current = environment()->Lookup(variable);
        if (current->op() != the_hole->op()) {
          value = BuildHoleCheckSilent(current, value, current);
        }
      } else if (mode == CONST_LEGACY && op != Token::INIT_CONST_LEGACY) {
        // Non-initializing assignment to legacy const is
        // - exception in strict mode.
        // - ignored in sloppy mode.
        if (is_strict(language_mode())) {
          return BuildThrowConstAssignError(bailout_id);
        }
        return value;
      } else if (mode == LET && op != Token::INIT_LET) {
        // Perform an initialization check for let declared variables.
        // Also note that the dynamic hole-check is only done to ensure that
        // this does not break in the presence of do-expressions within the
        // temporal dead zone of a let declared variable.
        Node* current = environment()->Lookup(variable);
        if (current->op() == the_hole->op()) {
          value = BuildThrowReferenceError(variable, bailout_id);
        } else if (value->opcode() == IrOpcode::kPhi) {
          value = BuildHoleCheckThrow(current, variable, value, bailout_id);
        }
      } else if (mode == CONST && op != Token::INIT_CONST) {
        // Assignment to const is exception in all modes.
        Node* current = environment()->Lookup(variable);
        if (current->op() == the_hole->op()) {
          return BuildThrowReferenceError(variable, bailout_id);
        } else if (value->opcode() == IrOpcode::kPhi) {
          BuildHoleCheckThrow(current, variable, value, bailout_id);
        }
        return BuildThrowConstAssignError(bailout_id);
      }
      environment()->Bind(variable, value);
      return value;
    case Variable::CONTEXT: {
      // Context variable (potentially up the context chain).
      int depth = current_scope()->ContextChainLength(variable->scope());
      if (mode == CONST_LEGACY && op == Token::INIT_CONST_LEGACY) {
        // Perform an initialization check for legacy const variables.
        const Operator* op =
            javascript()->LoadContext(depth, variable->index(), false);
        Node* current = NewNode(op, current_context());
        value = BuildHoleCheckSilent(current, value, current);
      } else if (mode == CONST_LEGACY && op != Token::INIT_CONST_LEGACY) {
        // Non-initializing assignment to legacy const is
        // - exception in strict mode.
        // - ignored in sloppy mode.
        if (is_strict(language_mode())) {
          return BuildThrowConstAssignError(bailout_id);
        }
        return value;
      } else if (mode == LET && op != Token::INIT_LET) {
        // Perform an initialization check for let declared variables.
        const Operator* op =
            javascript()->LoadContext(depth, variable->index(), false);
        Node* current = NewNode(op, current_context());
        value = BuildHoleCheckThrow(current, variable, value, bailout_id);
      } else if (mode == CONST && op != Token::INIT_CONST) {
        // Assignment to const is exception in all modes.
        const Operator* op =
            javascript()->LoadContext(depth, variable->index(), false);
        Node* current = NewNode(op, current_context());
        BuildHoleCheckThrow(current, variable, value, bailout_id);
        return BuildThrowConstAssignError(bailout_id);
      }
      const Operator* op = javascript()->StoreContext(depth, variable->index());
      return NewNode(op, current_context(), value);
    }
    case Variable::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Node* name = jsgraph()->Constant(variable->name());
      Node* language = jsgraph()->Constant(language_mode());
      // TODO(mstarzinger): Use Runtime::kInitializeLegacyConstLookupSlot for
      // initializations of const declarations.
      const Operator* op =
          javascript()->CallRuntime(Runtime::kStoreLookupSlot, 4);
      Node* store = NewNode(op, value, current_context(), name, language);
      PrepareFrameState(store, bailout_id, combine);
      return store;
    }
  }
  UNREACHABLE();
  return NULL;
}


static inline Node* Record(JSTypeFeedbackTable* js_type_feedback, Node* node,
                           TypeFeedbackId id) {
  if (js_type_feedback) js_type_feedback->Record(node, id);
  return node;
}


Node* AstGraphBuilder::BuildKeyedLoad(Node* object, Node* key,
                                      const VectorSlotPair& feedback,
                                      TypeFeedbackId id) {
  const Operator* op = javascript()->LoadProperty(feedback);
  return Record(js_type_feedback_, NewNode(op, object, key), id);
}


Node* AstGraphBuilder::BuildNamedLoad(Node* object, Handle<Name> name,
                                      const VectorSlotPair& feedback,
                                      TypeFeedbackId id, ContextualMode mode) {
  const Operator* op =
      javascript()->LoadNamed(MakeUnique(name), feedback, mode);
  return Record(js_type_feedback_, NewNode(op, object), id);
}


Node* AstGraphBuilder::BuildKeyedStore(Node* object, Node* key, Node* value,
                                       TypeFeedbackId id) {
  const Operator* op = javascript()->StoreProperty(language_mode());
  return Record(js_type_feedback_, NewNode(op, object, key, value), id);
}


Node* AstGraphBuilder::BuildNamedStore(Node* object, Handle<Name> name,
                                       Node* value, TypeFeedbackId id) {
  const Operator* op =
      javascript()->StoreNamed(language_mode(), MakeUnique(name));
  return Record(js_type_feedback_, NewNode(op, object, value), id);
}


Node* AstGraphBuilder::BuildLoadObjectField(Node* object, int offset) {
  return NewNode(jsgraph()->machine()->Load(kMachAnyTagged), object,
                 jsgraph()->IntPtrConstant(offset - kHeapObjectTag));
}


Node* AstGraphBuilder::BuildLoadBuiltinsObject() {
  Node* global = BuildLoadGlobalObject();
  Node* builtins =
      BuildLoadObjectField(global, JSGlobalObject::kBuiltinsOffset);
  return builtins;
}


Node* AstGraphBuilder::BuildLoadGlobalObject() {
  const Operator* load_op =
      javascript()->LoadContext(0, Context::GLOBAL_OBJECT_INDEX, true);
  return NewNode(load_op, function_context_.get());
}


Node* AstGraphBuilder::BuildLoadGlobalProxy() {
  Node* global = BuildLoadGlobalObject();
  Node* proxy =
      BuildLoadObjectField(global, JSGlobalObject::kGlobalProxyOffset);
  return proxy;
}


Node* AstGraphBuilder::BuildLoadExternal(ExternalReference reference,
                                         MachineType type) {
  return NewNode(jsgraph()->machine()->Load(type),
                 jsgraph()->ExternalConstant(reference),
                 jsgraph()->IntPtrConstant(0));
}


Node* AstGraphBuilder::BuildStoreExternal(ExternalReference reference,
                                          MachineType type, Node* value) {
  StoreRepresentation representation(type, kNoWriteBarrier);
  return NewNode(jsgraph()->machine()->Store(representation),
                 jsgraph()->ExternalConstant(reference),
                 jsgraph()->IntPtrConstant(0), value);
}


Node* AstGraphBuilder::BuildToBoolean(Node* input) {
  // TODO(titzer): This should be in a JSOperatorReducer.
  switch (input->opcode()) {
    case IrOpcode::kInt32Constant:
      return jsgraph_->BooleanConstant(!Int32Matcher(input).Is(0));
    case IrOpcode::kFloat64Constant:
      return jsgraph_->BooleanConstant(!Float64Matcher(input).Is(0));
    case IrOpcode::kNumberConstant:
      return jsgraph_->BooleanConstant(!NumberMatcher(input).Is(0));
    case IrOpcode::kHeapConstant: {
      Handle<Object> object = HeapObjectMatcher<Object>(input).Value().handle();
      return jsgraph_->BooleanConstant(object->BooleanValue());
    }
    default:
      break;
  }
  if (NodeProperties::IsTyped(input)) {
    Type* upper = NodeProperties::GetBounds(input).upper;
    if (upper->Is(Type::Boolean())) return input;
  }

  return NewNode(javascript()->ToBoolean(), input);
}


Node* AstGraphBuilder::BuildToName(Node* input, BailoutId bailout_id) {
  // TODO(turbofan): Possible optimization is to NOP on name constants. But the
  // same caveat as with BuildToBoolean applies, and it should be factored out
  // into a JSOperatorReducer.
  Node* name = NewNode(javascript()->ToName(), input);
  PrepareFrameState(name, bailout_id);
  return name;
}


Node* AstGraphBuilder::BuildSetHomeObject(Node* value, Node* home_object,
                                          Expression* expr) {
  if (!FunctionLiteral::NeedsHomeObject(expr)) return value;
  Handle<Name> name = isolate()->factory()->home_object_symbol();
  Node* store =
      BuildNamedStore(value, name, home_object, TypeFeedbackId::None());
  PrepareFrameState(store, BailoutId::None());
  return store;
}


Node* AstGraphBuilder::BuildThrowError(Node* exception, BailoutId bailout_id) {
  const Operator* op = javascript()->CallRuntime(Runtime::kThrow, 1);
  Node* call = NewNode(op, exception);
  PrepareFrameState(call, bailout_id);
  return call;
}


Node* AstGraphBuilder::BuildThrowReferenceError(Variable* variable,
                                                BailoutId bailout_id) {
  Node* variable_name = jsgraph()->Constant(variable->name());
  const Operator* op =
      javascript()->CallRuntime(Runtime::kThrowReferenceError, 1);
  Node* call = NewNode(op, variable_name);
  PrepareFrameState(call, bailout_id);
  return call;
}


Node* AstGraphBuilder::BuildThrowConstAssignError(BailoutId bailout_id) {
  const Operator* op =
      javascript()->CallRuntime(Runtime::kThrowConstAssignError, 0);
  Node* call = NewNode(op);
  PrepareFrameState(call, bailout_id);
  return call;
}


Node* AstGraphBuilder::BuildReturn(Node* return_value) {
  Node* control = NewNode(common()->Return(), return_value);
  UpdateControlDependencyToLeaveFunction(control);
  return control;
}


Node* AstGraphBuilder::BuildThrow(Node* exception_value) {
  NewNode(javascript()->CallRuntime(Runtime::kReThrow, 1), exception_value);
  Node* control = NewNode(common()->Throw(), exception_value);
  UpdateControlDependencyToLeaveFunction(control);
  return control;
}


Node* AstGraphBuilder::BuildBinaryOp(Node* left, Node* right, Token::Value op) {
  const Operator* js_op;
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
      js_op = javascript()->Add();
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
      js_op = NULL;
  }
  return NewNode(js_op, left, right);
}


bool AstGraphBuilder::CheckOsrEntry(IterationStatement* stmt) {
  if (info()->osr_ast_id() == stmt->OsrEntryId()) {
    info()->set_osr_expr_stack_height(std::max(
        environment()->stack_height(), info()->osr_expr_stack_height()));
    return true;
  }
  return false;
}


void AstGraphBuilder::PrepareFrameState(Node* node, BailoutId ast_id,
                                        OutputFrameStateCombine combine) {
  if (OperatorProperties::GetFrameStateInputCount(node->op()) > 0) {
    DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));

    DCHECK_EQ(IrOpcode::kDead,
              NodeProperties::GetFrameStateInput(node, 0)->opcode());
    NodeProperties::ReplaceFrameStateInput(
        node, 0, environment()->Checkpoint(ast_id, combine));
  }
}


void AstGraphBuilder::PrepareFrameStateAfterAndBefore(
    Node* node, BailoutId ast_id, OutputFrameStateCombine combine,
    Node* frame_state_before) {
  if (OperatorProperties::GetFrameStateInputCount(node->op()) > 0) {
    DCHECK_EQ(2, OperatorProperties::GetFrameStateInputCount(node->op()));

    DCHECK_EQ(IrOpcode::kDead,
              NodeProperties::GetFrameStateInput(node, 0)->opcode());
    NodeProperties::ReplaceFrameStateInput(
        node, 0, environment()->Checkpoint(ast_id, combine));

    DCHECK_EQ(IrOpcode::kDead,
              NodeProperties::GetFrameStateInput(node, 1)->opcode());
    NodeProperties::ReplaceFrameStateInput(node, 1, frame_state_before);
  }
}


BitVector* AstGraphBuilder::GetVariablesAssignedInLoop(
    IterationStatement* stmt) {
  if (loop_assignment_analysis_ == NULL) return NULL;
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
  DCHECK(op->ValueInputCount() == value_input_count);

  bool has_context = OperatorProperties::HasContextInput(op);
  int frame_state_count = OperatorProperties::GetFrameStateInputCount(op);
  bool has_control = op->ControlInputCount() == 1;
  bool has_effect = op->EffectInputCount() == 1;

  DCHECK(op->ControlInputCount() < 2);
  DCHECK(op->EffectInputCount() < 2);

  Node* result = NULL;
  if (!has_context && frame_state_count == 0 && !has_control && !has_effect) {
    result = graph()->NewNode(op, value_input_count, value_inputs, incomplete);
  } else {
    bool inside_try_scope = try_nesting_level_ > 0;
    int input_count_with_deps = value_input_count;
    if (has_context) ++input_count_with_deps;
    input_count_with_deps += frame_state_count;
    if (has_control) ++input_count_with_deps;
    if (has_effect) ++input_count_with_deps;
    Node** buffer = EnsureInputBufferSize(input_count_with_deps);
    memcpy(buffer, value_inputs, kPointerSize * value_input_count);
    Node** current_input = buffer + value_input_count;
    if (has_context) {
      *current_input++ = current_context();
    }
    for (int i = 0; i < frame_state_count; i++) {
      // The frame state will be inserted later. Here we misuse
      // the {DeadControl} node as a sentinel to be later overwritten
      // with the real frame state.
      *current_input++ = jsgraph()->DeadControl();
    }
    if (has_effect) {
      *current_input++ = environment_->GetEffectDependency();
    }
    if (has_control) {
      *current_input++ = environment_->GetControlDependency();
    }
    result = graph()->NewNode(op, input_count_with_deps, buffer, incomplete);
    if (has_effect) {
      environment_->UpdateEffectDependency(result);
    }
    if (!environment()->IsMarkedAsUnreachable()) {
      // Update the current control dependency for control-producing nodes.
      if (NodeProperties::IsControl(result)) {
        environment_->UpdateControlDependency(result);
      }
      // Add implicit exception continuation for throwing nodes.
      if (!result->op()->HasProperty(Operator::kNoThrow) && inside_try_scope) {
        Node* on_exception = graph()->NewNode(common()->IfException(), result);
        environment_->UpdateControlDependency(on_exception);
        execution_control()->ThrowValue(result);
      }
      // Add implicit success continuation for throwing nodes.
      if (!result->op()->HasProperty(Operator::kNoThrow)) {
        Node* on_success = graph()->NewNode(common()->IfSuccess(), result);
        environment_->UpdateControlDependency(on_success);
      }
    }
  }

  return result;
}


void AstGraphBuilder::UpdateControlDependencyToLeaveFunction(Node* exit) {
  if (environment()->IsMarkedAsUnreachable()) return;
  if (exit_control() != NULL) {
    exit = MergeControl(exit_control(), exit);
  }
  environment()->MarkAsUnreachable();
  set_exit_control(exit);
}


void AstGraphBuilder::Environment::Merge(Environment* other) {
  DCHECK(values_.size() == other->values_.size());
  // TODO(titzer): make context stack heights match.
  DCHECK(contexts_.size() <= other->contexts_.size());

  // Nothing to do if the other environment is dead.
  if (other->IsMarkedAsUnreachable()) return;

  // Resurrect a dead environment by copying the contents of the other one and
  // placing a singleton merge as the new control dependency.
  if (this->IsMarkedAsUnreachable()) {
    Node* other_control = other->control_dependency_;
    Node* inputs[] = {other_control};
    liveness_block_ = other->liveness_block_;
    control_dependency_ =
        graph()->NewNode(common()->Merge(1), arraysize(inputs), inputs, true);
    effect_dependency_ = other->effect_dependency_;
    values_ = other->values_;
    // TODO(titzer): make context stack heights match.
    size_t min = std::min(contexts_.size(), other->contexts_.size());
    contexts_ = other->contexts_;
    contexts_.resize(min, nullptr);
    return;
  }

  // Record the merge for the local variable liveness calculation.
  // Unfortunately, we have to mirror the logic in the MergeControl method:
  // connect before merge or loop, or create a new merge otherwise.
  if (FLAG_analyze_environment_liveness) {
    if (GetControlDependency()->opcode() != IrOpcode::kLoop &&
        GetControlDependency()->opcode() != IrOpcode::kMerge) {
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


void AstGraphBuilder::Environment::PrepareForLoop(BitVector* assigned,
                                                  bool is_osr) {
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

  if (builder_->info()->is_osr()) {
    // Introduce phis for all context values in the case of an OSR graph.
    for (int i = 0; i < static_cast<int>(contexts()->size()); ++i) {
      Node* val = contexts()->at(i);
      if (!IrOpcode::IsConstantOpcode(val->opcode())) {
        contexts()->at(i) = builder_->NewPhi(1, val, control);
      }
    }
  }

  if (is_osr) {
    // Merge OSR values as inputs to the phis of the loop.
    Graph* graph = builder_->graph();
    Node* osr_loop_entry = builder_->graph()->NewNode(
        builder_->common()->OsrLoopEntry(), graph->start(), graph->start());

    builder_->MergeControl(control, osr_loop_entry);
    builder_->MergeEffect(effect, osr_loop_entry, control);

    for (int i = 0; i < size; ++i) {
      Node* val = values()->at(i);
      if (!IrOpcode::IsConstantOpcode(val->opcode())) {
        Node* osr_value =
            graph->NewNode(builder_->common()->OsrValue(i), osr_loop_entry);
        values()->at(i) = builder_->MergeValue(val, osr_value, control);
      }
    }

    // Rename all the contexts in the environment.
    // The innermost context is the OSR value, and the outer contexts are
    // reconstructed by dynamically walking up the context chain.
    Node* osr_context = nullptr;
    const Operator* op =
        builder_->javascript()->LoadContext(0, Context::PREVIOUS_INDEX, true);
    int last = static_cast<int>(contexts()->size() - 1);
    for (int i = last; i >= 0; i--) {
      Node* val = contexts()->at(i);
      if (!IrOpcode::IsConstantOpcode(val->opcode())) {
        osr_context = (i == last) ? builder_->NewCurrentContextOsrValue()
                                  : graph->NewNode(op, osr_context, osr_context,
                                                   osr_loop_entry);
        contexts()->at(i) = builder_->MergeValue(val, osr_context, control);
      } else {
        osr_context = val;
      }
    }
  }
}


Node* AstGraphBuilder::NewPhi(int count, Node* input, Node* control) {
  const Operator* phi_op = common()->Phi(kMachAnyTagged, count);
  Node** buffer = EnsureInputBufferSize(count + 1);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer, true);
}


// TODO(mstarzinger): Revisit this once we have proper effect states.
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
    control->set_op(op);
  } else if (control->opcode() == IrOpcode::kMerge) {
    // Control node for merge exists, add input.
    const Operator* op = common()->Merge(inputs);
    control->AppendInput(graph_zone(), other);
    control->set_op(op);
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
    value->set_op(common()->EffectPhi(inputs));
    value->InsertInput(graph_zone(), inputs - 1, other);
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
    value->set_op(common()->Phi(kMachAnyTagged, inputs));
    value->InsertInput(graph_zone(), inputs - 1, other);
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

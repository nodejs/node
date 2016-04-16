// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-generator.h"

#include "src/ast/scopes.h"
#include "src/compiler.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "src/interpreter/control-flow-builders.h"
#include "src/objects.h"
#include "src/parsing/parser.h"
#include "src/parsing/token.h"

namespace v8 {
namespace internal {
namespace interpreter {


// Scoped class tracking context objects created by the visitor. Represents
// mutations of the context chain within the function body, allowing pushing and
// popping of the current {context_register} during visitation.
class BytecodeGenerator::ContextScope BASE_EMBEDDED {
 public:
  ContextScope(BytecodeGenerator* generator, Scope* scope,
               bool should_pop_context = true)
      : generator_(generator),
        scope_(scope),
        outer_(generator_->execution_context()),
        register_(Register::current_context()),
        depth_(0),
        should_pop_context_(should_pop_context) {
    if (outer_) {
      depth_ = outer_->depth_ + 1;

      // Push the outer context into a new context register.
      Register outer_context_reg(builder()->first_context_register().index() +
                                 outer_->depth_);
      outer_->set_register(outer_context_reg);
      generator_->builder()->PushContext(outer_context_reg);
    }
    generator_->set_execution_context(this);
  }

  ~ContextScope() {
    if (outer_ && should_pop_context_) {
      DCHECK_EQ(register_.index(), Register::current_context().index());
      generator_->builder()->PopContext(outer_->reg());
      outer_->set_register(register_);
    }
    generator_->set_execution_context(outer_);
  }

  // Returns the depth of the given |scope| for the current execution context.
  int ContextChainDepth(Scope* scope) {
    return scope_->ContextChainLength(scope);
  }

  // Returns the execution context at |depth| in the current context chain if it
  // is a function local execution context, otherwise returns nullptr.
  ContextScope* Previous(int depth) {
    if (depth > depth_) {
      return nullptr;
    }

    ContextScope* previous = this;
    for (int i = depth; i > 0; --i) {
      previous = previous->outer_;
    }
    return previous;
  }

  Scope* scope() const { return scope_; }
  Register reg() const { return register_; }

 private:
  const BytecodeArrayBuilder* builder() const { return generator_->builder(); }

  void set_register(Register reg) { register_ = reg; }

  BytecodeGenerator* generator_;
  Scope* scope_;
  ContextScope* outer_;
  Register register_;
  int depth_;
  bool should_pop_context_;
};


// Scoped class for tracking control statements entered by the
// visitor. The pattern derives AstGraphBuilder::ControlScope.
class BytecodeGenerator::ControlScope BASE_EMBEDDED {
 public:
  explicit ControlScope(BytecodeGenerator* generator)
      : generator_(generator), outer_(generator->execution_control()),
        context_(generator->execution_context()) {
    generator_->set_execution_control(this);
  }
  virtual ~ControlScope() { generator_->set_execution_control(outer()); }

  void Break(Statement* stmt) { PerformCommand(CMD_BREAK, stmt); }
  void Continue(Statement* stmt) { PerformCommand(CMD_CONTINUE, stmt); }
  void ReturnAccumulator() { PerformCommand(CMD_RETURN, nullptr); }
  void ReThrowAccumulator() { PerformCommand(CMD_RETHROW, nullptr); }

  class DeferredCommands;

 protected:
  enum Command { CMD_BREAK, CMD_CONTINUE, CMD_RETURN, CMD_RETHROW };
  void PerformCommand(Command command, Statement* statement);
  virtual bool Execute(Command command, Statement* statement) = 0;

  BytecodeGenerator* generator() const { return generator_; }
  ControlScope* outer() const { return outer_; }
  ContextScope* context() const { return context_; }

 private:
  BytecodeGenerator* generator_;
  ControlScope* outer_;
  ContextScope* context_;

  DISALLOW_COPY_AND_ASSIGN(ControlScope);
};


// Helper class for a try-finally control scope. It can record intercepted
// control-flow commands that cause entry into a finally-block, and re-apply
// them after again leaving that block. Special tokens are used to identify
// paths going through the finally-block to dispatch after leaving the block.
class BytecodeGenerator::ControlScope::DeferredCommands final {
 public:
  DeferredCommands(BytecodeGenerator* generator, Register token_register,
                   Register result_register)
      : generator_(generator),
        deferred_(generator->zone()),
        token_register_(token_register),
        result_register_(result_register) {}

  // One recorded control-flow command.
  struct Entry {
    Command command;       // The command type being applied on this path.
    Statement* statement;  // The target statement for the command or {nullptr}.
    int token;             // A token identifying this particular path.
  };

  // Records a control-flow command while entering the finally-block. This also
  // generates a new dispatch token that identifies one particular path. This
  // expects the result to be in the accumulator.
  void RecordCommand(Command command, Statement* statement) {
    int token = static_cast<int>(deferred_.size());
    deferred_.push_back({command, statement, token});

    builder()->StoreAccumulatorInRegister(result_register_);
    builder()->LoadLiteral(Smi::FromInt(token));
    builder()->StoreAccumulatorInRegister(token_register_);
  }

  // Records the dispatch token to be used to identify the re-throw path when
  // the finally-block has been entered through the exception handler. This
  // expects the exception to be in the accumulator.
  void RecordHandlerReThrowPath() {
    // The accumulator contains the exception object.
    RecordCommand(CMD_RETHROW, nullptr);
  }

  // Records the dispatch token to be used to identify the implicit fall-through
  // path at the end of a try-block into the corresponding finally-block.
  void RecordFallThroughPath() {
    builder()->LoadLiteral(Smi::FromInt(-1));
    builder()->StoreAccumulatorInRegister(token_register_);
  }

  // Applies all recorded control-flow commands after the finally-block again.
  // This generates a dynamic dispatch on the token from the entry point.
  void ApplyDeferredCommands() {
    // The fall-through path is covered by the default case, hence +1 here.
    SwitchBuilder dispatch(builder(), static_cast<int>(deferred_.size() + 1));
    for (size_t i = 0; i < deferred_.size(); ++i) {
      Entry& entry = deferred_[i];
      builder()->LoadLiteral(Smi::FromInt(entry.token));
      builder()->CompareOperation(Token::EQ_STRICT, token_register_);
      dispatch.Case(static_cast<int>(i));
    }
    dispatch.DefaultAt(static_cast<int>(deferred_.size()));
    for (size_t i = 0; i < deferred_.size(); ++i) {
      Entry& entry = deferred_[i];
      dispatch.SetCaseTarget(static_cast<int>(i));
      builder()->LoadAccumulatorWithRegister(result_register_);
      execution_control()->PerformCommand(entry.command, entry.statement);
    }
    dispatch.SetCaseTarget(static_cast<int>(deferred_.size()));
  }

  BytecodeArrayBuilder* builder() { return generator_->builder(); }
  ControlScope* execution_control() { return generator_->execution_control(); }

 private:
  BytecodeGenerator* generator_;
  ZoneVector<Entry> deferred_;
  Register token_register_;
  Register result_register_;
};


// Scoped class for dealing with control flow reaching the function level.
class BytecodeGenerator::ControlScopeForTopLevel final
    : public BytecodeGenerator::ControlScope {
 public:
  explicit ControlScopeForTopLevel(BytecodeGenerator* generator)
      : ControlScope(generator) {}

 protected:
  bool Execute(Command command, Statement* statement) override {
    switch (command) {
      case CMD_BREAK:
      case CMD_CONTINUE:
        break;
      case CMD_RETURN:
        generator()->builder()->Return();
        return true;
      case CMD_RETHROW:
        generator()->builder()->ReThrow();
        return true;
    }
    return false;
  }
};


// Scoped class for enabling break inside blocks and switch blocks.
class BytecodeGenerator::ControlScopeForBreakable final
    : public BytecodeGenerator::ControlScope {
 public:
  ControlScopeForBreakable(BytecodeGenerator* generator,
                           BreakableStatement* statement,
                           BreakableControlFlowBuilder* control_builder)
      : ControlScope(generator),
        statement_(statement),
        control_builder_(control_builder) {}

 protected:
  bool Execute(Command command, Statement* statement) override {
    if (statement != statement_) return false;
    switch (command) {
      case CMD_BREAK:
        control_builder_->Break();
        return true;
      case CMD_CONTINUE:
      case CMD_RETURN:
      case CMD_RETHROW:
        break;
    }
    return false;
  }

 private:
  Statement* statement_;
  BreakableControlFlowBuilder* control_builder_;
};


// Scoped class for enabling 'break' and 'continue' in iteration
// constructs, e.g. do...while, while..., for...
class BytecodeGenerator::ControlScopeForIteration final
    : public BytecodeGenerator::ControlScope {
 public:
  ControlScopeForIteration(BytecodeGenerator* generator,
                           IterationStatement* statement,
                           LoopBuilder* loop_builder)
      : ControlScope(generator),
        statement_(statement),
        loop_builder_(loop_builder) {}

 protected:
  bool Execute(Command command, Statement* statement) override {
    if (statement != statement_) return false;
    switch (command) {
      case CMD_BREAK:
        loop_builder_->Break();
        return true;
      case CMD_CONTINUE:
        loop_builder_->Continue();
        return true;
      case CMD_RETURN:
      case CMD_RETHROW:
        break;
    }
    return false;
  }

 private:
  Statement* statement_;
  LoopBuilder* loop_builder_;
};


// Scoped class for enabling 'throw' in try-catch constructs.
class BytecodeGenerator::ControlScopeForTryCatch final
    : public BytecodeGenerator::ControlScope {
 public:
  ControlScopeForTryCatch(BytecodeGenerator* generator,
                          TryCatchBuilder* try_catch_builder)
      : ControlScope(generator) {
    generator->try_catch_nesting_level_++;
  }
  virtual ~ControlScopeForTryCatch() {
    generator()->try_catch_nesting_level_--;
  }

 protected:
  bool Execute(Command command, Statement* statement) override {
    switch (command) {
      case CMD_BREAK:
      case CMD_CONTINUE:
      case CMD_RETURN:
        break;
      case CMD_RETHROW:
        generator()->builder()->ReThrow();
        return true;
    }
    return false;
  }
};


// Scoped class for enabling control flow through try-finally constructs.
class BytecodeGenerator::ControlScopeForTryFinally final
    : public BytecodeGenerator::ControlScope {
 public:
  ControlScopeForTryFinally(BytecodeGenerator* generator,
                            TryFinallyBuilder* try_finally_builder,
                            DeferredCommands* commands)
      : ControlScope(generator),
        try_finally_builder_(try_finally_builder),
        commands_(commands) {
    generator->try_finally_nesting_level_++;
  }
  virtual ~ControlScopeForTryFinally() {
    generator()->try_finally_nesting_level_--;
  }

 protected:
  bool Execute(Command command, Statement* statement) override {
    switch (command) {
      case CMD_BREAK:
      case CMD_CONTINUE:
      case CMD_RETURN:
      case CMD_RETHROW:
        commands_->RecordCommand(command, statement);
        try_finally_builder_->LeaveTry();
        return true;
    }
    return false;
  }

 private:
  TryFinallyBuilder* try_finally_builder_;
  DeferredCommands* commands_;
};


void BytecodeGenerator::ControlScope::PerformCommand(Command command,
                                                     Statement* statement) {
  ControlScope* current = this;
  ContextScope* context = this->context();
  do {
    if (current->Execute(command, statement)) { return; }
    current = current->outer();
    if (current->context() != context) {
      // Pop context to the expected depth.
      // TODO(rmcilroy): Only emit a single context pop.
      generator()->builder()->PopContext(current->context()->reg());
      context = current->context();
    }
  } while (current != nullptr);
  UNREACHABLE();
}


class BytecodeGenerator::RegisterAllocationScope {
 public:
  explicit RegisterAllocationScope(BytecodeGenerator* generator)
      : generator_(generator),
        outer_(generator->register_allocator()),
        allocator_(builder()->zone(),
                   builder()->temporary_register_allocator()) {
    generator_->set_register_allocator(this);
  }

  virtual ~RegisterAllocationScope() {
    generator_->set_register_allocator(outer_);
  }

  Register NewRegister() {
    RegisterAllocationScope* current_scope = generator()->register_allocator();
    if ((current_scope == this) ||
        (current_scope->outer() == this &&
         !current_scope->allocator_.HasConsecutiveAllocations())) {
      // Regular case - Allocating registers in current or outer context.
      // VisitForRegisterValue allocates register in outer context.
      return allocator_.NewRegister();
    } else {
      // If it is required to allocate a register other than current or outer
      // scopes, allocate a new temporary register. It might be expensive to
      // walk the full context chain and compute the list of consecutive
      // reservations in the innerscopes.
      UNIMPLEMENTED();
      return Register::invalid_value();
    }
  }

  void PrepareForConsecutiveAllocations(int count) {
    allocator_.PrepareForConsecutiveAllocations(count);
  }

  Register NextConsecutiveRegister() {
    return allocator_.NextConsecutiveRegister();
  }

  bool RegisterIsAllocatedInThisScope(Register reg) const {
    return allocator_.RegisterIsAllocatedInThisScope(reg);
  }

  RegisterAllocationScope* outer() const { return outer_; }

 private:
  BytecodeGenerator* generator() const { return generator_; }
  BytecodeArrayBuilder* builder() const { return generator_->builder(); }

  BytecodeGenerator* generator_;
  RegisterAllocationScope* outer_;
  BytecodeRegisterAllocator allocator_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocationScope);
};


// Scoped base class for determining where the result of an expression
// is stored.
class BytecodeGenerator::ExpressionResultScope {
 public:
  ExpressionResultScope(BytecodeGenerator* generator, Expression::Context kind)
      : generator_(generator),
        kind_(kind),
        outer_(generator->execution_result()),
        allocator_(generator),
        result_identified_(false) {
    generator_->set_execution_result(this);
  }

  virtual ~ExpressionResultScope() {
    generator_->set_execution_result(outer_);
    DCHECK(result_identified());
  }

  bool IsEffect() const { return kind_ == Expression::kEffect; }
  bool IsValue() const { return kind_ == Expression::kValue; }

  virtual void SetResultInAccumulator() = 0;
  virtual void SetResultInRegister(Register reg) = 0;

 protected:
  ExpressionResultScope* outer() const { return outer_; }
  BytecodeArrayBuilder* builder() const { return generator_->builder(); }
  const RegisterAllocationScope* allocator() const { return &allocator_; }

  void set_result_identified() {
    DCHECK(!result_identified());
    result_identified_ = true;
  }

  bool result_identified() const { return result_identified_; }

 private:
  BytecodeGenerator* generator_;
  Expression::Context kind_;
  ExpressionResultScope* outer_;
  RegisterAllocationScope allocator_;
  bool result_identified_;

  DISALLOW_COPY_AND_ASSIGN(ExpressionResultScope);
};


// Scoped class used when the result of the current expression is not
// expected to produce a result.
class BytecodeGenerator::EffectResultScope final
    : public ExpressionResultScope {
 public:
  explicit EffectResultScope(BytecodeGenerator* generator)
      : ExpressionResultScope(generator, Expression::kEffect) {
    set_result_identified();
  }

  virtual void SetResultInAccumulator() {}
  virtual void SetResultInRegister(Register reg) {}
};


// Scoped class used when the result of the current expression to be
// evaluated should go into the interpreter's accumulator register.
class BytecodeGenerator::AccumulatorResultScope final
    : public ExpressionResultScope {
 public:
  explicit AccumulatorResultScope(BytecodeGenerator* generator)
      : ExpressionResultScope(generator, Expression::kValue) {}

  virtual void SetResultInAccumulator() { set_result_identified(); }

  virtual void SetResultInRegister(Register reg) {
    builder()->LoadAccumulatorWithRegister(reg);
    set_result_identified();
  }
};


// Scoped class used when the result of the current expression to be
// evaluated should go into an interpreter register.
class BytecodeGenerator::RegisterResultScope final
    : public ExpressionResultScope {
 public:
  explicit RegisterResultScope(BytecodeGenerator* generator)
      : ExpressionResultScope(generator, Expression::kValue) {}

  virtual void SetResultInAccumulator() {
    result_register_ = allocator()->outer()->NewRegister();
    builder()->StoreAccumulatorInRegister(result_register_);
    set_result_identified();
  }

  virtual void SetResultInRegister(Register reg) {
    DCHECK(builder()->RegisterIsParameterOrLocal(reg) ||
           (builder()->TemporaryRegisterIsLive(reg) &&
            !allocator()->RegisterIsAllocatedInThisScope(reg)));
    result_register_ = reg;
    set_result_identified();
  }

  Register ResultRegister() const { return result_register_; }

 private:
  Register result_register_;
};

BytecodeGenerator::BytecodeGenerator(Isolate* isolate, Zone* zone)
    : isolate_(isolate),
      zone_(zone),
      builder_(nullptr),
      info_(nullptr),
      scope_(nullptr),
      globals_(0, zone),
      execution_control_(nullptr),
      execution_context_(nullptr),
      execution_result_(nullptr),
      register_allocator_(nullptr),
      try_catch_nesting_level_(0),
      try_finally_nesting_level_(0) {
  InitializeAstVisitor(isolate);
}

Handle<BytecodeArray> BytecodeGenerator::MakeBytecode(CompilationInfo* info) {
  set_info(info);
  set_scope(info->scope());

  // Initialize bytecode array builder.
  set_builder(new (zone()) BytecodeArrayBuilder(
      isolate(), zone(), info->num_parameters_including_this(),
      scope()->MaxNestedContextChainLength(), scope()->num_stack_slots()));

  // Initialize the incoming context.
  ContextScope incoming_context(this, scope(), false);

  // Initialize control scope.
  ControlScopeForTopLevel control(this);

  // Build function context only if there are context allocated variables.
  if (scope()->NeedsContext()) {
    // Push a new inner context scope for the function.
    VisitNewLocalFunctionContext();
    ContextScope local_function_context(this, scope(), false);
    VisitBuildLocalActivationContext();
    MakeBytecodeBody();
  } else {
    MakeBytecodeBody();
  }

  builder()->EnsureReturn(info->literal());
  set_scope(nullptr);
  set_info(nullptr);
  return builder()->ToBytecodeArray();
}


void BytecodeGenerator::MakeBytecodeBody() {
  // Build the arguments object if it is used.
  VisitArgumentsObject(scope()->arguments());

  // Build rest arguments array if it is used.
  int rest_index;
  Variable* rest_parameter = scope()->rest_parameter(&rest_index);
  VisitRestArgumentsArray(rest_parameter);

  // Build assignment to {.this_function} variable if it is used.
  VisitThisFunctionVariable(scope()->this_function_var());

  // Build assignment to {new.target} variable if it is used.
  VisitNewTargetVariable(scope()->new_target_var());

  // TODO(rmcilroy): Emit tracing call if requested to do so.
  if (FLAG_trace) {
    UNIMPLEMENTED();
  }

  // Visit illegal re-declaration and bail out if it exists.
  if (scope()->HasIllegalRedeclaration()) {
    VisitForEffect(scope()->GetIllegalRedeclaration());
    return;
  }

  // Visit declarations within the function scope.
  VisitDeclarations(scope()->declarations());

  // Perform a stack-check before the body.
  builder()->StackCheck();

  // Visit statements in the function body.
  VisitStatements(info()->literal()->body());
}


void BytecodeGenerator::VisitBlock(Block* stmt) {
  // Visit declarations and statements.
  if (stmt->scope() != nullptr && stmt->scope()->NeedsContext()) {
    VisitNewLocalBlockContext(stmt->scope());
    ContextScope scope(this, stmt->scope());
    VisitBlockDeclarationsAndStatements(stmt);
  } else {
    VisitBlockDeclarationsAndStatements(stmt);
  }
}


void BytecodeGenerator::VisitBlockDeclarationsAndStatements(Block* stmt) {
  BlockBuilder block_builder(builder());
  ControlScopeForBreakable execution_control(this, stmt, &block_builder);
  if (stmt->scope() != nullptr) {
    VisitDeclarations(stmt->scope()->declarations());
  }
  VisitStatements(stmt->statements());
  if (stmt->labels() != nullptr) block_builder.EndBlock();
}


void BytecodeGenerator::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  VariableMode mode = decl->mode();
  // Const and let variables are initialized with the hole so that we can
  // check that they are only assigned once.
  bool hole_init = mode == CONST || mode == CONST_LEGACY || mode == LET;
  switch (variable->location()) {
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED: {
      Handle<Oddball> value = variable->binding_needs_init()
                                  ? isolate()->factory()->the_hole_value()
                                  : isolate()->factory()->undefined_value();
      globals()->push_back(variable->name());
      globals()->push_back(value);
      break;
    }
    case VariableLocation::LOCAL:
      if (hole_init) {
        Register destination(variable->index());
        builder()->LoadTheHole().StoreAccumulatorInRegister(destination);
      }
      break;
    case VariableLocation::PARAMETER:
      if (hole_init) {
        // The parameter indices are shifted by 1 (receiver is variable
        // index -1 but is parameter index 0 in BytecodeArrayBuilder).
        Register destination(builder()->Parameter(variable->index() + 1));
        builder()->LoadTheHole().StoreAccumulatorInRegister(destination);
      }
      break;
    case VariableLocation::CONTEXT:
      if (hole_init) {
        builder()->LoadTheHole().StoreContextSlot(execution_context()->reg(),
                                                  variable->index());
      }
      break;
    case VariableLocation::LOOKUP: {
      DCHECK(IsDeclaredVariableMode(mode));

      register_allocator()->PrepareForConsecutiveAllocations(3);
      Register name = register_allocator()->NextConsecutiveRegister();
      Register init_value = register_allocator()->NextConsecutiveRegister();
      Register attributes = register_allocator()->NextConsecutiveRegister();

      builder()->LoadLiteral(variable->name()).StoreAccumulatorInRegister(name);
      if (hole_init) {
        builder()->LoadTheHole().StoreAccumulatorInRegister(init_value);
      } else {
        // For variables, we must not use an initial value (such as 'undefined')
        // because we may have a (legal) redeclaration and we must not destroy
        // the current value.
        builder()
            ->LoadLiteral(Smi::FromInt(0))
            .StoreAccumulatorInRegister(init_value);
      }
      builder()
          ->LoadLiteral(Smi::FromInt(variable->DeclarationPropertyAttributes()))
          .StoreAccumulatorInRegister(attributes)
          .CallRuntime(Runtime::kDeclareLookupSlot, name, 3);
      break;
    }
  }
}


void BytecodeGenerator::VisitFunctionDeclaration(FunctionDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  switch (variable->location()) {
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED: {
      Handle<SharedFunctionInfo> function = Compiler::GetSharedFunctionInfo(
          decl->fun(), info()->script(), info());
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals()->push_back(variable->name());
      globals()->push_back(function);
      break;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      VisitForAccumulatorValue(decl->fun());
      DCHECK(variable->mode() == LET || variable->mode() == VAR ||
             variable->mode() == CONST);
      VisitVariableAssignment(variable, Token::INIT,
                              FeedbackVectorSlot::Invalid());
      break;
    }
    case VariableLocation::CONTEXT: {
      DCHECK_EQ(0, execution_context()->ContextChainDepth(variable->scope()));
      VisitForAccumulatorValue(decl->fun());
      builder()->StoreContextSlot(execution_context()->reg(),
                                  variable->index());
      break;
    }
    case VariableLocation::LOOKUP: {
      register_allocator()->PrepareForConsecutiveAllocations(3);
      Register name = register_allocator()->NextConsecutiveRegister();
      Register literal = register_allocator()->NextConsecutiveRegister();
      Register attributes = register_allocator()->NextConsecutiveRegister();
      builder()->LoadLiteral(variable->name()).StoreAccumulatorInRegister(name);

      VisitForAccumulatorValue(decl->fun());
      builder()
          ->StoreAccumulatorInRegister(literal)
          .LoadLiteral(Smi::FromInt(variable->DeclarationPropertyAttributes()))
          .StoreAccumulatorInRegister(attributes)
          .CallRuntime(Runtime::kDeclareLookupSlot, name, 3);
    }
  }
}


void BytecodeGenerator::VisitImportDeclaration(ImportDeclaration* decl) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitExportDeclaration(ExportDeclaration* decl) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
  RegisterAllocationScope register_scope(this);
  DCHECK(globals()->empty());
  for (int i = 0; i < declarations->length(); i++) {
    RegisterAllocationScope register_scope(this);
    Visit(declarations->at(i));
  }
  if (globals()->empty()) return;
  int array_index = 0;
  Handle<FixedArray> data = isolate()->factory()->NewFixedArray(
      static_cast<int>(globals()->size()), TENURED);
  for (Handle<Object> obj : *globals()) data->set(array_index++, *obj);
  int encoded_flags = DeclareGlobalsEvalFlag::encode(info()->is_eval()) |
                      DeclareGlobalsNativeFlag::encode(info()->is_native()) |
                      DeclareGlobalsLanguageMode::encode(language_mode());

  Register pairs = register_allocator()->NewRegister();
  builder()->LoadLiteral(data);
  builder()->StoreAccumulatorInRegister(pairs);

  Register flags = register_allocator()->NewRegister();
  builder()->LoadLiteral(Smi::FromInt(encoded_flags));
  builder()->StoreAccumulatorInRegister(flags);
  DCHECK(flags.index() == pairs.index() + 1);

  builder()->CallRuntime(Runtime::kDeclareGlobals, pairs, 2);
  globals()->clear();
}


void BytecodeGenerator::VisitStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    // Allocate an outer register allocations scope for the statement.
    RegisterAllocationScope allocation_scope(this);
    Statement* stmt = statements->at(i);
    Visit(stmt);
    if (stmt->IsJump()) break;
  }
}


void BytecodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  builder()->SetStatementPosition(stmt);
  VisitForEffect(stmt->expression());
}


void BytecodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
}


void BytecodeGenerator::VisitIfStatement(IfStatement* stmt) {
  BytecodeLabel else_label, end_label;
  if (stmt->condition()->ToBooleanIsTrue()) {
    // Generate then block unconditionally as always true.
    Visit(stmt->then_statement());
  } else if (stmt->condition()->ToBooleanIsFalse()) {
    // Generate else block unconditionally if it exists.
    if (stmt->HasElseStatement()) {
      Visit(stmt->else_statement());
    }
  } else {
    // TODO(oth): If then statement is BreakStatement or
    // ContinueStatement we can reduce number of generated
    // jump/jump_ifs here. See BasicLoops test.
    VisitForAccumulatorValue(stmt->condition());
    builder()->JumpIfFalse(&else_label);
    Visit(stmt->then_statement());
    if (stmt->HasElseStatement()) {
      builder()->Jump(&end_label);
      builder()->Bind(&else_label);
      Visit(stmt->else_statement());
    } else {
      builder()->Bind(&else_label);
    }
    builder()->Bind(&end_label);
  }
}


void BytecodeGenerator::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* stmt) {
  Visit(stmt->statement());
}


void BytecodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  execution_control()->Continue(stmt->target());
}


void BytecodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  execution_control()->Break(stmt->target());
}


void BytecodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  VisitForAccumulatorValue(stmt->expression());
  builder()->SetStatementPosition(stmt);
  execution_control()->ReturnAccumulator();
}


void BytecodeGenerator::VisitWithStatement(WithStatement* stmt) {
  VisitForAccumulatorValue(stmt->expression());
  builder()->CastAccumulatorToJSObject();
  VisitNewLocalWithContext();
  VisitInScope(stmt->statement(), stmt->scope());
}


void BytecodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  // We need this scope because we visit for register values. We have to
  // maintain a execution result scope where registers can be allocated.
  ZoneList<CaseClause*>* clauses = stmt->cases();
  SwitchBuilder switch_builder(builder(), clauses->length());
  ControlScopeForBreakable scope(this, stmt, &switch_builder);
  int default_index = -1;

  // Keep the switch value in a register until a case matches.
  Register tag = VisitForRegisterValue(stmt->tag());

  // Iterate over all cases and create nodes for label comparison.
  BytecodeLabel done_label;
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);

    // The default is not a test, remember index.
    if (clause->is_default()) {
      default_index = i;
      continue;
    }

    // Perform label comparison as if via '===' with tag.
    VisitForAccumulatorValue(clause->label());
    builder()->CompareOperation(Token::Value::EQ_STRICT, tag);
    switch_builder.Case(i);
  }

  if (default_index >= 0) {
    // Emit default jump if there is a default case.
    switch_builder.DefaultAt(default_index);
  } else {
    // Otherwise if we have reached here none of the cases matched, so jump to
    // done.
    builder()->Jump(&done_label);
  }

  // Iterate over all cases and create the case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);
    switch_builder.SetCaseTarget(i);
    VisitStatements(clause->statements());
  }
  builder()->Bind(&done_label);

  switch_builder.SetBreakTarget(done_label);
}


void BytecodeGenerator::VisitCaseClause(CaseClause* clause) {
  // Handled entirely in VisitSwitchStatement.
  UNREACHABLE();
}

void BytecodeGenerator::VisitIterationBody(IterationStatement* stmt,
                                           LoopBuilder* loop_builder) {
  ControlScopeForIteration execution_control(this, stmt, loop_builder);
  builder()->StackCheck();
  Visit(stmt->body());
}

void BytecodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  LoopBuilder loop_builder(builder());
  loop_builder.LoopHeader();
  if (stmt->cond()->ToBooleanIsFalse()) {
    VisitIterationBody(stmt, &loop_builder);
    loop_builder.Condition();
  } else if (stmt->cond()->ToBooleanIsTrue()) {
    loop_builder.Condition();
    VisitIterationBody(stmt, &loop_builder);
    loop_builder.JumpToHeader();
  } else {
    VisitIterationBody(stmt, &loop_builder);
    loop_builder.Condition();
    VisitForAccumulatorValue(stmt->cond());
    loop_builder.JumpToHeaderIfTrue();
  }
  loop_builder.EndLoop();
}

void BytecodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  if (stmt->cond()->ToBooleanIsFalse()) {
    // If the condition is false there is no need to generate the loop.
    return;
  }

  LoopBuilder loop_builder(builder());
  loop_builder.LoopHeader();
  loop_builder.Condition();
  if (!stmt->cond()->ToBooleanIsTrue()) {
    VisitForAccumulatorValue(stmt->cond());
    loop_builder.BreakIfFalse();
  }
  VisitIterationBody(stmt, &loop_builder);
  loop_builder.JumpToHeader();
  loop_builder.EndLoop();
}


void BytecodeGenerator::VisitForStatement(ForStatement* stmt) {
  if (stmt->init() != nullptr) {
    Visit(stmt->init());
  }
  if (stmt->cond() && stmt->cond()->ToBooleanIsFalse()) {
    // If the condition is known to be false there is no need to generate
    // body, next or condition blocks. Init block should be generated.
    return;
  }

  LoopBuilder loop_builder(builder());
  loop_builder.LoopHeader();
  loop_builder.Condition();
  if (stmt->cond() && !stmt->cond()->ToBooleanIsTrue()) {
    VisitForAccumulatorValue(stmt->cond());
    loop_builder.BreakIfFalse();
  }
  VisitIterationBody(stmt, &loop_builder);
  if (stmt->next() != nullptr) {
    loop_builder.Next();
    Visit(stmt->next());
  }
  loop_builder.JumpToHeader();
  loop_builder.EndLoop();
}


void BytecodeGenerator::VisitForInAssignment(Expression* expr,
                                             FeedbackVectorSlot slot) {
  DCHECK(expr->IsValidReferenceExpression());

  // Evaluate assignment starting with the value to be stored in the
  // accumulator.
  Property* property = expr->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->AsVariableProxy()->var();
      VisitVariableAssignment(variable, Token::ASSIGN, slot);
      break;
    }
    case NAMED_PROPERTY: {
      RegisterAllocationScope register_scope(this);
      Register value = register_allocator()->NewRegister();
      builder()->StoreAccumulatorInRegister(value);
      Register object = VisitForRegisterValue(property->obj());
      Handle<String> name = property->key()->AsLiteral()->AsPropertyName();
      builder()->LoadAccumulatorWithRegister(value);
      builder()->StoreNamedProperty(object, name, feedback_index(slot),
                                    language_mode());
      break;
    }
    case KEYED_PROPERTY: {
      RegisterAllocationScope register_scope(this);
      Register value = register_allocator()->NewRegister();
      builder()->StoreAccumulatorInRegister(value);
      Register object = VisitForRegisterValue(property->obj());
      Register key = VisitForRegisterValue(property->key());
      builder()->LoadAccumulatorWithRegister(value);
      builder()->StoreKeyedProperty(object, key, feedback_index(slot),
                                    language_mode());
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      RegisterAllocationScope register_scope(this);
      register_allocator()->PrepareForConsecutiveAllocations(4);
      Register receiver = register_allocator()->NextConsecutiveRegister();
      Register home_object = register_allocator()->NextConsecutiveRegister();
      Register name = register_allocator()->NextConsecutiveRegister();
      Register value = register_allocator()->NextConsecutiveRegister();
      builder()->StoreAccumulatorInRegister(value);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), receiver);
      VisitForRegisterValue(super_property->home_object(), home_object);
      builder()
          ->LoadLiteral(property->key()->AsLiteral()->AsPropertyName())
          .StoreAccumulatorInRegister(name);
      BuildNamedSuperPropertyStore(receiver, home_object, name, value);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      RegisterAllocationScope register_scope(this);
      register_allocator()->PrepareForConsecutiveAllocations(4);
      Register receiver = register_allocator()->NextConsecutiveRegister();
      Register home_object = register_allocator()->NextConsecutiveRegister();
      Register key = register_allocator()->NextConsecutiveRegister();
      Register value = register_allocator()->NextConsecutiveRegister();
      builder()->StoreAccumulatorInRegister(value);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), receiver);
      VisitForRegisterValue(super_property->home_object(), home_object);
      VisitForRegisterValue(property->key(), key);
      BuildKeyedSuperPropertyStore(receiver, home_object, key, value);
      break;
    }
  }
}


void BytecodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  if (stmt->subject()->IsNullLiteral() ||
      stmt->subject()->IsUndefinedLiteral(isolate())) {
    // ForIn generates lots of code, skip if it wouldn't produce any effects.
    return;
  }

  LoopBuilder loop_builder(builder());
  BytecodeLabel subject_null_label, subject_undefined_label, not_object_label;

  // Prepare the state for executing ForIn.
  VisitForAccumulatorValue(stmt->subject());
  builder()->JumpIfUndefined(&subject_undefined_label);
  builder()->JumpIfNull(&subject_null_label);
  Register receiver = register_allocator()->NewRegister();
  builder()->CastAccumulatorToJSObject();
  builder()->JumpIfNull(&not_object_label);
  builder()->StoreAccumulatorInRegister(receiver);

  register_allocator()->PrepareForConsecutiveAllocations(3);
  Register cache_type = register_allocator()->NextConsecutiveRegister();
  Register cache_array = register_allocator()->NextConsecutiveRegister();
  Register cache_length = register_allocator()->NextConsecutiveRegister();
  // Used as kRegTriple8 and kRegPair8 in ForInPrepare and ForInNext.
  USE(cache_array);
  builder()->ForInPrepare(cache_type);

  // Set up loop counter
  Register index = register_allocator()->NewRegister();
  builder()->LoadLiteral(Smi::FromInt(0));
  builder()->StoreAccumulatorInRegister(index);

  // The loop
  loop_builder.LoopHeader();
  loop_builder.Condition();
  builder()->ForInDone(index, cache_length);
  loop_builder.BreakIfTrue();
  DCHECK(Register::AreContiguous(cache_type, cache_array));
  builder()->ForInNext(receiver, index, cache_type);
  loop_builder.ContinueIfUndefined();
  VisitForInAssignment(stmt->each(), stmt->EachFeedbackSlot());
  VisitIterationBody(stmt, &loop_builder);
  loop_builder.Next();
  builder()->ForInStep(index);
  builder()->StoreAccumulatorInRegister(index);
  loop_builder.JumpToHeader();
  loop_builder.EndLoop();
  builder()->Bind(&not_object_label);
  builder()->Bind(&subject_null_label);
  builder()->Bind(&subject_undefined_label);
}


void BytecodeGenerator::VisitForOfStatement(ForOfStatement* stmt) {
  LoopBuilder loop_builder(builder());
  ControlScopeForIteration control_scope(this, stmt, &loop_builder);

  VisitForEffect(stmt->assign_iterator());

  loop_builder.LoopHeader();
  loop_builder.Next();
  VisitForEffect(stmt->next_result());
  VisitForAccumulatorValue(stmt->result_done());
  loop_builder.BreakIfTrue();

  VisitForEffect(stmt->assign_each());
  VisitIterationBody(stmt, &loop_builder);
  loop_builder.JumpToHeader();
  loop_builder.EndLoop();
}


void BytecodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  TryCatchBuilder try_control_builder(builder());
  Register no_reg;

  // Preserve the context in a dedicated register, so that it can be restored
  // when the handler is entered by the stack-unwinding machinery.
  // TODO(mstarzinger): Be smarter about register allocation.
  Register context = register_allocator()->NewRegister();
  builder()->MoveRegister(Register::current_context(), context);

  // Evaluate the try-block inside a control scope. This simulates a handler
  // that is intercepting 'throw' control commands.
  try_control_builder.BeginTry(context);
  {
    ControlScopeForTryCatch scope(this, &try_control_builder);
    Visit(stmt->try_block());
  }
  try_control_builder.EndTry();

  // Create a catch scope that binds the exception.
  VisitNewLocalCatchContext(stmt->variable());
  builder()->StoreAccumulatorInRegister(context);

  // Clear message object as we enter the catch block.
  builder()->CallRuntime(Runtime::kInterpreterClearPendingMessage, no_reg, 0);

  // Load the catch context into the accumulator.
  builder()->LoadAccumulatorWithRegister(context);

  // Evaluate the catch-block.
  VisitInScope(stmt->catch_block(), stmt->scope());
  try_control_builder.EndCatch();
}


void BytecodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  TryFinallyBuilder try_control_builder(builder(), IsInsideTryCatch());
  Register no_reg;

  // We keep a record of all paths that enter the finally-block to be able to
  // dispatch to the correct continuation point after the statements in the
  // finally-block have been evaluated.
  //
  // The try-finally construct can enter the finally-block in three ways:
  // 1. By exiting the try-block normally, falling through at the end.
  // 2. By exiting the try-block with a function-local control flow transfer
  //    (i.e. through break/continue/return statements).
  // 3. By exiting the try-block with a thrown exception.
  //
  // The result register semantics depend on how the block was entered:
  //  - ReturnStatement: It represents the return value being returned.
  //  - ThrowStatement: It represents the exception being thrown.
  //  - BreakStatement/ContinueStatement: Undefined and not used.
  //  - Falling through into finally-block: Undefined and not used.
  Register token = register_allocator()->NewRegister();
  Register result = register_allocator()->NewRegister();
  ControlScope::DeferredCommands commands(this, token, result);

  // Preserve the context in a dedicated register, so that it can be restored
  // when the handler is entered by the stack-unwinding machinery.
  // TODO(mstarzinger): Be smarter about register allocation.
  Register context = register_allocator()->NewRegister();
  builder()->MoveRegister(Register::current_context(), context);

  // Evaluate the try-block inside a control scope. This simulates a handler
  // that is intercepting all control commands.
  try_control_builder.BeginTry(context);
  {
    ControlScopeForTryFinally scope(this, &try_control_builder, &commands);
    Visit(stmt->try_block());
  }
  try_control_builder.EndTry();

  // Record fall-through and exception cases.
  commands.RecordFallThroughPath();
  try_control_builder.LeaveTry();
  try_control_builder.BeginHandler();
  commands.RecordHandlerReThrowPath();

  // Pending message object is saved on entry.
  try_control_builder.BeginFinally();
  Register message = context;  // Reuse register.

  // Clear message object as we enter the finally block.
  builder()
      ->CallRuntime(Runtime::kInterpreterClearPendingMessage, no_reg, 0)
      .StoreAccumulatorInRegister(message);

  // Evaluate the finally-block.
  Visit(stmt->finally_block());
  try_control_builder.EndFinally();

  // Pending message object is restored on exit.
  builder()->CallRuntime(Runtime::kInterpreterSetPendingMessage, message, 1);

  // Dynamic dispatch after the finally-block.
  commands.ApplyDeferredCommands();
}


void BytecodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
  builder()->SetStatementPosition(stmt);
  builder()->Debugger();
}


void BytecodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  // Find or build a shared function info.
  Handle<SharedFunctionInfo> shared_info =
      Compiler::GetSharedFunctionInfo(expr, info()->script(), info());
  CHECK(!shared_info.is_null());  // TODO(rmcilroy): Set stack overflow?
  builder()->CreateClosure(shared_info,
                           expr->pretenure() ? TENURED : NOT_TENURED);
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitClassLiteral(ClassLiteral* expr) {
  if (expr->scope()->ContextLocalCount() > 0) {
    VisitNewLocalBlockContext(expr->scope());
    ContextScope scope(this, expr->scope());
    VisitDeclarations(expr->scope()->declarations());
    VisitClassLiteralContents(expr);
  } else {
    VisitDeclarations(expr->scope()->declarations());
    VisitClassLiteralContents(expr);
  }
}

void BytecodeGenerator::VisitClassLiteralContents(ClassLiteral* expr) {
  VisitClassLiteralForRuntimeDefinition(expr);

  // Load the "prototype" from the constructor.
  register_allocator()->PrepareForConsecutiveAllocations(2);
  Register literal = register_allocator()->NextConsecutiveRegister();
  Register prototype = register_allocator()->NextConsecutiveRegister();
  Handle<String> name = isolate()->factory()->prototype_string();
  FeedbackVectorSlot slot = expr->PrototypeSlot();
  builder()
      ->StoreAccumulatorInRegister(literal)
      .LoadNamedProperty(literal, name, feedback_index(slot))
      .StoreAccumulatorInRegister(prototype);

  VisitClassLiteralProperties(expr, literal, prototype);
  builder()->CallRuntime(Runtime::kFinalizeClassDefinition, literal, 2);
  // Assign to class variable.
  if (expr->class_variable_proxy() != nullptr) {
    Variable* var = expr->class_variable_proxy()->var();
    FeedbackVectorSlot slot = expr->NeedsProxySlot()
                                  ? expr->ProxySlot()
                                  : FeedbackVectorSlot::Invalid();
    VisitVariableAssignment(var, Token::INIT, slot);
  }
  execution_result()->SetResultInAccumulator();
}

void BytecodeGenerator::VisitClassLiteralForRuntimeDefinition(
    ClassLiteral* expr) {
  AccumulatorResultScope result_scope(this);
  register_allocator()->PrepareForConsecutiveAllocations(4);
  Register extends = register_allocator()->NextConsecutiveRegister();
  Register constructor = register_allocator()->NextConsecutiveRegister();
  Register start_position = register_allocator()->NextConsecutiveRegister();
  Register end_position = register_allocator()->NextConsecutiveRegister();

  VisitForAccumulatorValueOrTheHole(expr->extends());
  builder()->StoreAccumulatorInRegister(extends);

  VisitForAccumulatorValue(expr->constructor());
  builder()
      ->StoreAccumulatorInRegister(constructor)
      .LoadLiteral(Smi::FromInt(expr->start_position()))
      .StoreAccumulatorInRegister(start_position)
      .LoadLiteral(Smi::FromInt(expr->end_position()))
      .StoreAccumulatorInRegister(end_position)
      .CallRuntime(Runtime::kDefineClass, extends, 4);
  result_scope.SetResultInAccumulator();
}

void BytecodeGenerator::VisitClassLiteralProperties(ClassLiteral* expr,
                                                    Register literal,
                                                    Register prototype) {
  RegisterAllocationScope register_scope(this);
  register_allocator()->PrepareForConsecutiveAllocations(5);
  Register receiver = register_allocator()->NextConsecutiveRegister();
  Register key = register_allocator()->NextConsecutiveRegister();
  Register value = register_allocator()->NextConsecutiveRegister();
  Register attr = register_allocator()->NextConsecutiveRegister();
  Register set_function_name = register_allocator()->NextConsecutiveRegister();

  bool attr_assigned = false;
  Register old_receiver = Register::invalid_value();

  // Create nodes to store method values into the literal.
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);

    // Set-up receiver.
    Register new_receiver = property->is_static() ? literal : prototype;
    if (new_receiver != old_receiver) {
      builder()->MoveRegister(new_receiver, receiver);
      old_receiver = new_receiver;
    }

    VisitForAccumulatorValue(property->key());
    builder()->CastAccumulatorToName().StoreAccumulatorInRegister(key);
    // The static prototype property is read only. We handle the non computed
    // property name case in the parser. Since this is the only case where we
    // need to check for an own read only property we special case this so we do
    // not need to do this for every property.
    if (property->is_static() && property->is_computed_name()) {
      VisitClassLiteralStaticPrototypeWithComputedName(key);
    }
    VisitForAccumulatorValue(property->value());
    builder()->StoreAccumulatorInRegister(value);

    VisitSetHomeObject(value, receiver, property);

    if (!attr_assigned) {
      builder()
          ->LoadLiteral(Smi::FromInt(DONT_ENUM))
          .StoreAccumulatorInRegister(attr);
      attr_assigned = true;
    }

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
      case ObjectLiteral::Property::PROTOTYPE:
        // Invalid properties for ES6 classes.
        UNREACHABLE();
        break;
      case ObjectLiteral::Property::COMPUTED: {
        builder()
            ->LoadLiteral(Smi::FromInt(property->NeedsSetFunctionName()))
            .StoreAccumulatorInRegister(set_function_name);
        builder()->CallRuntime(Runtime::kDefineDataPropertyInLiteral, receiver,
                               5);
        break;
      }
      case ObjectLiteral::Property::GETTER: {
        builder()->CallRuntime(Runtime::kDefineGetterPropertyUnchecked,
                               receiver, 4);
        break;
      }
      case ObjectLiteral::Property::SETTER: {
        builder()->CallRuntime(Runtime::kDefineSetterPropertyUnchecked,
                               receiver, 4);
        break;
      }
    }
  }
}

void BytecodeGenerator::VisitClassLiteralStaticPrototypeWithComputedName(
    Register key) {
  BytecodeLabel done;
  builder()
      ->LoadLiteral(isolate()->factory()->prototype_string())
      .CompareOperation(Token::Value::EQ_STRICT, key)
      .JumpIfFalse(&done)
      .CallRuntime(Runtime::kThrowStaticPrototypeError, Register(0), 0)
      .Bind(&done);
}

void BytecodeGenerator::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
  // Find or build a shared function info for the native function template.
  Handle<SharedFunctionInfo> shared_info =
      Compiler::GetSharedFunctionInfoForNative(expr->extension(), expr->name());
  builder()->CreateClosure(shared_info, NOT_TENURED);
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitDoExpression(DoExpression* expr) {
  VisitBlock(expr->block());
  VisitVariableProxy(expr->result());
}


void BytecodeGenerator::VisitConditional(Conditional* expr) {
  // TODO(rmcilroy): Spot easy cases where there code would not need to
  // emit the then block or the else block, e.g. condition is
  // obviously true/1/false/0.

  BytecodeLabel else_label, end_label;

  VisitForAccumulatorValue(expr->condition());
  builder()->JumpIfFalse(&else_label);

  VisitForAccumulatorValue(expr->then_expression());
  builder()->Jump(&end_label);

  builder()->Bind(&else_label);
  VisitForAccumulatorValue(expr->else_expression());
  builder()->Bind(&end_label);

  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitLiteral(Literal* expr) {
  if (!execution_result()->IsEffect()) {
    Handle<Object> value = expr->value();
    if (value->IsSmi()) {
      builder()->LoadLiteral(Smi::cast(*value));
    } else if (value->IsUndefined()) {
      builder()->LoadUndefined();
    } else if (value->IsTrue()) {
      builder()->LoadTrue();
    } else if (value->IsFalse()) {
      builder()->LoadFalse();
    } else if (value->IsNull()) {
      builder()->LoadNull();
    } else if (value->IsTheHole()) {
      builder()->LoadTheHole();
    } else {
      builder()->LoadLiteral(value);
    }
    execution_result()->SetResultInAccumulator();
  }
}


void BytecodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  // Materialize a regular expression literal.
  builder()->CreateRegExpLiteral(expr->pattern(), expr->literal_index(),
                                 expr->flags());
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  // Deep-copy the literal boilerplate.
  builder()->CreateObjectLiteral(expr->constant_properties(),
                                 expr->literal_index(),
                                 expr->ComputeFlags(true));

  // Allocate in the outer scope since this register is used to return the
  // expression's results to the caller.
  Register literal = register_allocator()->outer()->NewRegister();
  builder()->StoreAccumulatorInRegister(literal);

  // Store computed values into the literal.
  int property_index = 0;
  AccessorTable accessor_table(zone());
  for (; property_index < expr->properties()->length(); property_index++) {
    ObjectLiteral::Property* property = expr->properties()->at(property_index);
    if (property->is_computed_name()) break;
    if (property->IsCompileTimeValue()) continue;

    RegisterAllocationScope inner_register_scope(this);
    Literal* literal_key = property->key()->AsLiteral();
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(property->value()));
      // Fall through.
      case ObjectLiteral::Property::COMPUTED: {
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (literal_key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(property->value());
            if (FunctionLiteral::NeedsHomeObject(property->value())) {
              RegisterAllocationScope register_scope(this);
              Register value = register_allocator()->NewRegister();
              builder()->StoreAccumulatorInRegister(value);
              builder()->StoreNamedProperty(
                  literal, literal_key->AsPropertyName(),
                  feedback_index(property->GetSlot(0)), language_mode());
              VisitSetHomeObject(value, literal, property, 1);
            } else {
              builder()->StoreNamedProperty(
                  literal, literal_key->AsPropertyName(),
                  feedback_index(property->GetSlot(0)), language_mode());
            }
          } else {
            VisitForEffect(property->value());
          }
        } else {
          register_allocator()->PrepareForConsecutiveAllocations(4);
          Register literal_argument =
              register_allocator()->NextConsecutiveRegister();
          Register key = register_allocator()->NextConsecutiveRegister();
          Register value = register_allocator()->NextConsecutiveRegister();
          Register language = register_allocator()->NextConsecutiveRegister();

          builder()->MoveRegister(literal, literal_argument);
          VisitForAccumulatorValue(property->key());
          builder()->StoreAccumulatorInRegister(key);
          VisitForAccumulatorValue(property->value());
          builder()->StoreAccumulatorInRegister(value);
          if (property->emit_store()) {
            builder()
                ->LoadLiteral(Smi::FromInt(SLOPPY))
                .StoreAccumulatorInRegister(language)
                .CallRuntime(Runtime::kSetProperty, literal_argument, 4);
            VisitSetHomeObject(value, literal, property);
          }
        }
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE: {
        DCHECK(property->emit_store());
        register_allocator()->PrepareForConsecutiveAllocations(2);
        Register literal_argument =
            register_allocator()->NextConsecutiveRegister();
        Register value = register_allocator()->NextConsecutiveRegister();

        builder()->MoveRegister(literal, literal_argument);
        VisitForAccumulatorValue(property->value());
        builder()->StoreAccumulatorInRegister(value).CallRuntime(
            Runtime::kInternalSetPrototype, literal_argument, 2);
        break;
      }
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          accessor_table.lookup(literal_key)->second->getter = property;
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          accessor_table.lookup(literal_key)->second->setter = property;
        }
        break;
    }
  }

  // Define accessors, using only a single call to the runtime for each pair of
  // corresponding getters and setters.
  for (AccessorTable::Iterator it = accessor_table.begin();
       it != accessor_table.end(); ++it) {
    RegisterAllocationScope inner_register_scope(this);
    register_allocator()->PrepareForConsecutiveAllocations(5);
    Register literal_argument = register_allocator()->NextConsecutiveRegister();
    Register name = register_allocator()->NextConsecutiveRegister();
    Register getter = register_allocator()->NextConsecutiveRegister();
    Register setter = register_allocator()->NextConsecutiveRegister();
    Register attr = register_allocator()->NextConsecutiveRegister();

    builder()->MoveRegister(literal, literal_argument);
    VisitForAccumulatorValue(it->first);
    builder()->StoreAccumulatorInRegister(name);
    VisitObjectLiteralAccessor(literal, it->second->getter, getter);
    VisitObjectLiteralAccessor(literal, it->second->setter, setter);
    builder()
        ->LoadLiteral(Smi::FromInt(NONE))
        .StoreAccumulatorInRegister(attr)
        .CallRuntime(Runtime::kDefineAccessorPropertyUnchecked,
                     literal_argument, 5);
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
    RegisterAllocationScope inner_register_scope(this);

    if (property->kind() == ObjectLiteral::Property::PROTOTYPE) {
      DCHECK(property->emit_store());
      register_allocator()->PrepareForConsecutiveAllocations(2);
      Register literal_argument =
          register_allocator()->NextConsecutiveRegister();
      Register value = register_allocator()->NextConsecutiveRegister();

      builder()->MoveRegister(literal, literal_argument);
      VisitForAccumulatorValue(property->value());
      builder()->StoreAccumulatorInRegister(value).CallRuntime(
          Runtime::kInternalSetPrototype, literal_argument, 2);
      continue;
    }

    register_allocator()->PrepareForConsecutiveAllocations(5);
    Register literal_argument = register_allocator()->NextConsecutiveRegister();
    Register key = register_allocator()->NextConsecutiveRegister();
    Register value = register_allocator()->NextConsecutiveRegister();
    Register attr = register_allocator()->NextConsecutiveRegister();
    DCHECK(Register::AreContiguous(literal_argument, key, value, attr));
    Register set_function_name =
        register_allocator()->NextConsecutiveRegister();

    builder()->MoveRegister(literal, literal_argument);
    VisitForAccumulatorValue(property->key());
    builder()->CastAccumulatorToName().StoreAccumulatorInRegister(key);
    VisitForAccumulatorValue(property->value());
    builder()->StoreAccumulatorInRegister(value);
    VisitSetHomeObject(value, literal, property);
    builder()->LoadLiteral(Smi::FromInt(NONE)).StoreAccumulatorInRegister(attr);
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::COMPUTED:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        builder()
            ->LoadLiteral(Smi::FromInt(property->NeedsSetFunctionName()))
            .StoreAccumulatorInRegister(set_function_name);
        builder()->CallRuntime(Runtime::kDefineDataPropertyInLiteral,
                               literal_argument, 5);
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        UNREACHABLE();  // Handled specially above.
        break;
      case ObjectLiteral::Property::GETTER:
        builder()->CallRuntime(Runtime::kDefineGetterPropertyUnchecked,
                               literal_argument, 4);
        break;
      case ObjectLiteral::Property::SETTER:
        builder()->CallRuntime(Runtime::kDefineSetterPropertyUnchecked,
                               literal_argument, 4);
        break;
    }
  }

  // Transform literals that contain functions to fast properties.
  if (expr->has_function()) {
    builder()->CallRuntime(Runtime::kToFastProperties, literal, 1);
  }

  execution_result()->SetResultInRegister(literal);
}


void BytecodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  // Deep-copy the literal boilerplate.
  builder()->CreateArrayLiteral(expr->constant_elements(),
                                expr->literal_index(),
                                expr->ComputeFlags(true));
  Register index, literal;

  // Evaluate all the non-constant subexpressions and store them into the
  // newly cloned array.
  bool literal_in_accumulator = true;
  for (int array_index = 0; array_index < expr->values()->length();
       array_index++) {
    Expression* subexpr = expr->values()->at(array_index);
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;
    DCHECK(!subexpr->IsSpread());

    if (literal_in_accumulator) {
      index = register_allocator()->NewRegister();
      literal = register_allocator()->NewRegister();
      builder()->StoreAccumulatorInRegister(literal);
      literal_in_accumulator = false;
    }

    FeedbackVectorSlot slot = expr->LiteralFeedbackSlot();
    builder()
        ->LoadLiteral(Smi::FromInt(array_index))
        .StoreAccumulatorInRegister(index);
    VisitForAccumulatorValue(subexpr);
    builder()->StoreKeyedProperty(literal, index, feedback_index(slot),
                                  language_mode());
  }

  if (!literal_in_accumulator) {
    // Restore literal array into accumulator.
    builder()->LoadAccumulatorWithRegister(literal);
  }
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitVariableProxy(VariableProxy* proxy) {
  VisitVariableLoad(proxy->var(), proxy->VariableFeedbackSlot());
}

void BytecodeGenerator::BuildHoleCheckForVariableLoad(VariableMode mode,
                                                      Handle<String> name) {
  if (mode == CONST_LEGACY) {
    BytecodeLabel end_label;
    builder()->JumpIfNotHole(&end_label).LoadUndefined().Bind(&end_label);
  } else if (mode == LET || mode == CONST) {
    BuildThrowIfHole(name);
  }
}

void BytecodeGenerator::VisitVariableLoad(Variable* variable,
                                          FeedbackVectorSlot slot,
                                          TypeofMode typeof_mode) {
  VariableMode mode = variable->mode();
  switch (variable->location()) {
    case VariableLocation::LOCAL: {
      Register source(Register(variable->index()));
      builder()->LoadAccumulatorWithRegister(source);
      BuildHoleCheckForVariableLoad(mode, variable->name());
      execution_result()->SetResultInAccumulator();
      break;
    }
    case VariableLocation::PARAMETER: {
      // The parameter indices are shifted by 1 (receiver is variable
      // index -1 but is parameter index 0 in BytecodeArrayBuilder).
      Register source = builder()->Parameter(variable->index() + 1);
      builder()->LoadAccumulatorWithRegister(source);
      BuildHoleCheckForVariableLoad(mode, variable->name());
      execution_result()->SetResultInAccumulator();
      break;
    }
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED: {
      builder()->LoadGlobal(variable->name(), feedback_index(slot),
                            typeof_mode);
      execution_result()->SetResultInAccumulator();
      break;
    }
    case VariableLocation::CONTEXT: {
      int depth = execution_context()->ContextChainDepth(variable->scope());
      ContextScope* context = execution_context()->Previous(depth);
      Register context_reg;
      if (context) {
        context_reg = context->reg();
      } else {
        context_reg = register_allocator()->NewRegister();
        // Walk the context chain to find the context at the given depth.
        // TODO(rmcilroy): Perform this work in a bytecode handler once we have
        // a generic mechanism for performing jumps in interpreter.cc.
        // TODO(mythria): Also update bytecode graph builder with correct depth
        // when this changes.
        builder()
            ->LoadAccumulatorWithRegister(execution_context()->reg())
            .StoreAccumulatorInRegister(context_reg);
        for (int i = 0; i < depth; ++i) {
          builder()
              ->LoadContextSlot(context_reg, Context::PREVIOUS_INDEX)
              .StoreAccumulatorInRegister(context_reg);
        }
      }

      builder()->LoadContextSlot(context_reg, variable->index());
      BuildHoleCheckForVariableLoad(mode, variable->name());
      execution_result()->SetResultInAccumulator();
      break;
    }
    case VariableLocation::LOOKUP: {
      builder()->LoadLookupSlot(variable->name(), typeof_mode);
      execution_result()->SetResultInAccumulator();
      break;
    }
  }
}

void BytecodeGenerator::VisitVariableLoadForAccumulatorValue(
    Variable* variable, FeedbackVectorSlot slot, TypeofMode typeof_mode) {
  AccumulatorResultScope accumulator_result(this);
  VisitVariableLoad(variable, slot, typeof_mode);
}

Register BytecodeGenerator::VisitVariableLoadForRegisterValue(
    Variable* variable, FeedbackVectorSlot slot, TypeofMode typeof_mode) {
  RegisterResultScope register_scope(this);
  VisitVariableLoad(variable, slot, typeof_mode);
  return register_scope.ResultRegister();
}

void BytecodeGenerator::BuildNamedSuperPropertyLoad(Register receiver,
                                                    Register home_object,
                                                    Register name) {
  DCHECK(Register::AreContiguous(receiver, home_object, name));
  builder()->CallRuntime(Runtime::kLoadFromSuper, receiver, 3);
}

void BytecodeGenerator::BuildKeyedSuperPropertyLoad(Register receiver,
                                                    Register home_object,
                                                    Register key) {
  DCHECK(Register::AreContiguous(receiver, home_object, key));
  builder()->CallRuntime(Runtime::kLoadKeyedFromSuper, receiver, 3);
}

void BytecodeGenerator::BuildNamedSuperPropertyStore(Register receiver,
                                                     Register home_object,
                                                     Register name,
                                                     Register value) {
  DCHECK(Register::AreContiguous(receiver, home_object, name, value));
  Runtime::FunctionId function_id = is_strict(language_mode())
                                        ? Runtime::kStoreToSuper_Strict
                                        : Runtime::kStoreToSuper_Sloppy;
  builder()->CallRuntime(function_id, receiver, 4);
}

void BytecodeGenerator::BuildKeyedSuperPropertyStore(Register receiver,
                                                     Register home_object,
                                                     Register key,
                                                     Register value) {
  DCHECK(Register::AreContiguous(receiver, home_object, key, value));
  Runtime::FunctionId function_id = is_strict(language_mode())
                                        ? Runtime::kStoreKeyedToSuper_Strict
                                        : Runtime::kStoreKeyedToSuper_Sloppy;
  builder()->CallRuntime(function_id, receiver, 4);
}

void BytecodeGenerator::BuildThrowReferenceError(Handle<String> name) {
  RegisterAllocationScope register_scope(this);
  Register name_reg = register_allocator()->NewRegister();
  builder()->LoadLiteral(name).StoreAccumulatorInRegister(name_reg).CallRuntime(
      Runtime::kThrowReferenceError, name_reg, 1);
}

void BytecodeGenerator::BuildThrowIfHole(Handle<String> name) {
  // TODO(interpreter): Can the parser reduce the number of checks
  // performed? Or should there be a ThrowIfHole bytecode.
  BytecodeLabel no_reference_error;
  builder()->JumpIfNotHole(&no_reference_error);
  BuildThrowReferenceError(name);
  builder()->Bind(&no_reference_error);
}

void BytecodeGenerator::BuildThrowIfNotHole(Handle<String> name) {
  // TODO(interpreter): Can the parser reduce the number of checks
  // performed? Or should there be a ThrowIfNotHole bytecode.
  BytecodeLabel no_reference_error, reference_error;
  builder()
      ->JumpIfNotHole(&reference_error)
      .Jump(&no_reference_error)
      .Bind(&reference_error);
  BuildThrowReferenceError(name);
  builder()->Bind(&no_reference_error);
}

void BytecodeGenerator::BuildThrowReassignConstant(Handle<String> name) {
  // TODO(mythria): This will be replaced by a new bytecode that throws an
  // appropriate error depending on the whether the value is a hole or not.
  BytecodeLabel const_assign_error;
  builder()->JumpIfNotHole(&const_assign_error);
  BuildThrowReferenceError(name);
  builder()
      ->Bind(&const_assign_error)
      .CallRuntime(Runtime::kThrowConstAssignError, Register(), 0);
}

void BytecodeGenerator::BuildHoleCheckForVariableAssignment(Variable* variable,
                                                            Token::Value op) {
  VariableMode mode = variable->mode();
  DCHECK(mode != CONST_LEGACY);
  if (mode == CONST && op != Token::INIT) {
    // Non-intializing assignments to constant is not allowed.
    BuildThrowReassignConstant(variable->name());
  } else if (mode == LET && op != Token::INIT) {
    // Perform an initialization check for let declared variables.
    // E.g. let x = (x = 20); is not allowed.
    BuildThrowIfHole(variable->name());
  } else {
    DCHECK(variable->is_this() && mode == CONST && op == Token::INIT);
    // Perform an initialization check for 'this'. 'this' variable is the
    // only variable able to trigger bind operations outside the TDZ
    // via 'super' calls.
    BuildThrowIfNotHole(variable->name());
  }
}

void BytecodeGenerator::VisitVariableAssignment(Variable* variable,
                                                Token::Value op,
                                                FeedbackVectorSlot slot) {
  VariableMode mode = variable->mode();
  RegisterAllocationScope assignment_register_scope(this);
  BytecodeLabel end_label;
  bool hole_check_required =
      (mode == CONST_LEGACY) || (mode == LET && op != Token::INIT) ||
      (mode == CONST && op != Token::INIT) ||
      (mode == CONST && op == Token::INIT && variable->is_this());
  switch (variable->location()) {
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      Register destination;
      if (VariableLocation::PARAMETER == variable->location()) {
        destination = Register(builder()->Parameter(variable->index() + 1));
      } else {
        destination = Register(variable->index());
      }

      if (hole_check_required) {
        // Load destination to check for hole.
        Register value_temp = register_allocator()->NewRegister();
        builder()
            ->StoreAccumulatorInRegister(value_temp)
            .LoadAccumulatorWithRegister(destination);

        if (mode == CONST_LEGACY && op == Token::INIT) {
          // Perform an intialization check for legacy constants.
          builder()
              ->JumpIfNotHole(&end_label)
              .MoveRegister(value_temp, destination)
              .Bind(&end_label)
              .LoadAccumulatorWithRegister(value_temp);
          // Break here because the value should not be stored unconditionally.
          break;
        } else if (mode == CONST_LEGACY && op != Token::INIT) {
          DCHECK(!is_strict(language_mode()));
          // Ensure accumulator is in the correct state.
          builder()->LoadAccumulatorWithRegister(value_temp);
          // Break here, non-initializing assignments to legacy constants are
          // ignored.
          break;
        } else {
          BuildHoleCheckForVariableAssignment(variable, op);
          builder()->LoadAccumulatorWithRegister(value_temp);
        }
      }

      builder()->StoreAccumulatorInRegister(destination);
      break;
    }
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED: {
      builder()->StoreGlobal(variable->name(), feedback_index(slot),
                             language_mode());
      break;
    }
    case VariableLocation::CONTEXT: {
      int depth = execution_context()->ContextChainDepth(variable->scope());
      ContextScope* context = execution_context()->Previous(depth);
      Register context_reg;

      if (context) {
        context_reg = context->reg();
      } else {
        Register value_temp = register_allocator()->NewRegister();
        context_reg = register_allocator()->NewRegister();
        // Walk the context chain to find the context at the given depth.
        // TODO(rmcilroy): Perform this work in a bytecode handler once we have
        // a generic mechanism for performing jumps in interpreter.cc.
        // TODO(mythria): Also update bytecode graph builder with correct depth
        // when this changes.
        builder()
            ->StoreAccumulatorInRegister(value_temp)
            .LoadAccumulatorWithRegister(execution_context()->reg())
            .StoreAccumulatorInRegister(context_reg);
        for (int i = 0; i < depth; ++i) {
          builder()
              ->LoadContextSlot(context_reg, Context::PREVIOUS_INDEX)
              .StoreAccumulatorInRegister(context_reg);
        }
        builder()->LoadAccumulatorWithRegister(value_temp);
      }

      if (hole_check_required) {
        // Load destination to check for hole.
        Register value_temp = register_allocator()->NewRegister();
        builder()
            ->StoreAccumulatorInRegister(value_temp)
            .LoadContextSlot(context_reg, variable->index());

        if (mode == CONST_LEGACY && op == Token::INIT) {
          // Perform an intialization check for legacy constants.
          builder()
              ->JumpIfNotHole(&end_label)
              .LoadAccumulatorWithRegister(value_temp)
              .StoreContextSlot(context_reg, variable->index())
              .Bind(&end_label);
          builder()->LoadAccumulatorWithRegister(value_temp);
          // Break here because the value should not be stored unconditionally.
          // The above code performs the store conditionally.
          break;
        } else if (mode == CONST_LEGACY && op != Token::INIT) {
          DCHECK(!is_strict(language_mode()));
          // Ensure accumulator is in the correct state.
          builder()->LoadAccumulatorWithRegister(value_temp);
          // Break here, non-initializing assignments to legacy constants are
          // ignored.
          break;
        } else {
          BuildHoleCheckForVariableAssignment(variable, op);
          builder()->LoadAccumulatorWithRegister(value_temp);
        }
      }

      builder()->StoreContextSlot(context_reg, variable->index());
      break;
    }
    case VariableLocation::LOOKUP: {
      if (mode == CONST_LEGACY && op == Token::INIT) {
        register_allocator()->PrepareForConsecutiveAllocations(3);
        Register value = register_allocator()->NextConsecutiveRegister();
        Register context = register_allocator()->NextConsecutiveRegister();
        Register name = register_allocator()->NextConsecutiveRegister();

        // InitializeLegacyConstLookupSlot runtime call returns the 'value'
        // passed to it. So, accumulator will have its original contents when
        // runtime call returns.
        builder()
            ->StoreAccumulatorInRegister(value)
            .MoveRegister(execution_context()->reg(), context)
            .LoadLiteral(variable->name())
            .StoreAccumulatorInRegister(name)
            .CallRuntime(Runtime::kInitializeLegacyConstLookupSlot, value, 3);
      } else if (mode == CONST_LEGACY && op != Token::INIT) {
        // Non-intializing assignments to legacy constants are ignored.
        DCHECK(!is_strict(language_mode()));
      } else {
        builder()->StoreLookupSlot(variable->name(), language_mode());
      }
      break;
    }
  }
}


void BytecodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpressionOrThis());
  Register object, key, home_object, value;
  Handle<String> name;

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do to evaluate variable assignment LHS.
      break;
    case NAMED_PROPERTY: {
      object = VisitForRegisterValue(property->obj());
      name = property->key()->AsLiteral()->AsPropertyName();
      break;
    }
    case KEYED_PROPERTY: {
      object = VisitForRegisterValue(property->obj());
      if (expr->is_compound()) {
        // Use VisitForAccumulator and store to register so that the key is
        // still in the accumulator for loading the old value below.
        key = register_allocator()->NewRegister();
        VisitForAccumulatorValue(property->key());
        builder()->StoreAccumulatorInRegister(key);
      } else {
        key = VisitForRegisterValue(property->key());
      }
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      register_allocator()->PrepareForConsecutiveAllocations(4);
      object = register_allocator()->NextConsecutiveRegister();
      home_object = register_allocator()->NextConsecutiveRegister();
      key = register_allocator()->NextConsecutiveRegister();
      value = register_allocator()->NextConsecutiveRegister();
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), object);
      VisitForRegisterValue(super_property->home_object(), home_object);
      builder()
          ->LoadLiteral(property->key()->AsLiteral()->AsPropertyName())
          .StoreAccumulatorInRegister(key);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      register_allocator()->PrepareForConsecutiveAllocations(4);
      object = register_allocator()->NextConsecutiveRegister();
      home_object = register_allocator()->NextConsecutiveRegister();
      key = register_allocator()->NextConsecutiveRegister();
      value = register_allocator()->NextConsecutiveRegister();
      builder()->StoreAccumulatorInRegister(value);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), object);
      VisitForRegisterValue(super_property->home_object(), home_object);
      VisitForRegisterValue(property->key(), key);
      break;
    }
  }

  // Evaluate the value and potentially handle compound assignments by loading
  // the left-hand side value and performing a binary operation.
  if (expr->is_compound()) {
    Register old_value;
    switch (assign_type) {
      case VARIABLE: {
        VariableProxy* proxy = expr->target()->AsVariableProxy();
        old_value = VisitVariableLoadForRegisterValue(
            proxy->var(), proxy->VariableFeedbackSlot());
        break;
      }
      case NAMED_PROPERTY: {
        FeedbackVectorSlot slot = property->PropertyFeedbackSlot();
        old_value = register_allocator()->NewRegister();
        builder()
            ->LoadNamedProperty(object, name, feedback_index(slot))
            .StoreAccumulatorInRegister(old_value);
        break;
      }
      case KEYED_PROPERTY: {
        // Key is already in accumulator at this point due to evaluating the
        // LHS above.
        FeedbackVectorSlot slot = property->PropertyFeedbackSlot();
        old_value = register_allocator()->NewRegister();
        builder()
            ->LoadKeyedProperty(object, feedback_index(slot))
            .StoreAccumulatorInRegister(old_value);
        break;
      }
      case NAMED_SUPER_PROPERTY: {
        old_value = register_allocator()->NewRegister();
        BuildNamedSuperPropertyLoad(object, home_object, key);
        builder()->StoreAccumulatorInRegister(old_value);
        break;
      }
      case KEYED_SUPER_PROPERTY: {
        old_value = register_allocator()->NewRegister();
        BuildKeyedSuperPropertyLoad(object, home_object, key);
        builder()->StoreAccumulatorInRegister(old_value);
        break;
      }
    }
    VisitForAccumulatorValue(expr->value());
    builder()->BinaryOperation(expr->binary_op(), old_value);
  } else {
    VisitForAccumulatorValue(expr->value());
  }

  // Store the value.
  FeedbackVectorSlot slot = expr->AssignmentSlot();
  switch (assign_type) {
    case VARIABLE: {
      // TODO(oth): The VisitVariableAssignment() call is hard to reason about.
      // Is the value in the accumulator safe? Yes, but scary.
      Variable* variable = expr->target()->AsVariableProxy()->var();
      VisitVariableAssignment(variable, expr->op(), slot);
      break;
    }
    case NAMED_PROPERTY:
      builder()->StoreNamedProperty(object, name, feedback_index(slot),
                                    language_mode());
      break;
    case KEYED_PROPERTY:
      builder()->StoreKeyedProperty(object, key, feedback_index(slot),
                                    language_mode());
      break;
    case NAMED_SUPER_PROPERTY: {
      builder()->StoreAccumulatorInRegister(value);
      BuildNamedSuperPropertyStore(object, home_object, key, value);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      builder()->StoreAccumulatorInRegister(value);
      BuildKeyedSuperPropertyStore(object, home_object, key, value);
      break;
    }
  }
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitYield(Yield* expr) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitThrow(Throw* expr) {
  VisitForAccumulatorValue(expr->exception());
  builder()->Throw();
  // Throw statments are modeled as expression instead of statments. These are
  // converted from assignment statements in Rewriter::ReWrite pass. An
  // assignment statement expects a value in the accumulator. This is a hack to
  // avoid DCHECK fails assert accumulator has been set.
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitPropertyLoad(Register obj, Property* expr) {
  LhsKind property_kind = Property::GetAssignType(expr);
  FeedbackVectorSlot slot = expr->PropertyFeedbackSlot();
  switch (property_kind) {
    case VARIABLE:
      UNREACHABLE();
    case NAMED_PROPERTY: {
      builder()->LoadNamedProperty(obj,
                                   expr->key()->AsLiteral()->AsPropertyName(),
                                   feedback_index(slot));
      break;
    }
    case KEYED_PROPERTY: {
      VisitForAccumulatorValue(expr->key());
      builder()->LoadKeyedProperty(obj, feedback_index(slot));
      break;
    }
    case NAMED_SUPER_PROPERTY:
      VisitNamedSuperPropertyLoad(expr, Register::invalid_value());
      break;
    case KEYED_SUPER_PROPERTY:
      VisitKeyedSuperPropertyLoad(expr, Register::invalid_value());
      break;
  }
  execution_result()->SetResultInAccumulator();
}

void BytecodeGenerator::VisitPropertyLoadForAccumulator(Register obj,
                                                        Property* expr) {
  AccumulatorResultScope result_scope(this);
  VisitPropertyLoad(obj, expr);
}

void BytecodeGenerator::VisitNamedSuperPropertyLoad(Property* property,
                                                    Register opt_receiver_out) {
  RegisterAllocationScope register_scope(this);
  register_allocator()->PrepareForConsecutiveAllocations(3);

  Register receiver, home_object, name;
  receiver = register_allocator()->NextConsecutiveRegister();
  home_object = register_allocator()->NextConsecutiveRegister();
  name = register_allocator()->NextConsecutiveRegister();
  SuperPropertyReference* super_property =
      property->obj()->AsSuperPropertyReference();
  VisitForRegisterValue(super_property->this_var(), receiver);
  VisitForRegisterValue(super_property->home_object(), home_object);
  builder()
      ->LoadLiteral(property->key()->AsLiteral()->AsPropertyName())
      .StoreAccumulatorInRegister(name);
  BuildNamedSuperPropertyLoad(receiver, home_object, name);

  if (opt_receiver_out.is_valid()) {
    builder()->MoveRegister(receiver, opt_receiver_out);
  }
}

void BytecodeGenerator::VisitKeyedSuperPropertyLoad(Property* property,
                                                    Register opt_receiver_out) {
  RegisterAllocationScope register_scope(this);
  register_allocator()->PrepareForConsecutiveAllocations(3);

  Register receiver, home_object, key;
  receiver = register_allocator()->NextConsecutiveRegister();
  home_object = register_allocator()->NextConsecutiveRegister();
  key = register_allocator()->NextConsecutiveRegister();
  SuperPropertyReference* super_property =
      property->obj()->AsSuperPropertyReference();
  VisitForRegisterValue(super_property->this_var(), receiver);
  VisitForRegisterValue(super_property->home_object(), home_object);
  VisitForRegisterValue(property->key(), key);
  BuildKeyedSuperPropertyLoad(receiver, home_object, key);

  if (opt_receiver_out.is_valid()) {
    builder()->MoveRegister(receiver, opt_receiver_out);
  }
}

void BytecodeGenerator::VisitProperty(Property* expr) {
  LhsKind property_kind = Property::GetAssignType(expr);
  if (property_kind != NAMED_SUPER_PROPERTY &&
      property_kind != KEYED_SUPER_PROPERTY) {
    Register obj = VisitForRegisterValue(expr->obj());
    VisitPropertyLoad(obj, expr);
  } else {
    VisitPropertyLoad(Register::invalid_value(), expr);
  }
}

Register BytecodeGenerator::VisitArguments(ZoneList<Expression*>* args) {
  if (args->length() == 0) {
    return Register();
  }

  // Visit arguments and place in a contiguous block of temporary
  // registers.  Return the first temporary register corresponding to
  // the first argument.
  //
  // NB the caller may have already called
  // PrepareForConsecutiveAllocations() with args->length() + N. The
  // second call here will be a no-op provided there have been N or
  // less calls to NextConsecutiveRegister(). Otherwise, the arguments
  // here will be consecutive, but they will not be consecutive with
  // earlier consecutive allocations made by the caller.
  register_allocator()->PrepareForConsecutiveAllocations(args->length());

  // Visit for first argument that goes into returned register
  Register first_arg = register_allocator()->NextConsecutiveRegister();
  VisitForAccumulatorValue(args->at(0));
  builder()->StoreAccumulatorInRegister(first_arg);

  // Visit remaining arguments
  for (int i = 1; i < static_cast<int>(args->length()); i++) {
    Register ith_arg = register_allocator()->NextConsecutiveRegister();
    VisitForAccumulatorValue(args->at(i));
    builder()->StoreAccumulatorInRegister(ith_arg);
    DCHECK(ith_arg.index() - i == first_arg.index());
  }
  return first_arg;
}

void BytecodeGenerator::VisitCall(Call* expr) {
  Expression* callee_expr = expr->expression();
  Call::CallType call_type = expr->GetCallType(isolate());

  if (call_type == Call::SUPER_CALL) {
    return VisitCallSuper(expr);
  }

  // Prepare the callee and the receiver to the function call. This depends on
  // the semantics of the underlying call type.

  // The receiver and arguments need to be allocated consecutively for
  // Call(). We allocate the callee and receiver consecutively for calls to
  // %LoadLookupSlotForCall. Future optimizations could avoid this there are
  // no arguments or the receiver and arguments are already consecutive.
  ZoneList<Expression*>* args = expr->arguments();
  register_allocator()->PrepareForConsecutiveAllocations(args->length() + 2);
  Register callee = register_allocator()->NextConsecutiveRegister();
  Register receiver = register_allocator()->NextConsecutiveRegister();

  switch (call_type) {
    case Call::NAMED_PROPERTY_CALL:
    case Call::KEYED_PROPERTY_CALL: {
      Property* property = callee_expr->AsProperty();
      VisitForAccumulatorValue(property->obj());
      builder()->StoreAccumulatorInRegister(receiver);
      VisitPropertyLoadForAccumulator(receiver, property);
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::GLOBAL_CALL: {
      // Receiver is undefined for global calls.
      builder()->LoadUndefined().StoreAccumulatorInRegister(receiver);
      // Load callee as a global variable.
      VariableProxy* proxy = callee_expr->AsVariableProxy();
      VisitVariableLoadForAccumulatorValue(proxy->var(),
                                           proxy->VariableFeedbackSlot());
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::LOOKUP_SLOT_CALL:
    case Call::POSSIBLY_EVAL_CALL: {
      if (callee_expr->AsVariableProxy()->var()->IsLookupSlot()) {
        RegisterAllocationScope inner_register_scope(this);
        Register name = register_allocator()->NewRegister();

        // Call %LoadLookupSlotForCall to get the callee and receiver.
        DCHECK(Register::AreContiguous(callee, receiver));
        Variable* variable = callee_expr->AsVariableProxy()->var();
        builder()
            ->LoadLiteral(variable->name())
            .StoreAccumulatorInRegister(name)
            .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, name, 1,
                                callee);
        break;
      }
      // Fall through.
      DCHECK_EQ(call_type, Call::POSSIBLY_EVAL_CALL);
    }
    case Call::OTHER_CALL: {
      builder()->LoadUndefined().StoreAccumulatorInRegister(receiver);
      VisitForAccumulatorValue(callee_expr);
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::NAMED_SUPER_PROPERTY_CALL: {
      Property* property = callee_expr->AsProperty();
      VisitNamedSuperPropertyLoad(property, receiver);
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::KEYED_SUPER_PROPERTY_CALL: {
      Property* property = callee_expr->AsProperty();
      VisitKeyedSuperPropertyLoad(property, receiver);
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::SUPER_CALL:
      UNREACHABLE();
      break;
  }

  // Evaluate all arguments to the function call and store in sequential
  // registers.
  Register arg = VisitArguments(args);
  CHECK(args->length() == 0 || arg.index() == receiver.index() + 1);

  // Resolve callee for a potential direct eval call. This block will mutate the
  // callee value.
  if (call_type == Call::POSSIBLY_EVAL_CALL && args->length() > 0) {
    RegisterAllocationScope inner_register_scope(this);
    register_allocator()->PrepareForConsecutiveAllocations(5);
    Register callee_for_eval = register_allocator()->NextConsecutiveRegister();
    Register source = register_allocator()->NextConsecutiveRegister();
    Register function = register_allocator()->NextConsecutiveRegister();
    Register language = register_allocator()->NextConsecutiveRegister();
    Register position = register_allocator()->NextConsecutiveRegister();

    // Set up arguments for ResolvePossiblyDirectEval by copying callee, source
    // strings and function closure, and loading language and
    // position.
    builder()
        ->MoveRegister(callee, callee_for_eval)
        .MoveRegister(arg, source)
        .MoveRegister(Register::function_closure(), function)
        .LoadLiteral(Smi::FromInt(language_mode()))
        .StoreAccumulatorInRegister(language)
        .LoadLiteral(
            Smi::FromInt(execution_context()->scope()->start_position()))
        .StoreAccumulatorInRegister(position);

    // Call ResolvePossiblyDirectEval and modify the callee.
    builder()
        ->CallRuntime(Runtime::kResolvePossiblyDirectEval, callee_for_eval, 5)
        .StoreAccumulatorInRegister(callee);
  }

  builder()->SetExpressionPosition(expr);
  builder()->Call(callee, receiver, 1 + args->length(),
                  feedback_index(expr->CallFeedbackICSlot()),
                  expr->tail_call_mode());
  execution_result()->SetResultInAccumulator();
}

void BytecodeGenerator::VisitCallSuper(Call* expr) {
  RegisterAllocationScope register_scope(this);
  SuperCallReference* super = expr->expression()->AsSuperCallReference();

  // Prepare the constructor to the super call.
  Register this_function = register_allocator()->NewRegister();
  VisitForAccumulatorValue(super->this_function_var());
  builder()
      ->StoreAccumulatorInRegister(this_function)
      .CallRuntime(Runtime::kInlineGetSuperConstructor, this_function, 1);

  Register constructor = this_function;  // Re-use dead this_function register.
  builder()->StoreAccumulatorInRegister(constructor);

  ZoneList<Expression*>* args = expr->arguments();
  Register first_arg = VisitArguments(args);

  // The new target is loaded into the accumulator from the
  // {new.target} variable.
  VisitForAccumulatorValue(super->new_target_var());

  // Call construct.
  builder()->SetExpressionPosition(expr);
  builder()->New(constructor, first_arg, args->length());
  execution_result()->SetResultInAccumulator();
}

void BytecodeGenerator::VisitCallNew(CallNew* expr) {
  Register constructor = register_allocator()->NewRegister();
  VisitForAccumulatorValue(expr->expression());
  builder()->StoreAccumulatorInRegister(constructor);

  ZoneList<Expression*>* args = expr->arguments();
  Register first_arg = VisitArguments(args);

  builder()->SetExpressionPosition(expr);
  // The accumulator holds new target which is the same as the
  // constructor for CallNew.
  builder()
      ->LoadAccumulatorWithRegister(constructor)
      .New(constructor, first_arg, args->length());
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  if (expr->is_jsruntime()) {
    // Allocate a register for the receiver and load it with undefined.
    register_allocator()->PrepareForConsecutiveAllocations(1 + args->length());
    Register receiver = register_allocator()->NextConsecutiveRegister();
    builder()->LoadUndefined().StoreAccumulatorInRegister(receiver);
    Register first_arg = VisitArguments(args);
    CHECK(args->length() == 0 || first_arg.index() == receiver.index() + 1);
    builder()->CallJSRuntime(expr->context_index(), receiver,
                             1 + args->length());
  } else {
    // Evaluate all arguments to the runtime call.
    Register first_arg = VisitArguments(args);
    Runtime::FunctionId function_id = expr->function()->function_id;
    builder()->CallRuntime(function_id, first_arg, args->length());
  }
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitVoid(UnaryOperation* expr) {
  VisitForEffect(expr->expression());
  builder()->LoadUndefined();
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitTypeOf(UnaryOperation* expr) {
  if (expr->expression()->IsVariableProxy()) {
    // Typeof does not throw a reference error on global variables, hence we
    // perform a non-contextual load in case the operand is a variable proxy.
    VariableProxy* proxy = expr->expression()->AsVariableProxy();
    VisitVariableLoadForAccumulatorValue(
        proxy->var(), proxy->VariableFeedbackSlot(), INSIDE_TYPEOF);
  } else {
    VisitForAccumulatorValue(expr->expression());
  }
  builder()->TypeOf();
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitNot(UnaryOperation* expr) {
  VisitForAccumulatorValue(expr->expression());
  builder()->LogicalNot();
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::Value::NOT:
      VisitNot(expr);
      break;
    case Token::Value::TYPEOF:
      VisitTypeOf(expr);
      break;
    case Token::Value::VOID:
      VisitVoid(expr);
      break;
    case Token::Value::DELETE:
      VisitDelete(expr);
      break;
    case Token::Value::BIT_NOT:
    case Token::Value::ADD:
    case Token::Value::SUB:
      // These operators are converted to an equivalent binary operators in
      // the parser. These operators are not expected to be visited here.
      UNREACHABLE();
    default:
      UNREACHABLE();
  }
}


void BytecodeGenerator::VisitDelete(UnaryOperation* expr) {
  if (expr->expression()->IsProperty()) {
    // Delete of an object property is allowed both in sloppy
    // and strict modes.
    Property* property = expr->expression()->AsProperty();
    Register object = VisitForRegisterValue(property->obj());
    VisitForAccumulatorValue(property->key());
    builder()->Delete(object, language_mode());
  } else if (expr->expression()->IsVariableProxy()) {
    // Delete of an unqualified identifier is allowed in sloppy mode but is
    // not allowed in strict mode. Deleting 'this' is allowed in both modes.
    VariableProxy* proxy = expr->expression()->AsVariableProxy();
    Variable* variable = proxy->var();
    DCHECK(is_sloppy(language_mode()) || variable->HasThisName(isolate()));
    switch (variable->location()) {
      case VariableLocation::GLOBAL:
      case VariableLocation::UNALLOCATED: {
        // Global var, let, const or variables not explicitly declared.
        Register native_context = register_allocator()->NewRegister();
        Register global_object = register_allocator()->NewRegister();
        builder()
            ->LoadContextSlot(execution_context()->reg(),
                              Context::NATIVE_CONTEXT_INDEX)
            .StoreAccumulatorInRegister(native_context)
            .LoadContextSlot(native_context, Context::EXTENSION_INDEX)
            .StoreAccumulatorInRegister(global_object)
            .LoadLiteral(variable->name())
            .Delete(global_object, language_mode());
        break;
      }
      case VariableLocation::PARAMETER:
      case VariableLocation::LOCAL:
      case VariableLocation::CONTEXT: {
        // Deleting local var/let/const, context variables, and arguments
        // does not have any effect.
        if (variable->HasThisName(isolate())) {
          builder()->LoadTrue();
        } else {
          builder()->LoadFalse();
        }
        break;
      }
      case VariableLocation::LOOKUP: {
        Register name_reg = register_allocator()->NewRegister();
        builder()
            ->LoadLiteral(variable->name())
            .StoreAccumulatorInRegister(name_reg)
            .CallRuntime(Runtime::kDeleteLookupSlot, name_reg, 1);
        break;
      }
      default:
        UNREACHABLE();
    }
  } else {
    // Delete of an unresolvable reference returns true.
    VisitForEffect(expr->expression());
    builder()->LoadTrue();
  }
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpressionOrThis());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->expression()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  // TODO(rmcilroy): Set is_postfix to false if visiting for effect.
  bool is_postfix = expr->is_postfix();

  // Evaluate LHS expression and get old value.
  Register object, home_object, key, old_value, value;
  Handle<String> name;
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      VisitVariableLoadForAccumulatorValue(proxy->var(),
                                           proxy->VariableFeedbackSlot());
      break;
    }
    case NAMED_PROPERTY: {
      FeedbackVectorSlot slot = property->PropertyFeedbackSlot();
      object = VisitForRegisterValue(property->obj());
      name = property->key()->AsLiteral()->AsPropertyName();
      builder()->LoadNamedProperty(object, name, feedback_index(slot));
      break;
    }
    case KEYED_PROPERTY: {
      FeedbackVectorSlot slot = property->PropertyFeedbackSlot();
      object = VisitForRegisterValue(property->obj());
      // Use visit for accumulator here since we need the key in the accumulator
      // for the LoadKeyedProperty.
      key = register_allocator()->NewRegister();
      VisitForAccumulatorValue(property->key());
      builder()->StoreAccumulatorInRegister(key).LoadKeyedProperty(
          object, feedback_index(slot));
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      register_allocator()->PrepareForConsecutiveAllocations(4);
      object = register_allocator()->NextConsecutiveRegister();
      home_object = register_allocator()->NextConsecutiveRegister();
      key = register_allocator()->NextConsecutiveRegister();
      value = register_allocator()->NextConsecutiveRegister();
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), object);
      VisitForRegisterValue(super_property->home_object(), home_object);
      builder()
          ->LoadLiteral(property->key()->AsLiteral()->AsPropertyName())
          .StoreAccumulatorInRegister(key);
      BuildNamedSuperPropertyLoad(object, home_object, key);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      register_allocator()->PrepareForConsecutiveAllocations(4);
      object = register_allocator()->NextConsecutiveRegister();
      home_object = register_allocator()->NextConsecutiveRegister();
      key = register_allocator()->NextConsecutiveRegister();
      value = register_allocator()->NextConsecutiveRegister();
      builder()->StoreAccumulatorInRegister(value);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), object);
      VisitForRegisterValue(super_property->home_object(), home_object);
      VisitForRegisterValue(property->key(), key);
      BuildKeyedSuperPropertyLoad(object, home_object, key);
      break;
    }
  }

  // Convert old value into a number.
  if (!is_strong(language_mode())) {
    builder()->CastAccumulatorToNumber();
  }

  // Save result for postfix expressions.
  if (is_postfix) {
    old_value = register_allocator()->outer()->NewRegister();
    builder()->StoreAccumulatorInRegister(old_value);
  }

  // Perform +1/-1 operation.
  builder()->CountOperation(expr->binary_op());

  // Store the value.
  FeedbackVectorSlot feedback_slot = expr->CountSlot();
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->expression()->AsVariableProxy()->var();
      VisitVariableAssignment(variable, expr->op(), feedback_slot);
      break;
    }
    case NAMED_PROPERTY: {
      builder()->StoreNamedProperty(object, name, feedback_index(feedback_slot),
                                    language_mode());
      break;
    }
    case KEYED_PROPERTY: {
      builder()->StoreKeyedProperty(object, key, feedback_index(feedback_slot),
                                    language_mode());
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      builder()->StoreAccumulatorInRegister(value);
      BuildNamedSuperPropertyStore(object, home_object, key, value);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      builder()->StoreAccumulatorInRegister(value);
      BuildKeyedSuperPropertyStore(object, home_object, key, value);
      break;
    }
  }

  // Restore old value for postfix expressions.
  if (is_postfix) {
    execution_result()->SetResultInRegister(old_value);
  } else {
    execution_result()->SetResultInAccumulator();
  }
}


void BytecodeGenerator::VisitBinaryOperation(BinaryOperation* binop) {
  switch (binop->op()) {
    case Token::COMMA:
      VisitCommaExpression(binop);
      break;
    case Token::OR:
      VisitLogicalOrExpression(binop);
      break;
    case Token::AND:
      VisitLogicalAndExpression(binop);
      break;
    default:
      VisitArithmeticExpression(binop);
      break;
  }
}


void BytecodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Register lhs = VisitForRegisterValue(expr->left());
  VisitForAccumulatorValue(expr->right());
  builder()->CompareOperation(expr->op(), lhs);
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitArithmeticExpression(BinaryOperation* expr) {
  Register lhs = VisitForRegisterValue(expr->left());
  VisitForAccumulatorValue(expr->right());
  builder()->BinaryOperation(expr->op(), lhs);
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitSpread(Spread* expr) { UNREACHABLE(); }


void BytecodeGenerator::VisitEmptyParentheses(EmptyParentheses* expr) {
  UNREACHABLE();
}


void BytecodeGenerator::VisitThisFunction(ThisFunction* expr) {
  execution_result()->SetResultInRegister(Register::function_closure());
}


void BytecodeGenerator::VisitSuperCallReference(SuperCallReference* expr) {
  // Handled by VisitCall().
  UNREACHABLE();
}


void BytecodeGenerator::VisitSuperPropertyReference(
    SuperPropertyReference* expr) {
  builder()->CallRuntime(Runtime::kThrowUnsupportedSuperError, Register(0), 0);
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitCommaExpression(BinaryOperation* binop) {
  VisitForEffect(binop->left());
  Visit(binop->right());
}


void BytecodeGenerator::VisitLogicalOrExpression(BinaryOperation* binop) {
  Expression* left = binop->left();
  Expression* right = binop->right();

  // Short-circuit evaluation- If it is known that left is always true,
  // no need to visit right
  if (left->ToBooleanIsTrue()) {
    VisitForAccumulatorValue(left);
  } else {
    BytecodeLabel end_label;
    VisitForAccumulatorValue(left);
    builder()->JumpIfTrue(&end_label);
    VisitForAccumulatorValue(right);
    builder()->Bind(&end_label);
  }
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitLogicalAndExpression(BinaryOperation* binop) {
  Expression* left = binop->left();
  Expression* right = binop->right();

  // Short-circuit evaluation- If it is known that left is always false,
  // no need to visit right
  if (left->ToBooleanIsFalse()) {
    VisitForAccumulatorValue(left);
  } else {
    BytecodeLabel end_label;
    VisitForAccumulatorValue(left);
    builder()->JumpIfFalse(&end_label);
    VisitForAccumulatorValue(right);
    builder()->Bind(&end_label);
  }
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitRewritableExpression(RewritableExpression* expr) {
  Visit(expr->expression());
}


void BytecodeGenerator::VisitNewLocalFunctionContext() {
  AccumulatorResultScope accumulator_execution_result(this);
  Scope* scope = this->scope();

  // Allocate a new local context.
  if (scope->is_script_scope()) {
    RegisterAllocationScope register_scope(this);
    Register closure = register_allocator()->NewRegister();
    Register scope_info = register_allocator()->NewRegister();
    DCHECK(Register::AreContiguous(closure, scope_info));
    builder()
        ->LoadAccumulatorWithRegister(Register::function_closure())
        .StoreAccumulatorInRegister(closure)
        .LoadLiteral(scope->GetScopeInfo(isolate()))
        .StoreAccumulatorInRegister(scope_info)
        .CallRuntime(Runtime::kNewScriptContext, closure, 2);
  } else {
    builder()->CallRuntime(Runtime::kNewFunctionContext,
                           Register::function_closure(), 1);
  }
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitBuildLocalActivationContext() {
  Scope* scope = this->scope();

  if (scope->has_this_declaration() && scope->receiver()->IsContextSlot()) {
    Variable* variable = scope->receiver();
    Register receiver(builder()->Parameter(0));
    // Context variable (at bottom of the context chain).
    DCHECK_EQ(0, scope->ContextChainLength(variable->scope()));
    builder()->LoadAccumulatorWithRegister(receiver).StoreContextSlot(
        execution_context()->reg(), variable->index());
  }

  // Copy parameters into context if necessary.
  int num_parameters = scope->num_parameters();
  for (int i = 0; i < num_parameters; i++) {
    Variable* variable = scope->parameter(i);
    if (!variable->IsContextSlot()) continue;

    // The parameter indices are shifted by 1 (receiver is variable
    // index -1 but is parameter index 0 in BytecodeArrayBuilder).
    Register parameter(builder()->Parameter(i + 1));
    // Context variable (at bottom of the context chain).
    DCHECK_EQ(0, scope->ContextChainLength(variable->scope()));
    builder()->LoadAccumulatorWithRegister(parameter)
        .StoreContextSlot(execution_context()->reg(), variable->index());
  }
}


void BytecodeGenerator::VisitNewLocalBlockContext(Scope* scope) {
  AccumulatorResultScope accumulator_execution_result(this);
  DCHECK(scope->is_block_scope());

  // Allocate a new local block context.
  register_allocator()->PrepareForConsecutiveAllocations(2);
  Register scope_info = register_allocator()->NextConsecutiveRegister();
  Register closure = register_allocator()->NextConsecutiveRegister();

  builder()
      ->LoadLiteral(scope->GetScopeInfo(isolate()))
      .StoreAccumulatorInRegister(scope_info);
  VisitFunctionClosureForContext();
  builder()
      ->StoreAccumulatorInRegister(closure)
      .CallRuntime(Runtime::kPushBlockContext, scope_info, 2);
  execution_result()->SetResultInAccumulator();
}

void BytecodeGenerator::VisitNewLocalWithContext() {
  AccumulatorResultScope accumulator_execution_result(this);

  register_allocator()->PrepareForConsecutiveAllocations(2);
  Register extension_object = register_allocator()->NextConsecutiveRegister();
  Register closure = register_allocator()->NextConsecutiveRegister();

  builder()->StoreAccumulatorInRegister(extension_object);
  VisitFunctionClosureForContext();
  builder()->StoreAccumulatorInRegister(closure).CallRuntime(
      Runtime::kPushWithContext, extension_object, 2);
  execution_result()->SetResultInAccumulator();
}

void BytecodeGenerator::VisitNewLocalCatchContext(Variable* variable) {
  AccumulatorResultScope accumulator_execution_result(this);
  DCHECK(variable->IsContextSlot());

  // Allocate a new local block context.
  register_allocator()->PrepareForConsecutiveAllocations(3);
  Register name = register_allocator()->NextConsecutiveRegister();
  Register exception = register_allocator()->NextConsecutiveRegister();
  Register closure = register_allocator()->NextConsecutiveRegister();

  builder()
      ->StoreAccumulatorInRegister(exception)
      .LoadLiteral(variable->name())
      .StoreAccumulatorInRegister(name);
  VisitFunctionClosureForContext();
  builder()->StoreAccumulatorInRegister(closure).CallRuntime(
      Runtime::kPushCatchContext, name, 3);
  execution_result()->SetResultInAccumulator();
}


void BytecodeGenerator::VisitObjectLiteralAccessor(
    Register home_object, ObjectLiteralProperty* property, Register value_out) {
  // TODO(rmcilroy): Replace value_out with VisitForRegister();
  if (property == nullptr) {
    builder()->LoadNull().StoreAccumulatorInRegister(value_out);
  } else {
    VisitForAccumulatorValue(property->value());
    builder()->StoreAccumulatorInRegister(value_out);
    VisitSetHomeObject(value_out, home_object, property);
  }
}

void BytecodeGenerator::VisitSetHomeObject(Register value, Register home_object,
                                           ObjectLiteralProperty* property,
                                           int slot_number) {
  Expression* expr = property->value();
  if (FunctionLiteral::NeedsHomeObject(expr)) {
    Handle<Name> name = isolate()->factory()->home_object_symbol();
    FeedbackVectorSlot slot = property->GetSlot(slot_number);
    builder()
        ->LoadAccumulatorWithRegister(home_object)
        .StoreNamedProperty(value, name, feedback_index(slot), language_mode());
  }
}


void BytecodeGenerator::VisitArgumentsObject(Variable* variable) {
  if (variable == nullptr) return;

  DCHECK(variable->IsContextSlot() || variable->IsStackAllocated());

  // Allocate and initialize a new arguments object and assign to the
  // {arguments} variable.
  CreateArgumentsType type =
      is_strict(language_mode()) || !info()->has_simple_parameters()
          ? CreateArgumentsType::kUnmappedArguments
          : CreateArgumentsType::kMappedArguments;
  builder()->CreateArguments(type);
  VisitVariableAssignment(variable, Token::ASSIGN,
                          FeedbackVectorSlot::Invalid());
}

void BytecodeGenerator::VisitRestArgumentsArray(Variable* rest) {
  if (rest == nullptr) return;

  // Allocate and initialize a new rest parameter and assign to the {rest}
  // variable.
  builder()->CreateArguments(CreateArgumentsType::kRestParameter);
  DCHECK(rest->IsContextSlot() || rest->IsStackAllocated());
  VisitVariableAssignment(rest, Token::ASSIGN, FeedbackVectorSlot::Invalid());
}

void BytecodeGenerator::VisitThisFunctionVariable(Variable* variable) {
  if (variable == nullptr) return;

  // Store the closure we were called with in the given variable.
  builder()->LoadAccumulatorWithRegister(Register::function_closure());
  VisitVariableAssignment(variable, Token::INIT, FeedbackVectorSlot::Invalid());
}


void BytecodeGenerator::VisitNewTargetVariable(Variable* variable) {
  if (variable == nullptr) return;

  // Store the new target we were called with in the given variable.
  builder()->LoadAccumulatorWithRegister(Register::new_target());
  VisitVariableAssignment(variable, Token::INIT, FeedbackVectorSlot::Invalid());
}


void BytecodeGenerator::VisitFunctionClosureForContext() {
  AccumulatorResultScope accumulator_execution_result(this);
  Scope* closure_scope = execution_context()->scope()->ClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function as
    // their closure, not the anonymous closure containing the global code.
    Register native_context = register_allocator()->NewRegister();
    builder()
        ->LoadContextSlot(execution_context()->reg(),
                          Context::NATIVE_CONTEXT_INDEX)
        .StoreAccumulatorInRegister(native_context)
        .LoadContextSlot(native_context, Context::CLOSURE_INDEX);
  } else if (closure_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code. Fetch it from the context.
    builder()->LoadContextSlot(execution_context()->reg(),
                               Context::CLOSURE_INDEX);
  } else {
    DCHECK(closure_scope->is_function_scope());
    builder()->LoadAccumulatorWithRegister(Register::function_closure());
  }
  execution_result()->SetResultInAccumulator();
}


// Visits the expression |expr| and places the result in the accumulator.
void BytecodeGenerator::VisitForAccumulatorValue(Expression* expr) {
  AccumulatorResultScope accumulator_scope(this);
  Visit(expr);
}

void BytecodeGenerator::VisitForAccumulatorValueOrTheHole(Expression* expr) {
  if (expr == nullptr) {
    builder()->LoadTheHole();
  } else {
    VisitForAccumulatorValue(expr);
  }
}

// Visits the expression |expr| and discards the result.
void BytecodeGenerator::VisitForEffect(Expression* expr) {
  EffectResultScope effect_scope(this);
  Visit(expr);
}


// Visits the expression |expr| and returns the register containing
// the expression result.
Register BytecodeGenerator::VisitForRegisterValue(Expression* expr) {
  RegisterResultScope register_scope(this);
  Visit(expr);
  return register_scope.ResultRegister();
}

// Visits the expression |expr| and stores the expression result in
// |destination|.
void BytecodeGenerator::VisitForRegisterValue(Expression* expr,
                                              Register destination) {
  AccumulatorResultScope register_scope(this);
  Visit(expr);
  builder()->StoreAccumulatorInRegister(destination);
}

void BytecodeGenerator::VisitInScope(Statement* stmt, Scope* scope) {
  ContextScope context_scope(this, scope);
  DCHECK(scope->declarations()->is_empty());
  Visit(stmt);
}


LanguageMode BytecodeGenerator::language_mode() const {
  return info()->language_mode();
}


int BytecodeGenerator::feedback_index(FeedbackVectorSlot slot) const {
  return info()->feedback_vector()->GetIndex(slot);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

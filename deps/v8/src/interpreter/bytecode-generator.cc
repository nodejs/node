// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-generator.h"

#include "src/api-inl.h"
#include "src/ast/ast-source-ranges.h"
#include "src/ast/scopes.h"
#include "src/builtins/builtins-constructor.h"
#include "src/code-stubs.h"
#include "src/compiler.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecode-jump-table.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "src/interpreter/control-flow-builders.h"
#include "src/objects-inl.h"
#include "src/objects/debug-objects.h"
#include "src/objects/literal-objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/token.h"
#include "src/unoptimized-compilation-info.h"

namespace v8 {
namespace internal {
namespace interpreter {

// Scoped class tracking context objects created by the visitor. Represents
// mutations of the context chain within the function body, allowing pushing and
// popping of the current {context_register} during visitation.
class BytecodeGenerator::ContextScope {
 public:
  ContextScope(BytecodeGenerator* generator, Scope* scope)
      : generator_(generator),
        scope_(scope),
        outer_(generator_->execution_context()),
        register_(Register::current_context()),
        depth_(0) {
    DCHECK(scope->NeedsContext() || outer_ == nullptr);
    if (outer_) {
      depth_ = outer_->depth_ + 1;

      // Push the outer context into a new context register.
      Register outer_context_reg =
          generator_->register_allocator()->NewRegister();
      outer_->set_register(outer_context_reg);
      generator_->builder()->PushContext(outer_context_reg);
    }
    generator_->set_execution_context(this);
  }

  ~ContextScope() {
    if (outer_) {
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

  Register reg() const { return register_; }

 private:
  const BytecodeArrayBuilder* builder() const { return generator_->builder(); }

  void set_register(Register reg) { register_ = reg; }

  BytecodeGenerator* generator_;
  Scope* scope_;
  ContextScope* outer_;
  Register register_;
  int depth_;
};

// Scoped class for tracking control statements entered by the
// visitor. The pattern derives AstGraphBuilder::ControlScope.
class BytecodeGenerator::ControlScope {
 public:
  explicit ControlScope(BytecodeGenerator* generator)
      : generator_(generator), outer_(generator->execution_control()),
        context_(generator->execution_context()) {
    generator_->set_execution_control(this);
  }
  virtual ~ControlScope() { generator_->set_execution_control(outer()); }

  void Break(Statement* stmt) {
    PerformCommand(CMD_BREAK, stmt, kNoSourcePosition);
  }
  void Continue(Statement* stmt) {
    PerformCommand(CMD_CONTINUE, stmt, kNoSourcePosition);
  }
  void ReturnAccumulator(int source_position = kNoSourcePosition) {
    PerformCommand(CMD_RETURN, nullptr, source_position);
  }
  void AsyncReturnAccumulator(int source_position = kNoSourcePosition) {
    PerformCommand(CMD_ASYNC_RETURN, nullptr, source_position);
  }

  class DeferredCommands;

 protected:
  enum Command {
    CMD_BREAK,
    CMD_CONTINUE,
    CMD_RETURN,
    CMD_ASYNC_RETURN,
    CMD_RETHROW
  };
  static constexpr bool CommandUsesAccumulator(Command command) {
    return command != CMD_BREAK && command != CMD_CONTINUE;
  }

  void PerformCommand(Command command, Statement* statement,
                      int source_position);
  virtual bool Execute(Command command, Statement* statement,
                       int source_position) = 0;

  // Helper to pop the context chain to a depth expected by this control scope.
  // Note that it is the responsibility of each individual {Execute} method to
  // trigger this when commands are handled and control-flow continues locally.
  void PopContextToExpectedDepth();

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
        result_register_(result_register),
        return_token_(-1),
        async_return_token_(-1),
        rethrow_token_(-1) {}

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
    int token = GetTokenForCommand(command, statement);

    DCHECK_LT(token, deferred_.size());
    DCHECK_EQ(deferred_[token].command, command);
    DCHECK_EQ(deferred_[token].statement, statement);
    DCHECK_EQ(deferred_[token].token, token);

    if (CommandUsesAccumulator(command)) {
      builder()->StoreAccumulatorInRegister(result_register_);
    }
    builder()->LoadLiteral(Smi::FromInt(token));
    builder()->StoreAccumulatorInRegister(token_register_);
    if (!CommandUsesAccumulator(command)) {
      // If we're not saving the accumulator in the result register, shove a
      // harmless value there instead so that it is still considered "killed" in
      // the liveness analysis. Normally we would LdaUndefined first, but the
      // Smi token value is just as good, and by reusing it we save a bytecode.
      builder()->StoreAccumulatorInRegister(result_register_);
    }
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
    // Since we're not saving the accumulator in the result register, shove a
    // harmless value there instead so that it is still considered "killed" in
    // the liveness analysis. Normally we would LdaUndefined first, but the Smi
    // token value is just as good, and by reusing it we save a bytecode.
    builder()->StoreAccumulatorInRegister(result_register_);
  }

  // Applies all recorded control-flow commands after the finally-block again.
  // This generates a dynamic dispatch on the token from the entry point.
  void ApplyDeferredCommands() {
    if (deferred_.size() == 0) return;

    BytecodeLabel fall_through;

    if (deferred_.size() == 1) {
      // For a single entry, just jump to the fallthrough if we don't match the
      // entry token.
      const Entry& entry = deferred_[0];

      builder()
          ->LoadLiteral(Smi::FromInt(entry.token))
          .CompareReference(token_register_)
          .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &fall_through);

      if (CommandUsesAccumulator(entry.command)) {
        builder()->LoadAccumulatorWithRegister(result_register_);
      }
      execution_control()->PerformCommand(entry.command, entry.statement,
                                          kNoSourcePosition);
    } else {
      // For multiple entries, build a jump table and switch on the token,
      // jumping to the fallthrough if none of them match.

      BytecodeJumpTable* jump_table =
          builder()->AllocateJumpTable(static_cast<int>(deferred_.size()), 0);
      builder()
          ->LoadAccumulatorWithRegister(token_register_)
          .SwitchOnSmiNoFeedback(jump_table)
          .Jump(&fall_through);
      for (const Entry& entry : deferred_) {
        builder()->Bind(jump_table, entry.token);

        if (CommandUsesAccumulator(entry.command)) {
          builder()->LoadAccumulatorWithRegister(result_register_);
        }
        execution_control()->PerformCommand(entry.command, entry.statement,
                                            kNoSourcePosition);
      }
    }

    builder()->Bind(&fall_through);
  }

  BytecodeArrayBuilder* builder() { return generator_->builder(); }
  ControlScope* execution_control() { return generator_->execution_control(); }

 private:
  int GetTokenForCommand(Command command, Statement* statement) {
    switch (command) {
      case CMD_RETURN:
        return GetReturnToken();
      case CMD_ASYNC_RETURN:
        return GetAsyncReturnToken();
      case CMD_RETHROW:
        return GetRethrowToken();
      default:
        // TODO(leszeks): We could also search for entries with the same
        // command and statement.
        return GetNewTokenForCommand(command, statement);
    }
  }

  int GetReturnToken() {
    if (return_token_ == -1) {
      return_token_ = GetNewTokenForCommand(CMD_RETURN, nullptr);
    }
    return return_token_;
  }

  int GetAsyncReturnToken() {
    if (async_return_token_ == -1) {
      async_return_token_ = GetNewTokenForCommand(CMD_ASYNC_RETURN, nullptr);
    }
    return async_return_token_;
  }

  int GetRethrowToken() {
    if (rethrow_token_ == -1) {
      rethrow_token_ = GetNewTokenForCommand(CMD_RETHROW, nullptr);
    }
    return rethrow_token_;
  }

  int GetNewTokenForCommand(Command command, Statement* statement) {
    int token = static_cast<int>(deferred_.size());
    deferred_.push_back({command, statement, token});
    return token;
  }

  BytecodeGenerator* generator_;
  ZoneVector<Entry> deferred_;
  Register token_register_;
  Register result_register_;

  // Tokens for commands that don't need a statement.
  int return_token_;
  int async_return_token_;
  int rethrow_token_;
};

// Scoped class for dealing with control flow reaching the function level.
class BytecodeGenerator::ControlScopeForTopLevel final
    : public BytecodeGenerator::ControlScope {
 public:
  explicit ControlScopeForTopLevel(BytecodeGenerator* generator)
      : ControlScope(generator) {}

 protected:
  bool Execute(Command command, Statement* statement,
               int source_position) override {
    switch (command) {
      case CMD_BREAK:  // We should never see break/continue in top-level.
      case CMD_CONTINUE:
        UNREACHABLE();
      case CMD_RETURN:
        // No need to pop contexts, execution leaves the method body.
        generator()->BuildReturn(source_position);
        return true;
      case CMD_ASYNC_RETURN:
        // No need to pop contexts, execution leaves the method body.
        generator()->BuildAsyncReturn(source_position);
        return true;
      case CMD_RETHROW:
        // No need to pop contexts, execution leaves the method body.
        generator()->BuildReThrow();
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
  bool Execute(Command command, Statement* statement,
               int source_position) override {
    if (statement != statement_) return false;
    switch (command) {
      case CMD_BREAK:
        PopContextToExpectedDepth();
        control_builder_->Break();
        return true;
      case CMD_CONTINUE:
      case CMD_RETURN:
      case CMD_ASYNC_RETURN:
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
        loop_builder_(loop_builder) {
    generator->loop_depth_++;
  }
  ~ControlScopeForIteration() override { generator()->loop_depth_--; }

 protected:
  bool Execute(Command command, Statement* statement,
               int source_position) override {
    if (statement != statement_) return false;
    switch (command) {
      case CMD_BREAK:
        PopContextToExpectedDepth();
        loop_builder_->Break();
        return true;
      case CMD_CONTINUE:
        PopContextToExpectedDepth();
        loop_builder_->Continue();
        return true;
      case CMD_RETURN:
      case CMD_ASYNC_RETURN:
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
      : ControlScope(generator) {}

 protected:
  bool Execute(Command command, Statement* statement,
               int source_position) override {
    switch (command) {
      case CMD_BREAK:
      case CMD_CONTINUE:
      case CMD_RETURN:
      case CMD_ASYNC_RETURN:
        break;
      case CMD_RETHROW:
        // No need to pop contexts, execution re-enters the method body via the
        // stack unwinding mechanism which itself restores contexts correctly.
        generator()->BuildReThrow();
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
        commands_(commands) {}

 protected:
  bool Execute(Command command, Statement* statement,
               int source_position) override {
    switch (command) {
      case CMD_BREAK:
      case CMD_CONTINUE:
      case CMD_RETURN:
      case CMD_ASYNC_RETURN:
      case CMD_RETHROW:
        PopContextToExpectedDepth();
        // We don't record source_position here since we don't generate return
        // bytecode right here and will generate it later as part of finally
        // block. Each return bytecode generated in finally block will get own
        // return source position from corresponded return statement or we'll
        // use end of function if no return statement is presented.
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

// Allocate and fetch the coverage indices tracking NaryLogical Expressions.
class BytecodeGenerator::NaryCodeCoverageSlots {
 public:
  NaryCodeCoverageSlots(BytecodeGenerator* generator, NaryOperation* expr)
      : generator_(generator) {
    if (generator_->block_coverage_builder_ == nullptr) return;
    for (size_t i = 0; i < expr->subsequent_length(); i++) {
      coverage_slots_.push_back(
          generator_->AllocateNaryBlockCoverageSlotIfEnabled(expr, i));
    }
  }

  int GetSlotFor(size_t subsequent_expr_index) const {
    if (generator_->block_coverage_builder_ == nullptr) {
      return BlockCoverageBuilder::kNoCoverageArraySlot;
    }
    DCHECK(coverage_slots_.size() > subsequent_expr_index);
    return coverage_slots_[subsequent_expr_index];
  }

 private:
  BytecodeGenerator* generator_;
  std::vector<int> coverage_slots_;
};

void BytecodeGenerator::ControlScope::PerformCommand(Command command,
                                                     Statement* statement,
                                                     int source_position) {
  ControlScope* current = this;
  do {
    if (current->Execute(command, statement, source_position)) {
      return;
    }
    current = current->outer();
  } while (current != nullptr);
  UNREACHABLE();
}

void BytecodeGenerator::ControlScope::PopContextToExpectedDepth() {
  // Pop context to the expected depth. Note that this can in fact pop multiple
  // contexts at once because the {PopContext} bytecode takes a saved register.
  if (generator()->execution_context() != context()) {
    generator()->builder()->PopContext(context()->reg());
  }
}

class BytecodeGenerator::RegisterAllocationScope final {
 public:
  explicit RegisterAllocationScope(BytecodeGenerator* generator)
      : generator_(generator),
        outer_next_register_index_(
            generator->register_allocator()->next_register_index()) {}

  ~RegisterAllocationScope() {
    generator_->register_allocator()->ReleaseRegisters(
        outer_next_register_index_);
  }

 private:
  BytecodeGenerator* generator_;
  int outer_next_register_index_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocationScope);
};

// Scoped base class for determining how the result of an expression will be
// used.
class BytecodeGenerator::ExpressionResultScope {
 public:
  ExpressionResultScope(BytecodeGenerator* generator, Expression::Context kind)
      : generator_(generator),
        outer_(generator->execution_result()),
        allocator_(generator),
        kind_(kind),
        type_hint_(TypeHint::kAny) {
    generator_->set_execution_result(this);
  }

  virtual ~ExpressionResultScope() {
    generator_->set_execution_result(outer_);
  }

  bool IsEffect() const { return kind_ == Expression::kEffect; }
  bool IsValue() const { return kind_ == Expression::kValue; }
  bool IsTest() const { return kind_ == Expression::kTest; }

  TestResultScope* AsTest() {
    DCHECK(IsTest());
    return reinterpret_cast<TestResultScope*>(this);
  }

  // Specify expression always returns a Boolean result value.
  void SetResultIsBoolean() {
    DCHECK_EQ(type_hint_, TypeHint::kAny);
    type_hint_ = TypeHint::kBoolean;
  }

  void SetResultIsString() {
    DCHECK_EQ(type_hint_, TypeHint::kAny);
    type_hint_ = TypeHint::kString;
  }

  TypeHint type_hint() const { return type_hint_; }

 private:
  BytecodeGenerator* generator_;
  ExpressionResultScope* outer_;
  RegisterAllocationScope allocator_;
  Expression::Context kind_;
  TypeHint type_hint_;

  DISALLOW_COPY_AND_ASSIGN(ExpressionResultScope);
};

// Scoped class used when the result of the current expression is not
// expected to produce a result.
class BytecodeGenerator::EffectResultScope final
    : public ExpressionResultScope {
 public:
  explicit EffectResultScope(BytecodeGenerator* generator)
      : ExpressionResultScope(generator, Expression::kEffect) {}
};

// Scoped class used when the result of the current expression to be
// evaluated should go into the interpreter's accumulator.
class BytecodeGenerator::ValueResultScope final : public ExpressionResultScope {
 public:
  explicit ValueResultScope(BytecodeGenerator* generator)
      : ExpressionResultScope(generator, Expression::kValue) {}
};

// Scoped class used when the result of the current expression to be
// evaluated is only tested with jumps to two branches.
class BytecodeGenerator::TestResultScope final : public ExpressionResultScope {
 public:
  TestResultScope(BytecodeGenerator* generator, BytecodeLabels* then_labels,
                  BytecodeLabels* else_labels, TestFallthrough fallthrough)
      : ExpressionResultScope(generator, Expression::kTest),
        result_consumed_by_test_(false),
        fallthrough_(fallthrough),
        then_labels_(then_labels),
        else_labels_(else_labels) {}

  // Used when code special cases for TestResultScope and consumes any
  // possible value by testing and jumping to a then/else label.
  void SetResultConsumedByTest() {
    result_consumed_by_test_ = true;
  }
  bool result_consumed_by_test() { return result_consumed_by_test_; }

  // Inverts the control flow of the operation, swapping the then and else
  // labels and the fallthrough.
  void InvertControlFlow() {
    std::swap(then_labels_, else_labels_);
    fallthrough_ = inverted_fallthrough();
  }

  BytecodeLabel* NewThenLabel() { return then_labels_->New(); }
  BytecodeLabel* NewElseLabel() { return else_labels_->New(); }

  BytecodeLabels* then_labels() const { return then_labels_; }
  BytecodeLabels* else_labels() const { return else_labels_; }

  void set_then_labels(BytecodeLabels* then_labels) {
    then_labels_ = then_labels;
  }
  void set_else_labels(BytecodeLabels* else_labels) {
    else_labels_ = else_labels;
  }

  TestFallthrough fallthrough() const { return fallthrough_; }
  TestFallthrough inverted_fallthrough() const {
    switch (fallthrough_) {
      case TestFallthrough::kThen:
        return TestFallthrough::kElse;
      case TestFallthrough::kElse:
        return TestFallthrough::kThen;
      default:
        return TestFallthrough::kNone;
    }
  }
  void set_fallthrough(TestFallthrough fallthrough) {
    fallthrough_ = fallthrough;
  }

 private:
  bool result_consumed_by_test_;
  TestFallthrough fallthrough_;
  BytecodeLabels* then_labels_;
  BytecodeLabels* else_labels_;

  DISALLOW_COPY_AND_ASSIGN(TestResultScope);
};

// Used to build a list of global declaration initial value pairs.
class BytecodeGenerator::GlobalDeclarationsBuilder final : public ZoneObject {
 public:
  explicit GlobalDeclarationsBuilder(Zone* zone)
      : declarations_(0, zone),
        constant_pool_entry_(0),
        has_constant_pool_entry_(false) {}

  void AddFunctionDeclaration(const AstRawString* name, FeedbackSlot slot,
                              FeedbackSlot literal_slot,
                              FunctionLiteral* func) {
    DCHECK(!slot.IsInvalid());
    declarations_.push_back(Declaration(name, slot, literal_slot, func));
  }

  void AddUndefinedDeclaration(const AstRawString* name, FeedbackSlot slot) {
    DCHECK(!slot.IsInvalid());
    declarations_.push_back(Declaration(name, slot, nullptr));
  }

  Handle<FixedArray> AllocateDeclarations(UnoptimizedCompilationInfo* info,
                                          Handle<Script> script,
                                          Isolate* isolate) {
    DCHECK(has_constant_pool_entry_);
    int array_index = 0;
    Handle<FixedArray> data = isolate->factory()->NewFixedArray(
        static_cast<int>(declarations_.size() * 4), TENURED);
    for (const Declaration& declaration : declarations_) {
      FunctionLiteral* func = declaration.func;
      Handle<Object> initial_value;
      if (func == nullptr) {
        initial_value = isolate->factory()->undefined_value();
      } else {
        initial_value = Compiler::GetSharedFunctionInfo(func, script, isolate);
      }

      // Return a null handle if any initial values can't be created. Caller
      // will set stack overflow.
      if (initial_value.is_null()) return Handle<FixedArray>();

      data->set(array_index++, *declaration.name->string());
      data->set(array_index++, Smi::FromInt(declaration.slot.ToInt()));
      Object* undefined_or_literal_slot;
      if (declaration.literal_slot.IsInvalid()) {
        undefined_or_literal_slot = ReadOnlyRoots(isolate).undefined_value();
      } else {
        undefined_or_literal_slot =
            Smi::FromInt(declaration.literal_slot.ToInt());
      }
      data->set(array_index++, undefined_or_literal_slot);
      data->set(array_index++, *initial_value);
    }
    return data;
  }

  size_t constant_pool_entry() {
    DCHECK(has_constant_pool_entry_);
    return constant_pool_entry_;
  }

  void set_constant_pool_entry(size_t constant_pool_entry) {
    DCHECK(!empty());
    DCHECK(!has_constant_pool_entry_);
    constant_pool_entry_ = constant_pool_entry;
    has_constant_pool_entry_ = true;
  }

  bool empty() { return declarations_.empty(); }

 private:
  struct Declaration {
    Declaration() : slot(FeedbackSlot::Invalid()), func(nullptr) {}
    Declaration(const AstRawString* name, FeedbackSlot slot,
                FeedbackSlot literal_slot, FunctionLiteral* func)
        : name(name), slot(slot), literal_slot(literal_slot), func(func) {}
    Declaration(const AstRawString* name, FeedbackSlot slot,
                FunctionLiteral* func)
        : name(name),
          slot(slot),
          literal_slot(FeedbackSlot::Invalid()),
          func(func) {}

    const AstRawString* name;
    FeedbackSlot slot;
    FeedbackSlot literal_slot;
    FunctionLiteral* func;
  };
  ZoneVector<Declaration> declarations_;
  size_t constant_pool_entry_;
  bool has_constant_pool_entry_;
};

class BytecodeGenerator::CurrentScope final {
 public:
  CurrentScope(BytecodeGenerator* generator, Scope* scope)
      : generator_(generator), outer_scope_(generator->current_scope()) {
    if (scope != nullptr) {
      DCHECK_EQ(outer_scope_, scope->outer_scope());
      generator_->set_current_scope(scope);
    }
  }
  ~CurrentScope() {
    if (outer_scope_ != generator_->current_scope()) {
      generator_->set_current_scope(outer_scope_);
    }
  }

 private:
  BytecodeGenerator* generator_;
  Scope* outer_scope_;
};

class BytecodeGenerator::FeedbackSlotCache : public ZoneObject {
 public:
  explicit FeedbackSlotCache(Zone* zone) : map_(zone) {}

  void Put(FeedbackSlotKind slot_kind, Variable* variable, FeedbackSlot slot) {
    PutImpl(slot_kind, 0, variable, slot);
  }
  void Put(FeedbackSlotKind slot_kind, AstNode* node, FeedbackSlot slot) {
    PutImpl(slot_kind, 0, node, slot);
  }
  void Put(FeedbackSlotKind slot_kind, int variable_index,
           const AstRawString* name, FeedbackSlot slot) {
    PutImpl(slot_kind, variable_index, name, slot);
  }

  FeedbackSlot Get(FeedbackSlotKind slot_kind, Variable* variable) const {
    return GetImpl(slot_kind, 0, variable);
  }
  FeedbackSlot Get(FeedbackSlotKind slot_kind, AstNode* node) const {
    return GetImpl(slot_kind, 0, node);
  }
  FeedbackSlot Get(FeedbackSlotKind slot_kind, int variable_index,
                   const AstRawString* name) const {
    return GetImpl(slot_kind, variable_index, name);
  }

 private:
  typedef std::tuple<FeedbackSlotKind, int, const void*> Key;

  void PutImpl(FeedbackSlotKind slot_kind, int index, const void* node,
               FeedbackSlot slot) {
    Key key = std::make_tuple(slot_kind, index, node);
    auto entry = std::make_pair(key, slot);
    map_.insert(entry);
  }

  FeedbackSlot GetImpl(FeedbackSlotKind slot_kind, int index,
                       const void* node) const {
    Key key = std::make_tuple(slot_kind, index, node);
    auto iter = map_.find(key);
    if (iter != map_.end()) {
      return iter->second;
    }
    return FeedbackSlot();
  }

  ZoneMap<Key, FeedbackSlot> map_;
};

class BytecodeGenerator::IteratorRecord final {
 public:
  IteratorRecord(Register object_register, Register next_register,
                 IteratorType type = IteratorType::kNormal)
      : type_(type), object_(object_register), next_(next_register) {
    DCHECK(object_.is_valid() && next_.is_valid());
  }

  inline IteratorType type() const { return type_; }
  inline Register object() const { return object_; }
  inline Register next() const { return next_; }

 private:
  IteratorType type_;
  Register object_;
  Register next_;
};

#ifdef DEBUG

static bool IsInEagerLiterals(
    FunctionLiteral* literal,
    const ZoneVector<FunctionLiteral*>& eager_literals) {
  for (FunctionLiteral* eager_literal : eager_literals) {
    if (literal == eager_literal) return true;
  }
  return false;
}

#endif  // DEBUG

BytecodeGenerator::BytecodeGenerator(
    UnoptimizedCompilationInfo* info,
    const AstStringConstants* ast_string_constants,
    ZoneVector<FunctionLiteral*>* eager_inner_literals)
    : zone_(info->zone()),
      builder_(zone(), info->num_parameters_including_this(),
               info->scope()->num_stack_slots(), info->feedback_vector_spec(),
               info->SourcePositionRecordingMode()),
      info_(info),
      ast_string_constants_(ast_string_constants),
      closure_scope_(info->scope()),
      current_scope_(info->scope()),
      eager_inner_literals_(eager_inner_literals),
      feedback_slot_cache_(new (zone()) FeedbackSlotCache(zone())),
      globals_builder_(new (zone()) GlobalDeclarationsBuilder(zone())),
      block_coverage_builder_(nullptr),
      global_declarations_(0, zone()),
      function_literals_(0, zone()),
      native_function_literals_(0, zone()),
      object_literals_(0, zone()),
      array_literals_(0, zone()),
      class_literals_(0, zone()),
      template_objects_(0, zone()),
      execution_control_(nullptr),
      execution_context_(nullptr),
      execution_result_(nullptr),
      incoming_new_target_or_generator_(),
      dummy_feedback_slot_(feedback_spec(), FeedbackSlotKind::kCompareOp),
      generator_jump_table_(nullptr),
      suspend_count_(0),
      loop_depth_(0),
      catch_prediction_(HandlerTable::UNCAUGHT) {
  DCHECK_EQ(closure_scope(), closure_scope()->GetClosureScope());
  if (info->has_source_range_map()) {
    block_coverage_builder_ = new (zone())
        BlockCoverageBuilder(zone(), builder(), info->source_range_map());
  }
}

Handle<BytecodeArray> BytecodeGenerator::FinalizeBytecode(
    Isolate* isolate, Handle<Script> script) {
  DCHECK(ThreadId::Current().Equals(isolate->thread_id()));

  AllocateDeferredConstants(isolate, script);

  if (block_coverage_builder_) {
    info()->set_coverage_info(
        isolate->factory()->NewCoverageInfo(block_coverage_builder_->slots()));
    if (FLAG_trace_block_coverage) {
      info()->coverage_info()->Print(info()->literal()->GetDebugName());
    }
  }

  if (HasStackOverflow()) return Handle<BytecodeArray>();
  Handle<BytecodeArray> bytecode_array = builder()->ToBytecodeArray(isolate);

  if (incoming_new_target_or_generator_.is_valid()) {
    bytecode_array->set_incoming_new_target_or_generator_register(
        incoming_new_target_or_generator_);
  }

  return bytecode_array;
}

void BytecodeGenerator::AllocateDeferredConstants(Isolate* isolate,
                                                  Handle<Script> script) {
  // Build global declaration pair arrays.
  for (GlobalDeclarationsBuilder* globals_builder : global_declarations_) {
    Handle<FixedArray> declarations =
        globals_builder->AllocateDeclarations(info(), script, isolate);
    if (declarations.is_null()) return SetStackOverflow();
    builder()->SetDeferredConstantPoolEntry(
        globals_builder->constant_pool_entry(), declarations);
  }

  // Find or build shared function infos.
  for (std::pair<FunctionLiteral*, size_t> literal : function_literals_) {
    FunctionLiteral* expr = literal.first;
    Handle<SharedFunctionInfo> shared_info =
        Compiler::GetSharedFunctionInfo(expr, script, isolate);
    if (shared_info.is_null()) return SetStackOverflow();
    builder()->SetDeferredConstantPoolEntry(literal.second, shared_info);
  }

  // Find or build shared function infos for the native function templates.
  for (std::pair<NativeFunctionLiteral*, size_t> literal :
       native_function_literals_) {
    NativeFunctionLiteral* expr = literal.first;
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);

    // Compute the function template for the native function.
    v8::Local<v8::FunctionTemplate> info =
        expr->extension()->GetNativeFunctionTemplate(
            v8_isolate, Utils::ToLocal(expr->name()));
    DCHECK(!info.IsEmpty());

    Handle<SharedFunctionInfo> shared_info =
        FunctionTemplateInfo::GetOrCreateSharedFunctionInfo(
            isolate, Utils::OpenHandle(*info), expr->name());
    DCHECK(!shared_info.is_null());
    builder()->SetDeferredConstantPoolEntry(literal.second, shared_info);
  }

  // Build object literal constant properties
  for (std::pair<ObjectLiteral*, size_t> literal : object_literals_) {
    ObjectLiteral* object_literal = literal.first;
    if (object_literal->properties_count() > 0) {
      // If constant properties is an empty fixed array, we've already added it
      // to the constant pool when visiting the object literal.
      Handle<ObjectBoilerplateDescription> constant_properties =
          object_literal->GetOrBuildBoilerplateDescription(isolate);

      builder()->SetDeferredConstantPoolEntry(literal.second,
                                              constant_properties);
    }
  }

  // Build array literal constant elements
  for (std::pair<ArrayLiteral*, size_t> literal : array_literals_) {
    ArrayLiteral* array_literal = literal.first;
    Handle<ArrayBoilerplateDescription> constant_elements =
        array_literal->GetOrBuildBoilerplateDescription(isolate);
    builder()->SetDeferredConstantPoolEntry(literal.second, constant_elements);
  }

  // Build class literal boilerplates.
  for (std::pair<ClassLiteral*, size_t> literal : class_literals_) {
    ClassLiteral* class_literal = literal.first;
    Handle<ClassBoilerplate> class_boilerplate =
        ClassBoilerplate::BuildClassBoilerplate(isolate, class_literal);
    builder()->SetDeferredConstantPoolEntry(literal.second, class_boilerplate);
  }

  // Build template literals.
  for (std::pair<GetTemplateObject*, size_t> literal : template_objects_) {
    GetTemplateObject* get_template_object = literal.first;
    Handle<TemplateObjectDescription> description =
        get_template_object->GetOrBuildDescription(isolate);
    builder()->SetDeferredConstantPoolEntry(literal.second, description);
  }
}

void BytecodeGenerator::GenerateBytecode(uintptr_t stack_limit) {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  InitializeAstVisitor(stack_limit);

  // Initialize the incoming context.
  ContextScope incoming_context(this, closure_scope());

  // Initialize control scope.
  ControlScopeForTopLevel control(this);

  RegisterAllocationScope register_scope(this);

  AllocateTopLevelRegisters();

  if (info()->literal()->CanSuspend()) {
    BuildGeneratorPrologue();
  }

  if (closure_scope()->NeedsContext()) {
    // Push a new inner context scope for the function.
    BuildNewLocalActivationContext();
    ContextScope local_function_context(this, closure_scope());
    BuildLocalActivationContextInitialization();
    GenerateBytecodeBody();
  } else {
    GenerateBytecodeBody();
  }

  // Check that we are not falling off the end.
  DCHECK(!builder()->RequiresImplicitReturn());
}

void BytecodeGenerator::GenerateBytecodeBody() {
  // Build the arguments object if it is used.
  VisitArgumentsObject(closure_scope()->arguments());

  // Build rest arguments array if it is used.
  Variable* rest_parameter = closure_scope()->rest_parameter();
  VisitRestArgumentsArray(rest_parameter);

  // Build assignment to the function name or {.this_function}
  // variables if used.
  VisitThisFunctionVariable(closure_scope()->function_var());
  VisitThisFunctionVariable(closure_scope()->this_function_var());

  // Build assignment to {new.target} variable if it is used.
  VisitNewTargetVariable(closure_scope()->new_target_var());

  // Create a generator object if necessary and initialize the
  // {.generator_object} variable.
  if (info()->literal()->CanSuspend()) {
    BuildGeneratorObjectVariableInitialization();
  }

  // Emit tracing call if requested to do so.
  if (FLAG_trace) builder()->CallRuntime(Runtime::kTraceEnter);

  // Emit type profile call.
  if (info()->collect_type_profile()) {
    feedback_spec()->AddTypeProfileSlot();
    int num_parameters = closure_scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Register parameter(builder()->Parameter(i));
      builder()->LoadAccumulatorWithRegister(parameter).CollectTypeProfile(
          closure_scope()->parameter(i)->initializer_position());
    }
  }

  // Visit declarations within the function scope.
  VisitDeclarations(closure_scope()->declarations());

  // Emit initializing assignments for module namespace imports (if any).
  VisitModuleNamespaceImports();

  // Perform a stack-check before the body.
  builder()->StackCheck(info()->literal()->start_position());

  // The derived constructor case is handled in VisitCallSuper.
  if (IsBaseConstructor(function_kind()) &&
      info()->literal()->requires_instance_fields_initializer()) {
    BuildInstanceFieldInitialization(Register::function_closure(),
                                     builder()->Receiver());
  }

  // Visit statements in the function body.
  VisitStatements(info()->literal()->body());

  // Emit an implicit return instruction in case control flow can fall off the
  // end of the function without an explicit return being present on all paths.
  if (builder()->RequiresImplicitReturn()) {
    builder()->LoadUndefined();
    BuildReturn();
  }
}

void BytecodeGenerator::AllocateTopLevelRegisters() {
  if (info()->literal()->CanSuspend()) {
    // Either directly use generator_object_var or allocate a new register for
    // the incoming generator object.
    Variable* generator_object_var = closure_scope()->generator_object_var();
    if (generator_object_var->location() == VariableLocation::LOCAL) {
      incoming_new_target_or_generator_ =
          GetRegisterForLocalVariable(generator_object_var);
    } else {
      incoming_new_target_or_generator_ = register_allocator()->NewRegister();
    }
  } else if (closure_scope()->new_target_var()) {
    // Either directly use new_target_var or allocate a new register for
    // the incoming new target object.
    Variable* new_target_var = closure_scope()->new_target_var();
    if (new_target_var->location() == VariableLocation::LOCAL) {
      incoming_new_target_or_generator_ =
          GetRegisterForLocalVariable(new_target_var);
    } else {
      incoming_new_target_or_generator_ = register_allocator()->NewRegister();
    }
  }
}

void BytecodeGenerator::BuildGeneratorPrologue() {
  DCHECK_GT(info()->literal()->suspend_count(), 0);
  DCHECK(generator_object().is_valid());
  generator_jump_table_ =
      builder()->AllocateJumpTable(info()->literal()->suspend_count(), 0);

  // If the generator is not undefined, this is a resume, so perform state
  // dispatch.
  builder()->SwitchOnGeneratorState(generator_object(), generator_jump_table_);

  // Otherwise, fall-through to the ordinary function prologue, after which we
  // will run into the generator object creation and other extra code inserted
  // by the parser.
}

void BytecodeGenerator::VisitBlock(Block* stmt) {
  // Visit declarations and statements.
  CurrentScope current_scope(this, stmt->scope());
  if (stmt->scope() != nullptr && stmt->scope()->NeedsContext()) {
    BuildNewLocalBlockContext(stmt->scope());
    ContextScope scope(this, stmt->scope());
    VisitBlockDeclarationsAndStatements(stmt);
  } else {
    VisitBlockDeclarationsAndStatements(stmt);
  }
}

void BytecodeGenerator::VisitBlockDeclarationsAndStatements(Block* stmt) {
  BlockBuilder block_builder(builder(), block_coverage_builder_, stmt);
  ControlScopeForBreakable execution_control(this, stmt, &block_builder);
  if (stmt->scope() != nullptr) {
    VisitDeclarations(stmt->scope()->declarations());
  }
  VisitStatements(stmt->statements());
}

void BytecodeGenerator::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      DCHECK(!variable->binding_needs_init());
      FeedbackSlot slot =
          GetCachedLoadGlobalICSlot(NOT_INSIDE_TYPEOF, variable);
      globals_builder()->AddUndefinedDeclaration(variable->raw_name(), slot);
      break;
    }
    case VariableLocation::LOCAL:
      if (variable->binding_needs_init()) {
        Register destination(builder()->Local(variable->index()));
        builder()->LoadTheHole().StoreAccumulatorInRegister(destination);
      }
      break;
    case VariableLocation::PARAMETER:
      if (variable->binding_needs_init()) {
        Register destination(builder()->Parameter(variable->index()));
        builder()->LoadTheHole().StoreAccumulatorInRegister(destination);
      }
      break;
    case VariableLocation::CONTEXT:
      if (variable->binding_needs_init()) {
        DCHECK_EQ(0, execution_context()->ContextChainDepth(variable->scope()));
        builder()->LoadTheHole().StoreContextSlot(execution_context()->reg(),
                                                  variable->index(), 0);
      }
      break;
    case VariableLocation::LOOKUP: {
      DCHECK_EQ(VariableMode::kVar, variable->mode());
      DCHECK(!variable->binding_needs_init());

      Register name = register_allocator()->NewRegister();

      builder()
          ->LoadLiteral(variable->raw_name())
          .StoreAccumulatorInRegister(name)
          .CallRuntime(Runtime::kDeclareEvalVar, name);
      break;
    }
    case VariableLocation::MODULE:
      if (variable->IsExport() && variable->binding_needs_init()) {
        builder()->LoadTheHole();
        BuildVariableAssignment(variable, Token::INIT, HoleCheckMode::kElided);
      }
      // Nothing to do for imports.
      break;
  }
}

void BytecodeGenerator::VisitFunctionDeclaration(FunctionDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  DCHECK(variable->mode() == VariableMode::kLet ||
         variable->mode() == VariableMode::kVar);
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      FeedbackSlot slot =
          GetCachedLoadGlobalICSlot(NOT_INSIDE_TYPEOF, variable);
      FeedbackSlot literal_slot = GetCachedCreateClosureSlot(decl->fun());
      globals_builder()->AddFunctionDeclaration(variable->raw_name(), slot,
                                                literal_slot, decl->fun());
      AddToEagerLiteralsIfEager(decl->fun());
      break;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      VisitForAccumulatorValue(decl->fun());
      BuildVariableAssignment(variable, Token::INIT, HoleCheckMode::kElided);
      break;
    }
    case VariableLocation::CONTEXT: {
      DCHECK_EQ(0, execution_context()->ContextChainDepth(variable->scope()));
      VisitForAccumulatorValue(decl->fun());
      builder()->StoreContextSlot(execution_context()->reg(), variable->index(),
                                  0);
      break;
    }
    case VariableLocation::LOOKUP: {
      RegisterList args = register_allocator()->NewRegisterList(2);
      builder()
          ->LoadLiteral(variable->raw_name())
          .StoreAccumulatorInRegister(args[0]);
      VisitForAccumulatorValue(decl->fun());
      builder()->StoreAccumulatorInRegister(args[1]).CallRuntime(
          Runtime::kDeclareEvalFunction, args);
      break;
    }
    case VariableLocation::MODULE:
      DCHECK_EQ(variable->mode(), VariableMode::kLet);
      DCHECK(variable->IsExport());
      VisitForAccumulatorValue(decl->fun());
      BuildVariableAssignment(variable, Token::INIT, HoleCheckMode::kElided);
      break;
  }
  DCHECK_IMPLIES(decl->fun()->ShouldEagerCompile(),
                 IsInEagerLiterals(decl->fun(), *eager_inner_literals_));
}

void BytecodeGenerator::VisitModuleNamespaceImports() {
  if (!closure_scope()->is_module_scope()) return;

  RegisterAllocationScope register_scope(this);
  Register module_request = register_allocator()->NewRegister();

  ModuleDescriptor* descriptor = closure_scope()->AsModuleScope()->module();
  for (auto entry : descriptor->namespace_imports()) {
    builder()
        ->LoadLiteral(Smi::FromInt(entry->module_request))
        .StoreAccumulatorInRegister(module_request)
        .CallRuntime(Runtime::kGetModuleNamespace, module_request);
    Variable* var = closure_scope()->LookupLocal(entry->local_name);
    DCHECK_NOT_NULL(var);
    BuildVariableAssignment(var, Token::INIT, HoleCheckMode::kElided);
  }
}

void BytecodeGenerator::VisitDeclarations(Declaration::List* declarations) {
  RegisterAllocationScope register_scope(this);
  DCHECK(globals_builder()->empty());
  for (Declaration* decl : *declarations) {
    RegisterAllocationScope register_scope(this);
    Visit(decl);
  }
  if (globals_builder()->empty()) return;

  globals_builder()->set_constant_pool_entry(
      builder()->AllocateDeferredConstantPoolEntry());
  int encoded_flags = DeclareGlobalsEvalFlag::encode(info()->is_eval()) |
                      DeclareGlobalsNativeFlag::encode(info()->is_native());

  // Emit code to declare globals.
  RegisterList args = register_allocator()->NewRegisterList(3);
  builder()
      ->LoadConstantPoolEntry(globals_builder()->constant_pool_entry())
      .StoreAccumulatorInRegister(args[0])
      .LoadLiteral(Smi::FromInt(encoded_flags))
      .StoreAccumulatorInRegister(args[1])
      .MoveRegister(Register::function_closure(), args[2])
      .CallRuntime(Runtime::kDeclareGlobals, args);

  // Push and reset globals builder.
  global_declarations_.push_back(globals_builder());
  globals_builder_ = new (zone()) GlobalDeclarationsBuilder(zone());
}

void BytecodeGenerator::VisitStatements(ZonePtrList<Statement>* statements) {
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
  ConditionalControlFlowBuilder conditional_builder(
      builder(), block_coverage_builder_, stmt);
  builder()->SetStatementPosition(stmt);

  if (stmt->condition()->ToBooleanIsTrue()) {
    // Generate then block unconditionally as always true.
    conditional_builder.Then();
    Visit(stmt->then_statement());
  } else if (stmt->condition()->ToBooleanIsFalse()) {
    // Generate else block unconditionally if it exists.
    if (stmt->HasElseStatement()) {
      conditional_builder.Else();
      Visit(stmt->else_statement());
    }
  } else {
    // TODO(oth): If then statement is BreakStatement or
    // ContinueStatement we can reduce number of generated
    // jump/jump_ifs here. See BasicLoops test.
    VisitForTest(stmt->condition(), conditional_builder.then_labels(),
                 conditional_builder.else_labels(), TestFallthrough::kThen);

    conditional_builder.Then();
    Visit(stmt->then_statement());

    if (stmt->HasElseStatement()) {
      conditional_builder.JumpToEnd();
      conditional_builder.Else();
      Visit(stmt->else_statement());
    }
  }
}

void BytecodeGenerator::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* stmt) {
  Visit(stmt->statement());
}

void BytecodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  AllocateBlockCoverageSlotIfEnabled(stmt, SourceRangeKind::kContinuation);
  builder()->SetStatementPosition(stmt);
  execution_control()->Continue(stmt->target());
}

void BytecodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  AllocateBlockCoverageSlotIfEnabled(stmt, SourceRangeKind::kContinuation);
  builder()->SetStatementPosition(stmt);
  execution_control()->Break(stmt->target());
}

void BytecodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  AllocateBlockCoverageSlotIfEnabled(stmt, SourceRangeKind::kContinuation);
  builder()->SetStatementPosition(stmt);
  VisitForAccumulatorValue(stmt->expression());
  if (stmt->is_async_return()) {
    execution_control()->AsyncReturnAccumulator(stmt->end_position());
  } else {
    execution_control()->ReturnAccumulator(stmt->end_position());
  }
}

void BytecodeGenerator::VisitWithStatement(WithStatement* stmt) {
  builder()->SetStatementPosition(stmt);
  VisitForAccumulatorValue(stmt->expression());
  BuildNewLocalWithContext(stmt->scope());
  VisitInScope(stmt->statement(), stmt->scope());
}

void BytecodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  // We need this scope because we visit for register values. We have to
  // maintain a execution result scope where registers can be allocated.
  ZonePtrList<CaseClause>* clauses = stmt->cases();
  SwitchBuilder switch_builder(builder(), block_coverage_builder_, stmt,
                               clauses->length());
  ControlScopeForBreakable scope(this, stmt, &switch_builder);
  int default_index = -1;

  builder()->SetStatementPosition(stmt);

  // Keep the switch value in a register until a case matches.
  Register tag = VisitForRegisterValue(stmt->tag());
  FeedbackSlot slot = clauses->length() > 0
                          ? feedback_spec()->AddCompareICSlot()
                          : FeedbackSlot::Invalid();

  // Iterate over all cases and create nodes for label comparison.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);

    // The default is not a test, remember index.
    if (clause->is_default()) {
      default_index = i;
      continue;
    }

    // Perform label comparison as if via '===' with tag.
    VisitForAccumulatorValue(clause->label());
    builder()->CompareOperation(Token::Value::EQ_STRICT, tag,
                                feedback_index(slot));
    switch_builder.Case(ToBooleanMode::kAlreadyBoolean, i);
  }

  if (default_index >= 0) {
    // Emit default jump if there is a default case.
    switch_builder.DefaultAt(default_index);
  } else {
    // Otherwise if we have reached here none of the cases matched, so jump to
    // the end.
    switch_builder.Break();
  }

  // Iterate over all cases and create the case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);
    switch_builder.SetCaseTarget(i, clause);
    VisitStatements(clause->statements());
  }
}

void BytecodeGenerator::VisitIterationBody(IterationStatement* stmt,
                                           LoopBuilder* loop_builder) {
  loop_builder->LoopBody();
  ControlScopeForIteration execution_control(this, stmt, loop_builder);
  builder()->StackCheck(stmt->position());
  Visit(stmt->body());
  loop_builder->BindContinueTarget();
}

void BytecodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  LoopBuilder loop_builder(builder(), block_coverage_builder_, stmt);
  if (stmt->cond()->ToBooleanIsFalse()) {
    VisitIterationBody(stmt, &loop_builder);
  } else if (stmt->cond()->ToBooleanIsTrue()) {
    loop_builder.LoopHeader();
    VisitIterationBody(stmt, &loop_builder);
    loop_builder.JumpToHeader(loop_depth_);
  } else {
    loop_builder.LoopHeader();
    VisitIterationBody(stmt, &loop_builder);
    builder()->SetExpressionAsStatementPosition(stmt->cond());
    BytecodeLabels loop_backbranch(zone());
    VisitForTest(stmt->cond(), &loop_backbranch, loop_builder.break_labels(),
                 TestFallthrough::kThen);
    loop_backbranch.Bind(builder());
    loop_builder.JumpToHeader(loop_depth_);
  }
}

void BytecodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  LoopBuilder loop_builder(builder(), block_coverage_builder_, stmt);

  if (stmt->cond()->ToBooleanIsFalse()) {
    // If the condition is false there is no need to generate the loop.
    return;
  }

  loop_builder.LoopHeader();
  if (!stmt->cond()->ToBooleanIsTrue()) {
    builder()->SetExpressionAsStatementPosition(stmt->cond());
    BytecodeLabels loop_body(zone());
    VisitForTest(stmt->cond(), &loop_body, loop_builder.break_labels(),
                 TestFallthrough::kThen);
    loop_body.Bind(builder());
  }
  VisitIterationBody(stmt, &loop_builder);
  loop_builder.JumpToHeader(loop_depth_);
}

void BytecodeGenerator::VisitForStatement(ForStatement* stmt) {
  LoopBuilder loop_builder(builder(), block_coverage_builder_, stmt);

  if (stmt->init() != nullptr) {
    Visit(stmt->init());
  }
  if (stmt->cond() && stmt->cond()->ToBooleanIsFalse()) {
    // If the condition is known to be false there is no need to generate
    // body, next or condition blocks. Init block should be generated.
    return;
  }

  loop_builder.LoopHeader();
  if (stmt->cond() && !stmt->cond()->ToBooleanIsTrue()) {
    builder()->SetExpressionAsStatementPosition(stmt->cond());
    BytecodeLabels loop_body(zone());
    VisitForTest(stmt->cond(), &loop_body, loop_builder.break_labels(),
                 TestFallthrough::kThen);
    loop_body.Bind(builder());
  }
  VisitIterationBody(stmt, &loop_builder);
  if (stmt->next() != nullptr) {
    builder()->SetStatementPosition(stmt->next());
    Visit(stmt->next());
  }
  loop_builder.JumpToHeader(loop_depth_);
}

void BytecodeGenerator::VisitForInAssignment(Expression* expr) {
  DCHECK(expr->IsValidReferenceExpression());

  // Evaluate assignment starting with the value to be stored in the
  // accumulator.
  Property* property = expr->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->AsVariableProxy();
      BuildVariableAssignment(proxy->var(), Token::ASSIGN,
                              proxy->hole_check_mode());
      break;
    }
    case NAMED_PROPERTY: {
      RegisterAllocationScope register_scope(this);
      Register value = register_allocator()->NewRegister();
      builder()->StoreAccumulatorInRegister(value);
      Register object = VisitForRegisterValue(property->obj());
      const AstRawString* name =
          property->key()->AsLiteral()->AsRawPropertyName();
      builder()->LoadAccumulatorWithRegister(value);
      FeedbackSlot slot = GetCachedStoreICSlot(property->obj(), name);
      builder()->StoreNamedProperty(object, name, feedback_index(slot),
                                    language_mode());
      builder()->LoadAccumulatorWithRegister(value);
      break;
    }
    case KEYED_PROPERTY: {
      RegisterAllocationScope register_scope(this);
      Register value = register_allocator()->NewRegister();
      builder()->StoreAccumulatorInRegister(value);
      Register object = VisitForRegisterValue(property->obj());
      Register key = VisitForRegisterValue(property->key());
      builder()->LoadAccumulatorWithRegister(value);
      FeedbackSlot slot = feedback_spec()->AddKeyedStoreICSlot(language_mode());
      builder()->StoreKeyedProperty(object, key, feedback_index(slot),
                                    language_mode());
      builder()->LoadAccumulatorWithRegister(value);
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      RegisterAllocationScope register_scope(this);
      RegisterList args = register_allocator()->NewRegisterList(4);
      builder()->StoreAccumulatorInRegister(args[3]);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), args[0]);
      VisitForRegisterValue(super_property->home_object(), args[1]);
      builder()
          ->LoadLiteral(property->key()->AsLiteral()->AsRawPropertyName())
          .StoreAccumulatorInRegister(args[2])
          .CallRuntime(StoreToSuperRuntimeId(), args);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      RegisterAllocationScope register_scope(this);
      RegisterList args = register_allocator()->NewRegisterList(4);
      builder()->StoreAccumulatorInRegister(args[3]);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), args[0]);
      VisitForRegisterValue(super_property->home_object(), args[1]);
      VisitForRegisterValue(property->key(), args[2]);
      builder()->CallRuntime(StoreKeyedToSuperRuntimeId(), args);
      break;
    }
  }
}

void BytecodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  if (stmt->subject()->IsNullLiteral() ||
      stmt->subject()->IsUndefinedLiteral()) {
    // ForIn generates lots of code, skip if it wouldn't produce any effects.
    return;
  }

  BytecodeLabel subject_null_label, subject_undefined_label;
  FeedbackSlot slot = feedback_spec()->AddForInSlot();

  // Prepare the state for executing ForIn.
  builder()->SetExpressionAsStatementPosition(stmt->subject());
  VisitForAccumulatorValue(stmt->subject());
  builder()->JumpIfUndefined(&subject_undefined_label);
  builder()->JumpIfNull(&subject_null_label);
  Register receiver = register_allocator()->NewRegister();
  builder()->ToObject(receiver);

  // Used as kRegTriple and kRegPair in ForInPrepare and ForInNext.
  RegisterList triple = register_allocator()->NewRegisterList(3);
  Register cache_length = triple[2];
  builder()->ForInEnumerate(receiver);
  builder()->ForInPrepare(triple, feedback_index(slot));

  // Set up loop counter
  Register index = register_allocator()->NewRegister();
  builder()->LoadLiteral(Smi::kZero);
  builder()->StoreAccumulatorInRegister(index);

  // The loop
  {
    LoopBuilder loop_builder(builder(), block_coverage_builder_, stmt);
    loop_builder.LoopHeader();
    builder()->SetExpressionAsStatementPosition(stmt->each());
    builder()->ForInContinue(index, cache_length);
    loop_builder.BreakIfFalse(ToBooleanMode::kAlreadyBoolean);
    builder()->ForInNext(receiver, index, triple.Truncate(2),
                         feedback_index(slot));
    loop_builder.ContinueIfUndefined();
    VisitForInAssignment(stmt->each());
    VisitIterationBody(stmt, &loop_builder);
    builder()->ForInStep(index);
    builder()->StoreAccumulatorInRegister(index);
    loop_builder.JumpToHeader(loop_depth_);
  }
  builder()->Bind(&subject_null_label);
  builder()->Bind(&subject_undefined_label);
}

void BytecodeGenerator::VisitForOfStatement(ForOfStatement* stmt) {
  LoopBuilder loop_builder(builder(), block_coverage_builder_, stmt);

  builder()->SetExpressionAsStatementPosition(stmt->assign_iterator());
  VisitForEffect(stmt->assign_iterator());
  VisitForEffect(stmt->assign_next());

  loop_builder.LoopHeader();
  builder()->SetExpressionAsStatementPosition(stmt->next_result());
  VisitForEffect(stmt->next_result());
  TypeHint type_hint = VisitForAccumulatorValue(stmt->result_done());
  loop_builder.BreakIfTrue(ToBooleanModeFromTypeHint(type_hint));

  VisitForEffect(stmt->assign_each());
  VisitIterationBody(stmt, &loop_builder);
  loop_builder.JumpToHeader(loop_depth_);
}

void BytecodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  // Update catch prediction tracking. The updated catch_prediction value lasts
  // until the end of the try_block in the AST node, and does not apply to the
  // catch_block.
  HandlerTable::CatchPrediction outer_catch_prediction = catch_prediction();
  set_catch_prediction(stmt->GetCatchPrediction(outer_catch_prediction));

  TryCatchBuilder try_control_builder(builder(), block_coverage_builder_, stmt,
                                      catch_prediction());

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
    set_catch_prediction(outer_catch_prediction);
  }
  try_control_builder.EndTry();

  if (stmt->scope()) {
    // Create a catch scope that binds the exception.
    BuildNewLocalCatchContext(stmt->scope());
    builder()->StoreAccumulatorInRegister(context);
  }

  // If requested, clear message object as we enter the catch block.
  if (stmt->ShouldClearPendingException(outer_catch_prediction)) {
    builder()->LoadTheHole().SetPendingMessage();
  }

  // Load the catch context into the accumulator.
  builder()->LoadAccumulatorWithRegister(context);

  // Evaluate the catch-block.
  if (stmt->scope()) {
    VisitInScope(stmt->catch_block(), stmt->scope());
  } else {
    VisitBlock(stmt->catch_block());
  }
  try_control_builder.EndCatch();
}

void BytecodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  // We can't know whether the finally block will override ("catch") an
  // exception thrown in the try block, so we just adopt the outer prediction.
  TryFinallyBuilder try_control_builder(builder(), block_coverage_builder_,
                                        stmt, catch_prediction());

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
  builder()->LoadTheHole().SetPendingMessage().StoreAccumulatorInRegister(
      message);

  // Evaluate the finally-block.
  Visit(stmt->finally_block());
  try_control_builder.EndFinally();

  // Pending message object is restored on exit.
  builder()->LoadAccumulatorWithRegister(message).SetPendingMessage();

  // Dynamic dispatch after the finally-block.
  commands.ApplyDeferredCommands();
}

void BytecodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
  builder()->SetStatementPosition(stmt);
  builder()->Debugger();
}

void BytecodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  DCHECK(expr->scope()->outer_scope() == current_scope());
  uint8_t flags = CreateClosureFlags::Encode(
      expr->pretenure(), closure_scope()->is_function_scope());
  size_t entry = builder()->AllocateDeferredConstantPoolEntry();
  FeedbackSlot slot = GetCachedCreateClosureSlot(expr);
  builder()->CreateClosure(entry, feedback_index(slot), flags);
  function_literals_.push_back(std::make_pair(expr, entry));
  AddToEagerLiteralsIfEager(expr);
}

void BytecodeGenerator::AddToEagerLiteralsIfEager(FunctionLiteral* literal) {
  if (eager_inner_literals_ && literal->ShouldEagerCompile()) {
    DCHECK(!IsInEagerLiterals(literal, *eager_inner_literals_));
    eager_inner_literals_->push_back(literal);
  }
}

bool BytecodeGenerator::ShouldOptimizeAsOneShot() const {
  if (!FLAG_enable_one_shot_optimization) return false;

  if (loop_depth_ > 0) return false;

  // A non-top-level iife is likely to be executed multiple times and so
  // shouldn`t be optimized as one-shot.
  bool is_toplevel_iife = info()->literal()->is_iife() &&
                          current_scope()->outer_scope()->is_script_scope();
  return info()->literal()->is_toplevel() || is_toplevel_iife;
}

void BytecodeGenerator::BuildClassLiteral(ClassLiteral* expr) {
  size_t class_boilerplate_entry =
      builder()->AllocateDeferredConstantPoolEntry();
  class_literals_.push_back(std::make_pair(expr, class_boilerplate_entry));

  VisitDeclarations(expr->scope()->declarations());
  Register class_constructor = register_allocator()->NewRegister();

  {
    RegisterAllocationScope register_scope(this);
    RegisterList args = register_allocator()->NewGrowableRegisterList();

    Register class_boilerplate = register_allocator()->GrowRegisterList(&args);
    Register class_constructor_in_args =
        register_allocator()->GrowRegisterList(&args);
    Register super_class = register_allocator()->GrowRegisterList(&args);
    DCHECK_EQ(ClassBoilerplate::kFirstDynamicArgumentIndex,
              args.register_count());

    VisitForAccumulatorValueOrTheHole(expr->extends());
    builder()->StoreAccumulatorInRegister(super_class);

    VisitFunctionLiteral(expr->constructor());
    builder()
        ->StoreAccumulatorInRegister(class_constructor)
        .MoveRegister(class_constructor, class_constructor_in_args)
        .LoadConstantPoolEntry(class_boilerplate_entry)
        .StoreAccumulatorInRegister(class_boilerplate);

    // Create computed names and method values nodes to store into the literal.
    for (int i = 0; i < expr->properties()->length(); i++) {
      ClassLiteral::Property* property = expr->properties()->at(i);
      if (property->is_computed_name()) {
        DCHECK_NE(property->kind(), ClassLiteral::Property::PRIVATE_FIELD);
        Register key = register_allocator()->GrowRegisterList(&args);

        builder()->SetExpressionAsStatementPosition(property->key());
        BuildLoadPropertyKey(property, key);
        if (property->is_static()) {
          // The static prototype property is read only. We handle the non
          // computed property name case in the parser. Since this is the only
          // case where we need to check for an own read only property we
          // special case this so we do not need to do this for every property.

          FeedbackSlot slot = GetDummyCompareICSlot();
          BytecodeLabel done;
          builder()
              ->LoadLiteral(ast_string_constants()->prototype_string())
              .CompareOperation(Token::Value::EQ_STRICT, key,
                                feedback_index(slot))
              .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &done)
              .CallRuntime(Runtime::kThrowStaticPrototypeError)
              .Bind(&done);
        }

        if (property->kind() == ClassLiteral::Property::PUBLIC_FIELD) {
          // Initialize field's name variable with the computed name.
          DCHECK_NOT_NULL(property->computed_name_var());
          builder()->LoadAccumulatorWithRegister(key);
          BuildVariableAssignment(property->computed_name_var(), Token::INIT,
                                  HoleCheckMode::kElided);
        }
      }

      if (property->kind() == ClassLiteral::Property::PUBLIC_FIELD) {
        // We don't compute field's value here, but instead do it in the
        // initializer function.
        continue;
      } else if (property->kind() == ClassLiteral::Property::PRIVATE_FIELD) {
        builder()->CallRuntime(Runtime::kCreatePrivateFieldSymbol);
        DCHECK_NOT_NULL(property->private_field_name_var());
        BuildVariableAssignment(property->private_field_name_var(), Token::INIT,
                                HoleCheckMode::kElided);
        continue;
      }

      Register value = register_allocator()->GrowRegisterList(&args);
      VisitForRegisterValue(property->value(), value);
    }

    builder()->CallRuntime(Runtime::kDefineClass, args);
  }
  Register prototype = register_allocator()->NewRegister();
  builder()->StoreAccumulatorInRegister(prototype);

  // Assign to class variable.
  if (expr->class_variable() != nullptr) {
    DCHECK(expr->class_variable()->IsStackLocal() ||
           expr->class_variable()->IsContextSlot());
    builder()->LoadAccumulatorWithRegister(class_constructor);
    BuildVariableAssignment(expr->class_variable(), Token::INIT,
                            HoleCheckMode::kElided);
  }

  if (expr->instance_fields_initializer_function() != nullptr) {
    Register initializer =
        VisitForRegisterValue(expr->instance_fields_initializer_function());

    if (FunctionLiteral::NeedsHomeObject(
            expr->instance_fields_initializer_function())) {
      FeedbackSlot slot = feedback_spec()->AddStoreICSlot(language_mode());
      builder()->LoadAccumulatorWithRegister(prototype).StoreHomeObjectProperty(
          initializer, feedback_index(slot), language_mode());
    }

    FeedbackSlot slot = feedback_spec()->AddStoreICSlot(language_mode());
    builder()
        ->LoadAccumulatorWithRegister(initializer)
        .StoreClassFieldsInitializer(class_constructor, feedback_index(slot))
        .LoadAccumulatorWithRegister(class_constructor);
  }

  if (expr->static_fields_initializer() != nullptr) {
    RegisterList args = register_allocator()->NewRegisterList(1);
    Register initializer =
        VisitForRegisterValue(expr->static_fields_initializer());

    if (FunctionLiteral::NeedsHomeObject(expr->static_fields_initializer())) {
      FeedbackSlot slot = feedback_spec()->AddStoreICSlot(language_mode());
      builder()
          ->LoadAccumulatorWithRegister(class_constructor)
          .StoreHomeObjectProperty(initializer, feedback_index(slot),
                                   language_mode());
    }

    builder()
        ->MoveRegister(class_constructor, args[0])
        .CallProperty(initializer, args,
                      feedback_index(feedback_spec()->AddCallICSlot()));
  }
  builder()->LoadAccumulatorWithRegister(class_constructor);
}

void BytecodeGenerator::VisitClassLiteral(ClassLiteral* expr) {
  CurrentScope current_scope(this, expr->scope());
  DCHECK_NOT_NULL(expr->scope());
  if (expr->scope()->NeedsContext()) {
    BuildNewLocalBlockContext(expr->scope());
    ContextScope scope(this, expr->scope());
    BuildClassLiteral(expr);
  } else {
    BuildClassLiteral(expr);
  }
}

void BytecodeGenerator::VisitInitializeClassFieldsStatement(
    InitializeClassFieldsStatement* stmt) {
  RegisterList args = register_allocator()->NewRegisterList(3);
  Register constructor = args[0], key = args[1], value = args[2];
  builder()->MoveRegister(builder()->Receiver(), constructor);

  for (int i = 0; i < stmt->fields()->length(); i++) {
    ClassLiteral::Property* property = stmt->fields()->at(i);

    if (property->is_computed_name()) {
      DCHECK_EQ(property->kind(), ClassLiteral::Property::PUBLIC_FIELD);
      Variable* var = property->computed_name_var();
      DCHECK_NOT_NULL(var);
      // The computed name is already evaluated and stored in a
      // variable at class definition time.
      BuildVariableLoad(var, HoleCheckMode::kElided);
      builder()->StoreAccumulatorInRegister(key);
    } else if (property->kind() == ClassLiteral::Property::PRIVATE_FIELD) {
      Variable* private_field_name_var = property->private_field_name_var();
      DCHECK_NOT_NULL(private_field_name_var);
      BuildVariableLoad(private_field_name_var, HoleCheckMode::kElided);
      builder()->StoreAccumulatorInRegister(key);
    } else {
      BuildLoadPropertyKey(property, key);
    }

    builder()->SetExpressionAsStatementPosition(property->value());
    VisitForRegisterValue(property->value(), value);
    VisitSetHomeObject(value, constructor, property);

    Runtime::FunctionId function_id =
        property->kind() == ClassLiteral::Property::PUBLIC_FIELD
            ? Runtime::kCreateDataProperty
            : Runtime::kAddPrivateField;
    builder()->CallRuntime(function_id, args);
  }
}

void BytecodeGenerator::BuildInstanceFieldInitialization(Register constructor,
                                                         Register instance) {
  RegisterList args = register_allocator()->NewRegisterList(1);
  Register initializer = register_allocator()->NewRegister();

  FeedbackSlot slot = feedback_spec()->AddLoadICSlot();
  BytecodeLabel done;

  builder()
      ->LoadClassFieldsInitializer(constructor, feedback_index(slot))
      // TODO(gsathya): This jump can be elided for the base
      // constructor and derived constructor. This is only required
      // when called from an arrow function.
      .JumpIfUndefined(&done)
      .StoreAccumulatorInRegister(initializer)
      .MoveRegister(instance, args[0])
      .CallProperty(initializer, args,
                    feedback_index(feedback_spec()->AddCallICSlot()))
      .Bind(&done);
}

void BytecodeGenerator::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
  size_t entry = builder()->AllocateDeferredConstantPoolEntry();
  FeedbackSlot slot = feedback_spec()->AddCreateClosureSlot();
  builder()->CreateClosure(entry, feedback_index(slot), NOT_TENURED);
  native_function_literals_.push_back(std::make_pair(expr, entry));
}

void BytecodeGenerator::VisitDoExpression(DoExpression* expr) {
  VisitBlock(expr->block());
  VisitVariableProxy(expr->result());
}

void BytecodeGenerator::VisitConditional(Conditional* expr) {
  ConditionalControlFlowBuilder conditional_builder(
      builder(), block_coverage_builder_, expr);

  if (expr->condition()->ToBooleanIsTrue()) {
    // Generate then block unconditionally as always true.
    conditional_builder.Then();
    VisitForAccumulatorValue(expr->then_expression());
  } else if (expr->condition()->ToBooleanIsFalse()) {
    // Generate else block unconditionally if it exists.
    conditional_builder.Else();
    VisitForAccumulatorValue(expr->else_expression());
  } else {
    VisitForTest(expr->condition(), conditional_builder.then_labels(),
                 conditional_builder.else_labels(), TestFallthrough::kThen);

    conditional_builder.Then();
    VisitForAccumulatorValue(expr->then_expression());
    conditional_builder.JumpToEnd();

    conditional_builder.Else();
    VisitForAccumulatorValue(expr->else_expression());
  }
}

void BytecodeGenerator::VisitLiteral(Literal* expr) {
  if (execution_result()->IsEffect()) return;
  switch (expr->type()) {
    case Literal::kSmi:
      builder()->LoadLiteral(expr->AsSmiLiteral());
      break;
    case Literal::kHeapNumber:
      builder()->LoadLiteral(expr->AsNumber());
      break;
    case Literal::kUndefined:
      builder()->LoadUndefined();
      break;
    case Literal::kBoolean:
      builder()->LoadBoolean(expr->ToBooleanIsTrue());
      execution_result()->SetResultIsBoolean();
      break;
    case Literal::kNull:
      builder()->LoadNull();
      break;
    case Literal::kTheHole:
      builder()->LoadTheHole();
      break;
    case Literal::kString:
      builder()->LoadLiteral(expr->AsRawString());
      execution_result()->SetResultIsString();
      break;
    case Literal::kSymbol:
      builder()->LoadLiteral(expr->AsSymbol());
      break;
    case Literal::kBigInt:
      builder()->LoadLiteral(expr->AsBigInt());
      break;
  }
}

void BytecodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  // Materialize a regular expression literal.
  builder()->CreateRegExpLiteral(
      expr->raw_pattern(), feedback_index(feedback_spec()->AddLiteralSlot()),
      expr->flags());
}

void BytecodeGenerator::BuildCreateObjectLiteral(Register literal,
                                                 uint8_t flags, size_t entry) {
  if (ShouldOptimizeAsOneShot()) {
    RegisterList args = register_allocator()->NewRegisterList(2);
    builder()
        ->LoadConstantPoolEntry(entry)
        .StoreAccumulatorInRegister(args[0])
        .LoadLiteral(Smi::FromInt(flags))
        .StoreAccumulatorInRegister(args[1])
        .CallRuntime(Runtime::kCreateObjectLiteralWithoutAllocationSite, args)
        .StoreAccumulatorInRegister(literal);

  } else {
    // TODO(cbruni): Directly generate runtime call for literals we cannot
    // optimize once the CreateShallowObjectLiteral stub is in sync with the TF
    // optimizations.
    int literal_index = feedback_index(feedback_spec()->AddLiteralSlot());
    builder()->CreateObjectLiteral(entry, literal_index, flags, literal);
  }
}

void BytecodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  expr->InitDepthAndFlags();

  // Fast path for the empty object literal which doesn't need an
  // AllocationSite.
  if (expr->IsEmptyObjectLiteral()) {
    DCHECK(expr->IsFastCloningSupported());
    builder()->CreateEmptyObjectLiteral();
    return;
  }

  // Deep-copy the literal boilerplate.
  uint8_t flags = CreateObjectLiteralFlags::Encode(
      expr->ComputeFlags(), expr->IsFastCloningSupported());

  Register literal = register_allocator()->NewRegister();

  // Create literal object.
  int property_index = 0;
  bool clone_object_spread =
      expr->properties()->first()->kind() == ObjectLiteral::Property::SPREAD;
  if (clone_object_spread) {
    // Avoid the slow path for spreads in the following common cases:
    //   1) `let obj = { ...source }`
    //   2) `let obj = { ...source, override: 1 }`
    //   3) `let obj = { ...source, ...overrides }`
    RegisterAllocationScope register_scope(this);
    Expression* property = expr->properties()->first()->value();
    Register from_value = VisitForRegisterValue(property);

    BytecodeLabels clone_object(zone());
    builder()->JumpIfUndefined(clone_object.New());
    builder()->JumpIfNull(clone_object.New());
    builder()->ToObject(from_value);

    clone_object.Bind(builder());
    int clone_index = feedback_index(feedback_spec()->AddCloneObjectSlot());
    builder()->CloneObject(from_value, flags, clone_index);
    builder()->StoreAccumulatorInRegister(literal);
    property_index++;
  } else {
    size_t entry;
    // If constant properties is an empty fixed array, use a cached empty fixed
    // array to ensure it's only added to the constant pool once.
    if (expr->properties_count() == 0) {
      entry = builder()->EmptyObjectBoilerplateDescriptionConstantPoolEntry();
    } else {
      entry = builder()->AllocateDeferredConstantPoolEntry();
      object_literals_.push_back(std::make_pair(expr, entry));
    }
    BuildCreateObjectLiteral(literal, flags, entry);
  }

  // Store computed values into the literal.
  AccessorTable accessor_table(zone());
  for (; property_index < expr->properties()->length(); property_index++) {
    ObjectLiteral::Property* property = expr->properties()->at(property_index);
    if (property->is_computed_name()) break;
    if (!clone_object_spread && property->IsCompileTimeValue()) continue;

    RegisterAllocationScope inner_register_scope(this);
    Literal* key = property->key()->AsLiteral();
    switch (property->kind()) {
      case ObjectLiteral::Property::SPREAD:
        UNREACHABLE();
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(clone_object_spread || !property->value()->IsCompileTimeValue());
        V8_FALLTHROUGH;
      case ObjectLiteral::Property::COMPUTED: {
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->IsStringLiteral()) {
          DCHECK(key->IsPropertyName());
          if (property->emit_store()) {
            builder()->SetExpressionPosition(property->value());
            VisitForAccumulatorValue(property->value());
            FeedbackSlot slot = feedback_spec()->AddStoreOwnICSlot();
            if (FunctionLiteral::NeedsHomeObject(property->value())) {
              RegisterAllocationScope register_scope(this);
              Register value = register_allocator()->NewRegister();
              builder()->StoreAccumulatorInRegister(value);
              builder()->StoreNamedOwnProperty(
                  literal, key->AsRawPropertyName(), feedback_index(slot));
              VisitSetHomeObject(value, literal, property);
            } else {
              builder()->StoreNamedOwnProperty(
                  literal, key->AsRawPropertyName(), feedback_index(slot));
            }
          } else {
            builder()->SetExpressionPosition(property->value());
            VisitForEffect(property->value());
          }
        } else {
          RegisterList args = register_allocator()->NewRegisterList(4);

          builder()->MoveRegister(literal, args[0]);
          builder()->SetExpressionPosition(property->key());
          VisitForRegisterValue(property->key(), args[1]);
          builder()->SetExpressionPosition(property->value());
          VisitForRegisterValue(property->value(), args[2]);
          if (property->emit_store()) {
            builder()
                ->LoadLiteral(Smi::FromEnum(LanguageMode::kSloppy))
                .StoreAccumulatorInRegister(args[3])
                .CallRuntime(Runtime::kSetKeyedProperty, args);
            Register value = args[2];
            VisitSetHomeObject(value, literal, property);
          }
        }
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE: {
        // __proto__:null is handled by CreateObjectLiteral.
        if (property->IsNullPrototype()) break;
        DCHECK(property->emit_store());
        DCHECK(!property->NeedsSetFunctionName());
        RegisterList args = register_allocator()->NewRegisterList(2);
        builder()->MoveRegister(literal, args[0]);
        builder()->SetExpressionPosition(property->value());
        VisitForRegisterValue(property->value(), args[1]);
        builder()->CallRuntime(Runtime::kInternalSetPrototype, args);
        break;
      }
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          accessor_table.lookup(key)->second->getter = property;
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          accessor_table.lookup(key)->second->setter = property;
        }
        break;
    }
  }

  // Define accessors, using only a single call to the runtime for each pair of
  // corresponding getters and setters.
  for (AccessorTable::Iterator it = accessor_table.begin();
       it != accessor_table.end(); ++it) {
    RegisterAllocationScope inner_register_scope(this);
    RegisterList args = register_allocator()->NewRegisterList(5);
    builder()->MoveRegister(literal, args[0]);
    VisitForRegisterValue(it->first, args[1]);
    VisitObjectLiteralAccessor(literal, it->second->getter, args[2]);
    VisitObjectLiteralAccessor(literal, it->second->setter, args[3]);
    builder()
        ->LoadLiteral(Smi::FromInt(NONE))
        .StoreAccumulatorInRegister(args[4])
        .CallRuntime(Runtime::kDefineAccessorPropertyUnchecked, args);
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

    if (property->IsPrototype()) {
      // __proto__:null is handled by CreateObjectLiteral.
      if (property->IsNullPrototype()) continue;
      DCHECK(property->emit_store());
      DCHECK(!property->NeedsSetFunctionName());
      RegisterList args = register_allocator()->NewRegisterList(2);
      builder()->MoveRegister(literal, args[0]);
      builder()->SetExpressionPosition(property->value());
      VisitForRegisterValue(property->value(), args[1]);
      builder()->CallRuntime(Runtime::kInternalSetPrototype, args);
      continue;
    }

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::COMPUTED:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL: {
        Register key = register_allocator()->NewRegister();
        BuildLoadPropertyKey(property, key);
        builder()->SetExpressionPosition(property->value());
        Register value = VisitForRegisterValue(property->value());
        VisitSetHomeObject(value, literal, property);

        DataPropertyInLiteralFlags data_property_flags =
            DataPropertyInLiteralFlag::kNoFlags;
        if (property->NeedsSetFunctionName()) {
          data_property_flags |= DataPropertyInLiteralFlag::kSetFunctionName;
        }

        FeedbackSlot slot =
            feedback_spec()->AddStoreDataPropertyInLiteralICSlot();
        builder()
            ->LoadAccumulatorWithRegister(value)
            .StoreDataPropertyInLiteral(literal, key, data_property_flags,
                                        feedback_index(slot));
        break;
      }
      case ObjectLiteral::Property::GETTER:
      case ObjectLiteral::Property::SETTER: {
        RegisterList args = register_allocator()->NewRegisterList(4);
        builder()->MoveRegister(literal, args[0]);
        BuildLoadPropertyKey(property, args[1]);
        builder()->SetExpressionPosition(property->value());
        VisitForRegisterValue(property->value(), args[2]);
        VisitSetHomeObject(args[2], literal, property);
        builder()
            ->LoadLiteral(Smi::FromInt(NONE))
            .StoreAccumulatorInRegister(args[3]);
        Runtime::FunctionId function_id =
            property->kind() == ObjectLiteral::Property::GETTER
                ? Runtime::kDefineGetterPropertyUnchecked
                : Runtime::kDefineSetterPropertyUnchecked;
        builder()->CallRuntime(function_id, args);
        break;
      }
      case ObjectLiteral::Property::SPREAD: {
        RegisterList args = register_allocator()->NewRegisterList(2);
        builder()->MoveRegister(literal, args[0]);
        builder()->SetExpressionPosition(property->value());
        VisitForRegisterValue(property->value(), args[1]);
        builder()->CallRuntime(Runtime::kCopyDataProperties, args);
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE:
        UNREACHABLE();  // Handled specially above.
        break;
    }
  }

  builder()->LoadAccumulatorWithRegister(literal);
}

void BytecodeGenerator::BuildArrayLiteralSpread(Spread* spread, Register array,
                                                Register index,
                                                FeedbackSlot index_slot,
                                                FeedbackSlot element_slot) {
  RegisterAllocationScope register_scope(this);
  Register value = register_allocator()->NewRegister();

  builder()->SetExpressionAsStatementPosition(spread->expression());
  IteratorRecord iterator =
      BuildGetIteratorRecord(spread->expression(), IteratorType::kNormal);

  LoopBuilder loop_builder(builder(), nullptr, nullptr);
  loop_builder.LoopHeader();

  // Call the iterator's .next() method. Break from the loop if the `done`
  // property is truthy, otherwise load the value from the iterator result and
  // append the argument.
  BuildIteratorNext(iterator, value);
  builder()->LoadNamedProperty(
      value, ast_string_constants()->done_string(),
      feedback_index(feedback_spec()->AddLoadICSlot()));
  loop_builder.BreakIfTrue(ToBooleanMode::kConvertToBoolean);

  loop_builder.LoopBody();
  builder()
      // value = value.value
      ->LoadNamedProperty(value, ast_string_constants()->value_string(),
                          feedback_index(feedback_spec()->AddLoadICSlot()))
      .StoreAccumulatorInRegister(value)
      // array[index] = value
      .StoreInArrayLiteral(array, index, feedback_index(element_slot))
      // index++
      .LoadAccumulatorWithRegister(index)
      .UnaryOperation(Token::INC, feedback_index(index_slot))
      .StoreAccumulatorInRegister(index);
  loop_builder.BindContinueTarget();
  loop_builder.JumpToHeader(loop_depth_);
}

void BytecodeGenerator::BuildCreateArrayLiteral(
    ZonePtrList<Expression>* elements, ArrayLiteral* expr) {
  RegisterAllocationScope register_scope(this);
  Register index = register_allocator()->NewRegister();
  Register array = register_allocator()->NewRegister();
  SharedFeedbackSlot element_slot(feedback_spec(),
                                  FeedbackSlotKind::kStoreInArrayLiteral);
  ZonePtrList<Expression>::iterator current = elements->begin();
  ZonePtrList<Expression>::iterator end = elements->end();
  bool is_empty = elements->is_empty();

  if (!is_empty && (*current)->IsSpread()) {
    // If we have a leading spread, use CreateArrayFromIterable to create
    // an array from it and then add the remaining components to that array.
    VisitForAccumulatorValue(*current);
    builder()->CreateArrayFromIterable().StoreAccumulatorInRegister(array);

    if (++current != end) {
      // If there are remaning elements, prepare the index register that is
      // used for adding those elements. The next index is the length of the
      // newly created array.
      auto length = ast_string_constants()->length_string();
      int length_load_slot = feedback_index(feedback_spec()->AddLoadICSlot());
      builder()
          ->LoadNamedProperty(array, length, length_load_slot)
          .StoreAccumulatorInRegister(index);
    }
  } else if (expr != nullptr) {
    // There are some elements before the first (if any) spread, and we can
    // use a boilerplate when creating the initial array from those elements.

    // First, allocate a constant pool entry for the boilerplate that will
    // be created during finalization, and will contain all the constant
    // elements before the first spread. This also handle the empty array case
    // and one-shot optimization.
    uint8_t flags = CreateArrayLiteralFlags::Encode(
        expr->IsFastCloningSupported(), expr->ComputeFlags());
    bool optimize_as_one_shot = ShouldOptimizeAsOneShot();
    size_t entry;
    if (is_empty && optimize_as_one_shot) {
      entry = builder()->EmptyArrayBoilerplateDescriptionConstantPoolEntry();
    } else if (!is_empty) {
      entry = builder()->AllocateDeferredConstantPoolEntry();
      array_literals_.push_back(std::make_pair(expr, entry));
    }

    if (optimize_as_one_shot) {
      RegisterList args = register_allocator()->NewRegisterList(2);
      builder()
          ->LoadConstantPoolEntry(entry)
          .StoreAccumulatorInRegister(args[0])
          .LoadLiteral(Smi::FromInt(flags))
          .StoreAccumulatorInRegister(args[1])
          .CallRuntime(Runtime::kCreateArrayLiteralWithoutAllocationSite, args);
    } else if (is_empty) {
      // Empty array literal fast-path.
      int literal_index = feedback_index(feedback_spec()->AddLiteralSlot());
      DCHECK(expr->IsFastCloningSupported());
      builder()->CreateEmptyArrayLiteral(literal_index);
    } else {
      // Create array literal from boilerplate.
      int literal_index = feedback_index(feedback_spec()->AddLiteralSlot());
      builder()->CreateArrayLiteral(entry, literal_index, flags);
    }
    builder()->StoreAccumulatorInRegister(array);

    // Insert the missing non-constant elements, up until the first spread
    // index, into the initial array (the remaining elements will be inserted
    // below).
    DCHECK_EQ(current, elements->begin());
    ZonePtrList<Expression>::iterator first_spread_or_end =
        expr->first_spread_index() >= 0 ? current + expr->first_spread_index()
                                        : end;
    int array_index = 0;
    for (; current != first_spread_or_end; ++current, array_index++) {
      Expression* subexpr = *current;
      DCHECK(!subexpr->IsSpread());
      // Skip the constants.
      if (subexpr->IsCompileTimeValue()) continue;

      builder()
          ->LoadLiteral(Smi::FromInt(array_index))
          .StoreAccumulatorInRegister(index);
      VisitForAccumulatorValue(subexpr);
      builder()->StoreInArrayLiteral(array, index,
                                     feedback_index(element_slot.Get()));
    }

    if (current != end) {
      // If there are remaining elements, prepare the index register
      // to store the next element, which comes from the first spread.
      builder()->LoadLiteral(array_index).StoreAccumulatorInRegister(index);
    }
  } else {
    // In other cases, we prepare an empty array to be filled in below.
    DCHECK(!elements->is_empty());
    int literal_index = feedback_index(feedback_spec()->AddLiteralSlot());
    builder()
        ->CreateEmptyArrayLiteral(literal_index)
        .StoreAccumulatorInRegister(array);
    // Prepare the index for the first element.
    builder()->LoadLiteral(Smi::FromInt(0)).StoreAccumulatorInRegister(index);
  }

  // Now build insertions for the remaining elements from current to end.
  SharedFeedbackSlot index_slot(feedback_spec(), FeedbackSlotKind::kBinaryOp);
  SharedFeedbackSlot length_slot(
      feedback_spec(), feedback_spec()->GetStoreICSlot(LanguageMode::kStrict));
  for (; current != end; ++current) {
    Expression* subexpr = *current;
    if (subexpr->IsSpread()) {
      FeedbackSlot real_index_slot = index_slot.Get();
      BuildArrayLiteralSpread(subexpr->AsSpread(), array, index,
                              real_index_slot, element_slot.Get());
    } else if (!subexpr->IsTheHoleLiteral()) {
      // literal[index++] = subexpr
      VisitForAccumulatorValue(subexpr);
      builder()
          ->StoreInArrayLiteral(array, index,
                                feedback_index(element_slot.Get()))
          .LoadAccumulatorWithRegister(index);
      // Only increase the index if we are not the last element.
      if (current + 1 != end) {
        builder()
            ->UnaryOperation(Token::INC, feedback_index(index_slot.Get()))
            .StoreAccumulatorInRegister(index);
      }
    } else {
      // literal.length = ++index
      // length_slot is only used when there are holes.
      auto length = ast_string_constants()->length_string();
      builder()
          ->LoadAccumulatorWithRegister(index)
          .UnaryOperation(Token::INC, feedback_index(index_slot.Get()))
          .StoreAccumulatorInRegister(index)
          .StoreNamedProperty(array, length, feedback_index(length_slot.Get()),
                              LanguageMode::kStrict);
    }
  }

  builder()->LoadAccumulatorWithRegister(array);
}

void BytecodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  expr->InitDepthAndFlags();
  BuildCreateArrayLiteral(expr->values(), expr);
}

void BytecodeGenerator::VisitStoreInArrayLiteral(StoreInArrayLiteral* expr) {
  builder()->SetExpressionAsStatementPosition(expr);
  RegisterAllocationScope register_scope(this);
  Register array = register_allocator()->NewRegister();
  Register index = register_allocator()->NewRegister();
  VisitForRegisterValue(expr->array(), array);
  VisitForRegisterValue(expr->index(), index);
  VisitForAccumulatorValue(expr->value());
  builder()->StoreInArrayLiteral(
      array, index,
      feedback_index(feedback_spec()->AddStoreInArrayLiteralICSlot()));
}

void BytecodeGenerator::VisitVariableProxy(VariableProxy* proxy) {
  builder()->SetExpressionPosition(proxy);
  BuildVariableLoad(proxy->var(), proxy->hole_check_mode());
}

void BytecodeGenerator::BuildVariableLoad(Variable* variable,
                                          HoleCheckMode hole_check_mode,
                                          TypeofMode typeof_mode) {
  switch (variable->location()) {
    case VariableLocation::LOCAL: {
      Register source(builder()->Local(variable->index()));
      // We need to load the variable into the accumulator, even when in a
      // VisitForRegisterScope, in order to avoid register aliasing if
      // subsequent expressions assign to the same variable.
      builder()->LoadAccumulatorWithRegister(source);
      if (hole_check_mode == HoleCheckMode::kRequired) {
        BuildThrowIfHole(variable);
      }
      break;
    }
    case VariableLocation::PARAMETER: {
      Register source;
      if (variable->IsReceiver()) {
        source = builder()->Receiver();
      } else {
        source = builder()->Parameter(variable->index());
      }
      // We need to load the variable into the accumulator, even when in a
      // VisitForRegisterScope, in order to avoid register aliasing if
      // subsequent expressions assign to the same variable.
      builder()->LoadAccumulatorWithRegister(source);
      if (hole_check_mode == HoleCheckMode::kRequired) {
        BuildThrowIfHole(variable);
      }
      break;
    }
    case VariableLocation::UNALLOCATED: {
      // The global identifier "undefined" is immutable. Everything
      // else could be reassigned. For performance, we do a pointer comparison
      // rather than checking if the raw_name is really "undefined".
      if (variable->raw_name() == ast_string_constants()->undefined_string()) {
        builder()->LoadUndefined();
      } else {
        FeedbackSlot slot = GetCachedLoadGlobalICSlot(typeof_mode, variable);
        builder()->LoadGlobal(variable->raw_name(), feedback_index(slot),
                              typeof_mode);
      }
      break;
    }
    case VariableLocation::CONTEXT: {
      int depth = execution_context()->ContextChainDepth(variable->scope());
      ContextScope* context = execution_context()->Previous(depth);
      Register context_reg;
      if (context) {
        context_reg = context->reg();
        depth = 0;
      } else {
        context_reg = execution_context()->reg();
      }

      BytecodeArrayBuilder::ContextSlotMutability immutable =
          (variable->maybe_assigned() == kNotAssigned)
              ? BytecodeArrayBuilder::kImmutableSlot
              : BytecodeArrayBuilder::kMutableSlot;

      builder()->LoadContextSlot(context_reg, variable->index(), depth,
                                 immutable);
      if (hole_check_mode == HoleCheckMode::kRequired) {
        BuildThrowIfHole(variable);
      }
      break;
    }
    case VariableLocation::LOOKUP: {
      switch (variable->mode()) {
        case VariableMode::kDynamicLocal: {
          Variable* local_variable = variable->local_if_not_shadowed();
          int depth =
              execution_context()->ContextChainDepth(local_variable->scope());
          builder()->LoadLookupContextSlot(variable->raw_name(), typeof_mode,
                                           local_variable->index(), depth);
          if (hole_check_mode == HoleCheckMode::kRequired) {
            BuildThrowIfHole(variable);
          }
          break;
        }
        case VariableMode::kDynamicGlobal: {
          int depth =
              current_scope()->ContextChainLengthUntilOutermostSloppyEval();
          FeedbackSlot slot = GetCachedLoadGlobalICSlot(typeof_mode, variable);
          builder()->LoadLookupGlobalSlot(variable->raw_name(), typeof_mode,
                                          feedback_index(slot), depth);
          break;
        }
        default:
          builder()->LoadLookupSlot(variable->raw_name(), typeof_mode);
      }
      break;
    }
    case VariableLocation::MODULE: {
      int depth = execution_context()->ContextChainDepth(variable->scope());
      builder()->LoadModuleVariable(variable->index(), depth);
      if (hole_check_mode == HoleCheckMode::kRequired) {
        BuildThrowIfHole(variable);
      }
      break;
    }
  }
}

void BytecodeGenerator::BuildVariableLoadForAccumulatorValue(
    Variable* variable, HoleCheckMode hole_check_mode, TypeofMode typeof_mode) {
  ValueResultScope accumulator_result(this);
  BuildVariableLoad(variable, hole_check_mode, typeof_mode);
}

void BytecodeGenerator::BuildReturn(int source_position) {
  if (FLAG_trace) {
    RegisterAllocationScope register_scope(this);
    Register result = register_allocator()->NewRegister();
    // Runtime returns {result} value, preserving accumulator.
    builder()->StoreAccumulatorInRegister(result).CallRuntime(
        Runtime::kTraceExit, result);
  }
  if (info()->collect_type_profile()) {
    builder()->CollectTypeProfile(info()->literal()->return_position());
  }
  builder()->SetReturnPosition(source_position, info()->literal());
  builder()->Return();
}

void BytecodeGenerator::BuildAsyncReturn(int source_position) {
  RegisterAllocationScope register_scope(this);

  if (IsAsyncGeneratorFunction(info()->literal()->kind())) {
    RegisterList args = register_allocator()->NewRegisterList(3);
    builder()
        ->MoveRegister(generator_object(), args[0])  // generator
        .StoreAccumulatorInRegister(args[1])         // value
        .LoadTrue()
        .StoreAccumulatorInRegister(args[2])  // done
        .CallRuntime(Runtime::kInlineAsyncGeneratorResolve, args);
  } else {
    DCHECK(IsAsyncFunction(info()->literal()->kind()));
    RegisterList args = register_allocator()->NewRegisterList(2);
    Register promise = args[0];
    Register return_value = args[1];
    builder()->StoreAccumulatorInRegister(return_value);

    Variable* var_promise = closure_scope()->promise_var();
    DCHECK_NOT_NULL(var_promise);
    BuildVariableLoad(var_promise, HoleCheckMode::kElided);
    builder()
        ->StoreAccumulatorInRegister(promise)
        .CallRuntime(Runtime::kInlineResolvePromise, args)
        .LoadAccumulatorWithRegister(promise);
  }

  BuildReturn(source_position);
}

void BytecodeGenerator::BuildReThrow() { builder()->ReThrow(); }

void BytecodeGenerator::BuildThrowIfHole(Variable* variable) {
  if (variable->is_this()) {
    DCHECK(variable->mode() == VariableMode::kConst);
    builder()->ThrowSuperNotCalledIfHole();
  } else {
    builder()->ThrowReferenceErrorIfHole(variable->raw_name());
  }
}

void BytecodeGenerator::BuildHoleCheckForVariableAssignment(Variable* variable,
                                                            Token::Value op) {
  if (variable->is_this() && variable->mode() == VariableMode::kConst &&
      op == Token::INIT) {
    // Perform an initialization check for 'this'. 'this' variable is the
    // only variable able to trigger bind operations outside the TDZ
    // via 'super' calls.
    builder()->ThrowSuperAlreadyCalledIfNotHole();
  } else {
    // Perform an initialization check for let/const declared variables.
    // E.g. let x = (x = 20); is not allowed.
    DCHECK(IsLexicalVariableMode(variable->mode()));
    BuildThrowIfHole(variable);
  }
}

void BytecodeGenerator::BuildVariableAssignment(
    Variable* variable, Token::Value op, HoleCheckMode hole_check_mode,
    LookupHoistingMode lookup_hoisting_mode) {
  VariableMode mode = variable->mode();
  RegisterAllocationScope assignment_register_scope(this);
  BytecodeLabel end_label;
  switch (variable->location()) {
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      Register destination;
      if (VariableLocation::PARAMETER == variable->location()) {
        if (variable->IsReceiver()) {
          destination = builder()->Receiver();
        } else {
          destination = builder()->Parameter(variable->index());
        }
      } else {
        destination = builder()->Local(variable->index());
      }

      if (hole_check_mode == HoleCheckMode::kRequired) {
        // Load destination to check for hole.
        Register value_temp = register_allocator()->NewRegister();
        builder()
            ->StoreAccumulatorInRegister(value_temp)
            .LoadAccumulatorWithRegister(destination);

        BuildHoleCheckForVariableAssignment(variable, op);
        builder()->LoadAccumulatorWithRegister(value_temp);
      }

      if (mode != VariableMode::kConst || op == Token::INIT) {
        builder()->StoreAccumulatorInRegister(destination);
      } else if (variable->throw_on_const_assignment(language_mode())) {
        builder()->CallRuntime(Runtime::kThrowConstAssignError);
      }
      break;
    }
    case VariableLocation::UNALLOCATED: {
      FeedbackSlot slot = GetCachedStoreGlobalICSlot(language_mode(), variable);
      builder()->StoreGlobal(variable->raw_name(), feedback_index(slot));
      break;
    }
    case VariableLocation::CONTEXT: {
      int depth = execution_context()->ContextChainDepth(variable->scope());
      ContextScope* context = execution_context()->Previous(depth);
      Register context_reg;

      if (context) {
        context_reg = context->reg();
        depth = 0;
      } else {
        context_reg = execution_context()->reg();
      }

      if (hole_check_mode == HoleCheckMode::kRequired) {
        // Load destination to check for hole.
        Register value_temp = register_allocator()->NewRegister();
        builder()
            ->StoreAccumulatorInRegister(value_temp)
            .LoadContextSlot(context_reg, variable->index(), depth,
                             BytecodeArrayBuilder::kMutableSlot);

        BuildHoleCheckForVariableAssignment(variable, op);
        builder()->LoadAccumulatorWithRegister(value_temp);
      }

      if (mode != VariableMode::kConst || op == Token::INIT) {
        builder()->StoreContextSlot(context_reg, variable->index(), depth);
      } else if (variable->throw_on_const_assignment(language_mode())) {
        builder()->CallRuntime(Runtime::kThrowConstAssignError);
      }
      break;
    }
    case VariableLocation::LOOKUP: {
      builder()->StoreLookupSlot(variable->raw_name(), language_mode(),
                                 lookup_hoisting_mode);
      break;
    }
    case VariableLocation::MODULE: {
      DCHECK(IsDeclaredVariableMode(mode));

      if (mode == VariableMode::kConst && op != Token::INIT) {
        builder()->CallRuntime(Runtime::kThrowConstAssignError);
        break;
      }

      // If we don't throw above, we know that we're dealing with an
      // export because imports are const and we do not generate initializing
      // assignments for them.
      DCHECK(variable->IsExport());

      int depth = execution_context()->ContextChainDepth(variable->scope());
      if (hole_check_mode == HoleCheckMode::kRequired) {
        Register value_temp = register_allocator()->NewRegister();
        builder()
            ->StoreAccumulatorInRegister(value_temp)
            .LoadModuleVariable(variable->index(), depth);
        BuildHoleCheckForVariableAssignment(variable, op);
        builder()->LoadAccumulatorWithRegister(value_temp);
      }
      builder()->StoreModuleVariable(variable->index(), depth);
      break;
    }
  }
}

void BytecodeGenerator::BuildLoadNamedProperty(Property* property,
                                               Register object,
                                               const AstRawString* name) {
  if (ShouldOptimizeAsOneShot()) {
    builder()->LoadNamedPropertyNoFeedback(object, name);
  } else {
    FeedbackSlot slot = GetCachedLoadICSlot(property->obj(), name);
    builder()->LoadNamedProperty(object, name, feedback_index(slot));
  }
}

void BytecodeGenerator::BuildStoreNamedProperty(Property* property,
                                                Register object,
                                                const AstRawString* name) {
  Register value;
  if (!execution_result()->IsEffect()) {
    value = register_allocator()->NewRegister();
    builder()->StoreAccumulatorInRegister(value);
  }

  if (ShouldOptimizeAsOneShot()) {
    builder()->StoreNamedPropertyNoFeedback(object, name, language_mode());
  } else {
    FeedbackSlot slot = GetCachedStoreICSlot(property->obj(), name);
    builder()->StoreNamedProperty(object, name, feedback_index(slot),
                                  language_mode());
  }

  if (!execution_result()->IsEffect()) {
    builder()->LoadAccumulatorWithRegister(value);
  }
}

void BytecodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpression() ||
         (expr->op() == Token::INIT && expr->target()->IsVariableProxy() &&
          expr->target()->AsVariableProxy()->is_this()));
  Register object, key;
  RegisterList super_property_args;
  const AstRawString* name;

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
      name = property->key()->AsLiteral()->AsRawPropertyName();
      break;
    }
    case KEYED_PROPERTY: {
      object = VisitForRegisterValue(property->obj());
      key = VisitForRegisterValue(property->key());
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      super_property_args = register_allocator()->NewRegisterList(4);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), super_property_args[0]);
      VisitForRegisterValue(super_property->home_object(),
                            super_property_args[1]);
      builder()
          ->LoadLiteral(property->key()->AsLiteral()->AsRawPropertyName())
          .StoreAccumulatorInRegister(super_property_args[2]);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      super_property_args = register_allocator()->NewRegisterList(4);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), super_property_args[0]);
      VisitForRegisterValue(super_property->home_object(),
                            super_property_args[1]);
      VisitForRegisterValue(property->key(), super_property_args[2]);
      break;
    }
  }

  // Evaluate the value and potentially handle compound assignments by loading
  // the left-hand side value and performing a binary operation.
  if (expr->IsCompoundAssignment()) {
    switch (assign_type) {
      case VARIABLE: {
        VariableProxy* proxy = expr->target()->AsVariableProxy();
        BuildVariableLoad(proxy->var(), proxy->hole_check_mode());
        break;
      }
      case NAMED_PROPERTY: {
        BuildLoadNamedProperty(property, object, name);
        break;
      }
      case KEYED_PROPERTY: {
        // Key is already in accumulator at this point due to evaluating the
        // LHS above.
        FeedbackSlot slot = feedback_spec()->AddKeyedLoadICSlot();
        builder()->LoadKeyedProperty(object, feedback_index(slot));
        break;
      }
      case NAMED_SUPER_PROPERTY: {
        builder()->CallRuntime(Runtime::kLoadFromSuper,
                               super_property_args.Truncate(3));
        break;
      }
      case KEYED_SUPER_PROPERTY: {
        builder()->CallRuntime(Runtime::kLoadKeyedFromSuper,
                               super_property_args.Truncate(3));
        break;
      }
    }
    BinaryOperation* binop = expr->AsCompoundAssignment()->binary_operation();
    FeedbackSlot slot = feedback_spec()->AddBinaryOpICSlot();
    if (expr->value()->IsSmiLiteral()) {
      builder()->BinaryOperationSmiLiteral(
          binop->op(), expr->value()->AsLiteral()->AsSmiLiteral(),
          feedback_index(slot));
    } else {
      Register old_value = register_allocator()->NewRegister();
      builder()->StoreAccumulatorInRegister(old_value);
      VisitForAccumulatorValue(expr->value());
      builder()->BinaryOperation(binop->op(), old_value, feedback_index(slot));
    }
  } else {
    VisitForAccumulatorValue(expr->value());
  }

  // Store the value.
  builder()->SetExpressionPosition(expr);
  switch (assign_type) {
    case VARIABLE: {
      // TODO(oth): The BuildVariableAssignment() call is hard to reason about.
      // Is the value in the accumulator safe? Yes, but scary.
      VariableProxy* proxy = expr->target()->AsVariableProxy();
      BuildVariableAssignment(proxy->var(), expr->op(),
                              proxy->hole_check_mode(),
                              expr->lookup_hoisting_mode());
      break;
    }
    case NAMED_PROPERTY: {
      BuildStoreNamedProperty(property, object, name);
      break;
    }
    case KEYED_PROPERTY: {
      FeedbackSlot slot = feedback_spec()->AddKeyedStoreICSlot(language_mode());
      Register value;
      if (!execution_result()->IsEffect()) {
        value = register_allocator()->NewRegister();
        builder()->StoreAccumulatorInRegister(value);
      }
      builder()->StoreKeyedProperty(object, key, feedback_index(slot),
                                    language_mode());
      if (!execution_result()->IsEffect()) {
        builder()->LoadAccumulatorWithRegister(value);
      }
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      builder()
          ->StoreAccumulatorInRegister(super_property_args[3])
          .CallRuntime(StoreToSuperRuntimeId(), super_property_args);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      builder()
          ->StoreAccumulatorInRegister(super_property_args[3])
          .CallRuntime(StoreKeyedToSuperRuntimeId(), super_property_args);
      break;
    }
  }
}

void BytecodeGenerator::VisitCompoundAssignment(CompoundAssignment* expr) {
  VisitAssignment(expr);
}

// Suspends the generator to resume at the next suspend_id, with output stored
// in the accumulator. When the generator is resumed, the sent value is loaded
// in the accumulator.
void BytecodeGenerator::BuildSuspendPoint(Expression* suspend_expr) {
  const int suspend_id = suspend_count_++;

  RegisterList registers = register_allocator()->AllLiveRegisters();

  // Save context, registers, and state. This bytecode then returns the value
  // in the accumulator.
  builder()->SetExpressionPosition(suspend_expr);
  builder()->SuspendGenerator(generator_object(), registers, suspend_id);

  // Upon resume, we continue here.
  builder()->Bind(generator_jump_table_, suspend_id);

  // Clobbers all registers and sets the accumulator to the
  // [[input_or_debug_pos]] slot of the generator object.
  builder()->ResumeGenerator(generator_object(), registers);
}

void BytecodeGenerator::VisitYield(Yield* expr) {
  builder()->SetExpressionPosition(expr);
  VisitForAccumulatorValue(expr->expression());

  // If this is not the first yield
  if (suspend_count_ > 0) {
    if (IsAsyncGeneratorFunction(function_kind())) {
      // AsyncGenerator yields (with the exception of the initial yield)
      // delegate work to the AsyncGeneratorYield stub, which Awaits the operand
      // and on success, wraps the value in an IteratorResult.
      RegisterAllocationScope register_scope(this);
      RegisterList args = register_allocator()->NewRegisterList(3);
      builder()
          ->MoveRegister(generator_object(), args[0])  // generator
          .StoreAccumulatorInRegister(args[1])         // value
          .LoadBoolean(catch_prediction() != HandlerTable::ASYNC_AWAIT)
          .StoreAccumulatorInRegister(args[2])  // is_caught
          .CallRuntime(Runtime::kInlineAsyncGeneratorYield, args);
    } else {
      // Generator yields (with the exception of the initial yield) wrap the
      // value into IteratorResult.
      RegisterAllocationScope register_scope(this);
      RegisterList args = register_allocator()->NewRegisterList(2);
      builder()
          ->StoreAccumulatorInRegister(args[0])  // value
          .LoadFalse()
          .StoreAccumulatorInRegister(args[1])   // done
          .CallRuntime(Runtime::kInlineCreateIterResultObject, args);
    }
  }

  BuildSuspendPoint(expr);
  // At this point, the generator has been resumed, with the received value in
  // the accumulator.

  // TODO(caitp): remove once yield* desugaring for async generators is handled
  // in BytecodeGenerator.
  if (expr->on_abrupt_resume() == Yield::kNoControl) {
    DCHECK(IsAsyncGeneratorFunction(function_kind()));
    return;
  }

  Register input = register_allocator()->NewRegister();
  builder()->StoreAccumulatorInRegister(input).CallRuntime(
      Runtime::kInlineGeneratorGetResumeMode, generator_object());

  // Now dispatch on resume mode.
  STATIC_ASSERT(JSGeneratorObject::kNext + 1 == JSGeneratorObject::kReturn);
  BytecodeJumpTable* jump_table =
      builder()->AllocateJumpTable(2, JSGeneratorObject::kNext);

  builder()->SwitchOnSmiNoFeedback(jump_table);

  {
    // Resume with throw (switch fallthrough).
    // TODO(leszeks): Add a debug-only check that the accumulator is
    // JSGeneratorObject::kThrow.
    builder()->SetExpressionPosition(expr);
    builder()->LoadAccumulatorWithRegister(input);
    builder()->Throw();
  }

  {
    // Resume with return.
    builder()->Bind(jump_table, JSGeneratorObject::kReturn);
    builder()->LoadAccumulatorWithRegister(input);
    if (IsAsyncGeneratorFunction(function_kind())) {
      execution_control()->AsyncReturnAccumulator();
    } else {
      execution_control()->ReturnAccumulator();
    }
  }

  {
    // Resume with next.
    builder()->Bind(jump_table, JSGeneratorObject::kNext);
    BuildIncrementBlockCoverageCounterIfEnabled(expr,
                                                SourceRangeKind::kContinuation);
    builder()->LoadAccumulatorWithRegister(input);
  }
}

// Desugaring of (yield* iterable)
//
//   do {
//     const kNext = 0;
//     const kReturn = 1;
//     const kThrow = 2;
//
//     let output; // uninitialized
//
//     let iteratorRecord = GetIterator(iterable);
//     let iterator = iteratorRecord.[[Iterator]];
//     let next = iteratorRecord.[[NextMethod]];
//     let input = undefined;
//     let resumeMode = kNext;
//
//     while (true) {
//       // From the generator to the iterator:
//       // Forward input according to resumeMode and obtain output.
//       switch (resumeMode) {
//         case kNext:
//           output = next.[[Call]](iterator, « »);;
//           break;
//         case kReturn:
//           let iteratorReturn = iterator.return;
//           if (IS_NULL_OR_UNDEFINED(iteratorReturn)) return input;
//           output = iteratorReturn.[[Call]](iterator, «input»);
//           break;
//         case kThrow:
//           let iteratorThrow = iterator.throw;
//           if (IS_NULL_OR_UNDEFINED(iteratorThrow)) {
//             let iteratorReturn = iterator.return;
//             if (!IS_NULL_OR_UNDEFINED(iteratorReturn)) {
//               output = iteratorReturn.[[Call]](iterator, « »);
//               if (IS_ASYNC_GENERATOR) output = await output;
//               if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
//             }
//             throw MakeTypeError(kThrowMethodMissing);
//           }
//           output = iteratorThrow.[[Call]](iterator, «input»);
//           break;
//       }
//
//       if (IS_ASYNC_GENERATOR) output = await output;
//       if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
//       if (output.done) break;
//
//       // From the generator to its user:
//       // Forward output, receive new input, and determine resume mode.
//       if (IS_ASYNC_GENERATOR) {
//         // AsyncGeneratorYield abstract operation awaits the operand before
//         // resolving the promise for the current AsyncGeneratorRequest.
//         %_AsyncGeneratorYield(output.value)
//       }
//       input = Suspend(output);
//       resumeMode = %GeneratorGetResumeMode();
//     }
//
//     if (resumeMode === kReturn) {
//       return output.value;
//     }
//     output.value
//   }
void BytecodeGenerator::VisitYieldStar(YieldStar* expr) {
  Register output = register_allocator()->NewRegister();
  Register resume_mode = register_allocator()->NewRegister();
  IteratorType iterator_type = IsAsyncGeneratorFunction(function_kind())
                                   ? IteratorType::kAsync
                                   : IteratorType::kNormal;

  {
    RegisterAllocationScope register_scope(this);
    RegisterList iterator_and_input = register_allocator()->NewRegisterList(2);
    IteratorRecord iterator = BuildGetIteratorRecord(
        expr->expression(),
        register_allocator()->NewRegister() /* next method */,
        iterator_and_input[0], iterator_type);

    Register input = iterator_and_input[1];
    builder()->LoadUndefined().StoreAccumulatorInRegister(input);
    builder()
        ->LoadLiteral(Smi::FromInt(JSGeneratorObject::kNext))
        .StoreAccumulatorInRegister(resume_mode);

    {
      // This loop builder does not construct counters as the loop is not
      // visible to the user, and we therefore neither pass the block coverage
      // builder nor the expression.
      //
      // In addition to the normal suspend for yield*, a yield* in an async
      // generator has 2 additional suspends:
      //   - One for awaiting the iterator result of closing the generator when
      //     resumed with a "throw" completion, and a throw method is not
      //     present on the delegated iterator
      //   - One for awaiting the iterator result yielded by the delegated
      //     iterator

      LoopBuilder loop(builder(), nullptr, nullptr);
      loop.LoopHeader();

      {
        BytecodeLabels after_switch(zone());
        BytecodeJumpTable* switch_jump_table =
            builder()->AllocateJumpTable(2, 1);

        builder()
            ->LoadAccumulatorWithRegister(resume_mode)
            .SwitchOnSmiNoFeedback(switch_jump_table);

        // Fallthrough to default case.
        // TODO(tebbi): Add debug code to check that {resume_mode} really is
        // {JSGeneratorObject::kNext} in this case.
        STATIC_ASSERT(JSGeneratorObject::kNext == 0);
        {
          FeedbackSlot slot = feedback_spec()->AddCallICSlot();
          builder()->CallProperty(iterator.next(), iterator_and_input,
                                  feedback_index(slot));
          builder()->Jump(after_switch.New());
        }

        STATIC_ASSERT(JSGeneratorObject::kReturn == 1);
        builder()->Bind(switch_jump_table, JSGeneratorObject::kReturn);
        {
          const AstRawString* return_string =
              ast_string_constants()->return_string();
          BytecodeLabels no_return_method(zone());

          BuildCallIteratorMethod(iterator.object(), return_string,
                                  iterator_and_input, after_switch.New(),
                                  &no_return_method);
          no_return_method.Bind(builder());
          builder()->LoadAccumulatorWithRegister(input);
          if (iterator_type == IteratorType::kAsync) {
            execution_control()->AsyncReturnAccumulator();
          } else {
            execution_control()->ReturnAccumulator();
          }
        }

        STATIC_ASSERT(JSGeneratorObject::kThrow == 2);
        builder()->Bind(switch_jump_table, JSGeneratorObject::kThrow);
        {
          const AstRawString* throw_string =
              ast_string_constants()->throw_string();
          BytecodeLabels no_throw_method(zone());
          BuildCallIteratorMethod(iterator.object(), throw_string,
                                  iterator_and_input, after_switch.New(),
                                  &no_throw_method);

          // If there is no "throw" method, perform IteratorClose, and finally
          // throw a TypeError.
          no_throw_method.Bind(builder());
          BuildIteratorClose(iterator, expr);
          builder()->CallRuntime(Runtime::kThrowThrowMethodMissing);
        }

        after_switch.Bind(builder());
      }

      if (iterator_type == IteratorType::kAsync) {
        // Await the result of the method invocation.
        BuildAwait(expr);
      }

      // Check that output is an object.
      BytecodeLabel check_if_done;
      builder()
          ->StoreAccumulatorInRegister(output)
          .JumpIfJSReceiver(&check_if_done)
          .CallRuntime(Runtime::kThrowIteratorResultNotAnObject, output);

      builder()->Bind(&check_if_done);
      // Break once output.done is true.
      builder()->LoadNamedProperty(
          output, ast_string_constants()->done_string(),
          feedback_index(feedback_spec()->AddLoadICSlot()));

      loop.BreakIfTrue(ToBooleanMode::kConvertToBoolean);

      // Suspend the current generator.
      if (iterator_type == IteratorType::kNormal) {
        builder()->LoadAccumulatorWithRegister(output);
      } else {
        RegisterAllocationScope register_scope(this);
        DCHECK_EQ(iterator_type, IteratorType::kAsync);
        // If generatorKind is async, perform AsyncGeneratorYield(output.value),
        // which will await `output.value` before resolving the current
        // AsyncGeneratorRequest's promise.
        builder()->LoadNamedProperty(
            output, ast_string_constants()->value_string(),
            feedback_index(feedback_spec()->AddLoadICSlot()));

        RegisterList args = register_allocator()->NewRegisterList(3);
        builder()
            ->MoveRegister(generator_object(), args[0])  // generator
            .StoreAccumulatorInRegister(args[1])         // value
            .LoadBoolean(catch_prediction() != HandlerTable::ASYNC_AWAIT)
            .StoreAccumulatorInRegister(args[2])  // is_caught
            .CallRuntime(Runtime::kInlineAsyncGeneratorYield, args);
      }

      BuildSuspendPoint(expr);
      builder()->StoreAccumulatorInRegister(input);
      builder()
          ->CallRuntime(Runtime::kInlineGeneratorGetResumeMode,
                        generator_object())
          .StoreAccumulatorInRegister(resume_mode);

      loop.BindContinueTarget();
      loop.JumpToHeader(loop_depth_);
    }
  }

  // Decide if we trigger a return or if the yield* expression should just
  // produce a value.
  BytecodeLabel completion_is_output_value;
  Register output_value = register_allocator()->NewRegister();
  builder()
      ->LoadNamedProperty(output, ast_string_constants()->value_string(),
                          feedback_index(feedback_spec()->AddLoadICSlot()))
      .StoreAccumulatorInRegister(output_value)
      .LoadLiteral(Smi::FromInt(JSGeneratorObject::kReturn))
      .CompareReference(resume_mode)
      .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &completion_is_output_value)
      .LoadAccumulatorWithRegister(output_value);
  if (iterator_type == IteratorType::kAsync) {
    execution_control()->AsyncReturnAccumulator();
  } else {
    execution_control()->ReturnAccumulator();
  }

  builder()->Bind(&completion_is_output_value);
  BuildIncrementBlockCoverageCounterIfEnabled(expr,
                                              SourceRangeKind::kContinuation);
  builder()->LoadAccumulatorWithRegister(output_value);
}

void BytecodeGenerator::BuildAwait(Expression* await_expr) {
  // Rather than HandlerTable::UNCAUGHT, async functions use
  // HandlerTable::ASYNC_AWAIT to communicate that top-level exceptions are
  // transformed into promise rejections. This is necessary to prevent emitting
  // multiple debug events for the same uncaught exception. There is no point
  // in the body of an async function where catch prediction is
  // HandlerTable::UNCAUGHT.
  DCHECK(catch_prediction() != HandlerTable::UNCAUGHT);

  {
    // Await(operand) and suspend.
    RegisterAllocationScope register_scope(this);

    int await_builtin_context_index;
    RegisterList args;
    if (IsAsyncGeneratorFunction(function_kind())) {
      await_builtin_context_index =
          catch_prediction() == HandlerTable::ASYNC_AWAIT
              ? Context::ASYNC_GENERATOR_AWAIT_UNCAUGHT
              : Context::ASYNC_GENERATOR_AWAIT_CAUGHT;
      args = register_allocator()->NewRegisterList(2);
      builder()
          ->MoveRegister(generator_object(), args[0])
          .StoreAccumulatorInRegister(args[1]);
    } else {
      await_builtin_context_index =
          catch_prediction() == HandlerTable::ASYNC_AWAIT
              ? Context::ASYNC_FUNCTION_AWAIT_UNCAUGHT_INDEX
              : Context::ASYNC_FUNCTION_AWAIT_CAUGHT_INDEX;
      args = register_allocator()->NewRegisterList(3);
      builder()
          ->MoveRegister(generator_object(), args[0])
          .StoreAccumulatorInRegister(args[1]);

      // AsyncFunction Await builtins require a 3rd parameter to hold the outer
      // promise.
      Variable* var_promise = closure_scope()->promise_var();
      BuildVariableLoadForAccumulatorValue(var_promise, HoleCheckMode::kElided);
      builder()->StoreAccumulatorInRegister(args[2]);
    }

    builder()->CallJSRuntime(await_builtin_context_index, args);
  }

  BuildSuspendPoint(await_expr);

  Register input = register_allocator()->NewRegister();
  Register resume_mode = register_allocator()->NewRegister();

  // Now dispatch on resume mode.
  BytecodeLabel resume_next;
  builder()
      ->StoreAccumulatorInRegister(input)
      .CallRuntime(Runtime::kInlineGeneratorGetResumeMode, generator_object())
      .StoreAccumulatorInRegister(resume_mode)
      .LoadLiteral(Smi::FromInt(JSGeneratorObject::kNext))
      .CompareReference(resume_mode)
      .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &resume_next);

  // Resume with "throw" completion (rethrow the received value).
  // TODO(leszeks): Add a debug-only check that the accumulator is
  // JSGeneratorObject::kThrow.
  builder()->LoadAccumulatorWithRegister(input).ReThrow();

  // Resume with next.
  builder()->Bind(&resume_next);
  builder()->LoadAccumulatorWithRegister(input);
}

void BytecodeGenerator::VisitAwait(Await* expr) {
  builder()->SetExpressionPosition(expr);
  VisitForAccumulatorValue(expr->expression());
  BuildAwait(expr);
  BuildIncrementBlockCoverageCounterIfEnabled(expr,
                                              SourceRangeKind::kContinuation);
}

void BytecodeGenerator::VisitThrow(Throw* expr) {
  AllocateBlockCoverageSlotIfEnabled(expr, SourceRangeKind::kContinuation);
  VisitForAccumulatorValue(expr->exception());
  builder()->SetExpressionPosition(expr);
  builder()->Throw();
}

void BytecodeGenerator::VisitPropertyLoad(Register obj, Property* property) {
  LhsKind property_kind = Property::GetAssignType(property);
  switch (property_kind) {
    case VARIABLE:
      UNREACHABLE();
    case NAMED_PROPERTY: {
      builder()->SetExpressionPosition(property);
      const AstRawString* name =
          property->key()->AsLiteral()->AsRawPropertyName();
      BuildLoadNamedProperty(property, obj, name);
      break;
    }
    case KEYED_PROPERTY: {
      VisitForAccumulatorValue(property->key());
      builder()->SetExpressionPosition(property);
      builder()->LoadKeyedProperty(
          obj, feedback_index(feedback_spec()->AddKeyedLoadICSlot()));
      break;
    }
    case NAMED_SUPER_PROPERTY:
      VisitNamedSuperPropertyLoad(property, Register::invalid_value());
      break;
    case KEYED_SUPER_PROPERTY:
      VisitKeyedSuperPropertyLoad(property, Register::invalid_value());
      break;
  }
}

void BytecodeGenerator::VisitPropertyLoadForRegister(Register obj,
                                                     Property* expr,
                                                     Register destination) {
  ValueResultScope result_scope(this);
  VisitPropertyLoad(obj, expr);
  builder()->StoreAccumulatorInRegister(destination);
}

void BytecodeGenerator::VisitNamedSuperPropertyLoad(Property* property,
                                                    Register opt_receiver_out) {
  RegisterAllocationScope register_scope(this);
  SuperPropertyReference* super_property =
      property->obj()->AsSuperPropertyReference();
  RegisterList args = register_allocator()->NewRegisterList(3);
  VisitForRegisterValue(super_property->this_var(), args[0]);
  VisitForRegisterValue(super_property->home_object(), args[1]);

  builder()->SetExpressionPosition(property);
  builder()
      ->LoadLiteral(property->key()->AsLiteral()->AsRawPropertyName())
      .StoreAccumulatorInRegister(args[2])
      .CallRuntime(Runtime::kLoadFromSuper, args);

  if (opt_receiver_out.is_valid()) {
    builder()->MoveRegister(args[0], opt_receiver_out);
  }
}

void BytecodeGenerator::VisitKeyedSuperPropertyLoad(Property* property,
                                                    Register opt_receiver_out) {
  RegisterAllocationScope register_scope(this);
  SuperPropertyReference* super_property =
      property->obj()->AsSuperPropertyReference();
  RegisterList args = register_allocator()->NewRegisterList(3);
  VisitForRegisterValue(super_property->this_var(), args[0]);
  VisitForRegisterValue(super_property->home_object(), args[1]);
  VisitForRegisterValue(property->key(), args[2]);

  builder()->SetExpressionPosition(property);
  builder()->CallRuntime(Runtime::kLoadKeyedFromSuper, args);

  if (opt_receiver_out.is_valid()) {
    builder()->MoveRegister(args[0], opt_receiver_out);
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

void BytecodeGenerator::VisitResolvedProperty(ResolvedProperty* expr) {
  // Handled by VisitCall().
  UNREACHABLE();
}

void BytecodeGenerator::VisitArguments(ZonePtrList<Expression>* args,
                                       RegisterList* arg_regs) {
  // Visit arguments.
  for (int i = 0; i < static_cast<int>(args->length()); i++) {
    VisitAndPushIntoRegisterList(args->at(i), arg_regs);
  }
}

void BytecodeGenerator::VisitCall(Call* expr) {
  Expression* callee_expr = expr->expression();
  Call::CallType call_type = expr->GetCallType();

  if (call_type == Call::SUPER_CALL) {
    return VisitCallSuper(expr);
  }

  // Grow the args list as we visit receiver / arguments to avoid allocating all
  // the registers up-front. Otherwise these registers are unavailable during
  // receiver / argument visiting and we can end up with memory leaks due to
  // registers keeping objects alive.
  Register callee = register_allocator()->NewRegister();
  RegisterList args = register_allocator()->NewGrowableRegisterList();

  bool implicit_undefined_receiver = false;
  // When a call contains a spread, a Call AST node is only created if there is
  // exactly one spread, and it is the last argument.
  bool is_spread_call = expr->only_last_arg_is_spread();
  bool optimize_as_one_shot = ShouldOptimizeAsOneShot();

  // TODO(petermarshall): We have a lot of call bytecodes that are very similar,
  // see if we can reduce the number by adding a separate argument which
  // specifies the call type (e.g., property, spread, tailcall, etc.).

  // Prepare the callee and the receiver to the function call. This depends on
  // the semantics of the underlying call type.
  switch (call_type) {
    case Call::NAMED_PROPERTY_CALL:
    case Call::KEYED_PROPERTY_CALL: {
      Property* property = callee_expr->AsProperty();
      VisitAndPushIntoRegisterList(property->obj(), &args);
      VisitPropertyLoadForRegister(args.last_register(), property, callee);
      break;
    }
    case Call::RESOLVED_PROPERTY_CALL: {
      ResolvedProperty* resolved = callee_expr->AsResolvedProperty();
      VisitAndPushIntoRegisterList(resolved->object(), &args);
      VisitForAccumulatorValue(resolved->property());
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::GLOBAL_CALL: {
      // Receiver is undefined for global calls.
      if (!is_spread_call && !optimize_as_one_shot) {
        implicit_undefined_receiver = true;
      } else {
        // TODO(leszeks): There's no special bytecode for tail calls or spread
        // calls with an undefined receiver, so just push undefined ourselves.
        BuildPushUndefinedIntoRegisterList(&args);
      }
      // Load callee as a global variable.
      VariableProxy* proxy = callee_expr->AsVariableProxy();
      BuildVariableLoadForAccumulatorValue(proxy->var(),
                                           proxy->hole_check_mode());
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::WITH_CALL: {
      Register receiver = register_allocator()->GrowRegisterList(&args);
      DCHECK(callee_expr->AsVariableProxy()->var()->IsLookupSlot());
      {
        RegisterAllocationScope inner_register_scope(this);
        Register name = register_allocator()->NewRegister();

        // Call %LoadLookupSlotForCall to get the callee and receiver.
        RegisterList result_pair = register_allocator()->NewRegisterList(2);
        Variable* variable = callee_expr->AsVariableProxy()->var();
        builder()
            ->LoadLiteral(variable->raw_name())
            .StoreAccumulatorInRegister(name)
            .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, name,
                                result_pair)
            .MoveRegister(result_pair[0], callee)
            .MoveRegister(result_pair[1], receiver);
      }
      break;
    }
    case Call::OTHER_CALL: {
      // Receiver is undefined for other calls.
      if (!is_spread_call && !optimize_as_one_shot) {
        implicit_undefined_receiver = true;
      } else {
        // TODO(leszeks): There's no special bytecode for tail calls or spread
        // calls with an undefined receiver, so just push undefined ourselves.
        BuildPushUndefinedIntoRegisterList(&args);
      }
      VisitForRegisterValue(callee_expr, callee);
      break;
    }
    case Call::NAMED_SUPER_PROPERTY_CALL: {
      Register receiver = register_allocator()->GrowRegisterList(&args);
      Property* property = callee_expr->AsProperty();
      VisitNamedSuperPropertyLoad(property, receiver);
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::KEYED_SUPER_PROPERTY_CALL: {
      Register receiver = register_allocator()->GrowRegisterList(&args);
      Property* property = callee_expr->AsProperty();
      VisitKeyedSuperPropertyLoad(property, receiver);
      builder()->StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::SUPER_CALL:
      UNREACHABLE();
      break;
  }

  // Evaluate all arguments to the function call and store in sequential args
  // registers.
  VisitArguments(expr->arguments(), &args);
  int reciever_arg_count = implicit_undefined_receiver ? 0 : 1;
  CHECK_EQ(reciever_arg_count + expr->arguments()->length(),
           args.register_count());

  // Resolve callee for a potential direct eval call. This block will mutate the
  // callee value.
  if (expr->is_possibly_eval() && expr->arguments()->length() > 0) {
    RegisterAllocationScope inner_register_scope(this);
    // Set up arguments for ResolvePossiblyDirectEval by copying callee, source
    // strings and function closure, and loading language and
    // position.
    Register first_arg = args[reciever_arg_count];
    RegisterList runtime_call_args = register_allocator()->NewRegisterList(6);
    builder()
        ->MoveRegister(callee, runtime_call_args[0])
        .MoveRegister(first_arg, runtime_call_args[1])
        .MoveRegister(Register::function_closure(), runtime_call_args[2])
        .LoadLiteral(Smi::FromEnum(language_mode()))
        .StoreAccumulatorInRegister(runtime_call_args[3])
        .LoadLiteral(Smi::FromInt(current_scope()->start_position()))
        .StoreAccumulatorInRegister(runtime_call_args[4])
        .LoadLiteral(Smi::FromInt(expr->position()))
        .StoreAccumulatorInRegister(runtime_call_args[5]);

    // Call ResolvePossiblyDirectEval and modify the callee.
    builder()
        ->CallRuntime(Runtime::kResolvePossiblyDirectEval, runtime_call_args)
        .StoreAccumulatorInRegister(callee);
  }

  builder()->SetExpressionPosition(expr);

  if (is_spread_call) {
    DCHECK(!implicit_undefined_receiver);
    builder()->CallWithSpread(callee, args,
                              feedback_index(feedback_spec()->AddCallICSlot()));
  } else if (optimize_as_one_shot) {
    DCHECK(!implicit_undefined_receiver);
    builder()->CallNoFeedback(callee, args);
  } else if (call_type == Call::NAMED_PROPERTY_CALL ||
             call_type == Call::KEYED_PROPERTY_CALL ||
             call_type == Call::RESOLVED_PROPERTY_CALL) {
    DCHECK(!implicit_undefined_receiver);
    builder()->CallProperty(callee, args,
                            feedback_index(feedback_spec()->AddCallICSlot()));
  } else if (implicit_undefined_receiver) {
    builder()->CallUndefinedReceiver(
        callee, args, feedback_index(feedback_spec()->AddCallICSlot()));
  } else {
    builder()->CallAnyReceiver(
        callee, args, feedback_index(feedback_spec()->AddCallICSlot()));
  }
}

void BytecodeGenerator::VisitCallSuper(Call* expr) {
  RegisterAllocationScope register_scope(this);
  SuperCallReference* super = expr->expression()->AsSuperCallReference();
  ZonePtrList<Expression>* args = expr->arguments();

  int first_spread_index = 0;
  for (; first_spread_index < args->length(); first_spread_index++) {
    if (args->at(first_spread_index)->IsSpread()) break;
  }

  // Prepare the constructor to the super call.
  Register this_function = VisitForRegisterValue(super->this_function_var());
  Register constructor = register_allocator()->NewRegister();
  builder()
      ->LoadAccumulatorWithRegister(this_function)
      .GetSuperConstructor(constructor);

  if (first_spread_index < expr->arguments()->length() - 1) {
    // We rewrite something like
    //    super(1, ...x, 2)
    // to
    //    %reflect_construct(constructor, [1, ...x, 2], new_target)
    // That is, we implement (non-last-arg) spreads in super calls via our
    // mechanism for spreads in array literals.

    // First generate the array containing all arguments.
    BuildCreateArrayLiteral(args, nullptr);

    // Now pass that array to %reflect_construct.
    RegisterList construct_args = register_allocator()->NewRegisterList(3);
    builder()->StoreAccumulatorInRegister(construct_args[1]);
    builder()->MoveRegister(constructor, construct_args[0]);
    VisitForRegisterValue(super->new_target_var(), construct_args[2]);
    builder()->CallJSRuntime(Context::REFLECT_CONSTRUCT_INDEX, construct_args);
  } else {
    RegisterList args_regs = register_allocator()->NewGrowableRegisterList();
    VisitArguments(args, &args_regs);
    // The new target is loaded into the accumulator from the
    // {new.target} variable.
    VisitForAccumulatorValue(super->new_target_var());
    builder()->SetExpressionPosition(expr);

    int feedback_slot_index = feedback_index(feedback_spec()->AddCallICSlot());

    if (first_spread_index == expr->arguments()->length() - 1) {
      builder()->ConstructWithSpread(constructor, args_regs,
                                     feedback_slot_index);
    } else {
      DCHECK_EQ(first_spread_index, expr->arguments()->length());
      // Call construct.
      // TODO(turbofan): For now we do gather feedback on super constructor
      // calls, utilizing the existing machinery to inline the actual call
      // target and the JSCreate for the implicit receiver allocation. This
      // is not an ideal solution for super constructor calls, but it gets
      // the job done for now. In the long run we might want to revisit this
      // and come up with a better way.
      builder()->Construct(constructor, args_regs, feedback_slot_index);
    }
  }

  // Explicit calls to the super constructor using super() perform an
  // implicit binding assignment to the 'this' variable.
  //
  // Default constructors don't need have to do the assignment because
  // 'this' isn't accessed in default constructors.
  if (!IsDefaultConstructor(info()->literal()->kind())) {
    BuildVariableAssignment(super->this_var()->var(), Token::INIT,
                            HoleCheckMode::kRequired);
  }

  // The derived constructor has the correct bit set always, so we
  // don't emit code to load and call the initializer if not
  // required.
  //
  // For the arrow function or eval case, we always emit code to load
  // and call the initializer.
  //
  // TODO(gsathya): In the future, we could tag nested arrow functions
  // or eval with the correct bit so that we do the load conditionally
  // if required.
  if (info()->literal()->requires_instance_fields_initializer() ||
      !IsDerivedConstructor(info()->literal()->kind())) {
    Register instance = register_allocator()->NewRegister();
    builder()->StoreAccumulatorInRegister(instance);
    BuildInstanceFieldInitialization(this_function, instance);
    builder()->LoadAccumulatorWithRegister(instance);
  }
}

void BytecodeGenerator::VisitCallNew(CallNew* expr) {
  Register constructor = VisitForRegisterValue(expr->expression());
  RegisterList args = register_allocator()->NewGrowableRegisterList();
  VisitArguments(expr->arguments(), &args);

  // The accumulator holds new target which is the same as the
  // constructor for CallNew.
  builder()->SetExpressionPosition(expr);
  builder()->LoadAccumulatorWithRegister(constructor);

  int feedback_slot_index = feedback_index(feedback_spec()->AddCallICSlot());
  if (expr->only_last_arg_is_spread()) {
    builder()->ConstructWithSpread(constructor, args, feedback_slot_index);
  } else {
    builder()->Construct(constructor, args, feedback_slot_index);
  }
}

void BytecodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  if (expr->is_jsruntime()) {
    RegisterList args = register_allocator()->NewGrowableRegisterList();
    VisitArguments(expr->arguments(), &args);
    builder()->CallJSRuntime(expr->context_index(), args);
  } else {
    // Evaluate all arguments to the runtime call.
    RegisterList args = register_allocator()->NewGrowableRegisterList();
    VisitArguments(expr->arguments(), &args);
    Runtime::FunctionId function_id = expr->function()->function_id;
    builder()->CallRuntime(function_id, args);
  }
}

void BytecodeGenerator::VisitVoid(UnaryOperation* expr) {
  VisitForEffect(expr->expression());
  builder()->LoadUndefined();
}

void BytecodeGenerator::VisitForTypeOfValue(Expression* expr) {
  if (expr->IsVariableProxy()) {
    // Typeof does not throw a reference error on global variables, hence we
    // perform a non-contextual load in case the operand is a variable proxy.
    VariableProxy* proxy = expr->AsVariableProxy();
    BuildVariableLoadForAccumulatorValue(proxy->var(), proxy->hole_check_mode(),
                                         INSIDE_TYPEOF);
  } else {
    VisitForAccumulatorValue(expr);
  }
}

void BytecodeGenerator::VisitTypeOf(UnaryOperation* expr) {
  VisitForTypeOfValue(expr->expression());
  builder()->TypeOf();
}

void BytecodeGenerator::VisitNot(UnaryOperation* expr) {
  if (execution_result()->IsEffect()) {
    VisitForEffect(expr->expression());
  } else if (execution_result()->IsTest()) {
    // No actual logical negation happening, we just swap the control flow, by
    // swapping the target labels and the fallthrough branch, and visit in the
    // same test result context.
    TestResultScope* test_result = execution_result()->AsTest();
    test_result->InvertControlFlow();
    VisitInSameTestExecutionScope(expr->expression());
  } else {
    TypeHint type_hint = VisitForAccumulatorValue(expr->expression());
    builder()->LogicalNot(ToBooleanModeFromTypeHint(type_hint));
    // Always returns a boolean value.
    execution_result()->SetResultIsBoolean();
  }
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
    case Token::Value::ADD:
    case Token::Value::SUB:
    case Token::Value::BIT_NOT:
      VisitForAccumulatorValue(expr->expression());
      builder()->SetExpressionPosition(expr);
      builder()->UnaryOperation(
          expr->op(), feedback_index(feedback_spec()->AddBinaryOpICSlot()));
      break;
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
    // not allowed in strict mode. Deleting 'this' and 'new.target' is allowed
    // in both modes.
    VariableProxy* proxy = expr->expression()->AsVariableProxy();
    DCHECK(is_sloppy(language_mode()) || proxy->is_this() ||
           proxy->is_new_target());
    if (proxy->is_this() || proxy->is_new_target()) {
      builder()->LoadTrue();
    } else {
      Variable* variable = proxy->var();
      switch (variable->location()) {
        case VariableLocation::PARAMETER:
        case VariableLocation::LOCAL:
        case VariableLocation::CONTEXT: {
          // Deleting local var/let/const, context variables, and arguments
          // does not have any effect.
          builder()->LoadFalse();
          break;
        }
        case VariableLocation::UNALLOCATED:
        // TODO(adamk): Falling through to the runtime results in correct
        // behavior, but does unnecessary context-walking (since scope
        // analysis has already proven that the variable doesn't exist in
        // any non-global scope). Consider adding a DeleteGlobal bytecode
        // that knows how to deal with ScriptContexts as well as global
        // object properties.
        case VariableLocation::LOOKUP: {
          Register name_reg = register_allocator()->NewRegister();
          builder()
              ->LoadLiteral(variable->raw_name())
              .StoreAccumulatorInRegister(name_reg)
              .CallRuntime(Runtime::kDeleteLookupSlot, name_reg);
          break;
        }
        default:
          UNREACHABLE();
      }
    }
  } else {
    // Delete of an unresolvable reference returns true.
    VisitForEffect(expr->expression());
    builder()->LoadTrue();
  }
}

void BytecodeGenerator::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpression());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->expression()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  bool is_postfix = expr->is_postfix() && !execution_result()->IsEffect();

  // Evaluate LHS expression and get old value.
  Register object, key, old_value;
  RegisterList super_property_args;
  const AstRawString* name;
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      BuildVariableLoadForAccumulatorValue(proxy->var(),
                                           proxy->hole_check_mode());
      break;
    }
    case NAMED_PROPERTY: {
      object = VisitForRegisterValue(property->obj());
      name = property->key()->AsLiteral()->AsRawPropertyName();
      builder()->LoadNamedProperty(
          object, name,
          feedback_index(GetCachedLoadICSlot(property->obj(), name)));
      break;
    }
    case KEYED_PROPERTY: {
      object = VisitForRegisterValue(property->obj());
      // Use visit for accumulator here since we need the key in the accumulator
      // for the LoadKeyedProperty.
      key = register_allocator()->NewRegister();
      VisitForAccumulatorValue(property->key());
      builder()->StoreAccumulatorInRegister(key).LoadKeyedProperty(
          object, feedback_index(feedback_spec()->AddKeyedLoadICSlot()));
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      super_property_args = register_allocator()->NewRegisterList(4);
      RegisterList load_super_args = super_property_args.Truncate(3);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), load_super_args[0]);
      VisitForRegisterValue(super_property->home_object(), load_super_args[1]);
      builder()
          ->LoadLiteral(property->key()->AsLiteral()->AsRawPropertyName())
          .StoreAccumulatorInRegister(load_super_args[2])
          .CallRuntime(Runtime::kLoadFromSuper, load_super_args);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      super_property_args = register_allocator()->NewRegisterList(4);
      RegisterList load_super_args = super_property_args.Truncate(3);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), load_super_args[0]);
      VisitForRegisterValue(super_property->home_object(), load_super_args[1]);
      VisitForRegisterValue(property->key(), load_super_args[2]);
      builder()->CallRuntime(Runtime::kLoadKeyedFromSuper, load_super_args);
      break;
    }
  }

  // Save result for postfix expressions.
  FeedbackSlot count_slot = feedback_spec()->AddBinaryOpICSlot();
  if (is_postfix) {
    old_value = register_allocator()->NewRegister();
    // Convert old value into a number before saving it.
    // TODO(ignition): Think about adding proper PostInc/PostDec bytecodes
    // instead of this ToNumeric + Inc/Dec dance.
    builder()
        ->ToNumeric(feedback_index(count_slot))
        .StoreAccumulatorInRegister(old_value);
  }

  // Perform +1/-1 operation.
  builder()->UnaryOperation(expr->op(), feedback_index(count_slot));

  // Store the value.
  builder()->SetExpressionPosition(expr);
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      BuildVariableAssignment(proxy->var(), expr->op(),
                              proxy->hole_check_mode());
      break;
    }
    case NAMED_PROPERTY: {
      FeedbackSlot slot = GetCachedStoreICSlot(property->obj(), name);
      Register value;
      if (!execution_result()->IsEffect()) {
        value = register_allocator()->NewRegister();
        builder()->StoreAccumulatorInRegister(value);
      }
      builder()->StoreNamedProperty(object, name, feedback_index(slot),
                                    language_mode());
      if (!execution_result()->IsEffect()) {
        builder()->LoadAccumulatorWithRegister(value);
      }
      break;
    }
    case KEYED_PROPERTY: {
      FeedbackSlot slot = feedback_spec()->AddKeyedStoreICSlot(language_mode());
      Register value;
      if (!execution_result()->IsEffect()) {
        value = register_allocator()->NewRegister();
        builder()->StoreAccumulatorInRegister(value);
      }
      builder()->StoreKeyedProperty(object, key, feedback_index(slot),
                                    language_mode());
      if (!execution_result()->IsEffect()) {
        builder()->LoadAccumulatorWithRegister(value);
      }
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      builder()
          ->StoreAccumulatorInRegister(super_property_args[3])
          .CallRuntime(StoreToSuperRuntimeId(), super_property_args);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      builder()
          ->StoreAccumulatorInRegister(super_property_args[3])
          .CallRuntime(StoreKeyedToSuperRuntimeId(), super_property_args);
      break;
    }
  }

  // Restore old value for postfix expressions.
  if (is_postfix) {
    builder()->LoadAccumulatorWithRegister(old_value);
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

void BytecodeGenerator::VisitNaryOperation(NaryOperation* expr) {
  switch (expr->op()) {
    case Token::COMMA:
      VisitNaryCommaExpression(expr);
      break;
    case Token::OR:
      VisitNaryLogicalOrExpression(expr);
      break;
    case Token::AND:
      VisitNaryLogicalAndExpression(expr);
      break;
    default:
      VisitNaryArithmeticExpression(expr);
      break;
  }
}

void BytecodeGenerator::BuildLiteralCompareNil(
    Token::Value op, BytecodeArrayBuilder::NilValue nil) {
  if (execution_result()->IsTest()) {
    TestResultScope* test_result = execution_result()->AsTest();
    switch (test_result->fallthrough()) {
      case TestFallthrough::kThen:
        builder()->JumpIfNotNil(test_result->NewElseLabel(), op, nil);
        break;
      case TestFallthrough::kElse:
        builder()->JumpIfNil(test_result->NewThenLabel(), op, nil);
        break;
      case TestFallthrough::kNone:
        builder()
            ->JumpIfNil(test_result->NewThenLabel(), op, nil)
            .Jump(test_result->NewElseLabel());
    }
    test_result->SetResultConsumedByTest();
  } else {
    builder()->CompareNil(op, nil);
  }
}

void BytecodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Expression* sub_expr;
  Literal* literal;
  if (expr->IsLiteralCompareTypeof(&sub_expr, &literal)) {
    // Emit a fast literal comparion for expressions of the form:
    // typeof(x) === 'string'.
    VisitForTypeOfValue(sub_expr);
    builder()->SetExpressionPosition(expr);
    TestTypeOfFlags::LiteralFlag literal_flag =
        TestTypeOfFlags::GetFlagForLiteral(ast_string_constants(), literal);
    if (literal_flag == TestTypeOfFlags::LiteralFlag::kOther) {
      builder()->LoadFalse();
    } else {
      builder()->CompareTypeOf(literal_flag);
    }
  } else if (expr->IsLiteralCompareUndefined(&sub_expr)) {
    VisitForAccumulatorValue(sub_expr);
    builder()->SetExpressionPosition(expr);
    BuildLiteralCompareNil(expr->op(), BytecodeArrayBuilder::kUndefinedValue);
  } else if (expr->IsLiteralCompareNull(&sub_expr)) {
    VisitForAccumulatorValue(sub_expr);
    builder()->SetExpressionPosition(expr);
    BuildLiteralCompareNil(expr->op(), BytecodeArrayBuilder::kNullValue);
  } else {
    Register lhs = VisitForRegisterValue(expr->left());
    VisitForAccumulatorValue(expr->right());
    builder()->SetExpressionPosition(expr);
    if (expr->op() == Token::IN) {
      builder()->CompareOperation(expr->op(), lhs);
    } else if (expr->op() == Token::INSTANCEOF) {
      FeedbackSlot slot = feedback_spec()->AddInstanceOfSlot();
      builder()->CompareOperation(expr->op(), lhs, feedback_index(slot));
    } else {
      FeedbackSlot slot = feedback_spec()->AddCompareICSlot();
      builder()->CompareOperation(expr->op(), lhs, feedback_index(slot));
    }
  }
  // Always returns a boolean value.
  execution_result()->SetResultIsBoolean();
}

void BytecodeGenerator::VisitArithmeticExpression(BinaryOperation* expr) {
  FeedbackSlot slot = feedback_spec()->AddBinaryOpICSlot();
  Expression* subexpr;
  Smi* literal;
  if (expr->IsSmiLiteralOperation(&subexpr, &literal)) {
    TypeHint type_hint = VisitForAccumulatorValue(subexpr);
    builder()->SetExpressionPosition(expr);
    builder()->BinaryOperationSmiLiteral(expr->op(), literal,
                                         feedback_index(slot));
    if (expr->op() == Token::ADD && type_hint == TypeHint::kString) {
      execution_result()->SetResultIsString();
    }
  } else {
    TypeHint lhs_type = VisitForAccumulatorValue(expr->left());
    Register lhs = register_allocator()->NewRegister();
    builder()->StoreAccumulatorInRegister(lhs);
    TypeHint rhs_type = VisitForAccumulatorValue(expr->right());
    if (expr->op() == Token::ADD &&
        (lhs_type == TypeHint::kString || rhs_type == TypeHint::kString)) {
      execution_result()->SetResultIsString();
    }

    builder()->SetExpressionPosition(expr);
    builder()->BinaryOperation(expr->op(), lhs, feedback_index(slot));
  }
}

void BytecodeGenerator::VisitNaryArithmeticExpression(NaryOperation* expr) {
  // TODO(leszeks): Add support for lhs smi in commutative ops.
  TypeHint type_hint = VisitForAccumulatorValue(expr->first());

  for (size_t i = 0; i < expr->subsequent_length(); ++i) {
    RegisterAllocationScope register_scope(this);
    if (expr->subsequent(i)->IsSmiLiteral()) {
      builder()->SetExpressionPosition(expr->subsequent_op_position(i));
      builder()->BinaryOperationSmiLiteral(
          expr->op(), expr->subsequent(i)->AsLiteral()->AsSmiLiteral(),
          feedback_index(feedback_spec()->AddBinaryOpICSlot()));
    } else {
      Register lhs = register_allocator()->NewRegister();
      builder()->StoreAccumulatorInRegister(lhs);
      TypeHint rhs_hint = VisitForAccumulatorValue(expr->subsequent(i));
      if (rhs_hint == TypeHint::kString) type_hint = TypeHint::kString;
      builder()->SetExpressionPosition(expr->subsequent_op_position(i));
      builder()->BinaryOperation(
          expr->op(), lhs,
          feedback_index(feedback_spec()->AddBinaryOpICSlot()));
    }
  }

  if (type_hint == TypeHint::kString && expr->op() == Token::ADD) {
    // If any operand of an ADD is a String, a String is produced.
    execution_result()->SetResultIsString();
  }
}

// Note: the actual spreading is performed by the surrounding expression's
// visitor.
void BytecodeGenerator::VisitSpread(Spread* expr) { Visit(expr->expression()); }

void BytecodeGenerator::VisitEmptyParentheses(EmptyParentheses* expr) {
  UNREACHABLE();
}

void BytecodeGenerator::VisitImportCallExpression(ImportCallExpression* expr) {
  RegisterList args = register_allocator()->NewRegisterList(2);
  VisitForRegisterValue(expr->argument(), args[1]);
  builder()
      ->MoveRegister(Register::function_closure(), args[0])
      .CallRuntime(Runtime::kDynamicImportCall, args);
}

void BytecodeGenerator::BuildGetIterator(Expression* iterable,
                                         IteratorType hint) {
  RegisterList args = register_allocator()->NewRegisterList(1);
  Register method = register_allocator()->NewRegister();
  Register obj = args[0];

  VisitForAccumulatorValue(iterable);

  if (hint == IteratorType::kAsync) {
    // Set method to GetMethod(obj, @@asyncIterator)
    builder()->StoreAccumulatorInRegister(obj).LoadAsyncIteratorProperty(
        obj, feedback_index(feedback_spec()->AddLoadICSlot()));

    BytecodeLabel async_iterator_undefined, async_iterator_null, done;
    // TODO(ignition): Add a single opcode for JumpIfNullOrUndefined
    builder()->JumpIfUndefined(&async_iterator_undefined);
    builder()->JumpIfNull(&async_iterator_null);

    // Let iterator be Call(method, obj)
    builder()->StoreAccumulatorInRegister(method).CallProperty(
        method, args, feedback_index(feedback_spec()->AddCallICSlot()));

    // If Type(iterator) is not Object, throw a TypeError exception.
    builder()->JumpIfJSReceiver(&done);
    builder()->CallRuntime(Runtime::kThrowSymbolAsyncIteratorInvalid);

    builder()->Bind(&async_iterator_undefined);
    builder()->Bind(&async_iterator_null);
    // If method is undefined,
    //     Let syncMethod be GetMethod(obj, @@iterator)
    builder()
        ->LoadIteratorProperty(obj,
                               feedback_index(feedback_spec()->AddLoadICSlot()))
        .StoreAccumulatorInRegister(method);

    //     Let syncIterator be Call(syncMethod, obj)
    builder()->CallProperty(method, args,
                            feedback_index(feedback_spec()->AddCallICSlot()));

    // Return CreateAsyncFromSyncIterator(syncIterator)
    // alias `method` register as it's no longer used
    Register sync_iter = method;
    builder()->StoreAccumulatorInRegister(sync_iter).CallRuntime(
        Runtime::kInlineCreateAsyncFromSyncIterator, sync_iter);

    builder()->Bind(&done);
  } else {
    // Let method be GetMethod(obj, @@iterator).
    builder()
        ->StoreAccumulatorInRegister(obj)
        .LoadIteratorProperty(obj,
                              feedback_index(feedback_spec()->AddLoadICSlot()))
        .StoreAccumulatorInRegister(method);

    // Let iterator be Call(method, obj).
    builder()->CallProperty(method, args,
                            feedback_index(feedback_spec()->AddCallICSlot()));

    // If Type(iterator) is not Object, throw a TypeError exception.
    BytecodeLabel no_type_error;
    builder()->JumpIfJSReceiver(&no_type_error);
    builder()->CallRuntime(Runtime::kThrowSymbolIteratorInvalid);
    builder()->Bind(&no_type_error);
  }
}

// Returns an IteratorRecord which is valid for the lifetime of the current
// register_allocation_scope.
BytecodeGenerator::IteratorRecord BytecodeGenerator::BuildGetIteratorRecord(
    Expression* iterable, Register next, Register object, IteratorType hint) {
  DCHECK(next.is_valid() && object.is_valid());
  BuildGetIterator(iterable, hint);

  builder()
      ->StoreAccumulatorInRegister(object)
      .LoadNamedProperty(object, ast_string_constants()->next_string(),
                         feedback_index(feedback_spec()->AddLoadICSlot()))
      .StoreAccumulatorInRegister(next);
  return IteratorRecord(object, next, hint);
}

BytecodeGenerator::IteratorRecord BytecodeGenerator::BuildGetIteratorRecord(
    Expression* iterable, IteratorType hint) {
  Register next = register_allocator()->NewRegister();
  Register object = register_allocator()->NewRegister();
  return BuildGetIteratorRecord(iterable, next, object, hint);
}

void BytecodeGenerator::BuildIteratorNext(const IteratorRecord& iterator,
                                          Register next_result) {
  DCHECK(next_result.is_valid());
  builder()->CallProperty(iterator.next(), RegisterList(iterator.object()),
                          feedback_index(feedback_spec()->AddCallICSlot()));

  // TODO(caitp): support async IteratorNext here.

  BytecodeLabel is_object;
  builder()
      ->StoreAccumulatorInRegister(next_result)
      .JumpIfJSReceiver(&is_object)
      .CallRuntime(Runtime::kThrowIteratorResultNotAnObject, next_result)
      .Bind(&is_object);
}

void BytecodeGenerator::BuildCallIteratorMethod(Register iterator,
                                                const AstRawString* method_name,
                                                RegisterList receiver_and_args,
                                                BytecodeLabel* if_called,
                                                BytecodeLabels* if_notcalled) {
  RegisterAllocationScope register_scope(this);

  Register method = register_allocator()->NewRegister();
  FeedbackSlot slot = feedback_spec()->AddLoadICSlot();
  builder()
      ->LoadNamedProperty(iterator, method_name, feedback_index(slot))
      .JumpIfUndefined(if_notcalled->New())
      .JumpIfNull(if_notcalled->New())
      .StoreAccumulatorInRegister(method)
      .CallProperty(method, receiver_and_args,
                    feedback_index(feedback_spec()->AddCallICSlot()))
      .Jump(if_called);
}

void BytecodeGenerator::BuildIteratorClose(const IteratorRecord& iterator,
                                           Expression* expr) {
  RegisterAllocationScope register_scope(this);
  BytecodeLabels done(zone());
  BytecodeLabel if_called;
  RegisterList args = RegisterList(iterator.object());
  BuildCallIteratorMethod(iterator.object(),
                          ast_string_constants()->return_string(), args,
                          &if_called, &done);
  builder()->Bind(&if_called);

  if (iterator.type() == IteratorType::kAsync) {
    DCHECK_NOT_NULL(expr);
    BuildAwait(expr);
  }

  builder()->JumpIfJSReceiver(done.New());
  {
    RegisterAllocationScope register_scope(this);
    Register return_result = register_allocator()->NewRegister();
    builder()
        ->StoreAccumulatorInRegister(return_result)
        .CallRuntime(Runtime::kThrowIteratorResultNotAnObject, return_result);
  }

  done.Bind(builder());
}

void BytecodeGenerator::VisitGetIterator(GetIterator* expr) {
  builder()->SetExpressionPosition(expr);
  BuildGetIterator(expr->iterable(), expr->hint());
}

void BytecodeGenerator::VisitGetTemplateObject(GetTemplateObject* expr) {
  builder()->SetExpressionPosition(expr);
  size_t entry = builder()->AllocateDeferredConstantPoolEntry();
  template_objects_.push_back(std::make_pair(expr, entry));
  FeedbackSlot literal_slot = feedback_spec()->AddLiteralSlot();
  builder()->GetTemplateObject(entry, feedback_index(literal_slot));
}

void BytecodeGenerator::VisitTemplateLiteral(TemplateLiteral* expr) {
  const ZonePtrList<const AstRawString>& parts = *expr->string_parts();
  const ZonePtrList<Expression>& substitutions = *expr->substitutions();
  // Template strings with no substitutions are turned into StringLiterals.
  DCHECK_GT(substitutions.length(), 0);
  DCHECK_EQ(parts.length(), substitutions.length() + 1);

  // Generate string concatenation
  // TODO(caitp): Don't generate feedback slot if it's not used --- introduce
  // a simple, concise, reusable mechanism to lazily create reusable slots.
  FeedbackSlot slot = feedback_spec()->AddBinaryOpICSlot();
  Register last_part = register_allocator()->NewRegister();
  bool last_part_valid = false;

  builder()->SetExpressionPosition(expr);
  for (int i = 0; i < substitutions.length(); ++i) {
    if (i != 0) {
      builder()->StoreAccumulatorInRegister(last_part);
      last_part_valid = true;
    }

    if (!parts[i]->IsEmpty()) {
      builder()->LoadLiteral(parts[i]);
      if (last_part_valid) {
        builder()->BinaryOperation(Token::ADD, last_part, feedback_index(slot));
      }
      builder()->StoreAccumulatorInRegister(last_part);
      last_part_valid = true;
    }

    TypeHint type_hint = VisitForAccumulatorValue(substitutions[i]);
    if (type_hint != TypeHint::kString) {
      builder()->ToString();
    }
    if (last_part_valid) {
      builder()->BinaryOperation(Token::ADD, last_part, feedback_index(slot));
    }
    last_part_valid = false;
  }

  if (!parts.last()->IsEmpty()) {
    builder()->StoreAccumulatorInRegister(last_part);
    builder()->LoadLiteral(parts.last());
    builder()->BinaryOperation(Token::ADD, last_part, feedback_index(slot));
  }
}

void BytecodeGenerator::VisitThisFunction(ThisFunction* expr) {
  builder()->LoadAccumulatorWithRegister(Register::function_closure());
}

void BytecodeGenerator::VisitSuperCallReference(SuperCallReference* expr) {
  // Handled by VisitCall().
  UNREACHABLE();
}

void BytecodeGenerator::VisitSuperPropertyReference(
    SuperPropertyReference* expr) {
  builder()->CallRuntime(Runtime::kThrowUnsupportedSuperError);
}

void BytecodeGenerator::VisitCommaExpression(BinaryOperation* binop) {
  VisitForEffect(binop->left());
  Visit(binop->right());
}

void BytecodeGenerator::VisitNaryCommaExpression(NaryOperation* expr) {
  DCHECK_GT(expr->subsequent_length(), 0);

  VisitForEffect(expr->first());
  for (size_t i = 0; i < expr->subsequent_length() - 1; ++i) {
    VisitForEffect(expr->subsequent(i));
  }
  Visit(expr->subsequent(expr->subsequent_length() - 1));
}

void BytecodeGenerator::VisitLogicalTestSubExpression(
    Token::Value token, Expression* expr, BytecodeLabels* then_labels,
    BytecodeLabels* else_labels, int coverage_slot) {
  DCHECK(token == Token::OR || token == Token::AND);

  BytecodeLabels test_next(zone());
  if (token == Token::OR) {
    VisitForTest(expr, then_labels, &test_next, TestFallthrough::kElse);
  } else {
    DCHECK_EQ(Token::AND, token);
    VisitForTest(expr, &test_next, else_labels, TestFallthrough::kThen);
  }
  test_next.Bind(builder());

  BuildIncrementBlockCoverageCounterIfEnabled(coverage_slot);
}

void BytecodeGenerator::VisitLogicalTest(Token::Value token, Expression* left,
                                         Expression* right,
                                         int right_coverage_slot) {
  DCHECK(token == Token::OR || token == Token::AND);
  TestResultScope* test_result = execution_result()->AsTest();
  BytecodeLabels* then_labels = test_result->then_labels();
  BytecodeLabels* else_labels = test_result->else_labels();
  TestFallthrough fallthrough = test_result->fallthrough();

  VisitLogicalTestSubExpression(token, left, then_labels, else_labels,
                                right_coverage_slot);
  // The last test has the same then, else and fallthrough as the parent test.
  VisitForTest(right, then_labels, else_labels, fallthrough);
}

void BytecodeGenerator::VisitNaryLogicalTest(
    Token::Value token, NaryOperation* expr,
    const NaryCodeCoverageSlots* coverage_slots) {
  DCHECK(token == Token::OR || token == Token::AND);
  DCHECK_GT(expr->subsequent_length(), 0);

  TestResultScope* test_result = execution_result()->AsTest();
  BytecodeLabels* then_labels = test_result->then_labels();
  BytecodeLabels* else_labels = test_result->else_labels();
  TestFallthrough fallthrough = test_result->fallthrough();

  VisitLogicalTestSubExpression(token, expr->first(), then_labels, else_labels,
                                coverage_slots->GetSlotFor(0));
  for (size_t i = 0; i < expr->subsequent_length() - 1; ++i) {
    VisitLogicalTestSubExpression(token, expr->subsequent(i), then_labels,
                                  else_labels,
                                  coverage_slots->GetSlotFor(i + 1));
  }
  // The last test has the same then, else and fallthrough as the parent test.
  VisitForTest(expr->subsequent(expr->subsequent_length() - 1), then_labels,
               else_labels, fallthrough);
}

bool BytecodeGenerator::VisitLogicalOrSubExpression(Expression* expr,
                                                    BytecodeLabels* end_labels,
                                                    int coverage_slot) {
  if (expr->ToBooleanIsTrue()) {
    VisitForAccumulatorValue(expr);
    end_labels->Bind(builder());
    return true;
  } else if (!expr->ToBooleanIsFalse()) {
    TypeHint type_hint = VisitForAccumulatorValue(expr);
    builder()->JumpIfTrue(ToBooleanModeFromTypeHint(type_hint),
                          end_labels->New());
  }

  BuildIncrementBlockCoverageCounterIfEnabled(coverage_slot);

  return false;
}

bool BytecodeGenerator::VisitLogicalAndSubExpression(Expression* expr,
                                                     BytecodeLabels* end_labels,
                                                     int coverage_slot) {
  if (expr->ToBooleanIsFalse()) {
    VisitForAccumulatorValue(expr);
    end_labels->Bind(builder());
    return true;
  } else if (!expr->ToBooleanIsTrue()) {
    TypeHint type_hint = VisitForAccumulatorValue(expr);
    builder()->JumpIfFalse(ToBooleanModeFromTypeHint(type_hint),
                           end_labels->New());
  }

  BuildIncrementBlockCoverageCounterIfEnabled(coverage_slot);

  return false;
}

void BytecodeGenerator::VisitLogicalOrExpression(BinaryOperation* binop) {
  Expression* left = binop->left();
  Expression* right = binop->right();

  int right_coverage_slot =
      AllocateBlockCoverageSlotIfEnabled(binop, SourceRangeKind::kRight);

  if (execution_result()->IsTest()) {
    TestResultScope* test_result = execution_result()->AsTest();
    if (left->ToBooleanIsTrue()) {
      builder()->Jump(test_result->NewThenLabel());
    } else if (left->ToBooleanIsFalse() && right->ToBooleanIsFalse()) {
      BuildIncrementBlockCoverageCounterIfEnabled(right_coverage_slot);
      builder()->Jump(test_result->NewElseLabel());
    } else {
      VisitLogicalTest(Token::OR, left, right, right_coverage_slot);
    }
    test_result->SetResultConsumedByTest();
  } else {
    BytecodeLabels end_labels(zone());
    if (VisitLogicalOrSubExpression(left, &end_labels, right_coverage_slot)) {
      return;
    }
    VisitForAccumulatorValue(right);
    end_labels.Bind(builder());
  }
}

void BytecodeGenerator::VisitNaryLogicalOrExpression(NaryOperation* expr) {
  Expression* first = expr->first();
  DCHECK_GT(expr->subsequent_length(), 0);

  NaryCodeCoverageSlots coverage_slots(this, expr);

  if (execution_result()->IsTest()) {
    TestResultScope* test_result = execution_result()->AsTest();
    if (first->ToBooleanIsTrue()) {
      builder()->Jump(test_result->NewThenLabel());
    } else {
      VisitNaryLogicalTest(Token::OR, expr, &coverage_slots);
    }
    test_result->SetResultConsumedByTest();
  } else {
    BytecodeLabels end_labels(zone());
    if (VisitLogicalOrSubExpression(first, &end_labels,
                                    coverage_slots.GetSlotFor(0))) {
      return;
    }
    for (size_t i = 0; i < expr->subsequent_length() - 1; ++i) {
      if (VisitLogicalOrSubExpression(expr->subsequent(i), &end_labels,
                                      coverage_slots.GetSlotFor(i + 1))) {
        return;
      }
    }
    // We have to visit the last value even if it's true, because we need its
    // actual value.
    VisitForAccumulatorValue(expr->subsequent(expr->subsequent_length() - 1));
    end_labels.Bind(builder());
  }
}

void BytecodeGenerator::VisitLogicalAndExpression(BinaryOperation* binop) {
  Expression* left = binop->left();
  Expression* right = binop->right();

  int right_coverage_slot =
      AllocateBlockCoverageSlotIfEnabled(binop, SourceRangeKind::kRight);

  if (execution_result()->IsTest()) {
    TestResultScope* test_result = execution_result()->AsTest();
    if (left->ToBooleanIsFalse()) {
      builder()->Jump(test_result->NewElseLabel());
    } else if (left->ToBooleanIsTrue() && right->ToBooleanIsTrue()) {
      BuildIncrementBlockCoverageCounterIfEnabled(right_coverage_slot);
      builder()->Jump(test_result->NewThenLabel());
    } else {
      VisitLogicalTest(Token::AND, left, right, right_coverage_slot);
    }
    test_result->SetResultConsumedByTest();
  } else {
    BytecodeLabels end_labels(zone());
    if (VisitLogicalAndSubExpression(left, &end_labels, right_coverage_slot)) {
      return;
    }
    VisitForAccumulatorValue(right);
    end_labels.Bind(builder());
  }
}

void BytecodeGenerator::VisitNaryLogicalAndExpression(NaryOperation* expr) {
  Expression* first = expr->first();
  DCHECK_GT(expr->subsequent_length(), 0);

  NaryCodeCoverageSlots coverage_slots(this, expr);

  if (execution_result()->IsTest()) {
    TestResultScope* test_result = execution_result()->AsTest();
    if (first->ToBooleanIsFalse()) {
      builder()->Jump(test_result->NewElseLabel());
    } else {
      VisitNaryLogicalTest(Token::AND, expr, &coverage_slots);
    }
    test_result->SetResultConsumedByTest();
  } else {
    BytecodeLabels end_labels(zone());
    if (VisitLogicalAndSubExpression(first, &end_labels,
                                     coverage_slots.GetSlotFor(0))) {
      return;
    }
    for (size_t i = 0; i < expr->subsequent_length() - 1; ++i) {
      if (VisitLogicalAndSubExpression(expr->subsequent(i), &end_labels,
                                       coverage_slots.GetSlotFor(i + 1))) {
        return;
      }
    }
    // We have to visit the last value even if it's false, because we need its
    // actual value.
    VisitForAccumulatorValue(expr->subsequent(expr->subsequent_length() - 1));
    end_labels.Bind(builder());
  }
}

void BytecodeGenerator::VisitRewritableExpression(RewritableExpression* expr) {
  Visit(expr->expression());
}

void BytecodeGenerator::BuildNewLocalActivationContext() {
  ValueResultScope value_execution_result(this);
  Scope* scope = closure_scope();
  DCHECK_EQ(current_scope(), closure_scope());

  // Create the appropriate context.
  if (scope->is_script_scope()) {
    Register scope_reg = register_allocator()->NewRegister();
    builder()
        ->LoadLiteral(scope)
        .StoreAccumulatorInRegister(scope_reg)
        .CallRuntime(Runtime::kNewScriptContext, scope_reg);
  } else if (scope->is_module_scope()) {
    // We don't need to do anything for the outer script scope.
    DCHECK(scope->outer_scope()->is_script_scope());

    // A JSFunction representing a module is called with the module object as
    // its sole argument.
    RegisterList args = register_allocator()->NewRegisterList(2);
    builder()
        ->MoveRegister(builder()->Parameter(0), args[0])
        .LoadLiteral(scope)
        .StoreAccumulatorInRegister(args[1])
        .CallRuntime(Runtime::kPushModuleContext, args);
  } else {
    DCHECK(scope->is_function_scope() || scope->is_eval_scope());
    int slot_count = scope->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (slot_count <= ConstructorBuiltins::MaximumFunctionContextSlots()) {
      switch (scope->scope_type()) {
        case EVAL_SCOPE:
          builder()->CreateEvalContext(scope, slot_count);
          break;
        case FUNCTION_SCOPE:
          builder()->CreateFunctionContext(scope, slot_count);
          break;
        default:
          UNREACHABLE();
      }
    } else {
      Register arg = register_allocator()->NewRegister();
      builder()->LoadLiteral(scope).StoreAccumulatorInRegister(arg).CallRuntime(
          Runtime::kNewFunctionContext, arg);
    }
  }
}

void BytecodeGenerator::BuildLocalActivationContextInitialization() {
  DeclarationScope* scope = closure_scope();

  if (scope->has_this_declaration() && scope->receiver()->IsContextSlot()) {
    Variable* variable = scope->receiver();
    Register receiver(builder()->Receiver());
    // Context variable (at bottom of the context chain).
    DCHECK_EQ(0, scope->ContextChainLength(variable->scope()));
    builder()->LoadAccumulatorWithRegister(receiver).StoreContextSlot(
        execution_context()->reg(), variable->index(), 0);
  }

  // Copy parameters into context if necessary.
  int num_parameters = scope->num_parameters();
  for (int i = 0; i < num_parameters; i++) {
    Variable* variable = scope->parameter(i);
    if (!variable->IsContextSlot()) continue;

    Register parameter(builder()->Parameter(i));
    // Context variable (at bottom of the context chain).
    DCHECK_EQ(0, scope->ContextChainLength(variable->scope()));
    builder()->LoadAccumulatorWithRegister(parameter).StoreContextSlot(
        execution_context()->reg(), variable->index(), 0);
  }
}

void BytecodeGenerator::BuildNewLocalBlockContext(Scope* scope) {
  ValueResultScope value_execution_result(this);
  DCHECK(scope->is_block_scope());

  builder()->CreateBlockContext(scope);
}

void BytecodeGenerator::BuildNewLocalWithContext(Scope* scope) {
  ValueResultScope value_execution_result(this);

  Register extension_object = register_allocator()->NewRegister();

  builder()->ToObject(extension_object);
  builder()->CreateWithContext(extension_object, scope);
}

void BytecodeGenerator::BuildNewLocalCatchContext(Scope* scope) {
  ValueResultScope value_execution_result(this);
  DCHECK(scope->catch_variable()->IsContextSlot());

  Register exception = register_allocator()->NewRegister();
  builder()->StoreAccumulatorInRegister(exception);
  builder()->CreateCatchContext(exception, scope);
}

void BytecodeGenerator::VisitObjectLiteralAccessor(
    Register home_object, ObjectLiteralProperty* property, Register value_out) {
  if (property == nullptr) {
    builder()->LoadNull().StoreAccumulatorInRegister(value_out);
  } else {
    VisitForRegisterValue(property->value(), value_out);
    VisitSetHomeObject(value_out, home_object, property);
  }
}

void BytecodeGenerator::VisitSetHomeObject(Register value, Register home_object,
                                           LiteralProperty* property) {
  Expression* expr = property->value();
  if (FunctionLiteral::NeedsHomeObject(expr)) {
    FeedbackSlot slot = feedback_spec()->AddStoreICSlot(language_mode());
    builder()
        ->LoadAccumulatorWithRegister(home_object)
        .StoreHomeObjectProperty(value, feedback_index(slot), language_mode());
  }
}

void BytecodeGenerator::VisitArgumentsObject(Variable* variable) {
  if (variable == nullptr) return;

  DCHECK(variable->IsContextSlot() || variable->IsStackAllocated());

  // Allocate and initialize a new arguments object and assign to the
  // {arguments} variable.
  builder()->CreateArguments(closure_scope()->GetArgumentsType());
  BuildVariableAssignment(variable, Token::ASSIGN, HoleCheckMode::kElided);
}

void BytecodeGenerator::VisitRestArgumentsArray(Variable* rest) {
  if (rest == nullptr) return;

  // Allocate and initialize a new rest parameter and assign to the {rest}
  // variable.
  builder()->CreateArguments(CreateArgumentsType::kRestParameter);
  DCHECK(rest->IsContextSlot() || rest->IsStackAllocated());
  BuildVariableAssignment(rest, Token::ASSIGN, HoleCheckMode::kElided);
}

void BytecodeGenerator::VisitThisFunctionVariable(Variable* variable) {
  if (variable == nullptr) return;

  // Store the closure we were called with in the given variable.
  builder()->LoadAccumulatorWithRegister(Register::function_closure());
  BuildVariableAssignment(variable, Token::INIT, HoleCheckMode::kElided);
}

void BytecodeGenerator::VisitNewTargetVariable(Variable* variable) {
  if (variable == nullptr) return;

  // The generator resume trampoline abuses the new.target register
  // to pass in the generator object.  In ordinary calls, new.target is always
  // undefined because generator functions are non-constructible, so don't
  // assign anything to the new.target variable.
  if (info()->literal()->CanSuspend()) return;

  if (variable->location() == VariableLocation::LOCAL) {
    // The new.target register was already assigned by entry trampoline.
    DCHECK_EQ(incoming_new_target_or_generator_.index(),
              GetRegisterForLocalVariable(variable).index());
    return;
  }

  // Store the new target we were called with in the given variable.
  builder()->LoadAccumulatorWithRegister(incoming_new_target_or_generator_);
  BuildVariableAssignment(variable, Token::INIT, HoleCheckMode::kElided);
}

void BytecodeGenerator::BuildGeneratorObjectVariableInitialization() {
  DCHECK(IsResumableFunction(info()->literal()->kind()));

  Variable* generator_object_var = closure_scope()->generator_object_var();
  RegisterAllocationScope register_scope(this);
  RegisterList args = register_allocator()->NewRegisterList(2);
  builder()
      ->MoveRegister(Register::function_closure(), args[0])
      .MoveRegister(builder()->Receiver(), args[1])
      .CallRuntime(Runtime::kInlineCreateJSGeneratorObject, args)
      .StoreAccumulatorInRegister(generator_object());

  if (generator_object_var->location() == VariableLocation::LOCAL) {
    // The generator object register is already set to the variable's local
    // register.
    DCHECK_EQ(generator_object().index(),
              GetRegisterForLocalVariable(generator_object_var).index());
  } else {
    BuildVariableAssignment(generator_object_var, Token::INIT,
                            HoleCheckMode::kElided);
  }
}

void BytecodeGenerator::BuildPushUndefinedIntoRegisterList(
    RegisterList* reg_list) {
  Register reg = register_allocator()->GrowRegisterList(reg_list);
  builder()->LoadUndefined().StoreAccumulatorInRegister(reg);
}

void BytecodeGenerator::BuildLoadPropertyKey(LiteralProperty* property,
                                             Register out_reg) {
  if (property->key()->IsStringLiteral()) {
    VisitForRegisterValue(property->key(), out_reg);
  } else {
    VisitForAccumulatorValue(property->key());
    builder()->ToName(out_reg);
  }
}

int BytecodeGenerator::AllocateBlockCoverageSlotIfEnabled(
    AstNode* node, SourceRangeKind kind) {
  return (block_coverage_builder_ == nullptr)
             ? BlockCoverageBuilder::kNoCoverageArraySlot
             : block_coverage_builder_->AllocateBlockCoverageSlot(node, kind);
}

int BytecodeGenerator::AllocateNaryBlockCoverageSlotIfEnabled(
    NaryOperation* node, size_t index) {
  return (block_coverage_builder_ == nullptr)
             ? BlockCoverageBuilder::kNoCoverageArraySlot
             : block_coverage_builder_->AllocateNaryBlockCoverageSlot(node,
                                                                      index);
}

void BytecodeGenerator::BuildIncrementBlockCoverageCounterIfEnabled(
    AstNode* node, SourceRangeKind kind) {
  if (block_coverage_builder_ == nullptr) return;
  block_coverage_builder_->IncrementBlockCounter(node, kind);
}

void BytecodeGenerator::BuildIncrementBlockCoverageCounterIfEnabled(
    int coverage_array_slot) {
  if (block_coverage_builder_ != nullptr) {
    block_coverage_builder_->IncrementBlockCounter(coverage_array_slot);
  }
}

// Visits the expression |expr| and places the result in the accumulator.
BytecodeGenerator::TypeHint BytecodeGenerator::VisitForAccumulatorValue(
    Expression* expr) {
  ValueResultScope accumulator_scope(this);
  Visit(expr);
  return accumulator_scope.type_hint();
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
  VisitForAccumulatorValue(expr);
  Register result = register_allocator()->NewRegister();
  builder()->StoreAccumulatorInRegister(result);
  return result;
}

// Visits the expression |expr| and stores the expression result in
// |destination|.
void BytecodeGenerator::VisitForRegisterValue(Expression* expr,
                                              Register destination) {
  ValueResultScope register_scope(this);
  Visit(expr);
  builder()->StoreAccumulatorInRegister(destination);
}

// Visits the expression |expr| and pushes the result into a new register
// added to the end of |reg_list|.
void BytecodeGenerator::VisitAndPushIntoRegisterList(Expression* expr,
                                                     RegisterList* reg_list) {
  {
    ValueResultScope register_scope(this);
    Visit(expr);
  }
  // Grow the register list after visiting the expression to avoid reserving
  // the register across the expression evaluation, which could cause memory
  // leaks for deep expressions due to dead objects being kept alive by pointers
  // in registers.
  Register destination = register_allocator()->GrowRegisterList(reg_list);
  builder()->StoreAccumulatorInRegister(destination);
}

void BytecodeGenerator::BuildTest(ToBooleanMode mode,
                                  BytecodeLabels* then_labels,
                                  BytecodeLabels* else_labels,
                                  TestFallthrough fallthrough) {
  switch (fallthrough) {
    case TestFallthrough::kThen:
      builder()->JumpIfFalse(mode, else_labels->New());
      break;
    case TestFallthrough::kElse:
      builder()->JumpIfTrue(mode, then_labels->New());
      break;
    case TestFallthrough::kNone:
      builder()->JumpIfTrue(mode, then_labels->New());
      builder()->Jump(else_labels->New());
      break;
  }
}

// Visits the expression |expr| for testing its boolean value and jumping to the
// |then| or |other| label depending on value and short-circuit semantics
void BytecodeGenerator::VisitForTest(Expression* expr,
                                     BytecodeLabels* then_labels,
                                     BytecodeLabels* else_labels,
                                     TestFallthrough fallthrough) {
  bool result_consumed;
  TypeHint type_hint;
  {
    // To make sure that all temporary registers are returned before generating
    // jumps below, we ensure that the result scope is deleted before doing so.
    // Dead registers might be materialized otherwise.
    TestResultScope test_result(this, then_labels, else_labels, fallthrough);
    Visit(expr);
    result_consumed = test_result.result_consumed_by_test();
    type_hint = test_result.type_hint();
    // Labels and fallthrough might have been mutated, so update based on
    // TestResultScope.
    then_labels = test_result.then_labels();
    else_labels = test_result.else_labels();
    fallthrough = test_result.fallthrough();
  }
  if (!result_consumed) {
    BuildTest(ToBooleanModeFromTypeHint(type_hint), then_labels, else_labels,
              fallthrough);
  }
}

void BytecodeGenerator::VisitInSameTestExecutionScope(Expression* expr) {
  DCHECK(execution_result()->IsTest());
  {
    RegisterAllocationScope reg_scope(this);
    Visit(expr);
  }
  if (!execution_result()->AsTest()->result_consumed_by_test()) {
    TestResultScope* result_scope = execution_result()->AsTest();
    BuildTest(ToBooleanModeFromTypeHint(result_scope->type_hint()),
              result_scope->then_labels(), result_scope->else_labels(),
              result_scope->fallthrough());
    result_scope->SetResultConsumedByTest();
  }
}

void BytecodeGenerator::VisitInScope(Statement* stmt, Scope* scope) {
  DCHECK(scope->declarations()->is_empty());
  CurrentScope current_scope(this, scope);
  ContextScope context_scope(this, scope);
  Visit(stmt);
}

Register BytecodeGenerator::GetRegisterForLocalVariable(Variable* variable) {
  DCHECK_EQ(VariableLocation::LOCAL, variable->location());
  return builder()->Local(variable->index());
}

FunctionKind BytecodeGenerator::function_kind() const {
  return info()->literal()->kind();
}

LanguageMode BytecodeGenerator::language_mode() const {
  return current_scope()->language_mode();
}

Register BytecodeGenerator::generator_object() const {
  DCHECK(info()->literal()->CanSuspend());
  return incoming_new_target_or_generator_;
}

FeedbackVectorSpec* BytecodeGenerator::feedback_spec() {
  return info()->feedback_vector_spec();
}

int BytecodeGenerator::feedback_index(FeedbackSlot slot) const {
  DCHECK(!slot.IsInvalid());
  return FeedbackVector::GetIndex(slot);
}

FeedbackSlot BytecodeGenerator::GetCachedLoadGlobalICSlot(
    TypeofMode typeof_mode, Variable* variable) {
  FeedbackSlotKind slot_kind =
      typeof_mode == INSIDE_TYPEOF
          ? FeedbackSlotKind::kLoadGlobalInsideTypeof
          : FeedbackSlotKind::kLoadGlobalNotInsideTypeof;
  FeedbackSlot slot = feedback_slot_cache()->Get(slot_kind, variable);
  if (!slot.IsInvalid()) {
    return slot;
  }
  slot = feedback_spec()->AddLoadGlobalICSlot(typeof_mode);
  feedback_slot_cache()->Put(slot_kind, variable, slot);
  return slot;
}

FeedbackSlot BytecodeGenerator::GetCachedStoreGlobalICSlot(
    LanguageMode language_mode, Variable* variable) {
  FeedbackSlotKind slot_kind = is_strict(language_mode)
                                   ? FeedbackSlotKind::kStoreGlobalStrict
                                   : FeedbackSlotKind::kStoreGlobalSloppy;
  FeedbackSlot slot = feedback_slot_cache()->Get(slot_kind, variable);
  if (!slot.IsInvalid()) {
    return slot;
  }
  slot = feedback_spec()->AddStoreGlobalICSlot(language_mode);
  feedback_slot_cache()->Put(slot_kind, variable, slot);
  return slot;
}

FeedbackSlot BytecodeGenerator::GetCachedLoadICSlot(const Expression* expr,
                                                    const AstRawString* name) {
  if (!FLAG_ignition_share_named_property_feedback) {
    return feedback_spec()->AddLoadICSlot();
  }
  FeedbackSlotKind slot_kind = FeedbackSlotKind::kLoadProperty;
  if (!expr->IsVariableProxy()) {
    return feedback_spec()->AddLoadICSlot();
  }
  const VariableProxy* proxy = expr->AsVariableProxy();
  FeedbackSlot slot =
      feedback_slot_cache()->Get(slot_kind, proxy->var()->index(), name);
  if (!slot.IsInvalid()) {
    return slot;
  }
  slot = feedback_spec()->AddLoadICSlot();
  feedback_slot_cache()->Put(slot_kind, proxy->var()->index(), name, slot);
  return slot;
}

FeedbackSlot BytecodeGenerator::GetCachedStoreICSlot(const Expression* expr,
                                                     const AstRawString* name) {
  if (!FLAG_ignition_share_named_property_feedback) {
    return feedback_spec()->AddStoreICSlot(language_mode());
  }
  FeedbackSlotKind slot_kind = is_strict(language_mode())
                                   ? FeedbackSlotKind::kStoreNamedStrict
                                   : FeedbackSlotKind::kStoreNamedSloppy;
  if (!expr->IsVariableProxy()) {
    return feedback_spec()->AddStoreICSlot(language_mode());
  }
  const VariableProxy* proxy = expr->AsVariableProxy();
  FeedbackSlot slot =
      feedback_slot_cache()->Get(slot_kind, proxy->var()->index(), name);
  if (!slot.IsInvalid()) {
    return slot;
  }
  slot = feedback_spec()->AddStoreICSlot(language_mode());
  feedback_slot_cache()->Put(slot_kind, proxy->var()->index(), name, slot);
  return slot;
}

FeedbackSlot BytecodeGenerator::GetCachedCreateClosureSlot(
    FunctionLiteral* literal) {
  FeedbackSlotKind slot_kind = FeedbackSlotKind::kCreateClosure;
  FeedbackSlot slot = feedback_slot_cache()->Get(slot_kind, literal);
  if (!slot.IsInvalid()) {
    return slot;
  }
  slot = feedback_spec()->AddCreateClosureSlot();
  feedback_slot_cache()->Put(slot_kind, literal, slot);
  return slot;
}

FeedbackSlot BytecodeGenerator::GetDummyCompareICSlot() {
  return dummy_feedback_slot_.Get();
}

Runtime::FunctionId BytecodeGenerator::StoreToSuperRuntimeId() {
  return is_strict(language_mode()) ? Runtime::kStoreToSuper_Strict
                                    : Runtime::kStoreToSuper_Sloppy;
}

Runtime::FunctionId BytecodeGenerator::StoreKeyedToSuperRuntimeId() {
  return is_strict(language_mode()) ? Runtime::kStoreKeyedToSuper_Strict
                                    : Runtime::kStoreKeyedToSuper_Sloppy;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

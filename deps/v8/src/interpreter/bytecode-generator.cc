// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-generator.h"

#include "src/ast/compile-time-value.h"
#include "src/ast/scopes.h"
#include "src/builtins/builtins-constructor.h"
#include "src/code-stubs.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "src/interpreter/control-flow-builders.h"
#include "src/objects.h"
#include "src/parsing/parse-info.h"
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
  bool ShouldPopContext() { return should_pop_context_; }

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
  void AsyncReturnAccumulator() { PerformCommand(CMD_ASYNC_RETURN, nullptr); }
  void ReThrowAccumulator() { PerformCommand(CMD_RETHROW, nullptr); }

  class DeferredCommands;

 protected:
  enum Command {
    CMD_BREAK,
    CMD_CONTINUE,
    CMD_RETURN,
    CMD_ASYNC_RETURN,
    CMD_RETHROW
  };
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
      case CMD_BREAK:  // We should never see break/continue in top-level.
      case CMD_CONTINUE:
        UNREACHABLE();
      case CMD_RETURN:
        generator()->BuildReturn();
        return true;
      case CMD_ASYNC_RETURN:
        generator()->BuildAsyncReturn();
        return true;
      case CMD_RETHROW:
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
  bool Execute(Command command, Statement* statement) override {
    if (statement != statement_) return false;
    switch (command) {
      case CMD_BREAK:
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
  ~ControlScopeForIteration() { generator()->loop_depth_--; }

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
  bool Execute(Command command, Statement* statement) override {
    switch (command) {
      case CMD_BREAK:
      case CMD_CONTINUE:
      case CMD_RETURN:
      case CMD_ASYNC_RETURN:
        break;
      case CMD_RETHROW:
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
  bool Execute(Command command, Statement* statement) override {
    switch (command) {
      case CMD_BREAK:
      case CMD_CONTINUE:
      case CMD_RETURN:
      case CMD_ASYNC_RETURN:
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
  ContextScope* context = generator()->execution_context();
  // Pop context to the expected depth but do not pop the outermost context.
  if (context != current->context() && context->ShouldPopContext()) {
    generator()->builder()->PopContext(current->context()->reg());
  }
  do {
    if (current->Execute(command, statement)) {
      return;
    }
    current = current->outer();
    if (current->context() != context && context->ShouldPopContext()) {
      // Pop context to the expected depth.
      // TODO(rmcilroy): Only emit a single context pop.
      generator()->builder()->PopContext(current->context()->reg());
    }
  } while (current != nullptr);
  UNREACHABLE();
}

class BytecodeGenerator::RegisterAllocationScope {
 public:
  explicit RegisterAllocationScope(BytecodeGenerator* generator)
      : generator_(generator),
        outer_next_register_index_(
            generator->register_allocator()->next_register_index()) {}

  virtual ~RegisterAllocationScope() {
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
        kind_(kind),
        outer_(generator->execution_result()),
        allocator_(generator) {
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

 private:
  BytecodeGenerator* generator_;
  Expression::Context kind_;
  ExpressionResultScope* outer_;
  RegisterAllocationScope allocator_;

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
        then_labels_(then_labels),
        else_labels_(else_labels),
        fallthrough_(fallthrough),
        result_consumed_by_test_(false) {}

  // Used when code special cases for TestResultScope and consumes any
  // possible value by testing and jumping to a then/else label.
  void SetResultConsumedByTest() {
    result_consumed_by_test_ = true;
  }

  bool ResultConsumedByTest() { return result_consumed_by_test_; }

  BytecodeLabel* NewThenLabel() { return then_labels_->New(); }
  BytecodeLabel* NewElseLabel() { return else_labels_->New(); }

  BytecodeLabels* then_labels() const { return then_labels_; }
  BytecodeLabels* else_labels() const { return else_labels_; }

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

 private:
  BytecodeLabels* then_labels_;
  BytecodeLabels* else_labels_;
  TestFallthrough fallthrough_;
  bool result_consumed_by_test_;

  DISALLOW_COPY_AND_ASSIGN(TestResultScope);
};

// Used to build a list of global declaration initial value pairs.
class BytecodeGenerator::GlobalDeclarationsBuilder final : public ZoneObject {
 public:
  explicit GlobalDeclarationsBuilder(Zone* zone)
      : declarations_(0, zone),
        constant_pool_entry_(0),
        has_constant_pool_entry_(false) {}

  void AddFunctionDeclaration(Handle<String> name, FeedbackVectorSlot slot,
                              FunctionLiteral* func) {
    DCHECK(!slot.IsInvalid());
    declarations_.push_back(Declaration(name, slot, func));
  }

  void AddUndefinedDeclaration(Handle<String> name, FeedbackVectorSlot slot) {
    DCHECK(!slot.IsInvalid());
    declarations_.push_back(Declaration(name, slot, nullptr));
  }

  Handle<FixedArray> AllocateDeclarations(CompilationInfo* info) {
    DCHECK(has_constant_pool_entry_);
    int array_index = 0;
    Handle<FixedArray> data = info->isolate()->factory()->NewFixedArray(
        static_cast<int>(declarations_.size() * 3), TENURED);
    for (const Declaration& declaration : declarations_) {
      FunctionLiteral* func = declaration.func;
      Handle<Object> initial_value;
      if (func == nullptr) {
        initial_value = info->isolate()->factory()->undefined_value();
      } else {
        initial_value =
            Compiler::GetSharedFunctionInfo(func, info->script(), info);
      }

      // Return a null handle if any initial values can't be created. Caller
      // will set stack overflow.
      if (initial_value.is_null()) return Handle<FixedArray>();

      data->set(array_index++, *declaration.name);
      data->set(array_index++, Smi::FromInt(declaration.slot.ToInt()));
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
    Declaration() : slot(FeedbackVectorSlot::Invalid()), func(nullptr) {}
    Declaration(Handle<String> name, FeedbackVectorSlot slot,
                FunctionLiteral* func)
        : name(name), slot(slot), func(func) {}

    Handle<String> name;
    FeedbackVectorSlot slot;
    FunctionLiteral* func;
  };
  ZoneVector<Declaration> declarations_;
  size_t constant_pool_entry_;
  bool has_constant_pool_entry_;
};

BytecodeGenerator::BytecodeGenerator(CompilationInfo* info)
    : zone_(info->zone()),
      builder_(new (zone()) BytecodeArrayBuilder(
          info->isolate(), info->zone(), info->num_parameters_including_this(),
          info->scope()->MaxNestedContextChainLength(),
          info->scope()->num_stack_slots(), info->literal(),
          info->SourcePositionRecordingMode())),
      info_(info),
      scope_(info->scope()),
      globals_builder_(new (zone()) GlobalDeclarationsBuilder(info->zone())),
      global_declarations_(0, info->zone()),
      function_literals_(0, info->zone()),
      native_function_literals_(0, info->zone()),
      object_literals_(0, info->zone()),
      array_literals_(0, info->zone()),
      execution_control_(nullptr),
      execution_context_(nullptr),
      execution_result_(nullptr),
      generator_resume_points_(info->literal()->yield_count(), info->zone()),
      generator_state_(),
      loop_depth_(0),
      home_object_symbol_(info->isolate()->factory()->home_object_symbol()),
      iterator_symbol_(info->isolate()->factory()->iterator_symbol()),
      prototype_string_(info->isolate()->factory()->prototype_string()),
      empty_fixed_array_(info->isolate()->factory()->empty_fixed_array()),
      undefined_string_(
          info->isolate()->ast_string_constants()->undefined_string()) {}

Handle<BytecodeArray> BytecodeGenerator::FinalizeBytecode(Isolate* isolate) {
  AllocateDeferredConstants(isolate);
  if (HasStackOverflow()) return Handle<BytecodeArray>();
  return builder()->ToBytecodeArray(isolate);
}

void BytecodeGenerator::AllocateDeferredConstants(Isolate* isolate) {
  // Build global declaration pair arrays.
  for (GlobalDeclarationsBuilder* globals_builder : global_declarations_) {
    Handle<FixedArray> declarations =
        globals_builder->AllocateDeclarations(info());
    if (declarations.is_null()) return SetStackOverflow();
    builder()->InsertConstantPoolEntryAt(globals_builder->constant_pool_entry(),
                                         declarations);
  }

  // Find or build shared function infos.
  for (std::pair<FunctionLiteral*, size_t> literal : function_literals_) {
    FunctionLiteral* expr = literal.first;
    Handle<SharedFunctionInfo> shared_info =
        Compiler::GetSharedFunctionInfo(expr, info()->script(), info());
    if (shared_info.is_null()) return SetStackOverflow();
    builder()->InsertConstantPoolEntryAt(literal.second, shared_info);
  }

  // Find or build shared function infos for the native function templates.
  for (std::pair<NativeFunctionLiteral*, size_t> literal :
       native_function_literals_) {
    NativeFunctionLiteral* expr = literal.first;
    Handle<SharedFunctionInfo> shared_info =
        Compiler::GetSharedFunctionInfoForNative(expr->extension(),
                                                 expr->name());
    if (shared_info.is_null()) return SetStackOverflow();
    builder()->InsertConstantPoolEntryAt(literal.second, shared_info);
  }

  // Build object literal constant properties
  for (std::pair<ObjectLiteral*, size_t> literal : object_literals_) {
    ObjectLiteral* object_literal = literal.first;
    if (object_literal->properties_count() > 0) {
      // If constant properties is an empty fixed array, we've already added it
      // to the constant pool when visiting the object literal.
      Handle<FixedArray> constant_properties =
          object_literal->GetOrBuildConstantProperties(isolate);

      builder()->InsertConstantPoolEntryAt(literal.second, constant_properties);
    }
  }

  // Build array literal constant elements
  for (std::pair<ArrayLiteral*, size_t> literal : array_literals_) {
    ArrayLiteral* array_literal = literal.first;
    Handle<ConstantElementsPair> constant_elements =
        array_literal->GetOrBuildConstantElements(isolate);
    builder()->InsertConstantPoolEntryAt(literal.second, constant_elements);
  }
}

void BytecodeGenerator::GenerateBytecode(uintptr_t stack_limit) {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  InitializeAstVisitor(stack_limit);

  // Initialize the incoming context.
  ContextScope incoming_context(this, scope(), false);

  // Initialize control scope.
  ControlScopeForTopLevel control(this);

  RegisterAllocationScope register_scope(this);

  if (IsResumableFunction(info()->literal()->kind())) {
    generator_state_ = register_allocator()->NewRegister();
    VisitGeneratorPrologue();
  }

  if (scope()->NeedsContext()) {
    // Push a new inner context scope for the function.
    BuildNewLocalActivationContext();
    ContextScope local_function_context(this, scope(), false);
    BuildLocalActivationContextInitialization();
    GenerateBytecodeBody();
  } else {
    GenerateBytecodeBody();
  }

  // In generator functions, we may not have visited every yield in the AST
  // since we skip some obviously dead code. Hence the generated bytecode may
  // contain jumps to unbound labels (resume points that will never be used).
  // We bind these now.
  for (auto& label : generator_resume_points_) {
    if (!label.is_bound()) builder()->Bind(&label);
  }

  // Emit an implicit return instruction in case control flow can fall off the
  // end of the function without an explicit return being present on all paths.
  if (builder()->RequiresImplicitReturn()) {
    builder()->LoadUndefined();
    BuildReturn();
  }
  DCHECK(!builder()->RequiresImplicitReturn());
}

void BytecodeGenerator::GenerateBytecodeBody() {
  // Build the arguments object if it is used.
  VisitArgumentsObject(scope()->arguments());

  // Build rest arguments array if it is used.
  Variable* rest_parameter = scope()->rest_parameter();
  VisitRestArgumentsArray(rest_parameter);

  // Build assignment to {.this_function} variable if it is used.
  VisitThisFunctionVariable(scope()->this_function_var());

  // Build assignment to {new.target} variable if it is used.
  VisitNewTargetVariable(scope()->new_target_var());

  // Emit tracing call if requested to do so.
  if (FLAG_trace) builder()->CallRuntime(Runtime::kTraceEnter);

  // Visit declarations within the function scope.
  VisitDeclarations(scope()->declarations());

  // Emit initializing assignments for module namespace imports (if any).
  VisitModuleNamespaceImports();

  // Perform a stack-check before the body.
  builder()->StackCheck(info()->literal()->start_position());

  // Visit statements in the function body.
  VisitStatements(info()->literal()->body());
}

void BytecodeGenerator::BuildIndexedJump(Register index, size_t start_index,
                                         size_t size,
                                         ZoneVector<BytecodeLabel>& targets) {
  // TODO(neis): Optimize this by using a proper jump table.
  DCHECK_LE(start_index + size, targets.size());
  for (size_t i = start_index; i < start_index + size; i++) {
    builder()
        ->LoadLiteral(Smi::FromInt(static_cast<int>(i)))
        .CompareOperation(Token::Value::EQ_STRICT, index)
        .JumpIfTrue(&(targets[i]));
  }
  BuildAbort(BailoutReason::kInvalidJumpTableIndex);
}

void BytecodeGenerator::VisitIterationHeader(IterationStatement* stmt,
                                             LoopBuilder* loop_builder) {
  // Recall that stmt->yield_count() is always zero inside ordinary
  // (i.e. non-generator) functions.
  if (stmt->yield_count() == 0) {
    loop_builder->LoopHeader();
  } else {
    // Collect all labels for generator resume points within the loop (if any)
    // so that they can be bound to the loop header below. Also create fresh
    // labels for these resume points, to be used inside the loop.
    ZoneVector<BytecodeLabel> resume_points_in_loop(zone());
    size_t first_yield = stmt->first_yield_id();
    DCHECK_LE(first_yield + stmt->yield_count(),
              generator_resume_points_.size());
    for (size_t id = first_yield; id < first_yield + stmt->yield_count();
         id++) {
      auto& label = generator_resume_points_[id];
      resume_points_in_loop.push_back(label);
      generator_resume_points_[id] = BytecodeLabel();
    }

    loop_builder->LoopHeader(&resume_points_in_loop);

    // If we are not resuming, fall through to loop body.
    // If we are resuming, perform state dispatch.
    BytecodeLabel not_resuming;
    builder()
        ->LoadLiteral(Smi::FromInt(JSGeneratorObject::kGeneratorExecuting))
        .CompareOperation(Token::Value::EQ, generator_state_)
        .JumpIfTrue(&not_resuming);
    BuildIndexedJump(generator_state_, first_yield,
        stmt->yield_count(), generator_resume_points_);
    builder()->Bind(&not_resuming);
  }
}

void BytecodeGenerator::VisitGeneratorPrologue() {
  // The generator resume trampoline abuses the new.target register both to
  // indicate that this is a resume call and to pass in the generator object.
  // In ordinary calls, new.target is always undefined because generator
  // functions are non-constructable.
  Register generator_object = Register::new_target();
  BytecodeLabel regular_call;
  builder()
      ->LoadAccumulatorWithRegister(generator_object)
      .JumpIfUndefined(&regular_call);

  // This is a resume call. Restore the current context and the registers, then
  // perform state dispatch.
  Register dummy = register_allocator()->NewRegister();
  builder()
      ->CallRuntime(Runtime::kInlineGeneratorGetContext, generator_object)
      .PushContext(dummy)
      .ResumeGenerator(generator_object)
      .StoreAccumulatorInRegister(generator_state_);
  BuildIndexedJump(generator_state_, 0, generator_resume_points_.size(),
                   generator_resume_points_);

  builder()
      ->Bind(&regular_call)
      .LoadLiteral(Smi::FromInt(JSGeneratorObject::kGeneratorExecuting))
      .StoreAccumulatorInRegister(generator_state_);
  // This is a regular call. Fall through to the ordinary function prologue,
  // after which we will run into the generator object creation and other extra
  // code inserted by the parser.
}

void BytecodeGenerator::VisitBlock(Block* stmt) {
  // Visit declarations and statements.
  if (stmt->scope() != nullptr && stmt->scope()->NeedsContext()) {
    BuildNewLocalBlockContext(stmt->scope());
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
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      DCHECK(!variable->binding_needs_init());
      FeedbackVectorSlot slot = decl->proxy()->VariableFeedbackSlot();
      globals_builder()->AddUndefinedDeclaration(variable->name(), slot);
      break;
    }
    case VariableLocation::LOCAL:
      if (variable->binding_needs_init()) {
        Register destination(variable->index());
        builder()->LoadTheHole().StoreAccumulatorInRegister(destination);
      }
      break;
    case VariableLocation::PARAMETER:
      if (variable->binding_needs_init()) {
        // The parameter indices are shifted by 1 (receiver is variable
        // index -1 but is parameter index 0 in BytecodeArrayBuilder).
        Register destination(builder()->Parameter(variable->index() + 1));
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
      DCHECK_EQ(VAR, variable->mode());
      DCHECK(!variable->binding_needs_init());

      Register name = register_allocator()->NewRegister();

      builder()
          ->LoadLiteral(variable->name())
          .StoreAccumulatorInRegister(name)
          .CallRuntime(Runtime::kDeclareEvalVar, name);
      break;
    }
    case VariableLocation::MODULE:
      if (variable->IsExport() && variable->binding_needs_init()) {
        builder()->LoadTheHole();
        BuildVariableAssignment(variable, Token::INIT,
                                FeedbackVectorSlot::Invalid(),
                                HoleCheckMode::kElided);
      }
      // Nothing to do for imports.
      break;
  }
}

void BytecodeGenerator::VisitFunctionDeclaration(FunctionDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  DCHECK(variable->mode() == LET || variable->mode() == VAR);
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      FeedbackVectorSlot slot = decl->proxy()->VariableFeedbackSlot();
      globals_builder()->AddFunctionDeclaration(variable->name(), slot,
                                                decl->fun());
      break;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      VisitForAccumulatorValue(decl->fun());
      BuildVariableAssignment(variable, Token::INIT,
                              FeedbackVectorSlot::Invalid(),
                              HoleCheckMode::kElided);
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
          ->LoadLiteral(variable->name())
          .StoreAccumulatorInRegister(args[0]);
      VisitForAccumulatorValue(decl->fun());
      builder()->StoreAccumulatorInRegister(args[1]).CallRuntime(
          Runtime::kDeclareEvalFunction, args);
      break;
    }
    case VariableLocation::MODULE:
      DCHECK_EQ(variable->mode(), LET);
      DCHECK(variable->IsExport());
      VisitForAccumulatorValue(decl->fun());
      BuildVariableAssignment(variable, Token::INIT,
                              FeedbackVectorSlot::Invalid(),
                              HoleCheckMode::kElided);
      break;
  }
}

void BytecodeGenerator::VisitModuleNamespaceImports() {
  if (!scope()->is_module_scope()) return;

  RegisterAllocationScope register_scope(this);
  Register module_request = register_allocator()->NewRegister();

  ModuleDescriptor* descriptor = scope()->AsModuleScope()->module();
  for (auto entry : descriptor->namespace_imports()) {
    builder()
        ->LoadLiteral(Smi::FromInt(entry->module_request))
        .StoreAccumulatorInRegister(module_request)
        .CallRuntime(Runtime::kGetModuleNamespace, module_request);
    Variable* var = scope()->LookupLocal(entry->local_name);
    DCHECK_NOT_NULL(var);
    BuildVariableAssignment(var, Token::INIT, FeedbackVectorSlot::Invalid(),
                            HoleCheckMode::kElided);
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
      builder()->AllocateConstantPoolEntry());
  int encoded_flags = info()->GetDeclareGlobalsFlags();

  // Emit code to declare globals.
  RegisterList args = register_allocator()->NewRegisterList(3);
  builder()
      ->LoadConstantPoolEntry(globals_builder()->constant_pool_entry())
      .StoreAccumulatorInRegister(args[0])
      .LoadLiteral(Smi::FromInt(encoded_flags))
      .StoreAccumulatorInRegister(args[1])
      .MoveRegister(Register::function_closure(), args[2])
      .CallRuntime(Runtime::kDeclareGlobalsForInterpreter, args);

  // Push and reset globals builder.
  global_declarations_.push_back(globals_builder());
  globals_builder_ = new (zone()) GlobalDeclarationsBuilder(zone());
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
  builder()->SetStatementPosition(stmt);
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
    BytecodeLabel end_label;
    BytecodeLabels then_labels(zone()), else_labels(zone());
    VisitForTest(stmt->condition(), &then_labels, &else_labels,
                 TestFallthrough::kThen);

    then_labels.Bind(builder());
    Visit(stmt->then_statement());

    if (stmt->HasElseStatement()) {
      builder()->Jump(&end_label);
      else_labels.Bind(builder());
      Visit(stmt->else_statement());
    } else {
      else_labels.Bind(builder());
    }
    builder()->Bind(&end_label);
  }
}

void BytecodeGenerator::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* stmt) {
  Visit(stmt->statement());
}

void BytecodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  builder()->SetStatementPosition(stmt);
  execution_control()->Continue(stmt->target());
}

void BytecodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  builder()->SetStatementPosition(stmt);
  execution_control()->Break(stmt->target());
}

void BytecodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  builder()->SetStatementPosition(stmt);
  VisitForAccumulatorValue(stmt->expression());

  if (stmt->is_async_return()) {
    execution_control()->AsyncReturnAccumulator();
  } else {
    execution_control()->ReturnAccumulator();
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
  ZoneList<CaseClause*>* clauses = stmt->cases();
  SwitchBuilder switch_builder(builder(), clauses->length());
  ControlScopeForBreakable scope(this, stmt, &switch_builder);
  int default_index = -1;

  builder()->SetStatementPosition(stmt);

  // Keep the switch value in a register until a case matches.
  Register tag = VisitForRegisterValue(stmt->tag());

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
    builder()->CompareOperation(
        Token::Value::EQ_STRICT, tag,
        feedback_index(clause->CompareOperationFeedbackSlot()));
    switch_builder.Case(i);
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
    switch_builder.SetCaseTarget(i);
    VisitStatements(clause->statements());
  }
  switch_builder.BindBreakTarget();
}

void BytecodeGenerator::VisitCaseClause(CaseClause* clause) {
  // Handled entirely in VisitSwitchStatement.
  UNREACHABLE();
}

void BytecodeGenerator::VisitIterationBody(IterationStatement* stmt,
                                           LoopBuilder* loop_builder) {
  ControlScopeForIteration execution_control(this, stmt, loop_builder);
  builder()->StackCheck(stmt->position());
  Visit(stmt->body());
  loop_builder->BindContinueTarget();
}

void BytecodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  LoopBuilder loop_builder(builder());
  if (stmt->cond()->ToBooleanIsFalse()) {
    VisitIterationBody(stmt, &loop_builder);
  } else if (stmt->cond()->ToBooleanIsTrue()) {
    VisitIterationHeader(stmt, &loop_builder);
    VisitIterationBody(stmt, &loop_builder);
    loop_builder.JumpToHeader(loop_depth_);
  } else {
    VisitIterationHeader(stmt, &loop_builder);
    VisitIterationBody(stmt, &loop_builder);
    builder()->SetExpressionAsStatementPosition(stmt->cond());
    BytecodeLabels loop_backbranch(zone());
    VisitForTest(stmt->cond(), &loop_backbranch, loop_builder.break_labels(),
                 TestFallthrough::kThen);
    loop_backbranch.Bind(builder());
    loop_builder.JumpToHeader(loop_depth_);
  }
  loop_builder.EndLoop();
}

void BytecodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  if (stmt->cond()->ToBooleanIsFalse()) {
    // If the condition is false there is no need to generate the loop.
    return;
  }

  LoopBuilder loop_builder(builder());
  VisitIterationHeader(stmt, &loop_builder);
  if (!stmt->cond()->ToBooleanIsTrue()) {
    builder()->SetExpressionAsStatementPosition(stmt->cond());
    BytecodeLabels loop_body(zone());
    VisitForTest(stmt->cond(), &loop_body, loop_builder.break_labels(),
                 TestFallthrough::kThen);
    loop_body.Bind(builder());
  }
  VisitIterationBody(stmt, &loop_builder);
  loop_builder.JumpToHeader(loop_depth_);
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
  VisitIterationHeader(stmt, &loop_builder);
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
      VariableProxy* proxy = expr->AsVariableProxy();
      BuildVariableAssignment(proxy->var(), Token::ASSIGN, slot,
                              proxy->hole_check_mode());
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
      RegisterList args = register_allocator()->NewRegisterList(4);
      builder()->StoreAccumulatorInRegister(args[3]);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), args[0]);
      VisitForRegisterValue(super_property->home_object(), args[1]);
      builder()
          ->LoadLiteral(property->key()->AsLiteral()->AsPropertyName())
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

  LoopBuilder loop_builder(builder());
  BytecodeLabel subject_null_label, subject_undefined_label;

  // Prepare the state for executing ForIn.
  builder()->SetExpressionAsStatementPosition(stmt->subject());
  VisitForAccumulatorValue(stmt->subject());
  builder()->JumpIfUndefined(&subject_undefined_label);
  builder()->JumpIfNull(&subject_null_label);
  Register receiver = register_allocator()->NewRegister();
  builder()->ConvertAccumulatorToObject(receiver);

  // Used as kRegTriple and kRegPair in ForInPrepare and ForInNext.
  RegisterList triple = register_allocator()->NewRegisterList(3);
  Register cache_length = triple[2];
  builder()->ForInPrepare(receiver, triple);

  // Set up loop counter
  Register index = register_allocator()->NewRegister();
  builder()->LoadLiteral(Smi::kZero);
  builder()->StoreAccumulatorInRegister(index);

  // The loop
  VisitIterationHeader(stmt, &loop_builder);
  builder()->SetExpressionAsStatementPosition(stmt->each());
  builder()->ForInContinue(index, cache_length);
  loop_builder.BreakIfFalse();
  FeedbackVectorSlot slot = stmt->ForInFeedbackSlot();
  builder()->ForInNext(receiver, index, triple.Truncate(2),
                       feedback_index(slot));
  loop_builder.ContinueIfUndefined();
  VisitForInAssignment(stmt->each(), stmt->EachFeedbackSlot());
  VisitIterationBody(stmt, &loop_builder);
  builder()->ForInStep(index);
  builder()->StoreAccumulatorInRegister(index);
  loop_builder.JumpToHeader(loop_depth_);
  loop_builder.EndLoop();
  builder()->Bind(&subject_null_label);
  builder()->Bind(&subject_undefined_label);
}

void BytecodeGenerator::VisitForOfStatement(ForOfStatement* stmt) {
  LoopBuilder loop_builder(builder());

  builder()->SetExpressionAsStatementPosition(stmt->assign_iterator());
  VisitForEffect(stmt->assign_iterator());

  VisitIterationHeader(stmt, &loop_builder);
  builder()->SetExpressionAsStatementPosition(stmt->next_result());
  VisitForEffect(stmt->next_result());
  VisitForAccumulatorValue(stmt->result_done());
  loop_builder.BreakIfTrue();

  VisitForEffect(stmt->assign_each());
  VisitIterationBody(stmt, &loop_builder);
  loop_builder.JumpToHeader(loop_depth_);
  loop_builder.EndLoop();
}

void BytecodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  TryCatchBuilder try_control_builder(builder(), stmt->catch_prediction());

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
  BuildNewLocalCatchContext(stmt->variable(), stmt->scope());
  builder()->StoreAccumulatorInRegister(context);

  // If requested, clear message object as we enter the catch block.
  if (stmt->clear_pending_message()) {
    builder()->LoadTheHole().SetPendingMessage();
  }

  // Load the catch context into the accumulator.
  builder()->LoadAccumulatorWithRegister(context);

  // Evaluate the catch-block.
  VisitInScope(stmt->catch_block(), stmt->scope());
  try_control_builder.EndCatch();
}

void BytecodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  TryFinallyBuilder try_control_builder(builder(), stmt->catch_prediction());

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
  uint8_t flags = CreateClosureFlags::Encode(expr->pretenure(),
                                             scope()->is_function_scope());
  size_t entry = builder()->AllocateConstantPoolEntry();
  int slot_index = feedback_index(expr->LiteralFeedbackSlot());
  builder()->CreateClosure(entry, slot_index, flags);
  function_literals_.push_back(std::make_pair(expr, entry));
}

void BytecodeGenerator::VisitClassLiteral(ClassLiteral* expr) {
  Register constructor = VisitForRegisterValue(expr->constructor());
  {
    RegisterAllocationScope register_scope(this);
    RegisterList args = register_allocator()->NewRegisterList(4);
    VisitForAccumulatorValueOrTheHole(expr->extends());
    builder()
        ->StoreAccumulatorInRegister(args[0])
        .MoveRegister(constructor, args[1])
        .LoadLiteral(Smi::FromInt(expr->start_position()))
        .StoreAccumulatorInRegister(args[2])
        .LoadLiteral(Smi::FromInt(expr->end_position()))
        .StoreAccumulatorInRegister(args[3])
        .CallRuntime(Runtime::kDefineClass, args);
  }
  Register prototype = register_allocator()->NewRegister();
  builder()->StoreAccumulatorInRegister(prototype);

  if (FunctionLiteral::NeedsHomeObject(expr->constructor())) {
    // Prototype is already in the accumulator.
    builder()->StoreNamedProperty(constructor, home_object_symbol(),
                                  feedback_index(expr->HomeObjectSlot()),
                                  language_mode());
  }

  VisitClassLiteralProperties(expr, constructor, prototype);
  BuildClassLiteralNameProperty(expr, constructor);
  builder()->CallRuntime(Runtime::kToFastProperties, constructor);
  // Assign to class variable.
  if (expr->class_variable_proxy() != nullptr) {
    VariableProxy* proxy = expr->class_variable_proxy();
    FeedbackVectorSlot slot = expr->NeedsProxySlot()
                                  ? expr->ProxySlot()
                                  : FeedbackVectorSlot::Invalid();
    BuildVariableAssignment(proxy->var(), Token::INIT, slot,
                            HoleCheckMode::kElided);
  }
}

void BytecodeGenerator::VisitClassLiteralProperties(ClassLiteral* expr,
                                                    Register constructor,
                                                    Register prototype) {
  RegisterAllocationScope register_scope(this);
  RegisterList args = register_allocator()->NewRegisterList(4);
  Register receiver = args[0], key = args[1], value = args[2], attr = args[3];

  bool attr_assigned = false;
  Register old_receiver = Register::invalid_value();

  // Create nodes to store method values into the literal.
  for (int i = 0; i < expr->properties()->length(); i++) {
    ClassLiteral::Property* property = expr->properties()->at(i);

    // Set-up receiver.
    Register new_receiver = property->is_static() ? constructor : prototype;
    if (new_receiver != old_receiver) {
      builder()->MoveRegister(new_receiver, receiver);
      old_receiver = new_receiver;
    }

    if (property->key()->IsStringLiteral()) {
      VisitForRegisterValue(property->key(), key);
    } else {
      VisitForAccumulatorValue(property->key());
      builder()->ConvertAccumulatorToName(key);
    }

    if (property->is_static() && property->is_computed_name()) {
      // The static prototype property is read only. We handle the non computed
      // property name case in the parser. Since this is the only case where we
      // need to check for an own read only property we special case this so we
      // do not need to do this for every property.
      BytecodeLabel done;
      builder()
          ->LoadLiteral(prototype_string())
          .CompareOperation(Token::Value::EQ_STRICT, key)
          .JumpIfFalse(&done)
          .CallRuntime(Runtime::kThrowStaticPrototypeError)
          .Bind(&done);
    }

    VisitForRegisterValue(property->value(), value);
    VisitSetHomeObject(value, receiver, property);

    if (!attr_assigned) {
      builder()
          ->LoadLiteral(Smi::FromInt(DONT_ENUM))
          .StoreAccumulatorInRegister(attr);
      attr_assigned = true;
    }

    switch (property->kind()) {
      case ClassLiteral::Property::METHOD: {
        DataPropertyInLiteralFlags flags = DataPropertyInLiteralFlag::kDontEnum;
        if (property->NeedsSetFunctionName()) {
          flags |= DataPropertyInLiteralFlag::kSetFunctionName;
        }

        FeedbackVectorSlot slot = property->GetStoreDataPropertySlot();
        DCHECK(!slot.IsInvalid());

        builder()
            ->LoadAccumulatorWithRegister(value)
            .StoreDataPropertyInLiteral(receiver, key, flags,
                                        feedback_index(slot));
        break;
      }
      case ClassLiteral::Property::GETTER: {
        builder()->CallRuntime(Runtime::kDefineGetterPropertyUnchecked, args);
        break;
      }
      case ClassLiteral::Property::SETTER: {
        builder()->CallRuntime(Runtime::kDefineSetterPropertyUnchecked, args);
        break;
      }
      case ClassLiteral::Property::FIELD: {
        UNREACHABLE();
        break;
      }
    }
  }
}

void BytecodeGenerator::BuildClassLiteralNameProperty(ClassLiteral* expr,
                                                      Register literal) {
  if (!expr->has_name_static_property() &&
      !expr->constructor()->raw_name()->IsEmpty()) {
    Runtime::FunctionId runtime_id =
        expr->has_static_computed_names()
            ? Runtime::kInstallClassNameAccessorWithCheck
            : Runtime::kInstallClassNameAccessor;
    builder()->CallRuntime(runtime_id, literal);
  }
}

void BytecodeGenerator::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
  size_t entry = builder()->AllocateConstantPoolEntry();
  int slot_index = feedback_index(expr->LiteralFeedbackSlot());
  builder()->CreateClosure(entry, slot_index, NOT_TENURED);
  native_function_literals_.push_back(std::make_pair(expr, entry));
}

void BytecodeGenerator::VisitDoExpression(DoExpression* expr) {
  VisitBlock(expr->block());
  VisitVariableProxy(expr->result());
}

void BytecodeGenerator::VisitConditional(Conditional* expr) {
  if (expr->condition()->ToBooleanIsTrue()) {
    // Generate then block unconditionally as always true.
    VisitForAccumulatorValue(expr->then_expression());
  } else if (expr->condition()->ToBooleanIsFalse()) {
    // Generate else block unconditionally if it exists.
    VisitForAccumulatorValue(expr->else_expression());
  } else {
    BytecodeLabel end_label;
    BytecodeLabels then_labels(zone()), else_labels(zone());

    VisitForTest(expr->condition(), &then_labels, &else_labels,
                 TestFallthrough::kThen);

    then_labels.Bind(builder());
    VisitForAccumulatorValue(expr->then_expression());
    builder()->Jump(&end_label);

    else_labels.Bind(builder());
    VisitForAccumulatorValue(expr->else_expression());
    builder()->Bind(&end_label);
  }
}

void BytecodeGenerator::VisitLiteral(Literal* expr) {
  if (!execution_result()->IsEffect()) {
    const AstValue* raw_value = expr->raw_value();
    if (raw_value->IsSmi()) {
      builder()->LoadLiteral(raw_value->AsSmi());
    } else if (raw_value->IsUndefined()) {
      builder()->LoadUndefined();
    } else if (raw_value->IsTrue()) {
      builder()->LoadTrue();
    } else if (raw_value->IsFalse()) {
      builder()->LoadFalse();
    } else if (raw_value->IsNull()) {
      builder()->LoadNull();
    } else if (raw_value->IsTheHole()) {
      builder()->LoadTheHole();
    } else {
      builder()->LoadLiteral(raw_value->value());
    }
  }
}

void BytecodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  // Materialize a regular expression literal.
  builder()->CreateRegExpLiteral(expr->pattern(), expr->literal_index(),
                                 expr->flags());
}

void BytecodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  // Deep-copy the literal boilerplate.
  uint8_t flags = CreateObjectLiteralFlags::Encode(
      expr->IsFastCloningSupported(),
      ConstructorBuiltinsAssembler::FastCloneShallowObjectPropertiesCount(
          expr->properties_count()),
      expr->ComputeFlags());

  Register literal = register_allocator()->NewRegister();
  size_t entry;
  // If constant properties is an empty fixed array, use our cached
  // empty_fixed_array to ensure it's only added to the constant pool once.
  if (expr->properties_count() == 0) {
    entry = builder()->GetConstantPoolEntry(empty_fixed_array());
  } else {
    entry = builder()->AllocateConstantPoolEntry();
    object_literals_.push_back(std::make_pair(expr, entry));
  }
  builder()->CreateObjectLiteral(entry, expr->literal_index(), flags, literal);

  // Store computed values into the literal.
  int property_index = 0;
  AccessorTable accessor_table(zone());
  for (; property_index < expr->properties()->length(); property_index++) {
    ObjectLiteral::Property* property = expr->properties()->at(property_index);
    if (property->is_computed_name()) break;
    if (property->IsCompileTimeValue()) continue;

    RegisterAllocationScope inner_register_scope(this);
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
            VisitForAccumulatorValue(property->value());
            if (FunctionLiteral::NeedsHomeObject(property->value())) {
              RegisterAllocationScope register_scope(this);
              Register value = register_allocator()->NewRegister();
              builder()->StoreAccumulatorInRegister(value);
              builder()->StoreNamedProperty(
                  literal, key->AsPropertyName(),
                  feedback_index(property->GetSlot(0)), language_mode());
              VisitSetHomeObject(value, literal, property, 1);
            } else {
              builder()->StoreNamedProperty(
                  literal, key->AsPropertyName(),
                  feedback_index(property->GetSlot(0)), language_mode());
            }
          } else {
            VisitForEffect(property->value());
          }
        } else {
          RegisterList args = register_allocator()->NewRegisterList(4);

          builder()->MoveRegister(literal, args[0]);
          VisitForRegisterValue(property->key(), args[1]);
          VisitForRegisterValue(property->value(), args[2]);
          if (property->emit_store()) {
            builder()
                ->LoadLiteral(Smi::FromInt(SLOPPY))
                .StoreAccumulatorInRegister(args[3])
                .CallRuntime(Runtime::kSetProperty, args);
            Register value = args[2];
            VisitSetHomeObject(value, literal, property);
          }
        }
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE: {
        DCHECK(property->emit_store());
        RegisterList args = register_allocator()->NewRegisterList(2);
        builder()->MoveRegister(literal, args[0]);
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

    if (property->kind() == ObjectLiteral::Property::PROTOTYPE) {
      DCHECK(property->emit_store());
      RegisterList args = register_allocator()->NewRegisterList(2);
      builder()->MoveRegister(literal, args[0]);
      VisitForRegisterValue(property->value(), args[1]);
      builder()->CallRuntime(Runtime::kInternalSetPrototype, args);
      continue;
    }

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::COMPUTED:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL: {
        Register key = register_allocator()->NewRegister();
        VisitForAccumulatorValue(property->key());
        builder()->ConvertAccumulatorToName(key);

        Register value = VisitForRegisterValue(property->value());
        VisitSetHomeObject(value, literal, property);

        DataPropertyInLiteralFlags data_property_flags =
            DataPropertyInLiteralFlag::kNoFlags;
        if (property->NeedsSetFunctionName()) {
          data_property_flags |= DataPropertyInLiteralFlag::kSetFunctionName;
        }

        FeedbackVectorSlot slot = property->GetStoreDataPropertySlot();
        DCHECK(!slot.IsInvalid());

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
        VisitForAccumulatorValue(property->key());
        builder()->ConvertAccumulatorToName(args[1]);
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

void BytecodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  // Deep-copy the literal boilerplate.
  uint8_t flags = CreateArrayLiteralFlags::Encode(
      expr->IsFastCloningSupported(), expr->ComputeFlags());

  size_t entry = builder()->AllocateConstantPoolEntry();
  builder()->CreateArrayLiteral(entry, expr->literal_index(), flags);
  array_literals_.push_back(std::make_pair(expr, entry));

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
}

void BytecodeGenerator::VisitVariableProxy(VariableProxy* proxy) {
  builder()->SetExpressionPosition(proxy);
  BuildVariableLoad(proxy->var(), proxy->VariableFeedbackSlot(),
                    proxy->hole_check_mode());
}

void BytecodeGenerator::BuildVariableLoad(Variable* variable,
                                          FeedbackVectorSlot slot,
                                          HoleCheckMode hole_check_mode,
                                          TypeofMode typeof_mode) {
  switch (variable->location()) {
    case VariableLocation::LOCAL: {
      Register source(Register(variable->index()));
      // We need to load the variable into the accumulator, even when in a
      // VisitForRegisterScope, in order to avoid register aliasing if
      // subsequent expressions assign to the same variable.
      builder()->LoadAccumulatorWithRegister(source);
      if (hole_check_mode == HoleCheckMode::kRequired) {
        BuildThrowIfHole(variable->name());
      }
      break;
    }
    case VariableLocation::PARAMETER: {
      // The parameter indices are shifted by 1 (receiver is variable
      // index -1 but is parameter index 0 in BytecodeArrayBuilder).
      Register source = builder()->Parameter(variable->index() + 1);
      // We need to load the variable into the accumulator, even when in a
      // VisitForRegisterScope, in order to avoid register aliasing if
      // subsequent expressions assign to the same variable.
      builder()->LoadAccumulatorWithRegister(source);
      if (hole_check_mode == HoleCheckMode::kRequired) {
        BuildThrowIfHole(variable->name());
      }
      break;
    }
    case VariableLocation::UNALLOCATED: {
      // The global identifier "undefined" is immutable. Everything
      // else could be reassigned. For performance, we do a pointer comparison
      // rather than checking if the raw_name is really "undefined".
      if (variable->raw_name() == undefined_string()) {
        builder()->LoadUndefined();
      } else {
        builder()->LoadGlobal(variable->name(), feedback_index(slot),
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

      builder()->LoadContextSlot(context_reg, variable->index(), depth);
      if (hole_check_mode == HoleCheckMode::kRequired) {
        BuildThrowIfHole(variable->name());
      }
      break;
    }
    case VariableLocation::LOOKUP: {
      switch (variable->mode()) {
        case DYNAMIC_LOCAL: {
          Variable* local_variable = variable->local_if_not_shadowed();
          int depth =
              execution_context()->ContextChainDepth(local_variable->scope());
          builder()->LoadLookupContextSlot(variable->name(), typeof_mode,
                                           local_variable->index(), depth);
          if (hole_check_mode == HoleCheckMode::kRequired) {
            BuildThrowIfHole(variable->name());
          }
          break;
        }
        case DYNAMIC_GLOBAL: {
          int depth = scope()->ContextChainLengthUntilOutermostSloppyEval();
          builder()->LoadLookupGlobalSlot(variable->name(), typeof_mode,
                                          feedback_index(slot), depth);
          break;
        }
        default:
          builder()->LoadLookupSlot(variable->name(), typeof_mode);
      }
      break;
    }
    case VariableLocation::MODULE: {
      int depth = execution_context()->ContextChainDepth(variable->scope());
      builder()->LoadModuleVariable(variable->index(), depth);
      if (hole_check_mode == HoleCheckMode::kRequired) {
        BuildThrowIfHole(variable->name());
      }
      break;
    }
  }
}

void BytecodeGenerator::BuildVariableLoadForAccumulatorValue(
    Variable* variable, FeedbackVectorSlot slot, HoleCheckMode hole_check_mode,
    TypeofMode typeof_mode) {
  ValueResultScope accumulator_result(this);
  BuildVariableLoad(variable, slot, hole_check_mode, typeof_mode);
}

void BytecodeGenerator::BuildReturn() {
  if (FLAG_trace) {
    RegisterAllocationScope register_scope(this);
    Register result = register_allocator()->NewRegister();
    // Runtime returns {result} value, preserving accumulator.
    builder()->StoreAccumulatorInRegister(result).CallRuntime(
        Runtime::kTraceExit, result);
  }
  builder()->Return();
}

void BytecodeGenerator::BuildAsyncReturn() {
  DCHECK(IsAsyncFunction(info()->literal()->kind()));
  RegisterAllocationScope register_scope(this);
  RegisterList args = register_allocator()->NewRegisterList(3);
  Register receiver = args[0];
  Register promise = args[1];
  Register return_value = args[2];
  builder()->StoreAccumulatorInRegister(return_value);

  Variable* var_promise = scope()->promise_var();
  DCHECK_NOT_NULL(var_promise);
  BuildVariableLoad(var_promise, FeedbackVectorSlot::Invalid(),
                    HoleCheckMode::kElided);
  builder()
      ->StoreAccumulatorInRegister(promise)
      .LoadUndefined()
      .StoreAccumulatorInRegister(receiver)
      .CallJSRuntime(Context::PROMISE_RESOLVE_INDEX, args)
      .LoadAccumulatorWithRegister(promise);
  BuildReturn();
}

void BytecodeGenerator::BuildReThrow() { builder()->ReThrow(); }

void BytecodeGenerator::BuildAbort(BailoutReason bailout_reason) {
  RegisterAllocationScope register_scope(this);
  Register reason = register_allocator()->NewRegister();
  builder()
      ->LoadLiteral(Smi::FromInt(static_cast<int>(bailout_reason)))
      .StoreAccumulatorInRegister(reason)
      .CallRuntime(Runtime::kAbort, reason);
}

void BytecodeGenerator::BuildThrowReferenceError(Handle<String> name) {
  RegisterAllocationScope register_scope(this);
  Register name_reg = register_allocator()->NewRegister();
  builder()->LoadLiteral(name).StoreAccumulatorInRegister(name_reg).CallRuntime(
      Runtime::kThrowReferenceError, name_reg);
}

void BytecodeGenerator::BuildThrowIfHole(Handle<String> name) {
  // TODO(interpreter): Can the parser reduce the number of checks
  // performed? Or should there be a ThrowIfHole bytecode.
  BytecodeLabel no_reference_error;
  builder()->JumpIfNotHole(&no_reference_error);
  BuildThrowReferenceError(name);
  builder()->Bind(&no_reference_error);
}

void BytecodeGenerator::BuildHoleCheckForVariableAssignment(Variable* variable,
                                                            Token::Value op) {
  if (variable->is_this() && variable->mode() == CONST && op == Token::INIT) {
    // Perform an initialization check for 'this'. 'this' variable is the
    // only variable able to trigger bind operations outside the TDZ
    // via 'super' calls.
    BytecodeLabel no_reference_error, reference_error;
    builder()
        ->JumpIfNotHole(&reference_error)
        .Jump(&no_reference_error)
        .Bind(&reference_error)
        .CallRuntime(Runtime::kThrowSuperAlreadyCalledError)
        .Bind(&no_reference_error);
  } else {
    // Perform an initialization check for let/const declared variables.
    // E.g. let x = (x = 20); is not allowed.
    DCHECK(IsLexicalVariableMode(variable->mode()));
    BuildThrowIfHole(variable->name());
  }
}

void BytecodeGenerator::BuildVariableAssignment(Variable* variable,
                                                Token::Value op,
                                                FeedbackVectorSlot slot,
                                                HoleCheckMode hole_check_mode) {
  VariableMode mode = variable->mode();
  RegisterAllocationScope assignment_register_scope(this);
  BytecodeLabel end_label;
  switch (variable->location()) {
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      Register destination;
      if (VariableLocation::PARAMETER == variable->location()) {
        destination = Register(builder()->Parameter(variable->index() + 1));
      } else {
        destination = Register(variable->index());
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

      if (mode != CONST || op == Token::INIT) {
        builder()->StoreAccumulatorInRegister(destination);
      } else if (variable->throw_on_const_assignment(language_mode())) {
        builder()->CallRuntime(Runtime::kThrowConstAssignError);
      }
      break;
    }
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
        depth = 0;
      } else {
        context_reg = execution_context()->reg();
      }

      if (hole_check_mode == HoleCheckMode::kRequired) {
        // Load destination to check for hole.
        Register value_temp = register_allocator()->NewRegister();
        builder()
            ->StoreAccumulatorInRegister(value_temp)
            .LoadContextSlot(context_reg, variable->index(), depth);

        BuildHoleCheckForVariableAssignment(variable, op);
        builder()->LoadAccumulatorWithRegister(value_temp);
      }

      if (mode != CONST || op == Token::INIT) {
        builder()->StoreContextSlot(context_reg, variable->index(), depth);
      } else if (variable->throw_on_const_assignment(language_mode())) {
        builder()->CallRuntime(Runtime::kThrowConstAssignError);
      }
      break;
    }
    case VariableLocation::LOOKUP: {
      builder()->StoreLookupSlot(variable->name(), language_mode());
      break;
    }
    case VariableLocation::MODULE: {
      DCHECK(IsDeclaredVariableMode(mode));

      if (mode == CONST && op != Token::INIT) {
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

void BytecodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpressionOrThis());
  Register object, key;
  RegisterList super_property_args;
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
          ->LoadLiteral(property->key()->AsLiteral()->AsPropertyName())
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
  if (expr->is_compound()) {
    Register old_value = register_allocator()->NewRegister();
    switch (assign_type) {
      case VARIABLE: {
        VariableProxy* proxy = expr->target()->AsVariableProxy();
        BuildVariableLoad(proxy->var(), proxy->VariableFeedbackSlot(),
                          proxy->hole_check_mode());
        builder()->StoreAccumulatorInRegister(old_value);
        break;
      }
      case NAMED_PROPERTY: {
        FeedbackVectorSlot slot = property->PropertyFeedbackSlot();
        builder()
            ->LoadNamedProperty(object, name, feedback_index(slot))
            .StoreAccumulatorInRegister(old_value);
        break;
      }
      case KEYED_PROPERTY: {
        // Key is already in accumulator at this point due to evaluating the
        // LHS above.
        FeedbackVectorSlot slot = property->PropertyFeedbackSlot();
        builder()
            ->LoadKeyedProperty(object, feedback_index(slot))
            .StoreAccumulatorInRegister(old_value);
        break;
      }
      case NAMED_SUPER_PROPERTY: {
        builder()
            ->CallRuntime(Runtime::kLoadFromSuper,
                          super_property_args.Truncate(3))
            .StoreAccumulatorInRegister(old_value);
        break;
      }
      case KEYED_SUPER_PROPERTY: {
        builder()
            ->CallRuntime(Runtime::kLoadKeyedFromSuper,
                          super_property_args.Truncate(3))
            .StoreAccumulatorInRegister(old_value);
        break;
      }
    }
    VisitForAccumulatorValue(expr->value());
    FeedbackVectorSlot slot =
        expr->binary_operation()->BinaryOperationFeedbackSlot();
    builder()->BinaryOperation(expr->binary_op(), old_value,
                               feedback_index(slot));
  } else {
    VisitForAccumulatorValue(expr->value());
  }

  // Store the value.
  builder()->SetExpressionPosition(expr);
  FeedbackVectorSlot slot = expr->AssignmentSlot();
  switch (assign_type) {
    case VARIABLE: {
      // TODO(oth): The BuildVariableAssignment() call is hard to reason about.
      // Is the value in the accumulator safe? Yes, but scary.
      VariableProxy* proxy = expr->target()->AsVariableProxy();
      BuildVariableAssignment(proxy->var(), expr->op(), slot,
                              proxy->hole_check_mode());
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

void BytecodeGenerator::VisitYield(Yield* expr) {
  builder()->SetExpressionPosition(expr);
  Register value = VisitForRegisterValue(expr->expression());

  Register generator = VisitForRegisterValue(expr->generator_object());

  // Save context, registers, and state. Then return.
  builder()
      ->LoadLiteral(Smi::FromInt(expr->yield_id()))
      .SuspendGenerator(generator)
      .LoadAccumulatorWithRegister(value)
      .Return();  // Hard return (ignore any finally blocks).

  builder()->Bind(&(generator_resume_points_[expr->yield_id()]));
  // Upon resume, we continue here.

  {
    RegisterAllocationScope register_scope(this);

    // Update state to indicate that we have finished resuming. Loop headers
    // rely on this.
    builder()
        ->LoadLiteral(Smi::FromInt(JSGeneratorObject::kGeneratorExecuting))
        .StoreAccumulatorInRegister(generator_state_);

    Register input = register_allocator()->NewRegister();
    builder()
        ->CallRuntime(Runtime::kInlineGeneratorGetInputOrDebugPos, generator)
        .StoreAccumulatorInRegister(input);

    Register resume_mode = register_allocator()->NewRegister();
    builder()
        ->CallRuntime(Runtime::kInlineGeneratorGetResumeMode, generator)
        .StoreAccumulatorInRegister(resume_mode);

    // Now dispatch on resume mode.

    BytecodeLabel resume_with_next;
    BytecodeLabel resume_with_return;
    BytecodeLabel resume_with_throw;

    builder()
        ->LoadLiteral(Smi::FromInt(JSGeneratorObject::kNext))
        .CompareOperation(Token::EQ_STRICT, resume_mode)
        .JumpIfTrue(&resume_with_next)
        .LoadLiteral(Smi::FromInt(JSGeneratorObject::kThrow))
        .CompareOperation(Token::EQ_STRICT, resume_mode)
        .JumpIfTrue(&resume_with_throw)
        .Jump(&resume_with_return);

    builder()->Bind(&resume_with_return);
    {
      RegisterList args = register_allocator()->NewRegisterList(2);
      builder()
          ->MoveRegister(input, args[0])
          .LoadTrue()
          .StoreAccumulatorInRegister(args[1])
          .CallRuntime(Runtime::kInlineCreateIterResultObject, args);
      execution_control()->ReturnAccumulator();
    }

    builder()->Bind(&resume_with_throw);
    builder()->SetExpressionPosition(expr);
    builder()->LoadAccumulatorWithRegister(input);
    if (expr->rethrow_on_exception()) {
      builder()->ReThrow();
    } else {
      builder()->Throw();
    }

    builder()->Bind(&resume_with_next);
    builder()->LoadAccumulatorWithRegister(input);
  }
}

void BytecodeGenerator::VisitThrow(Throw* expr) {
  VisitForAccumulatorValue(expr->exception());
  builder()->SetExpressionPosition(expr);
  builder()->Throw();
}

void BytecodeGenerator::VisitPropertyLoad(Register obj, Property* expr) {
  LhsKind property_kind = Property::GetAssignType(expr);
  FeedbackVectorSlot slot = expr->PropertyFeedbackSlot();
  builder()->SetExpressionPosition(expr);
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
  builder()
      ->LoadLiteral(property->key()->AsLiteral()->AsPropertyName())
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

void BytecodeGenerator::VisitArguments(ZoneList<Expression*>* args,
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

  Register callee = register_allocator()->NewRegister();
  // Grow the args list as we visit receiver / arguments to avoid allocating all
  // the registers up-front. Otherwise these registers are unavailable during
  // receiver / argument visiting and we can end up with memory leaks due to
  // registers keeping objects alive.
  RegisterList args = register_allocator()->NewGrowableRegisterList();

  // Prepare the callee and the receiver to the function call. This depends on
  // the semantics of the underlying call type.
  switch (call_type) {
    case Call::NAMED_PROPERTY_CALL:
    case Call::KEYED_PROPERTY_CALL: {
      Property* property = callee_expr->AsProperty();
      VisitAndPushIntoRegisterList(property->obj(), &args);
      VisitPropertyLoadForRegister(args[0], property, callee);
      break;
    }
    case Call::GLOBAL_CALL: {
      // Receiver is undefined for global calls.
      BuildPushUndefinedIntoRegisterList(&args);
      // Load callee as a global variable.
      VariableProxy* proxy = callee_expr->AsVariableProxy();
      BuildVariableLoadForAccumulatorValue(proxy->var(),
                                           proxy->VariableFeedbackSlot(),
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
        DCHECK(Register::AreContiguous(callee, receiver));
        RegisterList result_pair(callee.index(), 2);
        USE(receiver);
        Variable* variable = callee_expr->AsVariableProxy()->var();
        builder()
            ->LoadLiteral(variable->name())
            .StoreAccumulatorInRegister(name)
            .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, name,
                                result_pair);
      }
      break;
    }
    case Call::OTHER_CALL: {
      BuildPushUndefinedIntoRegisterList(&args);
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
  CHECK_EQ(expr->arguments()->length() + 1, args.register_count());

  // Resolve callee for a potential direct eval call. This block will mutate the
  // callee value.
  if (expr->is_possibly_eval() && expr->arguments()->length() > 0) {
    RegisterAllocationScope inner_register_scope(this);
    // Set up arguments for ResolvePossiblyDirectEval by copying callee, source
    // strings and function closure, and loading language and
    // position.
    RegisterList runtime_call_args = register_allocator()->NewRegisterList(6);
    builder()
        ->MoveRegister(callee, runtime_call_args[0])
        .MoveRegister(args[1], runtime_call_args[1])
        .MoveRegister(Register::function_closure(), runtime_call_args[2])
        .LoadLiteral(Smi::FromInt(language_mode()))
        .StoreAccumulatorInRegister(runtime_call_args[3])
        .LoadLiteral(
            Smi::FromInt(execution_context()->scope()->start_position()))
        .StoreAccumulatorInRegister(runtime_call_args[4])
        .LoadLiteral(Smi::FromInt(expr->position()))
        .StoreAccumulatorInRegister(runtime_call_args[5]);

    // Call ResolvePossiblyDirectEval and modify the callee.
    builder()
        ->CallRuntime(Runtime::kResolvePossiblyDirectEval, runtime_call_args)
        .StoreAccumulatorInRegister(callee);
  }

  builder()->SetExpressionPosition(expr);

  int const feedback_slot_index = feedback_index(expr->CallFeedbackICSlot());
  builder()->Call(callee, args, feedback_slot_index, call_type,
                  expr->tail_call_mode());
}

void BytecodeGenerator::VisitCallSuper(Call* expr) {
  RegisterAllocationScope register_scope(this);
  SuperCallReference* super = expr->expression()->AsSuperCallReference();

  // Prepare the constructor to the super call.
  VisitForAccumulatorValue(super->this_function_var());
  Register constructor = register_allocator()->NewRegister();
  builder()->GetSuperConstructor(constructor);

  ZoneList<Expression*>* args = expr->arguments();

  // When a super call contains a spread, a CallSuper AST node is only created
  // if there is exactly one spread, and it is the last argument.
  if (!args->is_empty() && args->last()->IsSpread()) {
    RegisterList args_regs = register_allocator()->NewGrowableRegisterList();
    Register constructor_arg =
        register_allocator()->GrowRegisterList(&args_regs);
    builder()->MoveRegister(constructor, constructor_arg);
    // Reserve argument reg for new.target in correct place for runtime call.
    // TODO(petermarshall): Remove this when changing bytecode to use the new
    // stub.
    Register new_target = register_allocator()->GrowRegisterList(&args_regs);
    VisitArguments(args, &args_regs);
    VisitForRegisterValue(super->new_target_var(), new_target);
    builder()->NewWithSpread(args_regs);
  } else {
    RegisterList args_regs = register_allocator()->NewGrowableRegisterList();
    VisitArguments(args, &args_regs);
    // The new target is loaded into the accumulator from the
    // {new.target} variable.
    VisitForAccumulatorValue(super->new_target_var());

    // Call construct.
    builder()->SetExpressionPosition(expr);
    // TODO(turbofan): For now we do gather feedback on super constructor
    // calls, utilizing the existing machinery to inline the actual call
    // target and the JSCreate for the implicit receiver allocation. This
    // is not an ideal solution for super constructor calls, but it gets
    // the job done for now. In the long run we might want to revisit this
    // and come up with a better way.
    int const feedback_slot_index = feedback_index(expr->CallFeedbackICSlot());
    builder()->New(constructor, args_regs, feedback_slot_index);
  }
}

void BytecodeGenerator::VisitCallNew(CallNew* expr) {
  Register constructor = VisitForRegisterValue(expr->expression());
  RegisterList args = register_allocator()->NewGrowableRegisterList();
  VisitArguments(expr->arguments(), &args);

  builder()->SetExpressionPosition(expr);
  // The accumulator holds new target which is the same as the
  // constructor for CallNew.
  builder()
      ->LoadAccumulatorWithRegister(constructor)
      .New(constructor, args, feedback_index(expr->CallNewFeedbackSlot()));
}

void BytecodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  if (expr->is_jsruntime()) {
    RegisterList args = register_allocator()->NewGrowableRegisterList();
    // Allocate a register for the receiver and load it with undefined.
    BuildPushUndefinedIntoRegisterList(&args);
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

void BytecodeGenerator::VisitTypeOf(UnaryOperation* expr) {
  if (expr->expression()->IsVariableProxy()) {
    // Typeof does not throw a reference error on global variables, hence we
    // perform a non-contextual load in case the operand is a variable proxy.
    VariableProxy* proxy = expr->expression()->AsVariableProxy();
    BuildVariableLoadForAccumulatorValue(
        proxy->var(), proxy->VariableFeedbackSlot(), proxy->hole_check_mode(),
        INSIDE_TYPEOF);
  } else {
    VisitForAccumulatorValue(expr->expression());
  }
  builder()->TypeOf();
}

void BytecodeGenerator::VisitNot(UnaryOperation* expr) {
  if (execution_result()->IsEffect()) {
    VisitForEffect(expr->expression());
  } else if (execution_result()->IsTest()) {
    TestResultScope* test_result = execution_result()->AsTest();
    // No actual logical negation happening, we just swap the control flow by
    // swapping the target labels and the fallthrough branch.
    VisitForTest(expr->expression(), test_result->else_labels(),
                 test_result->then_labels(),
                 test_result->inverted_fallthrough());
    test_result->SetResultConsumedByTest();
  } else {
    VisitForAccumulatorValue(expr->expression());
    builder()->LogicalNot();
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
    DCHECK(is_sloppy(language_mode()) || variable->is_this());
    switch (variable->location()) {
      case VariableLocation::UNALLOCATED: {
        // Global var, let, const or variables not explicitly declared.
        Register native_context = register_allocator()->NewRegister();
        Register global_object = register_allocator()->NewRegister();
        builder()
            ->LoadContextSlot(execution_context()->reg(),
                              Context::NATIVE_CONTEXT_INDEX, 0)
            .StoreAccumulatorInRegister(native_context)
            .LoadContextSlot(native_context, Context::EXTENSION_INDEX, 0)
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
        if (variable->is_this()) {
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
            .CallRuntime(Runtime::kDeleteLookupSlot, name_reg);
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
}

void BytecodeGenerator::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpressionOrThis());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->expression()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  bool is_postfix = expr->is_postfix() && !execution_result()->IsEffect();

  // Evaluate LHS expression and get old value.
  Register object, key, old_value;
  RegisterList super_property_args;
  Handle<String> name;
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      BuildVariableLoadForAccumulatorValue(proxy->var(),
                                           proxy->VariableFeedbackSlot(),
                                           proxy->hole_check_mode());
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
      super_property_args = register_allocator()->NewRegisterList(4);
      RegisterList load_super_args = super_property_args.Truncate(3);
      SuperPropertyReference* super_property =
          property->obj()->AsSuperPropertyReference();
      VisitForRegisterValue(super_property->this_var(), load_super_args[0]);
      VisitForRegisterValue(super_property->home_object(), load_super_args[1]);
      builder()
          ->LoadLiteral(property->key()->AsLiteral()->AsPropertyName())
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
  if (is_postfix) {
    // Convert old value into a number before saving it.
    old_value = register_allocator()->NewRegister();
    builder()
        ->ConvertAccumulatorToNumber(old_value)
        .LoadAccumulatorWithRegister(old_value);
  }

  // Perform +1/-1 operation.
  FeedbackVectorSlot slot = expr->CountBinaryOpFeedbackSlot();
  builder()->CountOperation(expr->binary_op(), feedback_index(slot));

  // Store the value.
  builder()->SetExpressionPosition(expr);
  FeedbackVectorSlot feedback_slot = expr->CountSlot();
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      BuildVariableAssignment(proxy->var(), expr->op(), feedback_slot,
                              proxy->hole_check_mode());
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

void BytecodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Register lhs = VisitForRegisterValue(expr->left());
  VisitForAccumulatorValue(expr->right());
  builder()->SetExpressionPosition(expr);
  FeedbackVectorSlot slot = expr->CompareOperationFeedbackSlot();
  builder()->CompareOperation(expr->op(), lhs, feedback_index(slot));
}

void BytecodeGenerator::VisitArithmeticExpression(BinaryOperation* expr) {
  // TODO(rmcilroy): Special case "x * 1.0" and "x * -1" which are generated for
  // +x and -x by the parser.
  Register lhs = VisitForRegisterValue(expr->left());
  VisitForAccumulatorValue(expr->right());
  FeedbackVectorSlot slot = expr->BinaryOperationFeedbackSlot();
  builder()->SetExpressionPosition(expr);
  builder()->BinaryOperation(expr->op(), lhs, feedback_index(slot));
}

void BytecodeGenerator::VisitSpread(Spread* expr) { Visit(expr->expression()); }

void BytecodeGenerator::VisitEmptyParentheses(EmptyParentheses* expr) {
  UNREACHABLE();
}

void BytecodeGenerator::VisitGetIterator(GetIterator* expr) {
  FeedbackVectorSlot load_slot = expr->IteratorPropertyFeedbackSlot();
  FeedbackVectorSlot call_slot = expr->IteratorCallFeedbackSlot();

  RegisterList args = register_allocator()->NewRegisterList(1);
  Register method = register_allocator()->NewRegister();
  Register obj = args[0];

  VisitForAccumulatorValue(expr->iterable());

  // Let method be GetMethod(obj, @@iterator).
  builder()
      ->StoreAccumulatorInRegister(obj)
      .LoadNamedProperty(obj, iterator_symbol(), feedback_index(load_slot))
      .StoreAccumulatorInRegister(method);

  // Let iterator be Call(method, obj).
  builder()->Call(method, args, feedback_index(call_slot),
                  Call::NAMED_PROPERTY_CALL);

  // If Type(iterator) is not Object, throw a TypeError exception.
  BytecodeLabel no_type_error;
  builder()->JumpIfJSReceiver(&no_type_error);
  builder()->CallRuntime(Runtime::kThrowSymbolIteratorInvalid);
  builder()->Bind(&no_type_error);
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

void BytecodeGenerator::VisitLogicalOrExpression(BinaryOperation* binop) {
  Expression* left = binop->left();
  Expression* right = binop->right();

  if (execution_result()->IsTest()) {
    TestResultScope* test_result = execution_result()->AsTest();

    if (left->ToBooleanIsTrue()) {
      builder()->Jump(test_result->NewThenLabel());
    } else if (left->ToBooleanIsFalse() && right->ToBooleanIsFalse()) {
      builder()->Jump(test_result->NewElseLabel());
    } else {
      BytecodeLabels test_right(zone());
      VisitForTest(left, test_result->then_labels(), &test_right,
                   TestFallthrough::kElse);
      test_right.Bind(builder());
      VisitForTest(right, test_result->then_labels(),
                   test_result->else_labels(), test_result->fallthrough());
    }
    test_result->SetResultConsumedByTest();
  } else {
    if (left->ToBooleanIsTrue()) {
      VisitForAccumulatorValue(left);
    } else if (left->ToBooleanIsFalse()) {
      VisitForAccumulatorValue(right);
    } else {
      BytecodeLabel end_label;
      VisitForAccumulatorValue(left);
      builder()->JumpIfTrue(&end_label);
      VisitForAccumulatorValue(right);
      builder()->Bind(&end_label);
    }
  }
}

void BytecodeGenerator::VisitLogicalAndExpression(BinaryOperation* binop) {
  Expression* left = binop->left();
  Expression* right = binop->right();

  if (execution_result()->IsTest()) {
    TestResultScope* test_result = execution_result()->AsTest();

    if (left->ToBooleanIsFalse()) {
      builder()->Jump(test_result->NewElseLabel());
    } else if (left->ToBooleanIsTrue() && right->ToBooleanIsTrue()) {
      builder()->Jump(test_result->NewThenLabel());
    } else {
      BytecodeLabels test_right(zone());
      VisitForTest(left, &test_right, test_result->else_labels(),
                   TestFallthrough::kThen);
      test_right.Bind(builder());
      VisitForTest(right, test_result->then_labels(),
                   test_result->else_labels(), test_result->fallthrough());
    }
    test_result->SetResultConsumedByTest();
  } else {
    if (left->ToBooleanIsFalse()) {
      VisitForAccumulatorValue(left);
    } else if (left->ToBooleanIsTrue()) {
      VisitForAccumulatorValue(right);
    } else {
      BytecodeLabel end_label;
      VisitForAccumulatorValue(left);
      builder()->JumpIfFalse(&end_label);
      VisitForAccumulatorValue(right);
      builder()->Bind(&end_label);
    }
  }
}

void BytecodeGenerator::VisitRewritableExpression(RewritableExpression* expr) {
  Visit(expr->expression());
}

void BytecodeGenerator::BuildNewLocalActivationContext() {
  ValueResultScope value_execution_result(this);
  Scope* scope = this->scope();

  // Create the appropriate context.
  if (scope->is_script_scope()) {
    RegisterList args = register_allocator()->NewRegisterList(2);
    builder()
        ->LoadAccumulatorWithRegister(Register::function_closure())
        .StoreAccumulatorInRegister(args[0])
        .LoadLiteral(scope->scope_info())
        .StoreAccumulatorInRegister(args[1])
        .CallRuntime(Runtime::kNewScriptContext, args);
  } else if (scope->is_module_scope()) {
    // We don't need to do anything for the outer script scope.
    DCHECK(scope->outer_scope()->is_script_scope());

    // A JSFunction representing a module is called with the module object as
    // its sole argument, which we pass on to PushModuleContext.
    RegisterList args = register_allocator()->NewRegisterList(3);
    builder()
        ->MoveRegister(builder()->Parameter(1), args[0])
        .LoadAccumulatorWithRegister(Register::function_closure())
        .StoreAccumulatorInRegister(args[1])
        .LoadLiteral(scope->scope_info())
        .StoreAccumulatorInRegister(args[2])
        .CallRuntime(Runtime::kPushModuleContext, args);
  } else {
    DCHECK(scope->is_function_scope() || scope->is_eval_scope());
    int slot_count = scope->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (slot_count <=
        ConstructorBuiltinsAssembler::MaximumFunctionContextSlots()) {
      switch (scope->scope_type()) {
        case EVAL_SCOPE:
          builder()->CreateEvalContext(slot_count);
          break;
        case FUNCTION_SCOPE:
          builder()->CreateFunctionContext(slot_count);
          break;
        default:
          UNREACHABLE();
      }
    } else {
      RegisterList args = register_allocator()->NewRegisterList(2);
      builder()
          ->MoveRegister(Register::function_closure(), args[0])
          .LoadLiteral(Smi::FromInt(scope->scope_type()))
          .StoreAccumulatorInRegister(args[1])
          .CallRuntime(Runtime::kNewFunctionContext, args);
    }
  }
}

void BytecodeGenerator::BuildLocalActivationContextInitialization() {
  DeclarationScope* scope = this->scope();

  if (scope->has_this_declaration() && scope->receiver()->IsContextSlot()) {
    Variable* variable = scope->receiver();
    Register receiver(builder()->Parameter(0));
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

    // The parameter indices are shifted by 1 (receiver is variable
    // index -1 but is parameter index 0 in BytecodeArrayBuilder).
    Register parameter(builder()->Parameter(i + 1));
    // Context variable (at bottom of the context chain).
    DCHECK_EQ(0, scope->ContextChainLength(variable->scope()));
    builder()->LoadAccumulatorWithRegister(parameter).StoreContextSlot(
        execution_context()->reg(), variable->index(), 0);
  }
}

void BytecodeGenerator::BuildNewLocalBlockContext(Scope* scope) {
  ValueResultScope value_execution_result(this);
  DCHECK(scope->is_block_scope());

  VisitFunctionClosureForContext();
  builder()->CreateBlockContext(scope->scope_info());
}

void BytecodeGenerator::BuildNewLocalWithContext(Scope* scope) {
  ValueResultScope value_execution_result(this);

  Register extension_object = register_allocator()->NewRegister();

  builder()->ConvertAccumulatorToObject(extension_object);
  VisitFunctionClosureForContext();
  builder()->CreateWithContext(extension_object, scope->scope_info());
}

void BytecodeGenerator::BuildNewLocalCatchContext(Variable* variable,
                                                  Scope* scope) {
  ValueResultScope value_execution_result(this);
  DCHECK(variable->IsContextSlot());

  Register exception = register_allocator()->NewRegister();
  builder()->StoreAccumulatorInRegister(exception);
  VisitFunctionClosureForContext();
  builder()->CreateCatchContext(exception, variable->name(),
                                scope->scope_info());
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
                                           LiteralProperty* property,
                                           int slot_number) {
  Expression* expr = property->value();
  if (FunctionLiteral::NeedsHomeObject(expr)) {
    FeedbackVectorSlot slot = property->GetSlot(slot_number);
    builder()
        ->LoadAccumulatorWithRegister(home_object)
        .StoreNamedProperty(value, home_object_symbol(), feedback_index(slot),
                            language_mode());
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
  BuildVariableAssignment(variable, Token::ASSIGN,
                          FeedbackVectorSlot::Invalid(),
                          HoleCheckMode::kElided);
}

void BytecodeGenerator::VisitRestArgumentsArray(Variable* rest) {
  if (rest == nullptr) return;

  // Allocate and initialize a new rest parameter and assign to the {rest}
  // variable.
  builder()->CreateArguments(CreateArgumentsType::kRestParameter);
  DCHECK(rest->IsContextSlot() || rest->IsStackAllocated());
  BuildVariableAssignment(rest, Token::ASSIGN, FeedbackVectorSlot::Invalid(),
                          HoleCheckMode::kElided);
}

void BytecodeGenerator::VisitThisFunctionVariable(Variable* variable) {
  if (variable == nullptr) return;

  // Store the closure we were called with in the given variable.
  builder()->LoadAccumulatorWithRegister(Register::function_closure());
  BuildVariableAssignment(variable, Token::INIT, FeedbackVectorSlot::Invalid(),
                          HoleCheckMode::kElided);
}

void BytecodeGenerator::VisitNewTargetVariable(Variable* variable) {
  if (variable == nullptr) return;

  // Store the new target we were called with in the given variable.
  builder()->LoadAccumulatorWithRegister(Register::new_target());
  BuildVariableAssignment(variable, Token::INIT, FeedbackVectorSlot::Invalid(),
                          HoleCheckMode::kElided);

  // TODO(mstarzinger): The <new.target> register is not set by the deoptimizer
  // and we need to make sure {BytecodeRegisterOptimizer} flushes its state
  // before a local variable containing the <new.target> is used. Using a label
  // as below flushes the entire pipeline, we should be more specific here.
  BytecodeLabel flush_state_label;
  builder()->Bind(&flush_state_label);
}

void BytecodeGenerator::VisitFunctionClosureForContext() {
  ValueResultScope value_execution_result(this);
  DeclarationScope* closure_scope =
      execution_context()->scope()->GetClosureScope();
  if (closure_scope->is_script_scope()) {
    // Contexts nested in the native context have a canonical empty function as
    // their closure, not the anonymous closure containing the global code.
    Register native_context = register_allocator()->NewRegister();
    builder()
        ->LoadContextSlot(execution_context()->reg(),
                          Context::NATIVE_CONTEXT_INDEX, 0)
        .StoreAccumulatorInRegister(native_context)
        .LoadContextSlot(native_context, Context::CLOSURE_INDEX, 0);
  } else if (closure_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code. Fetch it from the context.
    builder()->LoadContextSlot(execution_context()->reg(),
                               Context::CLOSURE_INDEX, 0);
  } else {
    DCHECK(closure_scope->is_function_scope() ||
           closure_scope->is_module_scope());
    builder()->LoadAccumulatorWithRegister(Register::function_closure());
  }
}

// Visits the expression |expr| and places the result in the accumulator.
void BytecodeGenerator::VisitForAccumulatorValue(Expression* expr) {
  ValueResultScope accumulator_scope(this);
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

void BytecodeGenerator::BuildPushUndefinedIntoRegisterList(
    RegisterList* reg_list) {
  Register reg = register_allocator()->GrowRegisterList(reg_list);
  builder()->LoadUndefined().StoreAccumulatorInRegister(reg);
}

// Visits the expression |expr| for testing its boolean value and jumping to the
// |then| or |other| label depending on value and short-circuit semantics
void BytecodeGenerator::VisitForTest(Expression* expr,
                                     BytecodeLabels* then_labels,
                                     BytecodeLabels* else_labels,
                                     TestFallthrough fallthrough) {
  bool result_consumed;
  {
    // To make sure that all temporary registers are returned before generating
    // jumps below, we ensure that the result scope is deleted before doing so.
    // Dead registers might be materialized otherwise.
    TestResultScope test_result(this, then_labels, else_labels, fallthrough);
    Visit(expr);
    result_consumed = test_result.ResultConsumedByTest();
  }
  if (!result_consumed) {
    switch (fallthrough) {
      case TestFallthrough::kThen:
        builder()->JumpIfFalse(else_labels->New());
        break;
      case TestFallthrough::kElse:
        builder()->JumpIfTrue(then_labels->New());
        break;
      case TestFallthrough::kNone:
        builder()->JumpIfTrue(then_labels->New());
        builder()->Jump(else_labels->New());
    }
  }
}

void BytecodeGenerator::VisitInScope(Statement* stmt, Scope* scope) {
  ContextScope context_scope(this, scope);
  DCHECK(scope->declarations()->is_empty());
  Visit(stmt);
}

LanguageMode BytecodeGenerator::language_mode() const {
  return execution_context()->scope()->language_mode();
}

int BytecodeGenerator::feedback_index(FeedbackVectorSlot slot) const {
  return FeedbackVector::GetIndex(slot);
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

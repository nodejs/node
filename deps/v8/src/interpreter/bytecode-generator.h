// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_GENERATOR_H_
#define V8_INTERPRETER_BYTECODE_GENERATOR_H_

#include "src/ast/ast.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {

class CompilationInfo;

namespace interpreter {

class LoopBuilder;

class BytecodeGenerator final : public AstVisitor<BytecodeGenerator> {
 public:
  explicit BytecodeGenerator(CompilationInfo* info);

  void GenerateBytecode(uintptr_t stack_limit);
  Handle<BytecodeArray> FinalizeBytecode(Isolate* isolate);

#define DECLARE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Visiting function for declarations list and statements are overridden.
  void VisitDeclarations(Declaration::List* declarations);
  void VisitStatements(ZoneList<Statement*>* statments);

 private:
  class ContextScope;
  class ControlScope;
  class ControlScopeForBreakable;
  class ControlScopeForIteration;
  class ControlScopeForTopLevel;
  class ControlScopeForTryCatch;
  class ControlScopeForTryFinally;
  class ExpressionResultScope;
  class EffectResultScope;
  class GlobalDeclarationsBuilder;
  class RegisterAllocationScope;
  class TestResultScope;
  class ValueResultScope;

  enum class TestFallthrough { kThen, kElse, kNone };

  void GenerateBytecodeBody();
  void AllocateDeferredConstants(Isolate* isolate);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

  // Dispatched from VisitBinaryOperation.
  void VisitArithmeticExpression(BinaryOperation* binop);
  void VisitCommaExpression(BinaryOperation* binop);
  void VisitLogicalOrExpression(BinaryOperation* binop);
  void VisitLogicalAndExpression(BinaryOperation* binop);

  // Dispatched from VisitUnaryOperation.
  void VisitVoid(UnaryOperation* expr);
  void VisitTypeOf(UnaryOperation* expr);
  void VisitNot(UnaryOperation* expr);
  void VisitDelete(UnaryOperation* expr);

  // Used by flow control routines to evaluate loop condition.
  void VisitCondition(Expression* expr);

  // Visit the arguments expressions in |args| and store them in |args_regs|,
  // growing |args_regs| for each argument visited.
  void VisitArguments(ZoneList<Expression*>* args, RegisterList* arg_regs);

  // Visit a keyed super property load. The optional
  // |opt_receiver_out| register will have the receiver stored to it
  // if it's a valid register. The loaded value is placed in the
  // accumulator.
  void VisitKeyedSuperPropertyLoad(Property* property,
                                   Register opt_receiver_out);

  // Visit a named super property load. The optional
  // |opt_receiver_out| register will have the receiver stored to it
  // if it's a valid register. The loaded value is placed in the
  // accumulator.
  void VisitNamedSuperPropertyLoad(Property* property,
                                   Register opt_receiver_out);

  void VisitPropertyLoad(Register obj, Property* expr);
  void VisitPropertyLoadForRegister(Register obj, Property* expr,
                                    Register destination);

  void BuildVariableLoad(Variable* variable, FeedbackVectorSlot slot,
                         HoleCheckMode hole_check_mode,
                         TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  void BuildVariableLoadForAccumulatorValue(
      Variable* variable, FeedbackVectorSlot slot,
      HoleCheckMode hole_check_mode,
      TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  void BuildVariableAssignment(Variable* variable, Token::Value op,
                               FeedbackVectorSlot slot,
                               HoleCheckMode hole_check_mode);

  void BuildReturn();
  void BuildAsyncReturn();
  void BuildReThrow();
  void BuildAbort(BailoutReason bailout_reason);
  void BuildThrowIfHole(Handle<String> name);
  void BuildThrowReferenceError(Handle<String> name);
  void BuildHoleCheckForVariableAssignment(Variable* variable, Token::Value op);

  // Build jump to targets[value], where
  // start_index <= value < start_index + size.
  void BuildIndexedJump(Register value, size_t start_index, size_t size,
                        ZoneVector<BytecodeLabel>& targets);

  void BuildNewLocalActivationContext();
  void BuildLocalActivationContextInitialization();
  void BuildNewLocalBlockContext(Scope* scope);
  void BuildNewLocalCatchContext(Variable* variable, Scope* scope);
  void BuildNewLocalWithContext(Scope* scope);

  void VisitGeneratorPrologue();

  void VisitArgumentsObject(Variable* variable);
  void VisitRestArgumentsArray(Variable* rest);
  void VisitCallSuper(Call* call);
  void VisitClassLiteralProperties(ClassLiteral* expr, Register constructor,
                                   Register prototype);
  void BuildClassLiteralNameProperty(ClassLiteral* expr, Register constructor);
  void VisitThisFunctionVariable(Variable* variable);
  void VisitNewTargetVariable(Variable* variable);
  void VisitBlockDeclarationsAndStatements(Block* stmt);
  void VisitFunctionClosureForContext();
  void VisitSetHomeObject(Register value, Register home_object,
                          LiteralProperty* property, int slot_number = 0);
  void VisitObjectLiteralAccessor(Register home_object,
                                  ObjectLiteralProperty* property,
                                  Register value_out);
  void VisitForInAssignment(Expression* expr, FeedbackVectorSlot slot);
  void VisitModuleNamespaceImports();

  // Visit the header/body of a loop iteration.
  void VisitIterationHeader(IterationStatement* stmt,
                            LoopBuilder* loop_builder);
  void VisitIterationBody(IterationStatement* stmt, LoopBuilder* loop_builder);

  // Visit a statement and switch scopes, the context is in the accumulator.
  void VisitInScope(Statement* stmt, Scope* scope);

  void BuildPushUndefinedIntoRegisterList(RegisterList* reg_list);

  // Visitors for obtaining expression result in the accumulator, in a
  // register, or just getting the effect.
  void VisitForAccumulatorValue(Expression* expr);
  void VisitForAccumulatorValueOrTheHole(Expression* expr);
  MUST_USE_RESULT Register VisitForRegisterValue(Expression* expr);
  void VisitForRegisterValue(Expression* expr, Register destination);
  void VisitAndPushIntoRegisterList(Expression* expr, RegisterList* reg_list);
  void VisitForEffect(Expression* expr);
  void VisitForTest(Expression* expr, BytecodeLabels* then_labels,
                    BytecodeLabels* else_labels, TestFallthrough fallthrough);

  // Returns the runtime function id for a store to super for the function's
  // language mode.
  inline Runtime::FunctionId StoreToSuperRuntimeId();
  inline Runtime::FunctionId StoreKeyedToSuperRuntimeId();

  inline BytecodeArrayBuilder* builder() const { return builder_; }
  inline Zone* zone() const { return zone_; }
  inline DeclarationScope* scope() const { return scope_; }
  inline CompilationInfo* info() const { return info_; }

  inline ControlScope* execution_control() const { return execution_control_; }
  inline void set_execution_control(ControlScope* scope) {
    execution_control_ = scope;
  }
  inline ContextScope* execution_context() const { return execution_context_; }
  inline void set_execution_context(ContextScope* context) {
    execution_context_ = context;
  }
  inline void set_execution_result(ExpressionResultScope* execution_result) {
    execution_result_ = execution_result;
  }
  ExpressionResultScope* execution_result() const { return execution_result_; }
  BytecodeRegisterAllocator* register_allocator() const {
    return builder()->register_allocator();
  }

  GlobalDeclarationsBuilder* globals_builder() { return globals_builder_; }
  inline LanguageMode language_mode() const;
  int feedback_index(FeedbackVectorSlot slot) const;

  Handle<Name> home_object_symbol() const { return home_object_symbol_; }
  Handle<Name> iterator_symbol() const { return iterator_symbol_; }
  Handle<Name> prototype_string() const { return prototype_string_; }
  Handle<FixedArray> empty_fixed_array() const { return empty_fixed_array_; }
  const AstRawString* undefined_string() const { return undefined_string_; }

  Zone* zone_;
  BytecodeArrayBuilder* builder_;
  CompilationInfo* info_;
  DeclarationScope* scope_;

  GlobalDeclarationsBuilder* globals_builder_;
  ZoneVector<GlobalDeclarationsBuilder*> global_declarations_;
  ZoneVector<std::pair<FunctionLiteral*, size_t>> function_literals_;
  ZoneVector<std::pair<NativeFunctionLiteral*, size_t>>
      native_function_literals_;
  ZoneVector<std::pair<ObjectLiteral*, size_t>> object_literals_;
  ZoneVector<std::pair<ArrayLiteral*, size_t>> array_literals_;

  ControlScope* execution_control_;
  ContextScope* execution_context_;
  ExpressionResultScope* execution_result_;

  ZoneVector<BytecodeLabel> generator_resume_points_;
  Register generator_state_;
  int loop_depth_;

  Handle<Name> home_object_symbol_;
  Handle<Name> iterator_symbol_;
  Handle<Name> prototype_string_;
  Handle<FixedArray> empty_fixed_array_;
  const AstRawString* undefined_string_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_GENERATOR_H_

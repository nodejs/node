// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_GENERATOR_H_
#define V8_INTERPRETER_BYTECODE_GENERATOR_H_

#include "src/ast/ast.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeGenerator final : public AstVisitor {
 public:
  BytecodeGenerator(Isolate* isolate, Zone* zone);

  Handle<BytecodeArray> MakeBytecode(CompilationInfo* info);

#define DECLARE_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Visiting function for declarations list and statements are overridden.
  void VisitDeclarations(ZoneList<Declaration*>* declarations) override;
  void VisitStatements(ZoneList<Statement*>* statments) override;

 private:
  class ContextScope;
  class ControlScope;
  class ControlScopeForBreakable;
  class ControlScopeForIteration;
  class ExpressionResultScope;
  class EffectResultScope;
  class AccumulatorResultScope;
  class RegisterResultScope;
  class RegisterAllocationScope;

  void MakeBytecodeBody();
  Register NextContextRegister() const;

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

  // Helper visitors which perform common operations.
  Register VisitArguments(ZoneList<Expression*>* arguments);

  void VisitPropertyLoad(Register obj, Property* expr);
  void VisitPropertyLoadForAccumulator(Register obj, Property* expr);

  void VisitVariableLoad(Variable* variable, FeedbackVectorSlot slot,
                         TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  void VisitVariableLoadForAccumulatorValue(
      Variable* variable, FeedbackVectorSlot slot,
      TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  MUST_USE_RESULT Register
  VisitVariableLoadForRegisterValue(Variable* variable, FeedbackVectorSlot slot,
                                    TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  void VisitVariableAssignment(Variable* variable, FeedbackVectorSlot slot);

  void VisitArgumentsObject(Variable* variable);
  void VisitThisFunctionVariable(Variable* variable);
  void VisitNewTargetVariable(Variable* variable);
  void VisitNewLocalFunctionContext();
  void VisitBuildLocalActivationContext();
  void VisitNewLocalBlockContext(Scope* scope);
  void VisitFunctionClosureForContext();
  void VisitSetHomeObject(Register value, Register home_object,
                          ObjectLiteralProperty* property, int slot_number = 0);
  void VisitObjectLiteralAccessor(Register home_object,
                                  ObjectLiteralProperty* property,
                                  Register value_out);
  void VisitForInAssignment(Expression* expr, FeedbackVectorSlot slot);

  // Visitors for obtaining expression result in the accumulator, in a
  // register, or just getting the effect.
  void VisitForAccumulatorValue(Expression* expression);
  MUST_USE_RESULT Register VisitForRegisterValue(Expression* expression);
  void VisitForEffect(Expression* node);

  // Methods for tracking and remapping register.
  void RecordStoreToRegister(Register reg);
  Register LoadFromAliasedRegister(Register reg);

  inline BytecodeArrayBuilder* builder() { return &builder_; }

  inline Isolate* isolate() const { return isolate_; }
  inline Zone* zone() const { return zone_; }

  inline Scope* scope() const { return scope_; }
  inline void set_scope(Scope* scope) { scope_ = scope; }
  inline CompilationInfo* info() const { return info_; }
  inline void set_info(CompilationInfo* info) { info_ = info; }

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
  inline void set_register_allocator(
      RegisterAllocationScope* register_allocator) {
    register_allocator_ = register_allocator;
  }
  RegisterAllocationScope* register_allocator() const {
    return register_allocator_;
  }

  ZoneVector<Handle<Object>>* globals() { return &globals_; }
  inline LanguageMode language_mode() const;
  Strength language_mode_strength() const;
  int feedback_index(FeedbackVectorSlot slot) const;

  Isolate* isolate_;
  Zone* zone_;
  BytecodeArrayBuilder builder_;
  CompilationInfo* info_;
  Scope* scope_;
  ZoneVector<Handle<Object>> globals_;
  ControlScope* execution_control_;
  ContextScope* execution_context_;
  ExpressionResultScope* execution_result_;
  RegisterAllocationScope* register_allocator_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_GENERATOR_H_

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-generator.h"

#include <stack>

#include "src/compiler.h"
#include "src/objects.h"
#include "src/scopes.h"
#include "src/token.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeGenerator::BytecodeGenerator(Isolate* isolate, Zone* zone)
    : builder_(isolate, zone) {
  InitializeAstVisitor(isolate, zone);
}


BytecodeGenerator::~BytecodeGenerator() {}


Handle<BytecodeArray> BytecodeGenerator::MakeBytecode(CompilationInfo* info) {
  set_info(info);
  set_scope(info->scope());

  // This a temporary guard (oth).
  DCHECK(scope()->is_function_scope());

  builder().set_parameter_count(info->num_parameters_including_this());
  builder().set_locals_count(scope()->num_stack_slots());

  // Visit implicit declaration of the function name.
  if (scope()->is_function_scope() && scope()->function() != NULL) {
    VisitVariableDeclaration(scope()->function());
  }

  // Visit declarations within the function scope.
  VisitDeclarations(scope()->declarations());

  // Visit statements in the function body.
  VisitStatements(info->literal()->body());

  set_scope(nullptr);
  set_info(nullptr);
  return builder_.ToBytecodeArray();
}


void BytecodeGenerator::VisitBlock(Block* node) {
  builder().EnterBlock();
  if (node->scope() == NULL) {
    // Visit statements in the same scope, no declarations.
    VisitStatements(node->statements());
  } else {
    // Visit declarations and statements in a block scope.
    if (node->scope()->ContextLocalCount() > 0) {
      UNIMPLEMENTED();
    } else {
      VisitDeclarations(node->scope()->declarations());
      VisitStatements(node->statements());
    }
  }
  builder().LeaveBlock();
}


void BytecodeGenerator::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  switch (variable->location()) {
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED:
      UNIMPLEMENTED();
      break;
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      // Details stored in scope, i.e. variable index.
      break;
    case VariableLocation::CONTEXT:
    case VariableLocation::LOOKUP:
      UNIMPLEMENTED();
      break;
  }
}


void BytecodeGenerator::VisitFunctionDeclaration(FunctionDeclaration* decl) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitImportDeclaration(ImportDeclaration* decl) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitExportDeclaration(ExportDeclaration* decl) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void BytecodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
  // TODO(oth): For control-flow it could be useful to signal empty paths here.
}


void BytecodeGenerator::VisitIfStatement(IfStatement* stmt) {
  BytecodeLabel else_start, else_end;
  // TODO(oth): Spot easy cases where there code would not need to
  // emit the then block or the else block, e.g. condition is
  // obviously true/1/false/0.
  Visit(stmt->condition());
  builder().CastAccumulatorToBoolean();
  builder().JumpIfFalse(&else_start);

  Visit(stmt->then_statement());
  builder().Jump(&else_end);
  builder().Bind(&else_start);

  Visit(stmt->else_statement());
  builder().Bind(&else_end);
}


void BytecodeGenerator::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* stmt) {
  Visit(stmt->statement());
}


void BytecodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Visit(stmt->expression());
  builder().Return();
}


void BytecodeGenerator::VisitWithStatement(WithStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitCaseClause(CaseClause* clause) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitForStatement(ForStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitForOfStatement(ForOfStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitClassLiteral(ClassLiteral* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitConditional(Conditional* expr) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitLiteral(Literal* expr) {
  Handle<Object> value = expr->value();
  if (value->IsSmi()) {
    builder().LoadLiteral(Smi::cast(*value));
  } else if (value->IsUndefined()) {
    builder().LoadUndefined();
  } else if (value->IsTrue()) {
    builder().LoadTrue();
  } else if (value->IsFalse()) {
    builder().LoadFalse();
  } else if (value->IsNull()) {
    builder().LoadNull();
  } else if (value->IsTheHole()) {
    builder().LoadTheHole();
  } else {
    builder().LoadLiteral(value);
  }
}


void BytecodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitVariableProxy(VariableProxy* proxy) {
  VisitVariableLoad(proxy->var());
}


void BytecodeGenerator::VisitVariableLoad(Variable* variable) {
  switch (variable->location()) {
    case VariableLocation::LOCAL: {
      Register source(variable->index());
      builder().LoadAccumulatorWithRegister(source);
      break;
    }
    case VariableLocation::PARAMETER: {
      // The parameter indices are shifted by 1 (receiver is variable
      // index -1 but is parameter index 0 in BytecodeArrayBuilder).
      Register source(builder().Parameter(variable->index() + 1));
      builder().LoadAccumulatorWithRegister(source);
      break;
    }
    case VariableLocation::GLOBAL: {
      // Global var, const, or let variable.
      // TODO(rmcilroy): If context chain depth is short enough, do this using
      // a generic version of LoadGlobalViaContextStub rather than calling the
      // runtime.
      DCHECK(variable->IsStaticGlobalObjectProperty());
      builder().LoadGlobal(variable->index());
      break;
    }
    case VariableLocation::UNALLOCATED:
    case VariableLocation::CONTEXT:
    case VariableLocation::LOOKUP:
      UNIMPLEMENTED();
  }
}


void BytecodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpression());
  TemporaryRegisterScope temporary_register_scope(&builder_);
  Register object, key;

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do to evaluate variable assignment LHS.
      break;
    case NAMED_PROPERTY:
      object = temporary_register_scope.NewRegister();
      key = temporary_register_scope.NewRegister();
      Visit(property->obj());
      builder().StoreAccumulatorInRegister(object);
      builder().LoadLiteral(property->key()->AsLiteral()->AsPropertyName());
      builder().StoreAccumulatorInRegister(key);
      break;
    case KEYED_PROPERTY:
      object = temporary_register_scope.NewRegister();
      key = temporary_register_scope.NewRegister();
      Visit(property->obj());
      builder().StoreAccumulatorInRegister(object);
      Visit(property->key());
      builder().StoreAccumulatorInRegister(key);
      break;
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNIMPLEMENTED();
  }

  // Evaluate the value and potentially handle compound assignments by loading
  // the left-hand side value and performing a binary operation.
  if (expr->is_compound()) {
    UNIMPLEMENTED();
  } else {
    Visit(expr->value());
  }

  // Store the value.
  FeedbackVectorICSlot slot = expr->AssignmentSlot();
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->target()->AsVariableProxy()->var();
      DCHECK(variable->location() == VariableLocation::LOCAL);
      Register destination(variable->index());
      builder().StoreAccumulatorInRegister(destination);
      break;
    }
    case NAMED_PROPERTY:
      builder().StoreNamedProperty(object, key, feedback_index(slot),
                                   language_mode());
      break;
    case KEYED_PROPERTY:
      builder().StoreKeyedProperty(object, key, feedback_index(slot),
                                   language_mode());
      break;
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNIMPLEMENTED();
  }
}


void BytecodeGenerator::VisitYield(Yield* expr) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitThrow(Throw* expr) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitPropertyLoad(Register obj, Property* expr) {
  LhsKind property_kind = Property::GetAssignType(expr);
  FeedbackVectorICSlot slot = expr->PropertyFeedbackSlot();
  switch (property_kind) {
    case VARIABLE:
      UNREACHABLE();
    case NAMED_PROPERTY: {
      builder().LoadLiteral(expr->key()->AsLiteral()->AsPropertyName());
      builder().LoadNamedProperty(obj, feedback_index(slot), language_mode());
      break;
    }
    case KEYED_PROPERTY: {
      Visit(expr->key());
      builder().LoadKeyedProperty(obj, feedback_index(slot), language_mode());
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNIMPLEMENTED();
  }
}


void BytecodeGenerator::VisitProperty(Property* expr) {
  TemporaryRegisterScope temporary_register_scope(&builder_);
  Register obj = temporary_register_scope.NewRegister();
  Visit(expr->obj());
  builder().StoreAccumulatorInRegister(obj);
  VisitPropertyLoad(obj, expr);
}


void BytecodeGenerator::VisitCall(Call* expr) {
  Expression* callee_expr = expr->expression();
  Call::CallType call_type = expr->GetCallType(isolate());

  // Prepare the callee and the receiver to the function call. This depends on
  // the semantics of the underlying call type.
  TemporaryRegisterScope temporary_register_scope(&builder_);
  Register callee = temporary_register_scope.NewRegister();
  Register receiver = temporary_register_scope.NewRegister();

  switch (call_type) {
    case Call::PROPERTY_CALL: {
      Property* property = callee_expr->AsProperty();
      if (property->IsSuperAccess()) {
        UNIMPLEMENTED();
      }
      Visit(property->obj());
      builder().StoreAccumulatorInRegister(receiver);
      // Perform a property load of the callee.
      VisitPropertyLoad(receiver, property);
      builder().StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::GLOBAL_CALL: {
      // Receiver is undefined for global calls.
      builder().LoadUndefined().StoreAccumulatorInRegister(receiver);
      // Load callee as a global variable.
      VariableProxy* proxy = callee_expr->AsVariableProxy();
      VisitVariableLoad(proxy->var());
      builder().StoreAccumulatorInRegister(callee);
      break;
    }
    case Call::LOOKUP_SLOT_CALL:
    case Call::SUPER_CALL:
    case Call::POSSIBLY_EVAL_CALL:
    case Call::OTHER_CALL:
      UNIMPLEMENTED();
  }

  // Evaluate all arguments to the function call and store in sequential
  // registers.
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Visit(args->at(i));
    Register arg = temporary_register_scope.NewRegister();
    DCHECK(arg.index() - i == receiver.index() + 1);
    builder().StoreAccumulatorInRegister(arg);
  }

  // TODO(rmcilroy): Deal with possible direct eval here?
  // TODO(rmcilroy): Use CallIC to allow call type feedback.
  builder().Call(callee, receiver, args->length());
}


void BytecodeGenerator::VisitCallNew(CallNew* expr) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitCallRuntime(CallRuntime* expr) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitCountOperation(CountOperation* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitBinaryOperation(BinaryOperation* binop) {
  switch (binop->op()) {
    case Token::COMMA:
    case Token::OR:
    case Token::AND:
      UNIMPLEMENTED();
      break;
    default:
      VisitArithmeticExpression(binop);
      break;
  }
}


void BytecodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Token::Value op = expr->op();
  Expression* left = expr->left();
  Expression* right = expr->right();

  TemporaryRegisterScope temporary_register_scope(&builder_);
  Register temporary = temporary_register_scope.NewRegister();

  Visit(left);
  builder().StoreAccumulatorInRegister(temporary);
  Visit(right);
  builder().CompareOperation(op, temporary, language_mode());
}


void BytecodeGenerator::VisitSpread(Spread* expr) { UNREACHABLE(); }


void BytecodeGenerator::VisitEmptyParentheses(EmptyParentheses* expr) {
  UNREACHABLE();
}


void BytecodeGenerator::VisitThisFunction(ThisFunction* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitSuperCallReference(SuperCallReference* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitSuperPropertyReference(
    SuperPropertyReference* expr) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitArithmeticExpression(BinaryOperation* binop) {
  Token::Value op = binop->op();
  Expression* left = binop->left();
  Expression* right = binop->right();

  TemporaryRegisterScope temporary_register_scope(&builder_);
  Register temporary = temporary_register_scope.NewRegister();

  Visit(left);
  builder().StoreAccumulatorInRegister(temporary);
  Visit(right);
  builder().BinaryOperation(op, temporary);
}


LanguageMode BytecodeGenerator::language_mode() const {
  return info()->language_mode();
}


int BytecodeGenerator::feedback_index(FeedbackVectorICSlot slot) const {
  return info()->feedback_vector()->GetIndex(slot);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

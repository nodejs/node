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
    : builder_(isolate) {
  InitializeAstVisitor(isolate, zone);
}


BytecodeGenerator::~BytecodeGenerator() {}


Handle<BytecodeArray> BytecodeGenerator::MakeBytecode(CompilationInfo* info) {
  set_scope(info->scope());

  // This a temporary guard (oth).
  DCHECK(scope()->is_function_scope());

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
  return builder_.ToBytecodeArray();
}


void BytecodeGenerator::VisitBlock(Block* node) {
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
}


void BytecodeGenerator::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  switch (variable->location()) {
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED:
      UNIMPLEMENTED();
      break;
    case VariableLocation::PARAMETER:
      UNIMPLEMENTED();
      break;
    case VariableLocation::LOCAL:
      // Details stored in scope, i.e. variable index.
      break;
    case VariableLocation::CONTEXT:
    case VariableLocation::LOOKUP:
      UNIMPLEMENTED();
      break;
  }
}


void BytecodeGenerator::VisitFunctionDeclaration(FunctionDeclaration* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitImportDeclaration(ImportDeclaration* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitExportDeclaration(ExportDeclaration* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void BytecodeGenerator::VisitEmptyStatement(EmptyStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitIfStatement(IfStatement* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitContinueStatement(ContinueStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitBreakStatement(BreakStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitReturnStatement(ReturnStatement* node) {
  Visit(node->expression());
  builder().Return();
}


void BytecodeGenerator::VisitWithStatement(WithStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitSwitchStatement(SwitchStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitCaseClause(CaseClause* clause) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitDoWhileStatement(DoWhileStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitWhileStatement(WhileStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitForStatement(ForStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitForInStatement(ForInStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitForOfStatement(ForOfStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitTryCatchStatement(TryCatchStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitDebuggerStatement(DebuggerStatement* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitClassLiteral(ClassLiteral* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitConditional(Conditional* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitLiteral(Literal* expr) {
  if (expr->IsPropertyName()) {
    UNIMPLEMENTED();
  }

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
    UNIMPLEMENTED();
  }
}


void BytecodeGenerator::VisitRegExpLiteral(RegExpLiteral* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitObjectLiteral(ObjectLiteral* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitArrayLiteral(ArrayLiteral* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitVariableProxy(VariableProxy* proxy) {
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::LOCAL: {
      Register source(variable->index());
      builder().LoadAccumulatorWithRegister(source);
      break;
    }
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED:
    case VariableLocation::PARAMETER:
    case VariableLocation::CONTEXT:
    case VariableLocation::LOOKUP:
      UNIMPLEMENTED();
  }
}


void BytecodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpression());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  DCHECK(!expr->is_compound());
  Visit(expr->value());

  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->target()->AsVariableProxy()->var();
      DCHECK(variable->location() == VariableLocation::LOCAL);
      Register destination(variable->index());
      builder().StoreAccumulatorInRegister(destination);
      break;
    }
    case NAMED_PROPERTY:
    case KEYED_PROPERTY:
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNIMPLEMENTED();
  }
}


void BytecodeGenerator::VisitYield(Yield* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitThrow(Throw* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitProperty(Property* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitCall(Call* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitCallNew(CallNew* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitCallRuntime(CallRuntime* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitUnaryOperation(UnaryOperation* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitCountOperation(CountOperation* node) {
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


void BytecodeGenerator::VisitCompareOperation(CompareOperation* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitSpread(Spread* node) { UNIMPLEMENTED(); }


void BytecodeGenerator::VisitThisFunction(ThisFunction* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitSuperCallReference(SuperCallReference* node) {
  UNIMPLEMENTED();
}


void BytecodeGenerator::VisitSuperPropertyReference(
    SuperPropertyReference* node) {
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

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

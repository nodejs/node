// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"
#include "src/ast/ast-expression-rewriter.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Implementation of AstExpressionRewriter
// The AST is traversed but no actual rewriting takes place, unless the
// Visit methods are overriden in subclasses.

#define REWRITE_THIS(node)                \
  do {                                    \
    if (!RewriteExpression(node)) return; \
  } while (false)
#define NOTHING() DCHECK_NULL(replacement_)


void AstExpressionRewriter::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
  for (int i = 0; i < declarations->length(); i++) {
    AST_REWRITE_LIST_ELEMENT(Declaration, declarations, i);
  }
}


void AstExpressionRewriter::VisitStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    AST_REWRITE_LIST_ELEMENT(Statement, statements, i);
    // Not stopping when a jump statement is found.
  }
}


void AstExpressionRewriter::VisitExpressions(
    ZoneList<Expression*>* expressions) {
  for (int i = 0; i < expressions->length(); i++) {
    // The variable statement visiting code may pass NULL expressions
    // to this code. Maybe this should be handled by introducing an
    // undefined expression or literal?  Revisit this code if this
    // changes
    if (expressions->at(i) != nullptr) {
      AST_REWRITE_LIST_ELEMENT(Expression, expressions, i);
    }
  }
}


void AstExpressionRewriter::VisitVariableDeclaration(
    VariableDeclaration* node) {
  // Not visiting `proxy_`.
  NOTHING();
}


void AstExpressionRewriter::VisitFunctionDeclaration(
    FunctionDeclaration* node) {
  // Not visiting `proxy_`.
  AST_REWRITE_PROPERTY(FunctionLiteral, node, fun);
}


void AstExpressionRewriter::VisitBlock(Block* node) {
  VisitStatements(node->statements());
}


void AstExpressionRewriter::VisitExpressionStatement(
    ExpressionStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, expression);
}


void AstExpressionRewriter::VisitEmptyStatement(EmptyStatement* node) {
  NOTHING();
}


void AstExpressionRewriter::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* node) {
  AST_REWRITE_PROPERTY(Statement, node, statement);
}


void AstExpressionRewriter::VisitIfStatement(IfStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, condition);
  AST_REWRITE_PROPERTY(Statement, node, then_statement);
  AST_REWRITE_PROPERTY(Statement, node, else_statement);
}


void AstExpressionRewriter::VisitContinueStatement(ContinueStatement* node) {
  NOTHING();
}


void AstExpressionRewriter::VisitBreakStatement(BreakStatement* node) {
  NOTHING();
}


void AstExpressionRewriter::VisitReturnStatement(ReturnStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, expression);
}


void AstExpressionRewriter::VisitWithStatement(WithStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, expression);
  AST_REWRITE_PROPERTY(Statement, node, statement);
}


void AstExpressionRewriter::VisitSwitchStatement(SwitchStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, tag);
  ZoneList<CaseClause*>* clauses = node->cases();
  for (int i = 0; i < clauses->length(); i++) {
    AST_REWRITE_LIST_ELEMENT(CaseClause, clauses, i);
  }
}


void AstExpressionRewriter::VisitDoWhileStatement(DoWhileStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, cond);
  AST_REWRITE_PROPERTY(Statement, node, body);
}


void AstExpressionRewriter::VisitWhileStatement(WhileStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, cond);
  AST_REWRITE_PROPERTY(Statement, node, body);
}


void AstExpressionRewriter::VisitForStatement(ForStatement* node) {
  if (node->init() != nullptr) {
    AST_REWRITE_PROPERTY(Statement, node, init);
  }
  if (node->cond() != nullptr) {
    AST_REWRITE_PROPERTY(Expression, node, cond);
  }
  if (node->next() != nullptr) {
    AST_REWRITE_PROPERTY(Statement, node, next);
  }
  AST_REWRITE_PROPERTY(Statement, node, body);
}


void AstExpressionRewriter::VisitForInStatement(ForInStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, each);
  AST_REWRITE_PROPERTY(Expression, node, subject);
  AST_REWRITE_PROPERTY(Statement, node, body);
}


void AstExpressionRewriter::VisitForOfStatement(ForOfStatement* node) {
  AST_REWRITE_PROPERTY(Expression, node, assign_iterator);
  AST_REWRITE_PROPERTY(Expression, node, next_result);
  AST_REWRITE_PROPERTY(Expression, node, result_done);
  AST_REWRITE_PROPERTY(Expression, node, assign_each);
  AST_REWRITE_PROPERTY(Statement, node, body);
}


void AstExpressionRewriter::VisitTryCatchStatement(TryCatchStatement* node) {
  AST_REWRITE_PROPERTY(Block, node, try_block);
  // Not visiting the variable.
  AST_REWRITE_PROPERTY(Block, node, catch_block);
}


void AstExpressionRewriter::VisitTryFinallyStatement(
    TryFinallyStatement* node) {
  AST_REWRITE_PROPERTY(Block, node, try_block);
  AST_REWRITE_PROPERTY(Block, node, finally_block);
}


void AstExpressionRewriter::VisitDebuggerStatement(DebuggerStatement* node) {
  NOTHING();
}


void AstExpressionRewriter::VisitFunctionLiteral(FunctionLiteral* node) {
  REWRITE_THIS(node);
  VisitDeclarations(node->scope()->declarations());
  ZoneList<Statement*>* body = node->body();
  if (body != nullptr) VisitStatements(body);
}


void AstExpressionRewriter::VisitClassLiteral(ClassLiteral* node) {
  REWRITE_THIS(node);
  // Not visiting `class_variable_proxy_`.
  if (node->extends() != nullptr) {
    AST_REWRITE_PROPERTY(Expression, node, extends);
  }
  AST_REWRITE_PROPERTY(FunctionLiteral, node, constructor);
  ZoneList<typename ClassLiteral::Property*>* properties = node->properties();
  for (int i = 0; i < properties->length(); i++) {
    VisitLiteralProperty(properties->at(i));
  }
}

void AstExpressionRewriter::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* node) {
  REWRITE_THIS(node);
  NOTHING();
}


void AstExpressionRewriter::VisitConditional(Conditional* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, condition);
  AST_REWRITE_PROPERTY(Expression, node, then_expression);
  AST_REWRITE_PROPERTY(Expression, node, else_expression);
}


void AstExpressionRewriter::VisitVariableProxy(VariableProxy* node) {
  REWRITE_THIS(node);
  NOTHING();
}


void AstExpressionRewriter::VisitLiteral(Literal* node) {
  REWRITE_THIS(node);
  NOTHING();
}


void AstExpressionRewriter::VisitRegExpLiteral(RegExpLiteral* node) {
  REWRITE_THIS(node);
  NOTHING();
}


void AstExpressionRewriter::VisitObjectLiteral(ObjectLiteral* node) {
  REWRITE_THIS(node);
  ZoneList<typename ObjectLiteral::Property*>* properties = node->properties();
  for (int i = 0; i < properties->length(); i++) {
    VisitLiteralProperty(properties->at(i));
  }
}

void AstExpressionRewriter::VisitLiteralProperty(LiteralProperty* property) {
  if (property == nullptr) return;
  AST_REWRITE_PROPERTY(Expression, property, key);
  AST_REWRITE_PROPERTY(Expression, property, value);
}


void AstExpressionRewriter::VisitArrayLiteral(ArrayLiteral* node) {
  REWRITE_THIS(node);
  VisitExpressions(node->values());
}


void AstExpressionRewriter::VisitAssignment(Assignment* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, target);
  AST_REWRITE_PROPERTY(Expression, node, value);
}


void AstExpressionRewriter::VisitYield(Yield* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, generator_object);
  AST_REWRITE_PROPERTY(Expression, node, expression);
}


void AstExpressionRewriter::VisitThrow(Throw* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, exception);
}


void AstExpressionRewriter::VisitProperty(Property* node) {
  REWRITE_THIS(node);
  if (node == nullptr) return;
  AST_REWRITE_PROPERTY(Expression, node, obj);
  AST_REWRITE_PROPERTY(Expression, node, key);
}


void AstExpressionRewriter::VisitCall(Call* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, expression);
  VisitExpressions(node->arguments());
}


void AstExpressionRewriter::VisitCallNew(CallNew* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, expression);
  VisitExpressions(node->arguments());
}


void AstExpressionRewriter::VisitCallRuntime(CallRuntime* node) {
  REWRITE_THIS(node);
  VisitExpressions(node->arguments());
}


void AstExpressionRewriter::VisitUnaryOperation(UnaryOperation* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, expression);
}


void AstExpressionRewriter::VisitCountOperation(CountOperation* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, expression);
}


void AstExpressionRewriter::VisitBinaryOperation(BinaryOperation* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, left);
  AST_REWRITE_PROPERTY(Expression, node, right);
}


void AstExpressionRewriter::VisitCompareOperation(CompareOperation* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, left);
  AST_REWRITE_PROPERTY(Expression, node, right);
}


void AstExpressionRewriter::VisitSpread(Spread* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Expression, node, expression);
}


void AstExpressionRewriter::VisitThisFunction(ThisFunction* node) {
  REWRITE_THIS(node);
  NOTHING();
}


void AstExpressionRewriter::VisitSuperPropertyReference(
    SuperPropertyReference* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(VariableProxy, node, this_var);
  AST_REWRITE_PROPERTY(Expression, node, home_object);
}


void AstExpressionRewriter::VisitSuperCallReference(SuperCallReference* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(VariableProxy, node, this_var);
  AST_REWRITE_PROPERTY(VariableProxy, node, new_target_var);
  AST_REWRITE_PROPERTY(VariableProxy, node, this_function_var);
}


void AstExpressionRewriter::VisitCaseClause(CaseClause* node) {
  if (!node->is_default()) {
    AST_REWRITE_PROPERTY(Expression, node, label);
  }
  VisitStatements(node->statements());
}


void AstExpressionRewriter::VisitEmptyParentheses(EmptyParentheses* node) {
  NOTHING();
}


void AstExpressionRewriter::VisitDoExpression(DoExpression* node) {
  REWRITE_THIS(node);
  AST_REWRITE_PROPERTY(Block, node, block);
  AST_REWRITE_PROPERTY(VariableProxy, node, result);
}


void AstExpressionRewriter::VisitRewritableExpression(
    RewritableExpression* node) {
  REWRITE_THIS(node);
  AST_REWRITE(Expression, node->expression(), node->Rewrite(replacement));
}


}  // namespace internal
}  // namespace v8

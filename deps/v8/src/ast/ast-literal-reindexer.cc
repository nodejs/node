// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast-literal-reindexer.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {


void AstLiteralReindexer::VisitVariableDeclaration(VariableDeclaration* node) {
  VisitVariableProxy(node->proxy());
}


void AstLiteralReindexer::VisitEmptyStatement(EmptyStatement* node) {}


void AstLiteralReindexer::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* node) {
  Visit(node->statement());
}


void AstLiteralReindexer::VisitContinueStatement(ContinueStatement* node) {}


void AstLiteralReindexer::VisitBreakStatement(BreakStatement* node) {}


void AstLiteralReindexer::VisitDebuggerStatement(DebuggerStatement* node) {}


void AstLiteralReindexer::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* node) {}


void AstLiteralReindexer::VisitDoExpression(DoExpression* node) {
  Visit(node->block());
  Visit(node->result());
}


void AstLiteralReindexer::VisitLiteral(Literal* node) {}


void AstLiteralReindexer::VisitRegExpLiteral(RegExpLiteral* node) {
  UpdateIndex(node);
}


void AstLiteralReindexer::VisitVariableProxy(VariableProxy* node) {}


void AstLiteralReindexer::VisitThisFunction(ThisFunction* node) {}


void AstLiteralReindexer::VisitSuperPropertyReference(
    SuperPropertyReference* node) {
  Visit(node->this_var());
  Visit(node->home_object());
}


void AstLiteralReindexer::VisitSuperCallReference(SuperCallReference* node) {
  Visit(node->this_var());
  Visit(node->new_target_var());
  Visit(node->this_function_var());
}


void AstLiteralReindexer::VisitRewritableExpression(
    RewritableExpression* node) {
  Visit(node->expression());
}


void AstLiteralReindexer::VisitExpressionStatement(ExpressionStatement* node) {
  Visit(node->expression());
}


void AstLiteralReindexer::VisitReturnStatement(ReturnStatement* node) {
  Visit(node->expression());
}


void AstLiteralReindexer::VisitYield(Yield* node) {
  Visit(node->generator_object());
  Visit(node->expression());
}


void AstLiteralReindexer::VisitThrow(Throw* node) { Visit(node->exception()); }


void AstLiteralReindexer::VisitUnaryOperation(UnaryOperation* node) {
  Visit(node->expression());
}


void AstLiteralReindexer::VisitCountOperation(CountOperation* node) {
  Visit(node->expression());
}


void AstLiteralReindexer::VisitBlock(Block* node) {
  VisitStatements(node->statements());
}


void AstLiteralReindexer::VisitFunctionDeclaration(FunctionDeclaration* node) {
  VisitVariableProxy(node->proxy());
  VisitFunctionLiteral(node->fun());
}


void AstLiteralReindexer::VisitCallRuntime(CallRuntime* node) {
  VisitArguments(node->arguments());
}


void AstLiteralReindexer::VisitWithStatement(WithStatement* node) {
  Visit(node->expression());
  Visit(node->statement());
}


void AstLiteralReindexer::VisitDoWhileStatement(DoWhileStatement* node) {
  Visit(node->body());
  Visit(node->cond());
}


void AstLiteralReindexer::VisitWhileStatement(WhileStatement* node) {
  Visit(node->cond());
  Visit(node->body());
}


void AstLiteralReindexer::VisitTryCatchStatement(TryCatchStatement* node) {
  Visit(node->try_block());
  Visit(node->catch_block());
}


void AstLiteralReindexer::VisitTryFinallyStatement(TryFinallyStatement* node) {
  Visit(node->try_block());
  Visit(node->finally_block());
}


void AstLiteralReindexer::VisitProperty(Property* node) {
  Visit(node->key());
  Visit(node->obj());
}


void AstLiteralReindexer::VisitAssignment(Assignment* node) {
  Visit(node->target());
  Visit(node->value());
}


void AstLiteralReindexer::VisitBinaryOperation(BinaryOperation* node) {
  Visit(node->left());
  Visit(node->right());
}


void AstLiteralReindexer::VisitCompareOperation(CompareOperation* node) {
  Visit(node->left());
  Visit(node->right());
}


void AstLiteralReindexer::VisitSpread(Spread* node) {
  // This is reachable because ParserBase::ParseArrowFunctionLiteral calls
  // ReindexLiterals before calling RewriteDestructuringAssignments.
  Visit(node->expression());
}


void AstLiteralReindexer::VisitEmptyParentheses(EmptyParentheses* node) {}

void AstLiteralReindexer::VisitGetIterator(GetIterator* node) {
  Visit(node->iterable());
}

void AstLiteralReindexer::VisitForInStatement(ForInStatement* node) {
  Visit(node->each());
  Visit(node->enumerable());
  Visit(node->body());
}


void AstLiteralReindexer::VisitForOfStatement(ForOfStatement* node) {
  Visit(node->assign_iterator());
  Visit(node->next_result());
  Visit(node->result_done());
  Visit(node->assign_each());
  Visit(node->body());
}


void AstLiteralReindexer::VisitConditional(Conditional* node) {
  Visit(node->condition());
  Visit(node->then_expression());
  Visit(node->else_expression());
}


void AstLiteralReindexer::VisitIfStatement(IfStatement* node) {
  Visit(node->condition());
  Visit(node->then_statement());
  if (node->HasElseStatement()) {
    Visit(node->else_statement());
  }
}


void AstLiteralReindexer::VisitSwitchStatement(SwitchStatement* node) {
  Visit(node->tag());
  ZoneList<CaseClause*>* cases = node->cases();
  for (int i = 0; i < cases->length(); i++) {
    VisitCaseClause(cases->at(i));
  }
}


void AstLiteralReindexer::VisitCaseClause(CaseClause* node) {
  if (!node->is_default()) Visit(node->label());
  VisitStatements(node->statements());
}


void AstLiteralReindexer::VisitForStatement(ForStatement* node) {
  if (node->init() != NULL) Visit(node->init());
  if (node->cond() != NULL) Visit(node->cond());
  if (node->next() != NULL) Visit(node->next());
  Visit(node->body());
}


void AstLiteralReindexer::VisitClassLiteral(ClassLiteral* node) {
  if (node->extends()) Visit(node->extends());
  if (node->constructor()) Visit(node->constructor());
  if (node->class_variable_proxy()) {
    VisitVariableProxy(node->class_variable_proxy());
  }
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitLiteralProperty(node->properties()->at(i));
  }
}

void AstLiteralReindexer::VisitObjectLiteral(ObjectLiteral* node) {
  UpdateIndex(node);
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitLiteralProperty(node->properties()->at(i));
  }
}

void AstLiteralReindexer::VisitLiteralProperty(LiteralProperty* node) {
  Visit(node->key());
  Visit(node->value());
}


void AstLiteralReindexer::VisitArrayLiteral(ArrayLiteral* node) {
  UpdateIndex(node);
  for (int i = 0; i < node->values()->length(); i++) {
    Visit(node->values()->at(i));
  }
}


void AstLiteralReindexer::VisitCall(Call* node) {
  Visit(node->expression());
  VisitArguments(node->arguments());
}


void AstLiteralReindexer::VisitCallNew(CallNew* node) {
  Visit(node->expression());
  VisitArguments(node->arguments());
}


void AstLiteralReindexer::VisitStatements(ZoneList<Statement*>* statements) {
  if (statements == NULL) return;
  for (int i = 0; i < statements->length(); i++) {
    Visit(statements->at(i));
  }
}


void AstLiteralReindexer::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
  for (int i = 0; i < declarations->length(); i++) {
    Visit(declarations->at(i));
  }
}


void AstLiteralReindexer::VisitArguments(ZoneList<Expression*>* arguments) {
  for (int i = 0; i < arguments->length(); i++) {
    Visit(arguments->at(i));
  }
}


void AstLiteralReindexer::VisitFunctionLiteral(FunctionLiteral* node) {
  // We don't recurse into the declarations or body of the function literal:
}

void AstLiteralReindexer::Reindex(Expression* pattern) { Visit(pattern); }
}  // namespace internal
}  // namespace v8

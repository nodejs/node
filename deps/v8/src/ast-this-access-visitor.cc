// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast-this-access-visitor.h"
#include "src/parser.h"

namespace v8 {
namespace internal {

typedef class AstThisAccessVisitor ATAV;  // for code shortitude.

ATAV::AstThisAccessVisitor(Zone* zone) : uses_this_(false) {
  InitializeAstVisitor(zone);
}


void ATAV::VisitVariableProxy(VariableProxy* proxy) {
  if (proxy->is_this()) {
    uses_this_ = true;
  }
}


void ATAV::VisitSuperReference(SuperReference* leaf) {
  // disallow super.method() and super(...).
  uses_this_ = true;
}


void ATAV::VisitCallNew(CallNew* e) {
  // new super(..) does not use 'this'.
  if (!e->expression()->IsSuperReference()) {
    Visit(e->expression());
  }
  VisitExpressions(e->arguments());
}


// ---------------------------------------------------------------------------
// -- Leaf nodes -------------------------------------------------------------
// ---------------------------------------------------------------------------

void ATAV::VisitVariableDeclaration(VariableDeclaration* leaf) {}
void ATAV::VisitFunctionDeclaration(FunctionDeclaration* leaf) {}
void ATAV::VisitModuleDeclaration(ModuleDeclaration* leaf) {}
void ATAV::VisitImportDeclaration(ImportDeclaration* leaf) {}
void ATAV::VisitExportDeclaration(ExportDeclaration* leaf) {}
void ATAV::VisitModuleVariable(ModuleVariable* leaf) {}
void ATAV::VisitModulePath(ModulePath* leaf) {}
void ATAV::VisitModuleUrl(ModuleUrl* leaf) {}
void ATAV::VisitEmptyStatement(EmptyStatement* leaf) {}
void ATAV::VisitContinueStatement(ContinueStatement* leaf) {}
void ATAV::VisitBreakStatement(BreakStatement* leaf) {}
void ATAV::VisitDebuggerStatement(DebuggerStatement* leaf) {}
void ATAV::VisitFunctionLiteral(FunctionLiteral* leaf) {}
void ATAV::VisitNativeFunctionLiteral(NativeFunctionLiteral* leaf) {}
void ATAV::VisitLiteral(Literal* leaf) {}
void ATAV::VisitRegExpLiteral(RegExpLiteral* leaf) {}
void ATAV::VisitThisFunction(ThisFunction* leaf) {}

// ---------------------------------------------------------------------------
// -- Pass-through nodes------------------------------------------------------
// ---------------------------------------------------------------------------
void ATAV::VisitModuleLiteral(ModuleLiteral* e) { Visit(e->body()); }


void ATAV::VisitBlock(Block* stmt) { VisitStatements(stmt->statements()); }


void ATAV::VisitExpressionStatement(ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void ATAV::VisitIfStatement(IfStatement* stmt) {
  Visit(stmt->condition());
  Visit(stmt->then_statement());
  Visit(stmt->else_statement());
}


void ATAV::VisitReturnStatement(ReturnStatement* stmt) {
  Visit(stmt->expression());
}


void ATAV::VisitWithStatement(WithStatement* stmt) {
  Visit(stmt->expression());
  Visit(stmt->statement());
}


void ATAV::VisitSwitchStatement(SwitchStatement* stmt) {
  Visit(stmt->tag());
  ZoneList<CaseClause*>* clauses = stmt->cases();
  for (int i = 0; i < clauses->length(); i++) {
    Visit(clauses->at(i));
  }
}


void ATAV::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  Visit(stmt->try_block());
  Visit(stmt->finally_block());
}


void ATAV::VisitClassLiteral(ClassLiteral* e) {
  VisitIfNotNull(e->extends());
  Visit(e->constructor());
  ZoneList<ObjectLiteralProperty*>* properties = e->properties();
  for (int i = 0; i < properties->length(); i++) {
    Visit(properties->at(i)->value());
  }
}


void ATAV::VisitConditional(Conditional* e) {
  Visit(e->condition());
  Visit(e->then_expression());
  Visit(e->else_expression());
}


void ATAV::VisitObjectLiteral(ObjectLiteral* e) {
  ZoneList<ObjectLiteralProperty*>* properties = e->properties();
  for (int i = 0; i < properties->length(); i++) {
    Visit(properties->at(i)->value());
  }
}


void ATAV::VisitArrayLiteral(ArrayLiteral* e) { VisitExpressions(e->values()); }


void ATAV::VisitYield(Yield* stmt) {
  Visit(stmt->generator_object());
  Visit(stmt->expression());
}


void ATAV::VisitThrow(Throw* stmt) { Visit(stmt->exception()); }


void ATAV::VisitProperty(Property* e) {
  Visit(e->obj());
  Visit(e->key());
}


void ATAV::VisitCall(Call* e) {
  Visit(e->expression());
  VisitExpressions(e->arguments());
}


void ATAV::VisitCallRuntime(CallRuntime* e) {
  VisitExpressions(e->arguments());
}


void ATAV::VisitUnaryOperation(UnaryOperation* e) { Visit(e->expression()); }


void ATAV::VisitBinaryOperation(BinaryOperation* e) {
  Visit(e->left());
  Visit(e->right());
}


void ATAV::VisitCompareOperation(CompareOperation* e) {
  Visit(e->left());
  Visit(e->right());
}


void ATAV::VisitCaseClause(CaseClause* cc) {
  if (!cc->is_default()) Visit(cc->label());
  VisitStatements(cc->statements());
}


void ATAV::VisitModuleStatement(ModuleStatement* stmt) { Visit(stmt->body()); }


void ATAV::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Visit(stmt->try_block());
  Visit(stmt->catch_block());
}


void ATAV::VisitDoWhileStatement(DoWhileStatement* loop) {
  Visit(loop->body());
  Visit(loop->cond());
}


void ATAV::VisitWhileStatement(WhileStatement* loop) {
  Visit(loop->cond());
  Visit(loop->body());
}


void ATAV::VisitForStatement(ForStatement* loop) {
  VisitIfNotNull(loop->init());
  VisitIfNotNull(loop->cond());
  Visit(loop->body());
  VisitIfNotNull(loop->next());
}


void ATAV::VisitForInStatement(ForInStatement* loop) {
  Visit(loop->each());
  Visit(loop->subject());
  Visit(loop->body());
}


void ATAV::VisitForOfStatement(ForOfStatement* loop) {
  Visit(loop->each());
  Visit(loop->subject());
  Visit(loop->body());
}


void ATAV::VisitAssignment(Assignment* stmt) {
  Expression* l = stmt->target();
  Visit(l);
  Visit(stmt->value());
}


void ATAV::VisitCountOperation(CountOperation* e) {
  Expression* l = e->expression();
  Visit(l);
}
}
}  // namespace v8::internal

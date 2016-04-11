// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/ast-loop-assignment-analyzer.h"
#include "src/compiler.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {
namespace compiler {

typedef class AstLoopAssignmentAnalyzer ALAA;  // for code shortitude.

ALAA::AstLoopAssignmentAnalyzer(Zone* zone, CompilationInfo* info)
    : info_(info), zone_(zone), loop_stack_(zone) {
  InitializeAstVisitor(info->isolate());
}


LoopAssignmentAnalysis* ALAA::Analyze() {
  LoopAssignmentAnalysis* a = new (zone_) LoopAssignmentAnalysis(zone_);
  result_ = a;
  VisitStatements(info()->literal()->body());
  result_ = nullptr;
  return a;
}


void ALAA::Enter(IterationStatement* loop) {
  int num_variables = 1 + info()->scope()->num_parameters() +
                      info()->scope()->num_stack_slots();
  BitVector* bits = new (zone_) BitVector(num_variables, zone_);
  if (info()->is_osr() && info()->osr_ast_id() == loop->OsrEntryId())
    bits->AddAll();
  loop_stack_.push_back(bits);
}


void ALAA::Exit(IterationStatement* loop) {
  DCHECK(loop_stack_.size() > 0);
  BitVector* bits = loop_stack_.back();
  loop_stack_.pop_back();
  if (!loop_stack_.empty()) {
    loop_stack_.back()->Union(*bits);
  }
  result_->list_.push_back(
      std::pair<IterationStatement*, BitVector*>(loop, bits));
}


// ---------------------------------------------------------------------------
// -- Leaf nodes -------------------------------------------------------------
// ---------------------------------------------------------------------------

void ALAA::VisitVariableDeclaration(VariableDeclaration* leaf) {}
void ALAA::VisitFunctionDeclaration(FunctionDeclaration* leaf) {}
void ALAA::VisitImportDeclaration(ImportDeclaration* leaf) {}
void ALAA::VisitExportDeclaration(ExportDeclaration* leaf) {}
void ALAA::VisitEmptyStatement(EmptyStatement* leaf) {}
void ALAA::VisitContinueStatement(ContinueStatement* leaf) {}
void ALAA::VisitBreakStatement(BreakStatement* leaf) {}
void ALAA::VisitDebuggerStatement(DebuggerStatement* leaf) {}
void ALAA::VisitFunctionLiteral(FunctionLiteral* leaf) {}
void ALAA::VisitNativeFunctionLiteral(NativeFunctionLiteral* leaf) {}
void ALAA::VisitVariableProxy(VariableProxy* leaf) {}
void ALAA::VisitLiteral(Literal* leaf) {}
void ALAA::VisitRegExpLiteral(RegExpLiteral* leaf) {}
void ALAA::VisitThisFunction(ThisFunction* leaf) {}
void ALAA::VisitSuperPropertyReference(SuperPropertyReference* leaf) {}
void ALAA::VisitSuperCallReference(SuperCallReference* leaf) {}


// ---------------------------------------------------------------------------
// -- Pass-through nodes------------------------------------------------------
// ---------------------------------------------------------------------------
void ALAA::VisitBlock(Block* stmt) { VisitStatements(stmt->statements()); }


void ALAA::VisitDoExpression(DoExpression* expr) {
  Visit(expr->block());
  Visit(expr->result());
}


void ALAA::VisitExpressionStatement(ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void ALAA::VisitIfStatement(IfStatement* stmt) {
  Visit(stmt->condition());
  Visit(stmt->then_statement());
  Visit(stmt->else_statement());
}


void ALAA::VisitReturnStatement(ReturnStatement* stmt) {
  Visit(stmt->expression());
}


void ALAA::VisitWithStatement(WithStatement* stmt) {
  Visit(stmt->expression());
  Visit(stmt->statement());
}


void ALAA::VisitSwitchStatement(SwitchStatement* stmt) {
  Visit(stmt->tag());
  ZoneList<CaseClause*>* clauses = stmt->cases();
  for (int i = 0; i < clauses->length(); i++) {
    Visit(clauses->at(i));
  }
}


void ALAA::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  Visit(stmt->try_block());
  Visit(stmt->finally_block());
}


void ALAA::VisitClassLiteral(ClassLiteral* e) {
  VisitIfNotNull(e->extends());
  VisitIfNotNull(e->constructor());
  ZoneList<ObjectLiteralProperty*>* properties = e->properties();
  for (int i = 0; i < properties->length(); i++) {
    Visit(properties->at(i)->key());
    Visit(properties->at(i)->value());
  }
}


void ALAA::VisitConditional(Conditional* e) {
  Visit(e->condition());
  Visit(e->then_expression());
  Visit(e->else_expression());
}


void ALAA::VisitObjectLiteral(ObjectLiteral* e) {
  ZoneList<ObjectLiteralProperty*>* properties = e->properties();
  for (int i = 0; i < properties->length(); i++) {
    Visit(properties->at(i)->key());
    Visit(properties->at(i)->value());
  }
}


void ALAA::VisitArrayLiteral(ArrayLiteral* e) { VisitExpressions(e->values()); }


void ALAA::VisitYield(Yield* stmt) {
  Visit(stmt->generator_object());
  Visit(stmt->expression());
}


void ALAA::VisitThrow(Throw* stmt) { Visit(stmt->exception()); }


void ALAA::VisitProperty(Property* e) {
  Visit(e->obj());
  Visit(e->key());
}


void ALAA::VisitCall(Call* e) {
  Visit(e->expression());
  VisitExpressions(e->arguments());
}


void ALAA::VisitCallNew(CallNew* e) {
  Visit(e->expression());
  VisitExpressions(e->arguments());
}


void ALAA::VisitCallRuntime(CallRuntime* e) {
  VisitExpressions(e->arguments());
}


void ALAA::VisitUnaryOperation(UnaryOperation* e) { Visit(e->expression()); }


void ALAA::VisitBinaryOperation(BinaryOperation* e) {
  Visit(e->left());
  Visit(e->right());
}


void ALAA::VisitCompareOperation(CompareOperation* e) {
  Visit(e->left());
  Visit(e->right());
}


void ALAA::VisitSpread(Spread* e) { Visit(e->expression()); }


void ALAA::VisitEmptyParentheses(EmptyParentheses* e) { UNREACHABLE(); }


void ALAA::VisitCaseClause(CaseClause* cc) {
  if (!cc->is_default()) Visit(cc->label());
  VisitStatements(cc->statements());
}


void ALAA::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* stmt) {
  Visit(stmt->statement());
}


// ---------------------------------------------------------------------------
// -- Interesting nodes-------------------------------------------------------
// ---------------------------------------------------------------------------
void ALAA::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Visit(stmt->try_block());
  Visit(stmt->catch_block());
  // TODO(turbofan): are catch variables well-scoped?
  AnalyzeAssignment(stmt->variable());
}


void ALAA::VisitDoWhileStatement(DoWhileStatement* loop) {
  Enter(loop);
  Visit(loop->body());
  Visit(loop->cond());
  Exit(loop);
}


void ALAA::VisitWhileStatement(WhileStatement* loop) {
  Enter(loop);
  Visit(loop->cond());
  Visit(loop->body());
  Exit(loop);
}


void ALAA::VisitForStatement(ForStatement* loop) {
  VisitIfNotNull(loop->init());
  Enter(loop);
  VisitIfNotNull(loop->cond());
  Visit(loop->body());
  VisitIfNotNull(loop->next());
  Exit(loop);
}


void ALAA::VisitForInStatement(ForInStatement* loop) {
  Enter(loop);
  Visit(loop->each());
  Visit(loop->subject());
  Visit(loop->body());
  Exit(loop);
}


void ALAA::VisitForOfStatement(ForOfStatement* loop) {
  Visit(loop->assign_iterator());
  Enter(loop);
  Visit(loop->assign_each());
  Visit(loop->each());
  Visit(loop->subject());
  Visit(loop->body());
  Exit(loop);
}


void ALAA::VisitAssignment(Assignment* stmt) {
  Expression* l = stmt->target();
  Visit(l);
  Visit(stmt->value());
  if (l->IsVariableProxy()) AnalyzeAssignment(l->AsVariableProxy()->var());
}


void ALAA::VisitCountOperation(CountOperation* e) {
  Expression* l = e->expression();
  Visit(l);
  if (l->IsVariableProxy()) AnalyzeAssignment(l->AsVariableProxy()->var());
}


void ALAA::VisitRewritableAssignmentExpression(
    RewritableAssignmentExpression* expr) {
  Visit(expr->expression());
}


void ALAA::AnalyzeAssignment(Variable* var) {
  if (!loop_stack_.empty() && var->IsStackAllocated()) {
    loop_stack_.back()->Add(GetVariableIndex(info()->scope(), var));
  }
}


int ALAA::GetVariableIndex(Scope* scope, Variable* var) {
  CHECK(var->IsStackAllocated());
  if (var->is_this()) return 0;
  if (var->IsParameter()) return 1 + var->index();
  return 1 + scope->num_parameters() + var->index();
}


int LoopAssignmentAnalysis::GetAssignmentCountForTesting(Scope* scope,
                                                         Variable* var) {
  int count = 0;
  int var_index = AstLoopAssignmentAnalyzer::GetVariableIndex(scope, var);
  for (size_t i = 0; i < list_.size(); i++) {
    if (list_[i].second->Contains(var_index)) count++;
  }
  return count;
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8

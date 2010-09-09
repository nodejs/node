// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "data-flow.h"
#include "scopes.h"

namespace v8 {
namespace internal {


#ifdef DEBUG
void BitVector::Print() {
  bool first = true;
  PrintF("{");
  for (int i = 0; i < length(); i++) {
    if (Contains(i)) {
      if (!first) PrintF(",");
      first = false;
      PrintF("%d");
    }
  }
  PrintF("}");
}
#endif


bool AssignedVariablesAnalyzer::Analyze() {
  Scope* scope = fun_->scope();
  int variables = scope->num_parameters() + scope->num_stack_slots();
  if (variables == 0) return true;
  av_.ExpandTo(variables);
  VisitStatements(fun_->body());
  return !HasStackOverflow();
}


Variable* AssignedVariablesAnalyzer::FindSmiLoopVariable(ForStatement* stmt) {
  // The loop must have all necessary parts.
  if (stmt->init() == NULL || stmt->cond() == NULL || stmt->next() == NULL) {
    return NULL;
  }
  // The initialization statement has to be a simple assignment.
  Assignment* init = stmt->init()->StatementAsSimpleAssignment();
  if (init == NULL) return NULL;

  // We only deal with local variables.
  Variable* loop_var = init->target()->AsVariableProxy()->AsVariable();
  if (loop_var == NULL || !loop_var->IsStackAllocated()) return NULL;

  // Don't try to get clever with const or dynamic variables.
  if (loop_var->mode() != Variable::VAR) return NULL;

  // The initial value has to be a smi.
  Literal* init_lit = init->value()->AsLiteral();
  if (init_lit == NULL || !init_lit->handle()->IsSmi()) return NULL;
  int init_value = Smi::cast(*init_lit->handle())->value();

  // The condition must be a compare of variable with <, <=, >, or >=.
  CompareOperation* cond = stmt->cond()->AsCompareOperation();
  if (cond == NULL) return NULL;
  if (cond->op() != Token::LT
      && cond->op() != Token::LTE
      && cond->op() != Token::GT
      && cond->op() != Token::GTE) return NULL;

  // The lhs must be the same variable as in the init expression.
  if (cond->left()->AsVariableProxy()->AsVariable() != loop_var) return NULL;

  // The rhs must be a smi.
  Literal* term_lit = cond->right()->AsLiteral();
  if (term_lit == NULL || !term_lit->handle()->IsSmi()) return NULL;
  int term_value = Smi::cast(*term_lit->handle())->value();

  // The count operation updates the same variable as in the init expression.
  CountOperation* update = stmt->next()->StatementAsCountOperation();
  if (update == NULL) return NULL;
  if (update->expression()->AsVariableProxy()->AsVariable() != loop_var) {
    return NULL;
  }

  // The direction of the count operation must agree with the start and the end
  // value. We currently do not allow the initial value to be the same as the
  // terminal value. This _would_ be ok as long as the loop body never executes
  // or executes exactly one time.
  if (init_value == term_value) return NULL;
  if (init_value < term_value && update->op() != Token::INC) return NULL;
  if (init_value > term_value && update->op() != Token::DEC) return NULL;

  // Check that the update operation cannot overflow the smi range. This can
  // occur in the two cases where the loop bound is equal to the largest or
  // smallest smi.
  if (update->op() == Token::INC && term_value == Smi::kMaxValue) return NULL;
  if (update->op() == Token::DEC && term_value == Smi::kMinValue) return NULL;

  // Found a smi loop variable.
  return loop_var;
}

int AssignedVariablesAnalyzer::BitIndex(Variable* var) {
  ASSERT(var != NULL);
  ASSERT(var->IsStackAllocated());
  Slot* slot = var->slot();
  if (slot->type() == Slot::PARAMETER) {
    return slot->index();
  } else {
    return fun_->scope()->num_parameters() + slot->index();
  }
}


void AssignedVariablesAnalyzer::RecordAssignedVar(Variable* var) {
  ASSERT(var != NULL);
  if (var->IsStackAllocated()) {
    av_.Add(BitIndex(var));
  }
}


void AssignedVariablesAnalyzer::MarkIfTrivial(Expression* expr) {
  Variable* var = expr->AsVariableProxy()->AsVariable();
  if (var != NULL &&
      var->IsStackAllocated() &&
      !var->is_arguments() &&
      var->mode() != Variable::CONST &&
      (var->is_this() || !av_.Contains(BitIndex(var)))) {
    expr->AsVariableProxy()->MarkAsTrivial();
  }
}


void AssignedVariablesAnalyzer::ProcessExpression(Expression* expr) {
  BitVector saved_av(av_);
  av_.Clear();
  Visit(expr);
  av_.Union(saved_av);
}

void AssignedVariablesAnalyzer::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void AssignedVariablesAnalyzer::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  ProcessExpression(stmt->expression());
}


void AssignedVariablesAnalyzer::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
}


void AssignedVariablesAnalyzer::VisitIfStatement(IfStatement* stmt) {
  ProcessExpression(stmt->condition());
  Visit(stmt->then_statement());
  Visit(stmt->else_statement());
}


void AssignedVariablesAnalyzer::VisitContinueStatement(
    ContinueStatement* stmt) {
  // Nothing to do.
}


void AssignedVariablesAnalyzer::VisitBreakStatement(BreakStatement* stmt) {
  // Nothing to do.
}


void AssignedVariablesAnalyzer::VisitReturnStatement(ReturnStatement* stmt) {
  ProcessExpression(stmt->expression());
}


void AssignedVariablesAnalyzer::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  ProcessExpression(stmt->expression());
}


void AssignedVariablesAnalyzer::VisitWithExitStatement(
    WithExitStatement* stmt) {
  // Nothing to do.
}


void AssignedVariablesAnalyzer::VisitSwitchStatement(SwitchStatement* stmt) {
  BitVector result(av_);
  av_.Clear();
  Visit(stmt->tag());
  result.Union(av_);
  for (int i = 0; i < stmt->cases()->length(); i++) {
    CaseClause* clause = stmt->cases()->at(i);
    if (!clause->is_default()) {
      av_.Clear();
      Visit(clause->label());
      result.Union(av_);
    }
    VisitStatements(clause->statements());
  }
  av_.Union(result);
}


void AssignedVariablesAnalyzer::VisitDoWhileStatement(DoWhileStatement* stmt) {
  ProcessExpression(stmt->cond());
  Visit(stmt->body());
}


void AssignedVariablesAnalyzer::VisitWhileStatement(WhileStatement* stmt) {
  ProcessExpression(stmt->cond());
  Visit(stmt->body());
}


void AssignedVariablesAnalyzer::VisitForStatement(ForStatement* stmt) {
  if (stmt->init() != NULL) Visit(stmt->init());
  if (stmt->cond() != NULL) ProcessExpression(stmt->cond());
  if (stmt->next() != NULL) Visit(stmt->next());

  // Process loop body. After visiting the loop body av_ contains
  // the assigned variables of the loop body.
  BitVector saved_av(av_);
  av_.Clear();
  Visit(stmt->body());

  Variable* var = FindSmiLoopVariable(stmt);
  if (var != NULL && !av_.Contains(BitIndex(var))) {
    stmt->set_loop_variable(var);
  }
  av_.Union(saved_av);
}


void AssignedVariablesAnalyzer::VisitForInStatement(ForInStatement* stmt) {
  ProcessExpression(stmt->each());
  ProcessExpression(stmt->enumerable());
  Visit(stmt->body());
}


void AssignedVariablesAnalyzer::VisitTryCatchStatement(
    TryCatchStatement* stmt) {
  Visit(stmt->try_block());
  Visit(stmt->catch_block());
}


void AssignedVariablesAnalyzer::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  Visit(stmt->try_block());
  Visit(stmt->finally_block());
}


void AssignedVariablesAnalyzer::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  // Nothing to do.
}


void AssignedVariablesAnalyzer::VisitFunctionLiteral(FunctionLiteral* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitConditional(Conditional* expr) {
  ASSERT(av_.IsEmpty());

  Visit(expr->condition());

  BitVector result(av_);
  av_.Clear();
  Visit(expr->then_expression());
  result.Union(av_);

  av_.Clear();
  Visit(expr->else_expression());
  av_.Union(result);
}


void AssignedVariablesAnalyzer::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void AssignedVariablesAnalyzer::VisitVariableProxy(VariableProxy* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitLiteral(Literal* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitRegExpLiteral(RegExpLiteral* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitObjectLiteral(ObjectLiteral* expr) {
  ASSERT(av_.IsEmpty());
  BitVector result(av_.length());
  for (int i = 0; i < expr->properties()->length(); i++) {
    Visit(expr->properties()->at(i)->value());
    result.Union(av_);
    av_.Clear();
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitArrayLiteral(ArrayLiteral* expr) {
  ASSERT(av_.IsEmpty());
  BitVector result(av_.length());
  for (int i = 0; i < expr->values()->length(); i++) {
    Visit(expr->values()->at(i));
    result.Union(av_);
    av_.Clear();
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->key());
  ProcessExpression(expr->value());
}


void AssignedVariablesAnalyzer::VisitAssignment(Assignment* expr) {
  ASSERT(av_.IsEmpty());

  // There are three kinds of assignments: variable assignments, property
  // assignments, and reference errors (invalid left-hand sides).
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();
  ASSERT(var == NULL || prop == NULL);

  if (var != NULL) {
    MarkIfTrivial(expr->value());
    Visit(expr->value());
    if (expr->is_compound()) {
      // Left-hand side occurs also as an rvalue.
      MarkIfTrivial(expr->target());
      ProcessExpression(expr->target());
    }
    RecordAssignedVar(var);

  } else if (prop != NULL) {
    MarkIfTrivial(expr->value());
    Visit(expr->value());
    if (!prop->key()->IsPropertyName()) {
      MarkIfTrivial(prop->key());
      ProcessExpression(prop->key());
    }
    MarkIfTrivial(prop->obj());
    ProcessExpression(prop->obj());

  } else {
    Visit(expr->target());
  }
}


void AssignedVariablesAnalyzer::VisitThrow(Throw* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->exception());
}


void AssignedVariablesAnalyzer::VisitProperty(Property* expr) {
  ASSERT(av_.IsEmpty());
  if (!expr->key()->IsPropertyName()) {
    MarkIfTrivial(expr->key());
    Visit(expr->key());
  }
  MarkIfTrivial(expr->obj());
  ProcessExpression(expr->obj());
}


void AssignedVariablesAnalyzer::VisitCall(Call* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->expression());
  BitVector result(av_);
  for (int i = 0; i < expr->arguments()->length(); i++) {
    av_.Clear();
    Visit(expr->arguments()->at(i));
    result.Union(av_);
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitCallNew(CallNew* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->expression());
  BitVector result(av_);
  for (int i = 0; i < expr->arguments()->length(); i++) {
    av_.Clear();
    Visit(expr->arguments()->at(i));
    result.Union(av_);
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitCallRuntime(CallRuntime* expr) {
  ASSERT(av_.IsEmpty());
  BitVector result(av_);
  for (int i = 0; i < expr->arguments()->length(); i++) {
    av_.Clear();
    Visit(expr->arguments()->at(i));
    result.Union(av_);
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitUnaryOperation(UnaryOperation* expr) {
  ASSERT(av_.IsEmpty());
  MarkIfTrivial(expr->expression());
  Visit(expr->expression());
}


void AssignedVariablesAnalyzer::VisitIncrementOperation(
    IncrementOperation* expr) {
  UNREACHABLE();
}


void AssignedVariablesAnalyzer::VisitCountOperation(CountOperation* expr) {
  ASSERT(av_.IsEmpty());
  if (expr->is_prefix()) MarkIfTrivial(expr->expression());
  Visit(expr->expression());

  Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
  if (var != NULL) RecordAssignedVar(var);
}


void AssignedVariablesAnalyzer::VisitBinaryOperation(BinaryOperation* expr) {
  ASSERT(av_.IsEmpty());
  MarkIfTrivial(expr->right());
  Visit(expr->right());
  MarkIfTrivial(expr->left());
  ProcessExpression(expr->left());
}


void AssignedVariablesAnalyzer::VisitCompareOperation(CompareOperation* expr) {
  ASSERT(av_.IsEmpty());
  MarkIfTrivial(expr->right());
  Visit(expr->right());
  MarkIfTrivial(expr->left());
  ProcessExpression(expr->left());
}


void AssignedVariablesAnalyzer::VisitCompareToNull(CompareToNull* expr) {
  ASSERT(av_.IsEmpty());
  MarkIfTrivial(expr->expression());
  Visit(expr->expression());
}


void AssignedVariablesAnalyzer::VisitThisFunction(ThisFunction* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


} }  // namespace v8::internal

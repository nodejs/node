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
#include "flow-graph.h"
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


void AstLabeler::Label(CompilationInfo* info) {
  info_ = info;
  VisitStatements(info_->function()->body());
}


void AstLabeler::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0, len = stmts->length(); i < len; i++) {
    Visit(stmts->at(i));
  }
}


void AstLabeler::VisitDeclarations(ZoneList<Declaration*>* decls) {
  UNREACHABLE();
}


void AstLabeler::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void AstLabeler::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void AstLabeler::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
}


void AstLabeler::VisitIfStatement(IfStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitReturnStatement(ReturnStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitWhileStatement(WhileStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitFunctionLiteral(FunctionLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitConditional(Conditional* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitVariableProxy(VariableProxy* expr) {
  expr->set_num(next_number_++);
  Variable* var = expr->var();
  if (var->is_global() && !var->is_this()) {
    info_->set_has_globals(true);
  }
}


void AstLabeler::VisitLiteral(Literal* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitObjectLiteral(ObjectLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitArrayLiteral(ArrayLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitAssignment(Assignment* expr) {
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->IsPropertyName());
  VariableProxy* proxy = prop->obj()->AsVariableProxy();
  USE(proxy);
  ASSERT(proxy != NULL && proxy->var()->is_this());
  info()->set_has_this_properties(true);

  prop->obj()->set_num(AstNode::kNoNumber);
  prop->key()->set_num(AstNode::kNoNumber);
  Visit(expr->value());
  expr->set_num(next_number_++);
}


void AstLabeler::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitProperty(Property* expr) {
  ASSERT(expr->key()->IsPropertyName());
  VariableProxy* proxy = expr->obj()->AsVariableProxy();
  USE(proxy);
  ASSERT(proxy != NULL && proxy->var()->is_this());
  info()->set_has_this_properties(true);

  expr->obj()->set_num(AstNode::kNoNumber);
  expr->key()->set_num(AstNode::kNoNumber);
  expr->set_num(next_number_++);
}


void AstLabeler::VisitCall(Call* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitCallNew(CallNew* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitCallRuntime(CallRuntime* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitUnaryOperation(UnaryOperation* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitCountOperation(CountOperation* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitBinaryOperation(BinaryOperation* expr) {
  Visit(expr->left());
  Visit(expr->right());
  expr->set_num(next_number_++);
}


void AstLabeler::VisitCompareOperation(CompareOperation* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


AssignedVariablesAnalyzer::AssignedVariablesAnalyzer(FunctionLiteral* fun)
    : fun_(fun),
      av_(fun->scope()->num_parameters() + fun->scope()->num_stack_slots()) {}


void AssignedVariablesAnalyzer::Analyze() {
  ASSERT(av_.length() > 0);
  VisitStatements(fun_->body());
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
    expr->AsVariableProxy()->set_is_trivial(true);
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

  if (expr->target()->AsProperty() != NULL) {
    // Visit receiver and key of property store and rhs.
    Visit(expr->target()->AsProperty()->obj());
    ProcessExpression(expr->target()->AsProperty()->key());
    ProcessExpression(expr->value());

    // If we have a variable as a receiver in a property store, check if
    // we can mark it as trivial.
    MarkIfTrivial(expr->target()->AsProperty()->obj());
  } else {
    Visit(expr->target());
    ProcessExpression(expr->value());

    Variable* var = expr->target()->AsVariableProxy()->AsVariable();
    if (var != NULL) RecordAssignedVar(var);
  }
}


void AssignedVariablesAnalyzer::VisitThrow(Throw* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->exception());
}


void AssignedVariablesAnalyzer::VisitProperty(Property* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->obj());
  ProcessExpression(expr->key());

  // In case we have a variable as a receiver, check if we can mark
  // it as trivial.
  MarkIfTrivial(expr->obj());
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
  Visit(expr->expression());
}


void AssignedVariablesAnalyzer::VisitCountOperation(CountOperation* expr) {
  ASSERT(av_.IsEmpty());

  Visit(expr->expression());

  Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
  if (var != NULL) RecordAssignedVar(var);
}


void AssignedVariablesAnalyzer::VisitBinaryOperation(BinaryOperation* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->left());

  ProcessExpression(expr->right());

  // In case we have a variable on the left side, check if we can mark
  // it as trivial.
  MarkIfTrivial(expr->left());
}


void AssignedVariablesAnalyzer::VisitCompareOperation(CompareOperation* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->left());

  ProcessExpression(expr->right());

  // In case we have a variable on the left side, check if we can mark
  // it as trivial.
  MarkIfTrivial(expr->left());
}


void AssignedVariablesAnalyzer::VisitThisFunction(ThisFunction* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


int ReachingDefinitions::IndexFor(Variable* var, int variable_count) {
  // Parameters are numbered left-to-right from the beginning of the bit
  // set.  Stack-allocated locals are allocated right-to-left from the end.
  ASSERT(var != NULL && var->IsStackAllocated());
  Slot* slot = var->slot();
  if (slot->type() == Slot::PARAMETER) {
    return slot->index();
  } else {
    return (variable_count - 1) - slot->index();
  }
}


void Node::InitializeReachingDefinitions(int definition_count,
                                         List<BitVector*>* variables,
                                         WorkList<Node>* worklist,
                                         bool mark) {
  ASSERT(!IsMarkedWith(mark));
  rd_.Initialize(definition_count);
  MarkWith(mark);
  worklist->Insert(this);
}


void BlockNode::InitializeReachingDefinitions(int definition_count,
                                              List<BitVector*>* variables,
                                              WorkList<Node>* worklist,
                                              bool mark) {
  ASSERT(!IsMarkedWith(mark));
  int instruction_count = instructions_.length();
  int variable_count = variables->length();

  rd_.Initialize(definition_count);
  // The RD_in set for the entry node has a definition for each parameter
  // and local.
  if (predecessor_ == NULL) {
    for (int i = 0; i < variable_count; i++) rd_.rd_in()->Add(i);
  }

  for (int i = 0; i < instruction_count; i++) {
    Expression* expr = instructions_[i]->AsExpression();
    if (expr == NULL) continue;
    Variable* var = expr->AssignedVariable();
    if (var == NULL || !var->IsStackAllocated()) continue;

    // All definitions of this variable are killed.
    BitVector* def_set =
        variables->at(ReachingDefinitions::IndexFor(var, variable_count));
    rd_.kill()->Union(*def_set);

    // All previously generated definitions are not generated.
    rd_.gen()->Subtract(*def_set);

    // This one is generated.
    rd_.gen()->Add(expr->num());
  }

  // Add all blocks except the entry node to the worklist.
  if (predecessor_ != NULL) {
    MarkWith(mark);
    worklist->Insert(this);
  }
}


void ExitNode::ComputeRDOut(BitVector* result) {
  // Should not be the predecessor of any node.
  UNREACHABLE();
}


void BlockNode::ComputeRDOut(BitVector* result) {
  // All definitions reaching this block ...
  *result = *rd_.rd_in();
  // ... except those killed by the block ...
  result->Subtract(*rd_.kill());
  // ... but including those generated by the block.
  result->Union(*rd_.gen());
}


void BranchNode::ComputeRDOut(BitVector* result) {
  // Branch nodes don't kill or generate definitions.
  *result = *rd_.rd_in();
}


void JoinNode::ComputeRDOut(BitVector* result) {
  // Join nodes don't kill or generate definitions.
  *result = *rd_.rd_in();
}


void ExitNode::UpdateRDIn(WorkList<Node>* worklist, bool mark) {
  // The exit node has no successors so we can just update in place.  New
  // RD_in is the union over all predecessors.
  int definition_count = rd_.rd_in()->length();
  rd_.rd_in()->Clear();

  BitVector temp(definition_count);
  for (int i = 0, len = predecessors_.length(); i < len; i++) {
    // Because ComputeRDOut always overwrites temp and its value is
    // always read out before calling ComputeRDOut again, we do not
    // have to clear it on each iteration of the loop.
    predecessors_[i]->ComputeRDOut(&temp);
    rd_.rd_in()->Union(temp);
  }
}


void BlockNode::UpdateRDIn(WorkList<Node>* worklist, bool mark) {
  // The entry block has no predecessor.  Its RD_in does not change.
  if (predecessor_ == NULL) return;

  BitVector new_rd_in(rd_.rd_in()->length());
  predecessor_->ComputeRDOut(&new_rd_in);

  if (rd_.rd_in()->Equals(new_rd_in)) return;

  // Update RD_in.
  *rd_.rd_in() = new_rd_in;
  // Add the successor to the worklist if not already present.
  if (!successor_->IsMarkedWith(mark)) {
    successor_->MarkWith(mark);
    worklist->Insert(successor_);
  }
}


void BranchNode::UpdateRDIn(WorkList<Node>* worklist, bool mark) {
  BitVector new_rd_in(rd_.rd_in()->length());
  predecessor_->ComputeRDOut(&new_rd_in);

  if (rd_.rd_in()->Equals(new_rd_in)) return;

  // Update RD_in.
  *rd_.rd_in() = new_rd_in;
  // Add the successors to the worklist if not already present.
  if (!successor0_->IsMarkedWith(mark)) {
    successor0_->MarkWith(mark);
    worklist->Insert(successor0_);
  }
  if (!successor1_->IsMarkedWith(mark)) {
    successor1_->MarkWith(mark);
    worklist->Insert(successor1_);
  }
}


void JoinNode::UpdateRDIn(WorkList<Node>* worklist, bool mark) {
  int definition_count = rd_.rd_in()->length();
  BitVector new_rd_in(definition_count);

  // New RD_in is the union over all predecessors.
  BitVector temp(definition_count);
  for (int i = 0, len = predecessors_.length(); i < len; i++) {
    predecessors_[i]->ComputeRDOut(&temp);
    new_rd_in.Union(temp);
  }

  if (rd_.rd_in()->Equals(new_rd_in)) return;

  // Update RD_in.
  *rd_.rd_in() = new_rd_in;
  // Add the successor to the worklist if not already present.
  if (!successor_->IsMarkedWith(mark)) {
    successor_->MarkWith(mark);
    worklist->Insert(successor_);
  }
}


void Node::PropagateReachingDefinitions(List<BitVector*>* variables) {
  // Nothing to do.
}


void BlockNode::PropagateReachingDefinitions(List<BitVector*>* variables) {
  // Propagate RD_in from the start of the block to all the variable
  // references.
  int variable_count = variables->length();
  BitVector rd = *rd_.rd_in();
  for (int i = 0, len = instructions_.length(); i < len; i++) {
    Expression* expr = instructions_[i]->AsExpression();
    if (expr == NULL) continue;

    // Look for a variable reference to record its reaching definitions.
    VariableProxy* proxy = expr->AsVariableProxy();
    if (proxy == NULL) {
      // Not a VariableProxy?  Maybe it's a count operation.
      CountOperation* count_operation = expr->AsCountOperation();
      if (count_operation != NULL) {
        proxy = count_operation->expression()->AsVariableProxy();
      }
    }
    if (proxy == NULL) {
      // OK, Maybe it's a compound assignment.
      Assignment* assignment = expr->AsAssignment();
      if (assignment != NULL && assignment->is_compound()) {
        proxy = assignment->target()->AsVariableProxy();
      }
    }

    if (proxy != NULL &&
        proxy->var()->IsStackAllocated() &&
        !proxy->var()->is_this()) {
      // All definitions for this variable.
      BitVector* definitions =
          variables->at(ReachingDefinitions::IndexFor(proxy->var(),
                                                      variable_count));
      BitVector* reaching_definitions = new BitVector(*definitions);
      // Intersected with all definitions (of any variable) reaching this
      // instruction.
      reaching_definitions->Intersect(rd);
      proxy->set_reaching_definitions(reaching_definitions);
    }

    // It may instead (or also) be a definition.  If so update the running
    // value of reaching definitions for the block.
    Variable* var = expr->AssignedVariable();
    if (var == NULL || !var->IsStackAllocated()) continue;

    // All definitions of this variable are killed.
    BitVector* def_set =
        variables->at(ReachingDefinitions::IndexFor(var, variable_count));
    rd.Subtract(*def_set);
    // This definition is generated.
    rd.Add(expr->num());
  }
}


void ReachingDefinitions::Compute() {
  // The definitions in the body plus an implicit definition for each
  // variable at function entry.
  int definition_count = body_definitions_->length() + variable_count_;
  int node_count = postorder_->length();

  // Step 1: For each stack-allocated variable, identify the set of all its
  // definitions.
  List<BitVector*> variables;
  for (int i = 0; i < variable_count_; i++) {
    // Add the initial definition for each variable.
    BitVector* initial = new BitVector(definition_count);
    initial->Add(i);
    variables.Add(initial);
  }
  for (int i = 0, len = body_definitions_->length(); i < len; i++) {
    // Account for each definition in the body as a definition of the
    // defined variable.
    Variable* var = body_definitions_->at(i)->AssignedVariable();
    variables[IndexFor(var, variable_count_)]->Add(i + variable_count_);
  }

  // Step 2: Compute KILL and GEN for each block node, initialize RD_in for
  // all nodes, and mark and add all nodes to the worklist in reverse
  // postorder.  All nodes should currently have the same mark.
  bool mark = postorder_->at(0)->IsMarkedWith(false);  // Negation of current.
  WorkList<Node> worklist(node_count);
  for (int i = node_count - 1; i >= 0; i--) {
    postorder_->at(i)->InitializeReachingDefinitions(definition_count,
                                                     &variables,
                                                     &worklist,
                                                     mark);
  }

  // Step 3: Until the worklist is empty, remove an item compute and update
  // its rd_in based on its predecessor's rd_out.  If rd_in has changed, add
  // all necessary successors to the worklist.
  while (!worklist.is_empty()) {
    Node* node = worklist.Remove();
    node->MarkWith(!mark);
    node->UpdateRDIn(&worklist, mark);
  }

  // Step 4: Based on RD_in for block nodes, propagate reaching definitions
  // to all variable uses in the block.
  for (int i = 0; i < node_count; i++) {
    postorder_->at(i)->PropagateReachingDefinitions(&variables);
  }
}


bool TypeAnalyzer::IsPrimitiveDef(int def_num) {
  if (def_num < param_count_) return false;
  if (def_num < variable_count_) return true;
  return body_definitions_->at(def_num - variable_count_)->IsPrimitive();
}


void TypeAnalyzer::Compute() {
  bool changed;
  int count = 0;

  do {
    changed = false;

    if (FLAG_print_graph_text) {
      PrintF("TypeAnalyzer::Compute - iteration %d\n", count++);
    }

    for (int i = postorder_->length() - 1; i >= 0; --i) {
      Node* node = postorder_->at(i);
      if (node->IsBlockNode()) {
        BlockNode* block = BlockNode::cast(node);
        for (int j = 0; j < block->instructions()->length(); j++) {
          Expression* expr = block->instructions()->at(j)->AsExpression();
          if (expr != NULL) {
            // For variable uses: Compute new type from reaching definitions.
            VariableProxy* proxy = expr->AsVariableProxy();
            if (proxy != NULL && proxy->reaching_definitions() != NULL) {
              BitVector* rd = proxy->reaching_definitions();
              bool prim_type = true;
              // TODO(fsc): A sparse set representation of reaching
              // definitions would speed up iterating here.
              for (int k = 0; k < rd->length(); k++) {
                if (rd->Contains(k) && !IsPrimitiveDef(k)) {
                  prim_type = false;
                  break;
                }
              }
              // Reset changed flag if new type information was computed.
              if (prim_type != proxy->IsPrimitive()) {
                changed = true;
                proxy->SetIsPrimitive(prim_type);
              }
            }
          }
        }
      }
    }
  } while (changed);
}


void Node::MarkCriticalInstructions(
    List<AstNode*>* stack,
    ZoneList<Expression*>* body_definitions,
    int variable_count) {
}


void BlockNode::MarkCriticalInstructions(
    List<AstNode*>* stack,
    ZoneList<Expression*>* body_definitions,
    int variable_count) {
  for (int i = instructions_.length() - 1; i >= 0; i--) {
    // Only expressions can appear in the flow graph for now.
    Expression* expr = instructions_[i]->AsExpression();
    if (expr != NULL && !expr->is_live() &&
        (expr->is_loop_condition() || expr->IsCritical())) {
      expr->mark_as_live();
      expr->ProcessNonLiveChildren(stack, body_definitions, variable_count);
    }
  }
}


void MarkLiveCode(ZoneList<Node*>* nodes,
                  ZoneList<Expression*>* body_definitions,
                  int variable_count) {
  List<AstNode*> stack(20);

  // Mark the critical AST nodes as live; mark their dependencies and
  // add them to the marking stack.
  for (int i = nodes->length() - 1; i >= 0; i--) {
    nodes->at(i)->MarkCriticalInstructions(&stack, body_definitions,
                                           variable_count);
  }

  // Continue marking dependencies until no more.
  while (!stack.is_empty()) {
  // Only expressions can appear in the flow graph for now.
    Expression* expr = stack.RemoveLast()->AsExpression();
    if (expr != NULL) {
      expr->ProcessNonLiveChildren(&stack, body_definitions, variable_count);
    }
  }
}


#ifdef DEBUG

// Print a textual representation of an instruction in a flow graph.  Using
// the AstVisitor is overkill because there is no recursion here.  It is
// only used for printing in debug mode.
class TextInstructionPrinter: public AstVisitor {
 public:
  TextInstructionPrinter() : number_(0) {}

  int NextNumber() { return number_; }
  void AssignNumber(AstNode* node) { node->set_num(number_++); }

 private:
  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  int number_;

  DISALLOW_COPY_AND_ASSIGN(TextInstructionPrinter);
};


void TextInstructionPrinter::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitBlock(Block* stmt) {
  PrintF("Block");
}


void TextInstructionPrinter::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  PrintF("ExpressionStatement");
}


void TextInstructionPrinter::VisitEmptyStatement(EmptyStatement* stmt) {
  PrintF("EmptyStatement");
}


void TextInstructionPrinter::VisitIfStatement(IfStatement* stmt) {
  PrintF("IfStatement");
}


void TextInstructionPrinter::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitReturnStatement(ReturnStatement* stmt) {
  PrintF("return @%d", stmt->expression()->num());
}


void TextInstructionPrinter::VisitWithEnterStatement(WithEnterStatement* stmt) {
  PrintF("WithEnterStatement");
}


void TextInstructionPrinter::VisitWithExitStatement(WithExitStatement* stmt) {
  PrintF("WithExitStatement");
}


void TextInstructionPrinter::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitDoWhileStatement(DoWhileStatement* stmt) {
  PrintF("DoWhileStatement");
}


void TextInstructionPrinter::VisitWhileStatement(WhileStatement* stmt) {
  PrintF("WhileStatement");
}


void TextInstructionPrinter::VisitForStatement(ForStatement* stmt) {
  PrintF("ForStatement");
}


void TextInstructionPrinter::VisitForInStatement(ForInStatement* stmt) {
  PrintF("ForInStatement");
}


void TextInstructionPrinter::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitDebuggerStatement(DebuggerStatement* stmt) {
  PrintF("DebuggerStatement");
}


void TextInstructionPrinter::VisitFunctionLiteral(FunctionLiteral* expr) {
  PrintF("FunctionLiteral");
}


void TextInstructionPrinter::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  PrintF("SharedFunctionInfoLiteral");
}


void TextInstructionPrinter::VisitConditional(Conditional* expr) {
  PrintF("Conditional");
}


void TextInstructionPrinter::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitVariableProxy(VariableProxy* expr) {
  Variable* var = expr->AsVariable();
  if (var != NULL) {
    PrintF("%s", *var->name()->ToCString());
    if (var->IsStackAllocated() && expr->reaching_definitions() != NULL) {
      expr->reaching_definitions()->Print();
    }
  } else {
    ASSERT(expr->AsProperty() != NULL);
    VisitProperty(expr->AsProperty());
  }
}


void TextInstructionPrinter::VisitLiteral(Literal* expr) {
  expr->handle()->ShortPrint();
}


void TextInstructionPrinter::VisitRegExpLiteral(RegExpLiteral* expr) {
  PrintF("RegExpLiteral");
}


void TextInstructionPrinter::VisitObjectLiteral(ObjectLiteral* expr) {
  PrintF("ObjectLiteral");
}


void TextInstructionPrinter::VisitArrayLiteral(ArrayLiteral* expr) {
  PrintF("ArrayLiteral");
}


void TextInstructionPrinter::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  PrintF("CatchExtensionObject");
}


void TextInstructionPrinter::VisitAssignment(Assignment* expr) {
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();

  if (var == NULL && prop == NULL) {
    // Throw reference error.
    Visit(expr->target());
    return;
  }

  // Print the left-hand side.
  if (var != NULL) {
    PrintF("%s", *var->name()->ToCString());
  } else if (prop != NULL) {
    PrintF("@%d", prop->obj()->num());
    if (prop->key()->IsPropertyName()) {
      PrintF(".");
      ASSERT(prop->key()->AsLiteral() != NULL);
      prop->key()->AsLiteral()->handle()->Print();
    } else {
      PrintF("[@%d]", prop->key()->num());
    }
  }

  // Print the operation.
  if (expr->is_compound()) {
    PrintF(" = ");
    // Print the left-hand side again when compound.
    if (var != NULL) {
      PrintF("@%d", expr->target()->num());
    } else {
      PrintF("@%d", prop->obj()->num());
      if (prop->key()->IsPropertyName()) {
        PrintF(".");
        ASSERT(prop->key()->AsLiteral() != NULL);
        prop->key()->AsLiteral()->handle()->Print();
      } else {
        PrintF("[@%d]", prop->key()->num());
      }
    }
    // Print the corresponding binary operator.
    PrintF(" %s ", Token::String(expr->binary_op()));
  } else {
    PrintF(" %s ", Token::String(expr->op()));
  }

  // Print the right-hand side.
  PrintF("@%d", expr->value()->num());

  if (expr->num() != AstNode::kNoNumber) {
    PrintF(" ;; D%d", expr->num());
  }
}


void TextInstructionPrinter::VisitThrow(Throw* expr) {
  PrintF("throw @%d", expr->exception()->num());
}


void TextInstructionPrinter::VisitProperty(Property* expr) {
  if (expr->key()->IsPropertyName()) {
    PrintF("@%d.", expr->obj()->num());
    ASSERT(expr->key()->AsLiteral() != NULL);
    expr->key()->AsLiteral()->handle()->Print();
  } else {
    PrintF("@%d[@%d]", expr->obj()->num(), expr->key()->num());
  }
}


void TextInstructionPrinter::VisitCall(Call* expr) {
  PrintF("@%d(", expr->expression()->num());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    if (i != 0) PrintF(", ");
    PrintF("@%d", arguments->at(i)->num());
  }
  PrintF(")");
}


void TextInstructionPrinter::VisitCallNew(CallNew* expr) {
  PrintF("new @%d(", expr->expression()->num());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    if (i != 0) PrintF(", ");
    PrintF("@%d", arguments->at(i)->num());
  }
  PrintF(")");
}


void TextInstructionPrinter::VisitCallRuntime(CallRuntime* expr) {
  PrintF("%s(", *expr->name()->ToCString());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    if (i != 0) PrintF(", ");
    PrintF("@%d", arguments->at(i)->num());
  }
  PrintF(")");
}


void TextInstructionPrinter::VisitUnaryOperation(UnaryOperation* expr) {
  PrintF("%s(@%d)", Token::String(expr->op()), expr->expression()->num());
}


void TextInstructionPrinter::VisitCountOperation(CountOperation* expr) {
  if (expr->is_prefix()) {
    PrintF("%s@%d", Token::String(expr->op()), expr->expression()->num());
  } else {
    PrintF("@%d%s", expr->expression()->num(), Token::String(expr->op()));
  }

  if (expr->num() != AstNode::kNoNumber) {
    PrintF(" ;; D%d", expr->num());
  }
}


void TextInstructionPrinter::VisitBinaryOperation(BinaryOperation* expr) {
  ASSERT(expr->op() != Token::COMMA);
  ASSERT(expr->op() != Token::OR);
  ASSERT(expr->op() != Token::AND);
  PrintF("@%d %s @%d",
         expr->left()->num(),
         Token::String(expr->op()),
         expr->right()->num());
}


void TextInstructionPrinter::VisitCompareOperation(CompareOperation* expr) {
  PrintF("@%d %s @%d",
         expr->left()->num(),
         Token::String(expr->op()),
         expr->right()->num());
}


void TextInstructionPrinter::VisitThisFunction(ThisFunction* expr) {
  PrintF("ThisFunction");
}


static int node_count = 0;
static int instruction_count = 0;


void Node::AssignNodeNumber() {
  set_number(node_count++);
}


void Node::PrintReachingDefinitions() {
  if (rd_.rd_in() != NULL) {
    ASSERT(rd_.kill() != NULL && rd_.gen() != NULL);

    PrintF("RD_in = ");
    rd_.rd_in()->Print();
    PrintF("\n");

    PrintF("RD_kill = ");
    rd_.kill()->Print();
    PrintF("\n");

    PrintF("RD_gen = ");
    rd_.gen()->Print();
    PrintF("\n");
  }
}


void ExitNode::PrintText() {
  PrintReachingDefinitions();
  PrintF("L%d: Exit\n\n", number());
}


void BlockNode::PrintText() {
  PrintReachingDefinitions();
  // Print the instructions in the block.
  PrintF("L%d: Block\n", number());
  TextInstructionPrinter printer;
  for (int i = 0, len = instructions_.length(); i < len; i++) {
    AstNode* instr = instructions_[i];
    // Print a star next to dead instructions.
    if (instr->AsExpression() != NULL && instr->AsExpression()->is_live()) {
      PrintF("  ");
    } else {
      PrintF("* ");
    }
    PrintF("%d ", printer.NextNumber());
    printer.Visit(instr);
    printer.AssignNumber(instr);
    PrintF("\n");
  }
  PrintF("goto L%d\n\n", successor_->number());
}


void BranchNode::PrintText() {
  PrintReachingDefinitions();
  PrintF("L%d: Branch\n", number());
  PrintF("goto (L%d, L%d)\n\n", successor0_->number(), successor1_->number());
}


void JoinNode::PrintText() {
  PrintReachingDefinitions();
  PrintF("L%d: Join(", number());
  for (int i = 0, len = predecessors_.length(); i < len; i++) {
    if (i != 0) PrintF(", ");
    PrintF("L%d", predecessors_[i]->number());
  }
  PrintF(")\ngoto L%d\n\n", successor_->number());
}


void FlowGraph::PrintText(FunctionLiteral* fun, ZoneList<Node*>* postorder) {
  PrintF("\n========\n");
  PrintF("name = %s\n", *fun->name()->ToCString());

  // Number nodes and instructions in reverse postorder.
  node_count = 0;
  instruction_count = 0;
  for (int i = postorder->length() - 1; i >= 0; i--) {
    postorder->at(i)->AssignNodeNumber();
  }

  // Print basic blocks in reverse postorder.
  for (int i = postorder->length() - 1; i >= 0; i--) {
    postorder->at(i)->PrintText();
  }
}

#endif  // DEBUG


} }  // namespace v8::internal

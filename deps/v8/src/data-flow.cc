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

namespace v8 {
namespace internal {


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


void AstLabeler::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
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


ZoneList<Expression*>* VarUseMap::Lookup(Variable* var) {
  HashMap::Entry* entry = HashMap::Lookup(var, var->name()->Hash(), true);
  if (entry->value == NULL) {
    entry->value = new ZoneList<Expression*>(1);
  }
  return reinterpret_cast<ZoneList<Expression*>*>(entry->value);
}


void LivenessAnalyzer::Analyze(FunctionLiteral* fun) {
  // Process the function body.
  VisitStatements(fun->body());

  // All variables are implicitly defined at the function start.
  // Record a definition of all variables live at function entry.
  for (HashMap::Entry* p = live_vars_.Start();
       p != NULL;
       p = live_vars_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->key);
    RecordDef(var, fun);
  }
}


void LivenessAnalyzer::VisitStatements(ZoneList<Statement*>* stmts) {
  // Visit statements right-to-left.
  for (int i = stmts->length() - 1; i >= 0; i--) {
    Visit(stmts->at(i));
  }
}


void LivenessAnalyzer::RecordUse(Variable* var, Expression* expr) {
  ASSERT(var->is_global() || var->is_this());
  ZoneList<Expression*>* uses = live_vars_.Lookup(var);
  uses->Add(expr);
}


void LivenessAnalyzer::RecordDef(Variable* var, Expression* expr) {
  ASSERT(var->is_global() || var->is_this());

  // We do not support other expressions that can define variables.
  ASSERT(expr->AsFunctionLiteral() != NULL);

  // Add the variable to the list of defined variables.
  if (expr->defined_vars() == NULL) {
    expr->set_defined_vars(new ZoneList<DefinitionInfo*>(1));
  }
  DefinitionInfo* def = new DefinitionInfo();
  expr->AsFunctionLiteral()->defined_vars()->Add(def);

  // Compute the last use of the definition. The variable uses are
  // inserted in reversed evaluation order. The first element
  // in the list of live uses is the last use.
  ZoneList<Expression*>* uses = live_vars_.Lookup(var);
  while (uses->length() > 0) {
    Expression* use_site = uses->RemoveLast();
    use_site->set_var_def(def);
    if (uses->length() == 0) {
      def->set_last_use(use_site);
    }
  }
}


// Visitor functions for live variable analysis.
void LivenessAnalyzer::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void LivenessAnalyzer::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void LivenessAnalyzer::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
}


void LivenessAnalyzer::VisitIfStatement(IfStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitReturnStatement(ReturnStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitWhileStatement(WhileStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitFunctionLiteral(FunctionLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitConditional(Conditional* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitVariableProxy(VariableProxy* expr) {
  Variable* var = expr->var();
  ASSERT(var->is_global());
  ASSERT(!var->is_this());
  RecordUse(var, expr);
}


void LivenessAnalyzer::VisitLiteral(Literal* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitObjectLiteral(ObjectLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitArrayLiteral(ArrayLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitAssignment(Assignment* expr) {
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->IsPropertyName());
  VariableProxy* proxy = prop->obj()->AsVariableProxy();
  ASSERT(proxy != NULL && proxy->var()->is_this());

  // Record use of this at the assignment node. Assignments to
  // this-properties are treated like unary operations.
  RecordUse(proxy->var(), expr);

  // Visit right-hand side.
  Visit(expr->value());
}


void LivenessAnalyzer::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitProperty(Property* expr) {
  ASSERT(expr->key()->IsPropertyName());
  VariableProxy* proxy = expr->obj()->AsVariableProxy();
  ASSERT(proxy != NULL && proxy->var()->is_this());
  RecordUse(proxy->var(), expr);
}


void LivenessAnalyzer::VisitCall(Call* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitCallNew(CallNew* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitCallRuntime(CallRuntime* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitUnaryOperation(UnaryOperation* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitCountOperation(CountOperation* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitBinaryOperation(BinaryOperation* expr) {
  // Visit child nodes in reverse evaluation order.
  Visit(expr->right());
  Visit(expr->left());
}


void LivenessAnalyzer::VisitCompareOperation(CompareOperation* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}


} }  // namespace v8::internal

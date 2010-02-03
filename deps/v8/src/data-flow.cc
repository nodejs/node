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


void AstLabeler::Label(FunctionLiteral* fun) {
  VisitStatements(fun->body());
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
  if (prop != NULL) {
    ASSERT(prop->key()->IsPropertyName());
    VariableProxy* proxy = prop->obj()->AsVariableProxy();
    if (proxy != NULL && proxy->var()->is_this()) {
      has_this_properties_ = true;
    } else {
      Visit(prop->obj());
    }
  }
  Visit(expr->value());
  expr->set_num(next_number_++);
}


void AstLabeler::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitProperty(Property* expr) {
  UNREACHABLE();
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

} }  // namespace v8::internal

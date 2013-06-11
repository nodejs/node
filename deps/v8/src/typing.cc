// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "typing.h"

#include "parser.h"  // for CompileTimeValue; TODO(rossberg): should move
#include "scopes.h"

namespace v8 {
namespace internal {


AstTyper::AstTyper(CompilationInfo* info)
    : info_(info),
      oracle_(
          Handle<Code>(info->closure()->shared()->code()),
          Handle<Context>(info->closure()->context()->native_context()),
          info->isolate(),
          info->zone()) {
  InitializeAstVisitor();
}


#define CHECK_ALIVE(call)                     \
  do {                                        \
    call;                                     \
    if (visitor->HasStackOverflow()) return;  \
  } while (false)


void AstTyper::Type(CompilationInfo* info) {
  AstTyper* visitor = new(info->zone()) AstTyper(info);
  Scope* scope = info->scope();

  // Handle implicit declaration of the function name in named function
  // expressions before other declarations.
  if (scope->is_function_scope() && scope->function() != NULL) {
    CHECK_ALIVE(visitor->VisitVariableDeclaration(scope->function()));
  }
  CHECK_ALIVE(visitor->VisitDeclarations(scope->declarations()));
  CHECK_ALIVE(visitor->VisitStatements(info->function()->body()));
}


#undef CHECK_ALIVE
#define CHECK_ALIVE(call)            \
  do {                               \
    call;                            \
    if (HasStackOverflow()) return;  \
  } while (false)


void AstTyper::VisitStatements(ZoneList<Statement*>* stmts) {
  ASSERT(!HasStackOverflow());
  for (int i = 0; i < stmts->length(); ++i) {
    Statement* stmt = stmts->at(i);
    CHECK_ALIVE(Visit(stmt));
  }
}


void AstTyper::VisitBlock(Block* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(VisitStatements(stmt->statements()));
}


void AstTyper::VisitExpressionStatement(ExpressionStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->expression()));
}


void AstTyper::VisitEmptyStatement(EmptyStatement* stmt) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitIfStatement(IfStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->condition()));
  CHECK_ALIVE(Visit(stmt->then_statement()));
  CHECK_ALIVE(Visit(stmt->else_statement()));

  if (!stmt->condition()->ToBooleanIsTrue() &&
      !stmt->condition()->ToBooleanIsFalse()) {
    stmt->condition()->RecordToBooleanTypeFeedback(oracle());
  }
}


void AstTyper::VisitContinueStatement(ContinueStatement* stmt) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitBreakStatement(BreakStatement* stmt) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitReturnStatement(ReturnStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->expression()));

  // TODO(rossberg): we only need this for inlining into test contexts...
  stmt->expression()->RecordToBooleanTypeFeedback(oracle());
}


void AstTyper::VisitWithStatement(WithStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(stmt->expression());
  CHECK_ALIVE(stmt->statement());
}


void AstTyper::VisitSwitchStatement(SwitchStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->tag()));
  ZoneList<CaseClause*>* clauses = stmt->cases();
  SwitchStatement::SwitchType switch_type = stmt->switch_type();
  for (int i = 0; i < clauses->length(); ++i) {
    CaseClause* clause = clauses->at(i);
    if (!clause->is_default()) {
      Expression* label = clause->label();
      CHECK_ALIVE(Visit(label));

      SwitchStatement::SwitchType label_switch_type =
          label->IsSmiLiteral() ? SwitchStatement::SMI_SWITCH :
          label->IsStringLiteral() ? SwitchStatement::STRING_SWITCH :
              SwitchStatement::GENERIC_SWITCH;
      if (switch_type == SwitchStatement::UNKNOWN_SWITCH)
        switch_type = label_switch_type;
      else if (switch_type != label_switch_type)
        switch_type = SwitchStatement::GENERIC_SWITCH;
    }
    CHECK_ALIVE(VisitStatements(clause->statements()));
  }
  if (switch_type == SwitchStatement::UNKNOWN_SWITCH)
    switch_type = SwitchStatement::GENERIC_SWITCH;
  stmt->set_switch_type(switch_type);

  // TODO(rossberg): can we eliminate this special case and extra loop?
  if (switch_type == SwitchStatement::SMI_SWITCH) {
    for (int i = 0; i < clauses->length(); ++i) {
      CaseClause* clause = clauses->at(i);
      if (!clause->is_default())
        clause->RecordTypeFeedback(oracle());
    }
  }
}


void AstTyper::VisitDoWhileStatement(DoWhileStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->body()));
  CHECK_ALIVE(Visit(stmt->cond()));

  if (!stmt->cond()->ToBooleanIsTrue()) {
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }
}


void AstTyper::VisitWhileStatement(WhileStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->cond()));
  CHECK_ALIVE(Visit(stmt->body()));

  if (!stmt->cond()->ToBooleanIsTrue()) {
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }
}


void AstTyper::VisitForStatement(ForStatement* stmt) {
  ASSERT(!HasStackOverflow());
  if (stmt->init() != NULL) {
    CHECK_ALIVE(Visit(stmt->init()));
  }
  if (stmt->cond() != NULL) {
    CHECK_ALIVE(Visit(stmt->cond()));

    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }
  CHECK_ALIVE(Visit(stmt->body()));
  if (stmt->next() != NULL) {
    CHECK_ALIVE(Visit(stmt->next()));
  }
}


void AstTyper::VisitForInStatement(ForInStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->enumerable()));
  CHECK_ALIVE(Visit(stmt->body()));

  stmt->RecordTypeFeedback(oracle());
}


void AstTyper::VisitForOfStatement(ForOfStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->iterable()));
  CHECK_ALIVE(Visit(stmt->body()));
}


void AstTyper::VisitTryCatchStatement(TryCatchStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->try_block()));
  CHECK_ALIVE(Visit(stmt->catch_block()));
}


void AstTyper::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->try_block()));
  CHECK_ALIVE(Visit(stmt->finally_block()));
}


void AstTyper::VisitDebuggerStatement(DebuggerStatement* stmt) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitFunctionLiteral(FunctionLiteral* expr) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitSharedFunctionInfoLiteral(SharedFunctionInfoLiteral* expr) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitConditional(Conditional* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->condition()));
  CHECK_ALIVE(Visit(expr->then_expression()));
  CHECK_ALIVE(Visit(expr->else_expression()));

  expr->condition()->RecordToBooleanTypeFeedback(oracle());
}


void AstTyper::VisitVariableProxy(VariableProxy* expr) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitLiteral(Literal* expr) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitRegExpLiteral(RegExpLiteral* expr) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitObjectLiteral(ObjectLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ZoneList<ObjectLiteral::Property*>* properties = expr->properties();
  for (int i = 0; i < properties->length(); ++i) {
    ObjectLiteral::Property* prop = properties->at(i);
    CHECK_ALIVE(Visit(prop->value()));

    if ((prop->kind() == ObjectLiteral::Property::MATERIALIZED_LITERAL &&
        !CompileTimeValue::IsCompileTimeValue(prop->value())) ||
        prop->kind() == ObjectLiteral::Property::COMPUTED) {
      if (prop->key()->handle()->IsInternalizedString() && prop->emit_store())
        prop->RecordTypeFeedback(oracle());
    }
  }
}


void AstTyper::VisitArrayLiteral(ArrayLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ZoneList<Expression*>* values = expr->values();
  for (int i = 0; i < values->length(); ++i) {
    Expression* value = values->at(i);
    CHECK_ALIVE(Visit(value));
  }
}


void AstTyper::VisitAssignment(Assignment* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->target()));
  CHECK_ALIVE(Visit(expr->value()));

  // TODO(rossberg): Can we clean this up?
  if (expr->is_compound()) {
    CHECK_ALIVE(Visit(expr->binary_operation()));

    Expression* target = expr->target();
    Property* prop = target->AsProperty();
    if (prop != NULL) {
      prop->RecordTypeFeedback(oracle(), zone());
      if (!prop->key()->IsPropertyName())  // i.e., keyed
        expr->RecordTypeFeedback(oracle(), zone());
    }
    return;
  }
  if (expr->target()->AsProperty())
    expr->RecordTypeFeedback(oracle(), zone());
}


void AstTyper::VisitYield(Yield* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->generator_object()));
  CHECK_ALIVE(Visit(expr->expression()));
}


void AstTyper::VisitThrow(Throw* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->exception()));
}


void AstTyper::VisitProperty(Property* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->obj()));
  CHECK_ALIVE(Visit(expr->key()));

  expr->RecordTypeFeedback(oracle(), zone());
}


void AstTyper::VisitCall(Call* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->expression()));
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    CHECK_ALIVE(Visit(arg));
  }

  Expression* callee = expr->expression();
  Property* prop = callee->AsProperty();
  if (prop != NULL) {
    if (prop->key()->IsPropertyName())
      expr->RecordTypeFeedback(oracle(), CALL_AS_METHOD);
  } else {
    expr->RecordTypeFeedback(oracle(), CALL_AS_FUNCTION);
  }
}


void AstTyper::VisitCallNew(CallNew* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->expression()));
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    CHECK_ALIVE(Visit(arg));
  }

  expr->RecordTypeFeedback(oracle());
}


void AstTyper::VisitCallRuntime(CallRuntime* expr) {
  ASSERT(!HasStackOverflow());
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    CHECK_ALIVE(Visit(arg));
  }
}


void AstTyper::VisitUnaryOperation(UnaryOperation* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->expression()));

  expr->RecordTypeFeedback(oracle());
  if (expr->op() == Token::NOT) {
    // TODO(rossberg): only do in test or value context.
    expr->expression()->RecordToBooleanTypeFeedback(oracle());
  }
}


void AstTyper::VisitCountOperation(CountOperation* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->expression()));

  expr->RecordTypeFeedback(oracle(), zone());
  Property* prop = expr->expression()->AsProperty();
  if (prop != NULL) {
    prop->RecordTypeFeedback(oracle(), zone());
  }
}


void AstTyper::VisitBinaryOperation(BinaryOperation* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->left()));
  CHECK_ALIVE(Visit(expr->right()));

  expr->RecordTypeFeedback(oracle());
  if (expr->op() == Token::OR || expr->op() == Token::AND) {
    expr->left()->RecordToBooleanTypeFeedback(oracle());
  }
}


void AstTyper::VisitCompareOperation(CompareOperation* expr) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(expr->left()));
  CHECK_ALIVE(Visit(expr->right()));

  expr->RecordTypeFeedback(oracle());
}


void AstTyper::VisitThisFunction(ThisFunction* expr) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitDeclarations(ZoneList<Declaration*>* decls) {
  ASSERT(!HasStackOverflow());
  for (int i = 0; i < decls->length(); ++i) {
    Declaration* decl = decls->at(i);
    CHECK_ALIVE(Visit(decl));
  }
}


void AstTyper::VisitVariableDeclaration(VariableDeclaration* declaration) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitFunctionDeclaration(FunctionDeclaration* declaration) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(declaration->fun()));
}


void AstTyper::VisitModuleDeclaration(ModuleDeclaration* declaration) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(declaration->module()));
}


void AstTyper::VisitImportDeclaration(ImportDeclaration* declaration) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(declaration->module()));
}


void AstTyper::VisitExportDeclaration(ExportDeclaration* declaration) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitModuleLiteral(ModuleLiteral* module) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(module->body()));
}


void AstTyper::VisitModuleVariable(ModuleVariable* module) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitModulePath(ModulePath* module) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(module->module()));
}


void AstTyper::VisitModuleUrl(ModuleUrl* module) {
  ASSERT(!HasStackOverflow());
}


void AstTyper::VisitModuleStatement(ModuleStatement* stmt) {
  ASSERT(!HasStackOverflow());
  CHECK_ALIVE(Visit(stmt->body()));
}


} }  // namespace v8::internal

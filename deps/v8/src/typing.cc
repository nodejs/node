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
          info->zone()),
      store_(info->zone()) {
  InitializeAstVisitor(info->isolate());
}


#define RECURSE(call)                         \
  do {                                        \
    ASSERT(!visitor->HasStackOverflow());     \
    call;                                     \
    if (visitor->HasStackOverflow()) return;  \
  } while (false)

void AstTyper::Run(CompilationInfo* info) {
  AstTyper* visitor = new(info->zone()) AstTyper(info);
  Scope* scope = info->scope();

  // Handle implicit declaration of the function name in named function
  // expressions before other declarations.
  if (scope->is_function_scope() && scope->function() != NULL) {
    RECURSE(visitor->VisitVariableDeclaration(scope->function()));
  }
  RECURSE(visitor->VisitDeclarations(scope->declarations()));
  RECURSE(visitor->VisitStatements(info->function()->body()));
}

#undef RECURSE

#define RECURSE(call)                \
  do {                               \
    ASSERT(!HasStackOverflow());     \
    call;                            \
    if (HasStackOverflow()) return;  \
  } while (false)


void AstTyper::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0; i < stmts->length(); ++i) {
    Statement* stmt = stmts->at(i);
    RECURSE(Visit(stmt));
    if (stmt->IsJump()) break;
  }
}


void AstTyper::VisitBlock(Block* stmt) {
  RECURSE(VisitStatements(stmt->statements()));
  if (stmt->labels() != NULL) {
    store_.Forget();  // Control may transfer here via 'break l'.
  }
}


void AstTyper::VisitExpressionStatement(ExpressionStatement* stmt) {
  RECURSE(Visit(stmt->expression()));
}


void AstTyper::VisitEmptyStatement(EmptyStatement* stmt) {
}


void AstTyper::VisitIfStatement(IfStatement* stmt) {
  // Collect type feedback.
  if (!stmt->condition()->ToBooleanIsTrue() &&
      !stmt->condition()->ToBooleanIsFalse()) {
    stmt->condition()->RecordToBooleanTypeFeedback(oracle());
  }

  RECURSE(Visit(stmt->condition()));
  Effects then_effects = EnterEffects();
  RECURSE(Visit(stmt->then_statement()));
  ExitEffects();
  Effects else_effects = EnterEffects();
  RECURSE(Visit(stmt->else_statement()));
  ExitEffects();
  then_effects.Alt(else_effects);
  store_.Seq(then_effects);
}


void AstTyper::VisitContinueStatement(ContinueStatement* stmt) {
  // TODO(rossberg): is it worth having a non-termination effect?
}


void AstTyper::VisitBreakStatement(BreakStatement* stmt) {
  // TODO(rossberg): is it worth having a non-termination effect?
}


void AstTyper::VisitReturnStatement(ReturnStatement* stmt) {
  // Collect type feedback.
  // TODO(rossberg): we only need this for inlining into test contexts...
  stmt->expression()->RecordToBooleanTypeFeedback(oracle());

  RECURSE(Visit(stmt->expression()));
  // TODO(rossberg): is it worth having a non-termination effect?
}


void AstTyper::VisitWithStatement(WithStatement* stmt) {
  RECURSE(stmt->expression());
  RECURSE(stmt->statement());
}


void AstTyper::VisitSwitchStatement(SwitchStatement* stmt) {
  RECURSE(Visit(stmt->tag()));

  ZoneList<CaseClause*>* clauses = stmt->cases();
  SwitchStatement::SwitchType switch_type = stmt->switch_type();
  Effects local_effects(zone());
  bool complex_effects = false;  // True for label effects or fall-through.

  for (int i = 0; i < clauses->length(); ++i) {
    CaseClause* clause = clauses->at(i);
    Effects clause_effects = EnterEffects();

    if (!clause->is_default()) {
      Expression* label = clause->label();
      SwitchStatement::SwitchType label_switch_type =
          label->IsSmiLiteral() ? SwitchStatement::SMI_SWITCH :
          label->IsStringLiteral() ? SwitchStatement::STRING_SWITCH :
              SwitchStatement::GENERIC_SWITCH;
      if (switch_type == SwitchStatement::UNKNOWN_SWITCH)
        switch_type = label_switch_type;
      else if (switch_type != label_switch_type)
        switch_type = SwitchStatement::GENERIC_SWITCH;

      RECURSE(Visit(label));
      if (!clause_effects.IsEmpty()) complex_effects = true;
    }

    ZoneList<Statement*>* stmts = clause->statements();
    RECURSE(VisitStatements(stmts));
    ExitEffects();
    if (stmts->is_empty() || stmts->last()->IsJump()) {
      local_effects.Alt(clause_effects);
    } else {
      complex_effects = true;
    }
  }

  if (complex_effects) {
    store_.Forget();  // Reached this in unknown state.
  } else {
    store_.Seq(local_effects);
  }

  if (switch_type == SwitchStatement::UNKNOWN_SWITCH)
    switch_type = SwitchStatement::GENERIC_SWITCH;
  stmt->set_switch_type(switch_type);

  // Collect type feedback.
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
  // Collect type feedback.
  if (!stmt->cond()->ToBooleanIsTrue()) {
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }

  // TODO(rossberg): refine the unconditional Forget (here and elsewhere) by
  // computing the set of variables assigned in only some of the origins of the
  // control transfer (such as the loop body here).
  store_.Forget();  // Control may transfer here via looping or 'continue'.
  RECURSE(Visit(stmt->body()));
  RECURSE(Visit(stmt->cond()));
  store_.Forget();  // Control may transfer here via 'break'.
}


void AstTyper::VisitWhileStatement(WhileStatement* stmt) {
  // Collect type feedback.
  if (!stmt->cond()->ToBooleanIsTrue()) {
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }

  store_.Forget();  // Control may transfer here via looping or 'continue'.
  RECURSE(Visit(stmt->cond()));
  RECURSE(Visit(stmt->body()));
  store_.Forget();  // Control may transfer here via termination or 'break'.
}


void AstTyper::VisitForStatement(ForStatement* stmt) {
  if (stmt->init() != NULL) {
    RECURSE(Visit(stmt->init()));
  }
  store_.Forget();  // Control may transfer here via looping.
  if (stmt->cond() != NULL) {
    // Collect type feedback.
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());

    RECURSE(Visit(stmt->cond()));
  }
  RECURSE(Visit(stmt->body()));
  store_.Forget();  // Control may transfer here via 'continue'.
  if (stmt->next() != NULL) {
    RECURSE(Visit(stmt->next()));
  }
  store_.Forget();  // Control may transfer here via termination or 'break'.
}


void AstTyper::VisitForInStatement(ForInStatement* stmt) {
  // Collect type feedback.
  stmt->RecordTypeFeedback(oracle());

  RECURSE(Visit(stmt->enumerable()));
  store_.Forget();  // Control may transfer here via looping or 'continue'.
  RECURSE(Visit(stmt->body()));
  store_.Forget();  // Control may transfer here via 'break'.
}


void AstTyper::VisitForOfStatement(ForOfStatement* stmt) {
  RECURSE(Visit(stmt->iterable()));
  store_.Forget();  // Control may transfer here via looping or 'continue'.
  RECURSE(Visit(stmt->body()));
  store_.Forget();  // Control may transfer here via 'break'.
}


void AstTyper::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Effects try_effects = EnterEffects();
  RECURSE(Visit(stmt->try_block()));
  ExitEffects();
  Effects catch_effects = EnterEffects();
  store_.Forget();  // Control may transfer here via 'throw'.
  RECURSE(Visit(stmt->catch_block()));
  ExitEffects();
  try_effects.Alt(catch_effects);
  store_.Seq(try_effects);
  // At this point, only variables that were reassigned in the catch block are
  // still remembered.
}


void AstTyper::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  RECURSE(Visit(stmt->try_block()));
  store_.Forget();  // Control may transfer here via 'throw'.
  RECURSE(Visit(stmt->finally_block()));
}


void AstTyper::VisitDebuggerStatement(DebuggerStatement* stmt) {
  store_.Forget();  // May do whatever.
}


void AstTyper::VisitFunctionLiteral(FunctionLiteral* expr) {
}


void AstTyper::VisitSharedFunctionInfoLiteral(SharedFunctionInfoLiteral* expr) {
}


void AstTyper::VisitConditional(Conditional* expr) {
  // Collect type feedback.
  expr->condition()->RecordToBooleanTypeFeedback(oracle());

  RECURSE(Visit(expr->condition()));
  Effects then_effects = EnterEffects();
  RECURSE(Visit(expr->then_expression()));
  ExitEffects();
  Effects else_effects = EnterEffects();
  RECURSE(Visit(expr->else_expression()));
  ExitEffects();
  then_effects.Alt(else_effects);
  store_.Seq(then_effects);

  NarrowType(expr, Bounds::Either(
      expr->then_expression()->bounds(),
      expr->else_expression()->bounds(), isolate_));
}


void AstTyper::VisitVariableProxy(VariableProxy* expr) {
  Variable* var = expr->var();
  if (var->IsStackAllocated()) {
    NarrowType(expr, store_.LookupBounds(variable_index(var)));
  }
}


void AstTyper::VisitLiteral(Literal* expr) {
  Type* type = Type::Constant(expr->value(), isolate_);
  NarrowType(expr, Bounds(type, isolate_));
}


void AstTyper::VisitRegExpLiteral(RegExpLiteral* expr) {
  NarrowType(expr, Bounds(Type::RegExp(), isolate_));
}


void AstTyper::VisitObjectLiteral(ObjectLiteral* expr) {
  ZoneList<ObjectLiteral::Property*>* properties = expr->properties();
  for (int i = 0; i < properties->length(); ++i) {
    ObjectLiteral::Property* prop = properties->at(i);

    // Collect type feedback.
    if ((prop->kind() == ObjectLiteral::Property::MATERIALIZED_LITERAL &&
        !CompileTimeValue::IsCompileTimeValue(prop->value())) ||
        prop->kind() == ObjectLiteral::Property::COMPUTED) {
      if (prop->key()->value()->IsInternalizedString() && prop->emit_store()) {
        prop->RecordTypeFeedback(oracle());
      }
    }

    RECURSE(Visit(prop->value()));
  }

  NarrowType(expr, Bounds(Type::Object(), isolate_));
}


void AstTyper::VisitArrayLiteral(ArrayLiteral* expr) {
  ZoneList<Expression*>* values = expr->values();
  for (int i = 0; i < values->length(); ++i) {
    Expression* value = values->at(i);
    RECURSE(Visit(value));
  }

  NarrowType(expr, Bounds(Type::Array(), isolate_));
}


void AstTyper::VisitAssignment(Assignment* expr) {
  // TODO(rossberg): Can we clean this up?
  if (expr->is_compound()) {
    // Collect type feedback.
    Expression* target = expr->target();
    Property* prop = target->AsProperty();
    if (prop != NULL) {
      prop->RecordTypeFeedback(oracle(), zone());
      expr->RecordTypeFeedback(oracle(), zone());
    }

    RECURSE(Visit(expr->binary_operation()));

    NarrowType(expr, expr->binary_operation()->bounds());
  } else {
    // Collect type feedback.
    if (expr->target()->IsProperty()) {
      expr->RecordTypeFeedback(oracle(), zone());
    }

    RECURSE(Visit(expr->target()));
    RECURSE(Visit(expr->value()));

    NarrowType(expr, expr->value()->bounds());
  }

  VariableProxy* proxy = expr->target()->AsVariableProxy();
  if (proxy != NULL && proxy->var()->IsStackAllocated()) {
    store_.Seq(variable_index(proxy->var()), Effect(expr->bounds()));
  }
}


void AstTyper::VisitYield(Yield* expr) {
  RECURSE(Visit(expr->generator_object()));
  RECURSE(Visit(expr->expression()));

  // We don't know anything about the result type.
}


void AstTyper::VisitThrow(Throw* expr) {
  RECURSE(Visit(expr->exception()));
  // TODO(rossberg): is it worth having a non-termination effect?

  NarrowType(expr, Bounds(Type::None(), isolate_));
}


void AstTyper::VisitProperty(Property* expr) {
  // Collect type feedback.
  expr->RecordTypeFeedback(oracle(), zone());

  RECURSE(Visit(expr->obj()));
  RECURSE(Visit(expr->key()));

  // We don't know anything about the result type.
}


void AstTyper::VisitCall(Call* expr) {
  // Collect type feedback.
  Expression* callee = expr->expression();
  Property* prop = callee->AsProperty();
  if (prop != NULL) {
    if (prop->key()->IsPropertyName())
      expr->RecordTypeFeedback(oracle(), CALL_AS_METHOD);
  } else {
    expr->RecordTypeFeedback(oracle(), CALL_AS_FUNCTION);
  }

  RECURSE(Visit(expr->expression()));
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  VariableProxy* proxy = expr->expression()->AsVariableProxy();
  if (proxy != NULL && proxy->var()->is_possibly_eval(isolate())) {
    store_.Forget();  // Eval could do whatever to local variables.
  }

  // We don't know anything about the result type.
}


void AstTyper::VisitCallNew(CallNew* expr) {
  // Collect type feedback.
  expr->RecordTypeFeedback(oracle());

  RECURSE(Visit(expr->expression()));
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  // We don't know anything about the result type.
}


void AstTyper::VisitCallRuntime(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  // We don't know anything about the result type.
}


void AstTyper::VisitUnaryOperation(UnaryOperation* expr) {
  // Collect type feedback.
  if (expr->op() == Token::NOT) {
    // TODO(rossberg): only do in test or value context.
    expr->expression()->RecordToBooleanTypeFeedback(oracle());
  }

  RECURSE(Visit(expr->expression()));

  switch (expr->op()) {
    case Token::NOT:
    case Token::DELETE:
      NarrowType(expr, Bounds(Type::Boolean(), isolate_));
      break;
    case Token::VOID:
      NarrowType(expr, Bounds(Type::Undefined(), isolate_));
      break;
    case Token::TYPEOF:
      NarrowType(expr, Bounds(Type::InternalizedString(), isolate_));
      break;
    default:
      UNREACHABLE();
  }
}


void AstTyper::VisitCountOperation(CountOperation* expr) {
  // Collect type feedback.
  expr->RecordTypeFeedback(oracle(), zone());
  Property* prop = expr->expression()->AsProperty();
  if (prop != NULL) {
    prop->RecordTypeFeedback(oracle(), zone());
  }

  RECURSE(Visit(expr->expression()));

  NarrowType(expr, Bounds(Type::Smi(), Type::Number(), isolate_));

  VariableProxy* proxy = expr->expression()->AsVariableProxy();
  if (proxy != NULL && proxy->var()->IsStackAllocated()) {
    store_.Seq(variable_index(proxy->var()), Effect(expr->bounds()));
  }
}


void AstTyper::VisitBinaryOperation(BinaryOperation* expr) {
  // Collect type feedback.
  Handle<Type> type, left_type, right_type;
  Maybe<int> fixed_right_arg;
  oracle()->BinaryType(expr->BinaryOperationFeedbackId(),
      &left_type, &right_type, &type, &fixed_right_arg);
  NarrowLowerType(expr, type);
  NarrowLowerType(expr->left(), left_type);
  NarrowLowerType(expr->right(), right_type);
  expr->set_fixed_right_arg(fixed_right_arg);
  if (expr->op() == Token::OR || expr->op() == Token::AND) {
    expr->left()->RecordToBooleanTypeFeedback(oracle());
  }

  switch (expr->op()) {
    case Token::COMMA:
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      NarrowType(expr, expr->right()->bounds());
      break;
    case Token::OR:
    case Token::AND: {
      Effects left_effects = EnterEffects();
      RECURSE(Visit(expr->left()));
      ExitEffects();
      Effects right_effects = EnterEffects();
      RECURSE(Visit(expr->right()));
      ExitEffects();
      left_effects.Alt(right_effects);
      store_.Seq(left_effects);

      NarrowType(expr, Bounds::Either(
          expr->left()->bounds(), expr->right()->bounds(), isolate_));
      break;
    }
    case Token::BIT_OR:
    case Token::BIT_AND: {
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      Type* upper = Type::Union(
          expr->left()->bounds().upper, expr->right()->bounds().upper);
      if (!upper->Is(Type::Signed32())) upper = Type::Signed32();
      NarrowType(expr, Bounds(Type::Smi(), upper, isolate_));
      break;
    }
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SAR:
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      NarrowType(expr, Bounds(Type::Smi(), Type::Signed32(), isolate_));
      break;
    case Token::SHR:
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      NarrowType(expr, Bounds(Type::Smi(), Type::Unsigned32(), isolate_));
      break;
    case Token::ADD: {
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      Bounds l = expr->left()->bounds();
      Bounds r = expr->right()->bounds();
      Type* lower =
          l.lower->Is(Type::Number()) && r.lower->Is(Type::Number()) ?
              Type::Smi() :
          l.lower->Is(Type::String()) || r.lower->Is(Type::String()) ?
              Type::String() : Type::None();
      Type* upper =
          l.upper->Is(Type::Number()) && r.upper->Is(Type::Number()) ?
              Type::Number() :
          l.upper->Is(Type::String()) || r.upper->Is(Type::String()) ?
              Type::String() : Type::NumberOrString();
      NarrowType(expr, Bounds(lower, upper, isolate_));
      break;
    }
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      NarrowType(expr, Bounds(Type::Smi(), Type::Number(), isolate_));
      break;
    default:
      UNREACHABLE();
  }
}


void AstTyper::VisitCompareOperation(CompareOperation* expr) {
  // Collect type feedback.
  Handle<Type> left_type, right_type, combined_type;
  oracle()->CompareType(expr->CompareOperationFeedbackId(),
      &left_type, &right_type, &combined_type);
  NarrowLowerType(expr->left(), left_type);
  NarrowLowerType(expr->right(), right_type);
  expr->set_combined_type(combined_type);

  RECURSE(Visit(expr->left()));
  RECURSE(Visit(expr->right()));

  NarrowType(expr, Bounds(Type::Boolean(), isolate_));
}


void AstTyper::VisitThisFunction(ThisFunction* expr) {
}


void AstTyper::VisitDeclarations(ZoneList<Declaration*>* decls) {
  for (int i = 0; i < decls->length(); ++i) {
    Declaration* decl = decls->at(i);
    RECURSE(Visit(decl));
  }
}


void AstTyper::VisitVariableDeclaration(VariableDeclaration* declaration) {
}


void AstTyper::VisitFunctionDeclaration(FunctionDeclaration* declaration) {
  RECURSE(Visit(declaration->fun()));
}


void AstTyper::VisitModuleDeclaration(ModuleDeclaration* declaration) {
  RECURSE(Visit(declaration->module()));
}


void AstTyper::VisitImportDeclaration(ImportDeclaration* declaration) {
  RECURSE(Visit(declaration->module()));
}


void AstTyper::VisitExportDeclaration(ExportDeclaration* declaration) {
}


void AstTyper::VisitModuleLiteral(ModuleLiteral* module) {
  RECURSE(Visit(module->body()));
}


void AstTyper::VisitModuleVariable(ModuleVariable* module) {
}


void AstTyper::VisitModulePath(ModulePath* module) {
  RECURSE(Visit(module->module()));
}


void AstTyper::VisitModuleUrl(ModuleUrl* module) {
}


void AstTyper::VisitModuleStatement(ModuleStatement* stmt) {
  RECURSE(Visit(stmt->body()));
}


} }  // namespace v8::internal

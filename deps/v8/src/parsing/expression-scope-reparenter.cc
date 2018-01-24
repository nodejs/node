// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/expression-scope-reparenter.h"

#include "src/ast/ast-traversal-visitor.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

class Reparenter final : public AstTraversalVisitor<Reparenter> {
 public:
  Reparenter(uintptr_t stack_limit, Expression* initializer, Scope* scope)
      : AstTraversalVisitor(stack_limit, initializer), scope_(scope) {}

 private:
  // This is required so that the overriden Visit* methods can be
  // called by the base class (template).
  friend class AstTraversalVisitor<Reparenter>;

  void VisitFunctionLiteral(FunctionLiteral* expr);
  void VisitClassLiteral(ClassLiteral* expr);
  void VisitVariableProxy(VariableProxy* expr);
  void VisitRewritableExpression(RewritableExpression* expr);

  void VisitBlock(Block* stmt);
  void VisitTryCatchStatement(TryCatchStatement* stmt);
  void VisitWithStatement(WithStatement* stmt);

  Scope* scope_;
};

void Reparenter::VisitFunctionLiteral(FunctionLiteral* function_literal) {
  function_literal->scope()->ReplaceOuterScope(scope_);
}

void Reparenter::VisitClassLiteral(ClassLiteral* class_literal) {
  class_literal->scope()->ReplaceOuterScope(scope_);
  // No need to visit the constructor since it will have the class
  // scope on its scope chain.
  DCHECK_EQ(class_literal->constructor()->scope()->outer_scope(),
            class_literal->scope());

  if (class_literal->static_fields_initializer() != nullptr) {
    DCHECK_EQ(
        class_literal->static_fields_initializer()->scope()->outer_scope(),
        class_literal->scope());
  }
#if DEBUG
  // The same goes for the rest of the class, but we do some
  // sanity checking in debug mode.
  ZoneList<ClassLiteralProperty*>* props = class_literal->properties();
  for (int i = 0; i < props->length(); ++i) {
    ClassLiteralProperty* prop = props->at(i);
    // No need to visit the values, since all values are functions with
    // the class scope on their scope chain.
    DCHECK(prop->value()->IsFunctionLiteral());
    DCHECK_EQ(prop->value()->AsFunctionLiteral()->scope()->outer_scope(),
              class_literal->scope());
  }
#endif
}

void Reparenter::VisitVariableProxy(VariableProxy* proxy) {
  if (!proxy->is_resolved()) {
    if (scope_->outer_scope()->RemoveUnresolved(proxy)) {
      scope_->AddUnresolved(proxy);
    }
  } else {
    // Ensure that temporaries we find are already in the correct scope.
    DCHECK(proxy->var()->mode() != TEMPORARY ||
           proxy->var()->scope() == scope_->GetClosureScope());
  }
}

void Reparenter::VisitRewritableExpression(RewritableExpression* expr) {
  Visit(expr->expression());
  expr->set_scope(scope_);
}

void Reparenter::VisitBlock(Block* stmt) {
  if (stmt->scope() != nullptr)
    stmt->scope()->ReplaceOuterScope(scope_);
  else
    VisitStatements(stmt->statements());
}

void Reparenter::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Visit(stmt->try_block());
  stmt->scope()->ReplaceOuterScope(scope_);
}

void Reparenter::VisitWithStatement(WithStatement* stmt) {
  Visit(stmt->expression());
  stmt->scope()->ReplaceOuterScope(scope_);
}

}  // anonymous namespace

void ReparentExpressionScope(uintptr_t stack_limit, Expression* expr,
                             Scope* scope) {
  // The only case that uses this code is block scopes for parameters containing
  // sloppy eval.
  DCHECK(scope->is_block_scope());
  DCHECK(scope->is_declaration_scope());
  DCHECK(scope->AsDeclarationScope()->calls_sloppy_eval());
  DCHECK(scope->outer_scope()->is_function_scope());

  Reparenter r(stack_limit, expr, scope);
  r.Run();
}

}  // namespace internal
}  // namespace v8

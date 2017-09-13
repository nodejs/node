// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parameter-initializer-rewriter.h"

#include "src/ast/ast-traversal-visitor.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {


class Rewriter final : public AstTraversalVisitor<Rewriter> {
 public:
  Rewriter(uintptr_t stack_limit, Expression* initializer, Scope* scope)
      : AstTraversalVisitor(stack_limit, initializer), scope_(scope) {}

 private:
  // This is required so that the overriden Visit* methods can be
  // called by the base class (template).
  friend class AstTraversalVisitor<Rewriter>;

  void VisitFunctionLiteral(FunctionLiteral* expr);
  void VisitClassLiteral(ClassLiteral* expr);
  void VisitVariableProxy(VariableProxy* expr);

  void VisitBlock(Block* stmt);
  void VisitTryCatchStatement(TryCatchStatement* stmt);
  void VisitWithStatement(WithStatement* stmt);

  Scope* scope_;
};

void Rewriter::VisitFunctionLiteral(FunctionLiteral* function_literal) {
  function_literal->scope()->ReplaceOuterScope(scope_);
}


void Rewriter::VisitClassLiteral(ClassLiteral* class_literal) {
  if (class_literal->extends() != nullptr) {
    Visit(class_literal->extends());
  }
  // No need to visit the constructor since it will have the class
  // scope on its scope chain.
  ZoneList<ClassLiteralProperty*>* props = class_literal->properties();
  for (int i = 0; i < props->length(); ++i) {
    ClassLiteralProperty* prop = props->at(i);
    if (!prop->key()->IsLiteral()) {
      Visit(prop->key());
    }
    // No need to visit the values, since all values are functions with
    // the class scope on their scope chain.
    DCHECK(prop->value()->IsFunctionLiteral());
  }
}


void Rewriter::VisitVariableProxy(VariableProxy* proxy) {
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


void Rewriter::VisitBlock(Block* stmt) {
  if (stmt->scope() != nullptr)
    stmt->scope()->ReplaceOuterScope(scope_);
  else
    VisitStatements(stmt->statements());
}


void Rewriter::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Visit(stmt->try_block());
  stmt->scope()->ReplaceOuterScope(scope_);
}


void Rewriter::VisitWithStatement(WithStatement* stmt) {
  Visit(stmt->expression());
  stmt->scope()->ReplaceOuterScope(scope_);
}


}  // anonymous namespace

void ReparentExpressionScope(uintptr_t stack_limit, Expression* expr,
                             Scope* scope) {
  // Both uses of this function should pass in a block scope.
  DCHECK(scope->is_block_scope());
  // These hold for the sloppy parameters-with-eval case...
  DCHECK_IMPLIES(scope->is_declaration_scope(), scope->calls_sloppy_eval());
  DCHECK_IMPLIES(scope->is_declaration_scope(),
                 scope->outer_scope()->is_function_scope());
  // ...whereas these hold for lexical declarations in for-in/of loops.
  DCHECK_IMPLIES(!scope->is_declaration_scope(),
                 scope->outer_scope()->is_block_scope());
  DCHECK_IMPLIES(!scope->is_declaration_scope(),
                 scope->outer_scope()->is_hidden());

  Rewriter rewriter(stack_limit, expr, scope);
  rewriter.Run();
}


}  // namespace internal
}  // namespace v8

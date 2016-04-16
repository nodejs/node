// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parameter-initializer-rewriter.h"

#include "src/ast/ast.h"
#include "src/ast/ast-expression-visitor.h"
#include "src/ast/scopes.h"

namespace v8 {
namespace internal {

namespace {


class Rewriter final : public AstExpressionVisitor {
 public:
  Rewriter(uintptr_t stack_limit, Expression* initializer, Scope* old_scope,
           Scope* new_scope)
      : AstExpressionVisitor(stack_limit, initializer),
        old_scope_(old_scope),
        new_scope_(new_scope) {}

 private:
  void VisitExpression(Expression* expr) override {}

  void VisitFunctionLiteral(FunctionLiteral* expr) override;
  void VisitClassLiteral(ClassLiteral* expr) override;
  void VisitVariableProxy(VariableProxy* expr) override;

  Scope* old_scope_;
  Scope* new_scope_;
};


void Rewriter::VisitFunctionLiteral(FunctionLiteral* function_literal) {
  function_literal->scope()->ReplaceOuterScope(new_scope_);
}


void Rewriter::VisitClassLiteral(ClassLiteral* class_literal) {
  class_literal->scope()->ReplaceOuterScope(new_scope_);
  if (class_literal->extends() != nullptr) {
    Visit(class_literal->extends());
  }
  // No need to visit the constructor since it will have the class
  // scope on its scope chain.
  ZoneList<ObjectLiteralProperty*>* props = class_literal->properties();
  for (int i = 0; i < props->length(); ++i) {
    ObjectLiteralProperty* prop = props->at(i);
    if (!prop->key()->IsLiteral()) {
      Visit(prop->key());
    }
    // No need to visit the values, since all values are functions with
    // the class scope on their scope chain.
    DCHECK(prop->value()->IsFunctionLiteral());
  }
}


void Rewriter::VisitVariableProxy(VariableProxy* proxy) {
  if (proxy->is_resolved()) {
    Variable* var = proxy->var();
    DCHECK_EQ(var->mode(), TEMPORARY);
    if (old_scope_->RemoveTemporary(var)) {
      var->set_scope(new_scope_);
      new_scope_->AddTemporary(var);
    }
  } else if (old_scope_->RemoveUnresolved(proxy)) {
    new_scope_->AddUnresolved(proxy);
  }
}


}  // anonymous namespace


void RewriteParameterInitializerScope(uintptr_t stack_limit,
                                      Expression* initializer, Scope* old_scope,
                                      Scope* new_scope) {
  Rewriter rewriter(stack_limit, initializer, old_scope, new_scope);
  rewriter.Run();
}


}  // namespace internal
}  // namespace v8

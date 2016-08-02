// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parameter-initializer-rewriter.h"

#include <algorithm>
#include <utility>
#include <vector>

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
  ~Rewriter();

 private:
  void VisitExpression(Expression* expr) override {}

  void VisitFunctionLiteral(FunctionLiteral* expr) override;
  void VisitClassLiteral(ClassLiteral* expr) override;
  void VisitVariableProxy(VariableProxy* expr) override;

  void VisitBlock(Block* stmt) override;
  void VisitTryCatchStatement(TryCatchStatement* stmt) override;
  void VisitWithStatement(WithStatement* stmt) override;

  Scope* old_scope_;
  Scope* new_scope_;
  std::vector<std::pair<Variable*, int>> temps_;
};

struct LessThanSecond {
  bool operator()(const std::pair<Variable*, int>& left,
                  const std::pair<Variable*, int>& right) {
    return left.second < right.second;
  }
};

Rewriter::~Rewriter() {
  if (!temps_.empty()) {
    // Ensure that we add temporaries in the order they appeared in old_scope_.
    std::sort(temps_.begin(), temps_.end(), LessThanSecond());
    for (auto var_and_index : temps_) {
      var_and_index.first->set_scope(new_scope_);
      new_scope_->AddTemporary(var_and_index.first);
    }
  }
}

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
    if (var->mode() != TEMPORARY) return;
    // For rewriting inside the same ClosureScope (e.g., putting default
    // parameter values in their own inner scope in certain cases), refrain
    // from invalidly moving temporaries to a block scope.
    if (var->scope()->ClosureScope() == new_scope_->ClosureScope()) return;
    int index = old_scope_->RemoveTemporary(var);
    if (index >= 0) {
      temps_.push_back(std::make_pair(var, index));
    }
  } else if (old_scope_->RemoveUnresolved(proxy)) {
    new_scope_->AddUnresolved(proxy);
  }
}


void Rewriter::VisitBlock(Block* stmt) {
  if (stmt->scope() != nullptr)
    stmt->scope()->ReplaceOuterScope(new_scope_);
  else
    VisitStatements(stmt->statements());
}


void Rewriter::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Visit(stmt->try_block());
  stmt->scope()->ReplaceOuterScope(new_scope_);
}


void Rewriter::VisitWithStatement(WithStatement* stmt) {
  Visit(stmt->expression());
  stmt->scope()->ReplaceOuterScope(new_scope_);
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

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_EXPRESSION_REWRITER_H_
#define V8_AST_AST_EXPRESSION_REWRITER_H_

#include "src/allocation.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/effects.h"
#include "src/type-info.h"
#include "src/types.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// A rewriting Visitor over a CompilationInfo's AST that invokes
// VisitExpression on each expression node.

class AstExpressionRewriter : public AstVisitor {
 public:
  explicit AstExpressionRewriter(Isolate* isolate) : AstVisitor() {
    InitializeAstRewriter(isolate);
  }
  explicit AstExpressionRewriter(uintptr_t stack_limit) : AstVisitor() {
    InitializeAstRewriter(stack_limit);
  }
  ~AstExpressionRewriter() override {}

  void VisitDeclarations(ZoneList<Declaration*>* declarations) override;
  void VisitStatements(ZoneList<Statement*>* statements) override;
  void VisitExpressions(ZoneList<Expression*>* expressions) override;

  virtual void VisitObjectLiteralProperty(ObjectLiteralProperty* property);

 protected:
  virtual bool RewriteExpression(Expression* expr) = 0;

 private:
  DEFINE_AST_REWRITER_SUBCLASS_MEMBERS();

#define DECLARE_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  DISALLOW_COPY_AND_ASSIGN(AstExpressionRewriter);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_EXPRESSION_REWRITER_H_

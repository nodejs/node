// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_EXPRESSION_REWRITER_H_
#define V8_AST_AST_EXPRESSION_REWRITER_H_

#include "src/allocation.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/type-info.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// A rewriting Visitor over a CompilationInfo's AST that invokes
// VisitExpression on each expression node.

// This AstVistor is not final, and provides the AstVisitor methods as virtual
// methods so they can be specialized by subclasses.
class AstExpressionRewriter : public AstVisitor<AstExpressionRewriter> {
 public:
  explicit AstExpressionRewriter(Isolate* isolate) {
    InitializeAstRewriter(isolate);
  }
  explicit AstExpressionRewriter(uintptr_t stack_limit) {
    InitializeAstRewriter(stack_limit);
  }
  virtual ~AstExpressionRewriter() {}

  virtual void VisitDeclarations(Declaration::List* declarations);
  virtual void VisitStatements(ZoneList<Statement*>* statements);
  virtual void VisitExpressions(ZoneList<Expression*>* expressions);

  virtual void VisitLiteralProperty(LiteralProperty* property);

 protected:
  virtual bool RewriteExpression(Expression* expr) = 0;

 private:
  DEFINE_AST_REWRITER_SUBCLASS_MEMBERS();

#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  DISALLOW_COPY_AND_ASSIGN(AstExpressionRewriter);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_EXPRESSION_REWRITER_H_

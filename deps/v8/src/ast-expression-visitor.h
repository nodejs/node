// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_EXPRESSION_VISITOR_H_
#define V8_AST_EXPRESSION_VISITOR_H_

#include "src/allocation.h"
#include "src/ast.h"
#include "src/effects.h"
#include "src/scopes.h"
#include "src/type-info.h"
#include "src/types.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// A Visitor over a CompilationInfo's AST that invokes
// VisitExpression on each expression node.

class AstExpressionVisitor : public AstVisitor {
 public:
  AstExpressionVisitor(Isolate* isolate, Zone* zone, FunctionLiteral* root);
  void Run();

 protected:
  virtual void VisitExpression(Expression* expression) = 0;
  int depth() { return depth_; }

 private:
  void VisitDeclarations(ZoneList<Declaration*>* d) override;
  void VisitStatements(ZoneList<Statement*>* s) override;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

#define DECLARE_VISIT(type) virtual void Visit##type(type* node) override;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  FunctionLiteral* root_;
  int depth_;

  DISALLOW_COPY_AND_ASSIGN(AstExpressionVisitor);
};
}
}  // namespace v8::internal

#endif  // V8_AST_EXPRESSION_VISITOR_H_

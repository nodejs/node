// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_LITERAL_REINDEXER
#define V8_AST_AST_LITERAL_REINDEXER

#include "src/ast/ast.h"
#include "src/ast/scopes.h"

namespace v8 {
namespace internal {

class AstLiteralReindexer final : public AstVisitor {
 public:
  AstLiteralReindexer() : AstVisitor(), next_index_(0) {}

  int count() const { return next_index_; }
  void Reindex(Expression* pattern);

 private:
#define DEFINE_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DEFINE_VISIT)
#undef DEFINE_VISIT

  void VisitStatements(ZoneList<Statement*>* statements) override;
  void VisitDeclarations(ZoneList<Declaration*>* declarations) override;
  void VisitArguments(ZoneList<Expression*>* arguments);
  void VisitObjectLiteralProperty(ObjectLiteralProperty* property);

  void UpdateIndex(MaterializedLiteral* literal) {
    literal->literal_index_ = next_index_++;
  }

  void Visit(AstNode* node) override { node->Accept(this); }

  int next_index_;

  DISALLOW_COPY_AND_ASSIGN(AstLiteralReindexer);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_LITERAL_REINDEXER

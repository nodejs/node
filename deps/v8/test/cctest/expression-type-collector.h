// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPRESSION_TYPE_COLLECTOR_H_
#define V8_EXPRESSION_TYPE_COLLECTOR_H_

#include "src/ast/ast-expression-visitor.h"

namespace v8 {
namespace internal {

// A Visitor over an AST that collects a human readable string summarizing
// structure and types. Used for testing of the typing information attached
// to the expression nodes of an AST.

struct ExpressionTypeEntry {
  int depth;
  const char* kind;
  const AstRawString* name;
  Bounds bounds;
};

class ExpressionTypeCollector : public AstExpressionVisitor {
 public:
  ExpressionTypeCollector(Isolate* isolate, FunctionLiteral* root,
                          ZoneVector<ExpressionTypeEntry>* dst);
  void Run();

 protected:
  void VisitExpression(Expression* expression);

 private:
  ZoneVector<ExpressionTypeEntry>* result_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_EXPRESSION_TYPE_COLLECTOR_H_

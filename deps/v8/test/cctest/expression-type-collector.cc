// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "test/cctest/expression-type-collector.h"

#include "src/ast/ast-type-bounds.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/codegen.h"

namespace v8 {
namespace internal {
namespace {

struct {
  AstNode::NodeType type;
  const char* name;
} NodeTypeNameList[] = {
#define DECLARE_VISIT(type)   \
  { AstNode::k##type, #type } \
  ,
    AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT
};

}  // namespace

ExpressionTypeCollector::ExpressionTypeCollector(
    Isolate* isolate, FunctionLiteral* root, const AstTypeBounds* bounds,
    ZoneVector<ExpressionTypeEntry>* dst)
    : AstExpressionVisitor(isolate, root), bounds_(bounds), result_(dst) {}

void ExpressionTypeCollector::Run() {
  result_->clear();
  AstExpressionVisitor::Run();
}


void ExpressionTypeCollector::VisitExpression(Expression* expression) {
  ExpressionTypeEntry e;
  e.depth = depth();
  VariableProxy* proxy = expression->AsVariableProxy();
  if (proxy) {
    e.name = proxy->raw_name();
  }
  e.bounds = bounds_->get(expression);
  AstNode::NodeType type = expression->node_type();
  e.kind = "unknown";
  for (size_t i = 0; i < arraysize(NodeTypeNameList); ++i) {
    if (NodeTypeNameList[i].type == type) {
      e.kind = NodeTypeNameList[i].name;
      break;
    }
  }
  result_->push_back(e);
}

}  // namespace internal
}  // namespace v8

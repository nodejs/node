// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/fast/ast.h"

namespace redline {
namespace fast {

const char* NodeKindName(NodeKind kind) {
  switch (kind) {
#define KIND_NAME(name)   \
  case NodeKind::k##name: \
    return #name;
    FAST_NODE_KIND_LIST(KIND_NAME)
#undef KIND_NAME
  }
  return "?";
}

bool IsStatementKind(NodeKind kind) {
  switch (kind) {
#define STATEMENT_CASE(name) case NodeKind::k##name:
    FAST_STATEMENT_KIND_LIST(STATEMENT_CASE)
#undef STATEMENT_CASE
    return true;
    default:
      return false;
  }
}

bool IsExpressionKind(NodeKind kind) {
  switch (kind) {
#define EXPRESSION_CASE(name) case NodeKind::k##name:
    FAST_EXPRESSION_KIND_LIST(EXPRESSION_CASE)
#undef EXPRESSION_CASE
    return true;
    default:
      return false;
  }
}

bool Node::IsPattern() const {
  switch (kind) {
    case NodeKind::kObjectPattern:
    case NodeKind::kArrayPattern:
    case NodeKind::kAssignmentPattern:
    case NodeKind::kRestElement:
      return true;
    default:
      return false;
  }
}

bool Node::IsFunction() const {
  switch (kind) {
    case NodeKind::kFunctionDeclaration:
    case NodeKind::kFunctionExpression:
    case NodeKind::kArrowFunctionExpression:
      return true;
    default:
      return false;
  }
}

bool Node::IsClass() const {
  return kind == NodeKind::kClassDeclaration ||
         kind == NodeKind::kClassExpression;
}

#define OPERATOR_TEXT(type, list)       \
  const char* OperatorText(type op) {   \
    switch (op) { list(OPERATOR_CASE) } \
    return "?";                         \
  }
#define OPERATOR_CASE(name, text) \
  case decltype(op)::name:        \
    return text;
OPERATOR_TEXT(UnaryOp, FAST_UNARY_OP_LIST)
OPERATOR_TEXT(UpdateOp, FAST_UPDATE_OP_LIST)
OPERATOR_TEXT(BinaryOp, FAST_BINARY_OP_LIST)
OPERATOR_TEXT(LogicalOp, FAST_LOGICAL_OP_LIST)
OPERATOR_TEXT(AssignOp, FAST_ASSIGN_OP_LIST)
#undef OPERATOR_CASE
#undef OPERATOR_TEXT

int BinaryPrecedence(BinaryOp op) {
  switch (op) {
    case BinaryOp::kBitOr:
      return 4;
    case BinaryOp::kBitXor:
      return 5;
    case BinaryOp::kBitAnd:
      return 6;
    case BinaryOp::kEq:
    case BinaryOp::kNotEq:
    case BinaryOp::kStrictEq:
    case BinaryOp::kStrictNotEq:
      return 7;
    case BinaryOp::kLessThan:
    case BinaryOp::kLessThanEq:
    case BinaryOp::kGreaterThan:
    case BinaryOp::kGreaterThanEq:
    case BinaryOp::kIn:
    case BinaryOp::kInstanceof:
      return 8;
    case BinaryOp::kShiftLeft:
    case BinaryOp::kShiftRight:
    case BinaryOp::kShiftRightLogical:
      return 9;
    case BinaryOp::kAdd:
    case BinaryOp::kSub:
      return 10;
    case BinaryOp::kMul:
    case BinaryOp::kDiv:
    case BinaryOp::kMod:
      return 11;
    case BinaryOp::kExp:
      return 12;
  }
  return 0;
}

int LogicalPrecedence(LogicalOp op) {
  switch (op) {
    case LogicalOp::kNullish:
      return 1;
    case LogicalOp::kOr:
      return 2;
    case LogicalOp::kAnd:
      return 3;
  }
  return 0;
}

const char* VariableKindText(VariableKind kind) {
  switch (kind) {
    case VariableKind::kVar:
      return "var";
    case VariableKind::kLet:
      return "let";
    case VariableKind::kConst:
      return "const";
    case VariableKind::kUsing:
      return "using";
    case VariableKind::kAwaitUsing:
      return "await using";
  }
  return "?";
}

FunctionData* AsFunctionData(Node* node) {
  if (node == nullptr) {
    return nullptr;
  }
  switch (node->kind) {
    case NodeKind::kFunctionDeclaration:
      return node->Cast<FunctionDeclaration>();
    case NodeKind::kFunctionExpression:
      return node->Cast<FunctionExpression>();
    case NodeKind::kArrowFunctionExpression:
      return node->Cast<ArrowFunctionExpression>();
    default:
      return nullptr;
  }
}

const FunctionData* AsFunctionData(const Node* node) {
  return AsFunctionData(const_cast<Node*>(node));
}

ClassData* AsClassData(Node* node) {
  if (node == nullptr) {
    return nullptr;
  }
  switch (node->kind) {
    case NodeKind::kClassDeclaration:
      return node->Cast<ClassDeclaration>();
    case NodeKind::kClassExpression:
      return node->Cast<ClassExpression>();
    default:
      return nullptr;
  }
}

const ClassData* AsClassData(const Node* node) {
  return AsClassData(const_cast<Node*>(node));
}

size_t CountNodes(const Node* node) {
  size_t count = 0;
  Walk(const_cast<Node*>(node), [&](Node* visited) {
    (void)visited;
    count++;
    return true;
  });
  return count;
}

}  // namespace fast
}  // namespace redline

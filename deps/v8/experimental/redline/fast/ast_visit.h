// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_AST_VISIT_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_AST_VISIT_H_

#include "experimental/redline/fast/ast.h"

namespace redline {
namespace fast {

namespace detail {

// All node pointer types share the representation of Node* (single
// inheritance chain from Node comes first in every node class), so a slot
// holding a derived pointer can be handed out as a Node** slot.
template <typename T, typename F>
inline void VisitSlot(T*& slot, F& f) {
  static_assert(std::is_base_of_v<Node, T>);
  Node* child = slot;
  f(&child);
  slot = static_cast<T*>(child);
}

template <typename T, typename F>
inline void VisitList(std::vector<T*>& list, F& f) {
  static_assert(std::is_base_of_v<Node, T>);
  for (T*& element : list) {
    VisitSlot(element, f);
  }
}

template <typename F>
inline void VisitFunction(FunctionData* fn, F& f) {
  VisitSlot(fn->name, f);
  VisitList(fn->params, f);
  VisitSlot(fn->body, f);
  VisitSlot(fn->expression_body, f);
}

template <typename F>
inline void VisitClass(ClassData* cls, F& f) {
  VisitSlot(cls->name, f);
  VisitSlot(cls->super_class, f);
  VisitSlot(cls->body, f);
}

}  // namespace detail

template <typename F>
void ForEachChildSlot(Node* node, F&& f) {
  using detail::VisitList;
  using detail::VisitSlot;
  switch (node->kind) {
    case NodeKind::kProgram:
      VisitList(node->Cast<Program>()->body, f);
      return;
    case NodeKind::kExpressionStatement:
      VisitSlot(node->Cast<ExpressionStatement>()->expression, f);
      return;
    case NodeKind::kBlockStatement:
      VisitList(node->Cast<BlockStatement>()->body, f);
      return;
    case NodeKind::kEmptyStatement:
    case NodeKind::kDebuggerStatement:
      return;
    case NodeKind::kWithStatement: {
      auto* n = node->Cast<WithStatement>();
      VisitSlot(n->object, f);
      VisitSlot(n->body, f);
      return;
    }
    case NodeKind::kReturnStatement:
      VisitSlot(node->Cast<ReturnStatement>()->argument, f);
      return;
    case NodeKind::kLabeledStatement: {
      auto* n = node->Cast<LabeledStatement>();
      VisitSlot(n->label, f);
      VisitSlot(n->body, f);
      return;
    }
    case NodeKind::kBreakStatement:
      VisitSlot(node->Cast<BreakStatement>()->label, f);
      return;
    case NodeKind::kContinueStatement:
      VisitSlot(node->Cast<ContinueStatement>()->label, f);
      return;
    case NodeKind::kIfStatement: {
      auto* n = node->Cast<IfStatement>();
      VisitSlot(n->test, f);
      VisitSlot(n->consequent, f);
      VisitSlot(n->alternate, f);
      return;
    }
    case NodeKind::kSwitchStatement: {
      auto* n = node->Cast<SwitchStatement>();
      VisitSlot(n->discriminant, f);
      VisitList(n->cases, f);
      return;
    }
    case NodeKind::kSwitchCase: {
      auto* n = node->Cast<SwitchCase>();
      VisitSlot(n->test, f);
      VisitList(n->consequent, f);
      return;
    }
    case NodeKind::kThrowStatement:
      VisitSlot(node->Cast<ThrowStatement>()->argument, f);
      return;
    case NodeKind::kTryStatement: {
      auto* n = node->Cast<TryStatement>();
      VisitSlot(n->block, f);
      VisitSlot(n->handler, f);
      VisitSlot(n->finalizer, f);
      return;
    }
    case NodeKind::kCatchClause: {
      auto* n = node->Cast<CatchClause>();
      VisitSlot(n->param, f);
      VisitSlot(n->body, f);
      return;
    }
    case NodeKind::kWhileStatement: {
      auto* n = node->Cast<WhileStatement>();
      VisitSlot(n->test, f);
      VisitSlot(n->body, f);
      return;
    }
    case NodeKind::kDoWhileStatement: {
      auto* n = node->Cast<DoWhileStatement>();
      VisitSlot(n->body, f);
      VisitSlot(n->test, f);
      return;
    }
    case NodeKind::kForStatement: {
      auto* n = node->Cast<ForStatement>();
      VisitSlot(n->init, f);
      VisitSlot(n->test, f);
      VisitSlot(n->update, f);
      VisitSlot(n->body, f);
      return;
    }
    case NodeKind::kForInStatement: {
      auto* n = node->Cast<ForInStatement>();
      VisitSlot(n->left, f);
      VisitSlot(n->right, f);
      VisitSlot(n->body, f);
      return;
    }
    case NodeKind::kForOfStatement: {
      auto* n = node->Cast<ForOfStatement>();
      VisitSlot(n->left, f);
      VisitSlot(n->right, f);
      VisitSlot(n->body, f);
      return;
    }
    case NodeKind::kFunctionDeclaration:
      detail::VisitFunction(node->Cast<FunctionDeclaration>(), f);
      return;
    case NodeKind::kVariableDeclaration:
      VisitList(node->Cast<VariableDeclaration>()->declarations, f);
      return;
    case NodeKind::kVariableDeclarator: {
      auto* n = node->Cast<VariableDeclarator>();
      VisitSlot(n->target, f);
      VisitSlot(n->init, f);
      return;
    }
    case NodeKind::kClassDeclaration:
      detail::VisitClass(node->Cast<ClassDeclaration>(), f);
      return;
    case NodeKind::kIdentifier:
    case NodeKind::kPrivateIdentifier:
    case NodeKind::kNullLiteral:
    case NodeKind::kBooleanLiteral:
    case NodeKind::kNumericLiteral:
    case NodeKind::kBigIntLiteral:
    case NodeKind::kStringLiteral:
    case NodeKind::kRegExpLiteral:
    case NodeKind::kThisExpression:
    case NodeKind::kSuper:
    case NodeKind::kMetaProperty:
    case NodeKind::kTemplateElement:
      return;
    case NodeKind::kArrayExpression:
      VisitList(node->Cast<ArrayExpression>()->elements, f);
      return;
    case NodeKind::kObjectExpression:
      VisitList(node->Cast<ObjectExpression>()->properties, f);
      return;
    case NodeKind::kProperty: {
      auto* n = node->Cast<Property>();
      VisitSlot(n->key, f);
      VisitSlot(n->value, f);
      return;
    }
    case NodeKind::kFunctionExpression:
      detail::VisitFunction(node->Cast<FunctionExpression>(), f);
      return;
    case NodeKind::kArrowFunctionExpression:
      detail::VisitFunction(node->Cast<ArrowFunctionExpression>(), f);
      return;
    case NodeKind::kClassExpression:
      detail::VisitClass(node->Cast<ClassExpression>(), f);
      return;
    case NodeKind::kUnaryExpression:
      VisitSlot(node->Cast<UnaryExpression>()->argument, f);
      return;
    case NodeKind::kUpdateExpression:
      VisitSlot(node->Cast<UpdateExpression>()->argument, f);
      return;
    case NodeKind::kBinaryExpression: {
      auto* n = node->Cast<BinaryExpression>();
      VisitSlot(n->left, f);
      VisitSlot(n->right, f);
      return;
    }
    case NodeKind::kLogicalExpression: {
      auto* n = node->Cast<LogicalExpression>();
      VisitSlot(n->left, f);
      VisitSlot(n->right, f);
      return;
    }
    case NodeKind::kAssignmentExpression: {
      auto* n = node->Cast<AssignmentExpression>();
      VisitSlot(n->left, f);
      VisitSlot(n->right, f);
      return;
    }
    case NodeKind::kMemberExpression: {
      auto* n = node->Cast<MemberExpression>();
      VisitSlot(n->object, f);
      VisitSlot(n->property, f);
      return;
    }
    case NodeKind::kConditionalExpression: {
      auto* n = node->Cast<ConditionalExpression>();
      VisitSlot(n->test, f);
      VisitSlot(n->consequent, f);
      VisitSlot(n->alternate, f);
      return;
    }
    case NodeKind::kCallExpression: {
      auto* n = node->Cast<CallExpression>();
      VisitSlot(n->callee, f);
      VisitList(n->arguments, f);
      return;
    }
    case NodeKind::kNewExpression: {
      auto* n = node->Cast<NewExpression>();
      VisitSlot(n->callee, f);
      VisitList(n->arguments, f);
      return;
    }
    case NodeKind::kSequenceExpression:
      VisitList(node->Cast<SequenceExpression>()->expressions, f);
      return;
    case NodeKind::kYieldExpression:
      VisitSlot(node->Cast<YieldExpression>()->argument, f);
      return;
    case NodeKind::kAwaitExpression:
      VisitSlot(node->Cast<AwaitExpression>()->argument, f);
      return;
    case NodeKind::kTemplateLiteral: {
      // Interleave quasis and expressions in source order.
      auto* n = node->Cast<TemplateLiteral>();
      for (size_t i = 0; i < n->quasis.size(); i++) {
        VisitSlot(n->quasis[i], f);
        if (i < n->expressions.size()) {
          VisitSlot(n->expressions[i], f);
        }
      }
      return;
    }
    case NodeKind::kTaggedTemplateExpression: {
      auto* n = node->Cast<TaggedTemplateExpression>();
      VisitSlot(n->tag, f);
      VisitSlot(n->quasi, f);
      return;
    }
    case NodeKind::kImportExpression: {
      auto* n = node->Cast<ImportExpression>();
      VisitSlot(n->source, f);
      VisitSlot(n->options, f);
      return;
    }
    case NodeKind::kChainExpression:
      VisitSlot(node->Cast<ChainExpression>()->expression, f);
      return;
    case NodeKind::kSpreadElement:
      VisitSlot(node->Cast<SpreadElement>()->argument, f);
      return;
    case NodeKind::kObjectPattern:
      VisitList(node->Cast<ObjectPattern>()->properties, f);
      return;
    case NodeKind::kArrayPattern:
      VisitList(node->Cast<ArrayPattern>()->elements, f);
      return;
    case NodeKind::kRestElement:
      VisitSlot(node->Cast<RestElement>()->argument, f);
      return;
    case NodeKind::kAssignmentPattern: {
      auto* n = node->Cast<AssignmentPattern>();
      VisitSlot(n->left, f);
      VisitSlot(n->right, f);
      return;
    }
    case NodeKind::kNativeCallExpression:
      VisitList(node->Cast<NativeCallExpression>()->arguments, f);
      return;
    case NodeKind::kClassBody:
      VisitList(node->Cast<ClassBody>()->members, f);
      return;
    case NodeKind::kMethodDefinition: {
      auto* n = node->Cast<MethodDefinition>();
      VisitSlot(n->key, f);
      VisitSlot(n->value, f);
      return;
    }
    case NodeKind::kPropertyDefinition: {
      auto* n = node->Cast<PropertyDefinition>();
      VisitSlot(n->key, f);
      VisitSlot(n->value, f);
      return;
    }
    case NodeKind::kStaticBlock:
      VisitList(node->Cast<StaticBlock>()->body, f);
      return;
  }
}

}  // namespace fast
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_AST_VISIT_H_

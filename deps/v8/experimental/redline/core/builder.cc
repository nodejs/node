// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/builder.h"

#include <algorithm>

#include "experimental/redline/core/ast_builder.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

const char* JsTypeName(JsType type) {
  switch (type) {
    case JsType::kUnknown:
      return "unknown";
    case JsType::kNumber:
      return "number";
    case JsType::kString:
      return "string";
    case JsType::kBoolean:
      return "boolean";
    case JsType::kBigInt:
      return "bigint";
    case JsType::kSymbol:
      return "symbol";
    case JsType::kNullish:
      return "nullish";
    case JsType::kArray:
      return "array";
    case JsType::kObject:
      return "object";
    case JsType::kFunction:
      return "function";
    case JsType::kClass:
      return "class";
    case JsType::kRegExp:
      return "regexp";
    case JsType::kMap:
      return "map";
    case JsType::kSet:
      return "set";
    case JsType::kPromise:
      return "promise";
    case JsType::kTypedArray:
      return "typedarray";
    case JsType::kArrayBuffer:
      return "arraybuffer";
    case JsType::kDate:
      return "date";
    case JsType::kError:
      return "error";
  }
  return "?";
}

Builder::Builder(Tree* tree, Random* rng) : tree_(tree), rng_(rng) {}
Builder::~Builder() = default;

void Builder::Invalidate() {
  scopes_.reset();
  parents_valid_ = false;
  // used_names_ only grows; names of removed code stay reserved, which is
  // harmless.
}

namespace {

// Assigns pre-order positions: start on entry, end on exit, each unique.
// Scope analysis orders declarations and references by these positions
// (TDZ approximation), so after edits they must reflect tree order rather
// than the original source offsets, and synthesized nodes must have them.
void Renumber(Node* node, int32_t* counter) {
  node->start = (*counter)++;
  ForEachChild(node, [&](Node* child) { Renumber(child, counter); });
  node->end = (*counter)++;
}

}  // namespace

void Builder::EnsureAnalysis() {
  if (scopes_ != nullptr) {
    return;
  }
  int32_t counter = 0;
  Renumber(tree_->root(), &counter);
  scopes_ = ScopeAnalysis::Analyze(tree_);
}

ScopeAnalysis& Builder::scopes() {
  EnsureAnalysis();
  return *scopes_;
}

void Builder::BuildParentMap() {
  parents_.assign(tree_->id_limit(), nullptr);
  Walk(tree_->root(), [&](Node* node) {
    ForEachChild(node, [&](Node* child) {
      if (child->id >= parents_.size()) {
        parents_.resize(child->id + 1, nullptr);
      }
      parents_[child->id] = node;
    });
    return true;
  });
  parents_valid_ = true;
}

Node* Builder::ParentOf(Node* node) {
  if (!parents_valid_) {
    BuildParentMap();
  }
  return node->id < parents_.size() ? parents_[node->id] : nullptr;
}

// --- statement lists ---------------------------------------------------------

// static
int Builder::LeadingDirectives(const std::vector<Statement*>& list) {
  int n = 0;
  for (Statement* s : list) {
    auto* e = s->As<ExpressionStatement>();
    if (e == nullptr || !e->is_directive) {
      break;
    }
    n++;
  }
  return n;
}

template <typename F>
void Builder::ForEachStatementList(F&& f) {
  Walk(tree_->root(), [&](Node* node) {
    switch (node->kind) {
      case NodeKind::kProgram:
        f(&node->Cast<Program>()->body, node);
        break;
      case NodeKind::kBlockStatement:
        f(&node->Cast<BlockStatement>()->body, node);
        break;
      case NodeKind::kSwitchCase:
        f(&node->Cast<SwitchCase>()->consequent, node);
        break;
      case NodeKind::kStaticBlock:
        f(&node->Cast<StaticBlock>()->body, node);
        break;
      default:
        break;
    }
    return true;
  });
}

std::vector<Builder::Slot> Builder::InsertionSlots() {
  std::vector<Slot> slots;
  ForEachStatementList([&](std::vector<Statement*>* list, Node* owner) {
    size_t first = static_cast<size_t>(LeadingDirectives(*list));
    for (size_t i = first; i <= list->size(); i++) {
      slots.push_back({list, i, owner});
    }
  });
  return slots;
}

std::vector<Builder::Slot> Builder::StatementSlots() {
  std::vector<Slot> slots;
  ForEachStatementList([&](std::vector<Statement*>* list, Node* owner) {
    size_t first = static_cast<size_t>(LeadingDirectives(*list));
    for (size_t i = first; i < list->size(); i++) {
      slots.push_back({list, i, owner});
    }
  });
  return slots;
}

Builder::Slot Builder::RandomInsertionSlot() {
  std::vector<Slot> slots = InsertionSlots();
  if (slots.empty()) {
    return Slot{&tree_->root()->body, 0, tree_->root()};
  }
  return rng_->Pick(slots);
}

Builder::Slot Builder::RandomStatementSlot() {
  std::vector<Slot> slots = StatementSlots();
  if (slots.empty()) {
    return Slot{};
  }
  return rng_->Pick(slots);
}

Builder::Slot Builder::SlotOf(Statement* statement) {
  Slot result;
  ForEachStatementList([&](std::vector<Statement*>* list, Node* owner) {
    if (result.valid()) {
      return;
    }
    for (size_t i = 0; i < list->size(); i++) {
      if ((*list)[i] == statement) {
        result = {list, i, owner};
        return;
      }
    }
  });
  return result;
}

Node* Builder::AnchorNodeFor(const Slot& slot, int32_t* position) {
  // Code inserted at `slot` sees what the previous statement sees (or what
  // the owner sees for the first position).
  Node* anchor = slot.index > 0 ? (*slot.list)[slot.index - 1] : slot.owner;
  if (position != nullptr) {
    *position = slot.index > 0 ? anchor->end : anchor->start;
    if (*position < 0) {
      *position = 0;
    }
  }
  return anchor;
}

void Builder::Insert(const Slot& slot, Statement* statement) {
  int32_t position = 0;
  AnchorNodeFor(slot, &position);
  mk::Anchor(statement, position);
  slot.list->insert(slot.list->begin() + static_cast<ptrdiff_t>(slot.index),
                    statement);
  Invalidate();
}

void Builder::InsertAll(const Slot& slot,
                        const std::vector<Statement*>& statements) {
  Slot cursor = slot;
  for (Statement* statement : statements) {
    Insert(cursor, statement);
    cursor.index++;
  }
}

Statement* Builder::Remove(const Slot& slot) {
  if (!slot.valid() || slot.index >= slot.list->size()) {
    return nullptr;
  }
  Statement* removed = (*slot.list)[slot.index];
  slot.list->erase(slot.list->begin() + static_cast<ptrdiff_t>(slot.index));
  Invalidate();
  return removed;
}

bool Builder::Replace(Node* old_node, Node* new_node) {
  Node* parent = ParentOf(old_node);
  if (parent == nullptr) {
    return false;
  }
  bool replaced = false;
  ForEachChildSlot(parent, [&](Node** slot) {
    if (*slot == old_node) {
      *slot = new_node;
      replaced = true;
    }
  });
  if (replaced) {
    if (old_node->start >= 0) {
      mk::Anchor(new_node, old_node->start);
    }
    Invalidate();
  }
  return replaced;
}

Builder::Slot Builder::Blockify(Statement* statement) {
  if (auto* block = statement->As<BlockStatement>()) {
    return Slot{&block->body, 0, block};
  }
  Slot existing = SlotOf(statement);
  if (existing.valid()) {
    return existing;
  }
  BlockStatement* block = mk::Block(tree_, {statement});
  block->start = statement->start;
  block->end = statement->end;
  if (!Replace(statement, block)) {
    return Slot{};
  }
  return Slot{&block->body, 0, block};
}

// --- node queries ------------------------------------------------------------

std::vector<Node*> Builder::Collect(const std::function<bool(Node*)>& pred) {
  std::vector<Node*> result;
  Walk(tree_->root(), [&](Node* node) {
    if (pred(node)) {
      result.push_back(node);
    }
    return true;
  });
  return result;
}

Node* Builder::RandomNode(const std::function<bool(Node*)>& pred) {
  std::vector<Node*> nodes = Collect(pred);
  if (nodes.empty()) {
    return nullptr;
  }
  return rng_->Pick(nodes);
}

namespace {

bool IsPatternKind(NodeKind kind) {
  return kind == NodeKind::kObjectPattern || kind == NodeKind::kArrayPattern ||
         kind == NodeKind::kAssignmentPattern || kind == NodeKind::kRestElement;
}

}  // namespace

ExpressionRole Builder::RoleOf(Node* node) {
  if (!node->IsExpression()) {
    return ExpressionRole::kNotReplaceable;
  }
  Node* parent = ParentOf(node);
  if (parent == nullptr) {
    return ExpressionRole::kNotReplaceable;
  }
  if (node->kind == NodeKind::kSuper ||
      node->kind == NodeKind::kPrivateIdentifier || IsPatternKind(node->kind)) {
    return ExpressionRole::kNotReplaceable;
  }
  // `{...x}`: the entry must stay a spread (or a property); its argument is
  // the replaceable part.
  if (node->kind == NodeKind::kSpreadElement &&
      parent->kind == NodeKind::kObjectExpression) {
    return ExpressionRole::kNotReplaceable;
  }
  switch (parent->kind) {
    case NodeKind::kAssignmentExpression:
      if (parent->Cast<AssignmentExpression>()->left == node) {
        return node->kind == NodeKind::kIdentifier ||
                       node->kind == NodeKind::kMemberExpression
                   ? ExpressionRole::kTarget
                   : ExpressionRole::kNotReplaceable;
      }
      return ExpressionRole::kValue;
    case NodeKind::kUpdateExpression:
      return ExpressionRole::kTarget;
    case NodeKind::kForInStatement:
      if (parent->Cast<ForInStatement>()->left == node) {
        return ExpressionRole::kTarget;
      }
      return ExpressionRole::kValue;
    case NodeKind::kForOfStatement:
      if (parent->Cast<ForOfStatement>()->left == node) {
        return ExpressionRole::kTarget;
      }
      return ExpressionRole::kValue;
    case NodeKind::kMemberExpression: {
      auto* m = parent->Cast<MemberExpression>();
      if (m->property == node && !m->computed) {
        return ExpressionRole::kNotReplaceable;
      }
      if (m->object == node && node->kind == NodeKind::kSuper) {
        return ExpressionRole::kNotReplaceable;
      }
      return ExpressionRole::kValue;
    }
    case NodeKind::kProperty: {
      auto* p = parent->Cast<Property>();
      if (p->key == node && !p->computed) {
        return ExpressionRole::kNotReplaceable;
      }
      if (p->value == node &&
          (p->method || p->property_kind != PropertyKind::kInit)) {
        return ExpressionRole::kNotReplaceable;
      }
      // Patterns: the value is a binding/assignment target.
      Node* grandparent = ParentOf(parent);
      if (grandparent != nullptr &&
          grandparent->kind == NodeKind::kObjectPattern && p->value == node) {
        return ExpressionRole::kNotReplaceable;
      }
      return ExpressionRole::kValue;
    }
    case NodeKind::kMethodDefinition:
    case NodeKind::kPropertyDefinition: {
      Expression* key = parent->kind == NodeKind::kMethodDefinition
                            ? parent->Cast<MethodDefinition>()->key
                            : parent->Cast<PropertyDefinition>()->key;
      bool computed = parent->kind == NodeKind::kMethodDefinition
                          ? parent->Cast<MethodDefinition>()->computed
                          : parent->Cast<PropertyDefinition>()->computed;
      if (key == node && !computed) {
        return ExpressionRole::kNotReplaceable;
      }
      if (parent->kind == NodeKind::kMethodDefinition && key != node) {
        return ExpressionRole::kNotReplaceable;  // the method function
      }
      return ExpressionRole::kValue;
    }
    case NodeKind::kVariableDeclarator:
      if (parent->Cast<VariableDeclarator>()->target == node) {
        return ExpressionRole::kNotReplaceable;
      }
      return ExpressionRole::kValue;
    case NodeKind::kFunctionDeclaration:
    case NodeKind::kFunctionExpression:
    case NodeKind::kArrowFunctionExpression: {
      const FunctionData* fn = AsFunctionData(parent);
      if (fn->name == node) {
        return ExpressionRole::kNotReplaceable;
      }
      for (Expression* param : fn->params) {
        if (param == node) {
          return ExpressionRole::kNotReplaceable;
        }
      }
      return ExpressionRole::kValue;  // concise arrow body
    }
    case NodeKind::kClassDeclaration:
    case NodeKind::kClassExpression:
      if (AsClassData(parent)->name == node) {
        return ExpressionRole::kNotReplaceable;
      }
      return ExpressionRole::kValue;  // heritage
    case NodeKind::kLabeledStatement:
    case NodeKind::kBreakStatement:
    case NodeKind::kContinueStatement:
    case NodeKind::kCatchClause:  // the parameter
      return ExpressionRole::kNotReplaceable;
    case NodeKind::kTaggedTemplateExpression:
      if (parent->Cast<TaggedTemplateExpression>()->quasi == node) {
        return ExpressionRole::kNotReplaceable;
      }
      return ExpressionRole::kValue;
    case NodeKind::kChainExpression:
    case NodeKind::kObjectPattern:
    case NodeKind::kArrayPattern:
    case NodeKind::kRestElement:
      return ExpressionRole::kNotReplaceable;
    case NodeKind::kAssignmentPattern:
      if (parent->Cast<AssignmentPattern>()->left == node) {
        return ExpressionRole::kNotReplaceable;
      }
      return ExpressionRole::kValue;
    case NodeKind::kCallExpression:
      if (parent->Cast<CallExpression>()->callee == node &&
          node->kind == NodeKind::kSuper) {
        return ExpressionRole::kNotReplaceable;
      }
      return ExpressionRole::kValue;
    case NodeKind::kBinaryExpression:
      // `#x in obj` has a PrivateIdentifier on the left (handled above).
    default:
      return ExpressionRole::kValue;
  }
}

std::vector<Expression*> Builder::ValueExpressions() {
  std::vector<Expression*> result;
  Walk(tree_->root(), [&](Node* node) {
    if (node->IsExpression() && RoleOf(node) == ExpressionRole::kValue) {
      result.push_back(node->Cast<Expression>());
    }
    return true;
  });
  return result;
}

Statement* Builder::EnclosingStatement(Node* node) {
  for (Node* n = node; n != nullptr; n = ParentOf(n)) {
    if (n->IsStatement()) {
      return n->Cast<Statement>();
    }
  }
  return nullptr;
}

Node* Builder::EnclosingFunction(Node* node) {
  for (Node* n = ParentOf(node); n != nullptr; n = ParentOf(n)) {
    if (n->IsFunction()) {
      return n;
    }
  }
  return nullptr;
}

// --- context -----------------------------------------------------------------

namespace {

bool IsLoopKind(NodeKind kind) {
  return kind == NodeKind::kForStatement || kind == NodeKind::kWhileStatement ||
         kind == NodeKind::kDoWhileStatement ||
         kind == NodeKind::kForInStatement || kind == NodeKind::kForOfStatement;
}

}  // namespace

Builder::Context Builder::ContextAt(Node* node) {
  Context c;
  // Any function (arrows included) ends the reach of enclosing loops,
  // switches and labels and fixes return/await/yield/strictness.
  bool code_boundary = false;
  // A non-arrow function, field initializer or static block fixes
  // this/super/arguments.
  bool this_boundary = false;
  bool class_resolved = false;
  Node* child = nullptr;
  for (Node* n = node; n != nullptr; child = n, n = ParentOf(n)) {
    switch (n->kind) {
      case NodeKind::kProgram:
        if (!code_boundary) {
          c.strict = n->Cast<Program>()->strict;
        }
        break;
      case NodeKind::kForStatement:
      case NodeKind::kWhileStatement:
      case NodeKind::kDoWhileStatement:
      case NodeKind::kForInStatement:
      case NodeKind::kForOfStatement:
        if (!code_boundary && n != node) {
          c.in_loop = true;
        }
        break;
      case NodeKind::kSwitchStatement:
        if (!code_boundary && n != node) {
          c.in_switch = true;
        }
        break;
      case NodeKind::kLabeledStatement:
        if (!code_boundary && n != node) {
          c.labels.push_back(n->Cast<LabeledStatement>()->label->name);
        }
        break;
      case NodeKind::kFunctionDeclaration:
      case NodeKind::kFunctionExpression:
      case NodeKind::kArrowFunctionExpression: {
        if (n == node) {
          break;
        }
        const FunctionData* fn = AsFunctionData(n);
        // Code in the parameter list or body of the function.
        if (!code_boundary) {
          c.in_function = true;
          c.in_async = fn->is_async;
          c.in_generator = fn->is_generator;
          c.strict = fn->strict;
          code_boundary = true;
        }
        if (!this_boundary && !fn->is_arrow) {
          this_boundary = true;
          c.has_arguments = true;
          Node* parent = ParentOf(n);
          if (parent != nullptr) {
            if (auto* m = parent->As<MethodDefinition>()) {
              if (m->value == n) {
                c.in_method = true;
                c.in_constructor = m->method_kind == MethodKind::kConstructor;
              }
            } else if (auto* p = parent->As<Property>()) {
              if (p->value == n &&
                  (p->method || p->property_kind != PropertyKind::kInit)) {
                c.in_method = true;  // object literal methods have super too
                class_resolved = true;
              }
            }
          }
          if (!c.in_method) {
            class_resolved = true;
          }
        }
        break;
      }
      case NodeKind::kPropertyDefinition:
        if (!code_boundary && child == n->Cast<PropertyDefinition>()->value) {
          c.in_class_field = true;
          c.in_method = true;  // super.x is allowed in field initializers
          c.strict = true;
          c.in_function = false;
          code_boundary = true;
          this_boundary = true;
        }
        break;
      case NodeKind::kStaticBlock:
        if (!code_boundary) {
          c.in_static_block = true;
          c.in_method = true;
          c.strict = true;
          code_boundary = true;
          this_boundary = true;
        }
        break;
      case NodeKind::kClassBody:
        c.strict = true;  // class bodies are strict, nested functions too
        break;
      case NodeKind::kClassDeclaration:
      case NodeKind::kClassExpression:
        c.strict = true;
        if (c.in_method && !class_resolved) {
          c.has_super_class = AsClassData(n)->super_class != nullptr;
          class_resolved = true;
        }
        break;
      default:
        break;
    }
  }
  return c;
}

Builder::Context Builder::ContextAtSlot(const Slot& slot) {
  // A statement list owner (block, case, static block, program) adds no
  // context of its own except: a block that is a loop/switch body is
  // inside that loop/switch, and ContextAt() deliberately ignores the
  // node's own loop markers.
  Context c = ContextAt(slot.owner);
  if (slot.owner->kind == NodeKind::kSwitchCase) {
    c.in_switch = true;
  }
  if (slot.owner->kind == NodeKind::kStaticBlock) {
    c.in_static_block = true;
    c.in_method = true;
    c.strict = true;
    c.in_function = false;
    c.in_loop = false;
    c.in_switch = false;
    c.labels.clear();
  }
  Node* parent = ParentOf(slot.owner);
  if (parent != nullptr && IsLoopKind(parent->kind)) {
    c.in_loop = true;
  }
  return c;
}

// --- variables ---------------------------------------------------------------

// static
JsType Builder::GuessType(const Expression* e) {
  if (e == nullptr) {
    return JsType::kUnknown;
  }
  switch (e->kind) {
    case NodeKind::kNumericLiteral:
      return JsType::kNumber;
    case NodeKind::kStringLiteral:
    case NodeKind::kTemplateLiteral:
      return JsType::kString;
    case NodeKind::kBooleanLiteral:
      return JsType::kBoolean;
    case NodeKind::kBigIntLiteral:
      return JsType::kBigInt;
    case NodeKind::kNullLiteral:
      return JsType::kNullish;
    case NodeKind::kRegExpLiteral:
      return JsType::kRegExp;
    case NodeKind::kArrayExpression:
      return JsType::kArray;
    case NodeKind::kObjectExpression:
      return JsType::kObject;
    case NodeKind::kFunctionExpression:
    case NodeKind::kArrowFunctionExpression:
      return JsType::kFunction;
    case NodeKind::kClassExpression:
      return JsType::kClass;
    case NodeKind::kIdentifier: {
      std::string_view name = e->Cast<Identifier>()->name;
      if (name == "undefined") {
        return JsType::kNullish;
      }
      if (name == "NaN" || name == "Infinity") {
        return JsType::kNumber;
      }
      return JsType::kUnknown;
    }
    case NodeKind::kUnaryExpression: {
      auto* u = e->Cast<UnaryExpression>();
      switch (u->op) {
        case UnaryOp::kNot:
        case UnaryOp::kDelete:
          return JsType::kBoolean;
        case UnaryOp::kTypeof:
          return JsType::kString;
        case UnaryOp::kVoid:
          return JsType::kNullish;
        case UnaryOp::kMinus:
        case UnaryOp::kPlus:
        case UnaryOp::kBitNot:
          return u->op == UnaryOp::kPlus                     ? JsType::kNumber
                 : GuessType(u->argument) == JsType::kBigInt ? JsType::kBigInt
                                                             : JsType::kNumber;
      }
      return JsType::kUnknown;
    }
    case NodeKind::kUpdateExpression:
      return JsType::kNumber;
    case NodeKind::kBinaryExpression: {
      auto* b = e->Cast<BinaryExpression>();
      switch (b->op) {
        case BinaryOp::kEq:
        case BinaryOp::kNotEq:
        case BinaryOp::kStrictEq:
        case BinaryOp::kStrictNotEq:
        case BinaryOp::kLessThan:
        case BinaryOp::kLessThanEq:
        case BinaryOp::kGreaterThan:
        case BinaryOp::kGreaterThanEq:
        case BinaryOp::kIn:
        case BinaryOp::kInstanceof:
          return JsType::kBoolean;
        case BinaryOp::kAdd: {
          JsType l = GuessType(b->left), r = GuessType(b->right);
          if (l == JsType::kString || r == JsType::kString) {
            return JsType::kString;
          }
          if (l == JsType::kNumber && r == JsType::kNumber) {
            return JsType::kNumber;
          }
          if (l == JsType::kBigInt && r == JsType::kBigInt) {
            return JsType::kBigInt;
          }
          return JsType::kUnknown;
        }
        default:
          return GuessType(b->left) == JsType::kBigInt ? JsType::kBigInt
                                                       : JsType::kNumber;
      }
    }
    case NodeKind::kLogicalExpression: {
      auto* l = e->Cast<LogicalExpression>();
      JsType a = GuessType(l->left), b = GuessType(l->right);
      return a == b ? a : JsType::kUnknown;
    }
    case NodeKind::kConditionalExpression: {
      auto* c = e->Cast<ConditionalExpression>();
      JsType a = GuessType(c->consequent), b = GuessType(c->alternate);
      return a == b ? a : JsType::kUnknown;
    }
    case NodeKind::kAssignmentExpression:
      return GuessType(e->Cast<AssignmentExpression>()->right);
    case NodeKind::kSequenceExpression: {
      auto* s = e->Cast<SequenceExpression>();
      return s->expressions.empty() ? JsType::kUnknown
                                    : GuessType(s->expressions.back());
    }
    case NodeKind::kAwaitExpression:
      return JsType::kUnknown;
    case NodeKind::kNewExpression: {
      auto* n = e->Cast<NewExpression>();
      auto* callee = n->callee->As<Identifier>();
      if (callee == nullptr) {
        return JsType::kObject;
      }
      std::string_view name = callee->name;
      if (name == "Array") {
        return JsType::kArray;
      }
      if (name == "Map" || name == "WeakMap") {
        return JsType::kMap;
      }
      if (name == "Set" || name == "WeakSet") {
        return JsType::kSet;
      }
      if (name == "Promise") {
        return JsType::kPromise;
      }
      if (name == "RegExp") {
        return JsType::kRegExp;
      }
      if (name == "Date") {
        return JsType::kDate;
      }
      if (name == "ArrayBuffer" || name == "SharedArrayBuffer") {
        return JsType::kArrayBuffer;
      }
      if (name == "String") {
        return JsType::kObject;
      }
      if (name == "Number") {
        return JsType::kObject;
      }
      if (name.size() > 5 && name.ends_with("Array")) {
        return JsType::kTypedArray;  // Uint8Array, Float64Array, ...
      }
      if (name.size() > 5 && name.ends_with("Error")) {
        return JsType::kError;
      }
      return JsType::kObject;
    }
    case NodeKind::kCallExpression: {
      auto* c = e->Cast<CallExpression>();
      if (auto* callee = c->callee->As<Identifier>()) {
        std::string_view name = callee->name;
        if (name == "String") {
          return JsType::kString;
        }
        if (name == "Number" || name == "parseInt" || name == "parseFloat") {
          return JsType::kNumber;
        }
        if (name == "Boolean") {
          return JsType::kBoolean;
        }
        if (name == "BigInt") {
          return JsType::kBigInt;
        }
        if (name == "Symbol") {
          return JsType::kSymbol;
        }
        if (name == "Array") {
          return JsType::kArray;
        }
        if (name == "Object") {
          return JsType::kObject;
        }
        return JsType::kUnknown;
      }
      if (auto* member = c->callee->As<MemberExpression>()) {
        auto* object = member->object->As<Identifier>();
        auto* property = member->property->As<Identifier>();
        if (object == nullptr || property == nullptr || member->computed) {
          return JsType::kUnknown;
        }
        std::string_view o = object->name, p = property->name;
        if (o == "Math") {
          return JsType::kNumber;
        }
        if (o == "JSON") {
          return p == "stringify" ? JsType::kString : JsType::kUnknown;
        }
        if (o == "Object") {
          if (p == "keys" || p == "values" || p == "entries" ||
              p == "getOwnPropertyNames") {
            return JsType::kArray;
          }
          return JsType::kObject;
        }
        if (o == "Array") {
          return JsType::kArray;
        }
        if (o == "String") {
          return JsType::kString;
        }
        if (o == "Number") {
          return JsType::kNumber;
        }
        if (o == "Reflect" && p == "ownKeys") {
          return JsType::kArray;
        }
        if (o == "Promise") {
          return JsType::kPromise;
        }
        return JsType::kUnknown;
      }
      return JsType::kUnknown;
    }
    default:
      return JsType::kUnknown;
  }
}

JsType Builder::GuessType(const Binding* binding) {
  switch (binding->kind) {
    case BindingKind::kFunction:
    case BindingKind::kFunctionName:
      return JsType::kFunction;
    case BindingKind::kClass:
    case BindingKind::kClassName:
      return JsType::kClass;
    case BindingKind::kCatchParameter:
      return JsType::kError;
    case BindingKind::kImplicit:
      return JsType::kObject;  // arguments
    case BindingKind::kParameter:
      return JsType::kUnknown;
    default:
      break;
  }
  if (binding->declaration == nullptr) {
    return JsType::kUnknown;
  }
  if (auto* declarator = binding->declaration->As<VariableDeclarator>()) {
    if (declarator->target != binding->identifier) {
      return JsType::kUnknown;
    }
    return GuessType(declarator->init);
  }
  return JsType::kUnknown;
}

Variable Builder::ToVariable(Binding* binding) {
  Variable v;
  v.name = binding->name;
  v.binding = binding;
  v.type = GuessType(binding);
  v.is_constant = binding->is_constant();
  return v;
}

std::vector<Variable> Builder::VisibleAt(Node* node) {
  std::vector<Variable> result;
  for (Binding* binding : scopes().VisibleAt(node, /*initialized_only=*/true)) {
    result.push_back(ToVariable(binding));
  }
  return result;
}

std::vector<Variable> Builder::VisibleAtSlot(const Slot& slot) {
  // The analysis renumbers positions; compute the slot position afterwards.
  ScopeAnalysis& analysis = scopes();
  int32_t position = 0;
  AnchorNodeFor(slot, &position);
  std::vector<Variable> result;
  for (Binding* binding :
       analysis.VisibleIn(slot.owner, position, /*initialized_only=*/true)) {
    result.push_back(ToVariable(binding));
  }
  return result;
}

std::vector<Binding*> Builder::EscapingBindings(const std::vector<Node*>& nodes,
                                                bool block_scoped_only) {
  ScopeAnalysis& analysis = scopes();  // renumbers positions first
  auto inside = [&](const Node* n) {
    for (const Node* root : nodes) {
      if (root->start <= n->start && n->end <= root->end) {
        return true;
      }
    }
    return false;
  };
  std::vector<Binding*> result;
  absl::flat_hash_set<Binding*> seen;
  for (Node* root : nodes) {
    Walk(root, [&](Node* n) {
      auto* id = n->As<Identifier>();
      if (id == nullptr || !analysis.IsDeclaration(id)) {
        return true;
      }
      Binding* binding = analysis.BindingOf(id);
      if (binding == nullptr || !seen.insert(binding).second) {
        return true;
      }
      if (block_scoped_only && binding->kind == BindingKind::kVar) {
        return true;
      }
      // Also declared elsewhere (`var x; ... var x;`): nothing is lost.
      for (Identifier* declaration : binding->declarations) {
        if (!inside(declaration)) {
          return true;
        }
      }
      for (Identifier* reference : binding->references) {
        if (!inside(reference)) {
          result.push_back(binding);
          break;
        }
      }
      return true;
    });
  }
  return result;
}

// static
std::vector<Variable> Builder::OfType(const std::vector<Variable>& candidates,
                                      JsType type) {
  std::vector<Variable> result;
  for (const Variable& v : candidates) {
    if (v.type == type) {
      result.push_back(v);
    }
  }
  return result;
}

// --- names -------------------------------------------------------------------

void Builder::EnsureUsedNames() {
  if (used_names_valid_) {
    return;
  }
  // The analysis' names are views into tree_'s arena, which outlives the
  // analysis: no copy needed.
  const absl::flat_hash_set<std::string_view>& names = scopes().all_names();
  used_names_.insert(names.begin(), names.end());
  used_names_valid_ = true;
}

std::string_view Builder::FreshName(std::string_view prefix) {
  EnsureUsedNames();
  std::string key(prefix);
  int& counter = name_counters_[key];
  for (;;) {
    std::string candidate = key + std::to_string(counter++);
    if (!used_names_.contains(candidate)) {
      std::string_view interned = tree_->Intern(candidate);
      used_names_.insert(interned);
      return interned;
    }
  }
}

void Builder::ReserveNames(const absl::flat_hash_set<std::string_view>& names) {
  EnsureUsedNames();
  for (std::string_view name : names) {
    if (!used_names_.contains(name)) {
      used_names_.insert(tree_->Intern(name));
    }
  }
}

}  // namespace redline

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/builder.h"

#include <algorithm>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/core/known_keys.h"

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
    case JsType::kDataView:
      return "dataview";
    case JsType::kSharedArrayBuffer:
      return "sharedarraybuffer";
    case JsType::kWeakMap:
      return "weakmap";
    case JsType::kWeakSet:
      return "weakset";
    case JsType::kWeakRef:
      return "weakref";
    case JsType::kFinalizationRegistry:
      return "finalizationregistry";
    case JsType::kIterator:
      return "iterator";
    case JsType::kDisposableStack:
      return "disposablestack";
    case JsType::kAsyncDisposableStack:
      return "asyncdisposablestack";
    case JsType::kTemporalInstant:
      return "temporal.instant";
    case JsType::kTemporalDuration:
      return "temporal.duration";
    case JsType::kTemporalPlainDate:
      return "temporal.plaindate";
    case JsType::kTemporalPlainTime:
      return "temporal.plaintime";
    case JsType::kTemporalPlainDateTime:
      return "temporal.plaindatetime";
    case JsType::kTemporalPlainYearMonth:
      return "temporal.plainyearmonth";
    case JsType::kTemporalPlainMonthDay:
      return "temporal.plainmonthday";
    case JsType::kTemporalZonedDateTime:
      return "temporal.zoneddatetime";
    case JsType::kIntlLocale:
      return "intl.locale";
  }
  return "?";
}

Builder::Builder(Tree* tree, Random* rng, const Environment* env)
    : tree_(tree), rng_(rng), env_(env) {}
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
  if (node == nullptr) {
    return nullptr;
  }
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

Node* Builder::ResolveFocusRoot() {
  if (focus_root_ == nullptr) {
    return tree_->root();
  }
  Node* found = nullptr;
  Walk(tree_->root(), [&](Node* node) {
    if (found != nullptr) {
      return false;
    }
    if (node == focus_root_) {
      found = node;
      return false;
    }
    return true;
  });
  return found;
}

template <typename F>
void Builder::ForEachStatementList(Node* start, F&& f) {
  Walk(start, [&](Node* node) {
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
  ForEachStatementList(
      ResolveFocusRoot(), [&](std::vector<Statement*>* list, Node* owner) {
        size_t first = static_cast<size_t>(LeadingDirectives(*list));
        for (size_t i = first; i <= list->size(); i++) {
          slots.push_back({list, i, owner});
        }
      });
  return slots;
}

std::vector<Builder::Slot> Builder::StatementSlots() {
  std::vector<Slot> slots;
  ForEachStatementList(
      ResolveFocusRoot(), [&](std::vector<Statement*>* list, Node* owner) {
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
    if (focus_root_ != nullptr) {
      return Slot{};
    }
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
  ForEachStatementList(tree_->root(),
                       [&](std::vector<Statement*>* list, Node* owner) {
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

// --- member lists ------------------------------------------------------------

std::vector<Builder::MemberSlot> Builder::MemberSlots() {
  std::vector<MemberSlot> slots;
  Walk(ResolveFocusRoot(), [&](Node* node) {
    size_t size = 0;
    if (auto* body = node->As<ClassBody>()) {
      size = body->members.size();
    } else if (auto* object = node->As<ObjectExpression>()) {
      size = object->properties.size();
    } else {
      return true;
    }
    for (size_t i = 0; i <= size; i++) {
      slots.push_back({node, i});
    }
    return true;
  });
  return slots;
}

void Builder::InsertMember(const MemberSlot& slot, Node* member) {
  std::vector<Node*>* list =
      slot.owner->Is<ClassBody>()
          ? &slot.owner->Cast<ClassBody>()->members
          : &slot.owner->Cast<ObjectExpression>()->properties;
  // The new member sees what its predecessor sees.
  int32_t position =
      slot.index > 0 ? (*list)[slot.index - 1]->end : slot.owner->start;
  mk::Anchor(member, position < 0 ? 0 : position);
  list->insert(list->begin() + static_cast<ptrdiff_t>(slot.index), member);
  Invalidate();
}

// --- node queries ------------------------------------------------------------

std::vector<Node*> Builder::Collect(const std::function<bool(Node*)>& pred) {
  std::vector<Node*> result;
  Walk(ResolveFocusRoot(), [&](Node* node) {
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
  Node* start = ResolveFocusRoot();
  std::vector<Expression*> result;
  Walk(start, [&](Node* node) {
    if (focus_root_ != nullptr && node == start && start->IsFunction()) {
      return true;
    }
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

// Adds the private names that `body` declares to `names`, unless an inner
// class declares the same name. A getter and a setter of one name make one
// entry that code can read and write. `outer` marks the names of a class
// that is not the innermost one.
void AddPrivateNames(const ClassBody* body, bool outer,
                     std::vector<PrivateName>* names) {
  const size_t inner = names->size();
  for (const Node* member : body->members) {
    const Expression* key = nullptr;
    PrivateName entry;
    entry.outer = outer;
    if (const auto* field = member->As<PropertyDefinition>()) {
      key = field->key;
      entry.is_static = field->is_static;
    } else if (const auto* method = member->As<MethodDefinition>()) {
      key = method->key;
      entry.is_static = method->is_static;
      entry.readable = method->method_kind != MethodKind::kSet;
      entry.writable = method->method_kind == MethodKind::kSet;
      entry.callable = method->method_kind == MethodKind::kMethod;
    }
    const auto* id = key != nullptr ? key->As<PrivateIdentifier>() : nullptr;
    if (id == nullptr) {
      continue;
    }
    entry.name = id->name;
    auto same = std::find_if(
        names->begin(), names->end(),
        [&](const PrivateName& other) { return other.name == entry.name; });
    if (same == names->end()) {
      names->push_back(entry);
    } else if (static_cast<size_t>(same - names->begin()) >= inner) {
      // The other accessor of a pair.
      same->readable = same->readable || entry.readable;
      same->writable = same->writable || entry.writable;
    }
  }
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
  bool inner_class_seen = false;
  Node* child = nullptr;
  for (Node* n = node; n != nullptr; child = n, n = ParentOf(n)) {
    switch (n->kind) {
      case NodeKind::kProgram:
        if (!code_boundary) {
          c.strict = c.strict || n->Cast<Program>()->strict;
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
          auto* labeled = n->Cast<LabeledStatement>();
          c.labels.push_back(labeled->label->name);
          if (labeled->body != nullptr && labeled->body == child &&
              IsLoopKind(labeled->body->kind)) {
            c.loop_labels.push_back(labeled->label->name);
          }
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
          c.strict = c.strict || fn->strict;
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
                c.static_member = m->is_static;
                c.class_this = true;
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
          c.static_member = n->Cast<PropertyDefinition>()->is_static;
          c.class_this = true;
          code_boundary = true;
          this_boundary = true;
        }
        break;
      case NodeKind::kStaticBlock:
        if (!code_boundary) {
          c.in_static_block = true;
          c.in_method = true;
          c.strict = true;
          c.static_member = true;
          c.class_this = true;
          code_boundary = true;
          this_boundary = true;
        }
        break;
      case NodeKind::kClassBody:
        c.strict = true;  // class bodies are strict, nested functions too
        AddPrivateNames(n->Cast<ClassBody>(), inner_class_seen,
                        &c.private_names);
        inner_class_seen = true;
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
    c.static_member = true;
    c.class_this = true;
    c.in_function = false;
    c.in_loop = false;
    c.in_switch = false;
    c.labels.clear();
    c.loop_labels.clear();
  }
  Node* parent = ParentOf(slot.owner);
  if (parent != nullptr && IsLoopKind(parent->kind)) {
    c.in_loop = true;
  }
  return c;
}

// --- variables ---------------------------------------------------------------

namespace {

// The type of the instances of a constructor of the typed model that is a
// property of a namespace (`Temporal.PlainDate`, `Intl.Locale`), or kUnknown.
JsType NamespacedConstructorType(std::string_view ns, std::string_view name) {
  if (ns == "Intl") {
    return name == "Locale" ? JsType::kIntlLocale : JsType::kUnknown;
  }
  if (ns != "Temporal") {
    return JsType::kUnknown;
  }
  static const struct {
    std::string_view name;
    JsType type;
  } kTemporal[] = {{"Instant", JsType::kTemporalInstant},
                   {"Duration", JsType::kTemporalDuration},
                   {"PlainDate", JsType::kTemporalPlainDate},
                   {"PlainTime", JsType::kTemporalPlainTime},
                   {"PlainDateTime", JsType::kTemporalPlainDateTime},
                   {"PlainYearMonth", JsType::kTemporalPlainYearMonth},
                   {"PlainMonthDay", JsType::kTemporalPlainMonthDay},
                   {"ZonedDateTime", JsType::kTemporalZonedDateTime}};
  for (const auto& entry : kTemporal) {
    if (entry.name == name) {
      return entry.type;
    }
  }
  return JsType::kUnknown;
}

// `Ns.Name` as two identifiers, or false.
bool NamespacePath(const Expression* e, std::string_view* ns,
                   std::string_view* name) {
  if (e == nullptr) {
    return false;
  }
  const auto* member = e->As<MemberExpression>();
  if (member == nullptr || member->computed || member->object == nullptr ||
      member->property == nullptr) {
    return false;
  }
  const auto* object = member->object->As<Identifier>();
  const auto* property = member->property->As<Identifier>();
  if (object == nullptr || property == nullptr) {
    return false;
  }
  *ns = object->name;
  *name = property->name;
  return true;
}

bool IsGlobalIdentifier(Builder* builder, const Identifier* id) {
  return id != nullptr &&
         (builder == nullptr || builder->scopes().BindingOf(id) == nullptr);
}

// Maximum recursion depth for local type propagation.
constexpr int kMaxTypeDepth = 8;
// Maximum depth for scanning assignment writes to an uninitialized binding.
constexpr int kMaxAssignmentDepth = 6;
// Maximum references scanned to infer a binding's type from assignments.
constexpr size_t kMaxAssignmentScans = 8;
// Maximum top-level statements scanned in a callee body for return types.
constexpr size_t kMaxReturnScans = 16;

// Type of a known global identifier, or `kUnknown`.
JsType GuessGlobalType(std::string_view name) {
  if (name == "undefined") {
    return JsType::kNullish;
  }
  if (name == "NaN" || name == "Infinity") {
    return JsType::kNumber;
  }
  if (name == "Math" || name == "JSON" || name == "Reflect" ||
      name == "Atomics" || name == "globalThis" || name == "Intl" ||
      name == "Temporal" || name == "console") {
    return JsType::kObject;
  }
  if (name == "Object" || name == "Array" || name == "Function" ||
      name == "Map" || name == "Set" || name == "WeakMap" ||
      name == "WeakSet" || name == "WeakRef" ||
      name == "FinalizationRegistry" || name == "Promise" || name == "RegExp" ||
      name == "Date" || name == "Error" || name == "TypeError" ||
      name == "RangeError" || name == "ReferenceError" ||
      name == "SyntaxError" || name == "URIError" || name == "EvalError" ||
      name == "AggregateError" || name == "Symbol" || name == "BigInt" ||
      name == "Number" || name == "String" || name == "Boolean" ||
      name == "Proxy" || name == "ArrayBuffer" || name == "SharedArrayBuffer" ||
      name == "DataView" || name == "DisposableStack" ||
      name == "AsyncDisposableStack" || name == "Iterator" ||
      name == "parseInt" || name == "parseFloat" || name == "isNaN" ||
      name == "isFinite" || (name.size() > 5 && name.ends_with("Array"))) {
    return JsType::kFunction;
  }
  return JsType::kUnknown;
}

// Common prototype method return types for non-computed calls `obj.method()`.
JsType GuessMethodReturnType(std::string_view method) {
  if (method == "keys" || method == "values" || method == "entries" ||
      method == "matchAll") {
    // Arrays, typed arrays, maps, sets and strings return iterators.
    return JsType::kIterator;
  }
  if (method == "toString" || method == "toLocaleString" ||
      method == "toFixed" || method == "toExponential" ||
      method == "toPrecision" || method == "trim" || method == "trimStart" ||
      method == "trimEnd" || method == "toLowerCase" ||
      method == "toUpperCase" || method == "charAt" || method == "padStart" ||
      method == "padEnd" || method == "repeat" || method == "substring" ||
      method == "substr" || method == "replace" || method == "replaceAll" ||
      method == "join" || method == "normalize") {
    return JsType::kString;
  }
  if (method == "includes" || method == "startsWith" || method == "endsWith" ||
      method == "has" || method == "delete" || method == "test" ||
      method == "some" || method == "every" || method == "hasOwnProperty" ||
      method == "isPrototypeOf" || method == "propertyIsEnumerable") {
    return JsType::kBoolean;
  }
  if (method == "indexOf" || method == "lastIndexOf" ||
      method == "charCodeAt" || method == "codePointAt" ||
      method == "localeCompare" || method == "search" ||
      method == "findIndex" || method == "findLastIndex" || method == "push" ||
      method == "unshift") {
    return JsType::kNumber;
  }
  if (method == "split" || method == "map" || method == "filter" ||
      method == "flat" || method == "flatMap" || method == "toReversed" ||
      method == "toSorted" || method == "toSpliced" || method == "toArray") {
    return JsType::kArray;
  }
  if (method == "bind") {
    return JsType::kFunction;
  }
  if (method == "then" || method == "catch" || method == "finally") {
    return JsType::kPromise;
  }
  return JsType::kUnknown;
}

// Returns the `FunctionData` of `binding`, or nullptr.
const FunctionData* DeclaredFunction(const Binding* binding) {
  if (binding == nullptr || binding->declaration == nullptr) {
    return nullptr;
  }
  const Node* decl = binding->declaration;
  if (auto* declarator = decl->As<VariableDeclarator>()) {
    if (declarator->target != binding->identifier ||
        declarator->init == nullptr) {
      return nullptr;
    }
    decl = declarator->init;
  }
  return AsFunctionData(decl);
}

}  // namespace

// static
JsType Builder::GuessFunctionReturnType(Builder* builder,
                                        const Environment* env,
                                        const FunctionData* fn, int depth) {
  if (fn == nullptr || depth > kMaxTypeDepth) {
    return JsType::kUnknown;
  }
  if (fn->is_async && !fn->is_generator) {
    return JsType::kPromise;
  }
  if (fn->is_generator) {
    return fn->is_async ? JsType::kUnknown : JsType::kIterator;
  }
  if (fn->expression_body != nullptr) {
    return GuessExpression(builder, env, fn->expression_body, depth + 1);
  }
  if (fn->body == nullptr || depth > 2) {
    return JsType::kUnknown;
  }
  JsType inferred = JsType::kUnknown;
  size_t count = std::min(fn->body->body.size(), kMaxReturnScans);
  for (size_t i = 0; i < count; i++) {
    const auto* ret = fn->body->body[i]->As<ReturnStatement>();
    if (ret == nullptr) {
      continue;
    }
    JsType t = ret->argument != nullptr
                   ? GuessExpression(builder, env, ret->argument, depth + 1)
                   : JsType::kNullish;
    if (t == JsType::kUnknown) {
      return JsType::kUnknown;
    }
    if (inferred == JsType::kUnknown) {
      inferred = t;
    } else if (inferred != t) {
      return JsType::kUnknown;
    }
  }
  return inferred;
}

// static
JsType Builder::GuessExpression(Builder* builder, const Environment* env,
                                const Expression* e, int depth) {
  if (e == nullptr || depth > kMaxTypeDepth) {
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
      const auto* id = e->Cast<Identifier>();
      if (builder != nullptr) {
        if (const Binding* binding = builder->scopes().BindingOf(id)) {
          return builder->GuessBindingType(binding, depth + 1);
        }
      }
      return GuessGlobalType(id->name);
    }
    case NodeKind::kChainExpression:
      return GuessExpression(builder, env,
                             e->Cast<ChainExpression>()->expression, depth + 1);
    case NodeKind::kMemberExpression: {
      const auto* m = e->Cast<MemberExpression>();
      if (m->computed || m->object == nullptr || m->property == nullptr) {
        return JsType::kUnknown;
      }
      const auto* prop = m->property->As<Identifier>();
      if (prop == nullptr) {
        return JsType::kUnknown;
      }
      if (env != nullptr) {
        if (const auto* obj_id = m->object->As<Identifier>();
            IsGlobalIdentifier(builder, obj_id)) {
          JsType static_type =
              env->LookupNamespacePropertyType(obj_id->name, prop->name);
          if (static_type != JsType::kUnknown) {
            return static_type;
          }
        }
        JsType recv = GuessExpression(builder, env, m->object, depth + 1);
        if (recv != JsType::kUnknown) {
          JsType prop_type = env->LookupPropertyType(recv, prop->name);
          if (prop_type != JsType::kUnknown) {
            return prop_type;
          }
        }
      }
      if (prop->name == "length" || prop->name == "byteLength" ||
          prop->name == "byteOffset") {
        return JsType::kNumber;
      }
      return JsType::kUnknown;
    }
    case NodeKind::kUnaryExpression: {
      const auto* u = e->Cast<UnaryExpression>();
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
          return u->op == UnaryOp::kPlus
                     ? JsType::kNumber
                     : (GuessExpression(builder, env, u->argument, depth + 1) ==
                                JsType::kBigInt
                            ? JsType::kBigInt
                            : JsType::kNumber);
      }
      return JsType::kUnknown;
    }
    case NodeKind::kUpdateExpression:
      return JsType::kNumber;
    case NodeKind::kBinaryExpression: {
      const auto* b = e->Cast<BinaryExpression>();
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
          JsType l = GuessExpression(builder, env, b->left, depth + 1);
          JsType r = GuessExpression(builder, env, b->right, depth + 1);
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
          return GuessExpression(builder, env, b->left, depth + 1) ==
                         JsType::kBigInt
                     ? JsType::kBigInt
                     : JsType::kNumber;
      }
    }
    case NodeKind::kLogicalExpression: {
      const auto* l = e->Cast<LogicalExpression>();
      JsType a = GuessExpression(builder, env, l->left, depth + 1);
      JsType b = GuessExpression(builder, env, l->right, depth + 1);
      return a == b ? a : JsType::kUnknown;
    }
    case NodeKind::kConditionalExpression: {
      const auto* c = e->Cast<ConditionalExpression>();
      JsType a = GuessExpression(builder, env, c->consequent, depth + 1);
      JsType b = GuessExpression(builder, env, c->alternate, depth + 1);
      return a == b ? a : JsType::kUnknown;
    }
    case NodeKind::kAssignmentExpression:
      return GuessExpression(builder, env,
                             e->Cast<AssignmentExpression>()->right, depth + 1);
    case NodeKind::kSequenceExpression: {
      const auto* s = e->Cast<SequenceExpression>();
      return s->expressions.empty()
                 ? JsType::kUnknown
                 : GuessExpression(builder, env, s->expressions.back(),
                                   depth + 1);
    }
    case NodeKind::kAwaitExpression:
      return JsType::kUnknown;
    case NodeKind::kNewExpression: {
      const auto* n = e->Cast<NewExpression>();
      if (n->callee == nullptr) {
        return JsType::kObject;
      }
      const auto* callee = n->callee->As<Identifier>();
      if (callee == nullptr) {
        std::string_view ns, member;
        if (NamespacePath(n->callee, &ns, &member)) {
          const auto* ns_id =
              n->callee->Cast<MemberExpression>()->object->As<Identifier>();
          if (IsGlobalIdentifier(builder, ns_id)) {
            JsType type = env != nullptr
                              ? env->LookupNamespaceCtorType(ns, member)
                              : NamespacedConstructorType(ns, member);
            if (type != JsType::kUnknown) {
              return type;
            }
          }
        }
        return JsType::kObject;
      }
      if (builder != nullptr &&
          builder->scopes().BindingOf(callee) != nullptr) {
        return JsType::kObject;
      }
      if (env != nullptr) {
        JsType type = env->LookupGlobalCtorType(callee->name);
        return type != JsType::kUnknown ? type : JsType::kObject;
      }
      std::string_view name = callee->name;
      if (name == "Array") {
        return JsType::kArray;
      }
      if (name == "Function") {
        return JsType::kFunction;
      }
      if (name == "Map") {
        return JsType::kMap;
      }
      if (name == "Set") {
        return JsType::kSet;
      }
      if (name == "WeakMap") {
        return JsType::kWeakMap;
      }
      if (name == "WeakSet") {
        return JsType::kWeakSet;
      }
      if (name == "WeakRef") {
        return JsType::kWeakRef;
      }
      if (name == "FinalizationRegistry") {
        return JsType::kFinalizationRegistry;
      }
      if (name == "DisposableStack") {
        return JsType::kDisposableStack;
      }
      if (name == "AsyncDisposableStack") {
        return JsType::kAsyncDisposableStack;
      }
      if (name == "DataView") {
        return JsType::kDataView;
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
      if (name == "ArrayBuffer") {
        return JsType::kArrayBuffer;
      }
      if (name == "SharedArrayBuffer") {
        return JsType::kSharedArrayBuffer;
      }
      if (name == "String" || name == "Number") {
        return JsType::kObject;
      }
      if (name.size() > 5 && name.ends_with("Array")) {
        return JsType::kTypedArray;  // Uint8Array, Float64Array, ...
      }
      if (name == "Error" || (name.size() > 5 && name.ends_with("Error"))) {
        return JsType::kError;
      }
      return JsType::kObject;
    }
    case NodeKind::kCallExpression: {
      const auto* c = e->Cast<CallExpression>();
      if (c->callee == nullptr) {
        return JsType::kUnknown;
      }
      if (const auto* callee = c->callee->As<Identifier>()) {
        if (builder != nullptr) {
          if (const Binding* callee_binding =
                  builder->scopes().BindingOf(callee)) {
            return GuessFunctionReturnType(
                builder, env, DeclaredFunction(callee_binding), depth + 1);
          }
        }
        if (env != nullptr) {
          return env->LookupGlobalCallReturnType(callee->name);
        }
        std::string_view name = callee->name;
        if (name == "String") {
          return JsType::kString;
        }
        if (name == "Number" || name == "parseInt" || name == "parseFloat") {
          return JsType::kNumber;
        }
        if (name == "Boolean" || name == "isNaN" || name == "isFinite") {
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
        if (name == "RegExp") {
          return JsType::kRegExp;
        }
        if (name == "Function") {
          return JsType::kFunction;
        }
        if (name == "Error" || (name.size() > 5 && name.ends_with("Error"))) {
          return JsType::kError;
        }
        return JsType::kUnknown;
      }
      if (const auto* member = c->callee->As<MemberExpression>()) {
        if (member->computed || member->object == nullptr ||
            member->property == nullptr) {
          return JsType::kUnknown;
        }
        const auto* object = member->object->As<Identifier>();
        const auto* property = member->property->As<Identifier>();
        if (property == nullptr) {
          return JsType::kUnknown;
        }
        if (env != nullptr) {
          if (IsGlobalIdentifier(builder, object)) {
            JsType ns_ret = env->LookupNamespaceCallReturnType(object->name,
                                                               property->name);
            if (ns_ret != JsType::kUnknown) {
              return ns_ret;
            }
          }
          std::string_view ns, name;
          if (NamespacePath(member->object, &ns, &name)) {
            const auto* root_id = member->object->Cast<MemberExpression>()
                                      ->object->As<Identifier>();
            if (IsGlobalIdentifier(builder, root_id)) {
              std::string full_ns(ns);
              full_ns.push_back('.');
              full_ns.append(name);
              JsType ns_ret =
                  env->LookupNamespaceCallReturnType(full_ns, property->name);
              if (ns_ret != JsType::kUnknown) {
                return ns_ret;
              }
            }
          }
          JsType recv =
              GuessExpression(builder, env, member->object, depth + 1);
          if (recv != JsType::kUnknown) {
            JsType ret = env->LookupMethodReturnType(recv, property->name);
            if (ret != JsType::kUnknown) {
              return ret;
            }
          }
          if (recv == JsType::kUnknown) {
            return GuessMethodReturnType(property->name);
          }
          return JsType::kUnknown;
        }
        if (object == nullptr) {
          // Static factories of namespaced types: Temporal.PlainDate.from(..),
          // Temporal.Now.instant().
          std::string_view ns, name;
          if (NamespacePath(member->object, &ns, &name)) {
            if (ns == "Temporal" && name == "Now") {
              std::string_view p = property->name;
              return p == "instant"            ? JsType::kTemporalInstant
                     : p == "plainDateISO"     ? JsType::kTemporalPlainDate
                     : p == "plainTimeISO"     ? JsType::kTemporalPlainTime
                     : p == "plainDateTimeISO" ? JsType::kTemporalPlainDateTime
                     : p == "zonedDateTimeISO" ? JsType::kTemporalZonedDateTime
                     : p == "timeZoneId"       ? JsType::kString
                                               : JsType::kUnknown;
            }
            if (property->name != "compare") {
              JsType type = NamespacedConstructorType(ns, name);
              if (type != JsType::kUnknown) {
                return type;
              }
            }
          }
          return GuessMethodReturnType(property->name);
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
              p == "getOwnPropertyNames" || p == "getOwnPropertySymbols") {
            return JsType::kArray;
          }
          if (p == "is" || p == "hasOwn" || p == "isFrozen" ||
              p == "isSealed" || p == "isExtensible") {
            return JsType::kBoolean;
          }
          return JsType::kObject;
        }
        if (o == "Array") {
          return p == "isArray" ? JsType::kBoolean : JsType::kArray;
        }
        if (o == "String") {
          return JsType::kString;
        }
        if (o == "Number") {
          if (p == "isNaN" || p == "isFinite" || p == "isInteger" ||
              p == "isSafeInteger") {
            return JsType::kBoolean;
          }
          return JsType::kNumber;
        }
        if (o == "Reflect") {
          if (p == "ownKeys") {
            return JsType::kArray;
          }
          if (p == "has" || p == "set" || p == "deleteProperty" ||
              p == "defineProperty" || p == "setPrototypeOf" ||
              p == "isExtensible" || p == "preventExtensions") {
            return JsType::kBoolean;
          }
          return JsType::kUnknown;
        }
        if (o == "Promise") {
          return JsType::kPromise;
        }
        if (o == "Iterator" && p != "prototype") {
          return JsType::kIterator;  // Iterator.from, concat, zip, zipKeyed
        }
        return GuessMethodReturnType(p);
      }
      return JsType::kUnknown;
    }
    default:
      return JsType::kUnknown;
  }
}

// static
JsType Builder::GuessType(const Expression* e, const Environment* env) {
  return GuessExpression(nullptr, env, e, 0);
}

JsType Builder::GuessExpressionType(const Expression* e) {
  return GuessExpression(this, env_, e, 0);
}

JsType Builder::GuessBindingType(const Binding* binding, int depth) {
  if (binding == nullptr || depth > kMaxTypeDepth) {
    return JsType::kUnknown;
  }
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
    case BindingKind::kParameter: {
      if (binding->identifier != nullptr) {
        Node* id_parent = ParentOf(binding->identifier);
        if (id_parent != nullptr) {
          if (id_parent->Is<RestElement>()) {
            Node* container = ParentOf(id_parent);
            return container != nullptr && container->Is<ObjectPattern>()
                       ? JsType::kObject
                       : JsType::kArray;
          }
          if (auto* ap = id_parent->As<AssignmentPattern>();
              ap != nullptr && ap->left == binding->identifier) {
            JsType t = GuessExpression(this, env_, ap->right, depth + 1);
            if (t != JsType::kUnknown) {
              return t;
            }
          }
        }
      }
      break;
    }
    default:
      break;
  }
  if (binding->declaration != nullptr) {
    if (auto* declarator = binding->declaration->As<VariableDeclarator>()) {
      if (declarator->target == binding->identifier) {
        if (declarator->init != nullptr) {
          JsType t = GuessExpression(this, env_, declarator->init, depth + 1);
          if (t != JsType::kUnknown) {
            return t;
          }
        } else {
          Node* decl = ParentOf(declarator);
          Node* loop = decl != nullptr ? ParentOf(decl) : nullptr;
          if (auto* for_in =
                  loop != nullptr ? loop->As<ForInStatement>() : nullptr;
              for_in != nullptr && for_in->left == decl) {
            return JsType::kString;
          }
        }
      } else if (binding->identifier != nullptr) {
        Node* id_parent = ParentOf(binding->identifier);
        if (id_parent != nullptr) {
          if (id_parent->Is<RestElement>()) {
            Node* container = ParentOf(id_parent);
            if (container != nullptr && container->Is<ArrayPattern>()) {
              return JsType::kArray;
            }
            if (container != nullptr && container->Is<ObjectPattern>()) {
              return JsType::kObject;
            }
          } else if (auto* ap = id_parent->As<AssignmentPattern>();
                     ap != nullptr && ap->left == binding->identifier) {
            JsType t = GuessExpression(this, env_, ap->right, depth + 1);
            if (t != JsType::kUnknown) {
              return t;
            }
          }
        }
      }
    }
  }
  // Infer from local writes when the declaration gives no type, for example
  // guarded declarations (`let x` then `try { x = init } catch {}`).
  if (!binding->assigned || depth > kMaxAssignmentDepth) {
    return JsType::kUnknown;
  }
  JsType inferred = JsType::kUnknown;
  size_t checked = 0;
  for (Identifier* ref : binding->references) {
    if (++checked > kMaxAssignmentScans) {
      break;
    }
    Node* parent = ParentOf(ref);
    if (parent == nullptr) {
      continue;
    }
    JsType write_type = JsType::kUnknown;
    if (auto* assign = parent->As<AssignmentExpression>();
        assign != nullptr && assign->left == ref) {
      if (assign->op == AssignOp::kAssign) {
        write_type = GuessExpression(this, env_, assign->right, depth + 1);
      } else if (assign->op != AssignOp::kAddAssign &&
                 assign->op != AssignOp::kOrAssign &&
                 assign->op != AssignOp::kAndAssign &&
                 assign->op != AssignOp::kNullishAssign) {
        write_type = GuessExpression(this, env_, assign->right, depth + 1) ==
                             JsType::kBigInt
                         ? JsType::kBigInt
                         : JsType::kNumber;
      }
    } else if (auto* upd = parent->As<UpdateExpression>();
               upd != nullptr && upd->argument == ref) {
      write_type = JsType::kNumber;
    } else {
      continue;
    }
    if (write_type == JsType::kUnknown) {
      continue;
    }
    if (inferred == JsType::kUnknown) {
      inferred = write_type;
    } else if (inferred != write_type) {
      return JsType::kUnknown;
    }
  }
  return inferred;
}

JsType Builder::GuessType(const Binding* binding) {
  return GuessBindingType(binding, 0);
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

// --- facts about variables ---------------------------------------------------

namespace {

// The parameters of the function or class constructor that `binding`
// declares, or nullptr if `binding` does not declare one.
const std::vector<Expression*>* DeclaredParameters(const Binding* binding) {
  static const std::vector<Expression*> kNone;
  const Node* declaration = binding->declaration;
  if (declaration == nullptr) {
    return nullptr;
  }
  const ClassBody* body = nullptr;
  if (auto* declarator = declaration->As<VariableDeclarator>()) {
    if (declarator->target != binding->identifier ||
        declarator->init == nullptr) {
      return nullptr;
    }
    declaration = declarator->init;
  }
  if (auto* fn = declaration->As<FunctionDeclaration>()) {
    return &fn->params;
  }
  if (auto* fn = declaration->As<FunctionExpression>()) {
    return &fn->params;
  }
  if (auto* fn = declaration->As<ArrowFunctionExpression>()) {
    return &fn->params;
  }
  if (auto* cls = declaration->As<ClassDeclaration>()) {
    body = cls->body;
  } else if (auto* cls_expr = declaration->As<ClassExpression>()) {
    body = cls_expr->body;
  }
  if (body == nullptr) {
    return nullptr;
  }
  for (const Node* member : body->members) {
    auto* method = member->As<MethodDefinition>();
    if (method != nullptr && method->method_kind == MethodKind::kConstructor &&
        method->value != nullptr) {
      return &method->value->params;
    }
  }
  return &kNone;  // the implicit constructor
}

// Adds to `info` the parameter types of the function or class that
// `binding` names. These are the types of the arguments of its first call
// or `new` in the tree. Generated code calls a function with arguments of
// its parameter types, and the call stays in the program.
void DeriveParameters(Builder& builder, const Binding* binding,
                      BindingInfo* info) {
  const std::vector<Expression*>* params = DeclaredParameters(binding);
  if (params == nullptr || params->empty()) {
    return;
  }
  for (Identifier* reference : binding->references) {
    Node* parent = builder.ParentOf(reference);
    const std::vector<Expression*>* args = nullptr;
    if (parent == nullptr) {
      continue;
    }
    if (auto* call = parent->As<CallExpression>();
        call != nullptr && call->callee == reference) {
      args = &call->arguments;
    } else if (auto* construct = parent->As<NewExpression>();
               construct != nullptr && construct->callee == reference) {
      args = &construct->arguments;
    }
    if (args == nullptr) {
      continue;
    }
    const size_t limit =
        std::min(params->size(), Builder::kMaxRecordedParameters);
    info->parameters.reserve(limit);
    for (size_t i = 0; i < limit; i++) {
      JsType type = JsType::kUnknown;
      if (i < args->size() && !(*args)[i]->Is<SpreadElement>()) {
        const Expression* arg = (*args)[i];
        const Binding* arg_binding = nullptr;
        if (auto* id = arg->As<Identifier>()) {
          arg_binding = builder.scopes().BindingOf(id);
        }
        type = arg_binding != nullptr ? builder.GuessType(arg_binding)
                                      : builder.GuessExpressionType(arg);
      }
      info->parameters.push_back(type);
    }
    return;
  }
}

// The derivations of the different fields are separate functions, so that
// changes of one field do not touch the code of the other fields.

// Adds to `info` the property keys of the value of `binding`.
void DeriveKeys(Builder& builder, const Binding* binding, BindingInfo* info) {
  DeriveKnownKeys(builder, binding, &info->keys);
}

}  // namespace

BindingInfo& Builder::InfoEntry(std::string_view name) {
  auto it = binding_info_.find(name);
  if (it != binding_info_.end()) {
    return it->second;
  }
  // Derive the facts once, for the first binding with this name.
  BindingInfo info;
  for (const Binding* binding : scopes().bindings()) {
    if (binding->name == name && binding->kind != BindingKind::kImplicit) {
      DeriveParameters(*this, binding, &info);
      DeriveKeys(*this, binding, &info);
      break;
    }
  }
  return binding_info_.try_emplace(tree_->Intern(name), std::move(info))
      .first->second;
}

BindingInfo& Builder::RecordInfo(std::string_view name) {
  return InfoEntry(name);
}

const BindingInfo* Builder::InfoOf(std::string_view name) {
  const BindingInfo& info = InfoEntry(name);
  if (info.parameters.empty() && info.keys.empty()) {
    return nullptr;
  }
  return &info;
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

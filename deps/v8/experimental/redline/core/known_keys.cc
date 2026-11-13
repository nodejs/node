// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/known_keys.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "experimental/redline/core/tunable.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

REDLINE_TUNABLE(kKnownKeysMaxClassDepth, "codegen.known_keys.max_class_depth",
                8, 0, 64,
                "most superclasses whose members count as known keys of a "
                "class and of its instances");

namespace {

// The number of parameters before a rest parameter.
int Arity(const FunctionData* fn) {
  if (fn == nullptr) {
    return -1;
  }
  int n = 0;
  for (const Expression* param : fn->params) {
    if (param->Is<RestElement>()) {
      break;
    }
    n++;
  }
  return n;
}

// Adds the keys of the `this.<name> = value` statements at the top level of
// the body of `fn`.
void AddThisStores(Tree* tree, const FunctionData* fn, bool on_instance,
                   std::vector<KnownKey>* keys) {
  if (fn == nullptr || fn->body == nullptr) {
    return;
  }
  for (const Statement* statement : fn->body->body) {
    const auto* expression = statement->As<ExpressionStatement>();
    if (expression == nullptr) {
      continue;
    }
    const auto* assign = expression->expression->As<AssignmentExpression>();
    if (assign == nullptr || assign->op != AssignOp::kAssign) {
      continue;
    }
    const auto* member = assign->left->As<MemberExpression>();
    if (member == nullptr || !member->object->Is<ThisExpression>()) {
      continue;
    }
    std::string_view name = KeyName(tree, member->property, member->computed);
    if (!name.empty()) {
      AddKnownKey(keys,
                  {name, Builder::GuessType(assign->right), -1, on_instance});
    }
  }
}

// The class that `binding` names, or nullptr.
const ClassData* ClassOfBinding(const Binding* binding) {
  Node* declaration = binding->declaration;
  if (declaration == nullptr) {
    return nullptr;
  }
  if (auto* cls = declaration->As<ClassDeclaration>()) {
    return cls;
  }
  if (auto* cls = declaration->As<ClassExpression>()) {
    return cls;
  }
  if (auto* declarator = declaration->As<VariableDeclarator>()) {
    if (declarator->target == binding->identifier &&
        declarator->init != nullptr) {
      return declarator->init->As<ClassExpression>();
    }
  }
  return nullptr;
}

// Adds the keys of `cls` and of its superclasses that the program declares.
// The keys of a subclass replace the keys of its superclasses.
void AddClassChainKeys(Builder& builder, const ClassData* cls,
                       std::vector<KnownKey>* keys) {
  for (int depth = 0;
       cls != nullptr && depth <= kKnownKeysMaxClassDepth.AsInt(); depth++) {
    std::vector<KnownKey> own;
    AddClassKeys(builder.tree(), cls->body, &own);
    for (const KnownKey& key : own) {
      bool shadowed = false;
      for (const KnownKey& known : *keys) {
        shadowed = shadowed || (known.name == key.name &&
                                known.on_instance == key.on_instance);
      }
      if (!shadowed) {
        keys->push_back(key);
      }
    }
    const auto* super_name = cls->super_class != nullptr
                                 ? cls->super_class->As<Identifier>()
                                 : nullptr;
    const Binding* super_binding = super_name != nullptr
                                       ? builder.scopes().BindingOf(super_name)
                                       : nullptr;
    cls = super_binding != nullptr ? ClassOfBinding(super_binding) : nullptr;
  }
}

}  // namespace

std::string_view KeyName(Tree* tree, const Expression* key, bool computed) {
  if (key == nullptr) {
    return {};
  }
  if (const auto* id = key->As<Identifier>()) {
    return computed ? std::string_view() : id->name;
  }
  if (const auto* text = key->As<StringLiteral>()) {
    return tree->Intern(text->value);
  }
  if (const auto* number = key->As<NumericLiteral>()) {
    const double value = number->value;
    // Integers only: they are the names that the printer and the parser
    // agree on without a number-to-string conversion.
    if (value >= 0 && value < 1e15 && value == std::floor(value)) {
      return tree->Intern(std::to_string(static_cast<int64_t>(value)));
    }
  }
  return {};
}

void AddKnownKey(std::vector<KnownKey>* keys, const KnownKey& key) {
  for (KnownKey& known : *keys) {
    if (known.name == key.name && known.on_instance == key.on_instance) {
      known.type = key.type;
      known.arity = key.arity;
      return;
    }
  }
  keys->push_back(key);
}

void AddObjectLiteralKeys(Tree* tree, const ObjectExpression* object,
                          std::vector<KnownKey>* keys) {
  for (const Node* node : object->properties) {
    const auto* property = node->As<Property>();
    if (property == nullptr) {
      continue;  // a spread
    }
    std::string_view name = KeyName(tree, property->key, property->computed);
    // `__proto__: v` sets the prototype and makes no property.
    if (name.empty() || (name == "__proto__" && !property->computed)) {
      continue;
    }
    if (property->method) {
      AddKnownKey(keys,
                  {name, JsType::kFunction,
                   Arity(property->value->As<FunctionExpression>()), false});
    } else if (property->property_kind != PropertyKind::kInit) {
      AddKnownKey(keys, {name, JsType::kUnknown, -1, false});
    } else {
      AddKnownKey(keys, {name, Builder::GuessType(property->value), -1, false});
    }
  }
}

void AddClassKeys(Tree* tree, const ClassBody* body,
                  std::vector<KnownKey>* keys) {
  for (const Node* member : body->members) {
    if (const auto* field = member->As<PropertyDefinition>()) {
      std::string_view name = KeyName(tree, field->key, field->computed);
      if (!name.empty()) {
        // A field without an initializer holds undefined.
        AddKnownKey(keys,
                    {name,
                     field->value != nullptr ? Builder::GuessType(field->value)
                                             : JsType::kNullish,
                     -1, !field->is_static});
      }
      continue;
    }
    const auto* method = member->As<MethodDefinition>();
    if (method == nullptr) {
      continue;  // a static block
    }
    if (method->method_kind == MethodKind::kConstructor) {
      AddThisStores(tree, method->value, true, keys);
      continue;
    }
    std::string_view name = KeyName(tree, method->key, method->computed);
    if (name.empty()) {
      continue;
    }
    if (method->method_kind == MethodKind::kMethod) {
      AddKnownKey(keys, {name, JsType::kFunction, Arity(method->value),
                         !method->is_static});
    } else {
      AddKnownKey(keys, {name, JsType::kUnknown, -1, !method->is_static});
    }
  }
}

void AddInstanceKeys(const std::vector<KnownKey>& class_keys,
                     std::vector<KnownKey>* keys) {
  for (const KnownKey& key : class_keys) {
    if (key.on_instance) {
      KnownKey own = key;
      own.on_instance = false;
      AddKnownKey(keys, own);
    }
  }
}

void DeriveKnownKeys(Builder& builder, const Binding* binding,
                     std::vector<KnownKey>* keys) {
  Tree* tree = builder.tree();
  if (const ClassData* cls = ClassOfBinding(binding)) {
    AddClassChainKeys(builder, cls, keys);
  } else if (binding->declaration != nullptr) {
    const auto* declarator = binding->declaration->As<VariableDeclarator>();
    const Expression* init =
        declarator != nullptr && declarator->target == binding->identifier
            ? declarator->init
            : nullptr;
    if (init != nullptr) {
      if (const auto* object = init->As<ObjectExpression>()) {
        AddObjectLiteralKeys(tree, object, keys);
      } else if (const auto* construct = init->As<NewExpression>()) {
        const auto* callee = construct->callee->As<Identifier>();
        const Binding* target =
            callee != nullptr ? builder.scopes().BindingOf(callee) : nullptr;
        if (target != nullptr) {
          if (const ClassData* target_class = ClassOfBinding(target)) {
            std::vector<KnownKey> class_keys;
            AddClassChainKeys(builder, target_class, &class_keys);
            AddInstanceKeys(class_keys, keys);
          } else if (target->declaration != nullptr) {
            // A constructor function: its `this.x = v` stores.
            AddThisStores(tree, target->declaration->As<FunctionDeclaration>(),
                          false, keys);
          }
        }
      }
    }
  }
  // The stores `binding.p = v` anywhere in the program.
  for (Identifier* reference : binding->references) {
    Node* parent = builder.ParentOf(reference);
    auto* member = parent != nullptr ? parent->As<MemberExpression>() : nullptr;
    if (member == nullptr || member->object != reference) {
      continue;
    }
    Node* grandparent = builder.ParentOf(member);
    auto* assign = grandparent != nullptr
                       ? grandparent->As<AssignmentExpression>()
                       : nullptr;
    if (assign == nullptr || assign->left != member) {
      continue;
    }
    std::string_view name = KeyName(tree, member->property, member->computed);
    if (!name.empty()) {
      AddKnownKey(keys, {name,
                         assign->op == AssignOp::kAssign
                             ? Builder::GuessType(assign->right)
                             : JsType::kUnknown,
                         -1, false});
    }
  }
}

}  // namespace redline

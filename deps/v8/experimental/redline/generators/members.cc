// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/members.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/known_keys.h"
#include "experimental/redline/core/tunable.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

using Statements = std::vector<Statement*>;
using Member = ClassMemberPlan::Member;
using Kind = ClassMemberPlan::Kind;
using KeyKind = ClassMemberPlan::KeyKind;

// Fuzzilli: Environment/JavaScriptEnvironment.swift:307-310.
constexpr std::string_view kPropertyNames[] = {"a", "b", "c", "d",
                                               "e", "f", "!", "42"};
constexpr std::string_view kMethodNames[] = {"m",  "n",       "o",       "?",
                                             "67", "valueOf", "toString"};
constexpr std::string_view kPrivateFieldNames[] = {"a", "b", "c",
                                                   "d", "e", "f"};
constexpr std::string_view kPrivateMethodNames[] = {"m", "n", "o", "p"};

template <size_t N>
std::string_view PickName(Random& r, const std::string_view (&names)[N]) {
  return names[r.Index(N)];
}

// True for a canonical array index such as "42": a numeric literal key.
bool IsIndexName(std::string_view name) {
  if (name.empty() || name.size() > 9 || (name.size() > 1 && name[0] == '0')) {
    return false;
  }
  return std::all_of(name.begin(), name.end(),
                     [](char c) { return c >= '0' && c <= '9'; });
}

}  // namespace

// --- member names ------------------------------------------------------------

std::string_view RandomMemberPropertyName(Random& r) {
  return PickName(r, kPropertyNames);
}

std::string_view RandomMemberMethodName(Random& r) {
  return PickName(r, kMethodNames);
}

std::string_view RandomPrivateFieldName(Random& r) {
  return PickName(r, kPrivateFieldNames);
}

std::string_view RandomPrivateMethodName(Random& r) {
  return PickName(r, kPrivateMethodNames);
}

Expression* MemberKey(Tree* t, std::string_view name) {
  if (IsIdentifierName(name)) {
    return mk::Id(t, name);
  }
  if (IsIndexName(name)) {
    return mk::Num(t, std::strtod(std::string(name).c_str(), nullptr));
  }
  return mk::Str(t, name);
}

// --- class bodies ------------------------------------------------------------
//
// Fuzzilli makes class members with 22 generators that run inside a class
// definition (CodeGen/CodeGenerators.swift:1080-1594). ClassDecl draws the
// same kinds here: fields, methods, getters and setters, each one static or
// not, with a plain, private or computed key.

REDLINE_TUNABLE(kClassMemberFieldWeight, "generators.ClassDecl.field_weight", 1,
                0, 100, "relative weight of a field among the members");
REDLINE_TUNABLE(kClassMemberMethodWeight, "generators.ClassDecl.method_weight",
                3, 0, 100, "relative weight of a method among the members");
REDLINE_TUNABLE(kClassMemberGetterWeight, "generators.ClassDecl.getter_weight",
                1, 0, 100, "relative weight of a getter among the members");
REDLINE_TUNABLE(kClassMemberSetterWeight, "generators.ClassDecl.setter_weight",
                0.3, 0, 100,
                "relative weight of a setter without a getter among the "
                "members");
REDLINE_TUNABLE(kClassMemberAccessorPairChance,
                "generators.ClassDecl.accessor_pair_chance", 0.5, 0, 1,
                "probability that a getter gets a setter with the same key");
REDLINE_TUNABLE(kClassMemberPrivateChance,
                "generators.ClassDecl.private_chance", 0.25, 0, 1,
                "probability that a member has a private name (#x)");
REDLINE_TUNABLE(kClassMemberComputedChance,
                "generators.ClassDecl.computed_chance", 0.15, 0, 1,
                "probability that a member without a private name has a "
                "computed key ([k])");
REDLINE_TUNABLE(kClassMemberComputedVariableChance,
                "generators.ClassDecl.computed_variable_chance", 0.5, 0, 1,
                "probability that a computed key is a visible variable, not a "
                "symbol or a literal");
REDLINE_TUNABLE(kClassMemberFieldInitChance,
                "generators.ClassDecl.field_init_chance", 0.8, 0, 1,
                "probability that a field has an initializer");
REDLINE_TUNABLE(kClassMemberAccessorMaxStatements,
                "generators.ClassDecl.accessor_max_statements", 2, 0, 64,
                "most statements in a getter or setter body before its return "
                "or store");
REDLINE_TUNABLE(kClassMemberSetterStoreChance,
                "generators.ClassDecl.setter_store_chance", 0.7, 0, 1,
                "probability that a setter stores its argument to a property "
                "of this");
REDLINE_TUNABLE(kClassMemberPrivateUseChance,
                "generators.ClassDecl.private_use_chance", 0.5, 0, 1,
                "probability that a getter returns, or a method or setter "
                "stores to, a private field of this when the class has one");

namespace {

// The well-known symbols, as keys of computed members.
constexpr std::string_view kWellKnownSymbols[] = {"iterator",
                                                  "asyncIterator",
                                                  "toPrimitive",
                                                  "hasInstance",
                                                  "toStringTag",
                                                  "species",
                                                  "isConcatSpreadable",
                                                  "unscopables",
                                                  "match",
                                                  "matchAll",
                                                  "replace",
                                                  "search",
                                                  "split"};

// A key for a computed member: a visible variable, a well-known symbol or a
// literal. Fuzzilli uses a random variable (CodeGenerators.swift:1164).
Expression* ComputedKey(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  if (!ctx.cannot_throw && r.Chance(kClassMemberComputedVariableChance())) {
    if (Identifier* id = cg.Variable()) {
      return id;
    }
  }
  if (r.Coin()) {
    return mk::Member(t, mk::Id(t, "Symbol"), PickName(r, kWellKnownSymbols));
  }
  return cg.Key();
}

// A private name of the pool that `taken` does not hold yet, or "".
template <size_t N>
std::string_view FreePrivateName(Random& r, const std::string_view (&names)[N],
                                 const std::vector<PrivateName>& taken) {
  const size_t start = r.Index(N);
  for (size_t i = 0; i < N; i++) {
    std::string_view name = names[(start + i) % N];
    if (std::none_of(taken.begin(), taken.end(),
                     [&](const PrivateName& p) { return p.name == name; })) {
      return name;
    }
  }
  return {};
}

// The context of a method, getter or setter body of the class.
GenContext MemberBodyContext(const GenContext& ctx, bool is_static,
                             bool has_super_class, bool is_async,
                             bool is_generator) {
  GenContext nested = ctx.Nested(true, false, is_async, is_generator);
  nested.syntax.strict = true;
  nested.syntax.in_method = true;
  nested.syntax.has_super_class = has_super_class;
  nested.syntax.static_member = is_static;
  nested.syntax.class_this = true;
  return nested;
}

// The context of a field initializer: strict code with `this`, without
// `arguments`.
GenContext FieldInitContext(const GenContext& ctx, bool is_static,
                            bool has_super_class) {
  GenContext nested = ctx.Nested(true, true, false, false);
  nested.syntax.strict = true;
  nested.syntax.in_class_field = true;
  nested.syntax.has_arguments = false;
  nested.syntax.in_method = true;
  nested.syntax.has_super_class = has_super_class;
  nested.syntax.static_member = is_static;
  nested.syntax.class_this = true;
  return nested;
}

// A private field of the class with the given staticness, or nullptr. Only
// fields: an accessor that reads or writes another accessor could call
// itself through it.
const PrivateName* OwnPrivateField(Random& r, const ClassMemberPlan& plan,
                                   bool is_static) {
  std::vector<const PrivateName*> candidates;
  for (const PrivateName& p : plan.private_names) {
    bool field = std::any_of(
        plan.members.begin(), plan.members.end(), [&](const Member& m) {
          return m.key_kind == KeyKind::kPrivate && m.name == p.name &&
                 m.kind == Kind::kField;
        });
    if (field && p.is_static == is_static) {
      candidates.push_back(&p);
    }
  }
  return candidates.empty() ? nullptr : r.Pick(candidates);
}

// `this.<name> = value` for a method or a setter. The name is never the key
// of an accessor of the class, so that a setter cannot call itself.
Statement* ThisStore(GenContext& ctx, CodeGen& inner,
                     const ClassMemberPlan& plan, bool is_static,
                     Expression* value) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  if (r.Chance(kClassMemberPrivateUseChance())) {
    if (const PrivateName* p = OwnPrivateField(r, plan, is_static)) {
      return inner.Stmt(
          mk::Assign(t, mk::PrivateMember(t, mk::This(t), p->name), value));
    }
  }
  std::vector<std::string_view> names;
  for (std::string_view name : kPropertyNames) {
    bool accessor = std::any_of(
        plan.members.begin(), plan.members.end(), [&](const Member& m) {
          return m.key_kind == KeyKind::kName && m.name == name &&
                 (m.kind == Kind::kGetter || m.kind == Kind::kSetter);
        });
    if (!accessor) {
      names.push_back(name);
    }
  }
  if (names.empty()) {
    return nullptr;
  }
  return inner.Stmt(
      mk::Assign(t, inner.Property(mk::This(t), r.Pick(names)), value));
}

Node* BuildMember(GenContext& ctx, CodeGen& cg,
                  const ClassMemberSettings& settings,
                  const ClassMemberPlan& plan, const Member& m) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  bool computed = false;
  Expression* key = nullptr;
  switch (m.key_kind) {
    case KeyKind::kName:
      key = MemberKey(t, m.name);
      break;
    case KeyKind::kPrivate:
      key = mk::PrivateId(t, m.name);
      break;
    case KeyKind::kComputed:
      key = ComputedKey(ctx, cg);
      computed = true;
      break;
  }
  switch (m.kind) {
    case Kind::kField: {
      GenContext nested =
          FieldInitContext(ctx, m.is_static, settings.has_super_class);
      CodeGen inner(nested);
      Expression* value = r.Chance(kClassMemberFieldInitChance())
                              ? inner.Value(JsType::kUnknown, 1)
                              : nullptr;
      PropertyDefinition* field = mk::ClassField(t, key, value, m.is_static);
      field->computed = computed;
      return field;
    }
    case Kind::kMethod: {
      GenContext nested =
          MemberBodyContext(ctx, m.is_static, settings.has_super_class,
                            r.Chance(settings.method_async_chance),
                            r.Chance(settings.method_generator_chance));
      int np = static_cast<int>(r.Range(0, settings.method_max_params));
      std::vector<Expression*> params = cg.Parameters(nested, np);
      CodeGen inner(nested);
      Statements body = inner.Body(
          nested, 1 + r.SmallCount(settings.method_max_statements - 1));
      if (r.Chance(settings.method_this_store_chance)) {
        if (Statement* s = ThisStore(ctx, inner, plan, m.is_static,
                                     inner.Value(JsType::kUnknown, 1))) {
          body.push_back(s);
        }
      }
      if (settings.has_super_class && r.Chance(settings.super_call_chance)) {
        SuperMethodCall(nested, inner, &body);
      }
      body.push_back(mk::Return(t, r.Coin()
                                       ? static_cast<Expression*>(mk::This(t))
                                       : inner.Value(JsType::kUnknown, 1)));
      FunctionExpression* fn =
          mk::Function(t, "", {}, std::move(body), nested.syntax.in_async,
                       nested.syntax.in_generator);
      fn->params = std::move(params);
      MethodDefinition* method =
          mk::ClassMethod(t, key, fn, MethodKind::kMethod, m.is_static);
      method->computed = computed;
      return method;
    }
    case Kind::kGetter: {
      GenContext nested = MemberBodyContext(
          ctx, m.is_static, settings.has_super_class, false, false);
      CodeGen inner(nested);
      Statements body = inner.Body(
          nested, r.SmallCount(kClassMemberAccessorMaxStatements.AsInt()));
      Expression* result = nullptr;
      if (r.Chance(kClassMemberPrivateUseChance())) {
        if (const PrivateName* p = OwnPrivateField(r, plan, m.is_static)) {
          result = mk::PrivateMember(t, mk::This(t), p->name);
        }
      }
      if (result == nullptr) {
        result = inner.Value(JsType::kUnknown, 1);
      }
      body.push_back(mk::Return(t, result));
      MethodDefinition* getter =
          mk::ClassMethod(t, key, mk::Function(t, "", {}, std::move(body)),
                          MethodKind::kGet, m.is_static);
      getter->computed = computed;
      return getter;
    }
    case Kind::kSetter: {
      std::string_view param = ctx.builder.FreshName("v");
      GenContext nested = MemberBodyContext(
          ctx, m.is_static, settings.has_super_class, false, false);
      nested.Declare(param, JsType::kUnknown);
      CodeGen inner(nested);
      Statements body = inner.Body(
          nested, r.SmallCount(kClassMemberAccessorMaxStatements.AsInt()));
      if (r.Chance(kClassMemberSetterStoreChance())) {
        if (Statement* s =
                ThisStore(ctx, inner, plan, m.is_static, mk::Id(t, param))) {
          body.push_back(s);
        }
      }
      MethodDefinition* setter =
          mk::ClassMethod(t, key, mk::Function(t, "", {param}, std::move(body)),
                          MethodKind::kSet, m.is_static);
      setter->computed = computed;
      return setter;
    }
  }
  return nullptr;
}

}  // namespace

ClassMemberPlan PlanClassMembers(GenContext& ctx, [[maybe_unused]] CodeGen& cg,
                                 const ClassMemberSettings& settings) {
  Random& r = ctx.rng();
  ClassMemberPlan plan;
  const std::vector<double> weights = {
      kClassMemberFieldWeight(), kClassMemberMethodWeight(),
      kClassMemberGetterWeight(), kClassMemberSetterWeight()};
  for (int i = 0; i < settings.count; i++) {
    Member m;
    switch (r.Weighted(weights)) {
      case 0:
        m.kind = Kind::kField;
        break;
      case 1:
        m.kind = Kind::kMethod;
        break;
      case 2:
        m.kind = Kind::kGetter;
        break;
      default:
        m.kind = Kind::kSetter;
        break;
    }
    m.is_static = r.Chance(settings.static_chance);
    const bool pair =
        m.kind == Kind::kGetter && r.Chance(kClassMemberAccessorPairChance());
    if (r.Chance(kClassMemberPrivateChance())) {
      std::string_view name =
          m.kind == Kind::kMethod
              ? FreePrivateName(r, kPrivateMethodNames, plan.private_names)
              : FreePrivateName(r, kPrivateFieldNames, plan.private_names);
      if (!name.empty()) {
        m.key_kind = KeyKind::kPrivate;
        m.name = name;
        PrivateName p;
        p.name = name;
        p.is_static = m.is_static;
        p.readable = m.kind != Kind::kSetter;
        p.writable = m.kind == Kind::kField || m.kind == Kind::kSetter || pair;
        p.callable = m.kind == Kind::kMethod;
        plan.private_names.push_back(p);
      }
    }
    if (m.key_kind != KeyKind::kPrivate) {
      if (r.Chance(kClassMemberComputedChance())) {
        m.key_kind = KeyKind::kComputed;
      } else {
        m.name = m.kind == Kind::kMethod ? RandomMemberMethodName(r)
                                         : RandomMemberPropertyName(r);
      }
    }
    plan.members.push_back(m);
    if (pair && m.key_kind != KeyKind::kComputed) {
      Member setter = m;
      setter.kind = Kind::kSetter;
      plan.members.push_back(setter);
    }
  }
  return plan;
}

void BuildClassMembers(GenContext& ctx, CodeGen& cg,
                       const ClassMemberSettings& settings,
                       const ClassMemberPlan& plan,
                       std::vector<Node*>* members) {
  for (const Member& m : plan.members) {
    if (Node* member = BuildMember(ctx, cg, settings, plan, m)) {
      members->push_back(member);
    }
  }
}

ScopedPrivateNames::ScopedPrivateNames(GenContext& ctx,
                                       const std::vector<PrivateName>& names)
    : ctx_(ctx), saved_(ctx.syntax.private_names) {
  std::vector<PrivateName> all = names;
  for (const PrivateName& p : saved_) {
    if (std::none_of(names.begin(), names.end(), [&](const PrivateName& own) {
          return own.name == p.name;
        })) {
      PrivateName outer = p;
      outer.outer = true;
      all.push_back(outer);
    }
  }
  ctx_.syntax.private_names = std::move(all);
}

ScopedPrivateNames::~ScopedPrivateNames() { Restore(); }

void ScopedPrivateNames::Restore() {
  if (!restored_) {
    ctx_.syntax.private_names = std::move(saved_);
    restored_ = true;
  }
}

// --- object literal members --------------------------------------------------
//
// Fuzzilli: ObjectLiteralMethod-, ObjectLiteralComputedMethod-,
// ObjectLiteralGetter-, ObjectLiteralSetter-, ObjectLiteralComputedGetter-,
// ObjectLiteralComputedSetter- and ObjectLiteralPrototypeGenerator
// (CodeGen/CodeGenerators.swift:917-1078).

REDLINE_TUNABLE(kLiteralSetterChance, "codegen.object.setter_chance", 0.2, 0, 1,
                "probability that an object-literal method is a setter");
REDLINE_TUNABLE(kLiteralComputedChance, "codegen.object.computed_method_chance",
                0.25, 0, 1,
                "probability that an object-literal method or accessor has a "
                "computed key (a symbol, a variable or a literal)");
REDLINE_TUNABLE(kLiteralMethodPoolChance,
                "codegen.object.method_pool_name_chance", 0.5, 0, 1,
                "probability that an object-literal method takes its name from "
                "the method name pool");
REDLINE_TUNABLE(kLiteralMethodMaxParams, "codegen.object.method_max_params", 2,
                0, 16, "most parameters of an object-literal method");
// Fuzzilli: randomAsyncAndGeneratorFlags(), CodeGenerators.swift:256-263.
REDLINE_TUNABLE(kLiteralMethodAsyncChance, "codegen.object.method_async_chance",
                0.05, 0, 1,
                "probability that an object-literal method is async only");
REDLINE_TUNABLE(
    kLiteralMethodGeneratorChance, "codegen.object.method_generator_chance",
    0.05, 0, 1,
    "probability that an object-literal method is a generator only");
REDLINE_TUNABLE(kLiteralMethodAsyncGeneratorChance,
                "codegen.object.method_async_generator_chance", 0.05, 0, 1,
                "probability that an object-literal method is an async "
                "generator");
REDLINE_TUNABLE(kLiteralSetterStoreChance, "codegen.object.setter_store_chance",
                0.7, 0, 1,
                "probability that an object-literal setter stores its argument "
                "to a backing property of this");
REDLINE_TUNABLE(kLiteralProtoNullChance, "codegen.object.proto_null_chance",
                0.1, 0, 1, "probability that a __proto__ entry is null");

namespace {

constexpr JsType kPrototypeTypes[] = {
    JsType::kObject, JsType::kArray,  JsType::kFunction, JsType::kClass,
    JsType::kMap,    JsType::kSet,    JsType::kRegExp,   JsType::kDate,
    JsType::kError,  JsType::kPromise};

}  // namespace

Expression* LiteralKey(Tree* t, std::string_view name, bool* computed) {
  *computed = false;
  if (name.starts_with("Symbol.")) {
    *computed = true;
    return mk::Member(t, mk::Id(t, "Symbol"), name.substr(7));
  }
  return MemberKey(t, name);
}

Node* LiteralMethod(GenContext& ctx, CodeGen& cg, std::string_view name,
                    const LiteralMemberSettings& settings) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  enum { kMethod, kGetter, kSetter } kind = kMethod;
  if (r.Chance(settings.getter_chance)) {
    kind = kGetter;
  } else if (r.Chance(kLiteralSetterChance())) {
    kind = kSetter;
  }
  if (kind == kMethod && r.Chance(kLiteralMethodPoolChance())) {
    name = RandomMemberMethodName(r);
  }
  bool computed = false;
  Expression* key = nullptr;
  if (r.Chance(kLiteralComputedChance())) {
    key = ComputedKey(ctx, cg);
    computed = true;
  } else {
    key = LiteralKey(t, name, &computed);
  }
  bool is_async = false;
  bool is_generator = false;
  if (kind == kMethod) {
    switch (r.Weighted(
        {kLiteralMethodAsyncChance(), kLiteralMethodGeneratorChance(),
         kLiteralMethodAsyncGeneratorChance(),
         1 - kLiteralMethodAsyncChance() - kLiteralMethodGeneratorChance() -
             kLiteralMethodAsyncGeneratorChance()})) {
      case 0:
        is_async = true;
        break;
      case 1:
        is_generator = true;
        break;
      case 2:
        is_async = true;
        is_generator = true;
        break;
      default:
        break;
    }
  }
  GenContext nested = ctx.Nested(true, false, is_async, is_generator);
  nested.syntax.in_method = true;
  std::vector<Expression*> params;
  std::string_view param;
  if (kind == kMethod) {
    params = cg.Parameters(
        nested, static_cast<int>(r.Range(0, kLiteralMethodMaxParams.AsInt())));
  } else if (kind == kSetter) {
    param = ctx.builder.FreshName("v");
    nested.Declare(param, JsType::kUnknown);
  }
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, (kind == kSetter ? 0 : 1) +
                             r.SmallCount(settings.max_extra_statements));
  if (kind == kSetter) {
    // A backing property that no accessor of the pools has: the setter
    // cannot call itself.
    if (r.Chance(kLiteralSetterStoreChance())) {
      std::string backing =
          "_" + std::string(IsIdentifierName(name) ? name : "p");
      body.push_back(inner.Stmt(
          mk::Assign(t, mk::Member(t, mk::This(t), t->Intern(backing)),
                     mk::Id(t, param))));
    }
  } else {
    body.push_back(
        mk::Return(t, inner.Value(JsType::kUnknown, settings.depth + 1)));
  }
  FunctionExpression* fn =
      mk::Function(t, "",
                   kind == kSetter ? std::vector<std::string_view>{param}
                                   : std::vector<std::string_view>{},
                   std::move(body), is_async, is_generator);
  if (kind == kMethod) {
    fn->params = std::move(params);
  }
  Property* property = mk::Prop(t, key, fn, computed);
  if (kind == kMethod) {
    property->method = true;
  } else {
    property->property_kind =
        kind == kGetter ? PropertyKind::kGet : PropertyKind::kSet;
  }
  return property;
}

Node* LiteralPrototype(GenContext& ctx, CodeGen& cg, int depth) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  Expression* value = nullptr;
  if (r.Chance(kLiteralProtoNullChance())) {
    value = mk::Null(t);
  } else {
    value = cg.Variable(kPrototypeTypes[r.Index(std::size(kPrototypeTypes))]);
    if (value == nullptr) {
      value = cg.Variable(JsType::kObject);
    }
    if (value == nullptr) {
      value = cg.ObjectLiteral(depth + 1);
    }
  }
  return mk::Prop(t, "__proto__", value);
}

// --- known keys --------------------------------------------------------------

namespace {

// The variable name of `expression`, or "".
std::string_view VariableName(const Expression* expression) {
  const auto* id =
      expression != nullptr ? expression->As<Identifier>() : nullptr;
  return id != nullptr ? id->name : std::string_view();
}

void RecordKeys(GenContext& ctx, std::string_view name,
                const std::vector<KnownKey>& keys) {
  if (keys.empty()) {
    return;
  }
  std::vector<KnownKey>& recorded = ctx.builder.RecordInfo(name).keys;
  for (const KnownKey& key : keys) {
    AddKnownKey(&recorded, key);
  }
}

}  // namespace

void RecordDeclaredKeys(GenContext& ctx, std::string_view name,
                        const Expression* init) {
  if (init == nullptr) {
    return;
  }
  Tree* t = ctx.tree();
  std::vector<KnownKey> keys;
  if (const auto* object = init->As<ObjectExpression>()) {
    AddObjectLiteralKeys(t, object, &keys);
  } else if (const auto* cls = init->As<ClassExpression>()) {
    AddClassKeys(t, cls->body, &keys);
  } else if (const auto* construct = init->As<NewExpression>()) {
    std::string_view callee = VariableName(construct->callee);
    if (!callee.empty()) {
      if (const BindingInfo* info = ctx.builder.InfoOf(callee)) {
        AddInstanceKeys(info->keys, &keys);
      }
    }
  }
  RecordKeys(ctx, name, keys);
}

void RecordClassKeys(GenContext& ctx, std::string_view name,
                     const Statement* statement) {
  const ClassData* cls = nullptr;
  if (const auto* declaration = statement->As<ClassDeclaration>()) {
    cls = declaration;
  } else if (const auto* variable = statement->As<VariableDeclaration>()) {
    if (variable->declarations.size() == 1 &&
        variable->declarations[0]->init != nullptr) {
      cls = variable->declarations[0]->init->As<ClassExpression>();
    }
  }
  if (cls == nullptr) {
    return;
  }
  std::vector<KnownKey> keys;
  AddClassKeys(ctx.tree(), cls->body, &keys);
  // The members of a known superclass, unless the class replaces them.
  std::string_view super_name = VariableName(cls->super_class);
  if (!super_name.empty()) {
    if (const BindingInfo* info = ctx.builder.InfoOf(super_name)) {
      for (const KnownKey& key : info->keys) {
        if (std::none_of(keys.begin(), keys.end(), [&](const KnownKey& own) {
              return own.name == key.name && own.on_instance == key.on_instance;
            })) {
          keys.push_back(key);
        }
      }
    }
  }
  RecordKeys(ctx, name, keys);
}

void RecordStore(GenContext& ctx, const Expression* target,
                 const Expression* value) {
  const auto* member = target->As<MemberExpression>();
  if (member == nullptr) {
    return;
  }
  std::string_view object = VariableName(member->object);
  std::string_view key =
      KeyName(ctx.tree(), member->property, member->computed);
  if (object.empty() || key.empty()) {
    return;
  }
  RecordKeys(ctx, object, {{key, Builder::GuessType(value), -1, false}});
}

std::optional<KnownKey> RandomKnownKey(GenContext& ctx,
                                       const Expression* object,
                                       bool methods_only) {
  std::string_view name = VariableName(object);
  if (name.empty()) {
    return std::nullopt;
  }
  const BindingInfo* info = ctx.builder.InfoOf(name);
  if (info == nullptr) {
    return std::nullopt;
  }
  std::vector<const KnownKey*> candidates;
  for (const KnownKey& key : info->keys) {
    if (!key.on_instance && (!methods_only || key.arity >= 0)) {
      candidates.push_back(&key);
    }
  }
  if (candidates.empty()) {
    return std::nullopt;
  }
  return *ctx.rng().Pick(candidates);
}

std::optional<KnownKey> KnownKeyOf(GenContext& ctx, const Expression* target) {
  const auto* member = target->As<MemberExpression>();
  if (member == nullptr) {
    return std::nullopt;
  }
  std::string_view object = VariableName(member->object);
  std::string_view key =
      KeyName(ctx.tree(), member->property, member->computed);
  if (object.empty() || key.empty()) {
    return std::nullopt;
  }
  const BindingInfo* info = ctx.builder.InfoOf(object);
  if (info == nullptr) {
    return std::nullopt;
  }
  for (const KnownKey& known : info->keys) {
    if (!known.on_instance && known.name == key) {
      return known;
    }
  }
  return std::nullopt;
}

// --- PrivateAccess -----------------------------------------------------------
//
// Fuzzilli: PrivatePropertyRetrievalGenerator, PrivatePropertyAssignment-,
// PrivatePropertyUpdate- and PrivateMethodCallGenerator
// (CodeGen/CodeGenerators.swift:2597-2655). Fuzzilli puts every access in a
// try block. Here an access on `this` in a member of the declaring class
// cannot throw, so it is guarded only sometimes.

REDLINE_TUNABLE(kPrivateAccessThisChance,
                "generators.PrivateAccess.this_chance", 0.75, 0, 1,
                "probability that the receiver is this when this has the "
                "private name");
REDLINE_TUNABLE(kPrivateAccessGuardChance,
                "generators.PrivateAccess.guard_chance", 0.1, 0, 1,
                "probability that an access on this is in a try block too");
REDLINE_TUNABLE(kPrivateAccessBrandCheckChance,
                "generators.PrivateAccess.brand_check_chance", 0.15, 0, 1,
                "probability of a brand check (#x in o) instead of an access");
REDLINE_TUNABLE(kPrivateAccessOptionalChance,
                "generators.PrivateAccess.optional_chance", 0.1, 0, 1,
                "probability that a load is an optional chain (o?.#x)");
REDLINE_TUNABLE(kPrivateAccessUpdateChance,
                "generators.PrivateAccess.update_chance", 0.2, 0, 1,
                "probability that a store to a readable name is a compound "
                "assignment or an increment");
REDLINE_TUNABLE(kPrivateAccessMaxArgs, "generators.PrivateAccess.max_args", 3,
                0, 16, "most arguments of a private method call");

bool GenPrivateAccess(GenContext& ctx, CodeGen& cg, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  if (ctx.syntax.private_names.empty()) {
    return false;
  }
  const PrivateName name = r.Pick(ctx.syntax.private_names);
  const Builder::Context& s = ctx.syntax;
  // `this` before super() in a derived constructor is a ReferenceError.
  const bool this_usable =
      s.has_this && s.class_this && !(s.in_constructor && s.has_super_class);
  const bool this_fits =
      this_usable && !name.outer && name.is_static == s.static_member;
  auto guarded = [&](Statement* statement, bool guard) {
    out->push_back(guard ? mk::Try(t, {statement}, "e", {}) : statement);
  };

  if (r.Chance(kPrivateAccessBrandCheckChance())) {
    // `#x in o` throws only if o is not an object.
    const bool on_this = this_usable && r.Coin();
    Expression* object = on_this ? mk::This(t) : cg.ObjectLike();
    Expression* check =
        mk::Binary(t, BinaryOp::kIn, mk::PrivateId(t, name.name), object);
    if (on_this) {
      out->push_back(cg.Declare(check, JsType::kBoolean, "b"));
    } else {
      for (Statement* st : cg.DeclareGuarded(check, JsType::kBoolean, "b")) {
        out->push_back(st);
      }
    }
    return true;
  }

  const bool on_this = this_fits && r.Chance(kPrivateAccessThisChance());
  Expression* receiver = on_this ? mk::This(t) : cg.ObjectLike();
  const bool guard = !on_this || r.Chance(kPrivateAccessGuardChance());
  // Choose an operation that the kind of the name allows.
  std::vector<int> ops;  // 0 load, 1 store, 2 call
  if (name.readable) {
    ops.push_back(0);
  }
  if (name.writable) {
    ops.push_back(1);
  }
  if (name.callable) {
    ops.push_back(2);
    ops.push_back(2);
  }
  if (ops.empty()) {
    return false;
  }
  switch (r.Pick(ops)) {
    case 0: {
      Expression* load = mk::PrivateMember(t, receiver, name.name);
      if (r.Chance(kPrivateAccessOptionalChance())) {
        load->Cast<MemberExpression>()->optional = true;
        load = mk::Chain(t, load);
      }
      if (guard) {
        for (Statement* st : cg.DeclareGuarded(load, JsType::kUnknown)) {
          out->push_back(st);
        }
      } else {
        out->push_back(cg.Declare(load, JsType::kUnknown));
      }
      return true;
    }
    case 1: {
      Expression* target = mk::PrivateMember(t, receiver, name.name);
      Expression* store = nullptr;
      if (name.readable && r.Chance(kPrivateAccessUpdateChance())) {
        store =
            r.Coin()
                ? static_cast<Expression*>(mk::Update(
                      t, r.Coin() ? UpdateOp::kIncrement : UpdateOp::kDecrement,
                      r.Coin(), target))
                : static_cast<Expression*>(
                      mk::Assign(t, target, cg.Value(JsType::kNumber, 0),
                                 CodeGen::RandomCompoundAssignOp(r)));
      } else {
        store = mk::Assign(t, target, cg.Value(JsType::kUnknown, 0));
      }
      guarded(cg.Stmt(store), guard);
      return true;
    }
    default: {
      Expression* call =
          mk::Call(t, mk::PrivateMember(t, receiver, name.name),
                   cg.Arguments(0, kPrivateAccessMaxArgs.AsInt()));
      if (guard) {
        for (Statement* st : cg.DeclareGuarded(call, JsType::kUnknown)) {
          out->push_back(st);
        }
      } else {
        out->push_back(cg.Declare(call, JsType::kUnknown));
      }
      return true;
    }
  }
}

// --- SuperAccess -------------------------------------------------------------
//
// Fuzzilli: SuperPropertyRetrieval-, SuperPropertyAssignment-,
// ComputedSuperPropertyRetrieval-, ComputedSuperPropertyAssignment-,
// SuperPropertyUpdate- and SuperMethodCallGenerator
// (CodeGen/CodeGenerators.swift:2579-2704). They run in every method with a
// home object: class members and object literal methods and accessors. The
// names come from the shared member pools, so that they often find a member
// of the superclass.

REDLINE_TUNABLE(kSuperAccessLoadWeight, "generators.SuperAccess.load_weight", 4,
                0, 100, "relative weight of super.p and super[k] loads");
REDLINE_TUNABLE(kSuperAccessStoreWeight, "generators.SuperAccess.store_weight",
                4, 0, 100,
                "relative weight of super.p = v and super[k] = v stores");
REDLINE_TUNABLE(kSuperAccessUpdateWeight,
                "generators.SuperAccess.update_weight", 1, 0, 100,
                "relative weight of super.p op= v updates");
REDLINE_TUNABLE(kSuperAccessCallWeight, "generators.SuperAccess.call_weight", 2,
                0, 100, "relative weight of super.m(...) calls");
REDLINE_TUNABLE(kSuperAccessComputedChance,
                "generators.SuperAccess.computed_chance", 0.5, 0, 1,
                "probability that a load or a store has a computed key "
                "(super[k])");
REDLINE_TUNABLE(kSuperAccessVariableKeyChance,
                "generators.SuperAccess.variable_key_chance", 0.5, 0, 1,
                "probability that a computed key is a visible variable rather "
                "than a key literal");
REDLINE_TUNABLE(kSuperAccessMethodNameChance,
                "generators.SuperAccess.method_name_chance", 0.3, 0, 1,
                "probability that a load or a store uses a method name of the "
                "pool rather than a property name");
REDLINE_TUNABLE(kSuperAccessCommonMethodChance,
                "generators.SuperAccess.common_method_chance", 0.5, 0, 1,
                "probability that a call uses a method that every object has, "
                "rather than a method name of the pool in a try block");
REDLINE_TUNABLE(kSuperAccessMaxArgs, "generators.SuperAccess.max_args", 3, 0,
                16, "most arguments of a super method call");
REDLINE_TUNABLE(kSuperAccessDeclareChance,
                "generators.SuperAccess.declare_chance", 0.6, 0, 1,
                "probability that the result of a call is bound to a variable");

namespace {

// The methods of Object.prototype: every super chain ends there, unless a
// prototype is null. Their arities.
struct CommonMethod {
  std::string_view name;
  int arity;
};
constexpr CommonMethod kCommonMethods[] = {
    {"toString", 0},       {"valueOf", 0},       {"toLocaleString", 0},
    {"hasOwnProperty", 1}, {"isPrototypeOf", 1}, {"propertyIsEnumerable", 1}};

// `super.<name>` or `super[key]`.
Expression* SuperMember(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  if (r.Chance(kSuperAccessComputedChance())) {
    Expression* key = nullptr;
    if (r.Chance(kSuperAccessVariableKeyChance())) {
      key = cg.Variable();
    }
    if (key == nullptr) {
      key = cg.Key();
    }
    return mk::Index(t, t->New<Super>(), key);
  }
  std::string_view name = r.Chance(kSuperAccessMethodNameChance())
                              ? RandomMemberMethodName(r)
                              : RandomMemberPropertyName(r);
  return cg.Property(t->New<Super>(), name);
}

}  // namespace

void SuperMethodCall(GenContext& ctx, CodeGen& cg, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  const bool common = r.Chance(kSuperAccessCommonMethodChance());
  std::string_view name;
  std::vector<Expression*> args;
  if (common) {
    const CommonMethod& method =
        kCommonMethods[r.Index(std::size(kCommonMethods))];
    name = method.name;
    for (int i = 0; i < method.arity; i++) {
      args.push_back(cg.Value(JsType::kUnknown, 1));
    }
  } else {
    name = RandomMemberMethodName(r);
    args = cg.Arguments(0, kSuperAccessMaxArgs.AsInt());
  }
  Expression* call =
      mk::Call(t, cg.Property(t->New<Super>(), name), std::move(args));
  // Before super() in a derived constructor, the call throws too.
  if (!common || (ctx.syntax.in_constructor && ctx.syntax.has_super_class)) {
    for (Statement* s : cg.DeclareGuarded(call, JsType::kUnknown)) {
      out->push_back(s);
    }
  } else if (r.Chance(kSuperAccessDeclareChance())) {
    out->push_back(cg.Declare(call, JsType::kUnknown));
  } else {
    out->push_back(cg.Stmt(call));
  }
}

bool GenSuperAccess(GenContext& ctx, CodeGen& cg, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  if (!ctx.syntax.in_method) {
    return false;
  }
  // In a derived constructor, code before super() cannot use `super.x`:
  // the access reads `this`. Such code goes into a try block.
  const bool guard = ctx.syntax.in_constructor && ctx.syntax.has_super_class;
  auto statement = [&](Expression* e) {
    Statement* s = cg.Stmt(e);
    out->push_back(guard ? mk::Try(t, {s}, "e", {}) : s);
  };
  switch (r.Weighted({kSuperAccessLoadWeight(), kSuperAccessStoreWeight(),
                      kSuperAccessUpdateWeight(), kSuperAccessCallWeight()})) {
    case 0:
      if (guard) {
        for (Statement* s :
             cg.DeclareGuarded(SuperMember(ctx, cg), JsType::kUnknown)) {
          out->push_back(s);
        }
      } else {
        out->push_back(cg.Declare(SuperMember(ctx, cg), JsType::kUnknown));
      }
      return true;
    case 1:
      // Sets the property on `this` unless a setter of the superclass
      // takes the value.
      statement(
          mk::Assign(t, SuperMember(ctx, cg), cg.Value(JsType::kUnknown, 0)));
      return true;
    case 2: {
      // Fuzzilli updates with a number.
      Expression* target =
          cg.Property(t->New<Super>(), RandomMemberPropertyName(r));
      statement(mk::Assign(t, target, cg.Value(JsType::kNumber, 0),
                           CodeGen::RandomCompoundAssignOp(r)));
      return true;
    }
    default:
      SuperMethodCall(ctx, cg, out);
      return true;
  }
}

// --- member generators -------------------------------------------------------

REDLINE_TUNABLE(kClassMemberStaticChance,
                "generators.ClassMember.static_chance", 0.2, 0, 1,
                "probability that the new member is static");
REDLINE_TUNABLE(kClassMemberMethodMaxParams,
                "generators.ClassMember.method_max_params", 2, 0, 16,
                "most parameters of a new method");
REDLINE_TUNABLE(kClassMemberMethodMaxStatements,
                "generators.ClassMember.method_max_statements", 3, 1, 64,
                "most statements in the body of a new method before its "
                "return");
REDLINE_TUNABLE(kClassMemberAsyncChance, "generators.ClassMember.async_chance",
                0.1, 0, 1, "probability that a new method is async");
REDLINE_TUNABLE(kClassMemberGeneratorChance,
                "generators.ClassMember.generator_chance", 0.1, 0, 1,
                "probability that a new method is a generator");
REDLINE_TUNABLE(kClassMemberThisStoreChance,
                "generators.ClassMember.this_store_chance", 0.3, 0, 1,
                "probability that a new method stores to a property of this");
REDLINE_TUNABLE(kClassMemberSuperCallChance,
                "generators.ClassMember.super_call_chance", 0.3, 0, 1,
                "probability that a new method of a derived class calls a "
                "super method");

REDLINE_TUNABLE(kLiteralMemberPropertyWeight,
                "generators.LiteralMember.property_weight", 4, 0, 100,
                "relative weight of a plain property");
REDLINE_TUNABLE(kLiteralMemberMethodWeight,
                "generators.LiteralMember.method_weight", 3, 0, 100,
                "relative weight of a method or an accessor");
REDLINE_TUNABLE(kLiteralMemberComputedWeight,
                "generators.LiteralMember.computed_weight", 1, 0, 100,
                "relative weight of a property with a visible variable as "
                "computed key");
REDLINE_TUNABLE(kLiteralMemberSpreadWeight,
                "generators.LiteralMember.spread_weight", 1, 0, 100,
                "relative weight of a spread of a visible object");
REDLINE_TUNABLE(kLiteralMemberProtoWeight,
                "generators.LiteralMember.proto_weight", 1, 0, 100,
                "relative weight of a __proto__ entry, if the literal has "
                "none");
REDLINE_TUNABLE(kLiteralMemberGetterChance,
                "generators.LiteralMember.getter_chance", 0.3, 0, 1,
                "probability that a new method is a getter");
REDLINE_TUNABLE(kLiteralMemberMaxExtraStatements,
                "generators.LiteralMember.max_extra_statements", 2, 0, 64,
                "most statements a new method gets beyond its first one");

bool GenClassMember(GenContext& ctx, CodeGen& cg,
                    [[maybe_unused]] Statements* out) {
  if (ctx.member_owner == nullptr || !ctx.member_owner->Is<ClassBody>() ||
      ctx.member_out == nullptr) {
    return false;
  }
  Node* cls = ctx.builder.ParentOf(ctx.member_owner);
  const ClassData* data = cls != nullptr ? AsClassData(cls) : nullptr;
  ClassMemberSettings settings;
  settings.count = 1;
  settings.has_super_class = data != nullptr && data->super_class != nullptr;
  settings.static_chance = kClassMemberStaticChance();
  settings.method_async_chance = kClassMemberAsyncChance();
  settings.method_generator_chance = kClassMemberGeneratorChance();
  settings.method_max_params = kClassMemberMethodMaxParams.AsInt();
  settings.method_max_statements = kClassMemberMethodMaxStatements.AsInt();
  settings.method_this_store_chance = kClassMemberThisStoreChance();
  settings.super_call_chance = kClassMemberSuperCallChance();
  ClassMemberPlan plan = PlanClassMembers(ctx, cg, settings);
  // A private name that the class declares already gets a public name: a
  // second declaration is an early error.
  for (Member& m : plan.members) {
    if (m.key_kind != KeyKind::kPrivate) {
      continue;
    }
    bool taken = std::any_of(
        ctx.syntax.private_names.begin(), ctx.syntax.private_names.end(),
        [&](const PrivateName& p) { return !p.outer && p.name == m.name; });
    if (taken) {
      plan.private_names.erase(
          std::remove_if(
              plan.private_names.begin(), plan.private_names.end(),
              [&](const PrivateName& p) { return p.name == m.name; }),
          plan.private_names.end());
      m.key_kind = KeyKind::kName;
      m.name = m.kind == Kind::kMethod ? RandomMemberMethodName(ctx.rng())
                                       : RandomMemberPropertyName(ctx.rng());
    }
  }
  ScopedPrivateNames private_names(ctx, plan.private_names);
  const size_t before = ctx.member_out->size();
  BuildClassMembers(ctx, cg, settings, plan, ctx.member_out);
  return ctx.member_out->size() > before;
}

bool GenLiteralMember(GenContext& ctx, CodeGen& cg,
                      [[maybe_unused]] Statements* out) {
  if (ctx.member_owner == nullptr ||
      !ctx.member_owner->Is<ObjectExpression>() || ctx.member_out == nullptr) {
    return false;
  }
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  const auto* object = ctx.member_owner->Cast<ObjectExpression>();
  bool has_proto = false;
  for (const Node* node : object->properties) {
    const auto* property = node->As<Property>();
    if (property != nullptr && !property->computed &&
        property->property_kind == PropertyKind::kInit && !property->method &&
        !property->shorthand &&
        KeyName(t, property->key, false) == "__proto__") {
      has_proto = true;
    }
  }
  Node* member = nullptr;
  switch (
      r.Weighted({kLiteralMemberPropertyWeight(), kLiteralMemberMethodWeight(),
                  kLiteralMemberComputedWeight(), kLiteralMemberSpreadWeight(),
                  has_proto ? 0.0 : kLiteralMemberProtoWeight()})) {
    case 0: {
      bool computed = false;
      Expression* key = LiteralKey(t, cg.PropertyName(), &computed);
      if (!computed && KeyName(t, key, false) == "__proto__") {
        return false;  // a second __proto__ entry is an early error
      }
      member = mk::Prop(t, key, cg.Value(JsType::kUnknown, 1), computed);
      break;
    }
    case 1: {
      LiteralMemberSettings settings;
      settings.depth = 1;
      settings.getter_chance = kLiteralMemberGetterChance();
      settings.max_extra_statements = kLiteralMemberMaxExtraStatements.AsInt();
      member = LiteralMethod(ctx, cg, RandomMemberPropertyName(r), settings);
      break;
    }
    case 2: {
      Identifier* key = cg.Variable();
      if (key == nullptr) {
        return false;
      }
      member = mk::Prop(t, key, cg.Value(JsType::kUnknown, 1), true);
      break;
    }
    case 3: {
      Identifier* source = cg.Variable(JsType::kObject);
      if (source == nullptr) {
        return false;
      }
      member = mk::Spread(t, source);
      break;
    }
    default:
      member = LiteralPrototype(ctx, cg, 1);
      break;
  }
  ctx.member_out->push_back(member);
  return true;
}

}  // namespace redline

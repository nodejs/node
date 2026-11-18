// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/values_generators.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/fast/scope.h"
#include "experimental/redline/generators/codegen.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

using Statements = std::vector<Statement*>;

Tree* T(GenContext& ctx) { return ctx.tree(); }

void Push(Statements* out, Statement* s) { out->push_back(s); }

// The name of the variable that the last Declare() registered.
std::string_view LastDeclared(GenContext& ctx) {
  return ctx.visible.front().name;
}

// Sets GenContext::cannot_throw while it lives. The value declarations of
// this file use it, so that their operands cannot throw in any context.
class CannotThrowScope {
 public:
  explicit CannotThrowScope(GenContext& ctx)
      : ctx_(ctx), saved_(ctx.cannot_throw) {
    ctx_.cannot_throw = true;
  }
  ~CannotThrowScope() { ctx_.cannot_throw = saved_; }
  CannotThrowScope(const CannotThrowScope&) = delete;
  CannotThrowScope& operator=(const CannotThrowScope&) = delete;

 private:
  GenContext& ctx_;
  bool saved_;
};

// --- shared helpers --------------------------------------------------------

REDLINE_TUNABLE(kSizeSmallMax, "codegen.size.small_max", 16, 0, 1 << 20,
                "largest value of the small range of a random size");
REDLINE_TUNABLE(kSizeMediumMax, "codegen.size.medium_max", 256, 0, 1 << 20,
                "largest value of the medium range of a random size");
REDLINE_TUNABLE(kSizeLargeMax, "codegen.size.large_max", 4096, 0, 1 << 24,
                "largest value of the large range of a random size");
REDLINE_TUNABLE(kSizeInterestingChance, "codegen.size.interesting_chance", 0.5,
                0, 1,
                "probability that a random size is an interesting integer of "
                "the environment");

// A size in [0, maximum], after Fuzzilli's randomSize(upTo:)
// (ProgramBuilder.swift:346): an interesting integer, or a uniform value in
// one of four ranges.
int64_t RandomSize(Random& r, const Environment& env, int64_t maximum) {
  if (maximum <= kSizeLargeMax.AsInt()) {
    return r.Range(0, maximum);
  }
  if (r.Chance(kSizeInterestingChance())) {
    std::vector<int64_t> sizes;
    for (double d : env.numbers()) {
      if (d >= 0 && d <= static_cast<double>(maximum) && d == std::floor(d)) {
        sizes.push_back(static_cast<int64_t>(d));
      }
    }
    if (!sizes.empty()) {
      return r.Pick(sizes);
    }
  }
  switch (r.Index(4)) {
    case 0:
      return r.Range(0, kSizeSmallMax.AsInt());
    case 1:
      return r.Range(0, kSizeMediumMax.AsInt());
    case 2:
      return r.Range(0, kSizeLargeMax.AsInt());
    default:
      return r.Range(0, maximum);
  }
}

// A typed array constructor and the size of its elements.
struct TypedArrayKind {
  const char* name;
  int element_size;
  bool bigint;
};

constexpr TypedArrayKind kTypedArrayKinds[] = {
    {"Int8Array", 1, false},         {"Uint8Array", 1, false},
    {"Uint8ClampedArray", 1, false}, {"Int16Array", 2, false},
    {"Uint16Array", 2, false},       {"Float16Array", 2, false},
    {"Int32Array", 4, false},        {"Uint32Array", 4, false},
    {"Float32Array", 4, false},      {"Float64Array", 8, false},
    {"BigInt64Array", 8, true},      {"BigUint64Array", 8, true},
};
constexpr size_t kNumTypedArrayKinds =
    sizeof(kTypedArrayKinds) / sizeof(kTypedArrayKinds[0]);

const TypedArrayKind& RandomTypedArrayKind(GenContext& ctx) {
  const bool has_float16 = ctx.env.Global("Float16Array") != nullptr;
  for (;;) {
    const TypedArrayKind& kind =
        kTypedArrayKinds[ctx.rng().Index(kNumTypedArrayKinds)];
    if (has_float16 || std::string_view(kind.name) != "Float16Array") {
      return kind;
    }
  }
}

// A visible variable whose type is one of `types`, or nullptr. Unlike
// CodeGen::Variable(), it never falls back to variables of unknown type.
Identifier* VariableOfTypes(GenContext& ctx,
                            std::initializer_list<JsType> types) {
  std::vector<std::string_view> names;
  for (const Variable& v : ctx.visible) {
    if (v.name == "arguments") {
      continue;
    }
    for (JsType type : types) {
      if (v.type == type) {
        names.push_back(v.name);
        break;
      }
    }
  }
  if (names.empty()) {
    return nullptr;
  }
  return mk::Id(T(ctx), ctx.rng().Pick(names));
}

// A primitive value: a visible variable of a primitive type, or a literal.
// It is never a symbol or an object, so a conversion to a string or a
// number cannot run user code.
Expression* PrimitiveValue(GenContext& ctx, CodeGen& cg) {
  JsType type =
      ctx.rng().Pick({JsType::kNumber, JsType::kNumber, JsType::kString,
                      JsType::kString, JsType::kBoolean, JsType::kNullish});
  if (ctx.rng().Coin()) {
    if (Identifier* id = VariableOfTypes(ctx, {type})) {
      return id;
    }
  }
  return cg.Literal(type);
}

// `Symbol.<name>`.
Expression* WellKnownSymbol(Tree* t, std::string_view name) {
  return mk::Member(t, mk::Id(t, "Symbol"), name);
}

// True if `key` is `Symbol.<name>`.
bool IsWellKnownSymbol(const Expression* key, std::string_view name) {
  const auto* member = key == nullptr ? nullptr : key->As<MemberExpression>();
  if (member == nullptr || member->computed || member->object == nullptr ||
      member->property == nullptr) {
    return false;
  }
  const auto* object = member->object->As<Identifier>();
  const auto* property = member->property->As<Identifier>();
  return object != nullptr && property != nullptr && object->name == "Symbol" &&
         property->name == name;
}

// The member `[key](...) {...}` of an object literal.
Property* ComputedMethod(Tree* t, Expression* key, FunctionExpression* fn) {
  Property* p = mk::Prop(t, key, fn, /*computed=*/true);
  p->method = true;
  return p;
}

// True if the object literal has a computed method keyed `Symbol.<name>`.
bool HasSymbolMethod(const ObjectExpression* object, std::string_view name) {
  for (const Node* node : object->properties) {
    const auto* p = node->As<Property>();
    if (p != nullptr && p->computed && IsWellKnownSymbol(p->key, name)) {
      return true;
    }
  }
  return false;
}

// True if the class has an instance method keyed `Symbol.<name>`.
bool HasSymbolMethod(const ClassData* cls, std::string_view name) {
  if (cls->body == nullptr) {
    return false;
  }
  for (const Node* node : cls->body->members) {
    const auto* m = node->As<MethodDefinition>();
    if (m != nullptr && m->computed && !m->is_static &&
        IsWellKnownSymbol(m->key, name)) {
      return true;
    }
  }
  return false;
}

// The initializer of a visible variable: the `init` of `let/const/var name =
// init`, if no code assigns to the variable. The variables of the current
// generation session have no binding yet: their declarations are in
// ctx.out. Returns nullptr if the initializer is not known.
const Expression* InitializerOf(GenContext& ctx, const Variable& v) {
  if (v.binding != nullptr) {
    if (v.binding->assigned || v.binding->declaration == nullptr) {
      return nullptr;
    }
    const auto* declarator = v.binding->declaration->As<VariableDeclarator>();
    if (declarator != nullptr && declarator->target == v.binding->identifier) {
      return declarator->init;
    }
    return nullptr;
  }
  if (ctx.out == nullptr) {
    return nullptr;
  }
  for (const Statement* s : *ctx.out) {
    const auto* declaration = s->As<VariableDeclaration>();
    if (declaration == nullptr) {
      continue;
    }
    for (const VariableDeclarator* d : declaration->declarations) {
      const auto* id =
          d->target == nullptr ? nullptr : d->target->As<Identifier>();
      if (id != nullptr && id->name == v.name) {
        return d->init;
      }
    }
  }
  return nullptr;
}

// The class that a visible variable names, or nullptr.
const ClassData* ClassOf(GenContext& ctx, std::string_view name) {
  for (const Variable& v : ctx.visible) {
    if (v.name != name) {
      continue;
    }
    if (v.binding != nullptr && v.binding->declaration != nullptr) {
      if (const auto* cls = v.binding->declaration->As<ClassDeclaration>()) {
        return cls;
      }
    }
    if (const Expression* init = InitializerOf(ctx, v)) {
      if (const auto* cls = init->As<ClassExpression>()) {
        return cls;
      }
    }
    if (v.binding == nullptr && ctx.out != nullptr) {
      for (const Statement* s : *ctx.out) {
        const auto* cls = s->As<ClassDeclaration>();
        if (cls != nullptr && cls->name != nullptr && cls->name->name == name) {
          return cls;
        }
      }
    }
    return nullptr;
  }
  return nullptr;
}

// The disposal that a value supports, judged from its initializer.
enum class Disposal { kNone, kSync, kAsync };

Disposal DisposalOf(GenContext& ctx, const Expression* init) {
  if (init == nullptr) {
    return Disposal::kNone;
  }
  if (const auto* object = init->As<ObjectExpression>()) {
    if (HasSymbolMethod(object, "dispose")) {
      return Disposal::kSync;
    }
    return HasSymbolMethod(object, "asyncDispose") ? Disposal::kAsync
                                                   : Disposal::kNone;
  }
  const auto* n = init->As<NewExpression>();
  if (n == nullptr || n->callee == nullptr) {
    return Disposal::kNone;
  }
  const ClassData* cls = nullptr;
  if (const auto* callee = n->callee->As<Identifier>()) {
    if (callee->name == "DisposableStack") {
      return Disposal::kSync;
    }
    if (callee->name == "AsyncDisposableStack") {
      return Disposal::kAsync;
    }
    cls = ClassOf(ctx, callee->name);
  } else if (const auto* expression = n->callee->As<ClassExpression>()) {
    cls = expression;
  }
  if (cls == nullptr) {
    return Disposal::kNone;
  }
  if (HasSymbolMethod(cls, "dispose")) {
    return Disposal::kSync;
  }
  return HasSymbolMethod(cls, "asyncDispose") ? Disposal::kAsync
                                              : Disposal::kNone;
}

// The visible variables that hold disposable values. With `async_ok`, the
// values that have only [Symbol.asyncDispose] count too (`await using`).
std::vector<std::string_view> VisibleDisposables(GenContext& ctx,
                                                 bool async_ok) {
  std::vector<std::string_view> names;
  for (const Variable& v : ctx.visible) {
    Disposal d = DisposalOf(ctx, InitializerOf(ctx, v));
    if (d == Disposal::kSync || (async_ok && d == Disposal::kAsync)) {
      names.push_back(v.name);
    }
  }
  return names;
}

// True for `new ArrayBuffer(n, {maxByteLength: m})` and for the same with a
// SharedArrayBuffer. Sets *shared for the latter.
bool IsResizableBufferInit(const Expression* init, bool* shared) {
  const auto* n = init == nullptr ? nullptr : init->As<NewExpression>();
  if (n == nullptr || n->callee == nullptr || n->arguments.size() < 2 ||
      n->arguments[1] == nullptr) {
    return false;
  }
  const auto* callee = n->callee->As<Identifier>();
  const auto* options = n->arguments[1]->As<ObjectExpression>();
  if (callee == nullptr || options == nullptr ||
      (callee->name != "ArrayBuffer" && callee->name != "SharedArrayBuffer")) {
    return false;
  }
  for (const Node* node : options->properties) {
    const auto* p = node->As<Property>();
    const auto* key = p == nullptr || p->computed || p->key == nullptr
                          ? nullptr
                          : p->key->As<Identifier>();
    if (key != nullptr && key->name == "maxByteLength") {
      *shared = callee->name == "SharedArrayBuffer";
      return true;
    }
  }
  return false;
}

// A visible resizable ArrayBuffer or growable SharedArrayBuffer, or an empty
// name. Sets *shared for a SharedArrayBuffer.
std::string_view VisibleResizableBuffer(GenContext& ctx, bool* shared) {
  std::vector<std::pair<std::string_view, bool>> buffers;
  for (const Variable& v : ctx.visible) {
    bool is_shared = false;
    if (IsResizableBufferInit(InitializerOf(ctx, v), &is_shared)) {
      buffers.emplace_back(v.name, is_shared);
    }
  }
  if (buffers.empty()) {
    return {};
  }
  const auto& picked = ctx.rng().Pick(buffers);
  *shared = picked.second;
  return picked.first;
}

// --- BuiltinInstanceDecl ----------------------------------------------------

REDLINE_TUNABLE(kBuiltinInstanceDeclContentsChance,
                "generators.BuiltinInstanceDecl.contents_chance", 0.3, 0, 1,
                "probability that a Map, Set, WeakMap, WeakSet or Date gets "
                "constructor arguments");
REDLINE_TUNABLE(kBuiltinInstanceDeclMaxEntries,
                "generators.BuiltinInstanceDecl.max_entries", 3, 1, 16,
                "most entries of the iterable passed to new Map or new Set");
REDLINE_TUNABLE(kBuiltinInstanceDeclMaxArrayLength,
                "generators.BuiltinInstanceDecl.max_array_length", 4096, 0,
                1 << 24, "largest n of new Array(n)");
REDLINE_TUNABLE(kBuiltinInstanceDeclCauseChance,
                "generators.BuiltinInstanceDecl.cause_chance", 0.2, 0, 1,
                "probability that a new error gets an options bag with a "
                "cause");

// A key that WeakMap and WeakSet accept: an object.
Expression* WeakKey(GenContext& ctx, CodeGen& cg) {
  if (ctx.rng().Coin()) {
    if (Identifier* id = VariableOfTypes(
            ctx, {JsType::kObject, JsType::kArray, JsType::kFunction})) {
      return id;
    }
  }
  return ctx.rng().Coin() ? cg.ObjectLiteral(1) : cg.ArrayLiteral(1);
}

// The entries of `new Map(...)`, `new WeakMap(...)`, `new Set(...)` or
// `new WeakSet(...)`.
Expression* CollectionEntries(GenContext& ctx, CodeGen& cg, bool is_map,
                              bool is_weak) {
  Tree* t = T(ctx);
  const int n =
      1 + ctx.rng().SmallCount(kBuiltinInstanceDeclMaxEntries.AsInt() - 1);
  std::vector<Expression*> entries;
  for (int i = 0; i < n; i++) {
    Expression* key =
        is_weak ? WeakKey(ctx, cg) : cg.Value(JsType::kUnknown, 1);
    entries.push_back(
        is_map ? mk::Array(t, {key, cg.Value(JsType::kUnknown, 1)}) : key);
  }
  return mk::Array(t, std::move(entries));
}

// A new error object. The constructors of all error types accept any
// message, and AggregateError gets an array of errors.
Expression* NewError(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view kind =
      r.Pick({"Error", "TypeError", "RangeError", "SyntaxError",
              "ReferenceError", "EvalError", "URIError", "AggregateError"});
  std::vector<Expression*> args;
  if (kind == "AggregateError") {
    args.push_back(mk::Array(t, {cg.Value(JsType::kUnknown, 1)}));
  }
  args.push_back(PrimitiveValue(ctx, cg));
  if (r.Chance(kBuiltinInstanceDeclCauseChance())) {
    args.push_back(
        mk::Object(t, {mk::Prop(t, "cause", cg.Value(JsType::kUnknown, 1))}));
  }
  return mk::New(t, mk::Id(t, kind), std::move(args));
}

// A builtin instance made by a constructor that cannot throw, after
// Fuzzilli's BuiltinObjectInstanceGenerator (CodeGenerators.swift:394). It
// also makes resolved promises and errors, and sometimes gives a
// collection initial contents.
bool GenBuiltinInstanceDecl(GenContext& ctx, CodeGen& cg, Statements* out) {
  CannotThrowScope safe(ctx);
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const bool contents = r.Chance(kBuiltinInstanceDeclContentsChance());
  Expression* value = nullptr;
  JsType type = JsType::kUnknown;
  switch (r.Index(8)) {
    case 0: {
      const int64_t length =
          RandomSize(r, ctx.env, kBuiltinInstanceDeclMaxArrayLength.AsInt());
      value = mk::New(t, mk::Id(t, "Array"),
                      {mk::Num(t, static_cast<double>(length))});
      break;
    }
    case 1:
    case 2:
    case 3:
    case 4: {
      const bool is_map = r.Coin();
      const bool is_weak = r.Coin();
      std::string_view name = is_map ? (is_weak ? "WeakMap" : "Map")
                                     : (is_weak ? "WeakSet" : "Set");
      std::vector<Expression*> args;
      if (contents) {
        args.push_back(CollectionEntries(ctx, cg, is_map, is_weak));
      }
      value = mk::New(t, mk::Id(t, name), std::move(args));
      break;
    }
    case 5: {
      std::vector<Expression*> args;
      if (contents) {
        args.push_back(r.Coin() ? cg.Number() : cg.String());
      }
      value = mk::New(t, mk::Id(t, "Date"), std::move(args));
      break;
    }
    case 6:
      value = mk::CallMethod(t, mk::Id(t, "Promise"), "resolve",
                             {PrimitiveValue(ctx, cg)});
      type = JsType::kPromise;
      break;
    default:
      value = NewError(ctx, cg);
      type = JsType::kError;
      break;
  }
  if (type == JsType::kUnknown) {
    type = Builder::GuessType(value);
  }
  Push(out, cg.Declare(value, type, "o"));
  return true;
}

// --- ConstructorFunctionDecl ------------------------------------------------

REDLINE_TUNABLE(kConstructorFunctionDeclMaxParams,
                "generators.ConstructorFunctionDecl.max_params", 3, 0, 8,
                "most parameters of the constructor function");
REDLINE_TUNABLE(kConstructorFunctionDeclDefaultChance,
                "generators.ConstructorFunctionDecl.default_chance", 0.1, 0, 1,
                "probability that a parameter gets a default value");
REDLINE_TUNABLE(kConstructorFunctionDeclMaxProperties,
                "generators.ConstructorFunctionDecl.max_properties", 3, 1, 12,
                "most this.p = v stores in the constructor body");
REDLINE_TUNABLE(kConstructorFunctionDeclPropertyNames,
                "generators.ConstructorFunctionDecl.property_names", 12, 1, 39,
                "number of leading environment property names the stores use");
REDLINE_TUNABLE(kConstructorFunctionDeclMinInstances,
                "generators.ConstructorFunctionDecl.min_instances", 1, 1, 16,
                "fewest new C(...) calls after the declaration");
REDLINE_TUNABLE(kConstructorFunctionDeclMaxInstances,
                "generators.ConstructorFunctionDecl.max_instances", 4, 1, 16,
                "most new C(...) calls after the declaration");
REDLINE_TUNABLE(kConstructorFunctionDeclVariableChance,
                "generators.ConstructorFunctionDecl.variable_chance", 0.8, 0, 1,
                "probability that a stored value is a visible variable or a "
                "parameter rather than an integer literal");

// `function C(a, b) { this.x = a; this.y = v; }` and 1-4 instances, after
// Fuzzilli's ObjectConstructorGenerator (CodeGenerators.swift:666). The
// body only stores values, so the constructor calls cannot throw.
bool GenConstructorFunctionDecl(GenContext& ctx, CodeGen& cg, Statements* out) {
  CannotThrowScope safe(ctx);
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view name = ctx.builder.FreshName("C");
  GenContext nested = ctx.Nested(true, false, false, false);
  const int np =
      static_cast<int>(r.Range(0, kConstructorFunctionDeclMaxParams.AsInt()));
  std::vector<Expression*> params;
  for (int i = 0; i < np; i++) {
    std::string_view param = ctx.builder.FreshName("a");
    Expression* node = mk::Id(t, param);
    if (r.Chance(kConstructorFunctionDeclDefaultChance())) {
      // The default value is computed in the caller's context at each call,
      // so it must not throw either.
      node = mk::WithDefault(t, node, cg.Value(JsType::kUnknown, 1));
    }
    params.push_back(node);
    nested.Declare(param, JsType::kUnknown);
  }
  CodeGen inner(nested);
  const size_t pool = std::min<size_t>(
      ctx.env.property_names().size(),
      static_cast<size_t>(kConstructorFunctionDeclPropertyNames.AsInt()));
  const int count =
      1 + r.SmallCount(kConstructorFunctionDeclMaxProperties.AsInt() - 1);
  std::vector<std::string_view> used;
  Statements body;
  for (int i = 0; i < count; i++) {
    std::string_view property = ctx.env.property_names()[r.Index(pool)];
    if (std::find(used.begin(), used.end(), property) != used.end() ||
        !IsIdentifierName(property)) {
      continue;
    }
    used.push_back(property);
    Expression* value = nullptr;
    if (r.Chance(kConstructorFunctionDeclVariableChance())) {
      value = inner.Variable();
    }
    if (value == nullptr) {
      value = inner.Integer();
    }
    body.push_back(
        cg.Stmt(mk::Assign(t, mk::Member(t, mk::This(t), property), value)));
  }
  FunctionDeclaration* decl = mk::FunctionDecl(t, name, {}, std::move(body));
  decl->params = std::move(params);
  Push(out, decl);
  ctx.Declare(name, JsType::kClass);
  const int instances = static_cast<int>(
      r.Range(kConstructorFunctionDeclMinInstances.AsInt(),
              std::max(kConstructorFunctionDeclMinInstances.AsInt(),
                       kConstructorFunctionDeclMaxInstances.AsInt())));
  for (int i = 0; i < instances; i++) {
    std::vector<Expression*> args;
    for (int k = 0; k < np; k++) {
      args.push_back(cg.Value(JsType::kUnknown, 1));
    }
    Push(out, cg.Declare(mk::New(t, mk::Id(t, name), std::move(args)),
                         JsType::kObject, "o"));
  }
  return true;
}

// --- IterableDecl -----------------------------------------------------------

REDLINE_TUNABLE(kIterableDeclMaxCount, "generators.IterableDecl.max_count", 10,
                0, 1000,
                "most values that a hand-written iterator produces (Fuzzilli: "
                "10)");
REDLINE_TUNABLE(kIterableDeclGeneratorChance,
                "generators.IterableDecl.generator_chance", 0.3, 0, 1,
                "probability that [Symbol.iterator] is a generator method "
                "instead of a hand-written next()");
REDLINE_TUNABLE(kIterableDeclMaxYields, "generators.IterableDecl.max_yields", 3,
                1, 16, "most yield statements of the generator method");
REDLINE_TUNABLE(kIterableDeclReturnChance,
                "generators.IterableDecl.return_chance", 0.3, 0, 1,
                "probability that the iterator also has a return() method, "
                "which a loop exit or a destructuring calls");
REDLINE_TUNABLE(kIterableDeclCounterValueChance,
                "generators.IterableDecl.counter_value_chance", 0.7, 0, 1,
                "probability that next() produces the counter rather than a "
                "visible variable or a literal");

// A value that is safe to compute each time an iterator or a disposer
// runs: a visible variable or a literal.
Expression* PlainValue(GenContext& ctx, CodeGen& cg) {
  if (ctx.rng().Coin()) {
    if (Identifier* id = cg.Variable()) {
      return id;
    }
  }
  return cg.Literal(
      ctx.rng().Pick({JsType::kNumber, JsType::kString, JsType::kBoolean,
                      JsType::kNullish, JsType::kBigInt}));
}

// An object with [Symbol.iterator], after Fuzzilli's IteratorGenerator
// (CodeGenerators.swift:3550): a hand-written iterator with a counter, or
// a generator method.
bool GenIterableDecl(GenContext& ctx, CodeGen& cg, Statements* out) {
  CannotThrowScope safe(ctx);
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  FunctionExpression* method = nullptr;
  if (r.Chance(kIterableDeclGeneratorChance())) {
    Statements body;
    const int yields = 1 + r.SmallCount(kIterableDeclMaxYields.AsInt() - 1);
    for (int i = 0; i < yields; i++) {
      body.push_back(cg.Stmt(mk::Yield(t, PlainValue(ctx, cg))));
    }
    method = mk::Function(t, "", {}, std::move(body), /*is_async=*/false,
                          /*is_generator=*/true);
  } else {
    // A counter n starts at K, and next() decrements it. The test `n < 0`
    // ends the iteration after K values for any K >= 0.
    std::string_view counter = ctx.builder.FreshName("n");
    const int count =
        static_cast<int>(r.Range(0, kIterableDeclMaxCount.AsInt()));
    Expression* value = r.Chance(kIterableDeclCounterValueChance())
                            ? static_cast<Expression*>(mk::Id(t, counter))
                            : PlainValue(ctx, cg);
    Statements next_body;
    next_body.push_back(cg.Stmt(
        mk::Update(t, UpdateOp::kDecrement, false, mk::Id(t, counter))));
    next_body.push_back(mk::Return(
        t,
        mk::Object(t, {mk::Prop(t, "done",
                                mk::Binary(t, BinaryOp::kLessThan,
                                           mk::Id(t, counter), mk::Num(t, 0))),
                       mk::Prop(t, "value", value)})));
    std::vector<Node*> iterator = {
        mk::Method(t, "next", mk::Function(t, "", {}, std::move(next_body)))};
    if (r.Chance(kIterableDeclReturnChance())) {
      iterator.push_back(mk::Method(
          t, "return",
          mk::Function(
              t, "", {},
              {mk::Return(t, mk::Object(t, {mk::Prop(t, "done",
                                                     mk::Bool(t, true))}))})));
    }
    method = mk::Function(
        t, "", {},
        {mk::Let(t, counter, mk::Num(t, static_cast<double>(count))),
         mk::Return(t, mk::Object(t, std::move(iterator)))});
  }
  Expression* iterable = mk::Object(
      t, {ComputedMethod(t, WellKnownSymbol(t, "iterator"), method)});
  Push(out, cg.Declare(iterable, JsType::kObject, "it"));
  return true;
}

// --- DisposableDecl and AsyncDisposableDecl ---------------------------------

REDLINE_TUNABLE(kDisposableDeclClassChance,
                "generators.DisposableDecl.class_chance", 0.3, 0, 1,
                "probability that the value is an instance of a new class "
                "instead of an object literal");
REDLINE_TUNABLE(kDisposableDeclEmptyChance,
                "generators.DisposableDecl.empty_chance", 0.4, 0, 1,
                "probability that the dispose method has an empty body, as "
                "in the Fuzzilli prefix");
REDLINE_TUNABLE(kDisposableDeclStoreChance,
                "generators.DisposableDecl.store_chance", 0.5, 0, 1,
                "probability that a dispose method with a body only stores a "
                "value into this");
REDLINE_TUNABLE(kDisposableDeclMaxStatements,
                "generators.DisposableDecl.max_statements", 2, 1, 16,
                "most generated statements in the dispose method");
REDLINE_TUNABLE(kDisposableDeclAsyncMethodChance,
                "generators.DisposableDecl.async_method_chance", 0.5, 0, 1,
                "probability that the asyncDispose method is an async method");

// The dispose method of a new value: empty, a store into `this`, or
// generated statements. It runs only at the end of a `using` scope or in a
// dispose() call, so its statements are not limited.
FunctionExpression* DisposeMethod(GenContext& ctx, CodeGen& cg, bool is_async,
                                  bool in_class) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Statements body;
  if (!r.Chance(kDisposableDeclEmptyChance())) {
    if (r.Chance(kDisposableDeclStoreChance())) {
      body.push_back(cg.Stmt(mk::Assign(
          t, mk::Member(t, mk::This(t), r.Pick({"a", "b", "c", "done"})),
          PlainValue(ctx, cg))));
    } else {
      GenContext nested = ctx.Nested(true, false, is_async, false);
      nested.syntax.in_method = true;
      // Class bodies are strict code.
      nested.syntax.strict = nested.syntax.strict || in_class;
      CodeGen inner(nested);
      body = inner.Body(
          nested, 1 + r.SmallCount(kDisposableDeclMaxStatements.AsInt() - 1));
    }
  }
  return mk::Function(t, "", {}, std::move(body), is_async, false);
}

// A value with [Symbol.dispose] (or [Symbol.asyncDispose]). It is an object
// literal, after Fuzzilli's DisposableGenerator and AsyncDisposableGenerator
// (CodeGenerators.swift:3574, :3583). Or it is an instance of a class with
// that method, after DisposableClassVariableGenerator (:843).
bool GenDisposableValue(GenContext& ctx, CodeGen& cg, Statements* out,
                        bool is_async) {
  CannotThrowScope safe(ctx);
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view symbol = is_async ? "asyncDispose" : "dispose";
  const bool async_method =
      is_async && r.Chance(kDisposableDeclAsyncMethodChance());
  const bool in_class = r.Chance(kDisposableDeclClassChance());
  FunctionExpression* method = DisposeMethod(ctx, cg, async_method, in_class);
  if (in_class) {
    std::string_view cls = ctx.builder.FreshName("D");
    MethodDefinition* member = mk::ClassMethod(
        t, WellKnownSymbol(t, symbol), method, MethodKind::kMethod, false);
    member->computed = true;
    Push(out, mk::ClassDecl(t, cls, nullptr, {member}));
    ctx.Declare(cls, JsType::kClass);
    Push(out, cg.Declare(mk::New(t, mk::Id(t, cls), {}), JsType::kObject,
                         is_async ? "ad" : "d"));
    return true;
  }
  Expression* value =
      mk::Object(t, {ComputedMethod(t, WellKnownSymbol(t, symbol), method)});
  Push(out, cg.Declare(value, JsType::kObject, is_async ? "ad" : "d"));
  return true;
}

bool GenDisposableDecl(GenContext& ctx, CodeGen& cg, Statements* out) {
  return GenDisposableValue(ctx, cg, out, false);
}

bool GenAsyncDisposableDecl(GenContext& ctx, CodeGen& cg, Statements* out) {
  return GenDisposableValue(ctx, cg, out, true);
}

// --- ResizableBufferDecl ----------------------------------------------------

REDLINE_TUNABLE(kResizableBufferMaxLength,
                "generators.ResizableBufferDecl.max_length", 4096, 0, 1 << 24,
                "largest initial byte length of a resizable buffer");
REDLINE_TUNABLE(kResizableBufferMaxByteLengthLog2,
                "generators.ResizableBufferDecl.max_byte_length_log2", 32, 0,
                52,
                "log2 of the largest maxByteLength of a resizable buffer "
                "(Fuzzilli: randomSize(), at most 2^32)");
REDLINE_TUNABLE(kResizableBufferSharedChance,
                "generators.ResizableBufferDecl.shared_chance", 0.5, 0, 1,
                "probability of a growable SharedArrayBuffer instead of a "
                "resizable ArrayBuffer");
REDLINE_TUNABLE(kResizableBufferTrackingChance,
                "generators.ResizableBufferDecl.tracking_chance", 0.5, 0, 1,
                "probability that a view tracks the buffer length");
REDLINE_TUNABLE(kResizableBufferFixedLengthChance,
                "generators.ResizableBufferDecl.fixed_length_chance", 0.5, 0, 1,
                "probability that a view with an offset has a fixed length");

// The parts of a new resizable buffer.
struct BufferPlan {
  bool shared = false;
  int64_t length = 0;      // the initial byteLength
  int64_t max_length = 0;  // maxByteLength
};

BufferPlan RandomBufferPlan(GenContext& ctx) {
  Random& r = ctx.rng();
  BufferPlan plan;
  plan.shared = r.Chance(kResizableBufferSharedChance());
  plan.length = RandomSize(r, ctx.env, kResizableBufferMaxLength.AsInt());
  // maxByteLength >= byteLength, with room to grow most of the time.
  const int64_t limit = int64_t{1} << kResizableBufferMaxByteLengthLog2.AsInt();
  plan.max_length =
      plan.length +
      RandomSize(r, ctx.env, std::max<int64_t>(0, limit - plan.length));
  return plan;
}

// `new ArrayBuffer(n, {maxByteLength: m})`, or a SharedArrayBuffer.
Expression* NewResizableBuffer(Tree* t, const BufferPlan& plan) {
  return mk::New(
      t, mk::Id(t, plan.shared ? "SharedArrayBuffer" : "ArrayBuffer"),
      {mk::Num(t, static_cast<double>(plan.length)),
       mk::Object(
           t, {mk::Prop(t, "maxByteLength",
                        mk::Num(t, static_cast<double>(plan.max_length)))})});
}

// A view of a buffer of `byte_length` bytes: a typed array of `kind`, or a
// DataView if `kind` is nullptr. It tracks the buffer length, or it has an
// offset and maybe a fixed length. The offset and the length fit the
// current byte length, so the constructor cannot throw. Sets *offset and
// *length (-1 for a view that tracks the length) if they are not null.
Expression* NewView(GenContext& ctx, std::string_view buffer,
                    int64_t byte_length, const TypedArrayKind* kind,
                    int64_t* offset_out = nullptr,
                    int64_t* length_out = nullptr) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const int64_t size = kind != nullptr ? kind->element_size : 1;
  std::vector<Expression*> args = {mk::Id(t, buffer)};
  int64_t offset = 0;
  int64_t length = -1;
  if (!r.Chance(kResizableBufferTrackingChance())) {
    offset = size * r.Range(0, byte_length / size);
    args.push_back(mk::Num(t, static_cast<double>(offset)));
    if (r.Chance(kResizableBufferFixedLengthChance())) {
      length = r.Range(0, (byte_length - offset) / size);
      args.push_back(mk::Num(t, static_cast<double>(length)));
    }
  }
  if (offset_out != nullptr) {
    *offset_out = offset;
  }
  if (length_out != nullptr) {
    *length_out = length;
  }
  return mk::New(t, mk::Id(t, kind != nullptr ? kind->name : "DataView"),
                 std::move(args));
}

// A resizable ArrayBuffer or a growable SharedArrayBuffer with a typed
// array view, after Fuzzilli's ResizableArrayBufferGenerator and
// GrowableSharedArrayBufferGenerator (CodeGenerators.swift:3459, :3481).
bool GenResizableBufferDecl(GenContext& ctx, CodeGen& cg, Statements* out) {
  Tree* t = T(ctx);
  const BufferPlan plan = RandomBufferPlan(ctx);
  Expression* buffer = NewResizableBuffer(t, plan);
  Push(out, cg.Declare(buffer, Builder::GuessType(buffer),
                       plan.shared ? "gsab" : "rab"));
  std::string_view name = LastDeclared(ctx);
  Expression* view =
      NewView(ctx, name, plan.length, &RandomTypedArrayKind(ctx));
  Push(out, cg.Declare(view, JsType::kTypedArray, "ta"));
  return true;
}

// --- special values (SpecialValueDecl, CodeGen::Value) ----------------------

REDLINE_TUNABLE(kSpecialValueChance, "codegen.value.special_chance", 0.004, 0,
                1,
                "probability that CodeGen::Value() of a number, a string or "
                "an unknown type is a native call that makes a special value");
REDLINE_TUNABLE(kSpecialValueSliceMaxOffset,
                "generators.SpecialValueDecl.slice_max_offset", 5, 1, 12,
                "largest offset of %ConstructSlicedString (Fuzzilli: 1-5)");

// A string long enough for the string shape natives: they return
// undefined for short strings. One-byte and two-byte, as in Fuzzilli.
Expression* LongString(GenContext& ctx) {
  return mk::Str(T(ctx), ctx.rng().Coin() ? "Long enough 1-byte string"
                                          : "Long eno\xC2\xB5gh 2-byte string");
}

// A string operand for %ConstructConsString and its relatives.
Expression* StringOperand(GenContext& ctx, CodeGen& cg) {
  if (ctx.rng().Coin()) {
    if (Identifier* id = VariableOfTypes(ctx, {JsType::kString})) {
      return id;
    }
  }
  return ctx.rng().Coin() ? LongString(ctx) : cg.String();
}

// A number made by a native: the hole NaN, the undefined NaN, or a heap
// number that could be a Smi (V8CommonProfile.swift:308-321).
Expression* SpecialNumber(GenContext& ctx, CodeGen& cg) {
  Tree* t = T(ctx);
  switch (ctx.rng().Index(3)) {
    case 0:
      return mk::Native(t, "GetHoleNaN", {});
    case 1:
      return mk::Native(t, "GetUndefinedNaN", {});
    default: {
      Expression* value = VariableOfTypes(ctx, {JsType::kNumber});
      if (value == nullptr) {
        value = ctx.rng().Coin() ? cg.Integer() : cg.Number();
      }
      return mk::Native(t, "AllocateHeapNumberWithValue", {value});
    }
  }
}

// A string of a special shape: cons, sliced, internalized or thin, after
// Fuzzilli's StringShapeGenerator (V8CommonProfile.swift:323).
Expression* SpecialString(GenContext& ctx, CodeGen& cg) {
  Tree* t = T(ctx);
  switch (ctx.rng().Index(4)) {
    case 0:
      return mk::Native(t, "ConstructConsString",
                        {StringOperand(ctx, cg), LongString(ctx)});
    case 1:
      return mk::Native(
          t, "ConstructSlicedString",
          {LongString(ctx),
           mk::Num(t, static_cast<double>(ctx.rng().Range(
                          1, kSpecialValueSliceMaxOffset.AsInt())))});
    case 2:
      return mk::Native(t, "ConstructInternalizedString",
                        {StringOperand(ctx, cg)});
    default:
      return mk::Native(t, "ConstructThinString", {LongString(ctx)});
  }
}

// A Wasm GC object (V8CommonProfile.swift:290-296). It is opaque: loads
// give undefined, and stores and conversions throw.
Expression* SpecialObject(GenContext& ctx) {
  return mk::Native(T(ctx), ctx.rng().Coin() ? "WasmStruct" : "WasmArray", {});
}

REDLINE_TUNABLE(kSpecialValueDeclObjectChance,
                "generators.SpecialValueDecl.object_chance", 0.4, 0, 1,
                "probability that SpecialValueDecl makes a Wasm struct or "
                "array");
REDLINE_TUNABLE(kSpecialValueDeclStringChance,
                "generators.SpecialValueDecl.string_chance", 0.4, 0, 1,
                "probability that a special value that is not an object is a "
                "string rather than a number");

// `let v = %Native(...)` of a special value. These natives cannot throw
// with --fuzzing.
bool GenSpecialValueDecl(GenContext& ctx, CodeGen& cg, Statements* out) {
  CannotThrowScope safe(ctx);
  Random& r = ctx.rng();
  if (r.Chance(kSpecialValueDeclObjectChance())) {
    // Unknown type: generators that store into objects must not pick it.
    Push(out, cg.Declare(SpecialObject(ctx), JsType::kUnknown, "w"));
  } else if (r.Chance(kSpecialValueDeclStringChance())) {
    Push(out, cg.Declare(SpecialString(ctx, cg), JsType::kString, "s"));
  } else {
    Push(out, cg.Declare(SpecialNumber(ctx, cg), JsType::kNumber));
  }
  return true;
}

}  // namespace

Expression* MaybeSpecialValue(CodeGen& cg, JsType type) {
  GenContext& ctx = cg.ctx();
  if (type != JsType::kNumber && type != JsType::kString &&
      type != JsType::kUnknown) {
    return nullptr;
  }
  if (!ctx.rng().Chance(kSpecialValueChance())) {
    return nullptr;
  }
  switch (type) {
    case JsType::kNumber:
      return SpecialNumber(ctx, cg);
    case JsType::kString:
      return SpecialString(ctx, cg);
    default:
      switch (ctx.rng().Index(3)) {
        case 0:
          return SpecialNumber(ctx, cg);
        case 1:
          return SpecialString(ctx, cg);
        default:
          // Opaque objects make conversions throw: not in code that must
          // not throw.
          return ctx.cannot_throw ? SpecialNumber(ctx, cg) : SpecialObject(ctx);
      }
  }
}

namespace {

// --- ResizableBuffer --------------------------------------------------------

REDLINE_TUNABLE(kResizableBufferReuseChance,
                "generators.ResizableBuffer.reuse_chance", 0.3, 0, 1,
                "probability to act on a visible resizable buffer instead of "
                "a new one");
REDLINE_TUNABLE(kResizableBufferMaxViews,
                "generators.ResizableBuffer.max_views", 3, 1, 8,
                "most views of the buffer");
REDLINE_TUNABLE(kResizableBufferDataViewChance,
                "generators.ResizableBuffer.data_view_chance", 0.3, 0, 1,
                "probability that a view is a DataView");
REDLINE_TUNABLE(kResizableBufferMinOps, "generators.ResizableBuffer.min_ops", 2,
                1, 32, "fewest operations on the views and the buffer");
REDLINE_TUNABLE(kResizableBufferMaxOps, "generators.ResizableBuffer.max_ops", 6,
                1, 32, "most operations on the views and the buffer");
REDLINE_TUNABLE(kResizableBufferResizeChance,
                "generators.ResizableBuffer.resize_chance", 0.3, 0, 1,
                "probability that an operation resizes or grows the buffer");
REDLINE_TUNABLE(kResizableBufferBuiltinChance,
                "generators.ResizableBuffer.builtin_chance", 0.3, 0, 1,
                "probability that an operation calls a TypedArray builtin");
REDLINE_TUNABLE(kResizableBufferJitChance,
                "generators.ResizableBuffer.jit_chance", 0.25, 0, 1,
                "probability that the element accesses are in a function that "
                "is optimized, then called after a resize");
REDLINE_TUNABLE(kResizableBufferEdgeIndexChance,
                "generators.ResizableBuffer.edge_index_chance", 0.5, 0, 1,
                "probability that an element access uses length - 1 or length "
                "instead of an index inside the view");
REDLINE_TUNABLE(kResizableBufferLittleEndianChance,
                "generators.ResizableBuffer.little_endian_chance", 0.5, 0, 1,
                "probability that a DataView access passes littleEndian");

// A view that the generator made, and what it knows about it.
struct ViewInfo {
  std::string_view name;
  const TypedArrayKind* kind = nullptr;  // nullptr for a DataView
  int64_t offset = 0;
  int64_t length = -1;  // elements (bytes of a DataView), -1 if it tracks
};

int64_t ElementSize(const ViewInfo& view) {
  return view.kind != nullptr ? view.kind->element_size : 1;
}

// True if the view is inside a buffer of `byte_length` bytes. Methods of a
// view that is out of bounds throw a TypeError.
bool InBounds(const ViewInfo& view, int64_t byte_length) {
  if (view.offset > byte_length) {
    return false;
  }
  return view.length < 0 ||
         view.offset + view.length * ElementSize(view) <= byte_length;
}

// The current number of elements (or bytes of a DataView) of the view.
int64_t CurrentLength(const ViewInfo& view, int64_t byte_length) {
  if (!InBounds(view, byte_length)) {
    return 0;
  }
  if (view.length >= 0) {
    return view.length;
  }
  return (byte_length - view.offset) / ElementSize(view);
}

// A value for an element of `view`: a BigInt for the BigInt arrays. Never
// a symbol or an object, so the conversion of the value cannot throw.
Expression* ElementValue(GenContext& ctx, CodeGen& cg, const ViewInfo& view) {
  const bool bigint = view.kind != nullptr && view.kind->bigint;
  if (ctx.rng().Coin()) {
    if (Identifier* id = VariableOfTypes(
            ctx, {bigint ? JsType::kBigInt : JsType::kNumber})) {
      return id;
    }
  }
  return bigint ? cg.BigInt() : cg.Number();
}

// An index of `view`: inside the view, `length - 1` or `length`.
Expression* ElementIndex(GenContext& ctx, const ViewInfo& view,
                         int64_t current) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  if (r.Chance(kResizableBufferEdgeIndexChance())) {
    Expression* length = mk::Member(t, mk::Id(t, view.name), "length");
    return r.Coin() ? static_cast<Expression*>(
                          mk::Binary(t, BinaryOp::kSub, length, mk::Num(t, 1)))
                    : length;
  }
  return mk::Num(
      t, static_cast<double>(r.Range(0, std::max<int64_t>(0, current - 1))));
}

// The element types of the DataView get and set methods.
struct DataViewType {
  const char* name;
  int size;
  bool bigint;
};

constexpr DataViewType kDataViewTypes[] = {
    {"Int8", 1, false},    {"Uint8", 1, false},    {"Int16", 2, false},
    {"Uint16", 2, false},  {"Float16", 2, false},  {"Int32", 4, false},
    {"Uint32", 4, false},  {"Float32", 4, false},  {"Float64", 8, false},
    {"BigInt64", 8, true}, {"BigUint64", 8, true},
};

// The state of the buffer that ResizableBuffer acts on.
struct BufferState {
  std::string_view name;
  bool shared = false;
  int64_t length = -1;  // the current byteLength, -1 if not known
  int64_t max_length = -1;
};

// Appends `statements`, in `try { ... } catch (e) {}` if `guard`.
void PushMaybeGuarded(GenContext& ctx, Statements* out, Statements statements,
                      bool guard) {
  if (!guard) {
    for (Statement* s : statements) {
      Push(out, s);
    }
    return;
  }
  Push(out,
       mk::Try(T(ctx), std::move(statements), ctx.builder.FreshName("e"), {}));
}

// `buffer.resize(n)`, or `buffer.grow(n)` for a SharedArrayBuffer. If the
// state is known, n is a length that the buffer accepts: shrink or grow
// within maxByteLength, and only grow a SharedArrayBuffer. Fuzzilli:
// ResizableBufferResizeGenerator and GrowableSharedBufferGrowGenerator
// (CodeGenerators.swift:3637, :3653).
void ResizeOp(GenContext& ctx, BufferState* buffer, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const bool known = buffer->length >= 0;
  int64_t length = 0;
  if (known) {
    // At most max_length bytes more: a huge length is slow to fill.
    const int64_t low = buffer->shared ? buffer->length : 0;
    length = r.Range(
        low, std::min(buffer->max_length,
                      buffer->length + kResizableBufferMaxLength.AsInt()));
  } else {
    length = RandomSize(r, ctx.env, kResizableBufferMaxLength.AsInt());
  }
  Expression* call = mk::CallMethod(t, mk::Id(t, buffer->name),
                                    buffer->shared ? "grow" : "resize",
                                    {mk::Num(t, static_cast<double>(length))});
  PushMaybeGuarded(ctx, out, {mk::ExprStmt(t, call)}, !known);
  buffer->length = known ? length : -1;
}

// `x = ta[i]` or `ta[i] = v`. Element accesses do not throw, also out of
// bounds.
void ElementOp(GenContext& ctx, CodeGen& cg, const ViewInfo& view,
               const BufferState& buffer, Statements* out) {
  Tree* t = T(ctx);
  const int64_t current =
      buffer.length >= 0 ? CurrentLength(view, buffer.length) : 0;
  Expression* element =
      mk::Index(t, mk::Id(t, view.name), ElementIndex(ctx, view, current));
  if (ctx.rng().Coin()) {
    Push(out, cg.Declare(element, view.kind->bigint ? JsType::kUnknown
                                                    : JsType::kNumber));
  } else {
    Push(out, cg.Stmt(mk::Assign(t, element, ElementValue(ctx, cg, view))));
  }
}

// `dv.getInt16(offset, le)` or `dv.setFloat64(offset, v, le)` with an
// offset of the right type. Unguarded if the access is in bounds.
void DataViewOp(GenContext& ctx, CodeGen& cg, const ViewInfo& view,
                const BufferState& buffer, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const DataViewType& type = kDataViewTypes[r.Index(sizeof(kDataViewTypes) /
                                                    sizeof(kDataViewTypes[0]))];
  const int64_t current =
      buffer.length >= 0 ? CurrentLength(view, buffer.length) : -1;
  const bool in_bounds = current >= type.size;
  Expression* offset = nullptr;
  if (in_bounds && !r.Chance(kResizableBufferEdgeIndexChance())) {
    offset = mk::Num(t, static_cast<double>(r.Range(0, current - type.size)));
  } else {
    // The last element, or one byte past it.
    offset = mk::Binary(
        t, BinaryOp::kSub, mk::Member(t, mk::Id(t, view.name), "byteLength"),
        mk::Num(t, static_cast<double>(type.size - (r.Coin() ? 0 : 1))));
  }
  const bool guard = !in_bounds || !offset->Is<NumericLiteral>();
  std::vector<Expression*> args = {offset};
  const bool is_set = r.Coin();
  if (is_set) {
    args.push_back(type.bigint ? cg.BigInt() : cg.Number());
  }
  if (r.Chance(kResizableBufferLittleEndianChance())) {
    args.push_back(cg.Boolean());
  }
  Expression* call =
      mk::CallMethod(t, mk::Id(t, view.name),
                     std::string(is_set ? "set" : "get") + type.name, args);
  if (is_set) {
    PushMaybeGuarded(ctx, out, {mk::ExprStmt(t, call)}, guard);
  } else if (guard) {
    for (Statement* s : cg.DeclareGuarded(call, JsType::kUnknown)) {
      Push(out, s);
    }
  } else {
    Push(out, cg.Declare(call, JsType::kUnknown));
  }
}

// A TypedArray builtin on `view`. It throws for a view that is out of
// bounds, and set() and with() also throw for some indices and lengths:
// these calls are guarded.
void BuiltinOp(GenContext& ctx, CodeGen& cg, const ViewInfo& view,
               const BufferState& buffer, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const bool in_bounds = buffer.length >= 0 && InBounds(view, buffer.length);
  Identifier* v = mk::Id(t, view.name);
  std::string_view method =
      r.Pick({"fill", "set", "subarray", "slice", "indexOf", "includes", "at",
              "copyWithin", "sort", "reverse", "join", "toReversed", "with",
              "lastIndexOf", "entries"});
  std::vector<Expression*> args;
  bool guard = !in_bounds;
  if (method == "fill" || method == "indexOf" || method == "includes" ||
      method == "lastIndexOf") {
    args.push_back(ElementValue(ctx, cg, view));
  } else if (method == "set") {
    args.push_back(mk::Array(t, {ElementValue(ctx, cg, view)}));
    guard = true;
  } else if (method == "with") {
    args.push_back(cg.Integer());
    args.push_back(ElementValue(ctx, cg, view));
    guard = true;
  } else if (method == "subarray" || method == "slice" ||
             method == "copyWithin") {
    args.push_back(cg.Integer());
    args.push_back(cg.Integer());
  } else if (method == "at") {
    args.push_back(mk::Num(t, -1));
  }
  Expression* call = mk::CallMethod(t, v, method, std::move(args));
  if (method == "entries") {
    call = mk::Array(t, {mk::Spread(t, call)});  // run the iterator
  }
  if (guard) {
    for (Statement* s : cg.DeclareGuarded(call, JsType::kUnknown)) {
      Push(out, s);
    }
  } else {
    Push(out, cg.Declare(call, JsType::kUnknown));
  }
}

REDLINE_TUNABLE(kResizableBufferJitWarmupCalls,
                "generators.ResizableBuffer.jit_warmup_calls", 2, 1, 16,
                "calls of the access function before it is optimized");
REDLINE_TUNABLE(kResizableBufferTurbofanChance,
                "generators.ResizableBuffer.turbofan_chance", 0.6, 0, 1,
                "probability of %OptimizeFunctionOnNextCall rather than "
                "%OptimizeMaglevOnNextCall for the access function");

// A function that loads and stores elements of its view argument. It is
// warmed up on `view`, optimized and called. Then the buffer gets a new
// length, and the optimized code runs again.
void JitAccess(GenContext& ctx, CodeGen& cg, const ViewInfo& view,
               BufferState* buffer, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view fn = ctx.builder.FreshName("f");
  std::string_view v = ctx.builder.FreshName("a");
  std::string_view i = ctx.builder.FreshName("a");
  std::string_view x = ctx.builder.FreshName("a");
  Expression* last = mk::Binary(
      t, BinaryOp::kSub, mk::Member(t, mk::Id(t, v), "length"), mk::Num(t, 1));
  Statements body;
  if (r.Coin()) {
    body.push_back(cg.Stmt(
        mk::Assign(t, mk::Index(t, mk::Id(t, v), mk::Id(t, i)), mk::Id(t, x))));
  }
  if (r.Coin()) {
    body.push_back(
        cg.Stmt(mk::Assign(t, mk::Index(t, mk::Id(t, v), last), mk::Id(t, x))));
    last = mk::Binary(t, BinaryOp::kSub, mk::Member(t, mk::Id(t, v), "length"),
                      mk::Num(t, 1));
  }
  body.push_back(mk::Return(t, r.Coin()
                                   ? mk::Index(t, mk::Id(t, v), mk::Id(t, i))
                                   : mk::Index(t, mk::Id(t, v), last)));
  Push(out, mk::FunctionDecl(t, fn, {v, i, x}, std::move(body)));
  ctx.Declare(fn, JsType::kFunction);
  const int64_t current =
      buffer->length >= 0 ? CurrentLength(view, buffer->length) : 0;
  auto call = [&]() {
    Push(out, cg.Stmt(mk::Call(
                  t, mk::Id(t, fn),
                  {mk::Id(t, view.name), ElementIndex(ctx, view, current),
                   ElementValue(ctx, cg, view)})));
  };
  Push(out, cg.Stmt(mk::Native(t, "PrepareFunctionForOptimization",
                               {mk::Id(t, fn)})));
  for (int k = 0; k < kResizableBufferJitWarmupCalls.AsInt(); k++) {
    call();
  }
  Push(out, cg.Stmt(mk::Native(t,
                               r.Chance(kResizableBufferTurbofanChance())
                                   ? "OptimizeFunctionOnNextCall"
                                   : "OptimizeMaglevOnNextCall",
                               {mk::Id(t, fn)})));
  call();
  ResizeOp(ctx, buffer, out);
  call();
}

// A resizable buffer (new, or visible) with 1-3 views of all typed array
// kinds and DataView. Then element and DataView accesses, resizes, accesses
// at the edges and TypedArray builtins (gap report, item 4). Fuzzilli acts
// on such buffers with TypedArrayFromBufferGenerator,
// DataViewFromBufferGenerator, TypedArrayLastIndexGenerator (:460-485) and
// the resize generators (:3637, :3653).
bool GenResizableBuffer(GenContext& ctx, CodeGen& cg, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  BufferState buffer;
  bool shared = false;
  std::string_view visible;
  if (r.Chance(kResizableBufferReuseChance())) {
    visible = VisibleResizableBuffer(ctx, &shared);
  }
  if (!visible.empty()) {
    buffer.name = visible;
    buffer.shared = shared;
  } else {
    const BufferPlan plan = RandomBufferPlan(ctx);
    Expression* init = NewResizableBuffer(t, plan);
    Push(out, cg.Declare(init, Builder::GuessType(init),
                         plan.shared ? "gsab" : "rab"));
    buffer.name = LastDeclared(ctx);
    buffer.shared = plan.shared;
    buffer.length = plan.length;
    buffer.max_length = plan.max_length;
  }
  std::vector<ViewInfo> views;
  const int count = 1 + r.SmallCount(kResizableBufferMaxViews.AsInt() - 1);
  for (int k = 0; k < count; k++) {
    ViewInfo view;
    if (!r.Chance(kResizableBufferDataViewChance())) {
      view.kind = &RandomTypedArrayKind(ctx);
    }
    Expression* init = nullptr;
    if (buffer.length >= 0) {
      init = NewView(ctx, buffer.name, buffer.length, view.kind, &view.offset,
                     &view.length);
    } else {
      // The length is not known: a view that tracks it is always valid.
      init = mk::New(t, mk::Id(t, view.kind ? view.kind->name : "DataView"),
                     {mk::Id(t, buffer.name)});
    }
    Push(out, cg.Declare(init, Builder::GuessType(init),
                         view.kind != nullptr ? "ta" : "dv"));
    view.name = LastDeclared(ctx);
    views.push_back(view);
  }
  std::vector<const ViewInfo*> typed;
  for (const ViewInfo& view : views) {
    if (view.kind != nullptr) {
      typed.push_back(&view);
    }
  }
  if (!typed.empty() && r.Chance(kResizableBufferJitChance())) {
    JitAccess(ctx, cg, *r.Pick(typed), &buffer, out);
  }
  const int ops =
      static_cast<int>(r.Range(kResizableBufferMinOps.AsInt(),
                               std::max(kResizableBufferMinOps.AsInt(),
                                        kResizableBufferMaxOps.AsInt())));
  for (int k = 0; k < ops; k++) {
    if (r.Chance(kResizableBufferResizeChance())) {
      ResizeOp(ctx, &buffer, out);
      continue;
    }
    const ViewInfo& view = r.Pick(views);
    if (view.kind == nullptr) {
      DataViewOp(ctx, cg, view, buffer, out);
    } else if (r.Chance(kResizableBufferBuiltinChance())) {
      BuiltinOp(ctx, cg, view, buffer, out);
    } else {
      ElementOp(ctx, cg, view, buffer, out);
    }
  }
  return true;
}

// --- using, for-of with using, DisposableStack ------------------------------

REDLINE_TUNABLE(kUsingReuseChance, "generators.UsingDecl.reuse_chance", 0.5, 0,
                1,
                "probability that `using` binds a visible disposable value "
                "when one exists");
REDLINE_TUNABLE(kUsingNullishChance, "generators.UsingDecl.nullish_chance",
                0.15, 0, 1,
                "probability that `using` binds null or undefined, which it "
                "accepts");
REDLINE_TUNABLE(kUsingStackChance, "generators.UsingDecl.stack_chance", 0.15, 0,
                1,
                "probability that `using` binds a new DisposableStack or "
                "AsyncDisposableStack");
REDLINE_TUNABLE(kUsingMaxStatements, "generators.UsingDecl.max_statements", 2,
                1, 16,
                "most generated statements in the block before the disposal");
REDLINE_TUNABLE(kUsingGuardChance, "generators.UsingDecl.guard_chance", 0.3, 0,
                1,
                "probability that the block is in a try statement, so that an "
                "error of the dispose method does not end the program");

// A value for `using` (or `await using`): a visible disposable, a new one,
// a new DisposableStack, null or undefined.
Expression* UsingOperand(GenContext& ctx, CodeGen& cg, bool is_await) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  if (r.Chance(kUsingNullishChance())) {
    return cg.Nullish();
  }
  if (r.Chance(kUsingReuseChance())) {
    std::vector<std::string_view> names = VisibleDisposables(ctx, is_await);
    if (!names.empty()) {
      return mk::Id(t, r.Pick(names));
    }
  }
  if (r.Chance(kUsingStackChance())) {
    return mk::New(t,
                   mk::Id(t, is_await && r.Coin() ? "AsyncDisposableStack"
                                                  : "DisposableStack"),
                   {});
  }
  const bool async_symbol = is_await && r.Coin();
  FunctionExpression* method =
      DisposeMethod(ctx, cg, async_symbol && r.Coin(), /*in_class=*/false);
  return mk::Object(
      t, {ComputedMethod(
             t, WellKnownSymbol(t, async_symbol ? "asyncDispose" : "dispose"),
             method)});
}

// `{ using x = <disposable>; ... }`: code with x visible, then the disposal
// at the end of the block. After Fuzzilli's DisposableObjVariableGenerator
// and its relatives (CodeGenerators.swift:825-852), which use real
// disposables.
bool UsingBlock(GenContext& ctx, CodeGen& cg, Statements* out, bool is_await) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* init = UsingOperand(ctx, cg, is_await);
  std::string_view name =
      ctx.builder.FreshName(is_await ? "await_using_var" : "using_var");
  GenContext inner = ctx.Inner(false, false);
  inner.Declare(name, JsType::kObject, /*is_constant=*/true);
  CodeGen icg(inner);
  Statements body = {mk::Declare(
      t, is_await ? VariableKind::kAwaitUsing : VariableKind::kUsing, name,
      init)};
  for (Statement* s :
       icg.Body(inner, 1 + r.SmallCount(kUsingMaxStatements.AsInt() - 1))) {
    body.push_back(s);
  }
  if (r.Chance(kUsingGuardChance())) {
    Push(out, mk::Try(t, std::move(body), ctx.builder.FreshName("e"), {}));
  } else {
    Push(out, mk::Block(t, std::move(body)));
  }
  return true;
}

REDLINE_TUNABLE(kForOfUsingMaxValues, "generators.ForOfUsing.max_values", 3, 1,
                16, "most disposable values that the loop iterates over");
REDLINE_TUNABLE(kForOfUsingGeneratorChance,
                "generators.ForOfUsing.generator_chance", 0.5, 0, 1,
                "probability that an async loop iterates over an async "
                "generator that yields the values instead of an array");
REDLINE_TUNABLE(kForOfUsingAwaitUsingChance,
                "generators.ForOfUsing.await_using_chance", 0.5, 0, 1,
                "probability of `await using` in an async context");

// `for (using x of [d1, d2])`, `for (await using x of ...)` and
// `for await (using x of g())` over an async generator that yields
// disposables. Fuzzilli: ForOfWithUsingLoopGenerator and its three
// relatives (CodeGenerators.swift:2912-2948), DisposableArrayGenerator and
// AsyncIterableDisposableGenerator (:3794-3821).
bool GenForOfUsing(GenContext& ctx, CodeGen& cg, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const bool in_async = ctx.syntax.in_async && !ctx.syntax.in_static_block;
  const bool await_using = in_async && r.Chance(kForOfUsingAwaitUsingChance());
  const bool for_await = in_async && r.Coin();
  const int count = 1 + r.SmallCount(kForOfUsingMaxValues.AsInt() - 1);
  std::vector<Expression*> values;
  for (int k = 0; k < count; k++) {
    values.push_back(UsingOperand(ctx, cg, await_using || for_await));
  }
  Expression* iterable = nullptr;
  if (for_await && r.Chance(kForOfUsingGeneratorChance())) {
    std::string_view gen = ctx.builder.FreshName("g");
    Statements body;
    for (Expression* value : values) {
      body.push_back(cg.Stmt(mk::Yield(t, value)));
    }
    Push(out, mk::FunctionDecl(t, gen, {}, std::move(body), /*is_async=*/true,
                               /*is_generator=*/true));
    ctx.Declare(gen, JsType::kFunction);
    iterable = mk::Call(t, mk::Id(t, gen), {});
  } else {
    iterable = mk::Array(t, std::move(values));
  }
  std::string_view name = ctx.builder.FreshName("e");
  GenContext inner = ctx.Inner(true, false);
  inner.Declare(name, JsType::kObject, /*is_constant=*/true);
  CodeGen icg(inner);
  Statements body =
      icg.Body(inner, 1 + r.SmallCount(kUsingMaxStatements.AsInt() - 1));
  Statement* loop = mk::ForOf(
      t, await_using ? VariableKind::kAwaitUsing : VariableKind::kUsing, name,
      iterable, mk::Block(t, std::move(body)), for_await);
  if (r.Chance(kUsingGuardChance())) {
    Push(out, mk::Try(t, {loop}, ctx.builder.FreshName("e"), {}));
  } else {
    Push(out, loop);
  }
  return true;
}

REDLINE_TUNABLE(kDisposableStackMinOps, "generators.DisposableStackOps.min_ops",
                1, 1, 16, "fewest use, adopt and defer calls");
REDLINE_TUNABLE(kDisposableStackMaxOps, "generators.DisposableStackOps.max_ops",
                4, 1, 16, "most use, adopt and defer calls");
REDLINE_TUNABLE(kDisposableStackThrowChance,
                "generators.DisposableStackOps.throw_chance", 0.3, 0, 1,
                "probability that a callback throws, so that the disposal "
                "makes SuppressedError chains");
REDLINE_TUNABLE(kDisposableStackGeneratedChance,
                "generators.DisposableStackOps.generated_chance", 0.3, 0, 1,
                "probability that a callback that does not throw has a "
                "generated body");
REDLINE_TUNABLE(kDisposableStackMoveChance,
                "generators.DisposableStackOps.move_chance", 0.3, 0, 1,
                "probability of move() before the disposal");
REDLINE_TUNABLE(kDisposableStackDisposeChance,
                "generators.DisposableStackOps.dispose_chance", 0.9, 0, 1,
                "probability that the stack is disposed at the end");

// A callback for adopt() or defer(): it throws, runs generated code, or
// stores a value. Sets *may_throw if it can throw.
Expression* StackCallback(GenContext& ctx, CodeGen& cg, bool is_async,
                          int params, bool* may_throw) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  GenContext nested = ctx.Nested(true, true, is_async, false);
  std::vector<std::string_view> names;
  for (int k = 0; k < params; k++) {
    names.push_back(ctx.builder.FreshName("a"));
    nested.Declare(names.back(), JsType::kUnknown);
  }
  CodeGen inner(nested);
  Statements body;
  if (r.Chance(kDisposableStackThrowChance())) {
    body.push_back(mk::Throw(t, PlainValue(ctx, cg)));
    *may_throw = true;
  } else if (r.Chance(kDisposableStackGeneratedChance())) {
    body = inner.Body(
        nested, 1 + r.SmallCount(kDisposableDeclMaxStatements.AsInt() - 1));
    *may_throw = true;
  } else {
    Identifier* target = inner.MutableVariable();
    if (target != nullptr) {
      body.push_back(inner.Stmt(mk::Assign(t, target, PlainValue(ctx, cg))));
    }
  }
  return mk::Arrow(t, std::move(names), std::move(body), is_async);
}

// A DisposableStack (or an AsyncDisposableStack in async code) with use(),
// adopt() and defer() calls, maybe move(), then dispose() or
// disposeAsync(). Throwing callbacks make SuppressedError chains (gap
// report, item 5).
bool GenDisposableStackOps(GenContext& ctx, CodeGen& cg, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const bool is_async =
      ctx.syntax.in_async && !ctx.syntax.in_static_block && r.Coin();
  Push(out, cg.Declare(mk::New(t,
                               mk::Id(t, is_async ? "AsyncDisposableStack"
                                                  : "DisposableStack"),
                               {}),
                       JsType::kObject, "st"));
  std::string_view stack = LastDeclared(ctx);
  bool may_throw = false;
  const int ops =
      static_cast<int>(r.Range(kDisposableStackMinOps.AsInt(),
                               std::max(kDisposableStackMinOps.AsInt(),
                                        kDisposableStackMaxOps.AsInt())));
  for (int k = 0; k < ops; k++) {
    Expression* call = nullptr;
    switch (r.Index(3)) {
      case 0:
        // use() takes a disposable, null or undefined. It returns it.
        call = mk::CallMethod(t, mk::Id(t, stack), "use",
                              {UsingOperand(ctx, cg, is_async)});
        may_throw = true;  // the dispose method may throw
        break;
      case 1:
        call =
            mk::CallMethod(t, mk::Id(t, stack), "adopt",
                           {PlainValue(ctx, cg),
                            StackCallback(ctx, cg, is_async, 1, &may_throw)});
        break;
      default:
        call =
            mk::CallMethod(t, mk::Id(t, stack), "defer",
                           {StackCallback(ctx, cg, is_async, 0, &may_throw)});
        break;
    }
    Push(out, cg.Stmt(call));
  }
  if (r.Chance(kDisposableStackMoveChance())) {
    Push(out, cg.Declare(mk::CallMethod(t, mk::Id(t, stack), "move", {}),
                         JsType::kObject, "st"));
    stack = LastDeclared(ctx);
  }
  if (r.Chance(kDisposableStackDisposeChance())) {
    Expression* dispose = mk::CallMethod(
        t, mk::Id(t, stack), is_async ? "disposeAsync" : "dispose", {});
    if (is_async) {
      dispose = mk::Await(t, dispose);
    }
    if (may_throw) {
      // The error is a SuppressedError when several callbacks threw.
      std::string_view error = ctx.builder.FreshName("e");
      MemberExpression* suppressed =
          mk::Member(t, mk::Id(t, error), "suppressed");
      suppressed->optional = true;  // the thrown value can be null
      Push(out, mk::Try(t, {cg.Stmt(dispose)}, error,
                        {cg.Stmt(mk::Chain(t, suppressed))}));
    } else {
      Push(out, cg.Stmt(dispose));
    }
  }
  Push(out, cg.Declare(mk::Member(t, mk::Id(t, stack), "disposed"),
                       JsType::kBoolean, "b"));
  return true;
}

// --- GC and allocation natives ----------------------------------------------

// The GC interval of %SetAllocationTimeout is a process-wide setting. It
// stays for all later programs of the d8 process (REPRL). A GC after every
// few hundred allocations makes these programs very slow. Fuzzilli uses 0.1.
REDLINE_TUNABLE(kAllocationTimeoutIntervalChance,
                "generators.GcNative.allocation_interval_chance", 0, 0, 1,
                "probability that %SetAllocationTimeout sets a GC interval "
                "instead of -1");
REDLINE_TUNABLE(kAllocationTimeoutMinInterval,
                "generators.GcNative.allocation_min_interval", 100, 1, 100000,
                "smallest allocation interval of %SetAllocationTimeout");
REDLINE_TUNABLE(kAllocationTimeoutMaxInterval,
                "generators.GcNative.allocation_max_interval", 10000, 1, 100000,
                "largest allocation interval of %SetAllocationTimeout");
REDLINE_TUNABLE(kAllocationTimeoutSmallMax,
                "generators.GcNative.allocation_small_timeout", 10, 0, 1000,
                "largest timeout of %SetAllocationTimeout half of the time");
REDLINE_TUNABLE(kAllocationTimeoutMax,
                "generators.GcNative.allocation_max_timeout", 100, 0, 1000,
                "largest timeout of %SetAllocationTimeout");
// %SetDispatchTableGCInterval(n) makes a full GC after every n new dispatch
// handles. It is also a process-wide setting that stays for all later
// programs of the d8 process. Fuzzilli uses 1-10, which makes these
// programs very slow.
REDLINE_TUNABLE(kDispatchTableGcMinInterval,
                "generators.GcNative.dispatch_table_min_interval", 100, 1,
                100000, "smallest interval of %SetDispatchTableGCInterval");
REDLINE_TUNABLE(kDispatchTableGcMaxInterval,
                "generators.GcNative.dispatch_table_max_interval", 1000, 1,
                100000, "largest interval of %SetDispatchTableGCInterval");
REDLINE_TUNABLE(kGcNativeInFunctionChance,
                "generators.GcNative.in_function_chance", 0.1, 0, 1,
                "probability that JitOptimize and LazyDeopt put a GC native "
                "into the function that they optimize");

// `%SetAllocationTimeout(interval, timeout)` with Fuzzilli's shapes
// (V8CommonProfile.swift:117): interval -1 most of the time, small timeouts.
std::vector<Expression*> AllocationTimeoutArguments(GenContext& ctx) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const int64_t interval =
      r.Chance(kAllocationTimeoutIntervalChance())
          ? r.Range(kAllocationTimeoutMinInterval.AsInt(),
                    std::max(kAllocationTimeoutMinInterval.AsInt(),
                             kAllocationTimeoutMaxInterval.AsInt()))
          : -1;
  const int64_t timeout =
      r.Range(0, r.Coin() ? kAllocationTimeoutSmallMax.AsInt()
                          : kAllocationTimeoutMax.AsInt());
  return {mk::Num(t, static_cast<double>(interval)),
          mk::Num(t, static_cast<double>(timeout))};
}

// One GC or allocation call (V8CommonProfile.swift:103-130):
// %MajorGCForCompilerTesting(), %SetAllocationTimeout(i, t),
// %SetDispatchTableGCInterval(n) or gc({type, execution}). This V8 has no
// %PretenureAllocationSite (V8CommonProfile.swift:302).
Expression* GcNativeCall(GenContext& ctx) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  switch (r.Index(4)) {
    case 0:
      return mk::Native(t, "MajorGCForCompilerTesting", {});
    case 1:
      return mk::Native(t, "SetAllocationTimeout",
                        AllocationTimeoutArguments(ctx));
    case 2:
      return mk::Native(
          t, "SetDispatchTableGCInterval",
          {mk::Num(t, static_cast<double>(r.Range(
                          kDispatchTableGcMinInterval.AsInt(),
                          std::max(kDispatchTableGcMinInterval.AsInt(),
                                   kDispatchTableGcMaxInterval.AsInt()))))});
    default: {
      std::vector<Node*> options;
      if (r.Coin()) {
        options.push_back(
            mk::Prop(t, "type", mk::Str(t, r.Coin() ? "major" : "minor")));
      }
      if (r.Coin()) {
        options.push_back(
            mk::Prop(t, "execution", mk::Str(t, r.Coin() ? "sync" : "async")));
      }
      return mk::Call(t, mk::Id(t, "gc"), {mk::Object(t, std::move(options))});
    }
  }
}

bool GenGcNative(GenContext& ctx, CodeGen& cg, Statements* out) {
  Push(out, cg.Stmt(GcNativeCall(ctx)));
  return true;
}

// `%VerifyType(v)` on a visible value, after Fuzzilli's
// TurbofanVerifyTypeGenerator (V8CommonProfile.swift:232 at the comparison
// commit). It checks the type that Turbofan inferred for v.
bool GenVerifyType(GenContext& ctx, CodeGen& cg, Statements* out) {
  Identifier* value = cg.Variable();
  if (value == nullptr) {
    return false;
  }
  Push(out, cg.Stmt(mk::Native(T(ctx), "VerifyType", {value})));
  return true;
}

}  // namespace

bool NativeArgumentsFor(CodeGen& cg, std::string_view name,
                        std::vector<fast::Expression*>* args) {
  GenContext& ctx = cg.ctx();
  Tree* t = T(ctx);
  if (name == "ConstructConsString") {
    *args = {StringOperand(ctx, cg), LongString(ctx)};
  } else if (name == "ConstructSlicedString") {
    // An integer offset into a string that is long enough to slice.
    *args = {LongString(ctx),
             mk::Num(t, static_cast<double>(ctx.rng().Range(
                            1, kSpecialValueSliceMaxOffset.AsInt())))};
  } else if (name == "ConstructThinString") {
    *args = {LongString(ctx)};
  } else if (name == "ConstructInternalizedString" ||
             name == "InternalizeString" || name == "FlattenString") {
    *args = {StringOperand(ctx, cg)};
  } else if (name == "AllocateHeapNumberWithValue") {
    Expression* value = VariableOfTypes(ctx, {JsType::kNumber});
    *args = {value != nullptr ? value : cg.Number()};
  } else if (name == "SetAllocationTimeout") {
    *args = AllocationTimeoutArguments(ctx);
  } else if (name == "SetDispatchTableGCInterval") {
    *args = {mk::Num(t, static_cast<double>(ctx.rng().Range(
                            kDispatchTableGcMinInterval.AsInt(),
                            std::max(kDispatchTableGcMinInterval.AsInt(),
                                     kDispatchTableGcMaxInterval.AsInt()))))};
  } else {
    return false;
  }
  return true;
}

void MaybeInsertGcNative(GenContext& ctx, CodeGen& cg,
                         std::vector<fast::Statement*>* body) {
  if (!ctx.rng().Chance(kGcNativeInFunctionChance())) {
    return;
  }
  // Before a final return, so that the call runs.
  size_t end = body->size();
  if (end > 0 && body->back()->Is<ReturnStatement>()) {
    end--;
  }
  body->insert(
      body->begin() + static_cast<std::ptrdiff_t>(ctx.rng().Index(end + 1)),
      cg.Stmt(GcNativeCall(ctx)));
}

bool GenUsingBlock(GenContext& ctx, CodeGen& cg,
                   std::vector<fast::Statement*>* out, bool is_await) {
  return UsingBlock(ctx, cg, out, is_await);
}

const std::vector<std::string>& CannotThrowValueGenerators() {
  static const std::vector<std::string>* names = new std::vector<std::string>{
      "BuiltinInstanceDecl", "ConstructorFunctionDecl", "IterableDecl",
      "DisposableDecl",      "AsyncDisposableDecl",     "ResizableBufferDecl",
      "SpecialValueDecl"};
  return *names;
}

void AddValueGenerators(const AddGeneratorFn& add) {
  // Value declarations that cannot throw (Fuzzilli's useInPrefix
  // generators). The Prefix mutator uses them too.
  add("BuiltinInstanceDecl",
      "let o = new Map() / new WeakSet() / new Array(n) / Promise.resolve(v) "
      "/ new TypeError(m)",
      2, kNoRequirement, GenBuiltinInstanceDecl);
  add("ConstructorFunctionDecl",
      "function C(a) { this.x = a; ... } and 1-4 new C(...)", 2, kNoRequirement,
      GenConstructorFunctionDecl);
  add("IterableDecl", "let it = { [Symbol.iterator]() {...} }", 1,
      kNoRequirement, GenIterableDecl);
  add("DisposableDecl",
      "let d = { [Symbol.dispose]() {...} } or a class instance", 1, kRecursive,
      GenDisposableDecl);
  add("AsyncDisposableDecl",
      "let ad = { [Symbol.asyncDispose]() {...} } or a class instance", 1,
      kRecursive, GenAsyncDisposableDecl);
  add("ResizableBufferDecl",
      "let rab = new ArrayBuffer(n, {maxByteLength: m}) (or a growable "
      "SharedArrayBuffer) and a view",
      1, kNoRequirement, GenResizableBufferDecl);
  // Operations on the new kinds of values.
  add("ResizableBuffer",
      "resizable or growable buffer with typed array and DataView views, "
      "accesses at the edges, resize/grow, builtins, sometimes optimized",
      1, kNoRequirement, GenResizableBuffer);
  add("ForOfUsing",
      "for (using x of [d, ...]) / for (await using ...) / for await (using x "
      "of asyncGen())",
      1, kRecursive, GenForOfUsing);
  add("DisposableStackOps",
      "new DisposableStack(): use/adopt/defer, move, dispose (or the async "
      "variant)",
      1, kRecursive, GenDisposableStackOps);
  add("GcNative",
      "%MajorGCForCompilerTesting() / %SetAllocationTimeout(i, t) / "
      "%SetDispatchTableGCInterval(n) / gc({type, execution})",
      0.5, kNoRequirement, GenGcNative);
  add("VerifyType", "%VerifyType(v)", 0.3, kNoRequirement, GenVerifyType);
  add("SpecialValueDecl",
      "let v = %GetHoleNaN() / %ConstructThinString(s) / %WasmStruct() ...", 1,
      kNoRequirement, GenSpecialValueDecl);
}

}  // namespace redline

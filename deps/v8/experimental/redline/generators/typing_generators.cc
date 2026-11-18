// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/typing_generators.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/fast/printer.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

// --- helpers
// -------------------------------------------------------------------

ArgumentLists::ArgumentLists(CodeGen& cg, Maker make, int max_lists)
    : cg_(cg),
      make_(std::move(make)),
      max_lists_(static_cast<size_t>(std::max(max_lists, 1))) {}

std::vector<Expression*> ArgumentLists::Copy(
    const std::vector<Expression*>& list) {
  std::vector<Expression*> copy;
  copy.reserve(list.size());
  for (const Expression* e : list) {
    copy.push_back(cg_.tree()->Clone(e));
  }
  return copy;
}

std::vector<Expression*> ArgumentLists::Next(double reuse_chance) {
  Random& r = cg_.rng();
  if (lists_.empty() ||
      (lists_.size() < max_lists_ && !r.Chance(reuse_chance))) {
    lists_.push_back(make_());
    // The stored list stays unused in the tree: each call gets a copy.
    return Copy(lists_.back());
  }
  return Copy(lists_[r.Index(lists_.size())]);
}

std::vector<Expression*> ArgumentLists::OtherTypes() {
  std::vector<Expression*> result;
  if (lists_.empty()) {
    return result;
  }
  static const JsType kTypes[] = {
      JsType::kNumber, JsType::kString,  JsType::kBoolean, JsType::kObject,
      JsType::kArray,  JsType::kNullish, JsType::kBigInt,  JsType::kFunction};
  Random& r = cg_.rng();
  for (const Expression* arg : lists_.front()) {
    const JsType old_type = ArgumentType(cg_.ctx(), arg);
    JsType type = old_type;
    while (type == old_type) {
      type = kTypes[r.Index(sizeof(kTypes) / sizeof(kTypes[0]))];
    }
    result.push_back(cg_.Value(type, 1));
  }
  return result;
}

JsType ArgumentType(const GenContext& ctx, const Expression* argument) {
  if (auto* id = argument->As<Identifier>()) {
    for (const Variable& v : ctx.visible) {
      if (v.name == id->name) {
        return v.type;
      }
    }
  }
  return Builder::GuessType(argument);
}

REDLINE_TUNABLE(kIterableMaxLength, "generators.Iterable.max_length", 4, 1, 64,
                "most values of a new Map, Set, generator or custom iterable "
                "that a destructuring source makes");

namespace {

// `{[Symbol.iterator]: function() { let i = 0; return {next: function() {
// return {done: i >= n, value: i++}; }}; }}`: a custom iterable with a
// counter (Fuzzilli's IteratorGenerator, CodeGenerators.swift:3550).
Expression* CustomIterable(GenContext& ctx, int length) {
  Tree* t = ctx.tree();
  std::string_view counter = ctx.builder.FreshName("i");
  Expression* result = mk::Object(
      t, {mk::Prop(t, "done",
                   mk::Binary(t, BinaryOp::kGreaterThanEq, mk::Id(t, counter),
                              mk::Num(t, static_cast<double>(length)))),
          mk::Prop(
              t, "value",
              mk::Update(t, UpdateOp::kIncrement, false, mk::Id(t, counter)))});
  Expression* next = mk::Function(t, "", {}, {mk::Return(t, result)});
  Expression* iterator = mk::Object(t, {mk::Prop(t, "next", next)});
  Expression* method = mk::Function(
      t, "", {}, {mk::Let(t, counter, mk::Num(t, 0)), mk::Return(t, iterator)});
  return mk::Object(
      t, {mk::Prop(t, mk::Member(t, mk::Id(t, "Symbol"), "iterator"), method,
                   /*computed=*/true)});
}

}  // namespace

Expression* IterableSource(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  const int length = 1 + r.SmallCount(kIterableMaxLength.AsInt() - 1);
  switch (r.Index(9)) {
    case 0:
      if (Identifier* v = cg.Variable(JsType::kArray)) {
        return v;
      }
      break;
    case 1:
      if (Identifier* v = cg.Variable(JsType::kString)) {
        return v;
      }
      break;
    case 2:
      if (Identifier* v = cg.Variable(r.Coin() ? JsType::kMap : JsType::kSet)) {
        return r.Coin() ? static_cast<Expression*>(v)
                        : mk::CallMethod(
                              t, v, r.Pick({"entries", "keys", "values"}), {});
      }
      break;
    case 3:
      if (Identifier* v = cg.Variable(JsType::kTypedArray)) {
        return v;
      }
      break;
    case 4: {
      // new Map([[k, v], ...])
      std::vector<Expression*> entries;
      for (int i = 0; i < length; i++) {
        entries.push_back(mk::Array(
            t, {cg.Value(JsType::kUnknown, 1), cg.Value(JsType::kUnknown, 1)}));
      }
      return mk::New(t, mk::Id(t, "Map"), {mk::Array(t, std::move(entries))});
    }
    case 5:
      return mk::New(t, mk::Id(t, "Set"), {cg.ArrayLiteral(1)});
    case 6: {
      // A generator object of a function that yields some values.
      std::vector<Statement*> body;
      for (int i = 0; i < length; i++) {
        body.push_back(
            mk::ExprStmt(t, mk::Yield(t, cg.Value(JsType::kUnknown, 1))));
      }
      return mk::Call(t, mk::Function(t, "", {}, std::move(body), false, true),
                      {});
    }
    case 7:
      return CustomIterable(ctx, length);
    default:
      break;
  }
  return cg.ArrayLiteral(1);
}

namespace {

using Statements = std::vector<Statement*>;

// Generators share one signature. Not every generator needs every argument.
#define GEN(name)                                  \
  bool Gen##name([[maybe_unused]] GenContext& ctx, \
                 [[maybe_unused]] CodeGen& cg, Statements* out)

Tree* T(GenContext& ctx) { return ctx.tree(); }

// A visible function, or a new function declaration (appended to `out`)
// if none is visible and the depth allows one. nullptr if neither exists.
// `*is_generator` tells if the function is a new generator function.
Identifier* ExistingOrNewFunction(GenContext& ctx, CodeGen& cg, Statements* out,
                                  bool* is_generator) {
  *is_generator = false;
  if (Identifier* f = cg.Variable(JsType::kFunction)) {
    return f;
  }
  Generator* declare = ctx.generators.Find("FunctionDecl");
  if (declare == nullptr || ctx.depth >= ctx.max_depth) {
    return nullptr;
  }
  Statements decl;
  if (!declare->Generate(ctx, &decl) || decl.empty()) {
    return nullptr;
  }
  auto* fn = decl.front()->As<FunctionDeclaration>();
  if (fn == nullptr || fn->name == nullptr) {
    return nullptr;
  }
  // Only the declaration: the loop makes the calls.
  out->push_back(fn);
  *is_generator = fn->is_generator;
  return mk::Id(T(ctx), fn->name->name);
}

// --- functions and JIT
// ---------------------------------------------------------

REDLINE_TUNABLE(kHotLoopIterations, "generators.HotLoop.iterations", 100, 2,
                10000, "iterations of the loop that calls the function");
REDLINE_TUNABLE(kHotLoopGuardChance, "generators.HotLoop.guard_chance", 0.5, 0,
                1,
                "probability that the call is in a try block whose handler "
                "ends the loop, so that an exception does not end the program");

// Fuzzilli's ForceJITCompilationThroughLoopGenerator
// (V8CommonProfile.swift:142-151). A loop calls an existing function many
// times with the same arguments. The function then gets stable feedback,
// and the JIT tiers compile it without a %Optimize... call.
GEN(HotLoop) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  bool is_generator = false;
  Identifier* f = ExistingOrNewFunction(ctx, cg, out, &is_generator);
  if (f == nullptr) {
    return false;
  }
  std::string_view counter = ctx.builder.FreshName("i");
  GenContext loop = ctx.Inner(true, false);
  loop.Declare(counter, JsType::kNumber, true);
  CodeGen lcg(loop);
  // One call expression: each iteration evaluates the same arguments.
  Expression* call = lcg.CallWithArguments(mk::Id(t, f->name));
  if (is_generator) {
    // The call only makes the generator object. next() runs the body.
    call = mk::CallMethod(t, call, "next", {});
  }
  Statement* body = lcg.Stmt(call);
  if (r.Chance(kHotLoopGuardChance())) {
    // A function that throws once usually throws in each iteration, so
    // the handler ends the loop.
    body = mk::Try(t, {body}, ctx.builder.FreshName("e"), {mk::Break(t)});
  }
  out->push_back(mk::CountedLoop(
      t, counter, mk::Num(t, static_cast<double>(kHotLoopIterations.AsInt())),
      mk::Block(t, {body})));
  return true;
}

// --- destructuring
// -------------------------------------------------------------

// True if a variable that code may assign to is visible: Fuzzilli's
// reassigning generators need one (they use randomJsVariable()).
bool HasMutableVariable(const GenContext& ctx) {
  for (const Variable& v : ctx.visible) {
    if (!v.is_constant && v.name != "arguments" && v.name != "eval") {
      return true;
    }
  }
  return false;
}

// A key for an object pattern: a property name of the environment. Names
// like "Symbol.iterator" become computed keys, other names that are not
// identifiers become string keys.
Property* PatternProperty(GenContext& ctx, std::string_view name,
                          Expression* value) {
  Tree* t = ctx.tree();
  if (IsIdentifierName(name)) {
    return mk::Prop(t, name, value);
  }
  if (name.starts_with("Symbol.")) {
    return mk::Prop(t, mk::Member(t, mk::Id(t, "Symbol"), name.substr(7)),
                    value, /*computed=*/true);
  }
  return mk::Prop(t, mk::Str(t, name), value, /*computed=*/false);
}

// Names for the properties of an object pattern: the known keys of the
// source first, if any (Fuzzilli's randomProperty()), then names of the
// environment. No name is used twice.
std::vector<std::string_view> PatternKeys(GenContext& ctx, CodeGen& cg,
                                          const Expression* source, int count) {
  std::vector<std::string_view> known;
  if (auto* id = source->As<Identifier>()) {
    if (const BindingInfo* info = ctx.builder.InfoOf(id->name)) {
      for (const KnownKey& key : info->keys) {
        known.push_back(key.name);
      }
    }
  }
  std::vector<std::string_view> keys;
  for (int i = 0; i < count; i++) {
    std::string_view name = !known.empty() && ctx.rng().Coin()
                                ? ctx.rng().Pick(known)
                                : cg.PropertyName();
    if (name != "__proto__" &&
        std::find(keys.begin(), keys.end(), name) == keys.end()) {
      keys.push_back(name);
    }
  }
  return keys;
}

REDLINE_TUNABLE(
    kDestructuringAssignArrayChance,
    "generators.DestructuringAssign.array_chance", 0.5, 0, 1,
    "probability of an array pattern rather than an object pattern");
REDLINE_TUNABLE(kDestructuringAssignMaxElements,
                "generators.DestructuringAssign.max_elements", 5, 1, 16,
                "most elements and holes of an array pattern");
REDLINE_TUNABLE(kDestructuringAssignElementChance,
                "generators.DestructuringAssign.element_chance", 0.7, 0, 1,
                "probability that an element of an array pattern has a target "
                "rather than a hole");
REDLINE_TUNABLE(kDestructuringAssignMaxProperties,
                "generators.DestructuringAssign.max_properties", 3, 1, 16,
                "most properties of an object pattern");
REDLINE_TUNABLE(kDestructuringAssignRestChance,
                "generators.DestructuringAssign.rest_chance", 0.33, 0, 1,
                "probability that the pattern ends with a ...rest target");
REDLINE_TUNABLE(kDestructuringAssignDefaultChance,
                "generators.DestructuringAssign.default_chance", 0.2, 0, 1,
                "probability that a target gets a default value");

// Fuzzilli's DestructArrayAndReassignGenerator and
// DestructObjectAndReassignGenerator (CodeGenerators.swift:2508, :2547).
// A destructuring assignment stores into existing variables and members:
// `[a, o.p, , ...r] = iterable;` or `({x: a, y: o[k], ...r} = obj);`.
GEN(DestructuringAssign) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  if (!HasMutableVariable(ctx)) {
    return false;
  }
  auto target = [&]() -> Expression* {
    Expression* e = cg.Target();
    if (r.Chance(kDestructuringAssignDefaultChance())) {
      e = mk::WithDefault(t, e, cg.Value(JsType::kUnknown, 1));
    }
    return e;
  };
  bool has_target = false;
  Expression* pattern = nullptr;
  Expression* source = nullptr;
  if (r.Chance(kDestructuringAssignArrayChance())) {
    auto* array = t->New<ArrayPattern>();
    const int count = static_cast<int>(
        r.Index(static_cast<size_t>(kDestructuringAssignMaxElements.AsInt())));
    for (int i = 0; i < count; i++) {
      if (r.Chance(kDestructuringAssignElementChance())) {
        array->elements.push_back(target());
        has_target = true;
      } else {
        array->elements.push_back(nullptr);
      }
    }
    if (has_target && r.Chance(kDestructuringAssignRestChance())) {
      array->elements.push_back(mk::Rest(t, cg.Target()));
    }
    pattern = array;
    source = IterableSource(ctx, cg);
  } else {
    auto* object = t->New<ObjectPattern>();
    source = cg.ObjectLike();
    const int count =
        1 + r.SmallCount(kDestructuringAssignMaxProperties.AsInt() - 1);
    for (std::string_view key : PatternKeys(ctx, cg, source, count)) {
      object->properties.push_back(PatternProperty(ctx, key, target()));
      has_target = true;
    }
    if (r.Chance(kDestructuringAssignRestChance())) {
      object->properties.push_back(mk::Rest(t, cg.Target()));
    }
    pattern = object;
  }
  out->push_back(cg.Stmt(mk::Assign(t, pattern, source)));
  return true;
}

REDLINE_TUNABLE(kForOfPatternMaxStatements,
                "generators.ForOfPattern.max_statements", 3, 1, 64,
                "most statements in the loop body");
REDLINE_TUNABLE(
    kForOfPatternObjectChance, "generators.ForOfPattern.object_chance", 0.4, 0,
    1, "probability of an object pattern rather than an array pattern");
REDLINE_TUNABLE(kForOfPatternAssignChance,
                "generators.ForOfPattern.assign_chance", 0.2, 0, 1,
                "probability that the loop head assigns to existing targets "
                "rather than declaring new variables");
REDLINE_TUNABLE(kForOfPatternForInChance,
                "generators.ForOfPattern.for_in_chance", 0.15, 0, 1,
                "probability of a for-in loop: its array pattern splits each "
                "key string");
REDLINE_TUNABLE(kForOfPatternAwaitChance,
                "generators.ForOfPattern.await_chance", 0.2, 0, 1,
                "probability of for await when in an async function");
REDLINE_TUNABLE(kForOfPatternMaxElements,
                "generators.ForOfPattern.max_elements", 4, 1, 16,
                "most elements and holes of an array pattern");
REDLINE_TUNABLE(kForOfPatternElementChance,
                "generators.ForOfPattern.element_chance", 0.8, 0, 1,
                "probability that an element of an array pattern has a target "
                "rather than a hole");
REDLINE_TUNABLE(kForOfPatternMaxProperties,
                "generators.ForOfPattern.max_properties", 3, 1, 16,
                "most properties of an object pattern");
REDLINE_TUNABLE(kForOfPatternRestChance, "generators.ForOfPattern.rest_chance",
                0.2, 0, 1,
                "probability that the pattern ends with a ...rest target");
REDLINE_TUNABLE(kForOfPatternDefaultChance,
                "generators.ForOfPattern.default_chance", 0.2, 0, 1,
                "probability that a target gets a default value");

// A source whose elements an array pattern can destructure. Its elements
// are iterable themselves: pairs of a Map or of entries(), arrays, or the
// characters of a string.
Expression* PairsSource(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  switch (r.Index(5)) {
    case 0:
      if (Identifier* v = cg.Variable(JsType::kMap)) {
        return v;
      }
      break;
    case 1:
      if (Identifier* v = cg.Variable(JsType::kString)) {
        return v;
      }
      break;
    case 2: {
      Expression* holder = cg.Variable(JsType::kArray);
      if (holder == nullptr) {
        holder = cg.Variable(JsType::kSet);
      }
      if (holder != nullptr) {
        return mk::CallMethod(t, holder, "entries", {});
      }
      break;
    }
    case 3:
      return mk::CallMethod(t, mk::Id(t, "Object"), "entries",
                            {cg.ObjectLike()});
    default:
      break;
  }
  // [[v, v], [v], ...]
  std::vector<Expression*> rows;
  const int length = 1 + r.SmallCount(kIterableMaxLength.AsInt() - 1);
  for (int i = 0; i < length; i++) {
    rows.push_back(cg.ArrayLiteral(1));
  }
  return mk::Array(t, std::move(rows));
}

// A source whose elements an object pattern can destructure: elements that
// are not null or undefined.
Expression* ObjectsSource(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  if (r.Coin()) {
    if (Identifier* v = cg.Variable(JsType::kString)) {
      return v;
    }
  }
  if (r.Coin()) {
    return PairsSource(ctx, cg);
  }
  std::vector<Expression*> objects;
  const int length = 1 + r.SmallCount(kIterableMaxLength.AsInt() - 1);
  for (int i = 0; i < length; i++) {
    objects.push_back(cg.ObjectLiteral(1));
  }
  return mk::Array(t, std::move(objects));
}

// Fuzzilli's ForOfWithArrayDestructLoop and ForOfWithObjectDestructLoop
// generators and their for-await versions (CodeGenerators.swift:2956-2974,
// :194-254). The head of a for-of (sometimes for-in) loop is a pattern: it
// declares new variables, or it assigns to existing variables and members.
GEN(ForOfPattern) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const bool for_in = r.Chance(kForOfPatternForInChance());
  const bool object = !for_in && r.Chance(kForOfPatternObjectChance());
  const bool assign =
      HasMutableVariable(ctx) && r.Chance(kForOfPatternAssignChance());
  const bool is_const = r.Coin();
  GenContext inner = ctx.Inner(true, false);
  // A target of the pattern: a new name for a declaration, or an existing
  // target for an assignment.
  auto target = [&](JsType type) -> Expression* {
    Expression* e = nullptr;
    if (assign) {
      e = cg.Target();
    } else {
      std::string_view name = ctx.builder.FreshName("d");
      inner.Declare(name, type, is_const);
      e = mk::Id(t, name);
    }
    if (r.Chance(kForOfPatternDefaultChance())) {
      e = mk::WithDefault(t, e, cg.Value(JsType::kUnknown, 1));
    }
    return e;
  };
  auto rest = [&](JsType type) -> Expression* {
    if (assign) {
      return mk::Rest(t, cg.Target());
    }
    std::string_view name = ctx.builder.FreshName("rest");
    inner.Declare(name, type, is_const);
    return mk::Rest(t, mk::Id(t, name));
  };
  Expression* pattern = nullptr;
  Expression* source = nullptr;
  if (object) {
    auto* p = t->New<ObjectPattern>();
    source = ObjectsSource(ctx, cg);
    const int count = 1 + r.SmallCount(kForOfPatternMaxProperties.AsInt() - 1);
    for (std::string_view key : PatternKeys(ctx, cg, source, count)) {
      p->properties.push_back(
          PatternProperty(ctx, key, target(JsType::kUnknown)));
    }
    if (r.Chance(kForOfPatternRestChance())) {
      p->properties.push_back(rest(JsType::kObject));
    }
    pattern = p;
  } else {
    auto* p = t->New<ArrayPattern>();
    source = for_in ? cg.ObjectLike() : PairsSource(ctx, cg);
    const int count =
        1 + static_cast<int>(
                r.Index(static_cast<size_t>(kForOfPatternMaxElements.AsInt())));
    bool has_target = false;
    for (int i = 0; i < count; i++) {
      if (r.Chance(kForOfPatternElementChance()) || i == count - 1) {
        p->elements.push_back(
            target(for_in ? JsType::kString : JsType::kUnknown));
        has_target = true;
      } else {
        p->elements.push_back(nullptr);
      }
    }
    if (has_target && r.Chance(kForOfPatternRestChance())) {
      p->elements.push_back(rest(JsType::kArray));
    }
    pattern = p;
  }
  CodeGen icg(inner);
  Statement* body = mk::Block(
      t, icg.Body(inner,
                  1 + r.SmallCount(kForOfPatternMaxStatements.AsInt() - 1)));
  Node* left = pattern;
  if (!assign) {
    VariableDeclaration* decl = mk::Declare(
        t, is_const ? VariableKind::kConst : VariableKind::kLet, "x", nullptr);
    decl->declarations[0]->target = pattern;
    left = decl;
  }
  if (for_in) {
    auto* loop = t->New<ForInStatement>();
    loop->left = left;
    loop->right = source;
    loop->body = body;
    out->push_back(loop);
    return true;
  }
  auto* loop = t->New<ForOfStatement>();
  loop->left = left;
  loop->right = source;
  loop->body = body;
  loop->is_await = ctx.syntax.in_async && !ctx.syntax.in_static_block &&
                   r.Chance(kForOfPatternAwaitChance());
  out->push_back(loop);
  return true;
}

// --- scopes
// ----------------------------------------------------------------------

// Fuzzilli's NamedVariableGenerator (CodeGenerators.swift:1650): let and
// const declarations over a small shared pool of names. At depth 0, there
// are also var declarations and implicit globals, and a declaration can
// shadow or redeclare a variable of the tree. Nested blocks do not shadow
// visible variables.
GEN(SharedNameDecl) {
  Expression* init = cg.Value(JsType::kUnknown, 1);
  Statement* s = cg.SharedNameStatement(init, ArgumentType(ctx, init));
  if (s == nullptr) {
    return false;
  }
  out->push_back(s);
  return true;
}

REDLINE_TUNABLE(kEvalCodeMaxStatements, "generators.EvalCode.max_statements", 3,
                1, 32, "most generated statements in the evaluated code");
REDLINE_TUNABLE(kEvalCodeIndirectChance, "generators.EvalCode.indirect_chance",
                0.3, 0, 1,
                "probability of an indirect eval: the code runs in the global "
                "scope and sees no local variable");
REDLINE_TUNABLE(kEvalCodeGuardChance, "generators.EvalCode.guard_chance", 0.5,
                0, 1,
                "probability that the eval call is in a try block, so that an "
                "exception does not end the program");

// Fuzzilli's EvalGenerator (CodeGenerators.swift:3318): eval of generated
// code. The generated statements are printed to a string. A direct eval
// sees the variables of its position. An indirect eval, `(0, eval)(s)`,
// runs the code in the global scope.
GEN(EvalCode) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const bool indirect = r.Chance(kEvalCodeIndirectChance());
  GenContext code = ctx.Inner(false, false);
  // Eval code is a script: no return, yield, await, break, continue or
  // super access, and no enclosing labels.
  Builder::Context& s = code.syntax;
  s.in_function = false;
  s.in_async = false;
  s.in_generator = false;
  s.in_loop = false;
  s.in_switch = false;
  s.in_method = false;
  s.in_constructor = false;
  s.has_super_class = false;
  s.in_static_block = false;
  s.in_class_field = false;
  s.labels.clear();
  if (indirect) {
    s.strict = false;
    s.has_arguments = false;
    code.visible.clear();
  }
  CodeGen ccg(code);
  Statements body =
      ccg.Body(code, 1 + r.SmallCount(kEvalCodeMaxStatements.AsInt() - 1));
  if (body.empty()) {
    return false;
  }
  fast::PrintOptions options;
  options.pretty = false;
  std::string source;
  for (const Statement* statement : body) {
    if (!source.empty()) {
      source += ' ';
    }
    source += fast::Print(statement, options);
  }
  Expression* callee = mk::Id(t, "eval");
  if (indirect) {
    callee = mk::Seq(t, {mk::Num(t, 0), callee});
  }
  Statement* call = cg.Stmt(mk::Call(t, callee, {mk::Str(t, source)}));
  if (r.Chance(kEvalCodeGuardChance())) {
    call = mk::Try(t, {call}, ctx.builder.FreshName("e"), {});
  }
  out->push_back(call);
  return true;
}

REDLINE_TUNABLE(kBuiltinReassignMaxStatements,
                "generators.BuiltinReassign.max_statements", 3, 0, 32,
                "most generated statements that run while the global has its "
                "new value");
REDLINE_TUNABLE(kBuiltinReassignCallableChance,
                "generators.BuiltinReassign.callable_chance", 0.3, 0, 1,
                "probability that the new value of the global is callable");

// True if `name` refers to the global object at this position. Then no
// variable and no binding of the tree has this name, and no scope is
// dynamic.
bool IsGlobalHere(GenContext& ctx, std::string_view name) {
  for (const Variable& v : ctx.visible) {
    if (v.name == name) {
      return false;
    }
  }
  if (!ctx.slot.valid()) {
    return true;
  }
  fast::ScopeAnalysis& analysis = ctx.builder.scopes();
  for (const fast::Scope* scope = analysis.ScopeOf(ctx.slot.owner);
       scope != nullptr; scope = scope->parent) {
    if (scope->by_name.contains(name) || scope->is_dynamic()) {
      return false;
    }
  }
  return true;
}

// Fuzzilli's BuiltinOverwriteGenerator (CodeGenerators.swift:1667): a
// global builtin binding gets a new value (`Intl = v`), and generated code
// runs with it. A try/finally block gives the global its old value back,
// so that the rest of the program still finds the builtin. The catch
// clause keeps an exception of the generated code inside the block.
GEN(BuiltinReassign) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::vector<std::string_view> names;
  for (const BuiltinGlobal& g : ctx.env.globals()) {
    if (g.name != "undefined" && g.name != "NaN" && g.name != "Infinity" &&
        IsIdentifierName(g.name) && IsGlobalHere(ctx, g.name)) {
      names.push_back(g.name);
    }
  }
  if (names.empty()) {
    return false;
  }
  std::string_view global = r.Pick(names);
  std::string_view saved = ctx.builder.FreshName("saved");
  GenContext inner = ctx.Inner(false, false);
  CodeGen icg(inner);
  Expression* value = r.Chance(kBuiltinReassignCallableChance())
                          ? icg.Callable()
                          : icg.Value(JsType::kUnknown, 1);
  Statements body = {icg.Stmt(mk::Assign(t, mk::Id(t, global), value))};
  Statements more =
      icg.Body(inner, r.SmallCount(kBuiltinReassignMaxStatements.AsInt()));
  body.insert(body.end(), more.begin(), more.end());
  Statements restore = {
      icg.Stmt(mk::Assign(t, mk::Id(t, global), mk::Id(t, saved)))};
  out->push_back(mk::Block(
      t,
      {mk::Const(t, saved, mk::Id(t, global)),
       mk::Try(t, std::move(body), ctx.builder.FreshName("e"), {}, &restore)}));
  return true;
}

#undef GEN

}  // namespace

void RegisterTypingGenerators(
    const std::function<void(const char*, const char*, double, uint32_t,
                             TypingGeneratorFn)>& add) {
  add("HotLoop", "for (let i = 0; i < 100; i++) f(args); with the same args", 1,
      kNoRequirement, GenHotLoop);
  add("DestructuringAssign", "[a, o.p, ...r] = iterable / ({x: a, ...r} = o)",
      1, kNoRequirement, GenDestructuringAssign);
  add("ForOfPattern", "for (const [a, , b] of pairs) / for ({x: o.p} of objs)",
      0.8, kRecursive, GenForOfPattern);
  add("EvalCode", "eval(\"<generated code>\") / (0, eval)(\"...\")", 0.4,
      kRecursive, GenEvalCode);
  add("SharedNameDecl", "var a = v / let b = v / c = v over a shared name pool",
      1, kNoRequirement, GenSharedNameDecl);
  add("BuiltinReassign",
      "{ const s = Intl; try { Intl = v; ... } catch {} finally { Intl = s; } "
      "}",
      0.4, kRecursive, GenBuiltinReassign);
}

}  // namespace redline

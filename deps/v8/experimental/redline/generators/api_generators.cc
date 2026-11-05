// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/api_generators.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/fast/scope.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

using Statements = std::vector<Statement*>;

// The generators of this file have the signature of the generators of
// generators.cc.
#define GEN(name)                                  \
  bool Gen##name([[maybe_unused]] GenContext& ctx, \
                 [[maybe_unused]] CodeGen& cg, Statements* out)

void PushAll(Statements* out, const Statements& statements) {
  out->insert(out->end(), statements.begin(), statements.end());
}

// The name that the last cg.Declare() registered. Declare() puts the new
// variable at the front of the visible list.
std::string_view LastDeclared(const GenContext& ctx) {
  return ctx.visible.front().name;
}

// A parameter of a new function, with the type that its body sees.
struct Param {
  std::string_view name;
  JsType type;
};

// The context of the body of a new function at `at`. The body sees the
// parameters.
GenContext FunctionContext(const GenContext& at, bool arrow, bool is_async,
                           bool is_generator,
                           const std::vector<Param>& params) {
  GenContext nested = at.Nested(true, arrow, is_async, is_generator);
  for (const Param& p : params) {
    nested.Declare(p.name, p.type);
  }
  return nested;
}

// A function expression or an arrow function with `params` and `body`.
Expression* MakeFunction(Tree* t, bool arrow, bool is_async,
                         const std::vector<Param>& params, Statements body) {
  std::vector<std::string_view> names;
  names.reserve(params.size());
  for (const Param& p : params) {
    names.push_back(p.name);
  }
  if (arrow) {
    return mk::Arrow(t, std::move(names), std::move(body), is_async);
  }
  return mk::Function(t, "", std::move(names), std::move(body), is_async);
}

// A number, string, boolean, null or undefined literal.
Expression* PrimitiveLiteral(CodeGen& c) {
  return c.Literal(c.rng().Pick(
      {JsType::kNumber, JsType::kString, JsType::kBoolean, JsType::kNullish}));
}

// A visible variable or a primitive literal. This expression cannot throw.
Expression* PlainValue(CodeGen& c) {
  if (c.rng().Coin()) {
    if (Identifier* id = c.Variable()) {
      return id;
    }
  }
  return PrimitiveLiteral(c);
}

// The names of the program functions around the insertion point of `ctx`:
// `function f() {}`, `let f = function() {}` and `f = () => {}`.
std::vector<std::string_view> EnclosingFunctionNames(GenContext& ctx) {
  std::vector<std::string_view> names;
  for (Node* node = ctx.slot.owner; node != nullptr;
       node = ctx.builder.ParentOf(node)) {
    const FunctionData* function = nullptr;
    if (auto* declaration = node->As<FunctionDeclaration>()) {
      function = declaration;
    } else if (auto* expression = node->As<FunctionExpression>()) {
      function = expression;
    } else if (auto* arrow = node->As<ArrowFunctionExpression>()) {
      function = arrow;
    }
    if (function == nullptr) {
      continue;
    }
    if (function->name != nullptr) {
      names.push_back(function->name->name);
    }
    Node* parent = ctx.builder.ParentOf(node);
    if (parent == nullptr) {
      continue;
    }
    if (auto* declarator = parent->As<VariableDeclarator>()) {
      if (auto* id = declarator->target->As<Identifier>()) {
        names.push_back(id->name);
      }
    } else if (auto* assignment = parent->As<AssignmentExpression>()) {
      if (auto* id = assignment->left->As<Identifier>()) {
        names.push_back(id->name);
      }
    }
  }
  return names;
}

// Makes the program functions around the insertion point invisible in
// `later`, a context for code that runs in a later job. A call of such a
// function there can start the same job again, without end, and the program
// then times out.
void HideEnclosingFunctions(GenContext& later) {
  const std::vector<std::string_view> names = EnclosingFunctionNames(later);
  if (names.empty()) {
    return;
  }
  std::erase_if(later.visible, [&](const Variable& v) {
    return std::find(names.begin(), names.end(), v.name) != names.end();
  });
}

// --- PromiseOps
// ------------------------------------------------------------------

REDLINE_TUNABLE(kPromiseOpsMaxShapes, "generators.PromiseOps.max_shapes", 2, 1,
                16, "most promise shapes in one invocation");
REDLINE_TUNABLE(kPromiseOpsExecutorWeight,
                "generators.PromiseOps.executor_weight", 1, 0, 100,
                "weight of new Promise(executor) with a generated executor");
REDLINE_TUNABLE(kPromiseOpsCombinatorWeight,
                "generators.PromiseOps.combinator_weight", 1.5, 0, 100,
                "weight of Promise.all, allSettled, any, race and "
                "withResolvers");
REDLINE_TUNABLE(kPromiseOpsChainWeight, "generators.PromiseOps.chain_weight",
                1.5, 0, 100, "weight of then, catch and finally chains");
REDLINE_TUNABLE(kPromiseOpsFromAsyncWeight,
                "generators.PromiseOps.from_async_weight", 1, 0, 100,
                "weight of Array.fromAsync");
REDLINE_TUNABLE(kPromiseOpsForAwaitWeight,
                "generators.PromiseOps.for_await_weight", 1, 0, 100,
                "weight of for await loops");
REDLINE_TUNABLE(kPromiseOpsMaxStatements,
                "generators.PromiseOps.max_statements", 3, 1, 64,
                "most generated statements in an executor, a callback, a then "
                "method or a loop body");
REDLINE_TUNABLE(kPromiseOpsArrowChance, "generators.PromiseOps.arrow_chance",
                0.5, 0, 1,
                "probability that an executor or a callback is an arrow "
                "function");
REDLINE_TUNABLE(kPromiseOpsAsyncCallbackChance,
                "generators.PromiseOps.async_callback_chance", 0.15, 0, 1,
                "probability that a callback is an async function");
REDLINE_TUNABLE(kPromiseOpsRejectChance, "generators.PromiseOps.reject_chance",
                0.2, 0, 1,
                "probability that an executor, a thenable or a new promise "
                "rejects instead of resolving");
REDLINE_TUNABLE(kPromiseOpsThrowChance, "generators.PromiseOps.throw_chance",
                0.1, 0, 1,
                "probability that an executor, a thenable or a callback "
                "throws");
REDLINE_TUNABLE(kPromiseOpsSilentChance, "generators.PromiseOps.silent_chance",
                0.1, 0, 1, "probability that a thenable never calls back");
REDLINE_TUNABLE(kPromiseOpsVisibleChance,
                "generators.PromiseOps.visible_chance", 0.5, 0, 1,
                "probability to use a visible promise, if there is one, "
                "instead of a new promise");
REDLINE_TUNABLE(kPromiseOpsMaxElements, "generators.PromiseOps.max_elements", 4,
                1, 16,
                "most elements in an array of promises, thenables and values");
REDLINE_TUNABLE(kPromiseOpsPromiseElementChance,
                "generators.PromiseOps.promise_element_chance", 0.4, 0, 1,
                "probability that an element of such an array is a promise");
REDLINE_TUNABLE(kPromiseOpsThenableElementChance,
                "generators.PromiseOps.thenable_element_chance", 0.4, 0, 1,
                "probability that an element of such an array that is not a "
                "promise is a thenable");
REDLINE_TUNABLE(kPromiseOpsWithResolversChance,
                "generators.PromiseOps.with_resolvers_chance", 0.2, 0, 1,
                "probability of Promise.withResolvers() and a later resolve or "
                "reject call instead of a combinator");
REDLINE_TUNABLE(kPromiseOpsMaxLinks, "generators.PromiseOps.max_links", 3, 1,
                16, "most then, catch and finally links in a chain");
REDLINE_TUNABLE(kPromiseOpsReturnThenableChance,
                "generators.PromiseOps.return_thenable_chance", 0.15, 0, 1,
                "probability that a callback returns a thenable");
REDLINE_TUNABLE(kPromiseOpsReturnPromiseChance,
                "generators.PromiseOps.return_promise_chance", 0.15, 0, 1,
                "probability that a callback returns a promise");
REDLINE_TUNABLE(kPromiseOpsHandledChance,
                "generators.PromiseOps.handled_chance", 0.95, 0, 1,
                "probability that a promise gets a last handler that cannot "
                "throw: .catch(f) or .then(f, g)");
REDLINE_TUNABLE(kPromiseOpsDeclareChance,
                "generators.PromiseOps.declare_chance", 0.6, 0, 1,
                "probability that the resulting promise is declared for later "
                "generators");
REDLINE_TUNABLE(kPromiseOpsMaxYields, "generators.PromiseOps.max_yields", 3, 1,
                16, "most yields in a new async generator");
REDLINE_TUNABLE(kPromiseOpsMaxParams, "generators.PromiseOps.max_params", 2, 0,
                16, "most parameters of a new async generator");
REDLINE_TUNABLE(kPromiseOpsMapperChance, "generators.PromiseOps.mapper_chance",
                0.3, 0, 1,
                "probability that Array.fromAsync gets a mapping function");
REDLINE_TUNABLE(kPromiseOpsArrayChance, "generators.PromiseOps.array_chance",
                0.2, 0, 1,
                "probability that Array.fromAsync or for await iterates an "
                "array instead of an async generator");
REDLINE_TUNABLE(kPromiseOpsDirectChance, "generators.PromiseOps.direct_chance",
                0.7, 0, 1,
                "probability that for await in an async function is not in a "
                "new async arrow function");

int PromiseBodyCount(Random& r) {
  return 1 + r.SmallCount(kPromiseOpsMaxStatements.AsInt() - 1);
}

// `Promise.name(args)`.
Expression* PromiseCall(Tree* t, std::string_view name,
                        std::vector<Expression*> args) {
  return mk::CallMethod(t, mk::Id(t, "Promise"), name, std::move(args));
}

// The last statement of an executor or of a then method: a call of `resolve`
// or `reject`, or a throw. It is nullptr for a then method that never calls
// back.
Statement* Settle(GenContext& nested, std::string_view resolve,
                  std::string_view reject, bool may_be_silent) {
  Random& r = nested.rng();
  Tree* t = nested.tree();
  CodeGen inner(nested);
  if (may_be_silent && r.Chance(kPromiseOpsSilentChance())) {
    return nullptr;
  }
  if (r.Chance(kPromiseOpsThrowChance())) {
    return mk::Throw(t, inner.Value(JsType::kUnknown, 1));
  }
  std::string_view callee =
      r.Chance(kPromiseOpsRejectChance()) ? reject : resolve;
  return inner.Stmt(
      mk::Call(t, mk::Id(t, callee), {inner.Value(JsType::kUnknown, 1)}));
}

// `new Promise((resolve, reject) => {...})` with a generated body, then a
// resolve or reject call or a throw.
Expression* NewPromise(GenContext& at) {
  Random& r = at.rng();
  Tree* t = at.tree();
  const bool arrow = r.Chance(kPromiseOpsArrowChance());
  const std::vector<Param> params = {
      {at.builder.FreshName("resolve"), JsType::kFunction},
      {at.builder.FreshName("reject"), JsType::kFunction}};
  GenContext nested = FunctionContext(at, arrow, false, false, params);
  CodeGen inner(nested);
  Statements body = inner.Body(nested, PromiseBodyCount(r));
  body.push_back(Settle(nested, params[0].name, params[1].name, false));
  return mk::New(t, mk::Id(t, "Promise"),
                 {MakeFunction(t, arrow, false, params, std::move(body))});
}

// `{ then(resolve, reject) {...} }` with a generated body, then a resolve or
// reject call, a throw or nothing.
Expression* Thenable(GenContext& at) {
  Tree* t = at.tree();
  const std::vector<Param> params = {
      {at.builder.FreshName("r"), JsType::kFunction},
      {at.builder.FreshName("j"), JsType::kFunction}};
  GenContext nested = FunctionContext(at, false, false, false, params);
  nested.syntax.in_method = true;
  HideEnclosingFunctions(nested);
  CodeGen inner(nested);
  Statements body = inner.Body(nested, PromiseBodyCount(at.rng()));
  if (Statement* end = Settle(nested, params[0].name, params[1].name, true)) {
    body.push_back(end);
  }
  FunctionExpression* then =
      mk::Function(t, "", {params[0].name, params[1].name}, std::move(body));
  return mk::Object(t, {mk::Method(t, "then", then)});
}

// A promise for code at `at`: a visible promise, Promise.resolve(v),
// Promise.reject(e) or a new promise. With `may_reject` false, it is only a
// visible promise or Promise.resolve(v). A loop over an array stops at the
// first rejection, and the next rejections then stay unhandled.
Expression* SomePromise(GenContext& at, bool may_reject) {
  Random& r = at.rng();
  Tree* t = at.tree();
  CodeGen c(at);
  if (r.Chance(kPromiseOpsVisibleChance())) {
    if (Identifier* p = c.Variable(JsType::kPromise)) {
      return p;
    }
  }
  if (!may_reject || r.Coin()) {
    return PromiseCall(t, "resolve", {c.Value(JsType::kUnknown, 1)});
  }
  if (r.Chance(kPromiseOpsRejectChance())) {
    return PromiseCall(t, "reject", {c.Value(JsType::kUnknown, 1)});
  }
  return NewPromise(at);
}

// An array of promises, thenables and values.
Expression* PromiseArray(GenContext& at, bool may_reject) {
  Random& r = at.rng();
  CodeGen c(at);
  const int count = 1 + r.SmallCount(kPromiseOpsMaxElements.AsInt() - 1);
  std::vector<Expression*> elements;
  for (int i = 0; i < count; i++) {
    if (r.Chance(kPromiseOpsPromiseElementChance())) {
      elements.push_back(SomePromise(at, may_reject));
    } else if (r.Chance(kPromiseOpsThenableElementChance())) {
      elements.push_back(Thenable(at));
    } else {
      elements.push_back(c.Value(JsType::kUnknown, 1));
    }
  }
  return mk::Array(at.tree(), std::move(elements));
}

// The last statement of a callback: it returns a value, a thenable or a
// promise, or it throws.
Statement* CallbackEnd(GenContext& nested) {
  Random& r = nested.rng();
  Tree* t = nested.tree();
  CodeGen inner(nested);
  if (r.Chance(kPromiseOpsThrowChance())) {
    return mk::Throw(t, inner.Value(JsType::kUnknown, 1));
  }
  if (r.Chance(kPromiseOpsReturnThenableChance())) {
    return mk::Return(t, Thenable(nested));
  }
  if (r.Chance(kPromiseOpsReturnPromiseChance())) {
    return mk::Return(t, SomePromise(nested, true));
  }
  return mk::Return(t, inner.Value(JsType::kUnknown, 1));
}

// A callback of a then, catch or finally link, or a mapping function: a
// generated body and CallbackEnd().
Expression* Callback(GenContext& at, int param_count) {
  Random& r = at.rng();
  const bool arrow = r.Chance(kPromiseOpsArrowChance());
  const bool is_async = r.Chance(kPromiseOpsAsyncCallbackChance());
  std::vector<Param> params;
  for (int i = 0; i < param_count; i++) {
    params.push_back({at.builder.FreshName("x"), JsType::kUnknown});
  }
  GenContext nested = FunctionContext(at, arrow, is_async, false, params);
  HideEnclosingFunctions(nested);
  CodeGen inner(nested);
  Statements body = inner.Body(nested, PromiseBodyCount(r));
  body.push_back(CallbackEnd(nested));
  return MakeFunction(at.tree(), arrow, is_async, params, std::move(body));
}

// `promise.then(f).catch(g).finally(h)`: 1-3 links with generated callbacks.
Expression* Chain(GenContext& at, Expression* promise) {
  Random& r = at.rng();
  Tree* t = at.tree();
  const int links = 1 + r.SmallCount(kPromiseOpsMaxLinks.AsInt() - 1);
  for (int i = 0; i < links; i++) {
    switch (r.Index(4)) {
      case 0:
        promise = mk::CallMethod(t, promise, "then", {Callback(at, 1)});
        break;
      case 1:
        promise = mk::CallMethod(t, promise, "then",
                                 {Callback(at, 1), Callback(at, 1)});
        break;
      case 2:
        promise = mk::CallMethod(t, promise, "catch", {Callback(at, 1)});
        break;
      default:
        promise = mk::CallMethod(t, promise, "finally", {Callback(at, 0)});
        break;
    }
  }
  return promise;
}

// `promise.catch(e => e)` or `promise.then(v => v, e => e)`. These handlers
// cannot throw, thus the result does not reject.
Expression* Handled(GenContext& at, Expression* promise) {
  Random& r = at.rng();
  Tree* t = at.tree();
  CodeGen c(at);
  const std::string_view e = at.builder.FreshName("e");
  Expression* result =
      r.Coin() ? static_cast<Expression*>(mk::Id(t, e)) : PlainValue(c);
  Expression* on_rejected = mk::ArrowExpr(t, {e}, result);
  if (r.Coin()) {
    return mk::CallMethod(t, promise, "catch", {on_rejected});
  }
  const std::string_view v = at.builder.FreshName("v");
  return mk::CallMethod(t, promise, "then",
                        {mk::ArrowExpr(t, {v}, mk::Id(t, v)), on_rejected});
}

// Appends `promise` as a declaration or as a statement. Most of the time, the
// promise first gets a last handler.
void EmitPromise(GenContext& ctx, CodeGen& cg, Expression* promise,
                 Statements* out) {
  Random& r = ctx.rng();
  if (r.Chance(kPromiseOpsHandledChance())) {
    promise = Handled(ctx, promise);
  }
  if (r.Chance(kPromiseOpsDeclareChance())) {
    out->push_back(cg.Declare(promise, JsType::kPromise, "p"));
  } else {
    out->push_back(cg.Stmt(promise));
  }
}

// `const w = Promise.withResolvers()` and a chain on w.promise. The call of
// w.resolve(v) or w.reject(e) goes to `deferred`, after the other shapes.
void WithResolvers(GenContext& ctx, CodeGen& cg, Statements* out,
                   Statements* deferred) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  out->push_back(
      cg.Declare(PromiseCall(t, "withResolvers", {}), JsType::kObject, "w"));
  const std::string_view w = LastDeclared(ctx);
  Expression* promise = mk::Member(t, mk::Id(t, w), "promise");
  if (r.Coin()) {
    promise = Chain(ctx, promise);
  }
  EmitPromise(ctx, cg, promise, out);
  const char* settle =
      r.Chance(kPromiseOpsRejectChance()) ? "reject" : "resolve";
  Expression* value = cg.Value(JsType::kUnknown, 1);
  deferred->push_back(
      cg.Stmt(mk::CallMethod(t, mk::Id(t, w), settle, {value})));
}

// `async function* g(a) {...}` with generated statements and 1-3 yields of
// generated values. Declares g at `ctx`, and returns its name and its number
// of parameters.
std::string_view AsyncGenerator(GenContext& ctx, CodeGen& cg, Statements* out,
                                int* param_count) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  const std::string_view name = ctx.builder.FreshName("g");
  GenContext nested = ctx.Nested(true, false, true, true);
  HideEnclosingFunctions(nested);
  const int np = static_cast<int>(r.Range(0, kPromiseOpsMaxParams.AsInt()));
  std::vector<Expression*> params = cg.Parameters(nested, np);
  CodeGen inner(nested);
  // The statements and the yields come in program order. Thus a yield can
  // use the variables of the statements before it.
  int statements = r.SmallCount(kPromiseOpsMaxStatements.AsInt());
  int yields = 1 + r.SmallCount(kPromiseOpsMaxYields.AsInt() - 1);
  Statements body;
  while (statements + yields > 0) {
    if (r.Index(static_cast<size_t>(statements + yields)) <
        static_cast<size_t>(statements)) {
      PushAll(&body, inner.Body(nested, 1));
      statements--;
    } else {
      body.push_back(
          inner.Stmt(mk::Yield(t, inner.Value(JsType::kUnknown, 1))));
      yields--;
    }
  }
  FunctionDeclaration* fn =
      mk::FunctionDecl(t, name, {}, std::move(body), true, true);
  fn->params = std::move(params);
  out->push_back(fn);
  ctx.Declare(name, JsType::kFunction);
  *param_count = np;
  return name;
}

// The iterable of Array.fromAsync or of for await: a call of a new async
// generator, or an array of promises, thenables and values.
Expression* AsyncSource(GenContext& ctx, CodeGen& cg, Statements* out) {
  if (ctx.rng().Chance(kPromiseOpsArrayChance())) {
    return PromiseArray(ctx, false);
  }
  int param_count = 0;
  const std::string_view g = AsyncGenerator(ctx, cg, out, &param_count);
  return cg.CallWithArguments(mk::Id(ctx.tree(), g), param_count);
}

// `Array.fromAsync(source)`, sometimes with a mapping function.
Expression* FromAsync(GenContext& ctx, CodeGen& cg, Statements* out) {
  Tree* t = ctx.tree();
  std::vector<Expression*> args = {AsyncSource(ctx, cg, out)};
  if (ctx.rng().Chance(kPromiseOpsMapperChance())) {
    args.push_back(Callback(ctx, 2));
  }
  return mk::CallMethod(t, mk::Id(t, "Array"), "fromAsync", std::move(args));
}

// `for await (const x of source) {...}` with a generated body. In an async
// function, the loop can stay bare. Elsewhere it goes into a new async arrow
// function that is called at once.
void ForAwait(GenContext& ctx, CodeGen& cg, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  Expression* source = AsyncSource(ctx, cg, out);
  auto loop = [&](GenContext& at) -> Statement* {
    const std::string_view x = ctx.builder.FreshName("x");
    GenContext body_ctx = at.Inner(true, false);
    HideEnclosingFunctions(body_ctx);
    body_ctx.Declare(x, JsType::kUnknown);
    CodeGen body_cg(body_ctx);
    Statements body = body_cg.Body(body_ctx, PromiseBodyCount(r));
    const VariableKind kind =
        r.Coin() ? VariableKind::kConst : VariableKind::kLet;
    return mk::ForOf(t, kind, x, source, mk::Block(t, std::move(body)),
                     /*is_await=*/true);
  };
  if (ctx.syntax.in_async && !ctx.syntax.in_static_block &&
      r.Chance(kPromiseOpsDirectChance())) {
    out->push_back(loop(ctx));
    return;
  }
  GenContext async_ctx = ctx.Nested(true, true, true, false);
  Statements body = {loop(async_ctx)};
  EmitPromise(ctx, cg, mk::Call(t, mk::Arrow(t, {}, std::move(body), true), {}),
              out);
}

// Fuzzilli: PromiseGenerator (CodeGen/CodeGenerators.swift:3243) and
// ForAwaitOfLoopGenerator (CodeGen/CodeGenerators.swift:2931). Fuzzilli gets
// the combinators, the chains and Array.fromAsync from its typed builtins.
GEN(PromiseOps) {
  Random& r = ctx.rng();
  Statements deferred;
  const int shapes = 1 + r.SmallCount(kPromiseOpsMaxShapes.AsInt() - 1);
  for (int i = 0; i < shapes; i++) {
    switch (
        r.Weighted({kPromiseOpsExecutorWeight(), kPromiseOpsCombinatorWeight(),
                    kPromiseOpsChainWeight(), kPromiseOpsFromAsyncWeight(),
                    kPromiseOpsForAwaitWeight()})) {
      case 0:
        EmitPromise(ctx, cg, NewPromise(ctx), out);
        break;
      case 1:
        if (r.Chance(kPromiseOpsWithResolversChance())) {
          WithResolvers(ctx, cg, out, &deferred);
        } else {
          const char* method = r.Pick({"all", "allSettled", "any", "race"});
          EmitPromise(
              ctx, cg,
              PromiseCall(ctx.tree(), method, {PromiseArray(ctx, true)}), out);
        }
        break;
      case 2:
        EmitPromise(ctx, cg, Chain(ctx, SomePromise(ctx, true)), out);
        break;
      case 3:
        EmitPromise(ctx, cg, FromAsync(ctx, cg, out), out);
        break;
      default:
        ForAwait(ctx, cg, out);
        break;
    }
  }
  PushAll(out, deferred);
  return true;
}

// --- HostObjectDecl
// --------------------------------------------------------------

// One shape by default. An interceptor object and an access-checked object
// in the same program make every later execution slower (6 ms to about
// 250 ms each). This continues until the d8 process respawns.
REDLINE_TUNABLE(kHostObjectDeclMaxShapes,
                "generators.HostObjectDecl.max_shapes", 1, 1, 16,
                "most host object shapes in one invocation");
REDLINE_TUNABLE(kHostObjectDeclInterceptorWeight,
                "generators.HostObjectDecl.interceptor_weight", 1, 0, 100,
                "weight of d8.test.createInterceptorObject");
REDLINE_TUNABLE(kHostObjectDeclAccessCheckedWeight,
                "generators.HostObjectDecl.access_checked_weight", 1, 0, 100,
                "weight of d8.test.createAccessCheckedObject and "
                "createAccessCheckedInterceptorObject");
REDLINE_TUNABLE(kHostObjectDeclSetPolicyWeight,
                "generators.HostObjectDecl.set_policy_weight", 1, 0, 100,
                "weight of d8.test.setAccessPolicy");
REDLINE_TUNABLE(kHostObjectDeclWithInterceptorChance,
                "generators.HostObjectDecl.with_interceptor_chance", 0.5, 0, 1,
                "probability that an access-checked object also has an "
                "interceptor");
REDLINE_TUNABLE(kHostObjectDeclBooleanPolicyChance,
                "generators.HostObjectDecl.boolean_policy_chance", 0.5, 0, 1,
                "probability that an access policy is a boolean instead of a "
                "policy name");
REDLINE_TUNABLE(kHostObjectDeclMaxStatements,
                "generators.HostObjectDecl.max_statements", 3, 1, 64,
                "most generated statements in an interceptor callback");
REDLINE_TUNABLE(kHostObjectDeclArrowChance,
                "generators.HostObjectDecl.arrow_chance", 0.2, 0, 1,
                "probability that an interceptor callback is an arrow "
                "function");
REDLINE_TUNABLE(kHostObjectDeclKeyCheckChance,
                "generators.HostObjectDecl.key_check_chance", 0.5, 0, 1,
                "probability that an interceptor callback returns a special "
                "value for one property name");
REDLINE_TUNABLE(kHostObjectDeclGuardBodyChance,
                "generators.HostObjectDecl.guard_body_chance", 0.7, 0, 1,
                "probability that the generated statements of an interceptor "
                "callback are in a try statement");
REDLINE_TUNABLE(kHostObjectDeclMaxOps, "generators.HostObjectDecl.max_ops", 3,
                1, 16, "most property operations on a host object");
REDLINE_TUNABLE(kHostObjectDeclUnguardedChance,
                "generators.HostObjectDecl.unguarded_chance", 0.5, 0, 1,
                "probability that an operation on a host object that cannot "
                "throw is not in a try statement");
REDLINE_TUNABLE(kHostObjectDeclLoopChance,
                "generators.HostObjectDecl.loop_chance", 0.25, 0, 1,
                "probability that the operations on a host object are in a "
                "loop");
REDLINE_TUNABLE(kHostObjectDeclMaxIterations,
                "generators.HostObjectDecl.max_iterations", 10, 1, 1000,
                "most iterations of that loop");
REDLINE_TUNABLE(kHostObjectDeclVisibleChance,
                "generators.HostObjectDecl.visible_chance", 0.3, 0, 1,
                "probability that setAccessPolicy gets any visible variable, "
                "in a try statement");
REDLINE_TUNABLE(kHostObjectDeclKnownKeyChance,
                "generators.HostObjectDecl.known_key_chance", 0.5, 0, 1,
                "probability that a load or an in check uses the special "
                "property name of the interceptor callback");

// A host object that this invocation made.
struct HostObject {
  std::string_view name;
  // True if setAccessPolicy accepts the object.
  bool access_checked = false;
  // True if a property operation on the object can throw.
  bool may_throw = false;
  // The property name that the interceptor callback checks, or empty.
  std::string_view key;
};

// `d8.test.name(args)`.
Expression* D8TestCall(Tree* t, std::string_view name,
                       std::vector<Expression*> args) {
  return mk::CallMethod(t, mk::Member(t, mk::Id(t, "d8"), "test"), name,
                        std::move(args));
}

// The interceptor callback of a host object: a function of the property name
// with generated statements. It returns a visible value or a literal.
// Sets *may_throw if the callback can throw, and *key to the property name
// that it checks. Fuzzilli: buildInterceptorCallback
// (Profiles/V8CommonProfile.swift:360).
Expression* InterceptorCallback(GenContext& ctx, bool* may_throw,
                                std::string_view* key) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  const bool arrow = r.Chance(kHostObjectDeclArrowChance());
  const std::vector<Param> params = {
      {ctx.builder.FreshName("k"), JsType::kUnknown}};
  GenContext nested = FunctionContext(ctx, arrow, false, false, params);
  // The receiver of a plain callback is the host object. A load of this.p
  // in the callback calls the callback again, without end.
  nested.syntax.has_this = false;
  // Property operations call the callback later, also in other jobs.
  HideEnclosingFunctions(nested);
  CodeGen inner(nested);
  Statements body;
  *key = {};
  if (r.Chance(kHostObjectDeclKeyCheckChance())) {
    *key = inner.PropertyName();
    Expression* test = mk::Binary(t, BinaryOp::kStrictEq,
                                  mk::Id(t, params[0].name), mk::Str(t, *key));
    body.push_back(mk::If(t, test, mk::Return(t, PlainValue(inner))));
  }
  const int count = 1 + r.SmallCount(kHostObjectDeclMaxStatements.AsInt() - 1);
  *may_throw = !r.Chance(kHostObjectDeclGuardBodyChance());
  if (*may_throw) {
    PushAll(&body, inner.Body(nested, count));
  } else {
    // The return statement must not see the variables of the try block.
    GenContext block = nested;
    CodeGen block_cg(block);
    body.push_back(mk::Try(t, block_cg.Body(block, count),
                           ctx.builder.FreshName("e"), {}));
  }
  body.push_back(mk::Return(t, PlainValue(inner)));
  return MakeFunction(t, arrow, false, params, std::move(body));
}

// An access policy that d8 accepts: true, false, "same-context" or
// "security-token". Sets *denies for false, which stops all access.
// Fuzzilli: buildAccessCheck (Profiles/V8CommonProfile.swift:353).
Expression* AccessPolicy(GenContext& ctx, bool* denies) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  if (r.Chance(kHostObjectDeclBooleanPolicyChance())) {
    const bool allow = r.Coin();
    *denies = !allow;
    return mk::Bool(t, allow);
  }
  *denies = false;
  return mk::Str(t, r.Coin() ? "same-context" : "security-token");
}

// A key for a load or an `in` check on `object`.
Expression* HostObjectKey(CodeGen& c, const HostObject& object) {
  if (!object.key.empty() && c.rng().Chance(kHostObjectDeclKnownKeyChance())) {
    return mk::Str(c.tree(), object.key);
  }
  return c.rng().Coin()
             ? static_cast<Expression*>(mk::Str(c.tree(), c.PropertyName()))
             : c.Key();
}

// 1-3 property loads, stores, `in` checks, removes or key lists on `object`,
// sometimes in a loop. An operation that can throw is in a try statement.
void HostObjectOps(GenContext& ctx, const HostObject& object, Statements* out) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  const bool loop = r.Chance(kHostObjectDeclLoopChance());
  GenContext loop_ctx = ctx.Inner(true, false);
  std::string_view counter;
  if (loop) {
    counter = ctx.builder.FreshName("i");
    loop_ctx.Declare(counter, JsType::kNumber);
  }
  // Without a loop, later generators can use the new variables.
  GenContext& at = loop ? loop_ctx : ctx;
  CodeGen c(at);
  const bool guard =
      object.may_throw || !r.Chance(kHostObjectDeclUnguardedChance());
  // A statement that can throw goes into a try statement.
  auto guarded = [&](Statement* s) -> Statement* {
    return guard ? mk::Try(t, {s}, ctx.builder.FreshName("e"), {}) : s;
  };
  Statements ops;
  const int count = 1 + r.SmallCount(kHostObjectDeclMaxOps.AsInt() - 1);
  for (int i = 0; i < count; i++) {
    Identifier* o = mk::Id(t, object.name);
    switch (r.Index(5)) {
      case 0: {
        Expression* load = r.Coin() ? c.Property(o)
                                    : mk::Index(t, o, HostObjectKey(c, object));
        if (guard) {
          PushAll(&ops, c.DeclareGuarded(load, JsType::kUnknown));
        } else {
          ops.push_back(c.Declare(load, JsType::kUnknown));
        }
        break;
      }
      case 1: {
        // An unguarded store gets a primitive value. Then a store to
        // __proto__ cannot make a cycle, which throws.
        Expression* value =
            guard ? c.Value(JsType::kUnknown, 1) : PrimitiveLiteral(c);
        ops.push_back(guarded(c.Stmt(mk::Assign(t, c.Property(o), value))));
        break;
      }
      case 2: {
        Expression* in =
            mk::Binary(t, BinaryOp::kIn, HostObjectKey(c, object), o);
        if (guard) {
          PushAll(&ops, c.DeclareGuarded(in, JsType::kBoolean));
        } else {
          ops.push_back(c.Declare(in, JsType::kBoolean));
        }
        break;
      }
      case 3:
        ops.push_back(
            guarded(c.Stmt(mk::Unary(t, UnaryOp::kDelete, c.Property(o)))));
        break;
      default: {
        const bool reflect = r.Coin();
        const char* method =
            reflect ? "ownKeys"
                    : r.Pick({"keys", "getOwnPropertyNames", "entries"});
        Expression* keys = mk::CallMethod(
            t, mk::Id(t, reflect ? "Reflect" : "Object"), method, {o});
        if (guard) {
          PushAll(&ops, c.DeclareGuarded(keys, JsType::kArray));
        } else {
          ops.push_back(c.Declare(keys, JsType::kArray));
        }
        break;
      }
    }
  }
  if (!loop) {
    PushAll(out, ops);
    return;
  }
  const double iterations =
      static_cast<double>(r.Range(1, kHostObjectDeclMaxIterations.AsInt()));
  out->push_back(mk::CountedLoop(t, counter, mk::Num(t, iterations),
                                 mk::Block(t, std::move(ops))));
}

// `d8.test.createInterceptorObject(function(k) {...})` and operations on it.
// Fuzzilli: V8InterceptorObjectGenerator (Profiles/V8CommonProfile.swift:367).
void InterceptorObject(GenContext& ctx, CodeGen& cg, Statements* out,
                       std::vector<HostObject>* made) {
  Tree* t = ctx.tree();
  HostObject object;
  // The callback comes first, as in Fuzzilli. Its generated statements can
  // then not change the new object.
  Expression* callback =
      InterceptorCallback(ctx, &object.may_throw, &object.key);
  out->push_back(
      cg.Declare(D8TestCall(t, "createInterceptorObject", {callback}),
                 JsType::kObject, "io"));
  object.name = LastDeclared(ctx);
  HostObjectOps(ctx, object, out);
  made->push_back(object);
}

// `d8.test.createAccessCheckedObject(policy)`, or
// `d8.test.createAccessCheckedInterceptorObject(policy, function(k) {...})`,
// and operations on it. Fuzzilli: V8AccessCheckedObjectGenerator
// (Profiles/V8CommonProfile.swift:383).
void AccessCheckedObject(GenContext& ctx, CodeGen& cg, Statements* out,
                         std::vector<HostObject>* made) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  HostObject object;
  object.access_checked = true;
  bool denies = false;
  Expression* create = nullptr;
  if (r.Chance(kHostObjectDeclWithInterceptorChance())) {
    // The callback runs only when the access check fails.
    bool callback_may_throw = false;
    Expression* callback =
        InterceptorCallback(ctx, &callback_may_throw, &object.key);
    Expression* policy = AccessPolicy(ctx, &denies);
    create = D8TestCall(t, "createAccessCheckedInterceptorObject",
                        {policy, callback});
  } else {
    Expression* policy = AccessPolicy(ctx, &denies);
    create = D8TestCall(t, "createAccessCheckedObject", {policy});
  }
  // A failed access check throws, except for loads with an interceptor.
  object.may_throw = denies;
  out->push_back(cg.Declare(create, JsType::kObject, "aco"));
  object.name = LastDeclared(ctx);
  HostObjectOps(ctx, object, out);
  made->push_back(object);
}

// The method name of the call that initializes `v`
// (`createAccessCheckedObject` for `let v = d8.test.createAccessCheckedObject(
// p)`), or empty if `v` has another initializer or if an assignment changes
// it.
std::string_view InitializerMethod(const Variable& v) {
  const Binding* binding = v.binding;
  if (binding == nullptr || binding->assigned ||
      binding->declaration == nullptr) {
    return {};
  }
  auto* declarator = binding->declaration->As<VariableDeclarator>();
  if (declarator == nullptr || declarator->target != binding->identifier ||
      declarator->init == nullptr) {
    return {};
  }
  auto* call = declarator->init->As<CallExpression>();
  if (call == nullptr) {
    return {};
  }
  auto* member = call->callee->As<MemberExpression>();
  if (member == nullptr || member->computed) {
    return {};
  }
  auto* property = member->property->As<Identifier>();
  return property != nullptr ? property->name : std::string_view();
}

// True if `v` holds an access-checked object of the program: its initializer
// calls d8.test.createAccessCheckedObject or
// createAccessCheckedInterceptorObject, and no assignment changes it.
bool IsAccessCheckedVariable(const Variable& v) {
  return InitializerMethod(v).starts_with("createAccessChecked");
}

// True if `v` holds an interceptor object of the program.
bool IsInterceptorVariable(const Variable& v) {
  return InitializerMethod(v) == "createInterceptorObject";
}

// `d8.test.setAccessPolicy(object, policy)` on an access-checked object of
// this invocation or of the program, then operations on it. Sometimes the
// object is any visible variable, and the call is in a try statement.
// Without an access-checked object, it makes one first. Fuzzilli:
// V8SetAccessPolicyGenerator (Profiles/V8CommonProfile.swift:407).
void SetAccessPolicy(GenContext& ctx, CodeGen& cg, Statements* out,
                     std::vector<HostObject>* made) {
  Random& r = ctx.rng();
  Tree* t = ctx.tree();
  bool denies = false;
  if (r.Chance(kHostObjectDeclVisibleChance())) {
    if (Identifier* any = cg.Variable()) {
      Expression* policy = AccessPolicy(ctx, &denies);
      out->push_back(
          mk::Try(t, {cg.Stmt(D8TestCall(t, "setAccessPolicy", {any, policy}))},
                  ctx.builder.FreshName("e"), {}));
      return;
    }
  }
  // Index into `made`, or -1 for a variable of the program.
  std::vector<std::pair<std::string_view, int>> candidates;
  for (size_t i = 0; i < made->size(); i++) {
    if ((*made)[i].access_checked) {
      candidates.emplace_back((*made)[i].name, static_cast<int>(i));
    }
  }
  for (const Variable& v : ctx.visible) {
    if (IsAccessCheckedVariable(v)) {
      candidates.emplace_back(v.name, -1);
    }
  }
  if (candidates.empty()) {
    AccessCheckedObject(ctx, cg, out, made);
    candidates.emplace_back(made->back().name,
                            static_cast<int>(made->size() - 1));
  }
  const std::pair<std::string_view, int> target = r.Pick(candidates);
  Expression* policy = AccessPolicy(ctx, &denies);
  out->push_back(cg.Stmt(
      D8TestCall(t, "setAccessPolicy", {mk::Id(t, target.first), policy})));
  HostObject object;
  if (target.second >= 0) {
    HostObject& own = (*made)[static_cast<size_t>(target.second)];
    own.may_throw = denies;
    object = own;
  } else {
    // The program can have given it an interceptor that throws.
    object.name = target.first;
    object.access_checked = true;
    object.may_throw = true;
  }
  HostObjectOps(ctx, object, out);
}

// Fuzzilli: V8InterceptorObjectGenerator, V8AccessCheckedObjectGenerator and
// V8SetAccessPolicyGenerator (Profiles/V8CommonProfile.swift:367-412).
// A program gets either interceptor objects or access-checked objects, not
// both. With both, every later execution in the same d8 process is slower,
// until the process respawns.
GEN(HostObjectDecl) {
  Random& r = ctx.rng();
  bool has_interceptor = false;
  bool has_access_checked = false;
  for (const Variable& v : ctx.visible) {
    has_access_checked = has_access_checked || IsAccessCheckedVariable(v);
    has_interceptor = has_interceptor || IsInterceptorVariable(v);
  }
  if (has_access_checked && has_interceptor) {
    return false;
  }
  std::vector<HostObject> made;
  const int shapes = 1 + r.SmallCount(kHostObjectDeclMaxShapes.AsInt() - 1);
  for (int i = 0; i < shapes; i++) {
    switch (
        r.Weighted({has_access_checked ? 0 : kHostObjectDeclInterceptorWeight(),
                    has_interceptor ? 0 : kHostObjectDeclAccessCheckedWeight(),
                    has_interceptor ? 0 : kHostObjectDeclSetPolicyWeight()})) {
      case 0:
        if (has_access_checked) {
          return false;  // the weights of all shapes are 0
        }
        InterceptorObject(ctx, cg, out, &made);
        has_interceptor = true;
        break;
      case 1:
        AccessCheckedObject(ctx, cg, out, &made);
        has_access_checked = true;
        break;
      default:
        SetAccessPolicy(ctx, cg, out, &made);
        has_access_checked = true;
        break;
    }
  }
  return true;
}

#undef GEN

}  // namespace

std::vector<ApiGenerator> ApiGenerators() {
  return {
      {"PromiseOps",
       "new Promise(executor), Promise.all/allSettled/any/race/withResolvers, "
       "then/catch/finally chains, Array.fromAsync, for await",
       2, kRecursive, GenPromiseOps},
      {"HostObjectDecl",
       "d8.test interceptor and access-checked objects, setAccessPolicy, "
       "property operations on them",
       1, kRecursive, GenHostObjectDecl},
  };
}

}  // namespace redline

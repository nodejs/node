// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/shapes_generators.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/generators/typing_generators.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

using Statements = std::vector<Statement*>;

// Generators share one signature. Not every generator needs every argument.
#define GEN(name)                                  \
  bool Gen##name([[maybe_unused]] GenContext& ctx, \
                 [[maybe_unused]] CodeGen& cg, Statements* out)

Tree* T(GenContext& ctx) { return ctx.tree(); }

Statement* Push(Statements* out, Statement* s) {
  out->push_back(s);
  return s;
}

// The name that the last cg.Declare() or ctx.Declare() registered.
std::string_view LastDeclared(GenContext& ctx) {
  return ctx.visible.front().name;
}

// The type guess of the visible variable `name`.
JsType TypeOf(GenContext& ctx, std::string_view name) {
  for (const Variable& v : ctx.visible) {
    if (v.name == name) {
      return v.type;
    }
  }
  return JsType::kUnknown;
}

// The context of the body of an object-literal method at this position.
GenContext MethodContext(const GenContext& ctx) {
  GenContext nested = ctx.Nested(true, false, false, false);
  nested.syntax.in_method = true;
  return nested;
}

// The context of a class field initializer at this position: strict code
// with `this`, but without `arguments`.
GenContext FieldContext(const GenContext& ctx) {
  GenContext nested = ctx.Nested(true, true, false, false);
  nested.syntax.strict = true;
  nested.syntax.in_class_field = true;
  nested.syntax.has_arguments = false;
  nested.syntax.in_method = true;
  nested.syntax.has_super_class = true;
  return nested;
}

// A property name that is an identifier and not __proto__.
std::string_view PlainName(CodeGen& cg) {
  std::string_view name = cg.PropertyName();
  return (name == "__proto__" || !IsIdentifierName(name)) ? "x" : name;
}

// --- Imitation -----------------------------------------------------------

REDLINE_TUNABLE(kImitationMaxStatements, "generators.Imitation.max_statements",
                3, 0, 16,
                "most generated statements in the body of an imitation hook "
                "or class");
REDLINE_TUNABLE(kImitationToPrimitiveChance,
                "generators.Imitation.to_primitive_chance", 0.5, 0, 1,
                "probability that a primitive imitation uses "
                "Symbol.toPrimitive instead of valueOf");
REDLINE_TUNABLE(kImitationSubclassChance,
                "generators.Imitation.subclass_chance", 0.5, 0, 1,
                "probability that an object imitation is an instance of a "
                "class that extends the constructor of the original");
REDLINE_TUNABLE(kImitationClassExpressionChance,
                "generators.Imitation.class_expression_chance", 0.3, 0, 1,
                "probability that the subclass is a class expression");
REDLINE_TUNABLE(kImitationMaxMembers, "generators.Imitation.max_members", 2, 0,
                16,
                "most members of the subclass or properties of the literal "
                "beside __proto__");
REDLINE_TUNABLE(kImitationTrapChance, "generators.Imitation.trap_chance", 0.3,
                0, 1,
                "probability that a function imitation proxy gets a "
                "forwarding trap instead of an empty handler");
REDLINE_TUNABLE(kImitationUseChance, "generators.Imitation.use_chance", 0.5, 0,
                1,
                "probability that the imitation replaces the original in one "
                "operation right away");

// The kind of stand-in that ImitationGenerator builds for a value.
enum class ImitationKind { kPrimitive, kFunction, kObject };

ImitationKind KindOf(JsType type, Random& r) {
  switch (type) {
    case JsType::kNumber:
    case JsType::kString:
    case JsType::kBoolean:
    case JsType::kBigInt:
    case JsType::kSymbol:
    case JsType::kNullish:
      return ImitationKind::kPrimitive;
    case JsType::kFunction:
    case JsType::kClass:
      return ImitationKind::kFunction;
    case JsType::kUnknown:
      return r.Pick({ImitationKind::kPrimitive, ImitationKind::kFunction,
                     ImitationKind::kObject});
    default:
      return ImitationKind::kObject;
  }
}

// `{valueOf() {...; return v;}}` or `{[Symbol.toPrimitive](hint) {...}}`.
Expression* PrimitiveImitation(GenContext& ctx, std::string_view orig) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  GenContext nested = MethodContext(ctx);
  const bool to_primitive = r.Chance(kImitationToPrimitiveChance());
  std::vector<std::string_view> params;
  if (to_primitive) {
    params.push_back(ctx.builder.FreshName("hint"));
    nested.Declare(params.back(), JsType::kString);
  }
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, r.SmallCount(kImitationMaxStatements.AsInt()));
  body.push_back(mk::Return(t, mk::Id(t, orig)));
  FunctionExpression* fn = mk::Function(t, "", params, std::move(body));
  if (!to_primitive) {
    return mk::Object(t, {mk::Method(t, "valueOf", fn)});
  }
  Property* hook =
      mk::Prop(t, mk::Member(t, mk::Id(t, "Symbol"), "toPrimitive"), fn, true);
  hook->method = true;
  return mk::Object(t, {hook});
}

// A handler trap that runs generated code and then forwards to Reflect.
Property* ForwardingTrap(GenContext& ctx) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  struct Trap {
    const char* name;
    int params;
  };
  static const Trap kTraps[] = {{"apply", 3}, {"construct", 3}, {"get", 3},
                                {"has", 2},   {"set", 4},       {"ownKeys", 1}};
  const Trap& trap = kTraps[r.Index(sizeof(kTraps) / sizeof(kTraps[0]))];
  GenContext nested = MethodContext(ctx);
  std::vector<std::string_view> params;
  for (int i = 0; i < trap.params; i++) {
    params.push_back(ctx.builder.FreshName(i == 0 ? "target" : "a"));
    nested.Declare(params.back(),
                   i == 0 ? JsType::kFunction : JsType::kUnknown);
  }
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, r.SmallCount(kImitationMaxStatements.AsInt()));
  std::vector<Expression*> args;
  args.reserve(params.size());
  for (std::string_view p : params) {
    args.push_back(mk::Id(t, p));
  }
  body.push_back(mk::Return(
      t, mk::CallMethod(t, mk::Id(t, "Reflect"), trap.name, std::move(args))));
  return mk::Method(t, trap.name, mk::Function(t, "", params, std::move(body)));
}

// Members of a class that extends the constructor of the original. They are
// fields, methods and getters with shared names. Thus they can shadow the
// members of the original.
std::vector<Node*> SubclassMembers(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::vector<Node*> members;
  const int n = r.SmallCount(kImitationMaxMembers.AsInt());
  for (int i = 0; i < n; i++) {
    std::string_view name = PlainName(cg);
    if (name == "constructor") {
      // A class element with this name is the constructor or an error.
      name = "x";
    }
    GenContext nested = MethodContext(ctx);
    nested.syntax.strict = true;
    nested.syntax.has_super_class = true;
    CodeGen inner(nested);
    switch (r.Index(3)) {
      case 0: {
        GenContext field = FieldContext(ctx);
        CodeGen fcg(field);
        members.push_back(
            mk::ClassField(t, name, fcg.Value(JsType::kUnknown, 1), false));
        break;
      }
      case 1: {
        Statements body =
            inner.Body(nested, r.SmallCount(kImitationMaxStatements.AsInt()));
        body.push_back(mk::Return(t, inner.Value(JsType::kUnknown, 1)));
        members.push_back(
            mk::ClassMethod(t, name, mk::Function(t, "", {}, std::move(body)),
                            MethodKind::kGet, false));
        break;
      }
      default: {
        std::vector<Expression*> params = cg.Parameters(nested, 1);
        Statements body =
            inner.Body(nested, r.SmallCount(kImitationMaxStatements.AsInt()));
        body.push_back(mk::Return(t, inner.Value(JsType::kUnknown, 1)));
        FunctionExpression* fn = mk::Function(t, "", {}, std::move(body));
        fn->params = std::move(params);
        members.push_back(mk::ClassMethod(t, name, fn));
        break;
      }
    }
  }
  return members;
}

// Fuzzilli's ImitationGenerator (CodeGenerators.swift:3396). It builds a
// stand-in for a value. A primitive gets an object with a valueOf or a
// Symbol.toPrimitive hook. A function gets a proxy. An object gets a
// subclass instance, or an object with the original as prototype.
GEN(Imitation) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view orig;
  if (Identifier* id = cg.Variable()) {
    orig = id->name;
  } else {
    // Missing data is normal: imitate a new value.
    Expression* value = cg.Value();
    Push(out, cg.Declare(value, ctx.builder.GuessExpressionType(value)));
    orig = LastDeclared(ctx);
  }
  const JsType type = TypeOf(ctx, orig);
  switch (KindOf(type, r)) {
    case ImitationKind::kPrimitive: {
      Push(out,
           cg.Declare(PrimitiveImitation(ctx, orig), JsType::kObject, "w"));
      break;
    }
    case ImitationKind::kFunction: {
      std::vector<Node*> handler;
      if (r.Chance(kImitationTrapChance())) {
        handler.push_back(ForwardingTrap(ctx));
      }
      Expression* proxy = mk::New(t, mk::Id(t, "Proxy"),
                                  {mk::Id(t, orig), mk::Object(t, handler)});
      // A proxy of a primitive throws, and the type of a variable is a guess.
      cg.DeclareMaybeGuarded(out, proxy, JsType::kFunction, "w",
                             type == JsType::kUnknown ? 1.0 : 0.0);
      break;
    }
    case ImitationKind::kObject: {
      if (r.Chance(kImitationSubclassChance())) {
        // class extends orig.constructor {...}, then new. The constructor can
        // be missing or reject the arguments, so both steps are guarded.
        Expression* super_class = mk::Member(t, mk::Id(t, orig), "constructor");
        std::vector<Node*> members = SubclassMembers(ctx, cg);
        Expression* cls = nullptr;
        if (r.Chance(kImitationClassExpressionChance())) {
          cls = mk::Class(t, "", super_class, std::move(members));
        } else {
          std::string_view name = ctx.builder.FreshName("C");
          Statements guarded = {
              mk::ClassDecl(t, name, super_class, std::move(members))};
          std::string_view holder = ctx.builder.FreshName("C");
          Push(out, mk::Let(t, holder, nullptr));
          guarded.push_back(
              cg.Stmt(mk::Assign(t, mk::Id(t, holder), mk::Id(t, name))));
          Push(out, mk::Try(t, std::move(guarded), "", {}));
          ctx.Declare(holder, JsType::kClass);
          cls = mk::Id(t, holder);
        }
        cg.DeclareMaybeGuarded(out, mk::New(t, cls, cg.Arguments(0, 2)),
                               JsType::kObject, "w", 1.0);
      } else {
        std::vector<Node*> properties = {
            mk::Prop(t, "__proto__", mk::Id(t, orig))};
        const int n = r.SmallCount(kImitationMaxMembers.AsInt());
        for (int i = 0; i < n; i++) {
          properties.push_back(
              mk::Prop(t, PlainName(cg), cg.Value(JsType::kUnknown, 1)));
        }
        Push(out, cg.Declare(mk::Object(t, std::move(properties)),
                             JsType::kObject, "w"));
      }
      break;
    }
  }
  if (r.Chance(kImitationUseChance())) {
    // Use the imitation where the original fits: a conversion, a property
    // load, a comparison or a call.
    Expression* w = mk::Id(t, LastDeclared(ctx));
    Expression* use = nullptr;
    switch (r.Index(4)) {
      case 0:
        use = mk::Binary(t, CodeGen::RandomArithmeticOp(r), w,
                         cg.Value(JsType::kNumber, 1));
        break;
      case 1:
        use = mk::Template(t, {"", ""}, {w});
        break;
      case 2:
        use = cg.Property(w);
        break;
      default:
        use = mk::Binary(t, CodeGen::RandomComparisonOp(r), w, mk::Id(t, orig));
        break;
    }
    cg.DeclareMaybeGuarded(out, use, JsType::kUnknown, "v", 1.0);
  }
  return true;
}

// --- LiteralFactory ------------------------------------------------------

REDLINE_TUNABLE(kLiteralFactoryMaxParams,
                "generators.LiteralFactory.max_params", 3, 0, 8,
                "most parameters of the factory function");
REDLINE_TUNABLE(kLiteralFactoryMaxStatements,
                "generators.LiteralFactory.max_statements", 3, 0, 16,
                "most generated statements before the returned literal");
REDLINE_TUNABLE(kLiteralFactoryMinCalls, "generators.LiteralFactory.min_calls",
                5, 1, 100, "fewest calls of the factory");
REDLINE_TUNABLE(kLiteralFactoryMaxCalls, "generators.LiteralFactory.max_calls",
                10, 1, 100, "most calls of the factory");
REDLINE_TUNABLE(kLiteralFactoryArrayChance,
                "generators.LiteralFactory.array_chance", 0.25, 0, 1,
                "probability that the factory returns an array literal "
                "instead of an object literal");
REDLINE_TUNABLE(kLiteralFactoryParamValueChance,
                "generators.LiteralFactory.param_value_chance", 0.6, 0, 1,
                "probability that a value of the literal is a parameter");
REDLINE_TUNABLE(kLiteralFactoryMaxFields,
                "generators.LiteralFactory.max_fields", 5, 1, 32,
                "most fields or elements of the returned literal");
REDLINE_TUNABLE(kLiteralFactoryCollectChance,
                "generators.LiteralFactory.collect_chance", 0.7, 0, 1,
                "probability that the loop collects the results in an array");

// Fuzzilli's ObjectBuilderFunctionGenerator (CodeGenerators.swift:629). A
// function returns a literal and runs 5-10 times, so that the literal's
// allocation site and boilerplate get feedback from many calls.
GEN(LiteralFactory) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view name = ctx.builder.FreshName("mk");
  GenContext nested = ctx.Nested(true, false, false, false);
  std::vector<std::string_view> params;
  const int np = static_cast<int>(r.Range(0, kLiteralFactoryMaxParams.AsInt()));
  for (int i = 0; i < np; i++) {
    params.push_back(ctx.builder.FreshName("a"));
    nested.Declare(params.back(), JsType::kUnknown);
  }
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, r.SmallCount(kLiteralFactoryMaxStatements.AsInt()));
  // The values of the literal: mostly the parameters, so that each call
  // makes the same shape with other values.
  auto value = [&]() -> Expression* {
    if (!params.empty() && r.Chance(kLiteralFactoryParamValueChance())) {
      return mk::Id(t, r.Pick(params));
    }
    return inner.Value(JsType::kUnknown, 1);
  };
  const int fields = 1 + r.SmallCount(kLiteralFactoryMaxFields.AsInt() - 1);
  Expression* literal = nullptr;
  if (r.Chance(kLiteralFactoryArrayChance())) {
    std::vector<Expression*> elements;
    for (int i = 0; i < fields; i++) {
      elements.push_back(value());
    }
    literal = mk::Array(t, std::move(elements));
  } else {
    std::vector<Node*> properties;
    std::vector<std::string_view> used;
    for (int i = 0; i < fields; i++) {
      std::string_view key = PlainName(cg);
      if (std::find(used.begin(), used.end(), key) != used.end()) {
        continue;
      }
      used.push_back(key);
      properties.push_back(mk::Prop(t, key, value()));
    }
    literal = mk::Object(t, std::move(properties));
  }
  body.push_back(mk::Return(t, literal));
  Push(out, mk::FunctionDecl(t, name, params, std::move(body)));
  ctx.Declare(name, JsType::kFunction);
  // The calls: a loop whose counter and the visible values are the
  // arguments.
  const int min_calls = kLiteralFactoryMinCalls.AsInt();
  const int calls = static_cast<int>(
      r.Range(min_calls, std::max(min_calls, kLiteralFactoryMaxCalls.AsInt())));
  std::string_view counter = ctx.builder.FreshName("i");
  GenContext loop = ctx.Inner(true, false);
  loop.Declare(counter, JsType::kNumber, /*is_constant=*/true);
  CodeGen lcg(loop);
  std::vector<Expression*> args;
  for (int i = 0; i < np; i++) {
    args.push_back(r.Coin() ? static_cast<Expression*>(mk::Id(t, counter))
                            : lcg.Value(JsType::kUnknown, 1));
  }
  Expression* call = mk::Call(t, mk::Id(t, name), std::move(args));
  Statement* loop_body = nullptr;
  if (r.Chance(kLiteralFactoryCollectChance())) {
    std::string_view results = ctx.builder.FreshName("objs");
    Push(out, mk::Const(t, results, mk::Array(t, {})));
    loop_body = lcg.Stmt(mk::CallMethod(t, mk::Id(t, results), "push", {call}));
    ctx.Declare(results, JsType::kArray, true);
  } else {
    loop_body = lcg.Stmt(call);
  }
  Push(out, mk::CountedLoop(t, counter, mk::Num(t, calls),
                            mk::Block(t, {loop_body})));
  return true;
}

// --- ObjectFlow ----------------------------------------------------------

REDLINE_TUNABLE(kObjectFlowMaxNames, "generators.ObjectFlow.max_names", 4, 1,
                16, "most field names that the objects of one program share");
REDLINE_TUNABLE(kObjectFlowMaxFields, "generators.ObjectFlow.max_fields", 6, 1,
                32, "most fields of a new object");
REDLINE_TUNABLE(kObjectFlowSharedNameChance,
                "generators.ObjectFlow.shared_name_chance", 0.75, 0, 1,
                "probability that a field of a new object has a shared name "
                "instead of a name of its own");
REDLINE_TUNABLE(kObjectFlowMaxMakers, "generators.ObjectFlow.max_makers", 2, 1,
                8,
                "most makers: functions, constructors and classes that make "
                "an object");
REDLINE_TUNABLE(kObjectFlowMaxMakerParams,
                "generators.ObjectFlow.max_maker_params", 2, 0, 8,
                "most parameters of a maker");
REDLINE_TUNABLE(kObjectFlowMakerMaxStatements,
                "generators.ObjectFlow.maker_max_statements", 2, 0, 16,
                "most computations in a maker before it makes the object");
REDLINE_TUNABLE(kObjectFlowConstructorChance,
                "generators.ObjectFlow.constructor_chance", 0.3, 0, 1,
                "probability that a maker is a constructor function or a "
                "class instead of a function that returns an object literal");
REDLINE_TUNABLE(kObjectFlowLocalMakerChance,
                "generators.ObjectFlow.local_maker_chance", 0.25, 0, 1,
                "probability that a literal maker is a closure in the "
                "function, where it can use the values of the function");
REDLINE_TUNABLE(kObjectFlowOutlineChance,
                "generators.ObjectFlow.outline_chance", 0.25, 0, 1,
                "probability that a maker or an accessor gets "
                "%DisableEagerInlining or %NeverOptimizeFunction");
REDLINE_TUNABLE(kObjectFlowMaxOuterObjects,
                "generators.ObjectFlow.max_outer_objects", 2, 0, 8,
                "most objects that exist before the calls of the function");
REDLINE_TUNABLE(kObjectFlowVisibleObjectChance,
                "generators.ObjectFlow.visible_object_chance", 0.3, 0, 1,
                "probability that such an object is a visible variable "
                "instead of a new object");
REDLINE_TUNABLE(kObjectFlowMaxAccessors, "generators.ObjectFlow.max_accessors",
                2, 0, 8,
                "most accessor functions: each one stores or loads one field "
                "of its parameter");
REDLINE_TUNABLE(kObjectFlowMaxParams, "generators.ObjectFlow.max_params", 3, 1,
                8, "most parameters of the function");
REDLINE_TUNABLE(kObjectFlowObjectParamChance,
                "generators.ObjectFlow.object_param_chance", 0.3, 0, 1,
                "probability that a parameter of the function is an object "
                "instead of a number");
REDLINE_TUNABLE(kObjectFlowMaxObjects, "generators.ObjectFlow.max_objects", 2,
                1, 8, "most object variables that the function declares");
REDLINE_TUNABLE(kObjectFlowMinSteps, "generators.ObjectFlow.min_steps", 3, 1,
                64, "fewest steps in the function");
REDLINE_TUNABLE(kObjectFlowMaxSteps, "generators.ObjectFlow.max_steps", 10, 1,
                64, "most steps in the function");
REDLINE_TUNABLE(kObjectFlowMaxBlockSteps,
                "generators.ObjectFlow.max_block_steps", 2, 1, 16,
                "most steps in the block of an if statement or a loop");
REDLINE_TUNABLE(kObjectFlowMaxLoopCount, "generators.ObjectFlow.max_loop_count",
                4, 1, 1000, "most iterations of a loop in the function");
REDLINE_TUNABLE(kObjectFlowStoreWeight, "generators.ObjectFlow.store_weight", 4,
                0, 100, "weight of the step `o.x = v`");
REDLINE_TUNABLE(kObjectFlowLoadWeight, "generators.ObjectFlow.load_weight", 3,
                0, 100, "weight of the step `let v = o.x`");
REDLINE_TUNABLE(kObjectFlowUpdateWeight, "generators.ObjectFlow.update_weight",
                2, 0, 100,
                "weight of a step that loads a field and stores it again");
REDLINE_TUNABLE(kObjectFlowRebindWeight, "generators.ObjectFlow.rebind_weight",
                2, 0, 100,
                "weight of a step that gives an object variable another "
                "object");
REDLINE_TUNABLE(kObjectFlowAccessorWeight,
                "generators.ObjectFlow.accessor_weight", 2, 0, 100,
                "weight of a step that calls an accessor function");
REDLINE_TUNABLE(kObjectFlowEscapeWeight, "generators.ObjectFlow.escape_weight",
                1, 0, 100,
                "weight of a step that stores an object of the function in a "
                "field of another object");
REDLINE_TUNABLE(kObjectFlowBranchWeight, "generators.ObjectFlow.branch_weight",
                2, 0, 100, "weight of an if statement with steps");
REDLINE_TUNABLE(kObjectFlowLoopWeight, "generators.ObjectFlow.loop_weight", 1,
                0, 100, "weight of a short loop with steps");
REDLINE_TUNABLE(kObjectFlowAllocationChance,
                "generators.ObjectFlow.allocation_chance", 0.6, 0, 1,
                "probability that an object comes from a maker or a literal "
                "instead of an object that exists");
REDLINE_TUNABLE(kObjectFlowNumberValueChance,
                "generators.ObjectFlow.number_value_chance", 0.7, 0, 1,
                "probability that a value is a number instead of an object, "
                "a string, a boolean, null or undefined");
REDLINE_TUNABLE(kObjectFlowDynamicValueChance,
                "generators.ObjectFlow.dynamic_value_chance", 0.5, 0, 1,
                "probability that a number is a parameter, a loaded field or "
                "a computation with them instead of a literal");
REDLINE_TUNABLE(kObjectFlowDoubleChance, "generators.ObjectFlow.double_chance",
                0.5, 0, 1,
                "probability that a number literal or a number argument is a "
                "double instead of an integer");
REDLINE_TUNABLE(kObjectFlowMaxWarmupCalls,
                "generators.ObjectFlow.max_warmup_calls", 4, 1, 64,
                "most calls of the function before the optimization request");
REDLINE_TUNABLE(kObjectFlowLoopWarmupChance,
                "generators.ObjectFlow.loop_warmup_chance", 0.3, 0, 1,
                "probability that a loop makes the calls before the "
                "optimization request");
REDLINE_TUNABLE(kObjectFlowLoopCount, "generators.ObjectFlow.loop_count", 100,
                1, 10000, "iterations of the loop that calls the function");
REDLINE_TUNABLE(kObjectFlowMaxArgumentLists,
                "generators.ObjectFlow.max_argument_lists", 2, 1, 8,
                "most different argument lists among the calls of the "
                "function");
REDLINE_TUNABLE(kObjectFlowWarmupReuseChance,
                "generators.ObjectFlow.warmup_reuse_chance", 0.9, 0, 1,
                "probability that a call before the optimization request "
                "reuses an earlier argument list");
REDLINE_TUNABLE(kObjectFlowOptimizedReuseChance,
                "generators.ObjectFlow.optimized_reuse_chance", 0.5, 0, 1,
                "probability that a call of the optimized function reuses an "
                "earlier argument list");
REDLINE_TUNABLE(kObjectFlowTurbofanChance,
                "generators.ObjectFlow.turbofan_chance", 0.6, 0, 1,
                "probability of %OptimizeFunctionOnNextCall rather than "
                "%OptimizeMaglevOnNextCall");
REDLINE_TUNABLE(kObjectFlowExtraCallChance,
                "generators.ObjectFlow.extra_call_chance", 0.3, 0, 1,
                "probability of a second call of the optimized function");
REDLINE_TUNABLE(kObjectFlowOtherTypesChance,
                "generators.ObjectFlow.other_types_chance", 0.15, 0, 1,
                "probability of a last call with objects of other shapes and "
                "values of other types, to force a deoptimization");

// A number literal that is not an integer. V8 keeps such a number as a
// double.
Expression* DoubleLiteral(CodeGen& cg) {
  NumericLiteral* literal = cg.Number()->Cast<NumericLiteral>();
  if (std::trunc(literal->value) == literal->value) {
    // cg.Number() also makes integers.
    literal->value += 0.5;
  }
  return literal;
}

// True if a store to `o.name` only changes a data property of a plain
// object. The other names can change the prototype of the object or its
// conversion to a primitive, or they are not valid as a class field.
bool IsPlainFieldName(std::string_view name) {
  return IsIdentifierName(name) && name != "__proto__" &&
         name != "constructor" && name != "valueOf" && name != "toString";
}

// Makes one ObjectFlow program. The objects of the program have a few
// shapes that share field names. Makers make the objects, and accessor
// functions store or load one field. One function moves the objects between
// its variables, and it stores and loads their fields with values of
// different representations. The program calls the function, requests its
// optimization and calls it again. The compiler can then inline the makers,
// remove allocations and specialize each field access for the shapes.
// MapTransitionFocus (template_generators.cc) makes such steps at the top
// level of a program.
class ObjectFlow {
 public:
  ObjectFlow(GenContext& ctx, CodeGen& cg, Statements* out)
      : ctx_(ctx), cg_(cg), r_(ctx.rng()), t_(ctx.tree()), out_(out) {}

  void Build() {
    const int names =
        static_cast<int>(r_.Range(1, kObjectFlowMaxNames.AsInt()));
    while (static_cast<int>(names_.size()) < names) {
      std::string_view name = cg_.PropertyName();
      if (!IsPlainFieldName(name) || Contains(names_, name)) {
        name = ctx_.builder.FreshName("p");
      }
      names_.push_back(name);
    }
    // Literal makers can be closures in the function. The function declares
    // them.
    int local_makers = 0;
    const int makers = 1 + r_.SmallCount(kObjectFlowMaxMakers.AsInt() - 1);
    for (int i = 0; i < makers; i++) {
      if (r_.Chance(kObjectFlowConstructorChance())) {
        DeclareOuterMaker(r_.Coin() ? MakerKind::kConstructor
                                    : MakerKind::kClass);
      } else if (r_.Chance(kObjectFlowLocalMakerChance())) {
        local_makers++;
      } else {
        DeclareOuterMaker(MakerKind::kLiteral);
      }
    }
    DeclareOuterObjects();
    DeclareAccessors();
    DeclareFunction(local_makers);
    CallFunction();
  }

 private:
  enum class MakerKind { kLiteral, kConstructor, kClass };
  // A function that returns an object literal, a constructor function or a
  // class.
  struct Maker {
    std::string_view name;
    int arity;
    bool construct;  // a call needs `new`
    bool local;      // a closure in the function
  };
  // `function put(o, v) { o.x = v; }` or `function get(o) { return o.x; }`.
  struct Accessor {
    std::string_view name;
    bool is_store;
  };
  // A parameter of the function. A number parameter gets doubles or
  // integers in the calls before the optimization request.
  struct Parameter {
    std::string_view name;
    bool is_object;
    bool is_double;
  };

  static bool Contains(const std::vector<std::string_view>& list,
                       std::string_view name) {
    return std::find(list.begin(), list.end(), name) != list.end();
  }
  Expression* Id(std::string_view name) { return mk::Id(t_, name); }
  Statement* Stmt(Expression* e) { return mk::ExprStmt(t_, e); }
  void PushNative(std::string_view native, std::string_view function) {
    Push(out_, Stmt(mk::Native(t_, native, {Id(function)})));
  }

  // --- values ----------------------------------------------------------------

  Expression* Number(bool is_double) {
    return is_double ? DoubleLiteral(cg_) : cg_.Integer();
  }

  // A number: a literal, a variable of values_ or an operation with such a
  // variable. A variable of values_ holds a number in most calls.
  Expression* NumberValue() {
    if (values_.empty() || !r_.Chance(kObjectFlowDynamicValueChance())) {
      return Number(r_.Chance(kObjectFlowDoubleChance()));
    }
    Expression* variable = Id(r_.Pick(values_));
    if (r_.Coin()) {
      return variable;
    }
    return mk::Binary(t_, CodeGen::RandomArithmeticOp(r_), variable,
                      Number(r_.Chance(kObjectFlowDoubleChance())));
  }

  // A value for a field or an argument. Only with `allocate`, an object
  // value can be a new object.
  Expression* Value(bool allocate) {
    if (r_.Chance(kObjectFlowNumberValueChance())) {
      return NumberValue();
    }
    if (r_.Coin()) {
      if (Expression* object = Source(allocate)) {
        return object;
      }
    }
    switch (r_.Index(3)) {
      case 0:
        return cg_.String();
      case 1:
        return cg_.Boolean();
      default:
        return cg_.Nullish();
    }
  }

  // --- objects ---------------------------------------------------------------

  // The field names of a new object, in the order of the fields. Shared
  // names are at different offsets in different shapes.
  std::vector<std::string_view> FieldNames() {
    const int count =
        static_cast<int>(r_.Range(1, kObjectFlowMaxFields.AsInt()));
    std::vector<std::string_view> unused = names_;
    std::vector<std::string_view> fields;
    for (int i = 0; i < count; i++) {
      if (!unused.empty() && r_.Chance(kObjectFlowSharedNameChance())) {
        const size_t pick = r_.Index(unused.size());
        fields.push_back(unused[pick]);
        unused.erase(unused.begin() + static_cast<ptrdiff_t>(pick));
      } else {
        fields.push_back(ctx_.builder.FreshName("p"));
      }
    }
    return fields;
  }

  Expression* Literal(const std::vector<std::string_view>& fields) {
    std::vector<Node*> properties;
    for (std::string_view field : fields) {
      properties.push_back(mk::Prop(t_, field, Value(false)));
    }
    return mk::Object(t_, std::move(properties));
  }

  // A new object: a call of a maker that is in scope, or a literal.
  Expression* Allocation() {
    std::vector<const Maker*> in_scope;
    for (const Maker& maker : makers_) {
      if (in_function_ || !maker.local) {
        in_scope.push_back(&maker);
      }
    }
    const size_t pick = r_.Index(in_scope.size() + 1);
    if (pick == in_scope.size()) {
      return Literal(FieldNames());
    }
    const Maker& maker = *in_scope[pick];
    std::vector<Expression*> args;
    for (int i = 0; i < maker.arity; i++) {
      args.push_back(Value(false));
    }
    if (maker.construct) {
      return mk::New(t_, Id(maker.name), std::move(args));
    }
    return mk::Call(t_, Id(maker.name), std::move(args));
  }

  // An object: a new object, or an object that a variable in scope holds.
  // Without `allocate`, the result is nullptr if no such variable exists.
  Expression* Source(bool allocate) {
    std::vector<std::string_view> existing = objects_;
    existing.insert(existing.end(), outers_.begin(), outers_.end());
    if (allocate &&
        (existing.empty() || r_.Chance(kObjectFlowAllocationChance()))) {
      return Allocation();
    }
    return existing.empty() ? nullptr : Id(r_.Pick(existing));
  }

  // `o.x` for an object in scope and a shared name.
  Expression* Field() { return mk::Member(t_, Source(false), r_.Pick(names_)); }

  // --- declarations before the function --------------------------------------

  // Declares a maker and adds it to makers_. The body of the maker can use
  // the variables of values_ and its parameters.
  Statement* DeclareMaker(MakerKind kind, bool local) {
    const bool construct = kind != MakerKind::kLiteral;
    std::string_view name = ctx_.builder.FreshName(construct ? "C" : "mk");
    std::vector<std::string_view> fields = FieldNames();
    std::vector<Node*> members;
    if (kind == MakerKind::kClass) {
      // Some fields are class fields. A class field cannot use the
      // parameters of the constructor.
      const size_t class_fields = r_.Index(fields.size() + 1);
      for (size_t i = 0; i < class_fields; i++) {
        members.push_back(mk::ClassField(t_, fields[i], Value(false)));
      }
      fields.erase(fields.begin(),
                   fields.begin() + static_cast<ptrdiff_t>(class_fields));
    }
    const size_t outer_values = values_.size();
    std::vector<std::string_view> params;
    const int arity =
        static_cast<int>(r_.Range(0, kObjectFlowMaxMakerParams.AsInt()));
    for (int i = 0; i < arity; i++) {
      params.push_back(ctx_.builder.FreshName("a"));
      values_.push_back(params.back());
    }
    Statements body;
    const int computations =
        r_.SmallCount(kObjectFlowMakerMaxStatements.AsInt());
    for (int i = 0; i < computations; i++) {
      std::string_view value = ctx_.builder.FreshName("v");
      body.push_back(mk::Let(t_, value, NumberValue()));
      values_.push_back(value);
    }
    if (construct) {
      for (std::string_view field : fields) {
        body.push_back(Stmt(
            mk::Assign(t_, mk::Member(t_, mk::This(t_), field), Value(false))));
      }
    } else {
      body.push_back(mk::Return(t_, Literal(fields)));
    }
    values_.resize(outer_values);
    makers_.push_back({name, arity, construct, local});
    if (kind != MakerKind::kClass) {
      return mk::FunctionDecl(t_, name, params, std::move(body));
    }
    members.push_back(mk::ClassMethod(
        t_, "constructor", mk::Function(t_, "", params, std::move(body)),
        MethodKind::kConstructor));
    return mk::ClassDecl(t_, name, nullptr, std::move(members));
  }

  void DeclareOuterMaker(MakerKind kind) {
    Push(out_, DeclareMaker(kind, /*local=*/false));
    const Maker& maker = makers_.back();
    ctx_.Declare(maker.name,
                 maker.construct ? JsType::kClass : JsType::kFunction,
                 kind == MakerKind::kClass);
    ctx_.builder.RecordInfo(maker.name)
        .parameters.assign(static_cast<size_t>(maker.arity), JsType::kNumber);
    PrepareCallee(maker.name);
  }

  // Gives `callee` feedback, so that the function can inline it. Some
  // callees get %DisableEagerInlining or %NeverOptimizeFunction. The
  // function then inlines the callee late, or it calls the callee.
  void PrepareCallee(std::string_view callee) {
    const bool outline = r_.Chance(kObjectFlowOutlineChance());
    if (outline && r_.Coin()) {
      PushNative("NeverOptimizeFunction", callee);
      return;
    }
    PushNative("PrepareFunctionForOptimization", callee);
    if (outline) {
      PushNative("DisableEagerInlining", callee);
    }
  }

  // Objects that exist before the calls of the function. Each call of the
  // function sees the fields that the earlier calls stored. Some objects
  // are visible variables. The type of a visible variable is a guess, so
  // the initializers of the new objects do not use them.
  void DeclareOuterObjects() {
    const int count = r_.SmallCount(kObjectFlowMaxOuterObjects.AsInt());
    // Select the visible variables before the declarations of the new
    // objects. A new object replaces a variable that is not applicable.
    std::vector<std::string_view> visible;
    int fresh = 0;
    for (int i = 0; i < count; i++) {
      Identifier* object = r_.Chance(kObjectFlowVisibleObjectChance())
                               ? cg_.Variable(JsType::kObject)
                               : nullptr;
      // In the function, `arguments` is the arguments object of the
      // function.
      if (object == nullptr || object->name == "arguments" ||
          Contains(visible, object->name)) {
        fresh++;
        continue;
      }
      visible.push_back(object->name);
    }
    for (int i = 0; i < fresh; i++) {
      std::string_view name = ctx_.builder.FreshName("o");
      Push(out_, mk::Const(t_, name, Allocation()));
      ctx_.Declare(name, JsType::kObject, /*is_constant=*/true);
      outers_.push_back(name);
    }
    if (!visible.empty()) {
      outers_.insert(outers_.end(), visible.begin(), visible.end());
      // A store to a visible variable can throw.
      guard_ = true;
    }
  }

  void DeclareAccessors() {
    const int count = r_.SmallCount(kObjectFlowMaxAccessors.AsInt());
    for (int i = 0; i < count; i++) {
      const bool is_store = r_.Coin();
      std::string_view name = ctx_.builder.FreshName(is_store ? "put" : "get");
      std::string_view object = ctx_.builder.FreshName("o");
      Expression* field = mk::Member(t_, Id(object), r_.Pick(names_));
      std::vector<JsType>& signature = ctx_.builder.RecordInfo(name).parameters;
      if (is_store) {
        std::string_view value = ctx_.builder.FreshName("v");
        Push(out_, mk::FunctionDecl(t_, name, {object, value},
                                    {Stmt(mk::Assign(t_, field, Id(value)))}));
        signature = {JsType::kObject, JsType::kUnknown};
      } else {
        Push(out_,
             mk::FunctionDecl(t_, name, {object}, {mk::Return(t_, field)}));
        signature = {JsType::kObject};
      }
      ctx_.Declare(name, JsType::kFunction);
      PrepareCallee(name);
      accessors_.push_back({name, is_store});
    }
  }

  // --- the function ----------------------------------------------------------

  void DeclareFunction(int local_makers) {
    function_ = ctx_.builder.FreshName("f");
    std::vector<std::string_view> names;
    std::vector<JsType> signature;
    const int count =
        static_cast<int>(r_.Range(1, kObjectFlowMaxParams.AsInt()));
    for (int i = 0; i < count; i++) {
      Parameter param = {ctx_.builder.FreshName("a"),
                         r_.Chance(kObjectFlowObjectParamChance()),
                         r_.Chance(kObjectFlowDoubleChance())};
      (param.is_object ? objects_ : values_).push_back(param.name);
      names.push_back(param.name);
      signature.push_back(param.is_object ? JsType::kObject : JsType::kNumber);
      params_.push_back(param);
    }
    in_function_ = true;
    Statements body;
    for (int i = 0; i < local_makers; i++) {
      body.push_back(DeclareMaker(MakerKind::kLiteral, /*local=*/true));
    }
    const int variables = 1 + r_.SmallCount(kObjectFlowMaxObjects.AsInt() - 1);
    for (int i = 0; i < variables; i++) {
      std::string_view name = ctx_.builder.FreshName("o");
      body.push_back(mk::Let(t_, name, Source(true)));
      objects_.push_back(name);
    }
    const int min_steps = kObjectFlowMinSteps.AsInt();
    const int steps = static_cast<int>(
        r_.Range(min_steps, std::max(min_steps, kObjectFlowMaxSteps.AsInt())));
    for (int i = 0; i < steps; i++) {
      body.push_back(Step(/*in_block=*/false));
    }
    body.push_back(mk::Return(t_, Result()));
    in_function_ = false;
    objects_.clear();
    values_.clear();
    Push(out_, mk::FunctionDecl(t_, function_, names, std::move(body)));
    ctx_.Declare(function_, JsType::kFunction);
    ctx_.builder.RecordInfo(function_).parameters = std::move(signature);
  }

  // One statement of the function. A block has no nested blocks.
  Statement* Step(bool in_block) {
    const std::vector<double> weights = {
        kObjectFlowStoreWeight(),
        kObjectFlowLoadWeight(),
        kObjectFlowUpdateWeight(),
        kObjectFlowRebindWeight(),
        accessors_.empty() ? 0 : kObjectFlowAccessorWeight(),
        kObjectFlowEscapeWeight(),
        in_block ? 0 : kObjectFlowBranchWeight(),
        in_block ? 0 : kObjectFlowLoopWeight()};
    switch (r_.Weighted(weights)) {
      case 0:
        return Stmt(mk::Assign(t_, Field(), Value(true)));
      case 1:
        return Load(Field());
      case 2:
        return Update();
      case 3:
        return Rebind();
      case 4:
        if (!accessors_.empty()) {
          return AccessorCall();
        }
        return Load(Field());
      case 5:
        // An object of the function becomes reachable from another object.
        return Stmt(mk::Assign(t_, Field(), Id(r_.Pick(objects_))));
      case 6:
        // Random::Weighted can select a step of weight 0 if all weights
        // are 0. A block then gets a load instead of a nested block.
        if (!in_block) {
          return Branch();
        }
        return Load(Field());
      default:
        if (!in_block) {
          return Loop();
        }
        return Load(Field());
    }
  }

  // `o = source;`. The variable can get an object of another shape. The
  // next field accesses on the variable then see more than one shape.
  Statement* Rebind() {
    std::string_view variable = r_.Pick(objects_);
    Expression* source = Source(true);
    if (auto* same = source->As<Identifier>();
        same != nullptr && same->name == variable) {
      // The variable holds this object already.
      source = Allocation();
    }
    return Stmt(mk::Assign(t_, Id(variable), source));
  }

  // `let v = value;`. The next steps can use v as a value.
  Statement* Load(Expression* value) {
    std::string_view name = ctx_.builder.FreshName("v");
    Statement* load = mk::Let(t_, name, value);
    values_.push_back(name);
    return load;
  }

  // `o.x += n`, `o.x++` or `o.x = o.y * n`.
  Statement* Update() {
    switch (r_.Index(3)) {
      case 0:
        return Stmt(mk::Assign(t_, Field(), NumberValue(),
                               CodeGen::RandomCompoundAssignOp(r_)));
      case 1:
        return Stmt(mk::Update(
            t_, r_.Coin() ? UpdateOp::kIncrement : UpdateOp::kDecrement,
            r_.Coin(), Field()));
      default:
        return Stmt(mk::Assign(t_, Field(),
                               mk::Binary(t_, CodeGen::RandomArithmeticOp(r_),
                                          Field(), NumberValue())));
    }
  }

  // A call of an accessor function. The accessor sees the shapes of all
  // objects that the calls give to it.
  Statement* AccessorCall() {
    const Accessor& accessor = r_.Pick(accessors_);
    if (accessor.is_store) {
      return Stmt(
          mk::Call(t_, Id(accessor.name), {Source(false), Value(true)}));
    }
    return Load(mk::Call(t_, Id(accessor.name), {Source(false)}));
  }

  // The steps of an if statement or a loop. A variable that a step
  // declares in the block is not in scope after the block.
  Statement* Block() {
    const size_t outer_values = values_.size();
    Statements steps;
    const int count = 1 + r_.SmallCount(kObjectFlowMaxBlockSteps.AsInt() - 1);
    for (int i = 0; i < count; i++) {
      steps.push_back(Step(/*in_block=*/true));
    }
    values_.resize(outer_values);
    return mk::Block(t_, std::move(steps));
  }

  // `if (v < n) {...} else {...}`. An object variable can hold objects of
  // different shapes after the statement.
  Statement* Branch() {
    Expression* left = values_.empty() ? Field() : Id(r_.Pick(values_));
    Expression* test =
        mk::Binary(t_, CodeGen::RandomComparisonOp(r_), left, NumberValue());
    Statement* consequent = Block();
    return mk::If(t_, test, consequent, r_.Coin() ? Block() : nullptr);
  }

  Statement* Loop() {
    std::string_view counter = ctx_.builder.FreshName("i");
    const int count =
        static_cast<int>(r_.Range(1, kObjectFlowMaxLoopCount.AsInt()));
    values_.push_back(counter);
    Statement* body = Block();
    values_.pop_back();
    return mk::CountedLoop(t_, counter, mk::Num(t_, count), body);
  }

  // The result of the function: an object, the sum of two fields or a
  // number.
  Expression* Result() {
    switch (r_.Index(3)) {
      case 0:
        return Id(r_.Pick(objects_));
      case 1:
        return mk::Binary(t_, BinaryOp::kAdd, Field(), Field());
      default:
        return NumberValue();
    }
  }

  // --- the calls -------------------------------------------------------------

  // Arguments for the function. A number argument has the representation
  // of its parameter. With `counter`, some number arguments follow this
  // loop counter.
  std::vector<Expression*> Arguments(std::string_view counter = {}) {
    std::vector<Expression*> args;
    for (const Parameter& param : params_) {
      if (param.is_object) {
        args.push_back(Source(true));
      } else if (counter.empty() || r_.Coin()) {
        args.push_back(Number(param.is_double));
      } else if (param.is_double) {
        args.push_back(
            mk::Binary(t_, BinaryOp::kAdd, Id(counter), DoubleLiteral(cg_)));
      } else {
        args.push_back(Id(counter));
      }
    }
    return args;
  }

  // Arguments that the optimized function did not see: objects of new
  // shapes, and numbers of the other representation or other values.
  std::vector<Expression*> OtherArguments() {
    std::vector<Expression*> args;
    for (const Parameter& param : params_) {
      if (param.is_object) {
        args.push_back(Literal(FieldNames()));
        continue;
      }
      switch (r_.Index(4)) {
        case 0:
          args.push_back(Number(!param.is_double));
          break;
        case 1:
          args.push_back(cg_.String());
          break;
        case 2:
          args.push_back(cg_.Nullish());
          break;
        default:
          args.push_back(Source(true));
          break;
      }
    }
    return args;
  }

  // A call of the function. If the call can throw, it is in a try
  // statement. In a loop, the handler ends the loop: a call that throws
  // once usually throws in each iteration.
  Statement* Call(std::vector<Expression*> args, bool in_loop = false) {
    Statement* call = Stmt(mk::Call(t_, Id(function_), std::move(args)));
    if (!guard_) {
      return call;
    }
    Statements handler;
    if (in_loop) {
      handler.push_back(mk::Break(t_));
    }
    return mk::Try(t_, {call}, ctx_.builder.FreshName("e"), std::move(handler));
  }

  void CallFunction() {
    PushNative("PrepareFunctionForOptimization", function_);
    ArgumentLists lists(
        cg_, [&]() { return Arguments(); },
        kObjectFlowMaxArgumentLists.AsInt());
    // A long loop in a loop of the program costs too much time.
    if (!ctx_.syntax.in_loop && r_.Chance(kObjectFlowLoopWarmupChance())) {
      std::string_view counter = ctx_.builder.FreshName("i");
      Statement* call = Call(Arguments(counter), /*in_loop=*/true);
      Push(out_, mk::CountedLoop(t_, counter,
                                 mk::Num(t_, kObjectFlowLoopCount.AsInt()),
                                 mk::Block(t_, {call})));
    } else {
      const int warmup =
          1 + r_.SmallCount(kObjectFlowMaxWarmupCalls.AsInt() - 1);
      for (int i = 0; i < warmup; i++) {
        Push(out_,
             Call(lists.Next(i == 0 ? 1.0 : kObjectFlowWarmupReuseChance())));
      }
    }
    PushNative(r_.Chance(kObjectFlowTurbofanChance())
                   ? "OptimizeFunctionOnNextCall"
                   : "OptimizeMaglevOnNextCall",
               function_);
    cg_.DeclareMaybeGuarded(
        out_,
        mk::Call(t_, Id(function_),
                 lists.Next(kObjectFlowOptimizedReuseChance())),
        JsType::kUnknown, "r", guard_ ? 1.0 : 0.0);
    if (r_.Chance(kObjectFlowExtraCallChance())) {
      Push(out_, Call(lists.Next(kObjectFlowOptimizedReuseChance())));
    }
    if (r_.Chance(kObjectFlowOtherTypesChance())) {
      Push(out_, Call(OtherArguments()));
    }
  }

  GenContext& ctx_;
  CodeGen& cg_;
  Random& r_;
  Tree* t_;
  Statements* out_;
  // The field names that the objects share.
  std::vector<std::string_view> names_;
  std::vector<Maker> makers_;
  std::vector<Accessor> accessors_;
  // Variables with an object that exists before the calls of the function.
  std::vector<std::string_view> outers_;
  std::vector<Parameter> params_;
  std::string_view function_;
  // True while the builder makes the body of the function.
  bool in_function_ = false;
  // True if a call of the function can throw. The calls are then in a try
  // statement.
  bool guard_ = false;
  // The object variables and the value variables in scope at the current
  // position.
  std::vector<std::string_view> objects_;
  std::vector<std::string_view> values_;
};

GEN(ObjectFlow) {
  ObjectFlow(ctx, cg, out).Build();
  return true;
}

// --- NumberChain ---------------------------------------------------------

REDLINE_TUNABLE(kNumberChainMinSteps, "generators.NumberChain.min_steps", 3, 1,
                32, "fewest dependent computations");
REDLINE_TUNABLE(kNumberChainMaxSteps, "generators.NumberChain.max_steps", 7, 1,
                32, "most dependent computations");
REDLINE_TUNABLE(kNumberChainMaxVariables,
                "generators.NumberChain.max_variables", 3, 0, 8,
                "most visible variables taken as inputs");
REDLINE_TUNABLE(kNumberChainUnknownInputChance,
                "generators.NumberChain.unknown_input_chance", 0.2, 0, 1,
                "probability that an input variable can have any type, not "
                "only the number type");
REDLINE_TUNABLE(kNumberChainSpecialChance,
                "generators.NumberChain.special_chance", 0.15, 0, 1,
                "probability that a literal operand is -0, NaN or an infinity");
REDLINE_TUNABLE(kNumberChainLiteralOperandChance,
                "generators.NumberChain.literal_operand_chance", 0.25, 0, 1,
                "probability that an operand is a new literal instead of an "
                "earlier value of the chain");
REDLINE_TUNABLE(kNumberChainLogicalChance,
                "generators.NumberChain.logical_chance", 0.05, 0, 1,
                "probability that a binary step uses &&, || or ??");
REDLINE_TUNABLE(kNumberChainUpdateChance,
                "generators.NumberChain.update_chance", 0.15, 0, 1,
                "probability that a unary step is ++ or -- on an earlier step");
REDLINE_TUNABLE(kNumberChainFunctionChance,
                "generators.NumberChain.function_chance", 0.3, 0, 1,
                "probability that the chain is the body of a function whose "
                "parameters are inputs, warmed up and optimized");
REDLINE_TUNABLE(kNumberChainWarmupCalls, "generators.NumberChain.warmup_calls",
                2, 1, 16, "calls of the chain function before optimization");

// The Math functions with their arity. Math.random is left out because its
// results change between runs.
struct MathFunction {
  const char* name;
  int min_args;
  int max_args;
};
constexpr MathFunction kMathFunctions[] = {
    {"abs", 1, 1},   {"acos", 1, 1},   {"acosh", 1, 1},
    {"asin", 1, 1},  {"asinh", 1, 1},  {"atan", 1, 1},
    {"atanh", 1, 1}, {"atan2", 2, 2},  {"cbrt", 1, 1},
    {"ceil", 1, 1},  {"clz32", 1, 1},  {"cos", 1, 1},
    {"cosh", 1, 1},  {"exp", 1, 1},    {"expm1", 1, 1},
    {"floor", 1, 1}, {"fround", 1, 1}, {"f16round", 1, 1},
    {"hypot", 0, 4}, {"imul", 2, 2},   {"log", 1, 1},
    {"log1p", 1, 1}, {"log10", 1, 1},  {"log2", 1, 1},
    {"max", 0, 4},   {"min", 0, 4},    {"pow", 2, 2},
    {"round", 1, 1}, {"sign", 1, 1},   {"sin", 1, 1},
    {"sinh", 1, 1},  {"sqrt", 1, 1},   {"tan", 1, 1},
    {"tanh", 1, 1},  {"trunc", 1, 1},  {"sumPrecise", 1, 1}};

// A number literal: an interesting integer or double of the environment, a
// small random one, or -0, NaN or an infinity.
Expression* NumberLiteral(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  if (r.Chance(kNumberChainSpecialChance())) {
    switch (r.Index(4)) {
      case 0:
        return mk::Unary(t, UnaryOp::kMinus, mk::Num(t, 0));
      case 1:
        return mk::Id(t, "NaN");
      case 2:
        return mk::Id(t, "Infinity");
      default:
        return mk::Unary(t, UnaryOp::kMinus, mk::Id(t, "Infinity"));
    }
  }
  return r.Coin() ? cg.Integer() : cg.Number();
}

// Fuzzilli's NumberComputationGenerator (CodeGenerators.swift:3349). It
// makes 3-7 dependent `let` statements. Each one applies an operator or a
// Math function to the inputs and to the earlier results.
GEN(NumberChain) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const bool in_function = r.Chance(kNumberChainFunctionChance());
  GenContext fctx = ctx.Nested(true, false, false, false);
  GenContext& chain_ctx = in_function ? fctx : ctx;
  CodeGen ccg(chain_ctx);
  // The inputs: visible variables, or parameters of the function.
  std::vector<std::string_view> inputs;
  std::vector<std::string_view> params;
  const int max_variables = kNumberChainMaxVariables.AsInt();
  const int variables =
      max_variables > 0
          ? 1 + static_cast<int>(r.Index(static_cast<size_t>(max_variables)))
          : 0;
  for (int i = 0; i < variables; i++) {
    if (in_function) {
      params.push_back(ctx.builder.FreshName("a"));
      fctx.Declare(params.back(), JsType::kNumber);
      inputs.push_back(params.back());
      continue;
    }
    Identifier* id = cg.Variable(r.Chance(kNumberChainUnknownInputChance())
                                     ? JsType::kUnknown
                                     : JsType::kNumber);
    if (id != nullptr &&
        std::find(inputs.begin(), inputs.end(), id->name) == inputs.end()) {
      inputs.push_back(id->name);
    }
  }
  Statements steps;
  // The results of the chain. Only these are mutable by ++ and --.
  std::vector<std::string_view> results;
  auto operand = [&]() -> Expression* {
    const size_t known = inputs.size() + results.size();
    if (known == 0 || r.Chance(kNumberChainLiteralOperandChance())) {
      return NumberLiteral(chain_ctx, ccg);
    }
    const size_t index = r.Index(known);
    return mk::Id(t, index < inputs.size() ? inputs[index]
                                           : results[index - inputs.size()]);
  };
  const int min_steps = kNumberChainMinSteps.AsInt();
  const int count = static_cast<int>(
      r.Range(min_steps, std::max(min_steps, kNumberChainMaxSteps.AsInt())));
  for (int i = 0; i < count; i++) {
    Expression* value = nullptr;
    switch (r.Index(3)) {
      case 0: {
        Expression* left = operand();
        Expression* right = operand();
        if (r.Chance(kNumberChainLogicalChance())) {
          value = mk::Logical(t, CodeGen::RandomLogicalOp(r), left, right);
          break;
        }
        BinaryOp op = CodeGen::RandomArithmeticOp(r);
        if (op == BinaryOp::kExp && left->Is<UnaryExpression>()) {
          // `-a ** b` is a syntax error.
          op = BinaryOp::kMul;
        }
        value = mk::Binary(t, op, left, right);
        break;
      }
      case 1: {
        if (!results.empty() && r.Chance(kNumberChainUpdateChance())) {
          value = mk::Update(
              t, r.Coin() ? UpdateOp::kIncrement : UpdateOp::kDecrement,
              r.Coin(), mk::Id(t, r.Pick(results)));
          break;
        }
        value = mk::Unary(t,
                          r.Pick({UnaryOp::kMinus, UnaryOp::kPlus,
                                  UnaryOp::kBitNot, UnaryOp::kNot}),
                          operand());
        break;
      }
      default: {
        const MathFunction& f = kMathFunctions[r.Index(
            sizeof(kMathFunctions) / sizeof(kMathFunctions[0]))];
        const int n = static_cast<int>(r.Range(f.min_args, f.max_args));
        std::vector<Expression*> args;
        for (int k = 0; k < n; k++) {
          args.push_back(operand());
        }
        if (std::string_view(f.name) == "sumPrecise") {
          // Math.sumPrecise takes an iterable of numbers.
          std::vector<Expression*> elements = {operand(), operand()};
          args = {mk::Array(t, std::move(elements))};
        }
        value = mk::CallMethod(t, mk::Id(t, "Math"), f.name, std::move(args));
        break;
      }
    }
    std::string_view name = ctx.builder.FreshName("t");
    steps.push_back(mk::Let(t, name, value));
    chain_ctx.Declare(name, JsType::kNumber);
    results.push_back(name);
  }
  if (!in_function) {
    for (Statement* s : steps) {
      Push(out, s);
    }
    return true;
  }
  // A function with the chain as body returns the last result. It is warmed
  // up, optimized and called with other inputs.
  steps.push_back(mk::Return(t, mk::Id(t, results.back())));
  std::string_view fn = ctx.builder.FreshName("f");
  Push(out, mk::FunctionDecl(t, fn, params, std::move(steps)));
  ctx.Declare(fn, JsType::kFunction);
  auto call = [&]() -> Expression* {
    std::vector<Expression*> args;
    for (size_t i = 0; i < params.size(); i++) {
      Identifier* id = cg.Variable(JsType::kNumber);
      args.push_back(id != nullptr && r.Coin() ? static_cast<Expression*>(id)
                                               : NumberLiteral(ctx, cg));
    }
    return mk::Call(t, mk::Id(t, fn), std::move(args));
  };
  Push(out, cg.Stmt(mk::Native(t, "PrepareFunctionForOptimization",
                               {mk::Id(t, fn)})));
  for (int i = 0; i < kNumberChainWarmupCalls.AsInt(); i++) {
    Push(out, cg.Stmt(call()));
  }
  Push(out, cg.Stmt(mk::Native(t,
                               r.Coin() ? "OptimizeFunctionOnNextCall"
                                        : "OptimizeMaglevOnNextCall",
                               {mk::Id(t, fn)})));
  Push(out, cg.Declare(call(), JsType::kNumber));
  return true;
}

// --- Control flow --------------------------------------------------------

REDLINE_TUNABLE(kTryFinallyMaxStatements,
                "generators.TryFinally.max_statements", 3, 1, 16,
                "most generated statements in a try or finally block");
REDLINE_TUNABLE(kTryFinallyThrowChance, "generators.TryFinally.throw_chance",
                0.4, 0, 1,
                "probability that the try block throws when a runtime "
                "condition holds");
REDLINE_TUNABLE(kTryFinallyNestedChance, "generators.TryFinally.nested_chance",
                0.25, 0, 1,
                "probability of a second catch-less try/finally around the "
                "first one");
REDLINE_TUNABLE(kTryFinallyAfterChance, "generators.TryFinally.after_chance",
                0.5, 0, 1,
                "probability of statements after the try/finally that run "
                "only on normal completion");
REDLINE_TUNABLE(kTryFinallyHandlerChance,
                "generators.TryFinally.handler_chance", 0.3, 0, 1,
                "probability that the guarding catch block gets statements");
REDLINE_TUNABLE(kTryFinallyRuntimeConditionChance,
                "generators.TryFinally.runtime_condition_chance", 0.7, 0, 1,
                "probability that the throw condition is a runtime value "
                "rather than true");

// A condition whose value is known only at run time: a boolean variable or
// a condition expression, negated half of the time.
Expression* RuntimeCondition(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Expression* condition = cg.Variable(JsType::kBoolean);
  if (condition == nullptr) {
    condition = cg.Condition(1);
  }
  if (r.Coin()) {
    condition = mk::Unary(T(ctx), UnaryOp::kNot, condition);
  }
  return condition;
}

// `try {...} finally {...}` without a catch clause.
TryStatement* TryFinallyStatement(Tree* t, Statements block,
                                  Statements finalizer) {
  TryStatement* s = t->New<TryStatement>();
  s->block = mk::Block(t, std::move(block));
  s->finalizer = mk::Block(t, std::move(finalizer));
  return s;
}

// Fuzzilli's TryFinallyGenerator (CodeGenerators.swift:3094) and
// ConditionalThrowGenerator (:3123). An exception of the try block runs the
// finally block and then propagates, so an outer try/catch guards it. Code
// after the try/finally runs only on normal completion. The generated code
// in both blocks can also contain break, continue and return.
GEN(TryFinally) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const int max_statements = kTryFinallyMaxStatements.AsInt();
  auto make_body = [&](GenContext& block_ctx) {
    CodeGen bcg(block_ctx);
    return bcg.Body(block_ctx, 1 + r.SmallCount(max_statements - 1));
  };
  GenContext try_ctx = ctx.Inner(false, false);
  CodeGen tcg(try_ctx);
  // The throw comes before the body: its condition must not use the
  // variables that the body declares.
  Statement* thrower = nullptr;
  if (r.Chance(kTryFinallyThrowChance())) {
    Expression* condition = r.Chance(kTryFinallyRuntimeConditionChance())
                                ? RuntimeCondition(try_ctx, tcg)
                                : static_cast<Expression*>(mk::Bool(t, true));
    thrower =
        mk::If(t, condition, mk::Throw(t, tcg.Value(JsType::kUnknown, 1)));
  }
  Statements block = make_body(try_ctx);
  if (thrower != nullptr) {
    block.insert(
        block.begin() + static_cast<std::ptrdiff_t>(r.Index(block.size() + 1)),
        thrower);
  }
  GenContext fin_ctx = ctx.Inner(false, false);
  Statements finalizer = make_body(fin_ctx);
  Statement* statement =
      TryFinallyStatement(t, std::move(block), std::move(finalizer));
  if (r.Chance(kTryFinallyNestedChance())) {
    GenContext outer_fin_ctx = ctx.Inner(false, false);
    Statements outer_finalizer = make_body(outer_fin_ctx);
    statement = TryFinallyStatement(t, {statement}, std::move(outer_finalizer));
  }
  Statements guarded = {statement};
  if (r.Chance(kTryFinallyAfterChance())) {
    GenContext after_ctx = ctx.Inner(false, false);
    Statements after = make_body(after_ctx);
    guarded.insert(guarded.end(), after.begin(), after.end());
  }
  std::string_view param = ctx.builder.FreshName("e");
  Statements handler;
  if (r.Chance(kTryFinallyHandlerChance())) {
    GenContext catch_ctx = ctx.Inner(false, false);
    catch_ctx.Declare(param, JsType::kUnknown);
    CodeGen ccg(catch_ctx);
    handler = ccg.Body(catch_ctx, 1 + r.SmallCount(max_statements - 1));
  }
  Push(out, mk::Try(t, std::move(guarded), param, std::move(handler)));
  return true;
}

REDLINE_TUNABLE(kComplexForLoopMaxStatements,
                "generators.ComplexForLoop.max_statements", 4, 1, 16,
                "most generated statements in the loop body");
REDLINE_TUNABLE(kComplexForLoopMaxVariables,
                "generators.ComplexForLoop.max_variables", 3, 2, 8,
                "most loop variables in a header with several variables");
REDLINE_TUNABLE(kComplexForLoopNoVariableChance,
                "generators.ComplexForLoop.no_variable_chance", 0.4, 0, 1,
                "probability of a loop without loop variables, counted down "
                "by its test or its update");
REDLINE_TUNABLE(kComplexForLoopEmptyTestChance,
                "generators.ComplexForLoop.empty_test_chance", 0.3, 0, 1,
                "probability that the test is empty and an if/break at the "
                "start of the body ends the loop");
REDLINE_TUNABLE(kComplexForLoopEmptyUpdateChance,
                "generators.ComplexForLoop.empty_update_chance", 0.2, 0, 1,
                "probability that the update is empty and the body ends with "
                "the update");
REDLINE_TUNABLE(kComplexForLoopVarChance,
                "generators.ComplexForLoop.var_chance", 0.2, 0, 1,
                "probability that the loop variables are var instead of let");
REDLINE_TUNABLE(kComplexForLoopMaxIterations,
                "generators.ComplexForLoop.max_iterations", 10, 1, 1000,
                "most iterations of the loop");

// Fuzzilli's ComplexForLoopGenerator (CodeGenerators.swift:2872): `for`
// headers with several variables, or with an empty init, test or update.
// The loop variables are constant for the generated body, so that the loop
// always ends.
GEN(ComplexForLoop) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const int iterations = 1 + static_cast<int>(r.Index(static_cast<size_t>(
                                 kComplexForLoopMaxIterations.AsInt())));
  GenContext inner = ctx.Inner(true, false);
  Node* init = nullptr;
  Expression* test = nullptr;
  Expression* update = nullptr;
  Statements prologue;  // statements at the start of the body
  Statements epilogue;  // statements at the end of the body
  if (r.Chance(kComplexForLoopNoVariableChance())) {
    // A counter before the loop, and `n--` as the test or the update.
    std::string_view counter = ctx.builder.FreshName("n");
    Push(out, mk::Let(t, counter, mk::Num(t, iterations)));
    Expression* dec =
        mk::Update(t, UpdateOp::kDecrement, false, mk::Id(t, counter));
    if (r.Coin()) {
      test = dec;
    } else {
      test = mk::Binary(t, BinaryOp::kGreaterThan, mk::Id(t, counter),
                        mk::Num(t, 0));
      update = dec;
    }
    inner.Declare(counter, JsType::kNumber, /*is_constant=*/true);
  } else {
    // Several variables: `i` counts up to the bound `j`, and the update
    // changes all of them.
    const int max_vars = std::max(2, kComplexForLoopMaxVariables.AsInt());
    const int nvars = static_cast<int>(r.Range(2, max_vars));
    VariableDeclaration* decl =
        mk::Declare(t,
                    r.Chance(kComplexForLoopVarChance()) ? VariableKind::kVar
                                                         : VariableKind::kLet,
                    ctx.builder.FreshName("i"), mk::Num(t, 0));
    std::vector<std::string_view> vars = {
        decl->declarations[0]->target->Cast<Identifier>()->name};
    for (int i = 1; i < nvars; i++) {
      VariableDeclarator* d = t->New<VariableDeclarator>();
      std::string_view name = ctx.builder.FreshName(i == 1 ? "j" : "k");
      d->target = mk::Id(t, name);
      // The second variable is the bound, the others are values that the
      // update changes too.
      d->init = i == 1 ? static_cast<Expression*>(mk::Num(t, iterations))
                       : cg.Value(JsType::kNumber, 1);
      decl->declarations.push_back(d);
      vars.push_back(name);
    }
    init = decl;
    test = mk::Binary(t, BinaryOp::kLessThan, mk::Id(t, vars[0]),
                      mk::Id(t, vars[1]));
    std::vector<Expression*> updates = {
        mk::Update(t, UpdateOp::kIncrement, false, mk::Id(t, vars[0]))};
    if (r.Coin()) {
      updates.push_back(
          mk::Update(t, UpdateOp::kDecrement, false, mk::Id(t, vars[1])));
    }
    for (size_t i = 2; i < vars.size(); i++) {
      updates.push_back(mk::Assign(t, mk::Id(t, vars[i]),
                                   cg.Value(JsType::kNumber, 1),
                                   CodeGen::RandomCompoundAssignOp(r)));
    }
    update = updates.size() == 1 ? updates[0] : mk::Seq(t, updates);
    for (std::string_view v : vars) {
      inner.Declare(v, JsType::kNumber, /*is_constant=*/true);
    }
  }
  if (r.Chance(kComplexForLoopEmptyTestChance()) && test != nullptr) {
    // An empty test: an if/break at the start of the body ends the loop.
    prologue.push_back(
        mk::If(t, mk::Unary(t, UnaryOp::kNot, test), mk::Break(t)));
    test = nullptr;
  }
  if (r.Chance(kComplexForLoopEmptyUpdateChance()) && update != nullptr) {
    // The update moves to the end of the body. A continue in the body skips
    // it, so the loop also counts its iterations in its test.
    epilogue.push_back(cg.Stmt(update));
    update = nullptr;
    std::string_view guard = ctx.builder.FreshName("g");
    Push(out, mk::Let(t, guard, mk::Num(t, 0)));
    prologue.insert(prologue.begin(),
                    mk::If(t,
                           mk::Binary(t, BinaryOp::kGreaterThan,
                                      mk::Update(t, UpdateOp::kIncrement, false,
                                                 mk::Id(t, guard)),
                                      mk::Num(t, iterations)),
                           mk::Break(t)));
  }
  CodeGen icg(inner);
  Statements body = icg.Body(
      inner, 1 + r.SmallCount(kComplexForLoopMaxStatements.AsInt() - 1));
  body.insert(body.begin(), prologue.begin(), prologue.end());
  body.insert(body.end(), epilogue.begin(), epilogue.end());
  Push(out, mk::For(t, init, test, update, mk::Block(t, std::move(body))));
  return true;
}

REDLINE_TUNABLE(kLabeledLoopsMaxStatements,
                "generators.LabeledLoops.max_statements", 3, 1, 16,
                "most generated statements in each loop body");
REDLINE_TUNABLE(kLabeledLoopsMaxJumps, "generators.LabeledLoops.max_jumps", 3,
                1, 8, "most labeled break and continue statements");
REDLINE_TUNABLE(kLabeledLoopsMaxIterations,
                "generators.LabeledLoops.max_iterations", 8, 1, 1000,
                "most iterations of each loop");
REDLINE_TUNABLE(kLabeledLoopsBlockChance,
                "generators.LabeledLoops.block_chance", 0.3, 0, 1,
                "probability of a labeled block left by break from nested "
                "if, switch and loop statements, instead of labeled loops");
REDLINE_TUNABLE(kLabeledLoopsDepth3Chance,
                "generators.LabeledLoops.depth3_chance", 0.25, 0, 1,
                "probability of a third nested labeled loop");
REDLINE_TUNABLE(kLabeledLoopsTryChance, "generators.LabeledLoops.try_chance",
                0.3, 0, 1,
                "probability that a try/catch encloses the labeled statement, "
                "so that the jumps also leave a try block");

// A loop of `iterations` iterations with an empty body. It is a counted for
// loop, a while loop with a counter declared in `out`, or a for-of loop over
// an array of values. `inner` gets the counter as a constant.
Statement* BoundedLoop(GenContext& ctx, CodeGen& cg, Statements* out,
                       GenContext& inner, int iterations) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  switch (r.Index(3)) {
    case 0: {
      std::string_view counter = ctx.builder.FreshName("i");
      inner.Declare(counter, JsType::kNumber, /*is_constant=*/true);
      return mk::CountedLoop(t, counter, mk::Num(t, iterations),
                             mk::Block(t, {}));
    }
    case 1: {
      std::string_view counter = ctx.builder.FreshName("n");
      Push(out, mk::Let(t, counter, mk::Num(t, 0)));
      inner.Declare(counter, JsType::kNumber, /*is_constant=*/true);
      Expression* test = mk::Binary(
          t, BinaryOp::kLessThan,
          mk::Update(t, UpdateOp::kIncrement, false, mk::Id(t, counter)),
          mk::Num(t, iterations));
      return mk::While(t, test, mk::Block(t, {}));
    }
    default: {
      std::vector<Expression*> elements;
      for (int i = 0; i < iterations; i++) {
        elements.push_back(cg.Value(JsType::kUnknown, 1));
      }
      std::string_view element = ctx.builder.FreshName("e");
      inner.Declare(element, JsType::kUnknown, /*is_constant=*/true);
      return mk::ForOf(t, VariableKind::kConst, element,
                       mk::Array(t, std::move(elements)), mk::Block(t, {}));
    }
  }
}

// The block of a loop made by BoundedLoop().
BlockStatement* LoopBlock(Statement* loop) {
  if (auto* s = loop->As<ForStatement>()) {
    return s->body->Cast<BlockStatement>();
  }
  if (auto* s = loop->As<WhileStatement>()) {
    return s->body->Cast<BlockStatement>();
  }
  return loop->Cast<ForOfStatement>()->body->Cast<BlockStatement>();
}

// `jump` directly, in an if block or in a switch case. Call this before the
// generation of the body that gets the jump: the condition must not use the
// variables that the body declares.
Statement* WrapJump(GenContext& ctx, CodeGen& cg, Statement* jump) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  switch (r.Index(3)) {
    case 0:
      return mk::If(t, RuntimeCondition(ctx, cg), mk::Block(t, {jump}));
    case 1: {
      Statements consequent = {cg.Stmt(cg.Value(JsType::kUnknown, 1)), jump};
      return mk::If(t, RuntimeCondition(ctx, cg),
                    mk::Block(t, std::move(consequent)), mk::Block(t, {}));
    }
    default: {
      std::vector<SwitchCase*> cases = {mk::Case(t, cg.Integer(), {jump}),
                                        mk::Case(t, nullptr, {})};
      return mk::Switch(t, cg.Value(JsType::kNumber, 1), std::move(cases));
    }
  }
}

// Inserts `statements` together at a random position of `body`.
void InsertAtRandom(Random& r, Statements* body, const Statements& statements) {
  body->insert(
      body->begin() + static_cast<std::ptrdiff_t>(r.Index(body->size() + 1)),
      statements.begin(), statements.end());
}

// Fuzzilli's labeled LoopBreakGenerator and ContinueGenerator
// (CodeGenerators.swift:3026, :3031) and BlockBreakGenerator (:3345).
// Nested labeled loops with `break L` and `continue L` that leave the inner
// loops, also from if and switch blocks, or a labeled block left by `break`
// from nested statements. The generated bodies can use the labels too.
GEN(LabeledLoops) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const int max_statements = kLabeledLoopsMaxStatements.AsInt();
  const int max_iterations = kLabeledLoopsMaxIterations.AsInt();
  if (r.Chance(kLabeledLoopsBlockChance())) {
    // A labeled block. Nested if, switch and loop statements leave it with
    // `break L`.
    std::string_view label = ctx.builder.FreshName("L");
    GenContext inner = ctx.Inner(false, false);
    inner.syntax.labels.push_back(label);
    CodeGen icg(inner);
    // The jumps come first, and the body after them.
    std::vector<Statements> units;
    const int jumps = 1 + r.SmallCount(kLabeledLoopsMaxJumps.AsInt() - 1);
    for (int i = 0; i < jumps; i++) {
      if (r.Coin()) {
        units.push_back({WrapJump(inner, icg, mk::Break(t, label))});
        continue;
      }
      // A break of the block from a loop inside it.
      Statements unit;
      GenContext loop_ctx = inner.Inner(true, false);
      Statement* loop = BoundedLoop(
          inner, icg, &unit, loop_ctx,
          1 + static_cast<int>(r.Index(static_cast<size_t>(max_iterations))));
      CodeGen lcg(loop_ctx);
      Statement* jump = WrapJump(loop_ctx, lcg, mk::Break(t, label));
      Statements statements = lcg.Body(loop_ctx, r.SmallCount(max_statements));
      InsertAtRandom(r, &statements, {jump});
      LoopBlock(loop)->body = std::move(statements);
      unit.push_back(loop);
      units.push_back(std::move(unit));
    }
    Statements body = icg.Body(inner, 1 + r.SmallCount(max_statements - 1));
    for (const Statements& unit : units) {
      InsertAtRandom(r, &body, unit);
    }
    Statement* labeled = mk::Labeled(t, label, mk::Block(t, std::move(body)));
    Push(out, r.Chance(kLabeledLoopsTryChance())
                  ? mk::Try(t, {labeled}, ctx.builder.FreshName("e"), {})
                  : labeled);
    return true;
  }
  // Nested labeled loops. The innermost loop has `break` and `continue`
  // statements with the labels of all loops.
  const int depth = r.Chance(kLabeledLoopsDepth3Chance()) ? 3 : 2;
  std::vector<std::string_view> labels;
  std::vector<Statement*> loops;
  std::vector<GenContext> contexts;
  contexts.reserve(static_cast<size_t>(depth));
  GenContext* parent = &ctx;
  Statements prelude;  // counters of while loops, before the outer loop
  for (int level = 0; level < depth; level++) {
    std::string_view label = ctx.builder.FreshName("L");
    contexts.push_back(parent->Inner(true, false));
    GenContext& loop_ctx = contexts.back();
    loop_ctx.syntax.labels.push_back(label);
    loop_ctx.syntax.loop_labels.push_back(label);
    Statements resets;
    Statement* loop = BoundedLoop(
        *parent, cg, level == 0 ? &prelude : &resets, loop_ctx,
        1 + static_cast<int>(r.Index(static_cast<size_t>(max_iterations))));
    if (level > 0 && !resets.empty()) {
      // The counter of an inner while loop restarts with each iteration of
      // the outer loop.
      Statements& outer_body = LoopBlock(loops.back())->body;
      outer_body.insert(outer_body.end(), resets.begin(), resets.end());
    }
    labels.push_back(label);
    loops.push_back(loop);
    parent = &loop_ctx;
  }
  // Bodies from the innermost loop outwards: each one ends with or contains
  // the next inner loop.
  for (int level = depth - 1; level >= 0; level--) {
    GenContext& loop_ctx = contexts[static_cast<size_t>(level)];
    CodeGen lcg(loop_ctx);
    // The jumps of the innermost loop go to any label. They come before the
    // body, which can declare variables.
    Statements jumps;
    if (level == depth - 1) {
      const int n = 1 + r.SmallCount(kLabeledLoopsMaxJumps.AsInt() - 1);
      for (int i = 0; i < n; i++) {
        std::string_view label = r.Pick(labels);
        jumps.push_back(
            WrapJump(loop_ctx, lcg,
                     r.Coin() ? static_cast<Statement*>(mk::Break(t, label))
                              : mk::Continue(t, label)));
      }
    }
    Statements body = lcg.Body(loop_ctx, r.SmallCount(max_statements));
    for (Statement* jump : jumps) {
      InsertAtRandom(r, &body, {jump});
    }
    if (level + 1 < depth) {
      Statement* inner_loop =
          mk::Labeled(t, labels[static_cast<size_t>(level + 1)],
                      loops[static_cast<size_t>(level + 1)]);
      body.insert(
          body.begin() + static_cast<std::ptrdiff_t>(r.Index(body.size() + 1)),
          inner_loop);
    }
    Statements& block = LoopBlock(loops[static_cast<size_t>(level)])->body;
    // Keep the counter resets that the loop setup put first.
    block.insert(block.end(), body.begin(), body.end());
  }
  Statement* labeled = mk::Labeled(t, labels[0], loops[0]);
  if (r.Chance(kLabeledLoopsTryChance())) {
    prelude.push_back(labeled);
    Push(out, mk::Try(t, std::move(prelude), ctx.builder.FreshName("e"), {}));
    return true;
  }
  for (Statement* s : prelude) {
    Push(out, s);
  }
  Push(out, labeled);
  return true;
}

// --- Symbols and iteration -----------------------------------------------

REDLINE_TUNABLE(kSymbolHookMaxStatements,
                "generators.SymbolHook.max_statements", 2, 0, 16,
                "most generated statements in a hook function body");
REDLINE_TUNABLE(kSymbolHookVisibleReceiverChance,
                "generators.SymbolHook.visible_receiver_chance", 0.6, 0, 1,
                "probability that the receiver is a visible variable");
REDLINE_TUNABLE(kSymbolHookDefineChance, "generators.SymbolHook.define_chance",
                0.3, 0, 1,
                "probability that Object.defineProperty installs the hook");
REDLINE_TUNABLE(kSymbolHookUseChance, "generators.SymbolHook.use_chance", 0.8,
                0, 1, "probability of an operation that reads the hook");
REDLINE_TUNABLE(kSymbolHookMaxUnscopables,
                "generators.SymbolHook.max_unscopables", 3, 1, 16,
                "most names in a Symbol.unscopables object");
REDLINE_TUNABLE(kSymbolHookNullChance, "generators.SymbolHook.null_chance", 0.1,
                0, 1,
                "probability that Symbol.unscopables is null instead of an "
                "object");
REDLINE_TUNABLE(kSymbolHookOddSpeciesChance,
                "generators.SymbolHook.odd_species_chance", 0.2, 0, 1,
                "probability that the Symbol.species getter returns any value "
                "instead of a constructor");

// The well-known symbols of JavaScriptEnvironment.swift:55.
constexpr const char* kWellKnownSymbols[] = {
    "iterator",    "asyncIterator", "match",
    "matchAll",    "replace",       "search",
    "split",       "hasInstance",   "isConcatSpreadable",
    "unscopables", "species",       "toPrimitive",
    "toStringTag", "dispose",       "asyncDispose"};

// A function expression with `params` parameters and a generated body that
// ends with `return <result>`. A null result gives a generated value.
FunctionExpression* HookFunction(GenContext& ctx, int params,
                                 Expression* result, bool is_async = false,
                                 bool is_generator = false) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  GenContext nested = ctx.Nested(true, false, is_async, is_generator);
  std::vector<std::string_view> names;
  for (int i = 0; i < params; i++) {
    names.push_back(ctx.builder.FreshName("a"));
    nested.Declare(names.back(), JsType::kUnknown);
  }
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, r.SmallCount(kSymbolHookMaxStatements.AsInt()));
  if (is_generator) {
    body.push_back(inner.Stmt(mk::Yield(t, inner.Value(JsType::kUnknown, 1))));
    if (r.Coin()) {
      body.push_back(inner.Stmt(mk::Yield(t, AnyIterable(inner), true)));
    }
  }
  if (result == nullptr) {
    result = inner.Value(JsType::kUnknown, 1);
  }
  body.push_back(mk::Return(t, result));
  return mk::Function(t, "", names, std::move(body), is_async, is_generator);
}

// The value to install for the well-known symbol `name`. It is a function of
// the right kind for the protocol, or a plain value if the protocol reads
// one. The hook of species is a getter, so *getter becomes true for it.
Expression* HookValue(GenContext& ctx, CodeGen& cg, std::string_view name,
                      bool* getter) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  *getter = false;
  if (name == "iterator") {
    if (r.Coin()) {
      return HookFunction(ctx, 0, nullptr, false, true);
    }
    return HookFunction(ctx, 0,
                        mk::CallMethod(t, cg.ArrayLiteral(1), "values", {}));
  }
  if (name == "asyncIterator") {
    return HookFunction(ctx, 0, nullptr, true, true);
  }
  if (name == "match" && r.Coin()) {
    // IsRegExp() reads Symbol.match as a boolean.
    return cg.Boolean();
  }
  if (name == "isConcatSpreadable") {
    return r.Coin() ? cg.Boolean() : cg.Value(JsType::kUnknown, 1);
  }
  if (name == "unscopables") {
    std::vector<Node*> props;
    const int n = 1 + r.SmallCount(kSymbolHookMaxUnscopables.AsInt() - 1);
    for (int i = 0; i < n; i++) {
      props.push_back(mk::Prop(t, PlainName(cg), cg.Boolean()));
    }
    return r.Chance(kSymbolHookNullChance())
               ? static_cast<Expression*>(mk::Null(t))
               : mk::Object(t, std::move(props));
  }
  if (name == "toStringTag") {
    return cg.String();
  }
  if (name == "species") {
    *getter = true;
    Expression* ctor = cg.Variable(JsType::kClass);
    if (ctor == nullptr) {
      ctor = mk::Id(t, r.Pick({"Array", "Object", "Promise", "Map",
                               "Uint8Array", "RegExp"}));
    }
    return HookFunction(ctx, 0,
                        r.Chance(kSymbolHookOddSpeciesChance())
                            ? cg.Value(JsType::kUnknown, 1)
                            : ctor);
  }
  if (name == "split") {
    return HookFunction(ctx, 2, cg.ArrayLiteral(1));
  }
  if (name == "replace" || name == "match" || name == "search") {
    return HookFunction(ctx, 2, nullptr);
  }
  if (name == "matchAll") {
    return HookFunction(ctx, 1,
                        mk::CallMethod(t, cg.ArrayLiteral(1), "values", {}));
  }
  if (name == "hasInstance") {
    return HookFunction(ctx, 1, cg.Condition(1));
  }
  if (name == "toPrimitive") {
    return HookFunction(ctx, 1, cg.Value(JsType::kNumber, 1));
  }
  if (name == "asyncDispose") {
    return HookFunction(ctx, 0, nullptr, true, false);
  }
  // dispose
  return HookFunction(ctx, 0, nullptr);
}

// An operation that reads the hook of `name` on `receiver`, or nullptr when
// it needs a context that this position does not give.
Statement* HookUse(GenContext& ctx, CodeGen& cg, std::string_view name,
                   std::string_view receiver) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* o = mk::Id(t, receiver);
  Expression* use = nullptr;
  if (name == "iterator") {
    use = r.Coin() ? static_cast<Expression*>(mk::Array(t, {mk::Spread(t, o)}))
                   : mk::CallMethod(t, mk::Id(t, "Array"), "from", {o});
  } else if (name == "asyncIterator") {
    use = mk::CallMethod(t, mk::Id(t, "Array"), "fromAsync", {o});
  } else if (name == "match" || name == "matchAll" || name == "replace" ||
             name == "search" || name == "split") {
    std::vector<Expression*> args = {o};
    if (name == "replace") {
      args.push_back(cg.String());
    }
    const char* method = name == "match" && r.Coin()
                             ? r.Pick({"startsWith", "includes", "endsWith"})
                             : (name == "match"      ? "match"
                                : name == "matchAll" ? "matchAll"
                                : name == "replace"  ? "replace"
                                : name == "search"   ? "search"
                                                     : "split");
    use = mk::CallMethod(t, cg.String(), method, std::move(args));
  } else if (name == "hasInstance") {
    use =
        mk::Binary(t, BinaryOp::kInstanceof, cg.Value(JsType::kUnknown, 1), o);
  } else if (name == "isConcatSpreadable") {
    use = mk::CallMethod(t, cg.ArrayLiteral(1), "concat", {o});
  } else if (name == "unscopables") {
    if (ctx.syntax.strict) {
      return nullptr;
    }
    WithStatement* w = t->New<WithStatement>();
    w->object = o;
    w->body = mk::Block(t, {cg.Stmt(mk::Id(t, PlainName(cg)))});
    return w;
  } else if (name == "species") {
    use =
        mk::CallMethod(t, o, r.Pick({"map", "filter", "slice", "concat"}), {});
  } else if (name == "toPrimitive") {
    use = r.Coin() ? static_cast<Expression*>(
                         mk::Binary(t, BinaryOp::kAdd, o, cg.Number()))
                   : mk::Template(t, {"", ""}, {o});
  } else if (name == "toStringTag") {
    use = mk::Call(
        t,
        mk::Member(
            t,
            mk::Member(t, mk::Member(t, mk::Id(t, "Object"), "prototype"),
                       "toString"),
            "call"),
        {o});
  } else if (name == "dispose") {
    VariableDeclaration* d =
        mk::Declare(t, VariableKind::kUsing, ctx.builder.FreshName("u"), o);
    return mk::Try(t, {mk::Block(t, {d})}, "", {});
  } else {
    // asyncDispose
    if (!ctx.syntax.in_async || ctx.syntax.in_static_block) {
      return nullptr;
    }
    VariableDeclaration* d = mk::Declare(t, VariableKind::kAwaitUsing,
                                         ctx.builder.FreshName("u"), o);
    return mk::Try(t, {mk::Block(t, {d})}, "", {});
  }
  // The hook can throw, and the receiver may not fit the protocol.
  return mk::Try(t, {cg.Stmt(use)}, "", {});
}

// Fuzzilli's WellKnownPropertyLoad/StoreGenerator (CodeGenerators.swift:3136,
// :3145). It installs a hook for one of the 15 well-known symbols. The
// receiver is an object, an array, a regexp, a function or a class
// prototype. Then an operation reads the hook.
GEN(SymbolHook) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const std::string_view name = kWellKnownSymbols[r.Index(
      sizeof(kWellKnownSymbols) / sizeof(kWellKnownSymbols[0]))];
  // The receiver.
  std::string_view receiver;
  if (r.Chance(kSymbolHookVisibleReceiverChance())) {
    static const JsType kTypes[] = {JsType::kObject, JsType::kArray,
                                    JsType::kRegExp, JsType::kFunction,
                                    JsType::kClass};
    if (Identifier* id =
            cg.Variable(kTypes[r.Index(sizeof(kTypes) / sizeof(kTypes[0]))])) {
      receiver = id->name;
    }
  }
  if (receiver.empty()) {
    Expression* value = nullptr;
    JsType type = JsType::kObject;
    switch (r.Index(4)) {
      case 0:
        value = cg.ObjectLiteral(1);
        break;
      case 1:
        value = cg.ArrayLiteral(1);
        type = JsType::kArray;
        break;
      case 2:
        value = cg.RegExp();
        type = JsType::kRegExp;
        break;
      default:
        value = mk::Member(t, mk::Class(t, "", nullptr, {}), "prototype");
        break;
    }
    Push(out, cg.Declare(value, type, "o"));
    receiver = LastDeclared(ctx);
  }
  bool getter = false;
  Expression* hook = HookValue(ctx, cg, name, &getter);
  Expression* key = mk::Member(t, mk::Id(t, "Symbol"), name);
  // Function.prototype[Symbol.hasInstance] is not writable, and a getter
  // needs a descriptor: both use Object.defineProperty.
  if (getter || name == "hasInstance" || r.Chance(kSymbolHookDefineChance())) {
    std::vector<Node*> descriptor = {
        mk::Prop(t, getter ? "get" : "value", hook),
        mk::Prop(t, "configurable", mk::Bool(t, true))};
    Push(out,
         mk::Try(
             t,
             {cg.Stmt(mk::CallMethod(t, mk::Id(t, "Object"), "defineProperty",
                                     {mk::Id(t, receiver), key,
                                      mk::Object(t, std::move(descriptor))}))},
             "", {}));
  } else {
    Push(out, mk::Try(t,
                      {cg.Stmt(mk::Assign(
                          t, mk::Index(t, mk::Id(t, receiver), key), hook))},
                      "", {}));
  }
  if (r.Chance(kSymbolHookUseChance())) {
    if (Statement* use = HookUse(ctx, cg, name, receiver)) {
      Push(out, use);
    }
  }
  return true;
}

REDLINE_TUNABLE(kIteratorChainMaxSteps, "generators.IteratorChain.max_steps", 3,
                0, 8, "most lazy helper calls between the source and the end");
REDLINE_TUNABLE(kIteratorChainZipChance, "generators.IteratorChain.zip_chance",
                0.25, 0, 1,
                "probability that the source is Iterator.zip or "
                "Iterator.zipKeyed");
REDLINE_TUNABLE(kIteratorChainOptionsChance,
                "generators.IteratorChain.options_chance", 0.5, 0, 1,
                "probability that a zip call gets an options object");
REDLINE_TUNABLE(kIteratorChainZipLongestPaddingChance,
                "generators.IteratorChain.zip_longest_padding_chance", 0.5, 0,
                1,
                "probability that zip options use mode longest with padding");
REDLINE_TUNABLE(kIteratorChainMaxZipped, "generators.IteratorChain.max_zipped",
                3, 1, 8, "most iterables that a zip call combines");
REDLINE_TUNABLE(kIteratorChainMaxCount, "generators.IteratorChain.max_count", 5,
                0, 1000, "largest count passed to take and drop");
REDLINE_TUNABLE(kIteratorChainChunkWindowChance,
                "generators.IteratorChain.chunk_window_chance", 0.35, 0, 1,
                "probability that a lazy step calls chunks or windows");
REDLINE_TUNABLE(kIteratorChainManualStepChance,
                "generators.IteratorChain.manual_step_chance", 0.25, 0, 1,
                "probability that the chain ends with next and return calls");
REDLINE_TUNABLE(kIteratorChainForOfDestructChance,
                "generators.IteratorChain.for_of_destruct_chance", 0.25, 0, 1,
                "probability that a for-of destructuring loop consumes the "
                "chain");
REDLINE_TUNABLE(kIteratorChainMaxNextCalls,
                "generators.IteratorChain.max_next_calls", 3, 1, 16,
                "most next calls before return in manual stepping");

// A callback for an iterator helper: an arrow with a generated expression,
// or a function with a generated body.
Expression* HelperCallback(GenContext& ctx, int params, bool returns_iterable) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  GenContext nested = ctx.Nested(true, true, false, false);
  std::vector<std::string_view> names;
  for (int i = 0; i < params; i++) {
    names.push_back(ctx.builder.FreshName("x"));
    nested.Declare(names.back(), JsType::kUnknown);
  }
  CodeGen inner(nested);
  Expression* result =
      returns_iterable ? AnyIterable(inner) : inner.Value(JsType::kUnknown, 1);
  if (r.Coin()) {
    return mk::ArrowExpr(t, names, result);
  }
  Statements body =
      inner.Body(nested, r.SmallCount(kSymbolHookMaxStatements.AsInt()));
  body.push_back(mk::Return(t, result));
  return mk::Arrow(t, names, std::move(body));
}

// Builds an iterator chain from `Iterator.from`, `Iterator.zip`, or
// `Iterator.zipKeyed`. Lazy steps include `map`, `filter`, `flatMap`, `take`,
// `drop`, `chunks`, and `windows`. The chain ends with a terminal method,
// manual `next` and `return` calls, or `for..of` destructuring.
GEN(IteratorChain) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* chain = nullptr;
  std::vector<std::string_view> zipped_keys;
  bool yields_arrays = false;
  if (r.Chance(kIteratorChainZipChance())) {
    const int n =
        1 + static_cast<int>(
                r.Index(static_cast<size_t>(kIteratorChainMaxZipped.AsInt())));
    const bool keyed = r.Coin();
    yields_arrays = !keyed;
    std::vector<Expression*> iterables;
    std::vector<Node*> props;
    for (int i = 0; i < n; i++) {
      if (keyed) {
        std::string_view key = ctx.builder.FreshName("k");
        zipped_keys.push_back(key);
        props.push_back(mk::Prop(t, key, AnyIterable(cg)));
      } else {
        iterables.push_back(AnyIterable(cg));
      }
    }
    std::vector<Expression*> args = {
        keyed ? static_cast<Expression*>(mk::Object(t, std::move(props)))
              : mk::Array(t, std::move(iterables))};
    if (r.Chance(kIteratorChainOptionsChance())) {
      std::vector<Node*> options;
      const bool longest_pad =
          r.Chance(kIteratorChainZipLongestPaddingChance());
      options.push_back(mk::Prop(
          t, "mode",
          mk::Str(t, longest_pad ? "longest"
                                 : r.Pick({"shortest", "longest", "strict"}))));
      if (longest_pad || r.Coin()) {
        Expression* padding = nullptr;
        if (keyed) {
          std::vector<Node*> pad_props;
          for (std::string_view key : zipped_keys) {
            if (pad_props.empty() || r.Coin()) {
              pad_props.push_back(
                  mk::Prop(t, key, cg.Value(JsType::kUnknown, 1)));
            }
          }
          padding = mk::Object(t, std::move(pad_props));
        } else if (r.Coin()) {
          std::vector<Expression*> pad_elems;
          for (int i = 0; i < n; i++) {
            pad_elems.push_back(cg.Value(JsType::kUnknown, 1));
          }
          padding = mk::Array(t, std::move(pad_elems));
        } else {
          padding = cg.ArgumentFor(arg::Iterable());
        }
        options.push_back(mk::Prop(t, "padding", padding));
      }
      args.push_back(mk::Object(t, std::move(options)));
    }
    chain = mk::CallMethod(t, mk::Id(t, "Iterator"), keyed ? "zipKeyed" : "zip",
                           std::move(args));
  } else {
    chain = mk::CallMethod(t, mk::Id(t, "Iterator"), "from", {AnyIterable(cg)});
  }
  const int max_steps = kIteratorChainMaxSteps.AsInt();
  const int steps = max_steps > 0 ? (1 + r.SmallCount(max_steps - 1)) : 0;
  for (int i = 0; i < steps; i++) {
    if (r.Chance(kIteratorChainChunkWindowChance())) {
      Expression* size = cg.ArgumentFor(arg::Range(1, 16, "ChunkSize"));
      if (r.Coin()) {
        chain = mk::CallMethod(t, chain, "chunks", {size});
      } else {
        std::vector<Expression*> wargs = {size};
        if (r.Coin()) {
          wargs.push_back(mk::Str(t, r.Pick({"only-full", "allow-partial"})));
        }
        chain = mk::CallMethod(t, chain, "windows", std::move(wargs));
      }
      yields_arrays = true;
      zipped_keys.clear();
      continue;
    }
    switch (r.Index(5)) {
      case 0:
        chain =
            mk::CallMethod(t, chain, "map", {HelperCallback(ctx, 2, false)});
        yields_arrays = false;
        zipped_keys.clear();
        break;
      case 1:
        chain =
            mk::CallMethod(t, chain, "filter", {HelperCallback(ctx, 2, false)});
        break;
      case 2:
        chain = mk::CallMethod(t, chain, "flatMap",
                               {HelperCallback(ctx, 1, /*iterable=*/true)});
        yields_arrays = false;
        zipped_keys.clear();
        break;
      default:
        chain = mk::CallMethod(
            t, chain, r.Coin() ? "take" : "drop",
            {mk::Num(t, static_cast<double>(
                            r.Range(0, kIteratorChainMaxCount.AsInt())))});
        break;
    }
  }
  if (r.Chance(kIteratorChainManualStepChance())) {
    std::string_view it_name = ctx.builder.FreshName("it");
    Push(out, mk::Let(t, it_name, nullptr));
    Statements try_body;
    try_body.push_back(
        mk::ExprStmt(t, mk::Assign(t, mk::Id(t, it_name), chain)));
    const int nexts = 1 + r.SmallCount(kIteratorChainMaxNextCalls.AsInt() - 1);
    for (int i = 0; i < nexts; i++) {
      try_body.push_back(
          mk::ExprStmt(t, mk::CallMethod(t, mk::Id(t, it_name), "next", {})));
    }
    try_body.push_back(
        mk::ExprStmt(t, mk::CallMethod(t, mk::Id(t, it_name), "return", {})));
    Push(out, mk::Try(t, std::move(try_body), "e", {}));
    ctx.Declare(it_name, JsType::kIterator);
    return true;
  }
  if (r.Chance(kIteratorChainForOfDestructChance())) {
    Expression* pattern = nullptr;
    std::string_view first_var;
    if (yields_arrays || (zipped_keys.empty() && r.Coin())) {
      auto* arr_pat = t->New<ArrayPattern>();
      first_var = ctx.builder.FreshName("d");
      arr_pat->elements.push_back(mk::Id(t, first_var));
      if (r.Coin()) {
        arr_pat->elements.push_back(mk::Id(t, ctx.builder.FreshName("d")));
      }
      if (r.Coin()) {
        arr_pat->elements.push_back(
            mk::Rest(t, mk::Id(t, ctx.builder.FreshName("rest"))));
      }
      pattern = arr_pat;
    } else {
      auto* obj_pat = t->New<ObjectPattern>();
      if (!zipped_keys.empty()) {
        for (std::string_view key : zipped_keys) {
          std::string_view var = ctx.builder.FreshName("d");
          if (first_var.empty()) {
            first_var = var;
          }
          obj_pat->properties.push_back(mk::Prop(t, key, mk::Id(t, var)));
        }
      } else {
        first_var = ctx.builder.FreshName("d");
        std::string_view key = cg.PropertyName();
        if (!IsIdentifierName(key)) {
          key = "a";
        }
        obj_pat->properties.push_back(mk::Prop(t, key, mk::Id(t, first_var)));
      }
      pattern = obj_pat;
    }
    VariableDeclaration* decl =
        mk::Declare(t, VariableKind::kConst, "x", nullptr);
    decl->declarations[0]->target = pattern;
    auto* loop = t->New<ForOfStatement>();
    loop->left = decl;
    loop->right = chain;
    Statements loop_body = {mk::ExprStmt(t, mk::Id(t, first_var))};
    if (r.Coin()) {
      loop_body.push_back(mk::Break(t));
    }
    loop->body = mk::Block(t, std::move(loop_body));
    Push(out, mk::Try(t, {loop}, "e", {}));
    return true;
  }
  switch (r.Index(6)) {
    case 0:
      chain = mk::CallMethod(t, chain, "reduce",
                             {HelperCallback(ctx, 2, false), cg.Value()});
      break;
    case 1:
      chain = mk::CallMethod(t, chain, r.Pick({"some", "every", "find"}),
                             {HelperCallback(ctx, 1, false)});
      break;
    case 2:
      chain =
          mk::CallMethod(t, chain, "forEach", {HelperCallback(ctx, 1, false)});
      break;
    case 3:
      chain = mk::Array(t, {mk::Spread(t, chain)});
      break;
    default:
      chain = mk::CallMethod(t, chain, "toArray", {});
      break;
  }
  // `Iterator.zip` with mode `strict` throws on length mismatch, and callbacks
  // can throw.
  cg.DeclareMaybeGuarded(out, chain, JsType::kArray, "r", 1.0);
  return true;
}

#undef GEN

}  // namespace

REDLINE_TUNABLE(kAnyIterableVariableChance,
                "codegen.any_iterable.variable_chance", 0.5, 0, 1,
                "probability that an iterable is a visible variable of an "
                "iterable type");
REDLINE_TUNABLE(kAnyIterableMaxPairs, "codegen.any_iterable.max_pairs", 3, 0,
                16, "most entries of a new Map");

fast::Expression* AnyIterable(CodeGen& cg) {
  Random& r = cg.rng();
  Tree* t = cg.tree();
  if (r.Chance(kAnyIterableVariableChance())) {
    static const JsType kTypes[] = {JsType::kArray, JsType::kString,
                                    JsType::kMap, JsType::kSet,
                                    JsType::kTypedArray};
    if (Identifier* id =
            cg.Variable(kTypes[r.Index(sizeof(kTypes) / sizeof(kTypes[0]))])) {
      return id;
    }
  }
  switch (r.Index(7)) {
    case 0:
      return cg.String();
    case 1:
      return mk::New(t, mk::Id(t, "Set"), {cg.ArrayLiteral(1)});
    case 2: {
      std::vector<Expression*> pairs;
      const int n = r.SmallCount(kAnyIterableMaxPairs.AsInt());
      for (int i = 0; i < n; i++) {
        pairs.push_back(mk::Array(
            t, {cg.Value(JsType::kUnknown, 1), cg.Value(JsType::kUnknown, 1)}));
      }
      return mk::New(t, mk::Id(t, "Map"), {mk::Array(t, std::move(pairs))});
    }
    case 3: {
      Expression* base = cg.Variable(JsType::kArray);
      if (base == nullptr) {
        base = cg.ArrayLiteral(1);
      }
      return mk::CallMethod(t, base, r.Pick({"keys", "values", "entries"}), {});
    }
    case 4: {
      // A generator object of a new generator function.
      FunctionExpression* fn =
          mk::Function(t, "", {},
                       {cg.Stmt(mk::Yield(t, cg.Value(JsType::kUnknown, 1))),
                        cg.Stmt(mk::Yield(t, cg.ArrayLiteral(1), true))},
                       false, true);
      return mk::Call(t, fn, {});
    }
    case 5:
      return mk::New(
          t, mk::Id(t, r.Pick({"Uint8Array", "Float64Array", "Int16Array"})),
          {cg.ArrayLiteral(1)});
    default:
      return cg.ArrayLiteral(1);
  }
}

std::vector<ShapesGenerator> ShapesGenerators() {
  return {
      {"Imitation",
       "a stand-in for a value: valueOf/toPrimitive hook, proxy, subclass "
       "instance or __proto__ literal",
       1.5, kRecursive, GenImitation},
      {"LiteralFactory",
       "function mk(a) { ...; return {x: a}; } called 5-10 times in a loop", 1,
       kRecursive, GenLiteralFactory},
      {"ObjectFlow",
       "function f(a) { let o = mk(a); o.x = v; o = other; ... return o.y; } "
       "with inlined makers and accessors, warmed up and optimized",
       1, kRecursive, GenObjectFlow},
      {"NumberChain",
       "3-7 dependent let statements with arithmetic, bitwise, shift and "
       "Math operations",
       2, kNoRequirement, GenNumberChain},
      {"TryFinally",
       "try { try {...} finally {...} } catch {}: a catch-less try/finally "
       "in a guard",
       1, kRecursive, GenTryFinally},
      {"ComplexForLoop",
       "for (let i = 0, j = n; ; i++, j--) with several variables or an "
       "empty part",
       1, kRecursive, GenComplexForLoop},
      {"LabeledLoops",
       "L1: for (...) { L2: while (...) { if (c) continue L1; } } and "
       "break L from nested blocks",
       1, kRecursive, GenLabeledLoops},
      {"SymbolHook",
       "o[Symbol.split] = function(s, l) {...} for the 15 well-known symbols, "
       "then a use",
       1, kRecursive, GenSymbolHook},
      {"IteratorChain",
       "Iterator.from(x).map(f).take(n).toArray(), Iterator.zip([a, b], "
       "{mode})",
       1, kRecursive, GenIteratorChain},
  };
}

}  // namespace redline

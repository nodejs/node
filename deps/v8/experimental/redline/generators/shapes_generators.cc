// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/shapes_generators.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"

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
    Push(out, cg.Declare(value, Builder::GuessType(value)));
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
REDLINE_TUNABLE(kIteratorChainMaxZipped, "generators.IteratorChain.max_zipped",
                3, 1, 8, "most iterables that a zip call combines");
REDLINE_TUNABLE(kIteratorChainMaxCount, "generators.IteratorChain.max_count", 5,
                0, 1000, "largest count passed to take and drop");

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

// Fuzzilli's IteratorZipGenerator and IteratorZipKeyedGenerator
// (CodeGenerators.swift:2199, :2232) and the typed Iterator helpers of
// JavaScriptEnvironment.swift. The source is Iterator.from of any iterable,
// or Iterator.zip or Iterator.zipKeyed with options. Lazy helpers follow
// (map, filter, take, drop and flatMap). A consumer ends the chain (toArray,
// reduce, some, every, find, forEach or a spread).
GEN(IteratorChain) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* chain = nullptr;
  if (r.Chance(kIteratorChainZipChance())) {
    const int n =
        1 + static_cast<int>(
                r.Index(static_cast<size_t>(kIteratorChainMaxZipped.AsInt())));
    const bool keyed = r.Coin();
    std::vector<Expression*> iterables;
    std::vector<Node*> props;
    for (int i = 0; i < n; i++) {
      if (keyed) {
        props.push_back(
            mk::Prop(t, ctx.builder.FreshName("k"), AnyIterable(cg)));
      } else {
        iterables.push_back(AnyIterable(cg));
      }
    }
    std::vector<Expression*> args = {
        keyed ? static_cast<Expression*>(mk::Object(t, std::move(props)))
              : mk::Array(t, std::move(iterables))};
    if (r.Chance(kIteratorChainOptionsChance())) {
      std::vector<Node*> options = {mk::Prop(
          t, "mode", mk::Str(t, r.Pick({"shortest", "longest", "strict"})))};
      if (r.Coin()) {
        options.push_back(mk::Prop(
            t, "padding", keyed ? cg.ObjectLiteral(1) : cg.ArrayLiteral(1)));
      }
      args.push_back(mk::Object(t, std::move(options)));
    }
    chain = mk::CallMethod(t, mk::Id(t, "Iterator"), keyed ? "zipKeyed" : "zip",
                           std::move(args));
  } else {
    chain = mk::CallMethod(t, mk::Id(t, "Iterator"), "from", {AnyIterable(cg)});
  }
  const int steps = r.SmallCount(kIteratorChainMaxSteps.AsInt());
  for (int i = 0; i < steps; i++) {
    switch (r.Index(5)) {
      case 0:
        chain =
            mk::CallMethod(t, chain, "map", {HelperCallback(ctx, 2, false)});
        break;
      case 1:
        chain =
            mk::CallMethod(t, chain, "filter", {HelperCallback(ctx, 2, false)});
        break;
      case 2:
        chain = mk::CallMethod(t, chain, "flatMap",
                               {HelperCallback(ctx, 1, /*iterable=*/true)});
        break;
      default:
        chain = mk::CallMethod(
            t, chain, r.Coin() ? "take" : "drop",
            {mk::Num(t, static_cast<double>(
                            r.Range(0, kIteratorChainMaxCount.AsInt())))});
        break;
    }
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
  // Iterator.zip with mode "strict" throws for iterables of other lengths,
  // and the callbacks can throw.
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

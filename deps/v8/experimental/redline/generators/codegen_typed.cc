// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Typed builtin arguments: the CodeGen side of the typed model of
// core/environment_typed.cc. Modeled on Fuzzilli's findOrGenerateArguments
// and generateTypeInternal (Sources/Fuzzilli/Base/ProgramBuilder.swift:
// 709-1084): reuse a visible variable of the parameter's kind, or make a new
// value of the kind.

#include <algorithm>
#include <string>
#include <utility>

#include "experimental/redline/core/tunable.h"
#include "experimental/redline/generators/codegen.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

REDLINE_TUNABLE(kArgumentReuseChance, "codegen.argument.reuse_chance", 0.9, 0,
                1,
                "probability that a typed object argument is a visible "
                "variable of its type, if there is one (Fuzzilli: 0.9)");
REDLINE_TUNABLE(kArgumentReusePrimitiveChance,
                "codegen.argument.reuse_primitive_chance", 0.4, 0, 1,
                "probability that an integer, index or length argument is a "
                "visible number variable, if there is one");
REDLINE_TUNABLE(kArgumentOptionalStopChance,
                "codegen.argument.optional_stop_chance", 0.5, 0, 1,
                "probability that a typed call stops at an optional parameter "
                "(Fuzzilli: 0.5)");
REDLINE_TUNABLE(kArgumentMaxRestValues, "codegen.argument.max_rest_values", 2,
                0, 16, "most values that a rest parameter takes");
REDLINE_TUNABLE(kArgumentIndexMax, "codegen.argument.index_max", 8, 0, 1024,
                "largest value of a new index argument");
REDLINE_TUNABLE(kArgumentLengthMax, "codegen.argument.length_max", 16, 0, 4096,
                "largest value of a new length or count argument");
REDLINE_TUNABLE(kArgumentBoundaryChance, "codegen.argument.boundary_chance",
                0.05, 0, 1,
                "probability that a new index or length argument is an "
                "interesting integer instead of a small one");
REDLINE_TUNABLE(kArgumentRangeEdgeChance, "codegen.argument.range_edge_chance",
                0.01, 0, 1,
                "probability that a new range argument is one past the end "
                "of its range");
REDLINE_TUNABLE(kArgumentCallbackVariableChance,
                "codegen.argument.callback_variable_chance", 0.2, 0, 1,
                "probability that a callback argument is a visible function "
                "variable instead of a new arrow");
REDLINE_TUNABLE(kArgumentEnumInvalidChance,
                "codegen.argument.enum_invalid_chance", 0.02, 0, 1,
                "probability that an enum argument is a random string that "
                "the builtin rejects");
REDLINE_TUNABLE(kArgumentOptionKeyChance, "codegen.argument.option_key_chance",
                0.5, 0, 1, "probability that an options bag has each key");
REDLINE_TUNABLE(kArgumentIterableMaxElements,
                "codegen.argument.iterable_max_elements", 4, 0, 64,
                "most elements of a new iterable argument");
REDLINE_TUNABLE(kArgumentTypedArrayMinLength,
                "codegen.argument.typed_array_min_length", 4, 0, 1024,
                "smallest length of a new typed array argument");
REDLINE_TUNABLE(kArgumentTypedArrayMaxLength,
                "codegen.argument.typed_array_max_length", 16, 1, 4096,
                "largest length of a new typed array argument");
REDLINE_TUNABLE(kArgumentMaxDepth, "codegen.argument.max_depth", 4, 1, 16,
                "nesting depth at which typed arguments stop making new "
                "instances");
REDLINE_TUNABLE(kArgumentFactoryChance, "codegen.argument.factory_chance", 0.3,
                0, 1,
                "probability that a new instance comes from a static factory "
                "of the model (Temporal.PlainDate.from) instead of `new`");
REDLINE_TUNABLE(kTypedReceiverRecentChance,
                "codegen.typed_receiver.recent_chance", 0.5, 0, 1,
                "probability that a typed receiver is one of the 4 most "
                "recent typed variables");

namespace {

// True for the types whose values are objects that the model knows well
// enough to pick them as typed arguments with the high reuse probability.
bool IsObjectKind(JsType type) {
  switch (type) {
    case JsType::kUnknown:
    case JsType::kNumber:
    case JsType::kString:
    case JsType::kBoolean:
    case JsType::kBigInt:
    case JsType::kSymbol:
    case JsType::kNullish:
      return false;
    default:
      return true;
  }
}

}  // namespace

fast::Expression* CodeGen::GlobalPath(std::string_view path) {
  size_t dot = path.find('.');
  fast::Expression* e = mk::Id(tree(), path.substr(0, dot));
  while (dot != std::string_view::npos) {
    size_t next = path.find('.', dot + 1);
    std::string_view part = path.substr(dot + 1, next == std::string_view::npos
                                                     ? std::string_view::npos
                                                     : next - dot - 1);
    e = mk::Member(tree(), e, part);
    dot = next;
  }
  return e;
}

fast::Identifier* CodeGen::TypedReceiver() {
  std::vector<const redline::Variable*> candidates;
  for (const redline::Variable& v : ctx_.visible) {
    switch (v.type) {
      case JsType::kUnknown:
      case JsType::kObject:
      case JsType::kFunction:
      case JsType::kClass:
      case JsType::kNullish:
      case JsType::kBoolean:
        continue;
      default:
        break;
    }
    const BuiltinType* bt = env().ForType(v.type);
    if (bt != nullptr && !bt->methods.empty()) {
      candidates.push_back(&v);
    }
  }
  if (candidates.empty()) {
    return nullptr;
  }
  Random& r = rng();
  size_t index = r.Chance(kTypedReceiverRecentChance())
                     ? r.Index(std::min<size_t>(candidates.size(), 4))
                     : r.Index(candidates.size());
  return mk::Id(tree(), candidates[index]->name);
}

std::vector<fast::Expression*> CodeGen::ArgumentsFor(
    std::span<const ArgSpec> params, int depth) {
  Random& r = rng();
  std::vector<fast::Expression*> args;
  for (const ArgSpec& spec : params) {
    if (spec.optional && r.Chance(kArgumentOptionalStopChance())) {
      break;
    }
    if (spec.rest) {
      int n = static_cast<int>(r.Range(0, kArgumentMaxRestValues.AsInt()));
      for (int i = 0; i < n; i++) {
        args.push_back(ArgumentFor(spec, depth));
      }
      break;
    }
    args.push_back(ArgumentFor(spec, depth));
  }
  return args;
}

fast::Expression* CodeGen::ArgumentFor(const ArgSpec& spec, int depth) {
  ExprDepthGuard guard(ctx_);
  Random& r = rng();
  Tree* t = tree();
  if (TooDeep() || depth > kArgumentMaxDepth.AsInt()) {
    if (TooDeep() && spec.kind == ArgKind::kIterable &&
        spec.type == JsType::kPromise) {
      return mk::Array(t, {mk::CallMethod(t, mk::Id(t, "Promise"), "resolve",
                                          {mk::Num(t, 0)})});
    }
    // Deep enough: a simple value of the coarse type ends the recursion.
    JsType type =
        spec.kind == ArgKind::kInstance || spec.kind == ArgKind::kValue
            ? spec.type
            : JsType::kUnknown;
    if (fast::Identifier* id = Variable(type)) {
      return id;
    }
    return mk::Num(t, 0);
  }
  auto small = [&](int max) -> fast::Expression* {
    if (r.Chance(kArgumentBoundaryChance())) {
      return Integer();
    }
    return mk::Num(t, static_cast<double>(r.Range(0, max)));
  };
  switch (spec.kind) {
    case ArgKind::kAny:
      return Value(JsType::kUnknown, depth);
    case ArgKind::kValue:
      if (IsObjectKind(spec.type) && r.Chance(kArgumentReuseChance())) {
        if (fast::Identifier* id = Variable(spec.type)) {
          return id;
        }
      }
      return Value(spec.type, depth);
    case ArgKind::kInteger:
    case ArgKind::kIndex:
    case ArgKind::kLength:
      if (r.Chance(kArgumentReusePrimitiveChance())) {
        if (fast::Identifier* id = Variable(JsType::kNumber)) {
          return id;
        }
      }
      if (spec.kind == ArgKind::kInteger) {
        return Integer();
      }
      return small(spec.kind == ArgKind::kIndex ? kArgumentIndexMax.AsInt()
                                                : kArgumentLengthMax.AsInt());
    case ArgKind::kRange: {
      int64_t value = r.Range(spec.min, spec.max);
      if (r.Chance(kArgumentRangeEdgeChance())) {
        value = r.Coin() ? static_cast<int64_t>(spec.min) - 1
                         : static_cast<int64_t>(spec.max) + 1;
      }
      return mk::Num(t, static_cast<double>(value));
    }
    case ArgKind::kIterable: {
      if (r.Chance(kArgumentReuseChance())) {
        if (fast::Identifier* id = Variable(JsType::kArray)) {
          return id;
        }
      }
      std::vector<fast::Expression*> elements;
      int n =
          static_cast<int>(r.Range(0, kArgumentIterableMaxElements.AsInt()));
      if (spec.type == JsType::kPromise && n == 0) {
        n = 1;  // Promise.any([]) rejects, and nothing handles it.
      }
      for (int i = 0; i < n; i++) {
        fast::Expression* e =
            Value(spec.type == JsType::kPromise ? JsType::kUnknown : spec.type,
                  depth + 1);
        if (spec.type == JsType::kPromise && r.Coin()) {
          // Promises and plain values for the combinators.
          e = mk::CallMethod(t, mk::Id(t, "Promise"), "resolve", {e});
        }
        elements.push_back(e);
      }
      fast::Expression* array = mk::Array(t, std::move(elements));
      switch (r.Index(6)) {
        case 0:
          return mk::New(t, mk::Id(t, "Set"), {array});
        case 1:
          return mk::CallMethod(t, array, "values", {});
        default:
          return array;
      }
    }
    case ArgKind::kCallback: {
      if (r.Chance(kArgumentCallbackVariableChance())) {
        if (fast::Identifier* id = Variable(JsType::kFunction)) {
          return id;
        }
      }
      GenContext nested = ctx_.Nested(true, true, false, false);
      std::vector<fast::Expression*> params = Parameters(nested, spec.min, "e");
      CodeGen inner(nested);
      ArrowFunctionExpression* callback =
          mk::ArrowExpr(t, {}, inner.Expression(2));
      callback->params = std::move(params);
      return callback;
    }
    case ArgKind::kOptions: {
      const OptionsBag* bag = env().Options(spec.name);
      if (bag == nullptr) {
        return ObjectLiteral(depth);
      }
      std::vector<bool> chosen(bag->keys.size(), false);
      int count = 0;
      for (size_t i = 0; i < chosen.size(); i++) {
        if (bag->keys[i].required || r.Chance(kArgumentOptionKeyChance())) {
          chosen[i] = true;
          count++;
        }
      }
      while (count < bag->min_keys && count < static_cast<int>(chosen.size())) {
        size_t i = r.Index(chosen.size());
        if (!chosen[i]) {
          chosen[i] = true;
          count++;
        }
      }
      std::vector<Node*> properties;
      for (size_t i = 0; i < chosen.size(); i++) {
        if (chosen[i]) {
          properties.push_back(mk::Prop(
              t, bag->keys[i].key, ArgumentFor(bag->keys[i].value, depth + 1)));
        }
      }
      return mk::Object(t, std::move(properties));
    }
    case ArgKind::kEnum: {
      const EnumList* list = env().Enum(spec.name);
      if (list == nullptr || list->values.empty() ||
          r.Chance(kArgumentEnumInvalidChance())) {
        return String();
      }
      return mk::Str(t, r.Pick(list->values));
    }
    case ArgKind::kInstance:
      if (r.Chance(kArgumentReuseChance())) {
        if (fast::Identifier* id = Variable(spec.type)) {
          return id;
        }
      }
      if (fast::Expression* e = Instance(spec.type, depth + 1)) {
        return e;
      }
      return Value(spec.type, depth);
  }
  return Value(JsType::kUnknown, depth);
}

fast::Expression* CodeGen::Instance(JsType type, int depth) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return nullptr;
  }
  Random& r = rng();
  Tree* t = tree();
  switch (type) {
    case JsType::kIterator: {
      // Iterator is abstract: take the iterator of an iterable.
      fast::Expression* source = ArgumentFor(arg::Iterable(), depth);
      return r.Coin()
                 ? mk::CallMethod(t, mk::Id(t, "Iterator"), "from", {source})
                 : mk::CallMethod(t, ArrayLiteral(depth), "values", {});
    }
    case JsType::kTypedArray: {
      // An integer element type, so that Atomics accepts it.
      std::string_view kind =
          r.Pick({"Int8Array", "Uint8Array", "Int16Array", "Uint16Array",
                  "Int32Array", "Uint32Array"});
      return mk::New(t, mk::Id(t, kind),
                     {mk::Num(t, static_cast<double>(r.Range(
                                     kArgumentTypedArrayMinLength.AsInt(),
                                     kArgumentTypedArrayMaxLength.AsInt())))});
    }
    case JsType::kArray:
      return ArrayLiteral(depth);
    case JsType::kUnknown:
    case JsType::kNumber:
    case JsType::kString:
    case JsType::kBoolean:
    case JsType::kBigInt:
    case JsType::kSymbol:
    case JsType::kNullish:
    case JsType::kRegExp:
    case JsType::kObject:
    case JsType::kFunction:
      return Value(type, depth);
    default:
      break;
  }
  // A static factory that returns the type (Temporal.PlainDate.from(..),
  // Temporal.Now.instant()), or a typed constructor.
  if (r.Chance(kArgumentFactoryChance())) {
    std::vector<std::pair<const BuiltinGlobal*, const BuiltinFunction*>>
        factories;
    for (const BuiltinGlobal& g : env().globals()) {
      for (const BuiltinFunction& f : g.static_methods) {
        if (f.result == type && !f.params.empty()) {
          factories.push_back({&g, &f});
        }
      }
    }
    if (!factories.empty()) {
      const auto& [g, f] = r.Pick(factories);
      return mk::Call(t, mk::Member(t, GlobalPath(g->name), f->name),
                      ArgumentsFor(f->params, depth));
    }
  }
  std::vector<const BuiltinGlobal*> constructors = env().ConstructorsOf(type);
  if (constructors.empty()) {
    return nullptr;
  }
  const BuiltinGlobal& g = *r.Pick(constructors);
  if (g.params.empty()) {
    BuiltinFunction f{g.name, g.min_args, g.max_args, g.instance_type};
    return mk::New(t, GlobalPath(g.name), Arguments(f));
  }
  return mk::New(t, GlobalPath(g.name), ArgumentsFor(g.params, depth));
}

}  // namespace redline

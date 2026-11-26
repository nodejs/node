// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzilli's program templates as generators with a low weight. Each
// generator makes the structure of one template. The values and the small
// bodies inside the structure come from the other generators.

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/generators/shapes_generators.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

using Statements = std::vector<Statement*>;

// Generators share one signature. Not every generator needs every argument.
#define GEN(name)                                  \
  bool Gen##name([[maybe_unused]] GenContext& ctx, \
                 [[maybe_unused]] CodeGen& cg, Statements* out)

Tree* T(GenContext& ctx) { return ctx.tree(); }

void Push(Statements* out, Statement* s) { out->push_back(s); }

void PushAll(Statements* out, const Statements& statements) {
  out->insert(out->end(), statements.begin(), statements.end());
}

// The name that the last cg.Declare() or cg.DeclareGuarded() registered.
std::string_view LastDeclared(const GenContext& ctx) {
  return ctx.visible.front().name;
}

// `try { body } catch (e) {}` with a fresh name for the exception.
Statement* Guarded(GenContext& ctx, Statements body) {
  return mk::Try(T(ctx), std::move(body), ctx.builder.FreshName("e"), {});
}

// At least one statement, and at most `max` statements.
int BodySize(Random& r, int max) {
  return 1 + r.SmallCount(std::max(0, max - 1));
}

// `Symbol.<name>`.
Expression* WellKnownSymbol(Tree* t, std::string_view name) {
  return mk::Member(t, mk::Id(t, "Symbol"), name);
}

// A name from the first `size` names of a small shared pool, like Fuzzilli's
// custom property names (JavaScriptEnvironment.swift:307). Objects that use
// names of one small pool often share maps and transitions.
std::string_view PoolName(Random& r, int size) {
  static constexpr std::string_view kPool[] = {"a", "b", "c", "d",
                                               "e", "f", "g", "h"};
  constexpr int kPoolSize = sizeof(kPool) / sizeof(kPool[0]);
  return kPool[r.Index(static_cast<size_t>(std::clamp(size, 1, kPoolSize)))];
}

// --- RegExpPaths ------------------------------------------------------------

REDLINE_TUNABLE(kRegExpPathsEdgeLastIndexChance,
                "generators.RegExpPaths.edge_last_index_chance", 0.9, 0, 1,
                "probability that lastIndex is an integer from the edge-case "
                "list rather than undefined, NaN or a string");
REDLINE_TUNABLE(kRegExpPathsTwoByteSubjectChance,
                "generators.RegExpPaths.two_byte_subject_chance", 0.1, 0, 1,
                "probability that the subject is the fixed two-byte string");
REDLINE_TUNABLE(kRegExpPathsSpreadMatchAllChance,
                "generators.RegExpPaths.spread_match_all_chance", 0.5, 0, 1,
                "probability that the iterator of Symbol.matchAll is spread "
                "into an array");
REDLINE_TUNABLE(kRegExpPathsSplitNumericLimitChance,
                "generators.RegExpPaths.split_numeric_limit_chance", 0.9, 0, 1,
                "probability that the limit of Symbol.split is undefined or an "
                "integer rather than a string");
REDLINE_TUNABLE(kRegExpPathsMaxStatements,
                "generators.RegExpPaths.max_statements", 3, 1, 16,
                "most generated statements after the regexp call, and in a "
                "replacer function");

// The replacement argument of Symbol.replace (V8CommonProfile.swift:713-738):
// a string, a pattern with `$` substitutions or a function.
Expression* RegExpReplacement(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  switch (r.Index(3)) {
    case 0:
      return cg.String();
    case 1:
      return mk::Str(t, r.Pick({"X", "$1$2$3", "$$$&$`$' $1"}));
    default:
      break;
  }
  switch (r.Index(3)) {
    case 0:
      return mk::ArrowExpr(t, {}, mk::Str(t, "X"));
    case 1:
      return mk::ArrowExpr(t, {}, mk::Num(t, 42));
    default:
      break;
  }
  // A replacer with five parameters and a generated body.
  GenContext nested = ctx.Nested(true, true, false, false);
  std::vector<std::string_view> params;
  for (int i = 0; i < 5; i++) {
    params.push_back(ctx.builder.FreshName("a"));
    nested.Declare(params.back(), JsType::kUnknown);
  }
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, BodySize(r, kRegExpPathsMaxStatements.AsInt()));
  body.push_back(mk::Return(t, inner.Value(JsType::kUnknown, 1)));
  return mk::Arrow(t, std::move(params), std::move(body));
}

// One call of a regexp method on `re` (V8CommonProfile.swift:687-773).
Expression* RegExpMethodCall(GenContext& ctx, CodeGen& cg, std::string_view re,
                             std::string_view subject) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  auto symbol_call = [&](std::string_view symbol,
                         std::vector<Expression*> args) -> Expression* {
    args.insert(args.begin(), mk::Id(t, subject));
    return mk::Call(t, mk::Index(t, mk::Id(t, re), WellKnownSymbol(t, symbol)),
                    std::move(args));
  };
  switch (r.Index(7)) {
    case 0:
      return mk::CallMethod(t, mk::Id(t, re), "exec", {mk::Id(t, subject)});
    case 1:
      return symbol_call("match", {});
    case 2: {
      Expression* it = symbol_call("matchAll", {});
      if (r.Chance(kRegExpPathsSpreadMatchAllChance())) {
        return mk::Array(t, {mk::Spread(t, it)});
      }
      return it;
    }
    case 3:
      return symbol_call("replace", {RegExpReplacement(ctx, cg)});
    case 4:
      return symbol_call("search", {});
    case 5: {
      Expression* limit = nullptr;
      if (!r.Chance(kRegExpPathsSplitNumericLimitChance())) {
        limit = mk::Str(t, "not a number");
      } else if (r.Coin()) {
        limit = mk::Undefined(t);
      } else {
        limit = cg.Integer();
      }
      return symbol_call("split", {limit});
    }
    default:
      return mk::CallMethod(t, mk::Id(t, re), "test", {mk::Id(t, subject)});
  }
}

// Fuzzilli's V8RegExpFuzzer (V8CommonProfile.swift:644-792). A function makes
// a regexp, sets an edge-case lastIndex and calls one regexp method in a try
// block. The program calls the function twice on the fast path. Then it calls
// the function once more under %SetForceSlowPath(true).
GEN(RegExpPaths) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view fn_name = ctx.builder.FreshName("f");
  GenContext f_ctx = ctx.Nested(true, false, false, false);
  CodeGen f_cg(f_ctx);
  Statements body;

  std::string_view re = ctx.builder.FreshName("re");
  body.push_back(mk::Const(t, re, f_cg.RegExp()));
  f_ctx.Declare(re, JsType::kRegExp, true);

  // V8CommonProfile.swift:654-675.
  Expression* last_index = nullptr;
  if (r.Chance(kRegExpPathsEdgeLastIndexChance())) {
    static const double kLastIndex[] = {
        -1,           0,           1, 2, 3, 4, 5, 6, 7, 8, 9, 50, 2147483647.0,
        2147483648.0, 4294967296.0};
    last_index = mk::Num(
        t, kLastIndex[r.Index(sizeof(kLastIndex) / sizeof(kLastIndex[0]))]);
  } else {
    switch (r.Index(3)) {
      case 0:
        last_index = mk::Undefined(t);
        break;
      case 1:
        last_index = mk::Id(t, "NaN");
        break;
      default:
        last_index = mk::Str(t, "Not a Number");
        break;
    }
  }
  body.push_back(f_cg.Stmt(
      mk::Assign(t, mk::Member(t, mk::Id(t, re), "lastIndex"), last_index)));

  // V8CommonProfile.swift:646 and :677-683.
  std::string_view subject = ctx.builder.FreshName("s");
  Expression* subject_value = r.Chance(kRegExpPathsTwoByteSubjectChance())
                                  ? mk::Str(t, "f\U0001F4A9ba\u2603")
                                  : f_cg.Value(JsType::kString, 1);
  body.push_back(mk::Const(t, subject, subject_value));
  f_ctx.Declare(subject, JsType::kString, true);

  std::string_view result = ctx.builder.FreshName("r");
  body.push_back(mk::Let(t, result, mk::Null(t)));
  body.push_back(Guarded(
      ctx,
      {f_cg.Stmt(mk::Assign(t, mk::Id(t, result),
                            RegExpMethodCall(f_ctx, f_cg, re, subject)))}));
  f_ctx.Declare(result, JsType::kUnknown);

  // V8CommonProfile.swift:778-780.
  PushAll(&body,
          f_cg.Body(f_ctx, BodySize(r, kRegExpPathsMaxStatements.AsInt())));
  body.push_back(mk::Return(t, mk::Id(t, result)));
  Push(out, mk::FunctionDecl(t, fn_name, {}, std::move(body)));
  ctx.Declare(fn_name, JsType::kFunction);

  // V8CommonProfile.swift:783-789. The calls are guarded, so that the slow
  // path is always switched off again.
  for (int i = 0; i < 2; i++) {
    PushAll(out, cg.DeclareGuarded(mk::Call(t, mk::Id(t, fn_name), {}),
                                   JsType::kUnknown, "r"));
  }
  Push(out, cg.Stmt(mk::Native(t, "SetForceSlowPath", {mk::Bool(t, true)})));
  PushAll(out, cg.DeclareGuarded(mk::Call(t, mk::Id(t, fn_name), {}),
                                 JsType::kUnknown, "r"));
  Push(out, cg.Stmt(mk::Native(t, "SetForceSlowPath", {mk::Bool(t, false)})));
  return true;
}

// --- SerializerRoundTrip ----------------------------------------------------

REDLINE_TUNABLE(kSerializerRoundTripVariableChance,
                "generators.SerializerRoundTrip.variable_chance", 0.6, 0, 1,
                "probability to serialize a visible variable rather than a new "
                "value");
REDLINE_TUNABLE(kSerializerRoundTripFlipChance,
                "generators.SerializerRoundTrip.flip_chance", 0.5, 0, 1,
                "probability that a change flips one bit rather than replaces "
                "the byte");
REDLINE_TUNABLE(kSerializerRoundTripMaxIndex,
                "generators.SerializerRoundTrip.max_index", 99, 0, 4095,
                "largest index of a changed byte");
REDLINE_TUNABLE(kSerializerRoundTripSmallIndexChance,
                "generators.SerializerRoundTrip.small_index_chance", 0.5, 0, 1,
                "probability that the index of a changed byte is at most "
                "small_index_max, because small values have short encodings");
REDLINE_TUNABLE(kSerializerRoundTripSmallIndexMax,
                "generators.SerializerRoundTrip.small_index_max", 15, 0, 4095,
                "largest small index of a changed byte");
REDLINE_TUNABLE(kSerializerRoundTripMaxExtraChanges,
                "generators.SerializerRoundTrip.max_extra_changes", 1, 0, 16,
                "changes of the buffer after the first one (geometric)");

// A new value for the serializer: a literal, or an object of a builtin type
// that the serializer knows.
Expression* SerializableValue(GenContext& ctx, CodeGen& cg) {
  Random& r = ctx.rng();
  switch (r.Index(4)) {
    case 0:
      return cg.ObjectLiteral(1);
    case 1:
      return cg.ArrayLiteral(1);
    case 2: {
      JsType type = r.Pick({JsType::kMap, JsType::kSet, JsType::kTypedArray,
                            JsType::kArrayBuffer, JsType::kDate,
                            JsType::kRegExp, JsType::kError});
      if (Expression* e = cg.Construct(type)) {
        return e;
      }
      return cg.Literal(type);
    }
    default:
      return cg.Value(JsType::kUnknown, 1);
  }
}

// Fuzzilli's ValueSerializerFuzzer (V8CommonProfile.swift:606-640).
// d8.serializer.serialize() writes a value to an ArrayBuffer. The program
// flips a bit or replaces a byte through a Uint8Array view, and then
// deserializes the buffer. Both calls can throw, so they are guarded.
GEN(SerializerRoundTrip) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  auto serializer = [&]() {
    return mk::Member(t, mk::Id(t, "d8"), "serializer");
  };
  Expression* value = nullptr;
  if (r.Chance(kSerializerRoundTripVariableChance())) {
    value = cg.Variable();
  }
  if (value == nullptr) {
    value = SerializableValue(ctx, cg);
  }
  PushAll(out, cg.DeclareGuarded(
                   mk::CallMethod(t, serializer(), "serialize", {value}),
                   JsType::kArrayBuffer, "buf"));
  std::string_view buffer = LastDeclared(ctx);
  // A missing buffer (serialize threw) gives an empty view.
  Push(out, cg.Declare(mk::New(t, mk::Id(t, "Uint8Array"), {mk::Id(t, buffer)}),
                       JsType::kTypedArray, "u8"));
  std::string_view view = LastDeclared(ctx);

  // V8CommonProfile.swift:621-633.
  const int changes =
      1 + r.SmallCount(kSerializerRoundTripMaxExtraChanges.AsInt());
  for (int i = 0; i < changes; i++) {
    const int max_index = r.Chance(kSerializerRoundTripSmallIndexChance())
                              ? kSerializerRoundTripSmallIndexMax.AsInt()
                              : kSerializerRoundTripMaxIndex.AsInt();
    Expression* element =
        mk::Index(t, mk::Id(t, view),
                  mk::Num(t, static_cast<double>(r.Range(0, max_index))));
    if (r.Chance(kSerializerRoundTripFlipChance())) {
      Push(out,
           cg.Stmt(mk::Assign(t, element,
                              mk::Num(t, static_cast<double>(1 << r.Index(8))),
                              AssignOp::kBitXorAssign)));
    } else {
      Push(out,
           cg.Stmt(mk::Assign(t, element,
                              mk::Num(t, static_cast<double>(r.Index(256))))));
    }
  }
  PushAll(out, cg.DeclareGuarded(mk::CallMethod(t, serializer(), "deserialize",
                                                {mk::Id(t, buffer)}),
                                 JsType::kUnknown, "v"));
  return true;
}

// --- FastApiCall ------------------------------------------------------------

REDLINE_TUNABLE(kFastApiCallNoParamsChance,
                "generators.FastApiCall.no_params_chance", 0.5, 0, 1,
                "probability that the calling function has no parameters");
REDLINE_TUNABLE(kFastApiCallMaxParams, "generators.FastApiCall.max_params", 4,
                1, 16, "most parameters of the calling function");
REDLINE_TUNABLE(kFastApiCallParamArgumentChance,
                "generators.FastApiCall.param_argument_chance", 0.3, 0, 1,
                "probability that an argument of the API call is a parameter "
                "of the calling function");
REDLINE_TUNABLE(kFastApiCallEdgeValueChance,
                "generators.FastApiCall.edge_value_chance", 0.3, 0, 1,
                "probability that a numeric argument is an edge value of the C "
                "integer and float types");
REDLINE_TUNABLE(kFastApiCallMaxStatements,
                "generators.FastApiCall.max_statements", 3, 1, 16,
                "most generated statements in the calling function before the "
                "API call");
REDLINE_TUNABLE(kFastApiCallGuardCallsChance,
                "generators.FastApiCall.guard_calls_chance", 0.5, 0, 1,
                "probability that the calls of the calling function are "
                "guarded");
REDLINE_TUNABLE(kFastApiCallTurbofanChance,
                "generators.FastApiCall.turbofan_chance", 0.6, 0, 1,
                "probability of %OptimizeFunctionOnNextCall rather than "
                "%OptimizeMaglevOnNextCall");

// A method of d8.test.FastCAPI (src/d8/d8-test.cc) and the kinds of its
// arguments. Letters i, u, l, w, f, d, b, n and s name scalar types. Letters
// U, A, T (`typed_array`), p and x name arrays, pointers or any value.
struct FastApiMethod {
  const char* name;
  const char* args;
  const char* typed_array;
};

constexpr FastApiMethod kFastApiMethods[] = {
    {"add_32bit_int", "iu", nullptr},
    {"add_all", "iulwfd", nullptr},
    {"add_all_no_options", "iulwfd", nullptr},
    {"add_all_annotate_enforce_range", "iulw", nullptr},
    {"add_all_5args_enforce_range", "iiiii", nullptr},
    {"overloaded_add_all_32bit_int", "iiiii", nullptr},
    {"overloaded_add_all_32bit_int", "iiiiii", nullptr},
    {"overloaded_add_all_8args", "iiiiiiii", nullptr},
    {"overloaded_add_all_32bit_int_no_sig", "iiiiii", nullptr},
    {"add_all_sequence", "A", nullptr},
    {"add_all_overload", "A", nullptr},
    {"add_all_overload", "T", "Int32Array"},
    {"add_all_overload", "T", "Float64Array"},
    {"add_all_uint8_typed_array", "T", "Uint8Array"},
    {"add_all_int32_typed_array", "T", "Int32Array"},
    {"add_all_int64_typed_array", "T", "BigInt64Array"},
    {"add_all_uint64_typed_array", "T", "BigUint64Array"},
    {"add_all_uint32_typed_array", "T", "Uint32Array"},
    {"add_all_float32_typed_array", "T", "Float32Array"},
    {"add_all_float64_typed_array", "T", "Float64Array"},
    {"enforce_range_compare_i32", "bdd", nullptr},
    {"enforce_range_compare_u32", "bdd", nullptr},
    {"enforce_range_compare_i64", "bdd", nullptr},
    {"enforce_range_compare_u64", "bdd", nullptr},
    {"clamp_compare_i32", "bdd", nullptr},
    {"clamp_compare_u32", "bdd", nullptr},
    {"clamp_compare_i64", "bdd", nullptr},
    {"clamp_compare_u64", "bdd", nullptr},
    {"check_restricted_float32", "f", nullptr},
    {"check_restricted_float64", "d", nullptr},
    {"is_fast_c_api_object", "x", nullptr},
    {"call_to_number", "x", nullptr},
    {"sum_int64_as_number", "ll", nullptr},
    {"sum_uint64_as_number", "ww", nullptr},
    {"sum_int64_as_bigint", "nn", nullptr},
    {"sum_uint64_as_bigint", "nn", nullptr},
    {"copy_string", "sU", nullptr},
    {"get_pointer", "", nullptr},
    {"get_null_pointer", "", nullptr},
    {"pass_pointer", "p", nullptr},
    {"compare_pointers", "pp", nullptr},
    {"throw_no_fallback", "", nullptr},
};

// A number for an argument of C type `kind`: often an edge value of the C
// types, else a generated number.
Expression* FastApiNumber(CodeGen& cg, char kind) {
  Random& r = cg.rng();
  Tree* t = cg.tree();
  if (!r.Chance(kFastApiCallEdgeValueChance())) {
    if (kind == 'f' || kind == 'd') {
      return cg.Value(JsType::kNumber, 1);
    }
    return cg.Integer();
  }
  static const double kEdges[] = {0,
                                  -0.0,
                                  1,
                                  -1,
                                  0.5,
                                  -1.5,
                                  127,
                                  255,
                                  65535,
                                  2147483647.0,
                                  -2147483648.0,
                                  2147483648.0,
                                  4294967295.0,
                                  4294967296.0,
                                  9007199254740991.0,
                                  -9007199254740991.0,
                                  1.8446744073709552e19,
                                  3.4028234663852886e38,
                                  1e308,
                                  5e-324};
  return mk::Num(t, kEdges[r.Index(sizeof(kEdges) / sizeof(kEdges[0]))]);
}

// An argument of `kind` for a FastCAPI method. `api` is the receiver.
Expression* FastApiArgument(CodeGen& cg, char kind, const FastApiMethod& method,
                            std::string_view api) {
  Random& r = cg.rng();
  Tree* t = cg.tree();
  auto small_numbers = [&]() {
    std::vector<Expression*> elements;
    const int n = r.SmallCount(kFastApiCallMaxParams.AsInt() * 2);
    for (int i = 0; i < n; i++) {
      elements.push_back(FastApiNumber(cg, 'd'));
    }
    return elements;
  };
  switch (kind) {
    case 'b':
      return cg.Boolean();
    case 'n':
      return cg.BigInt();
    case 's':
      return cg.Value(JsType::kString, 1);
    case 'U':
      return mk::New(t, mk::Id(t, "Uint8Array"),
                     {mk::Num(t, static_cast<double>(r.Range(0, 64)))});
    case 'A':
      return mk::Array(t, small_numbers());
    case 'T': {
      std::vector<Expression*> elements;
      const bool big = std::string_view(method.typed_array).find("Big") == 0;
      const int n = r.SmallCount(kFastApiCallMaxParams.AsInt() * 2);
      for (int i = 0; i < n; i++) {
        elements.push_back(big ? cg.BigInt() : FastApiNumber(cg, 'd'));
      }
      return mk::New(t, mk::Id(t, method.typed_array),
                     {mk::Array(t, std::move(elements))});
    }
    case 'p':
      return r.Coin() ? static_cast<Expression*>(mk::CallMethod(
                            t, mk::Id(t, api), "get_pointer", {}))
                      : static_cast<Expression*>(mk::Null(t));
    case 'x':
      return cg.Value(JsType::kUnknown, 1);
    default:
      return FastApiNumber(cg, kind);
  }
}

// Fuzzilli's FastApiCallFuzzer (V8CommonProfile.swift:1187-1218), with the
// methods of d8.test.FastCAPI at :97-100 and :1136-1141. A function calls a
// method of the API object with typed arguments in a try block. The program
// prepares the function, calls it twice, optimizes it and calls it again.
// This port knows more methods than Fuzzilli: all the methods of
// src/d8/d8-test.cc that work without Wasm, and the fast attribute.
GEN(FastApiCall) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Push(out,
       cg.Declare(mk::New(t,
                          mk::Member(t, mk::Member(t, mk::Id(t, "d8"), "test"),
                                     "FastCAPI"),
                          {}),
                  JsType::kObject, "api"));
  std::string_view api = LastDeclared(ctx);

  std::string_view fn_name = ctx.builder.FreshName("f");
  GenContext f_ctx = ctx.Nested(true, false, false, false);
  const int param_count =
      r.Chance(kFastApiCallNoParamsChance())
          ? 0
          : static_cast<int>(r.Range(1, kFastApiCallMaxParams.AsInt()));
  std::vector<std::string_view> params;
  for (int i = 0; i < param_count; i++) {
    params.push_back(ctx.builder.FreshName("a"));
    f_ctx.Declare(params.back(), JsType::kUnknown);
  }
  CodeGen f_cg(f_ctx);
  Statements body =
      f_cg.Body(f_ctx, BodySize(r, kFastApiCallMaxStatements.AsInt()));

  // The API call. A setter of the fast attribute has no result.
  constexpr size_t kMethods =
      sizeof(kFastApiMethods) / sizeof(kFastApiMethods[0]);
  std::string_view result = ctx.builder.FreshName("r");
  body.push_back(mk::Let(t, result, nullptr));
  Expression* call = nullptr;
  const size_t pick = r.Index(kMethods + 1);
  if (pick == kMethods) {
    Expression* attribute = mk::Member(t, mk::Id(t, api), "fast_attribute");
    call = r.Coin() ? attribute
                    : static_cast<Expression*>(
                          mk::Assign(t, attribute, FastApiNumber(f_cg, 'i')));
  } else {
    const FastApiMethod& method = kFastApiMethods[pick];
    std::vector<Expression*> args;
    for (const char* k = method.args; *k != '\0'; k++) {
      if (!params.empty() && r.Chance(kFastApiCallParamArgumentChance())) {
        args.push_back(mk::Id(t, r.Pick(params)));
      } else {
        args.push_back(FastApiArgument(f_cg, *k, method, api));
      }
    }
    call = mk::CallMethod(t, mk::Id(t, api), method.name, std::move(args));
  }
  body.push_back(
      Guarded(ctx, {f_cg.Stmt(mk::Assign(t, mk::Id(t, result), call))}));
  body.push_back(mk::Return(t, mk::Id(t, result)));
  FunctionDeclaration* fn =
      mk::FunctionDecl(t, fn_name, params, std::move(body));
  Push(out, fn);
  ctx.Declare(fn_name, JsType::kFunction);

  // The same arguments for each call (V8CommonProfile.swift:1204-1215).
  std::vector<std::string_view> arg_names;
  const int arg_count = static_cast<int>(r.Range(0, param_count + 1));
  for (int i = 0; i < arg_count; i++) {
    Push(out, cg.Declare(FastApiNumber(cg, r.Pick({'i', 'd'})), JsType::kNumber,
                         "x"));
    arg_names.push_back(LastDeclared(ctx));
  }
  const bool guard_calls = r.Chance(kFastApiCallGuardCallsChance());
  auto call_fn = [&]() {
    std::vector<Expression*> args;
    for (std::string_view a : arg_names) {
      args.push_back(mk::Id(t, a));
    }
    Statement* s = cg.Stmt(mk::Call(t, mk::Id(t, fn_name), std::move(args)));
    Push(out, guard_calls ? Guarded(ctx, {s}) : s);
  };
  call_fn();
  Push(out, cg.Stmt(mk::Native(t, "PrepareFunctionForOptimization",
                               {mk::Id(t, fn_name)})));
  call_fn();
  call_fn();
  Push(out, cg.Stmt(mk::Native(t,
                               r.Chance(kFastApiCallTurbofanChance())
                                   ? "OptimizeFunctionOnNextCall"
                                   : "OptimizeMaglevOnNextCall",
                               {mk::Id(t, fn_name)})));
  call_fn();
  return true;
}

// --- HomomorphicFeedback ----------------------------------------------------

REDLINE_TUNABLE(kHomomorphicFeedbackMinObjects,
                "generators.HomomorphicFeedback.min_objects", 5, 2, 32,
                "fewest objects");
REDLINE_TUNABLE(kHomomorphicFeedbackMaxObjects,
                "generators.HomomorphicFeedback.max_objects", 8, 2, 32,
                "most objects");
REDLINE_TUNABLE(kHomomorphicFeedbackWarmupObjects,
                "generators.HomomorphicFeedback.warmup_objects", 5, 1, 32,
                "objects passed to the consumer before the optimization "
                "request");
REDLINE_TUNABLE(kHomomorphicFeedbackPoolSize,
                "generators.HomomorphicFeedback.pool_size", 6, 1, 8,
                "property names in the shared pool");
REDLINE_TUNABLE(
    kHomomorphicFeedbackLoadChance,
    "generators.HomomorphicFeedback.load_chance", 0.7, 0, 1,
    "probability that the consumer loads the common property first");
REDLINE_TUNABLE(kHomomorphicFeedbackMaxStatements,
                "generators.HomomorphicFeedback.max_statements", 3, 1, 16,
                "most generated statements in the consumer");
REDLINE_TUNABLE(kHomomorphicFeedbackGuardChance,
                "generators.HomomorphicFeedback.guard_chance", 0.5, 0, 1,
                "probability that the calls in the loops are guarded");

// Fuzzilli's HomomorphicFeedbackFuzzer (V8CommonProfile.swift:794-849).
// Objects get a distinct, a common and again a distinct property from a small
// name pool. Thus the common property is at different offsets, and some
// objects share a map. A consumer loads the common property. It warms up on
// some objects, is optimized and then runs on all objects. The homomorphic
// mode of ShapeChange (generators.cc) is similar, with fresh names.
GEN(HomomorphicFeedback) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const int pool = kHomomorphicFeedbackPoolSize.AsInt();
  const int min_objects = kHomomorphicFeedbackMinObjects.AsInt();
  const int total = static_cast<int>(
      r.Range(min_objects,
              std::max(min_objects, kHomomorphicFeedbackMaxObjects.AsInt())));
  std::string_view common = PoolName(r, pool);
  std::vector<std::string_view> objects;
  for (int i = 0; i < total; i++) {
    Push(out,
         cg.Declare(mk::Object(t, {mk::Prop(t, PoolName(r, pool),
                                            cg.Value(JsType::kUnknown, 1))}),
                    JsType::kObject, "o"));
    objects.push_back(LastDeclared(ctx));
  }
  for (int pass = 0; pass < 2; pass++) {
    for (std::string_view o : objects) {
      std::string_view name = pass == 0 ? common : PoolName(r, pool);
      Push(out, cg.Stmt(mk::Assign(t, mk::Member(t, mk::Id(t, o), name),
                                   cg.Value(JsType::kUnknown, 1))));
    }
  }

  // V8CommonProfile.swift:819-828.
  std::string_view fn_name = ctx.builder.FreshName("f");
  std::string_view param = ctx.builder.FreshName("o");
  GenContext f_ctx = ctx.Nested(true, false, false, false);
  f_ctx.Declare(param, JsType::kObject);
  CodeGen f_cg(f_ctx);
  Statements body;
  if (r.Chance(kHomomorphicFeedbackLoadChance())) {
    body.push_back(f_cg.Declare(mk::Member(t, mk::Id(t, param), common),
                                JsType::kUnknown));
  }
  PushAll(
      &body,
      f_cg.Body(f_ctx, BodySize(r, kHomomorphicFeedbackMaxStatements.AsInt())));
  body.push_back(mk::Return(t, f_cg.Value(JsType::kUnknown, 1)));
  Push(out, mk::FunctionDecl(t, fn_name, {param}, std::move(body)));
  ctx.Declare(fn_name, JsType::kFunction);

  // V8CommonProfile.swift:830-848.
  const bool guard = r.Chance(kHomomorphicFeedbackGuardChance());
  auto loop = [&](int count) {
    std::vector<Expression*> elements;
    for (int i = 0; i < count; i++) {
      elements.push_back(mk::Id(t, objects[static_cast<size_t>(i)]));
    }
    std::string_view x = ctx.builder.FreshName("x");
    Statement* call = cg.Stmt(mk::Call(t, mk::Id(t, fn_name), {mk::Id(t, x)}));
    Push(out, mk::ForOf(t, VariableKind::kConst, x,
                        mk::Array(t, std::move(elements)),
                        mk::Block(t, {guard ? Guarded(ctx, {call}) : call})));
  };
  Push(out, cg.Stmt(mk::Native(t, "PrepareFunctionForOptimization",
                               {mk::Id(t, fn_name)})));
  loop(std::min(total, kHomomorphicFeedbackWarmupObjects.AsInt()));
  Push(out, cg.Stmt(mk::Native(t, "OptimizeFunctionOnNextCall",
                               {mk::Id(t, fn_name)})));
  loop(total);
  return true;
}

// --- ProtoAssignSeq ---------------------------------------------------------

REDLINE_TUNABLE(kProtoAssignSeqMinStores,
                "generators.ProtoAssignSeq.min_stores", 2, 1, 64,
                "fewest stores to the prototype in a row");
REDLINE_TUNABLE(kProtoAssignSeqMaxStores,
                "generators.ProtoAssignSeq.max_stores", 10, 1, 64,
                "most stores to the prototype in a row");
REDLINE_TUNABLE(kProtoAssignSeqPoolSize, "generators.ProtoAssignSeq.pool_size",
                6, 1, 8, "property names in the shared pool");
REDLINE_TUNABLE(
    kProtoAssignSeqMaxParams, "generators.ProtoAssignSeq.max_params", 2, 0, 8,
    "most parameters of the containing function and of the function "
    "that gets the prototype stores");
REDLINE_TUNABLE(kProtoAssignSeqMaxStatements,
                "generators.ProtoAssignSeq.max_statements", 3, 1, 16,
                "most generated statements in each body of the template");
REDLINE_TUNABLE(kProtoAssignSeqSuperClassChance,
                "generators.ProtoAssignSeq.super_class_chance", 0.5, 0, 1,
                "probability that a class extends a visible class or a builtin "
                "constructor");

// Fuzzilli's ProtoAssignSeqOptFuzzer (V8CommonProfile.swift:1220-1266). In a
// containing function, the program makes a plain function, an arrow, a
// generator, an async function, a constructor or a class. It reads the
// prototype, stores 2-10 primitives to `prototype.<name>` in a row and calls
// `new` in a try block. Arrows, generators and async functions are not
// constructors. The proto_seq mode of ShapeChange (generators.cc) is a
// simpler version.
GEN(ProtoAssignSeq) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const int pool = kProtoAssignSeqPoolSize.AsInt();
  const int max_statements = kProtoAssignSeqMaxStatements.AsInt();
  std::string_view outer_name = ctx.builder.FreshName("f");
  GenContext o_ctx = ctx.Nested(true, false, false, false);
  std::vector<Expression*> outer_params = cg.Parameters(
      o_ctx, static_cast<int>(r.Range(0, kProtoAssignSeqMaxParams.AsInt())));
  CodeGen o_cg(o_ctx);
  Statements outer;

  // V8CommonProfile.swift:1225-1241.
  std::string_view fn = ctx.builder.FreshName("g");
  const size_t kind = r.Index(6);
  if (kind == 5) {
    Expression* super_class = nullptr;
    if (r.Chance(kProtoAssignSeqSuperClassChance())) {
      super_class = o_cg.Variable(JsType::kClass);
      if (super_class == nullptr) {
        super_class = mk::Id(t, r.Pick({"Object", "Array", "Error", "Map",
                                        "Promise", "Uint8Array"}));
      }
    }
    std::vector<Node*> members;
    const int fields = r.SmallCount(max_statements);
    for (int i = 0; i < fields; i++) {
      GenContext f_ctx = o_ctx.Nested(true, true, false, false);
      f_ctx.syntax.strict = true;
      f_ctx.syntax.in_class_field = true;
      f_ctx.syntax.has_arguments = false;
      f_ctx.syntax.in_method = true;
      CodeGen f_cg(f_ctx);
      members.push_back(mk::ClassField(t, PoolName(r, pool),
                                       f_cg.Value(JsType::kUnknown, 1)));
    }
    if (r.Coin()) {
      GenContext m_ctx = o_ctx.Nested(true, false, false, false);
      m_ctx.syntax.strict = true;
      m_ctx.syntax.in_method = true;
      m_ctx.syntax.has_super_class = super_class != nullptr;
      CodeGen m_cg(m_ctx);
      Statements m_body = m_cg.Body(m_ctx, BodySize(r, max_statements));
      m_body.push_back(mk::Return(t, m_cg.Value(JsType::kUnknown, 1)));
      members.push_back(mk::ClassMethod(
          t, PoolName(r, pool), mk::Function(t, "", {}, std::move(m_body))));
    }
    outer.push_back(mk::ClassDecl(t, fn, super_class, std::move(members)));
    o_ctx.Declare(fn, JsType::kClass, true);
  } else {
    const bool is_arrow = kind == 1;
    const bool is_generator = kind == 2;
    const bool is_async = kind == 3;
    GenContext g_ctx = o_ctx.Nested(true, is_arrow, is_async, is_generator);
    std::vector<Expression*> params = o_cg.Parameters(
        g_ctx, static_cast<int>(r.Range(0, kProtoAssignSeqMaxParams.AsInt())));
    CodeGen g_cg(g_ctx);
    Statements body;
    if (kind == 4) {
      // A constructor stores to `this` (V8CommonProfile.swift:1235).
      body.push_back(
          g_cg.Stmt(mk::Assign(t, mk::Member(t, mk::This(t), PoolName(r, pool)),
                               g_cg.Value(JsType::kUnknown, 1))));
    }
    PushAll(&body, g_cg.Body(g_ctx, BodySize(r, max_statements)));
    body.push_back(mk::Return(t, g_cg.Value(JsType::kUnknown, 1)));
    if (is_arrow) {
      ArrowFunctionExpression* arrow = mk::Arrow(t, {}, std::move(body));
      arrow->params = std::move(params);
      outer.push_back(mk::Const(t, fn, arrow));
      o_ctx.Declare(fn, JsType::kFunction, true);
    } else {
      FunctionDeclaration* decl =
          mk::FunctionDecl(t, fn, {}, std::move(body), is_async, is_generator);
      decl->params = std::move(params);
      outer.push_back(decl);
      o_ctx.Declare(fn, kind == 4 ? JsType::kClass : JsType::kFunction);
    }
  }

  // V8CommonProfile.swift:1242-1253. Arrows and async functions have no
  // prototype, so the stores then go to a new object.
  outer.push_back(o_cg.Declare(mk::Member(t, mk::Id(t, fn), "prototype"),
                               JsType::kUnknown, "proto"));
  if (kind == 1 || kind == 3) {
    outer.push_back(o_cg.Stmt(mk::Assign(
        t, mk::Member(t, mk::Id(t, fn), "prototype"), mk::Object(t, {}))));
  }
  PushAll(&outer, o_cg.Body(o_ctx, r.SmallCount(max_statements)));
  const int min_stores = kProtoAssignSeqMinStores.AsInt();
  const int stores = static_cast<int>(r.Range(
      min_stores, std::max(min_stores, kProtoAssignSeqMaxStores.AsInt())));
  for (int i = 0; i < stores; i++) {
    JsType type = r.Pick({JsType::kNumber, JsType::kString, JsType::kBoolean,
                          JsType::kBigInt, JsType::kNullish});
    outer.push_back(o_cg.Stmt(
        mk::Assign(t,
                   mk::Member(t, mk::Member(t, mk::Id(t, fn), "prototype"),
                              PoolName(r, pool)),
                   o_cg.Value(type, 1))));
  }

  // V8CommonProfile.swift:1254-1261.
  PushAll(&outer, o_cg.Body(o_ctx, r.SmallCount(max_statements)));
  PushAll(&outer,
          o_cg.DeclareGuarded(mk::New(t, mk::Id(t, fn), o_cg.Arguments(0, 2)),
                              JsType::kObject, "o"));
  PushAll(&outer, o_cg.Body(o_ctx, r.SmallCount(max_statements)));
  outer.push_back(mk::Return(t, o_cg.Value(JsType::kUnknown, 1)));
  FunctionDeclaration* decl =
      mk::FunctionDecl(t, outer_name, {}, std::move(outer));
  const size_t outer_arity = outer_params.size();
  decl->params = std::move(outer_params);
  Push(out, decl);
  ctx.Declare(outer_name, JsType::kFunction);

  // V8CommonProfile.swift:1263-1264.
  PushAll(out,
          cg.DeclareGuarded(cg.CallWithArguments(mk::Id(t, outer_name),
                                                 static_cast<int>(outer_arity)),
                            JsType::kUnknown, "r"));
  return true;
}

// --- MapTransitionFocus -----------------------------------------------------

REDLINE_TUNABLE(kMapTransitionFocusMinSteps,
                "generators.MapTransitionFocus.min_steps", 4, 1, 64,
                "fewest focused steps in the block");
REDLINE_TUNABLE(kMapTransitionFocusMaxSteps,
                "generators.MapTransitionFocus.max_steps", 10, 1, 64,
                "most focused steps in the block");
REDLINE_TUNABLE(kMapTransitionFocusPoolSize,
                "generators.MapTransitionFocus.pool_size", 6, 1, 8,
                "property names in the shared pool");
REDLINE_TUNABLE(kMapTransitionFocusMaxExtraProps,
                "generators.MapTransitionFocus.max_extra_props", 3, 0, 8,
                "properties of a new object after the first one");
REDLINE_TUNABLE(kMapTransitionFocusMaxStores,
                "generators.MapTransitionFocus.max_stores", 3, 1, 16,
                "most stores in one store step");
REDLINE_TUNABLE(kMapTransitionFocusMakerCalls,
                "generators.MapTransitionFocus.maker_calls", 3, 1, 16,
                "objects made by each object maker, constructor or class");
REDLINE_TUNABLE(kMapTransitionFocusLoopCount,
                "generators.MapTransitionFocus.loop_count", 100, 1, 10000,
                "iterations of the loop that calls a function with objects");
REDLINE_TUNABLE(kMapTransitionFocusMaxStatements,
                "generators.MapTransitionFocus.max_statements", 3, 1, 16,
                "most generated statements in a function of the block");

// The weights of Fuzzilli's focused generators (V8CommonProfile.swift:575-588),
// in the order of the cases in MapTransitionFocus. The last step merges the
// function definition and the function call generators.
constexpr double kMapTransitionStepWeights[] = {2, 1, 1, 1, 1, 10, 10, 5, 4};

// Fuzzilli's MapTransitionFuzzer (V8CommonProfile.swift:415-604). A block of
// focused steps works on a few objects and a small shared name pool. The steps
// make literals, makers, constructors and classes, and load, store or
// configure properties. Then %HeapObjectVerify checks each object of the block.
GEN(MapTransitionFocus) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const int pool = kMapTransitionFocusPoolSize.AsInt();
  const int max_statements = kMapTransitionFocusMaxStatements.AsInt();
  std::vector<std::string_view> objects;

  // Property names "a" and up to max_extra_props names more, without
  // duplicates (V8CommonProfile.swift:430-446).
  auto names = [&]() {
    std::vector<std::string_view> result = {"a"};
    const int extra =
        r.SmallCount(kMapTransitionFocusMaxExtraProps.AsInt(), 0.7);
    for (int i = 0; i < extra; i++) {
      std::string_view name = PoolName(r, pool);
      if (std::find(result.begin(), result.end(), name) == result.end()) {
        result.push_back(name);
      }
    }
    return result;
  };
  // A literal of `names` with values that `vcg` makes.
  auto literal = [&](CodeGen& vcg) {
    std::vector<Node*> props;
    for (std::string_view name : names()) {
      props.push_back(mk::Prop(t, name, vcg.Value(JsType::kUnknown, 1)));
    }
    return mk::Object(t, std::move(props));
  };
  auto some_object = [&]() -> Expression* {
    if (objects.empty()) {
      return nullptr;
    }
    return mk::Id(t, r.Pick(objects));
  };
  // `count` objects from `new C()` or `maker()`.
  auto make_objects = [&](std::string_view fn, bool construct, int arity) {
    const int count = kMapTransitionFocusMakerCalls.AsInt();
    for (int i = 0; i < count; i++) {
      Expression* call = construct
                             ? static_cast<Expression*>(mk::New(
                                   t, mk::Id(t, fn), cg.Arguments(0, arity)))
                             : static_cast<Expression*>(mk::Call(
                                   t, mk::Id(t, fn), cg.Arguments(0, arity)));
      Push(out, cg.Declare(call, JsType::kObject, "o"));
      objects.push_back(LastDeclared(ctx));
    }
  };

  const int min_steps = kMapTransitionFocusMinSteps.AsInt();
  const int steps = static_cast<int>(r.Range(
      min_steps, std::max(min_steps, kMapTransitionFocusMaxSteps.AsInt())));
  std::vector<double> weights(std::begin(kMapTransitionStepWeights),
                              std::end(kMapTransitionStepWeights));
  for (int step = 0; step < steps; step++) {
    size_t kind = r.Weighted(weights);
    // Loads, stores and defineProperty need an object of the block.
    if (objects.empty() && kind >= 5 && kind <= 7) {
      kind = 1;
    }
    switch (kind) {
      case 0: {
        // A primitive value (V8CommonProfile.swift:449-463).
        JsType type =
            r.Pick({JsType::kNumber, JsType::kNumber, JsType::kString});
        Push(out,
             cg.Declare(type == JsType::kString ? cg.String() : cg.Number(),
                        type, "v"));
        break;
      }
      case 1: {
        // An object literal (:464-471).
        Push(out, cg.Declare(literal(cg), JsType::kObject, "o"));
        objects.push_back(LastDeclared(ctx));
        break;
      }
      case 2: {
        // An object maker called several times (:472-483).
        std::string_view fn = ctx.builder.FreshName("mk");
        GenContext f_ctx = ctx.Nested(true, false, false, false);
        std::vector<Expression*> params =
            cg.Parameters(f_ctx, static_cast<int>(r.Index(3)));
        const int arity = static_cast<int>(params.size());
        CodeGen f_cg(f_ctx);
        FunctionDeclaration* decl =
            mk::FunctionDecl(t, fn, {}, {mk::Return(t, literal(f_cg))});
        decl->params = std::move(params);
        Push(out, decl);
        ctx.Declare(fn, JsType::kFunction);
        make_objects(fn, false, arity);
        break;
      }
      case 3: {
        // A constructor function that stores to `this` (:484-496).
        std::string_view fn = ctx.builder.FreshName("C");
        GenContext f_ctx = ctx.Nested(true, false, false, false);
        std::vector<Expression*> params =
            cg.Parameters(f_ctx, static_cast<int>(r.Index(3)));
        const int arity = static_cast<int>(params.size());
        CodeGen f_cg(f_ctx);
        Statements body;
        for (std::string_view name : names()) {
          body.push_back(
              f_cg.Stmt(mk::Assign(t, mk::Member(t, mk::This(t), name),
                                   f_cg.Value(JsType::kUnknown, 1))));
        }
        FunctionDeclaration* decl =
            mk::FunctionDecl(t, fn, {}, std::move(body));
        decl->params = std::move(params);
        Push(out, decl);
        ctx.Declare(fn, JsType::kClass);
        make_objects(fn, true, arity);
        break;
      }
      case 4: {
        // A class with instance fields, sometimes a subclass (:497-511).
        std::string_view fn = ctx.builder.FreshName("C");
        Expression* super_class =
            r.Coin() ? cg.Variable(JsType::kClass) : nullptr;
        GenContext f_ctx = ctx.Nested(true, true, false, false);
        f_ctx.syntax.strict = true;
        f_ctx.syntax.in_class_field = true;
        f_ctx.syntax.has_arguments = false;
        f_ctx.syntax.in_method = true;
        CodeGen f_cg(f_ctx);
        std::vector<Node*> members;
        for (std::string_view name : names()) {
          members.push_back(
              mk::ClassField(t, name, f_cg.Value(JsType::kUnknown, 1)));
        }
        Push(out, mk::ClassDecl(t, fn, super_class, std::move(members)));
        ctx.Declare(fn, JsType::kClass, true);
        // A visible class can need arguments, so these objects are guarded.
        const int count = kMapTransitionFocusMakerCalls.AsInt();
        for (int i = 0; i < count; i++) {
          PushAll(out, cg.DeclareGuarded(mk::New(t, mk::Id(t, fn), {}),
                                         JsType::kObject, "o"));
          objects.push_back(LastDeclared(ctx));
        }
        break;
      }
      case 5: {
        // Stores of shared names (:517-524).
        const int stores =
            1 + r.SmallCount(kMapTransitionFocusMaxStores.AsInt() - 1);
        for (int i = 0; i < stores; i++) {
          Push(out, cg.Stmt(mk::Assign(
                        t, mk::Member(t, some_object(), PoolName(r, pool)),
                        cg.Value(JsType::kUnknown, 1))));
        }
        break;
      }
      case 6: {
        // A load of a shared name (:512-516).
        Push(out, cg.Declare(mk::Member(t, some_object(), PoolName(r, pool)),
                             JsType::kUnknown, "v"));
        break;
      }
      case 7: {
        // Object.defineProperty with random flags (:525-531).
        std::vector<Node*> props = {
            mk::Prop(t, "value", cg.Value(JsType::kUnknown, 1))};
        for (std::string_view flag :
             {"writable", "enumerable", "configurable"}) {
          if (r.Coin()) {
            props.push_back(mk::Prop(t, flag, mk::Bool(t, r.Coin())));
          }
        }
        // A non-configurable property can make a later redefinition throw.
        Push(out,
             Guarded(ctx, {cg.Stmt(mk::CallMethod(
                              t, mk::Id(t, "Object"), "defineProperty",
                              {some_object(), mk::Str(t, PoolName(r, pool)),
                               mk::Object(t, std::move(props))}))}));
        break;
      }
      default: {
        // A function with object parameters, called in a loop (:532-571).
        std::string_view fn = ctx.builder.FreshName("f");
        std::string_view p0 = ctx.builder.FreshName("a");
        std::string_view p1 = ctx.builder.FreshName("a");
        GenContext f_ctx = ctx.Nested(true, false, false, false);
        f_ctx.Declare(p1, JsType::kObject);
        f_ctx.Declare(p0, JsType::kObject);
        CodeGen f_cg(f_ctx);
        Statements body = f_cg.Body(f_ctx, BodySize(r, max_statements));
        body.push_back(mk::Return(t, f_cg.Value(JsType::kUnknown, 1)));
        Push(out, mk::FunctionDecl(t, fn, {p0, p1}, std::move(body)));
        ctx.Declare(fn, JsType::kFunction);
        auto object_arg = [&]() -> Expression* {
          Expression* o = some_object();
          return o != nullptr ? o : literal(cg);
        };
        std::string_view i = ctx.builder.FreshName("i");
        Statement* call =
            cg.Stmt(mk::Call(t, mk::Id(t, fn), {object_arg(), object_arg()}));
        Push(out, mk::CountedLoop(
                      t, i, mk::Num(t, kMapTransitionFocusLoopCount.AsInt()),
                      mk::Block(t, {Guarded(ctx, {call})})));
        break;
      }
    }
  }
  // V8CommonProfile.swift:600-603.
  for (std::string_view o : objects) {
    Push(out, cg.Stmt(mk::Native(t, "HeapObjectVerify", {mk::Id(t, o)})));
  }
  return true;
}

// --- JsonRoundTrip ----------------------------------------------------------

REDLINE_TUNABLE(kJsonRoundTripMaxPayloads,
                "generators.JsonRoundTrip.max_payloads", 5, 1, 16,
                "most values given to JSON.stringify");
REDLINE_TUNABLE(kJsonRoundTripVariableChance,
                "generators.JsonRoundTrip.variable_chance", 0.6, 0, 1,
                "probability to stringify a visible variable rather than a new "
                "value");
REDLINE_TUNABLE(kJsonRoundTripChangeChance,
                "generators.JsonRoundTrip.change_chance", 0.25, 0, 1,
                "probability that a JSON text gets a one-character change");
REDLINE_TUNABLE(kJsonRoundTripParsesPerPayload,
                "generators.JsonRoundTrip.parses_per_payload", 2, 1, 8,
                "JSON.parse calls for each JSON text");
REDLINE_TUNABLE(kJsonRoundTripDataChance,
                "generators.JsonRoundTrip.data_chance", 0.5, 0, 1,
                "probability that a new value is plain data (objects, arrays, "
                "numbers, strings, booleans and null) that JSON can represent");
REDLINE_TUNABLE(kJsonRoundTripMaxDataDepth,
                "generators.JsonRoundTrip.max_data_depth", 3, 0, 8,
                "deepest nesting of objects and arrays in plain data");
REDLINE_TUNABLE(kJsonRoundTripMaxDataElements,
                "generators.JsonRoundTrip.max_data_elements", 4, 0, 16,
                "most properties or elements of an object or array in plain "
                "data (geometric)");

// Plain data for JSON.stringify: nested object and array literals with
// numbers, strings, booleans and null. BigInts and functions are left out,
// because JSON.stringify rejects or drops them.
Expression* JsonData(CodeGen& cg, int depth) {
  Random& r = cg.rng();
  Tree* t = cg.tree();
  const bool nest = depth < kJsonRoundTripMaxDataDepth.AsInt();
  switch (r.Index(nest ? 7 : 5)) {
    case 0:
      return cg.Number();
    case 1:
      return cg.String();
    case 2:
      return cg.Boolean();
    case 3:
      return mk::Null(t);
    case 4:
      return cg.Integer();
    case 5: {
      std::vector<Expression*> elements;
      const int n = r.SmallCount(kJsonRoundTripMaxDataElements.AsInt(), 0.7);
      for (int i = 0; i < n; i++) {
        elements.push_back(JsonData(cg, depth + 1));
      }
      return mk::Array(t, std::move(elements));
    }
    default: {
      std::vector<Node*> props;
      const int n = r.SmallCount(kJsonRoundTripMaxDataElements.AsInt(), 0.7);
      for (int i = 0; i < n; i++) {
        props.push_back(
            mk::Prop(t, PoolName(r, n + 1), JsonData(cg, depth + 1)));
      }
      return mk::Object(t, std::move(props));
    }
  }
}

// Fuzzilli's JSONFuzzer (ProgramTemplates.swift:496-566). JSON.stringify
// makes 1-5 JSON texts. Some texts get a one-character change: the character
// at a fixed fraction of the length is changed with xor. Then guarded
// JSON.parse calls read random texts. The results are declared, so that the
// next code can use them.
GEN(JsonRoundTrip) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  auto json = [&]() { return mk::Id(t, "JSON"); };
  std::vector<std::string_view> texts;
  const int payloads = 1 + r.SmallCount(kJsonRoundTripMaxPayloads.AsInt() - 1);
  for (int i = 0; i < payloads; i++) {
    Expression* value = nullptr;
    if (r.Chance(kJsonRoundTripVariableChance())) {
      value = cg.Variable();
    }
    if (value == nullptr) {
      value = r.Chance(kJsonRoundTripDataChance())
                  ? JsonData(cg, 0)
                  : cg.Value(JsType::kUnknown, 1);
    }
    // A BigInt or a cyclic object makes JSON.stringify throw.
    PushAll(out,
            cg.DeclareGuarded(mk::CallMethod(t, json(), "stringify", {value}),
                              JsType::kString, "json"));
    texts.push_back(LastDeclared(ctx));
  }

  // ProgramTemplates.swift:511-554. Fuzzilli calls a helper function. This
  // port changes the text in place: the index is floor(fraction * length).
  for (std::string_view& text : texts) {
    if (!r.Chance(kJsonRoundTripChangeChance())) {
      continue;
    }
    Push(out,
         cg.Declare(mk::CallMethod(
                        t, mk::Id(t, "Math"), "floor",
                        {mk::Binary(t, BinaryOp::kMul, mk::Num(t, r.Double()),
                                    mk::Member(t, mk::Id(t, text), "length"))}),
                    JsType::kNumber, "i"));
    std::string_view index = LastDeclared(ctx);
    Expression* prefix = mk::CallMethod(t, mk::Id(t, text), "substring",
                                        {mk::Num(t, 0), mk::Id(t, index)});
    Expression* changed = mk::CallMethod(
        t, mk::Id(t, "String"), "fromCharCode",
        {mk::Binary(t, BinaryOp::kBitXor,
                    mk::CallMethod(t, mk::Id(t, text), "charCodeAt",
                                   {mk::Id(t, index)}),
                    mk::Num(t, static_cast<double>(r.Range(1, 127))))});
    Expression* suffix = mk::CallMethod(
        t, mk::Id(t, text), "substring",
        {mk::Binary(t, BinaryOp::kAdd, mk::Id(t, index), mk::Num(t, 1))});
    // An undefined text (stringify threw) makes these calls throw.
    PushAll(out, cg.DeclareGuarded(
                     mk::Binary(t, BinaryOp::kAdd,
                                mk::Binary(t, BinaryOp::kAdd, prefix, changed),
                                suffix),
                     JsType::kString, "json"));
    text = LastDeclared(ctx);
  }

  // ProgramTemplates.swift:556-562.
  const int parses =
      static_cast<int>(texts.size()) * kJsonRoundTripParsesPerPayload.AsInt();
  for (int i = 0; i < parses; i++) {
    PushAll(out, cg.DeclareGuarded(mk::CallMethod(t, json(), "parse",
                                                  {mk::Id(t, r.Pick(texts))}),
                                   JsType::kUnknown, "v"));
  }
  return true;
}

#undef GEN

}  // namespace

std::vector<ShapesGenerator> TemplateGenerators() {
  return {
      {"RegExpPaths",
       "regexp with an edge-case lastIndex and Symbol methods, called on the "
       "fast path and under %SetForceSlowPath(true)",
       0.4, kRecursive, GenRegExpPaths},
      {"SerializerRoundTrip",
       "d8.serializer.serialize(v), change one byte, "
       "d8.serializer.deserialize(buf)",
       0.4, kNoRequirement, GenSerializerRoundTrip},
      {"FastApiCall",
       "function calling a d8.test.FastCAPI method with typed arguments, "
       "prepared and optimized",
       0.4, kRecursive, GenFastApiCall},
      {"HomomorphicFeedback",
       "objects {p: v}.c = v .q = v from a small name pool, f(o) loads o.c, "
       "warm-up, optimize, call on all objects",
       0.4, kRecursive, GenHomomorphicFeedback},
      {"ProtoAssignSeq",
       "fresh function or class with a row of g.prototype.a = v stores and a "
       "guarded new g() inside a called function",
       0.4, kRecursive, GenProtoAssignSeq},
      {"MapTransitionFocus",
       "objects, makers, constructors and classes over names a-f, loads, "
       "stores, defineProperty, then %HeapObjectVerify(o)",
       0.4, kRecursive, GenMapTransitionFocus},
      {"JsonRoundTrip",
       "JSON.stringify(v), sometimes one character changed, guarded "
       "JSON.parse of the texts",
       0.4, kNoRequirement, GenJsonRoundTrip},
  };
}

}  // namespace redline

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/mutators/explore.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include "experimental/redline/base/json_parser.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/generators/codegen.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

// ---------------------------------------------------------------------------
// The runtime side. Everything the helper needs from the builtins is
// captured up front because the program under test may have replaced them.
// Actions are reported as "EXPLORE_ACTION: <json>" lines on the fuzzout
// channel (fuzzilli('FUZZILLI_PRINT', ...)); failures as "EXPLORE_FAILURE:
// <id>". Argument descriptors: {"a": i} the i-th probe argument, {"n": "7"}
// a number, {"s": "x"} a string, {"bi": "7"} a bigint, {"b": true},
// {"u": 1} undefined, {"f": 1} a fresh identity-like callback.
const char kExploreRuntime[] = R"js(const __redline_explore = (function() {
  const ObjectGetOwnPropertyNames = Object.getOwnPropertyNames;
  const ObjectGetPrototypeOf = Object.getPrototypeOf;
  const ObjectPrototype = Object.prototype;
  const FunctionPrototype = Function.prototype;
  const ReflectApply = Reflect.apply;
  const ReflectConstruct = Reflect.construct;
  const ArrayPush = Array.prototype.push;
  const JSONStringify = JSON.stringify;
  const NumberIsInteger = Number.isInteger;
  const NumberIsFinite = Number.isFinite;
  const NumberIsNaN = Number.isNaN;
  const MathFloor = Math.floor;
  const SymbolIterator = Symbol.iterator;
  const StringCharCodeAt = String.prototype.charCodeAt;
  const emit = typeof fuzzilli === 'function' ? function(s) { fuzzilli('FUZZILLI_PRINT', s); }
                                              : function(s) {};
  function push(a, v) { ReflectApply(ArrayPush, a, [v]); }

  let rngState = 1;
  function reseed(s) { rngState = (s >>> 0) || 1; }
  function random() {
    let x = rngState;
    x ^= x << 13; x >>>= 0;
    x ^= x >>> 17;
    x ^= x << 5; x >>>= 0;
    rngState = x;
    return x / 4294967296;
  }
  function randomInt(n) { return MathFloor(random() * n); }
  function pick(a) { return a[randomInt(a.length)]; }
  function chance(p) { return random() < p; }
  function hashString(s) {
    let h = 2166136261;
    for (let i = 0; i < s.length; i++) {
      h ^= ReflectApply(StringCharCodeAt, s, [i]);
      h = (h * 16777619) >>> 0;
    }
    return h;
  }

  const results = {__proto__: null};
  let exploring = false;
  let args = [];
  let canAwait = false;
  let mutable = false;

  const INTEGERS = [0, 1, -1, 2, 3, 7, 8, 16, 31, 32, 64, 100, 127, 128, 255, 256, 1000,
                    1023, 1024, 4096, 65535, 65536, 1073741823, 2147483647, 2147483648,
                    4294967295, 4294967296, 9007199254740991, -2147483648];
  const NUMBERS = [0.5, -0.0, 1.5, 1e6, 1e-6, 3.14, 1e21, 1e300, NaN, Infinity, -Infinity];
  const STRINGS = ['', 'a', 'abc', 'length', '0', '1', 'x', 'constructor', 'valueOf', 'toString',
                   'undefined', '\u0000', 'ab\u{1F600}'];
  const PROPERTY_NAMES = ['a', 'b', 'c', 'x', 'y', 'foo', 'bar', 'length', 'name', 'value',
                          'valueOf', 'toString', 'then', 'next', 'done', '0', '1', '2', '100',
                          '-1', '4294967295', 'size', 'key'];
  const CALLBACK_METHODS = ['map', 'filter', 'forEach', 'reduce', 'reduceRight', 'sort', 'find',
                            'findIndex', 'findLast', 'findLastIndex', 'some', 'every', 'flatMap',
                            'then', 'catch', 'finally', 'replace', 'replaceAll', 'toSorted',
                            'groupBy', 'from', 'register', 'addEventListener', 'take', 'drop'];
  const ARITHMETIC = ['+', '-', '*', '/', '%', '**'];
  const BITWISE = ['|', '&', '^', '<<', '>>', '>>>'];
  const COMPARISONS = ['==', '===', '!=', '!==', '<', '<=', '>', '>='];
  const UNARY = ['-', '+', '!', '~', 'typeof', 'void'];

  function isObject(v) { return typeof v === 'object' && v !== null; }
  function isFunction(v) { return typeof v === 'function'; }
  function includes(a, v) { for (let i = 0; i < a.length; i++) if (a[i] === v) return true; return false; }

  // --- inputs ---------------------------------------------------------------
  function argInput(i) { return {a: i}; }
  function randomArgument() {
    if (args.length === 0) return {u: 1};
    return argInput(randomInt(args.length));
  }
  function findArgument(predicate) {
    for (let i = 0; i < args.length; i++) {
      let ok = false;
      try { ok = predicate(args[i]); } catch (e) {}
      if (ok) return argInput(i);
    }
    return null;
  }
  function randomNumberInput() {
    return findArgument(x => typeof x === 'number') ||
           (chance(0.7) ? {n: '' + pick(INTEGERS)} : {n: '' + pick(NUMBERS)});
  }
  function randomIntegerInput(lo, hi) {
    return findArgument(x => NumberIsInteger(x) && x >= lo && x < hi) ||
           {n: '' + (lo + randomInt(hi - lo))};
  }
  function randomBigintInput() {
    return findArgument(x => typeof x === 'bigint') || {bi: '' + pick(INTEGERS)};
  }
  function randomStringInput() {
    return findArgument(x => typeof x === 'string') || {s: pick(STRINGS)};
  }
  function anyInput() {
    const r = random();
    if (r < 0.6 && args.length > 0) return randomArgument();
    if (r < 0.75) return {n: '' + pick(INTEGERS)};
    if (r < 0.85) return {s: pick(STRINGS)};
    if (r < 0.9) return {b: chance(0.5)};
    if (r < 0.95) return {f: 1};
    return {u: 1};
  }
  function argumentsFor(name, count) {
    const inputs = [];
    const wantsCallback = includes(CALLBACK_METHODS, name);
    for (let i = 0; i < count; i++) {
      if (i === 0 && wantsCallback && chance(0.8)) push(inputs, {f: 1});
      else push(inputs, anyInput());
    }
    return inputs;
  }
  function valueOf(input) {
    if ('a' in input) return args[input.a];
    if ('n' in input) return +input.n;
    if ('s' in input) return input.s;
    if ('bi' in input) return BigInt(input.n === undefined ? input.bi : input.n);
    if ('b' in input) return input.b;
    if ('f' in input) return function(a, b) { return a; };
    return undefined;
  }
  function valuesOf(inputs) {
    const out = [];
    for (let i = 0; i < inputs.length; i++) push(out, valueOf(inputs[i]));
    return out;
  }

  // --- introspection -------------------------------------------------------
  function propertyNames(o, skipOwn) {
    const names = [];
    const seen = {__proto__: null};
    let depth = 0;
    let p = o;
    // Large arrays/typed arrays: enumerating every index is slow and
    // pointless; start at the prototype.
    if (skipOwn) { try { p = ObjectGetPrototypeOf(o); } catch (e) { return names; } }
    for (; p !== null && p !== undefined && depth < 6; depth++) {
      if ((p === ObjectPrototype || p === FunctionPrototype) && !chance(0.15)) break;
      let own;
      try { own = ObjectGetOwnPropertyNames(p); } catch (e) { break; }
      for (let i = 0; i < own.length; i++) {
        const n = own[i];
        if (n === '__proto__' || n === 'caller' || n === 'callee' || n === 'arguments') continue;
        if (n === 'constructor' && !chance(0.1)) continue;
        if (seen[n]) continue;
        seen[n] = true;
        push(names, n);
      }
      try { p = ObjectGetPrototypeOf(p); } catch (e) { break; }
    }
    return names;
  }
  function looksLikeConstructor(f) {
    let name;
    try { name = f.name; } catch (e) { return false; }
    if (typeof name !== 'string' || name.length === 0) return chance(0.1);
    const c = ReflectApply(StringCharCodeAt, name, [0]);
    return (c >= 65 && c <= 90) ? chance(0.9) : chance(0.1);
  }
  function arity(f) {
    let n;
    try { n = f.length; } catch (e) { return 0; }
    if (!NumberIsInteger(n) || n < 0 || n > 8) return 1;
    return n;
  }

  // --- action selection ----------------------------------------------------
  function exploreObject(o) {
    const r = random();
    let length = -1;
    try { if (NumberIsInteger(o.length) && o.length >= 0) length = o.length; } catch (e) {}
    if (length > 0 && r < 0.2) {
      const index = randomIntegerInput(0, length + 1);
      return chance(0.5) ? {op: 'getElem', index: index}
                         : {op: 'setElem', index: index, args: [anyInput()]};
    }
    let iterable = false;
    try { iterable = isFunction(o[SymbolIterator]); } catch (e) {}
    if (iterable && r < 0.3) return chance(0.7) ? {op: 'iterate'} : {op: 'keys'};
    let thenable = false;
    try { thenable = isFunction(o.then); } catch (e) {}
    if (thenable && canAwait && r < 0.5) return {op: 'await'};
    const names = propertyNames(o, length > 200);
    if (names.length === 0 || r > 0.9) {
      return {op: 'setProp', name: pick(PROPERTY_NAMES), args: [anyInput()]};
    }
    const name = pick(names);
    let value;
    try { value = o[name]; } catch (e) { return {op: 'getProp', name: name}; }
    if (isFunction(value)) {
      const inputs = argumentsFor(name, arity(value));
      const construct = looksLikeConstructor(value);
      return {op: construct ? 'constructMethod' : 'callMethod', name: name, args: inputs};
    }
    const r2 = random();
    if (r2 < 0.55) return {op: 'getProp', name: name};
    if (r2 < 0.85) return {op: 'setProp', name: name, args: [anyInput()]};
    return {op: 'setProp', name: pick(PROPERTY_NAMES), args: [anyInput()]};
  }
  function exploreFunction(f) {
    if (chance(0.4)) return exploreObject(f);
    const inputs = argumentsFor(f.name, arity(f));
    return {op: looksLikeConstructor(f) ? 'construct' : 'call', args: inputs};
  }
  function exploreNumber(n) {
    const r = random();
    if (r < 0.15) return exploreObject(new Number(n));
    if (r < 0.3) {
      if (NumberIsNaN(n) || !NumberIsFinite(n)) return {op: 'unary', operator: pick(UNARY)};
      return {op: 'compare', operator: pick(COMPARISONS), args: [randomNumberInput()]};
    }
    if (r < 0.45) return {op: 'unary', operator: pick(UNARY)};
    if (r < 0.55 && mutable) return {op: 'update', operator: chance(0.5) ? '++' : '--'};
    if (r < 0.75) return {op: 'binary', operator: pick(BITWISE), args: [randomIntegerInput(0, 64)]};
    return {op: 'binary', operator: pick(ARITHMETIC), args: [randomNumberInput()]};
  }
  function exploreBigint(b) {
    const r = random();
    if (r < 0.15) return exploreObject(Object(b));
    if (r < 0.35) return {op: 'compare', operator: pick(COMPARISONS), args: [randomBigintInput()]};
    if (r < 0.5) return {op: 'unary', operator: chance(0.5) ? '-' : '~'};
    const ops = ['+', '-', '*', '/', '%', '**', '|', '&', '^', '<<', '>>'];
    return {op: 'binary', operator: pick(ops), args: [randomBigintInput()]};
  }
  function exploreString(s) {
    const r = random();
    if (r < 0.1) return {op: 'compare', operator: pick(['==', '===', '<', '>']), args: [randomStringInput()]};
    if (r < 0.2) return {op: 'binary', operator: '+', args: [anyInput()]};
    if (r < 0.3 && s.length > 0) return {op: 'getElem', index: randomIntegerInput(0, s.length)};
    return exploreObject(new String(s));
  }
  function exploreBoolean(b) {
    const r = random();
    if (r < 0.3) return {op: 'unary', operator: '!'};
    return {op: 'logical', operator: pick(['&&', '||']), args: [randomArgument()]};
  }
  function exploreNullish(v) {
    return chance(0.7) ? {op: 'logical', operator: '??', args: [randomArgument()]}
                       : {op: 'compare', operator: pick(['==', '===']), args: [anyInput()]};
  }
  function exploreSymbol(s) {
    return chance(0.5) ? {op: 'getProp', name: 'description'}
                       : {op: 'callMethod', name: 'toString', args: []};
  }
  function chooseAction(v) {
    if (v === null || v === undefined) return exploreNullish(v);
    switch (typeof v) {
      case 'object': return exploreObject(v);
      case 'function': return exploreFunction(v);
      case 'number': return exploreNumber(v);
      case 'bigint': return exploreBigint(v);
      case 'string': return exploreString(v);
      case 'boolean': return exploreBoolean(v);
      case 'symbol': return exploreSymbol(v);
      default: return exploreObject(Object(v));
    }
  }

  // --- action execution (validation) ---------------------------------------
  function perform(action, v) {
    const a = action.args ? valuesOf(action.args) : [];
    switch (action.op) {
      case 'callMethod': return ReflectApply(v[action.name], v, a);
      case 'constructMethod': return ReflectConstruct(v[action.name], a);
      case 'getProp': return v[action.name];
      case 'setProp': v[action.name] = a[0]; return;
      case 'getElem': return v[valueOf(action.index)];
      case 'setElem': v[valueOf(action.index)] = a[0]; return;
      case 'call': return ReflectApply(v, undefined, a);
      case 'construct': return ReflectConstruct(v, a);
      case 'binary':
        switch (action.operator) {
          case '+': return v + a[0]; case '-': return v - a[0]; case '*': return v * a[0];
          case '/': return v / a[0]; case '%': return v % a[0]; case '**': return v ** a[0];
          case '|': return v | a[0]; case '&': return v & a[0]; case '^': return v ^ a[0];
          case '<<': return v << a[0]; case '>>': return v >> a[0]; case '>>>': return v >>> a[0];
        }
        return;
      case 'compare':
        switch (action.operator) {
          case '==': return v == a[0]; case '===': return v === a[0];
          case '!=': return v != a[0]; case '!==': return v !== a[0];
          case '<': return v < a[0]; case '<=': return v <= a[0];
          case '>': return v > a[0]; case '>=': return v >= a[0];
        }
        return;
      case 'unary':
        switch (action.operator) {
          case '-': return -v; case '+': return +v; case '!': return !v; case '~': return ~v;
          case 'typeof': return typeof v; case 'void': return void v;
        }
        return;
      case 'update': return action.operator === '++' ? v + 1 : v - 1;
      case 'logical':
        switch (action.operator) {
          case '&&': return v && a[0]; case '||': return v || a[0]; case '??': return v ?? a[0];
        }
        return;
      case 'iterate': { let n = 0; for (const x of v) { if (++n > 100) break; } return; }
      case 'keys': { let n = 0; for (const k in v) { if (++n > 100) break; } return; }
      case 'await': return;
    }
  }

  function explore(id, v, probeArgs, flags, seed) {
    if (exploring) return;
    exploring = true;
    try {
      args = probeArgs;
      canAwait = (flags & 1) !== 0;
      mutable = (flags & 2) !== 0;
      let action = results[id];
      if (action === undefined) {
        reseed(hashString(id) ^ seed);
        for (let attempt = 0; attempt < 4 && action === undefined; attempt++) {
          const candidate = chooseAction(v);
          let ok = true;
          try { perform(candidate, v); } catch (e) { ok = false; }
          if (ok) action = candidate;
        }
        if (action === undefined) {
          results[id] = null;
          emit('EXPLORE_FAILURE: ' + id);
        } else {
          action.id = id;
          results[id] = action;
          emit('EXPLORE_ACTION: ' + JSONStringify(action));
        }
      } else if (action !== null) {
        try { perform(action, v); } catch (e) {}
      }
    } catch (e) {
      try { emit('EXPLORE_ERROR: ' + id + ' ' + e); } catch (e2) {}
    }
    exploring = false;
  }
  return explore;
})();
try { %NeverOptimizeFunction(__redline_explore); } catch (e) {}
)js";

namespace {

constexpr int kMaxProbes = 3;

bool IsBoring(const Variable& v) {
  // Exploring `arguments`/`undefined`-like names makes little sense.
  return v.name == "arguments" || v.name == "undefined" || v.name == "NaN" ||
         v.name == "Infinity";
}

// Prefers variables whose static type is unknown or object-like: those are
// the ones where runtime inspection adds the most information.
double ExploreWeight(const Variable& v) {
  switch (v.type) {
    case JsType::kUnknown:
    case JsType::kObject:
    case JsType::kClass:
    case JsType::kFunction:
      return 4;
    case JsType::kArray:
    case JsType::kTypedArray:
    case JsType::kMap:
    case JsType::kSet:
    case JsType::kPromise:
    case JsType::kRegExp:
    case JsType::kArrayBuffer:
    case JsType::kDate:
    case JsType::kError:
      return 2;
    default:
      return 1;
  }
}

Expression* LiteralFor(Tree* t, const ExploreArg& arg) {
  switch (arg.kind) {
    case ExploreArg::Kind::kVariable:
      return mk::Id(t, arg.text);
    case ExploreArg::Kind::kNumber:
      return mk::Num(t, strtod(arg.text.c_str(), nullptr));
    case ExploreArg::Kind::kString:
      return mk::Str(t, arg.text);
    case ExploreArg::Kind::kBoolean:
      return mk::Bool(t, arg.flag);
    case ExploreArg::Kind::kBigInt:
      return mk::BigInt(t, arg.text);
    case ExploreArg::Kind::kNull:
      return mk::Null(t);
    case ExploreArg::Kind::kUndefined:
      return mk::Undefined(t);
  }
  return mk::Undefined(t);
}

// The statement `try { __redline_explore("id", v, [args], flags, seed); } catch
// (e) {}`.
Statement* MakeExploreProbe(Tree* t, const ExploreProbe& probe,
                            bool mutable_variable, uint32_t seed) {
  std::vector<Expression*> args;
  args.reserve(probe.args.size());
  for (const ExploreArg& arg : probe.args) {
    args.push_back(LiteralFor(t, arg));
  }
  int flags = (probe.can_await ? 1 : 0) | (mutable_variable ? 2 : 0);
  return RuntimeAssistedMutator::MakeProbe(
      t, ExploreMutator::kProbeFunction, probe,
      {mk::Array(t, std::move(args)), mk::Num(t, flags),
       mk::Num(t, static_cast<double>(seed))});
}

// --- applying actions -------------------------------------------------------

struct Action {
  std::string id;
  std::string op;
  std::string name;
  std::string op_text;  // operator
  JsonValue index;
  std::vector<JsonValue> args;
};

bool ParseAction(const JsonValue& json, Action* action) {
  if (!json.is_object()) {
    return false;
  }
  action->id = json.GetString("id", "");
  action->op = json.GetString("op", "");
  action->name = json.GetString("name", "");
  action->op_text = json.GetString("operator", "");
  action->index = json.Get("index");
  const JsonValue& args = json.Get("args");
  if (args.is_array()) {
    for (const JsonValue& a : args.as_array()) {
      action->args.push_back(a);
    }
  }
  return !action->id.empty() && !action->op.empty();
}

// Maps an argument descriptor to an expression; nullptr if malformed.
Expression* ExpressionFor(Builder& b, const ExploreProbe& probe,
                          const JsonValue& input) {
  Tree* t = b.tree();
  if (!input.is_object()) {
    return nullptr;
  }
  if (input.Has("a")) {
    int64_t i = input.GetInt("a", -1);
    if (i < 0 || static_cast<size_t>(i) >= probe.args.size()) {
      return nullptr;
    }
    return LiteralFor(t, probe.args[static_cast<size_t>(i)]);
  }
  if (input.Has("n")) {
    return mk::Num(t, strtod(input.GetString("n", "0").c_str(), nullptr));
  }
  if (input.Has("s")) {
    return mk::Str(t, input.GetString("s", ""));
  }
  if (input.Has("bi")) {
    return mk::BigInt(t, input.GetString("bi", "0"));
  }
  if (input.Has("b")) {
    return mk::Bool(t, input.GetBool("b", false));
  }
  if (input.Has("f")) {
    std::string_view p0 = b.FreshName("p");
    std::string_view p1 = b.FreshName("p");
    return mk::ArrowExpr(t, {p0, p1}, mk::Id(t, p0));
  }
  return mk::Undefined(t);
}

template <typename Op>
bool OpFromText(const std::string& text, Op* out, Op first, Op last) {
  for (int i = static_cast<int>(first); i <= static_cast<int>(last); i++) {
    Op op = static_cast<Op>(i);
    if (text == OperatorText(op)) {
      *out = op;
      return true;
    }
  }
  return false;
}

Expression* Access(Tree* t, Expression* object, const std::string& name) {
  if (IsIdentifierName(name)) {
    return mk::Member(t, object, name);
  }
  return mk::Index(t, object, mk::Str(t, name));
}

// Builds the statements for `action` on `probe.variable`; empty if the
// action cannot be expressed (malformed or not applicable).
std::vector<Statement*> StatementsFor(Builder& b, const ExploreProbe& probe,
                                      const Action& action,
                                      std::string* describe) {
  Tree* t = b.tree();
  std::vector<Statement*> out;
  auto var = [&] { return mk::Id(t, probe.variable); };
  std::vector<Expression*> args;
  for (const JsonValue& a : action.args) {
    Expression* e = ExpressionFor(b, probe, a);
    if (e == nullptr) {
      return out;
    }
    args.push_back(e);
  }
  auto result = [&](Expression* value) {
    out.push_back(mk::Let(t, b.FreshName("r"), value));
  };
  const std::string& op = action.op;
  *describe = probe.variable + " " + op;
  if (op == "callMethod" || op == "constructMethod") {
    if (action.name.empty()) {
      return out;
    }
    *describe += " " + action.name;
    Expression* callee = Access(t, var(), action.name);
    result(op == "callMethod"
               ? static_cast<Expression*>(mk::Call(t, callee, std::move(args)))
               : static_cast<Expression*>(mk::New(t, callee, std::move(args))));
  } else if (op == "getProp") {
    if (action.name.empty()) {
      return out;
    }
    *describe += " " + action.name;
    result(Access(t, var(), action.name));
  } else if (op == "setProp") {
    if (action.name.empty() || args.size() != 1) {
      return out;
    }
    *describe += " " + action.name;
    out.push_back(
        mk::ExprStmt(t, mk::Assign(t, Access(t, var(), action.name), args[0])));
  } else if (op == "getElem" || op == "setElem") {
    Expression* index = ExpressionFor(b, probe, action.index);
    if (index == nullptr) {
      return out;
    }
    if (op == "getElem") {
      result(mk::Index(t, var(), index));
    } else {
      if (args.size() != 1) {
        return out;
      }
      out.push_back(
          mk::ExprStmt(t, mk::Assign(t, mk::Index(t, var(), index), args[0])));
    }
  } else if (op == "call") {
    result(mk::Call(t, var(), std::move(args)));
  } else if (op == "construct") {
    result(mk::New(t, var(), std::move(args)));
  } else if (op == "binary" || op == "compare") {
    BinaryOp bop;
    if (args.size() != 1 || !OpFromText(action.op_text, &bop, BinaryOp::kEq,
                                        BinaryOp::kInstanceof)) {
      return out;
    }
    *describe += " " + action.op_text;
    result(mk::Binary(t, bop, var(), args[0]));
  } else if (op == "logical") {
    LogicalOp lop;
    if (args.size() != 1 || !OpFromText(action.op_text, &lop, LogicalOp::kOr,
                                        LogicalOp::kNullish)) {
      return out;
    }
    *describe += " " + action.op_text;
    result(mk::Logical(t, lop, var(), args[0]));
  } else if (op == "unary") {
    UnaryOp uop;
    if (!OpFromText(action.op_text, &uop, UnaryOp::kMinus, UnaryOp::kDelete)) {
      return out;
    }
    if (uop == UnaryOp::kDelete) {
      return out;
    }
    *describe += " " + action.op_text;
    result(mk::Unary(t, uop, var()));
  } else if (op == "update") {
    UpdateOp uop =
        action.op_text == "--" ? UpdateOp::kDecrement : UpdateOp::kIncrement;
    out.push_back(mk::ExprStmt(t, mk::Update(t, uop, /*prefix=*/false, var())));
  } else if (op == "iterate") {
    std::string_view counter = b.FreshName("r");
    std::string_view element = b.FreshName("e");
    out.push_back(mk::Let(t, counter, mk::Num(t, 0)));
    Statement* guard =
        mk::If(t,
               mk::Binary(t, BinaryOp::kGreaterThan,
                          mk::Update(t, UpdateOp::kIncrement, /*prefix=*/true,
                                     mk::Id(t, counter)),
                          mk::Num(t, 100)),
               mk::Break(t));
    out.push_back(mk::ForOf(t, VariableKind::kConst, element, var(),
                            mk::Block(t, {guard})));
  } else if (op == "keys") {
    std::string_view keys = b.FreshName("r");
    std::string_view key = b.FreshName("k");
    out.push_back(mk::Let(t, keys, mk::Array(t, {})));
    Statement* body = mk::ExprStmt(
        t, mk::CallMethod(t, mk::Id(t, keys), "push", {mk::Id(t, key)}));
    out.push_back(
        mk::ForIn(t, VariableKind::kConst, key, var(), mk::Block(t, {body})));
  } else if (op == "await") {
    if (!probe.can_await) {
      return out;
    }
    result(mk::Await(t, var()));
  } else {
    return out;
  }
  return out;
}

}  // namespace

// static
Statement* RuntimeAssistedMutator::MakeProbe(
    Tree* t, std::string_view function, const ExploreProbe& probe,
    const std::vector<Expression*>& extra) {
  std::vector<Expression*> args = {mk::Str(t, probe.id),
                                   mk::Id(t, probe.variable)};
  for (Expression* e : extra) {
    args.push_back(e);
  }
  CallExpression* call = mk::Call(t, mk::Id(t, function), std::move(args));
  return mk::Try(t, {mk::ExprStmt(t, call)}, "e", {});
}

// static
Statement* RuntimeAssistedMutator::FindProbe(Builder& b,
                                             std::string_view function,
                                             const std::string& id) {
  std::vector<Node*> calls = b.Collect([&](Node* n) {
    auto* call = n->As<CallExpression>();
    if (call == nullptr || call->arguments.empty()) {
      return false;
    }
    auto* callee = call->callee->As<Identifier>();
    if (callee == nullptr || callee->name != function) {
      return false;
    }
    auto* literal = call->arguments[0]->As<StringLiteral>();
    return literal != nullptr && literal->value == id;
  });
  if (calls.empty()) {
    return nullptr;
  }
  Statement* statement = b.EnclosingStatement(calls[0]);
  if (statement == nullptr) {
    return nullptr;
  }
  Node* block = b.ParentOf(statement);
  Node* try_statement = block != nullptr ? b.ParentOf(block) : nullptr;
  if (try_statement != nullptr && try_statement->Is<TryStatement>()) {
    return try_statement->Cast<TryStatement>();
  }
  return statement;
}

ExploreMutator::ExploreMutator()
    : RuntimeAssistedMutator("Explore",
                             "inspects variables at runtime and performs an "
                             "action fitting their actual value",
                             3, false) {}

std::string_view ExploreMutator::runtime() const { return kExploreRuntime; }

int ExploreMutator::Apply(Builder& builder,
                          const std::vector<ExploreProbe>& probes,
                          const std::string& fuzzout, MutationContext& /*ctx*/,
                          std::string* detail) {
  return ApplyExploreActions(builder, probes, fuzzout, detail);
}

bool ExploreMutator::Mutate(Builder& b, MutationContext& ctx) {
  Random& r = b.rng();
  ctx.probes.clear();
  std::vector<Builder::Slot> slots = b.InsertionSlots();
  if (slots.empty()) {
    return false;
  }
  int wanted = 1 + r.SmallCount(kMaxProbes - 1);
  const uint32_t seed = static_cast<uint32_t>(r.Next());
  std::vector<std::pair<Builder::Slot, Statement*>> insertions;
  std::vector<std::string> explored;
  for (int attempt = 0;
       attempt < 4 * wanted && static_cast<int>(ctx.probes.size()) < wanted;
       attempt++) {
    const Builder::Slot& slot = slots[r.Index(slots.size())];
    Builder::Context syntax = b.ContextAtSlot(slot);
    if (syntax.in_class_field) {
      continue;
    }
    std::vector<Variable> visible = b.VisibleAtSlot(slot);
    std::vector<const Variable*> candidates;
    std::vector<double> weights;
    for (const Variable& v : visible) {
      if (IsBoring(v)) {
        continue;
      }
      if (std::find(explored.begin(), explored.end(), std::string(v.name)) !=
          explored.end()) {
        continue;
      }
      candidates.push_back(&v);
      weights.push_back(ExploreWeight(v));
    }
    if (candidates.empty()) {
      continue;
    }
    const Variable& target = *candidates[r.Weighted(weights)];
    ExploreProbe probe;
    probe.id = StringPrintf("e%u", static_cast<unsigned>(r.Next() % 100000));
    probe.variable = std::string(target.name);
    probe.can_await = syntax.in_async && !syntax.in_static_block;
    // Up to 4 other visible variables plus a literal or two as arguments.
    std::vector<const Variable*> others;
    for (const Variable& v : visible) {
      if (v.name != target.name && !IsBoring(v)) {
        others.push_back(&v);
      }
    }
    for (int i = 0; i < 4 && !others.empty(); i++) {
      size_t index = r.Index(others.size());
      ExploreArg arg;
      arg.kind = ExploreArg::Kind::kVariable;
      arg.text = std::string(others[index]->name);
      probe.args.push_back(arg);
      others.erase(others.begin() + static_cast<std::ptrdiff_t>(index));
    }
    {
      ExploreArg arg;
      arg.kind = ExploreArg::Kind::kNumber;
      const std::vector<double>& numbers = ctx.env->numbers();
      double n = numbers[r.Index(numbers.size())];
      arg.text = StringPrintf("%.17g", n);
      if (arg.text.find("nan") != std::string::npos ||
          arg.text.find("inf") != std::string::npos) {
        arg.text = "0";
      }
      probe.args.push_back(arg);
    }
    if (r.Coin()) {
      ExploreArg arg;
      arg.kind = ExploreArg::Kind::kString;
      const std::vector<std::string_view>& strings = ctx.env->strings();
      arg.text = std::string(strings[r.Index(strings.size())]);
      probe.args.push_back(arg);
    }
    Statement* statement =
        MakeExploreProbe(b.tree(), probe, !target.is_constant, seed);
    insertions.emplace_back(slot, statement);
    explored.push_back(probe.variable);
    ctx.probes.push_back(std::move(probe));
  }
  if (ctx.probes.empty()) {
    return false;
  }
  // Insert back to front within each list so that earlier slot indices stay
  // valid; different lists do not interfere.
  std::stable_sort(insertions.begin(), insertions.end(),
                   [](const auto& x, const auto& y) {
                     return x.first.index > y.first.index;
                   });
  for (auto& [slot, statement] : insertions) {
    b.Insert(slot, statement);
  }
  ctx.detail = "probing";
  for (const ExploreProbe& p : ctx.probes) {
    ctx.detail += " " + p.variable;
  }
  return true;
}

int ApplyExploreActions(Builder& b, const std::vector<ExploreProbe>& probes,
                        const std::string& fuzzout, std::string* detail) {
  std::unordered_map<std::string, Action> actions;
  size_t start = 0;
  while (start < fuzzout.size()) {
    size_t nl = fuzzout.find('\n', start);
    if (nl == std::string::npos) {
      nl = fuzzout.size();
    }
    std::string_view line(fuzzout.data() + start, nl - start);
    start = nl + 1;
    constexpr std::string_view kAction = "EXPLORE_ACTION: ";
    if (!line.starts_with(kAction)) {
      continue;
    }
    JsonValue json;
    std::string error;
    if (!JsonValue::Parse(line.substr(kAction.size()), &json, &error)) {
      REDLINE_VLOG("explore", "unparsable action: %s", error.c_str());
      continue;
    }
    Action action;
    if (ParseAction(json, &action)) {
      actions[action.id] = std::move(action);
    }
  }
  int applied = 0;
  std::string description;
  for (const ExploreProbe& probe : probes) {
    Statement* statement = RuntimeAssistedMutator::FindProbe(
        b, ExploreMutator::kProbeFunction, probe.id);
    if (statement == nullptr) {
      continue;
    }
    Builder::Slot slot = b.SlotOf(statement);
    if (!slot.valid()) {
      continue;
    }
    std::vector<Statement*> replacement;
    auto it = actions.find(probe.id);
    if (it != actions.end()) {
      std::string describe;
      replacement = StatementsFor(b, probe, it->second, &describe);
      if (!replacement.empty()) {
        applied++;
        if (!description.empty()) {
          description += "; ";
        }
        description += describe;
      }
    }
    b.Remove(slot);
    if (!replacement.empty()) {
      b.InsertAll(slot, replacement);
    }
  }
  if (detail != nullptr) {
    *detail = "explored " + description;
  }
  return applied;
}

}  // namespace redline

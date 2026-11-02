// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/environment.h"

#include <cmath>
#include <limits>

#include "experimental/redline/base/json_parser.h"

namespace redline {

namespace {

using T = JsType;

BuiltinFunction F(std::string_view name, int min_args, int max_args,
                  JsType result, JsType a0 = T::kUnknown,
                  JsType a1 = T::kUnknown, JsType a2 = T::kUnknown) {
  BuiltinFunction f{name, min_args, max_args, result};
  f.args[0] = a0;
  f.args[1] = a1;
  f.args[2] = a2;
  return f;
}

}  // namespace

// static
const Environment& Environment::Default() {
  static const Environment* instance = new Environment();
  return *instance;
}

Environment::Environment() {
  // --- values by type -------------------------------------------------------
  types_ = {
      {T::kArray,
       {F("push", 0, 3, T::kNumber),
        F("pop", 0, 0, T::kUnknown),
        F("shift", 0, 0, T::kUnknown),
        F("unshift", 0, 3, T::kNumber),
        F("slice", 0, 2, T::kArray, T::kNumber, T::kNumber),
        F("splice", 1, 3, T::kArray, T::kNumber, T::kNumber),
        F("concat", 0, 2, T::kArray),
        F("join", 0, 1, T::kString, T::kString),
        F("reverse", 0, 0, T::kArray),
        F("sort", 0, 1, T::kArray, T::kFunction),
        F("indexOf", 1, 2, T::kNumber),
        F("includes", 1, 2, T::kBoolean),
        F("map", 1, 1, T::kArray, T::kFunction),
        F("filter", 1, 1, T::kArray, T::kFunction),
        F("forEach", 1, 1, T::kNullish, T::kFunction),
        F("reduce", 1, 2, T::kUnknown, T::kFunction),
        F("some", 1, 1, T::kBoolean, T::kFunction),
        F("every", 1, 1, T::kBoolean, T::kFunction),
        F("find", 1, 1, T::kUnknown, T::kFunction),
        F("findIndex", 1, 1, T::kNumber, T::kFunction),
        F("findLast", 1, 1, T::kUnknown, T::kFunction),
        F("fill", 1, 3, T::kArray, T::kUnknown, T::kNumber, T::kNumber),
        F("flat", 0, 1, T::kArray, T::kNumber),
        F("flatMap", 1, 1, T::kArray, T::kFunction),
        F("at", 1, 1, T::kUnknown, T::kNumber),
        F("keys", 0, 0, T::kObject),
        F("values", 0, 0, T::kObject),
        F("entries", 0, 0, T::kObject),
        F("copyWithin", 2, 3, T::kArray, T::kNumber, T::kNumber),
        F("lastIndexOf", 1, 1, T::kNumber),
        F("toString", 0, 0, T::kString),
        F("toSorted", 0, 1, T::kArray, T::kFunction),
        F("toReversed", 0, 0, T::kArray),
        F("with", 2, 2, T::kArray, T::kNumber)},
       {"length", "0", "1", "constructor"}},
      {T::kString,
       {F("charAt", 1, 1, T::kString, T::kNumber),
        F("charCodeAt", 1, 1, T::kNumber, T::kNumber),
        F("codePointAt", 1, 1, T::kNumber, T::kNumber),
        F("concat", 1, 2, T::kString, T::kString),
        F("indexOf", 1, 2, T::kNumber, T::kString),
        F("includes", 1, 1, T::kBoolean, T::kString),
        F("startsWith", 1, 1, T::kBoolean, T::kString),
        F("endsWith", 1, 1, T::kBoolean, T::kString),
        F("slice", 1, 2, T::kString, T::kNumber, T::kNumber),
        F("substring", 1, 2, T::kString, T::kNumber, T::kNumber),
        F("substr", 1, 2, T::kString, T::kNumber, T::kNumber),
        F("split", 0, 2, T::kArray, T::kString, T::kNumber),
        F("replace", 2, 2, T::kString, T::kUnknown, T::kUnknown),
        F("replaceAll", 2, 2, T::kString, T::kString, T::kString),
        F("repeat", 1, 1, T::kString, T::kNumber),
        F("padStart", 1, 2, T::kString, T::kNumber, T::kString),
        F("padEnd", 1, 2, T::kString, T::kNumber, T::kString),
        F("trim", 0, 0, T::kString),
        F("toUpperCase", 0, 0, T::kString),
        F("toLowerCase", 0, 0, T::kString),
        F("at", 1, 1, T::kString, T::kNumber),
        F("match", 1, 1, T::kUnknown, T::kRegExp),
        F("matchAll", 1, 1, T::kObject, T::kRegExp),
        F("search", 1, 1, T::kNumber, T::kRegExp),
        F("localeCompare", 1, 1, T::kNumber, T::kString),
        F("normalize", 0, 1, T::kString),
        F("valueOf", 0, 0, T::kString),
        F("toString", 0, 0, T::kString),
        F("isWellFormed", 0, 0, T::kBoolean)},
       {"length", "0", "constructor"}},
      {T::kNumber,
       {F("toString", 0, 1, T::kString, T::kNumber),
        F("toFixed", 0, 1, T::kString, T::kNumber),
        F("toPrecision", 0, 1, T::kString, T::kNumber),
        F("toExponential", 0, 1, T::kString, T::kNumber),
        F("valueOf", 0, 0, T::kNumber)},
       {}},
      {T::kBigInt,
       {F("toString", 0, 1, T::kString, T::kNumber),
        F("valueOf", 0, 0, T::kBigInt)},
       {}},
      {T::kObject,
       {F("hasOwnProperty", 1, 1, T::kBoolean, T::kString),
        F("toString", 0, 0, T::kString), F("valueOf", 0, 0, T::kUnknown),
        F("isPrototypeOf", 1, 1, T::kBoolean),
        F("propertyIsEnumerable", 1, 1, T::kBoolean, T::kString)},
       {"a", "b", "c", "x", "y", "length", "constructor", "__proto__",
        "valueOf", "toString", "0", "1", "name"}},
      {T::kFunction,
       {F("call", 0, 3, T::kUnknown),
        F("apply", 0, 2, T::kUnknown, T::kUnknown, T::kArray),
        F("bind", 0, 2, T::kFunction), F("toString", 0, 0, T::kString)},
       {"length", "name", "prototype"}},
      {T::kClass,
       {F("toString", 0, 0, T::kString)},
       {"prototype", "name", "length"}},
      {T::kRegExp,
       {F("test", 1, 1, T::kBoolean, T::kString),
        F("exec", 1, 1, T::kUnknown, T::kString),
        F("toString", 0, 0, T::kString)},
       {"lastIndex", "source", "flags", "global", "sticky"}},
      {T::kMap,
       {F("get", 1, 1, T::kUnknown), F("set", 2, 2, T::kMap),
        F("has", 1, 1, T::kBoolean), F("delete", 1, 1, T::kBoolean),
        F("clear", 0, 0, T::kNullish), F("keys", 0, 0, T::kObject),
        F("values", 0, 0, T::kObject), F("entries", 0, 0, T::kObject),
        F("forEach", 1, 1, T::kNullish, T::kFunction)},
       {"size"}},
      {T::kSet,
       {F("add", 1, 1, T::kSet), F("has", 1, 1, T::kBoolean),
        F("delete", 1, 1, T::kBoolean), F("clear", 0, 0, T::kNullish),
        F("values", 0, 0, T::kObject),
        F("forEach", 1, 1, T::kNullish, T::kFunction),
        F("union", 1, 1, T::kSet, T::kSet),
        F("intersection", 1, 1, T::kSet, T::kSet)},
       {"size"}},
      {T::kPromise,
       {F("then", 1, 2, T::kPromise, T::kFunction, T::kFunction),
        F("catch", 1, 1, T::kPromise, T::kFunction),
        F("finally", 1, 1, T::kPromise, T::kFunction)},
       {}},
      {T::kTypedArray,
       {F("fill", 1, 3, T::kTypedArray, T::kNumber, T::kNumber, T::kNumber),
        F("set", 1, 2, T::kNullish, T::kArray, T::kNumber),
        F("subarray", 0, 2, T::kTypedArray, T::kNumber, T::kNumber),
        F("slice", 0, 2, T::kTypedArray, T::kNumber, T::kNumber),
        F("map", 1, 1, T::kTypedArray, T::kFunction),
        F("sort", 0, 1, T::kTypedArray),
        F("indexOf", 1, 1, T::kNumber, T::kNumber), F("join", 0, 1, T::kString),
        F("reverse", 0, 0, T::kTypedArray),
        F("at", 1, 1, T::kNumber, T::kNumber),
        F("copyWithin", 2, 3, T::kTypedArray, T::kNumber, T::kNumber)},
       {"length", "byteLength", "byteOffset", "buffer", "0", "1"}},
      {T::kArrayBuffer,
       {F("slice", 0, 2, T::kArrayBuffer, T::kNumber, T::kNumber),
        F("resize", 1, 1, T::kNullish, T::kNumber),
        F("transfer", 0, 1, T::kArrayBuffer, T::kNumber),
        F("grow", 1, 1, T::kNullish, T::kNumber)},
       {"byteLength", "maxByteLength", "resizable", "detached", "growable"}},
      {T::kDate,
       {F("getTime", 0, 0, T::kNumber), F("getFullYear", 0, 0, T::kNumber),
        F("toISOString", 0, 0, T::kString),
        F("setTime", 1, 1, T::kNumber, T::kNumber),
        F("valueOf", 0, 0, T::kNumber)},
       {}},
      {T::kError,
       {F("toString", 0, 0, T::kString)},
       {"message", "name", "stack"}},
      {T::kSymbol, {F("toString", 0, 0, T::kString)}, {"description"}},
  };

  // --- globals --------------------------------------------------------------
  globals_ = {
      {"Object",
       T::kClass,
       true,
       T::kObject,
       0,
       1,
       {F("keys", 1, 1, T::kArray, T::kObject),
        F("values", 1, 1, T::kArray, T::kObject),
        F("entries", 1, 1, T::kArray, T::kObject),
        F("assign", 1, 3, T::kObject, T::kObject, T::kObject),
        F("freeze", 1, 1, T::kObject, T::kObject),
        F("seal", 1, 1, T::kObject, T::kObject),
        F("preventExtensions", 1, 1, T::kObject, T::kObject),
        F("isFrozen", 1, 1, T::kBoolean),
        F("create", 1, 2, T::kObject, T::kObject),
        F("getPrototypeOf", 1, 1, T::kObject, T::kObject),
        F("setPrototypeOf", 2, 2, T::kObject, T::kObject, T::kObject),
        F("defineProperty", 3, 3, T::kObject, T::kObject, T::kString,
          T::kObject),
        F("getOwnPropertyNames", 1, 1, T::kArray, T::kObject),
        F("getOwnPropertyDescriptor", 2, 2, T::kObject, T::kObject, T::kString),
        F("fromEntries", 1, 1, T::kObject, T::kArray),
        F("groupBy", 2, 2, T::kObject, T::kArray, T::kFunction),
        F("is", 2, 2, T::kBoolean),
        F("hasOwn", 2, 2, T::kBoolean, T::kObject, T::kString)},
       {"prototype"}},
      {"Array",
       T::kClass,
       true,
       T::kArray,
       0,
       3,
       {F("from", 1, 2, T::kArray, T::kUnknown, T::kFunction),
        F("of", 0, 3, T::kArray), F("isArray", 1, 1, T::kBoolean)},
       {"prototype"}},
      {"Function", T::kClass, true, T::kFunction, 0, 2, {}, {"prototype"}},
      {"String",
       T::kClass,
       false,
       T::kString,
       0,
       1,
       {F("fromCharCode", 0, 3, T::kString, T::kNumber, T::kNumber, T::kNumber),
        F("fromCodePoint", 0, 2, T::kString, T::kNumber, T::kNumber),
        F("raw", 1, 1, T::kString, T::kObject)},
       {"prototype"}},
      {"Number",
       T::kClass,
       false,
       T::kNumber,
       0,
       1,
       {F("isInteger", 1, 1, T::kBoolean),
        F("isSafeInteger", 1, 1, T::kBoolean), F("isNaN", 1, 1, T::kBoolean),
        F("isFinite", 1, 1, T::kBoolean),
        F("parseFloat", 1, 1, T::kNumber, T::kString),
        F("parseInt", 1, 2, T::kNumber, T::kString, T::kNumber)},
       {"MAX_SAFE_INTEGER", "MIN_SAFE_INTEGER", "MAX_VALUE", "MIN_VALUE",
        "EPSILON", "NaN", "POSITIVE_INFINITY", "NEGATIVE_INFINITY"}},
      {"Boolean", T::kClass, false, T::kBoolean, 0, 1, {}, {}},
      {"Symbol",
       T::kFunction,
       false,
       T::kSymbol,
       0,
       1,
       {F("for", 1, 1, T::kSymbol, T::kString)},
       {"iterator", "asyncIterator", "hasInstance", "toPrimitive",
        "toStringTag", "species", "isConcatSpreadable", "match", "replace",
        "split", "unscopables", "dispose"}},
      {"BigInt",
       T::kFunction,
       false,
       T::kBigInt,
       1,
       1,
       {F("asIntN", 2, 2, T::kBigInt, T::kNumber, T::kBigInt),
        F("asUintN", 2, 2, T::kBigInt, T::kNumber, T::kBigInt)},
       {}},
      {"Math",
       T::kObject,
       false,
       T::kObject,
       0,
       0,
       {F("abs", 1, 1, T::kNumber, T::kNumber),
        F("floor", 1, 1, T::kNumber, T::kNumber),
        F("ceil", 1, 1, T::kNumber, T::kNumber),
        F("round", 1, 1, T::kNumber, T::kNumber),
        F("trunc", 1, 1, T::kNumber, T::kNumber),
        F("sign", 1, 1, T::kNumber, T::kNumber),
        F("sqrt", 1, 1, T::kNumber, T::kNumber),
        F("cbrt", 1, 1, T::kNumber, T::kNumber),
        F("pow", 2, 2, T::kNumber, T::kNumber, T::kNumber),
        F("max", 0, 3, T::kNumber, T::kNumber, T::kNumber, T::kNumber),
        F("min", 0, 3, T::kNumber, T::kNumber, T::kNumber, T::kNumber),
        F("random", 0, 0, T::kNumber),
        F("sin", 1, 1, T::kNumber, T::kNumber),
        F("cos", 1, 1, T::kNumber, T::kNumber),
        F("atan2", 2, 2, T::kNumber, T::kNumber, T::kNumber),
        F("log", 1, 1, T::kNumber, T::kNumber),
        F("exp", 1, 1, T::kNumber, T::kNumber),
        F("imul", 2, 2, T::kNumber, T::kNumber, T::kNumber),
        F("clz32", 1, 1, T::kNumber, T::kNumber),
        F("fround", 1, 1, T::kNumber, T::kNumber),
        F("hypot", 0, 3, T::kNumber, T::kNumber, T::kNumber, T::kNumber),
        F("f16round", 1, 1, T::kNumber, T::kNumber)},
       {"PI", "E", "LN2", "SQRT2"}},
      {"JSON",
       T::kObject,
       false,
       T::kObject,
       0,
       0,
       {F("stringify", 1, 3, T::kString),
        F("parse", 1, 2, T::kUnknown, T::kString, T::kFunction),
        F("rawJSON", 1, 1, T::kObject, T::kString)},
       {}},
      {"Reflect",
       T::kObject,
       false,
       T::kObject,
       0,
       0,
       {F("get", 2, 3, T::kUnknown, T::kObject, T::kString),
        F("set", 3, 3, T::kBoolean, T::kObject, T::kString),
        F("has", 2, 2, T::kBoolean, T::kObject, T::kString),
        F("ownKeys", 1, 1, T::kArray, T::kObject),
        F("apply", 3, 3, T::kUnknown, T::kFunction, T::kUnknown, T::kArray),
        F("construct", 2, 3, T::kObject, T::kClass, T::kArray),
        F("defineProperty", 3, 3, T::kBoolean, T::kObject, T::kString,
          T::kObject),
        F("deleteProperty", 2, 2, T::kBoolean, T::kObject, T::kString),
        F("getPrototypeOf", 1, 1, T::kObject, T::kObject),
        F("setPrototypeOf", 2, 2, T::kBoolean, T::kObject, T::kObject)},
       {}},
      {"Map",
       T::kClass,
       true,
       T::kMap,
       0,
       1,
       {F("groupBy", 2, 2, T::kMap, T::kArray, T::kFunction)},
       {}},
      {"Set", T::kClass, true, T::kSet, 0, 1, {}, {}},
      {"WeakMap", T::kClass, true, T::kMap, 0, 1, {}, {}},
      {"WeakSet", T::kClass, true, T::kSet, 0, 1, {}, {}},
      {"WeakRef", T::kClass, true, T::kObject, 1, 1, {}, {}},
      {"FinalizationRegistry", T::kClass, true, T::kObject, 1, 1, {}, {}},
      {"Promise",
       T::kClass,
       true,
       T::kPromise,
       1,
       1,
       {F("resolve", 0, 1, T::kPromise), F("reject", 0, 1, T::kPromise),
        F("all", 1, 1, T::kPromise, T::kArray),
        F("allSettled", 1, 1, T::kPromise, T::kArray),
        F("race", 1, 1, T::kPromise, T::kArray),
        F("any", 1, 1, T::kPromise, T::kArray),
        F("withResolvers", 0, 0, T::kObject),
        F("try", 1, 2, T::kPromise, T::kFunction)},
       {}},
      {"RegExp", T::kClass, true, T::kRegExp, 1, 2, {}, {}},
      {"Date",
       T::kClass,
       true,
       T::kDate,
       0,
       1,
       {F("now", 0, 0, T::kNumber)},
       {}},
      {"Error",
       T::kClass,
       true,
       T::kError,
       0,
       2,
       {F("captureStackTrace", 1, 1, T::kNullish, T::kObject)},
       {}},
      {"TypeError", T::kClass, true, T::kError, 0, 1, {}, {}},
      {"RangeError", T::kClass, true, T::kError, 0, 1, {}, {}},
      {"ArrayBuffer",
       T::kClass,
       true,
       T::kArrayBuffer,
       1,
       2,
       {F("isView", 1, 1, T::kBoolean)},
       {}},
      {"SharedArrayBuffer", T::kClass, true, T::kArrayBuffer, 1, 2, {}, {}},
      {"DataView", T::kClass, true, T::kObject, 1, 3, {}, {}},
      {"Uint8Array",
       T::kClass,
       true,
       T::kTypedArray,
       0,
       3,
       {F("from", 1, 1, T::kTypedArray, T::kArray),
        F("of", 0, 3, T::kTypedArray, T::kNumber, T::kNumber, T::kNumber)},
       {"BYTES_PER_ELEMENT"}},
      {"Int8Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Uint8ClampedArray", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Int16Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Uint16Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Int32Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Uint32Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Float32Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Float64Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"BigInt64Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"BigUint64Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Float16Array", T::kClass, true, T::kTypedArray, 0, 3, {}, {}},
      {"Proxy",
       T::kClass,
       true,
       T::kObject,
       2,
       2,
       {F("revocable", 2, 2, T::kObject, T::kObject, T::kObject)},
       {}},
      {"Atomics",
       T::kObject,
       false,
       T::kObject,
       0,
       0,
       {F("add", 3, 3, T::kNumber, T::kTypedArray, T::kNumber, T::kNumber),
        F("load", 2, 2, T::kNumber, T::kTypedArray, T::kNumber),
        F("store", 3, 3, T::kNumber, T::kTypedArray, T::kNumber, T::kNumber),
        F("compareExchange", 4, 4, T::kNumber, T::kTypedArray, T::kNumber,
          T::kNumber)},
       {}},
      {"Iterator",
       T::kClass,
       false,
       T::kObject,
       0,
       0,
       {F("from", 1, 1, T::kObject)},
       {"prototype"}},
      {"globalThis", T::kObject, false, T::kObject, 0, 0, {}, {}},
      {"parseInt", T::kFunction, false, T::kNumber, 1, 2, {}, {}},
      {"parseFloat", T::kFunction, false, T::kNumber, 1, 1, {}, {}},
      {"isNaN", T::kFunction, false, T::kBoolean, 1, 1, {}, {}},
      {"isFinite", T::kFunction, false, T::kBoolean, 1, 1, {}, {}},
      {"encodeURIComponent", T::kFunction, false, T::kString, 1, 1, {}, {}},
      {"decodeURIComponent", T::kFunction, false, T::kString, 1, 1, {}, {}},
      {"escape", T::kFunction, false, T::kString, 1, 1, {}, {}},
      {"gc", T::kFunction, false, T::kNullish, 0, 1, {}, {}},
  };

  // --- natives (all safe with --fuzzing)
  // --------------------------------------
  natives_ = {
      {"PrepareFunctionForOptimization", 1, T::kFunction},
      {"OptimizeFunctionOnNextCall", 1, T::kFunction},
      {"OptimizeMaglevOnNextCall", 1, T::kFunction},
      {"CompileBaseline", 1, T::kFunction},
      {"DeoptimizeFunction", 1, T::kFunction},
      {"DeoptimizeNow", 0, T::kUnknown},
      {"NeverOptimizeFunction", 1, T::kFunction},
      {"OptimizeOsr", 0, T::kUnknown},
      {"BaselineOsr", 0, T::kUnknown},
      {"SimulateNewspaceFull", 0, T::kUnknown},
      {"ArrayBufferDetach", 1, T::kArrayBuffer},
      {"HaveSameMap", 2, T::kObject},
      {"HasFastProperties", 1, T::kObject},
      {"HasSmiElements", 1, T::kArray},
      {"HasDoubleElements", 1, T::kArray},
      {"HasHoleyElements", 1, T::kArray},
      {"HasDictionaryElements", 1, T::kArray},
      {"NormalizeElements", 1, T::kArray},
      {"ToFastProperties", 1, T::kObject},
      {"InternalizeString", 1, T::kString},
      {"ConstructConsString", 2, T::kString},
      {"ConstructSlicedString", 2, T::kString},
      {"FlattenString", 1, T::kString},
      {"IsBeingInterpreted", 0, T::kUnknown},
      {"PretenureAllocationSite", 1, T::kObject},
      {"SetAllocationTimeout", 2, T::kNumber},
  };

  property_names_ = {"a",
                     "b",
                     "c",
                     "d",
                     "x",
                     "y",
                     "z",
                     "foo",
                     "bar",
                     "baz",
                     "length",
                     "name",
                     "value",
                     "valueOf",
                     "toString",
                     "constructor",
                     "prototype",
                     "__proto__",
                     "get",
                     "set",
                     "then",
                     "next",
                     "done",
                     "0",
                     "1",
                     "2",
                     "100",
                     "4294967295",
                     "-1",
                     "1.5",
                     "key",
                     "size",
                     "callee",
                     "caller",
                     "arguments",
                     "Symbol.iterator",
                     "Symbol.toPrimitive",
                     "Symbol.hasInstance",
                     "Symbol.species"};

  numbers_ = {0,
              -0.0,
              1,
              -1,
              2,
              3,
              4,
              7,
              8,
              10,
              16,
              31,
              32,
              42,
              63,
              64,
              100,
              127,
              128,
              255,
              256,
              1000,
              1023,
              1024,
              4095,
              4096,
              65535,
              65536,
              1e5,
              1e6,
              16777215,
              16777216,
              2147483647,
              2147483648,
              -2147483648,
              -2147483649,
              4294967295,
              4294967296,
              1e10,
              9007199254740991,
              9007199254740992,
              -9007199254740991,
              1e21,
              1e300,
              1.7976931348623157e308,
              5e-324,
              2.2250738585072014e-308,
              0.1,
              0.5,
              1.5,
              -1.5,
              0.3,
              1 / 3.0,
              std::numeric_limits<double>::quiet_NaN(),
              std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()};

  strings_ = {"",
              "a",
              "ab",
              "abc",
              "foo",
              "bar",
              "0",
              "1",
              "-1",
              "1.5",
              "NaN",
              "undefined",
              "null",
              "true",
              "length",
              "constructor",
              "__proto__",
              "valueOf",
              "toString",
              "prototype",
              "hello world",
              "\xC3\xA9\xC3\xA8\xC3\xA0",
              "\xE2\x82\xAC",
              "\xF0\x9F\x98\x80",
              "\\u0000",
              " ",
              "\t\n",
              "x",
              "\x7F",
              "\\n"};

  regexps_ = {{"a", ""},    {"(a|b)*", "g"},  {"\\d+", "u"},
              {"^$", "m"},  {"[a-z]+", "gi"}, {"(?<x>a)\\k<x>", ""},
              {".", "s"},   {"a*?", "y"},     {"\\p{L}", "v"},
              {"(?:)", "d"}};
}  // NOLINT(readability/fn_size)

const BuiltinType* Environment::ForType(JsType type) const {
  for (const BuiltinType& t : types_) {
    if (t.type == type) {
      return &t;
    }
  }
  return nullptr;
}

const BuiltinGlobal* Environment::Global(std::string_view name) const {
  for (const BuiltinGlobal& g : globals_) {
    if (g.name == name) {
      return &g;
    }
  }
  return nullptr;
}

std::vector<const BuiltinGlobal*> Environment::ConstructorsOf(
    JsType type) const {
  std::vector<const BuiltinGlobal*> result;
  for (const BuiltinGlobal& g : globals_) {
    if (g.constructible && g.instance_type == type) {
      result.push_back(&g);
    }
  }
  return result;
}

size_t Environment::discovered_member_count() const {
  size_t n = 0;
  for (const DiscoveredApi& api : discovered_) {
    n += api.statics.size() + api.methods.size() + api.getters.size();
  }
  return n;
}

std::string_view Environment::Intern(std::string_view text) {
  owned_strings_.emplace_back(text);
  return owned_strings_.back();
}

// static
std::string_view Environment::IntrospectionScript() {
  // Printed as one JSON line: [{"p": "Intl.NumberFormat", "k": "C", "a": 0,
  // "s": [["supportedLocalesOf", 1]], "m": [["format", 1], ...], "g":
  // ["<getter name>", ...]}, ...]. Kinds: F function, C constructor, N
  // namespace object. Shell I/O, process control, threads and WebAssembly
  // are left out on purpose.
  static constexpr char kScript[] = R"js((function() {
  const out = [];
  const seen = new Set();
  const deny = new Set(['print', 'printErr', 'write', 'quit', 'load', 'read', 'readline',
    'readbuffer', 'os', 'version', 'Worker', 'fuzzilli', 'testRunner',
    'globalThis', 'setTimeout', 'queueMicrotask', 'arguments', '__redline_explore',
    'd8.file', 'd8.log', 'd8.terminate', 'd8.quit', 'd8.debugger', 'd8.profiler', 'd8.dom', 'd8.wasm',
    'Realm.dispose', 'Realm.navigate', 'Realm.detachGlobal', 'performance.measureMemory',
    'Atomics.wait', 'Atomics.waitAsync', 'Atomics.pause']);
  function isConstructor(f) {
    try { Reflect.construct(function() {}, [], f); return true; } catch (e) { return false; }
  }
  function members(o, skip) {
    const statics = [], getters = [];
    let names;
    try { names = Object.getOwnPropertyNames(o); } catch (e) { return [statics, getters]; }
    for (const n of names) {
      if (skip.has(n) || n === 'constructor' || n === 'prototype' || n === '__proto__' ||
          n === 'caller' || n === 'callee' || n === 'arguments' || n === 'length' ||
          n === 'name') continue;
      let d;
      try { d = Object.getOwnPropertyDescriptor(o, n); } catch (e) { continue; }
      if (!d) continue;
      if (typeof d.get === 'function') { getters.push(n); continue; }
      if (typeof d.value === 'function') {
        let a = d.value.length;
        statics.push([n, (typeof a === 'number' && a >= 0 && a < 10) ? a : 1]);
      }
    }
    return [statics, getters];
  }
  function describe(path, v, depth) {
    if (seen.has(v)) return;
    const t = typeof v;
    if (t === 'function') {
      seen.add(v);
      const ctor = isConstructor(v);
      const [statics] = members(v, new Set());
      let methods = [], getters = [];
      if (ctor) {
        let proto;
        try { proto = v.prototype; } catch (e) {}
        if (proto && typeof proto === 'object') {
          [methods, getters] = members(proto, new Set());
          // Subclasses (typed arrays, errors) inherit their methods from an
          // intermediate prototype (%TypedArray%.prototype, Error.prototype).
          let parent = null;
          try { parent = Object.getPrototypeOf(proto); } catch (e) {}
          if (parent && parent !== Object.prototype && parent !== Function.prototype) {
            const [pm, pg] = members(parent, new Set());
            methods = methods.concat(pm);
            getters = getters.concat(pg);
          }
        }
      }
      out.push({p: path, k: ctor ? 'C' : 'F', a: v.length | 0, s: statics, m: methods, g: getters});
      if (depth < 1) nested(path, v, depth);
    } else if (t === 'object' && v !== null) {
      seen.add(v);
      const [statics, getters] = members(v, new Set());
      out.push({p: path, k: 'N', a: 0, s: statics, m: [], g: getters});
      if (depth < 2) nested(path, v, depth);
    }
  }
  // A nested function is worth an entry of its own only if it is a class
  // (uppercase name or a prototype with members); plain functions are
  // already listed as statics of their namespace.
  function isClassLike(f, name) {
    if (!isConstructor(f)) return false;
    const c = name.charCodeAt(0);
    if (c >= 65 && c <= 90) return true;
    try { return Object.getOwnPropertyNames(f.prototype).length > 1; } catch (e) { return false; }
  }
  function nested(path, v, depth) {
    let names;
    try { names = Object.getOwnPropertyNames(v); } catch (e) { return; }
    for (const n of names) {
      if (n === 'prototype' || n === 'constructor' || n === '__proto__') continue;
      const p = path + '.' + n;
      if (deny.has(p)) continue;
      let d;
      try { d = Object.getOwnPropertyDescriptor(v, n); } catch (e) { continue; }
      if (!d || !('value' in d)) continue;
      const w = d.value;
      if (typeof w === 'object' && w !== null && !Array.isArray(w)) describe(p, w, depth + 1);
      else if (typeof w === 'function' && isClassLike(w, n)) describe(p, w, depth + 1);
    }
  }
  const globals = Object.getOwnPropertyNames(globalThis);
  for (const n of globals) {
    if (deny.has(n)) continue;
    let d;
    try { d = Object.getOwnPropertyDescriptor(globalThis, n); } catch (e) { continue; }
    if (!d || !('value' in d)) continue;
    describe(n, d.value, 0);
  }
  fuzzilli('FUZZILLI_PRINT', JSON.stringify(out));
})();
)js";
  return kScript;
}

// static
std::unique_ptr<Environment> Environment::WithDiscovered(
    const std::string& json, std::string* error) {
  std::unique_ptr<Environment> env(new Environment());
  auto read_members = [&env](const JsonValue& array,
                             std::vector<DiscoveredMember>* out) {
    if (!array.is_array()) {
      return;
    }
    for (const JsonValue& item : array.as_array()) {
      if (item.is_string()) {
        out->push_back({env->Intern(item.as_string()), -1});
      } else if (item.is_array() && item.as_array().size() == 2 &&
                 item.as_array()[0].is_string()) {
        const JsonValue& arity = item.as_array()[1];
        out->push_back(
            {env->Intern(item.as_array()[0].as_string()),
             arity.is_number() ? static_cast<int>(arity.as_number()) : 1});
      }
    }
  };
  JsonValue root;
  // The target may have printed other things first; take the last line
  // that looks like a JSON array.
  size_t start = json.rfind("\n[");
  std::string text = start == std::string::npos ? json : json.substr(start + 1);
  if (!JsonValue::Parse(text, &root, error)) {
    return nullptr;
  }
  if (!root.is_array()) {
    *error = "introspection output is not a JSON array";
    return nullptr;
  }
  for (const JsonValue& item : root.as_array()) {
    if (!item.is_object()) {
      continue;
    }
    DiscoveredApi api;
    api.path = env->Intern(item.GetString("p", ""));
    if (api.path.empty()) {
      continue;
    }
    std::string kind = item.GetString("k", "N");
    api.kind = kind == "C"   ? DiscoveredApi::Kind::kConstructor
               : kind == "F" ? DiscoveredApi::Kind::kFunction
                             : DiscoveredApi::Kind::kNamespace;
    api.arity = static_cast<int>(item.GetInt("a", 0));
    if (api.arity < 0 || api.arity > 8) {
      api.arity = 1;
    }
    read_members(item.Get("s"), &api.statics);
    read_members(item.Get("m"), &api.methods);
    read_members(item.Get("g"), &api.getters);
    // Namespaces without anything callable are useless to the generators.
    if (api.kind == DiscoveredApi::Kind::kNamespace && api.statics.empty() &&
        api.getters.empty()) {
      continue;
    }
    env->discovered_.push_back(std::move(api));
  }
  if (env->discovered_.empty()) {
    *error = "introspection found no usable globals";
    return nullptr;
  }
  return env;
}

}  // namespace redline

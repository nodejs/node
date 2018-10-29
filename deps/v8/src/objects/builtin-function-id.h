// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BUILTIN_FUNCTION_ID_H_
#define V8_OBJECTS_BUILTIN_FUNCTION_ID_H_

#include <stdint.h>

namespace v8 {
namespace internal {

// List of builtin functions we want to identify to improve code
// generation.
//
// Each entry has a name of a global object property holding an object
// optionally followed by ".prototype", a name of a builtin function
// on the object (the one the id is set for), and a label.
//
// Installation of ids for the selected builtin functions is handled
// by the bootstrapper.
#define FUNCTIONS_WITH_ID_LIST(V)                           \
  V(Array, isArray, ArrayIsArray)                           \
  V(Array.prototype, concat, ArrayConcat)                   \
  V(Array.prototype, every, ArrayEvery)                     \
  V(Array.prototype, fill, ArrayFill)                       \
  V(Array.prototype, filter, ArrayFilter)                   \
  V(Array.prototype, findIndex, ArrayFindIndex)             \
  V(Array.prototype, forEach, ArrayForEach)                 \
  V(Array.prototype, includes, ArrayIncludes)               \
  V(Array.prototype, indexOf, ArrayIndexOf)                 \
  V(Array.prototype, join, ArrayJoin)                       \
  V(Array.prototype, lastIndexOf, ArrayLastIndexOf)         \
  V(Array.prototype, map, ArrayMap)                         \
  V(Array.prototype, pop, ArrayPop)                         \
  V(Array.prototype, push, ArrayPush)                       \
  V(Array.prototype, reverse, ArrayReverse)                 \
  V(Array.prototype, shift, ArrayShift)                     \
  V(Array.prototype, slice, ArraySlice)                     \
  V(Array.prototype, some, ArraySome)                       \
  V(Array.prototype, splice, ArraySplice)                   \
  V(Array.prototype, unshift, ArrayUnshift)                 \
  V(Date, now, DateNow)                                     \
  V(Date.prototype, getDate, DateGetDate)                   \
  V(Date.prototype, getDay, DateGetDay)                     \
  V(Date.prototype, getFullYear, DateGetFullYear)           \
  V(Date.prototype, getHours, DateGetHours)                 \
  V(Date.prototype, getMilliseconds, DateGetMilliseconds)   \
  V(Date.prototype, getMinutes, DateGetMinutes)             \
  V(Date.prototype, getMonth, DateGetMonth)                 \
  V(Date.prototype, getSeconds, DateGetSeconds)             \
  V(Date.prototype, getTime, DateGetTime)                   \
  V(Function.prototype, apply, FunctionApply)               \
  V(Function.prototype, bind, FunctionBind)                 \
  V(Function.prototype, call, FunctionCall)                 \
  V(Object, assign, ObjectAssign)                           \
  V(Object, create, ObjectCreate)                           \
  V(Object, is, ObjectIs)                                   \
  V(Object.prototype, hasOwnProperty, ObjectHasOwnProperty) \
  V(Object.prototype, isPrototypeOf, ObjectIsPrototypeOf)   \
  V(Object.prototype, toString, ObjectToString)             \
  V(RegExp.prototype, compile, RegExpCompile)               \
  V(RegExp.prototype, exec, RegExpExec)                     \
  V(RegExp.prototype, test, RegExpTest)                     \
  V(RegExp.prototype, toString, RegExpToString)             \
  V(String.prototype, charCodeAt, StringCharCodeAt)         \
  V(String.prototype, charAt, StringCharAt)                 \
  V(String.prototype, codePointAt, StringCodePointAt)       \
  V(String.prototype, concat, StringConcat)                 \
  V(String.prototype, endsWith, StringEndsWith)             \
  V(String.prototype, includes, StringIncludes)             \
  V(String.prototype, indexOf, StringIndexOf)               \
  V(String.prototype, lastIndexOf, StringLastIndexOf)       \
  V(String.prototype, repeat, StringRepeat)                 \
  V(String.prototype, slice, StringSlice)                   \
  V(String.prototype, startsWith, StringStartsWith)         \
  V(String.prototype, substr, StringSubstr)                 \
  V(String.prototype, substring, StringSubstring)           \
  V(String.prototype, toLowerCase, StringToLowerCase)       \
  V(String.prototype, toString, StringToString)             \
  V(String.prototype, toUpperCase, StringToUpperCase)       \
  V(String.prototype, trim, StringTrim)                     \
  V(String.prototype, trimLeft, StringTrimStart)            \
  V(String.prototype, trimRight, StringTrimEnd)             \
  V(String.prototype, valueOf, StringValueOf)               \
  V(String, fromCharCode, StringFromCharCode)               \
  V(String, fromCodePoint, StringFromCodePoint)             \
  V(String, raw, StringRaw)                                 \
  V(Math, random, MathRandom)                               \
  V(Math, floor, MathFloor)                                 \
  V(Math, round, MathRound)                                 \
  V(Math, ceil, MathCeil)                                   \
  V(Math, abs, MathAbs)                                     \
  V(Math, log, MathLog)                                     \
  V(Math, log1p, MathLog1p)                                 \
  V(Math, log2, MathLog2)                                   \
  V(Math, log10, MathLog10)                                 \
  V(Math, cbrt, MathCbrt)                                   \
  V(Math, exp, MathExp)                                     \
  V(Math, expm1, MathExpm1)                                 \
  V(Math, sqrt, MathSqrt)                                   \
  V(Math, pow, MathPow)                                     \
  V(Math, max, MathMax)                                     \
  V(Math, min, MathMin)                                     \
  V(Math, cos, MathCos)                                     \
  V(Math, cosh, MathCosh)                                   \
  V(Math, sign, MathSign)                                   \
  V(Math, sin, MathSin)                                     \
  V(Math, sinh, MathSinh)                                   \
  V(Math, tan, MathTan)                                     \
  V(Math, tanh, MathTanh)                                   \
  V(Math, acos, MathAcos)                                   \
  V(Math, acosh, MathAcosh)                                 \
  V(Math, asin, MathAsin)                                   \
  V(Math, asinh, MathAsinh)                                 \
  V(Math, atan, MathAtan)                                   \
  V(Math, atan2, MathAtan2)                                 \
  V(Math, atanh, MathAtanh)                                 \
  V(Math, imul, MathImul)                                   \
  V(Math, clz32, MathClz32)                                 \
  V(Math, fround, MathFround)                               \
  V(Math, trunc, MathTrunc)                                 \
  V(Number, isFinite, NumberIsFinite)                       \
  V(Number, isInteger, NumberIsInteger)                     \
  V(Number, isNaN, NumberIsNaN)                             \
  V(Number, isSafeInteger, NumberIsSafeInteger)             \
  V(Number, parseFloat, NumberParseFloat)                   \
  V(Number, parseInt, NumberParseInt)                       \
  V(Number.prototype, toString, NumberToString)             \
  V(Map.prototype, clear, MapClear)                         \
  V(Map.prototype, delete, MapDelete)                       \
  V(Map.prototype, entries, MapEntries)                     \
  V(Map.prototype, forEach, MapForEach)                     \
  V(Map.prototype, has, MapHas)                             \
  V(Map.prototype, keys, MapKeys)                           \
  V(Map.prototype, get, MapGet)                             \
  V(Map.prototype, set, MapSet)                             \
  V(Map.prototype, values, MapValues)                       \
  V(Set.prototype, add, SetAdd)                             \
  V(Set.prototype, clear, SetClear)                         \
  V(Set.prototype, delete, SetDelete)                       \
  V(Set.prototype, entries, SetEntries)                     \
  V(Set.prototype, forEach, SetForEach)                     \
  V(Set.prototype, has, SetHas)                             \
  V(Set.prototype, values, SetValues)                       \
  V(WeakMap.prototype, delete, WeakMapDelete)               \
  V(WeakMap.prototype, has, WeakMapHas)                     \
  V(WeakMap.prototype, set, WeakMapSet)                     \
  V(WeakSet.prototype, add, WeakSetAdd)                     \
  V(WeakSet.prototype, delete, WeakSetDelete)               \
  V(WeakSet.prototype, has, WeakSetHas)

#define ATOMIC_FUNCTIONS_WITH_ID_LIST(V)              \
  V(Atomics, load, AtomicsLoad)                       \
  V(Atomics, store, AtomicsStore)                     \
  V(Atomics, exchange, AtomicsExchange)               \
  V(Atomics, compareExchange, AtomicsCompareExchange) \
  V(Atomics, add, AtomicsAdd)                         \
  V(Atomics, sub, AtomicsSub)                         \
  V(Atomics, and, AtomicsAnd)                         \
  V(Atomics, or, AtomicsOr)                           \
  V(Atomics, xor, AtomicsXor)

enum class BuiltinFunctionId : uint8_t {
  kArrayConstructor,
#define DECL_FUNCTION_ID(ignored1, ignore2, name) k##name,
  FUNCTIONS_WITH_ID_LIST(DECL_FUNCTION_ID)
      ATOMIC_FUNCTIONS_WITH_ID_LIST(DECL_FUNCTION_ID)
#undef DECL_FUNCTION_ID
  // These are manually assigned to special getters during bootstrapping.
  kArrayBufferByteLength,
  kArrayBufferIsView,
  kArrayEntries,
  kArrayKeys,
  kArrayValues,
  kArrayIteratorNext,
  kBigIntConstructor,
  kMapSize,
  kSetSize,
  kMapIteratorNext,
  kSetIteratorNext,
  kDataViewBuffer,
  kDataViewByteLength,
  kDataViewByteOffset,
  kFunctionHasInstance,
  kGlobalDecodeURI,
  kGlobalDecodeURIComponent,
  kGlobalEncodeURI,
  kGlobalEncodeURIComponent,
  kGlobalEscape,
  kGlobalUnescape,
  kGlobalIsFinite,
  kGlobalIsNaN,
  kNumberConstructor,
  kPromiseAll,
  kPromisePrototypeCatch,
  kPromisePrototypeFinally,
  kPromisePrototypeThen,
  kPromiseRace,
  kPromiseReject,
  kPromiseResolve,
  kSymbolConstructor,
  kSymbolPrototypeToString,
  kSymbolPrototypeValueOf,
  kTypedArrayByteLength,
  kTypedArrayByteOffset,
  kTypedArrayEntries,
  kTypedArrayKeys,
  kTypedArrayLength,
  kTypedArrayToStringTag,
  kTypedArrayValues,
  kSharedArrayBufferByteLength,
  kStringConstructor,
  kStringIterator,
  kStringIteratorNext,
  kStringToLowerCaseIntl,
  kStringToUpperCaseIntl,
  kInvalidBuiltinFunctionId = static_cast<uint8_t>(-1),
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_BUILTIN_FUNCTION_ID_H_

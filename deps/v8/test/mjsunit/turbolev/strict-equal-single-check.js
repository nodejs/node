// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

const objA = { id: 1 };
const objB = { id: 2 };

function strictEqReceiver(a, b) {
  return a === b;
}
%PrepareFunctionForOptimization(strictEqReceiver);
assertTrue(strictEqReceiver(objA, objA));
assertFalse(strictEqReceiver(objA, objB));
%OptimizeFunctionOnNextCall(strictEqReceiver);
assertTrue(strictEqReceiver(objA, objA));
assertFalse(strictEqReceiver(objA, objB));
assertFalse(strictEqReceiver(objA, null));
assertFalse(strictEqReceiver(objA, undefined));
assertFalse(strictEqReceiver(objA, 42));
assertFalse(strictEqReceiver(objA, "str"));
assertOptimized(strictEqReceiver);
// Passing a non-receiver as the checked first operand deopts, including NaN === NaN.
assertFalse(strictEqReceiver(NaN, NaN));
assertUnoptimized(strictEqReceiver);

// When the first operand is statically known NOT to be a JSReceiver (e.g. an
// inlined Smi argument `42`), the check is placed on the second operand instead
// of emitting an unconditional deopt on the first operand.
function compareHelper(doCompare, a, b) {
  if (!doCompare) return false;
  return a === b;
}
function callerWithKnownSmiLeft(doCompare, b) {
  return compareHelper(doCompare, 42, b);
}
%PrepareFunctionForOptimization(compareHelper);
%PrepareFunctionForOptimization(callerWithKnownSmiLeft);
assertTrue(compareHelper(true, objA, objA));
assertFalse(compareHelper(true, objA, objB));
assertFalse(callerWithKnownSmiLeft(false, objA));
%OptimizeFunctionOnNextCall(callerWithKnownSmiLeft);
assertFalse(callerWithKnownSmiLeft(true, objA));
assertOptimized(callerWithKnownSmiLeft);
assertTrue(callerWithKnownSmiLeft(true, 42));
assertUnoptimized(callerWithKnownSmiLeft);

// When either operand is already statically known to be a JSReceiver, zero
// checks are emitted, so comparing against any type stays optimized.
function strictEqFreshObjectLeft(b) {
  const fresh = { x: 1 };
  return fresh === b;
}
%PrepareFunctionForOptimization(strictEqFreshObjectLeft);
assertFalse(strictEqFreshObjectLeft(objA));
assertFalse(strictEqFreshObjectLeft(objB));
%OptimizeFunctionOnNextCall(strictEqFreshObjectLeft);
assertFalse(strictEqFreshObjectLeft(objA));
assertFalse(strictEqFreshObjectLeft(42));
assertFalse(strictEqFreshObjectLeft("str"));
assertFalse(strictEqFreshObjectLeft(null));
assertOptimized(strictEqFreshObjectLeft);

function strictEqFreshObjectRight(a) {
  const fresh = { x: 1 };
  return a === fresh;
}
%PrepareFunctionForOptimization(strictEqFreshObjectRight);
assertFalse(strictEqFreshObjectRight(objA));
assertFalse(strictEqFreshObjectRight(objB));
%OptimizeFunctionOnNextCall(strictEqFreshObjectRight);
assertFalse(strictEqFreshObjectRight(objA));
assertFalse(strictEqFreshObjectRight(42));
assertFalse(strictEqFreshObjectRight("str"));
assertFalse(strictEqFreshObjectRight(null));
assertOptimized(strictEqFreshObjectRight);

// Untagged Int32 and HoleyFloat64 operands on the unchecked side.
const holeyDoubles = [1.5, , 2.5];
function strictEqReceiverOrNull(a, b) {
  return a === b;
}
%PrepareFunctionForOptimization(strictEqReceiverOrNull);
assertTrue(strictEqReceiverOrNull(objA, objA));
assertFalse(strictEqReceiverOrNull(objA, null));
assertTrue(strictEqReceiverOrNull(null, null));
assertFalse(strictEqReceiverOrNull(null, undefined));
%OptimizeFunctionOnNextCall(strictEqReceiverOrNull);
assertTrue(strictEqReceiverOrNull(objA, objA));
assertFalse(strictEqReceiverOrNull(objA, null));
assertTrue(strictEqReceiverOrNull(null, null));
assertFalse(strictEqReceiverOrNull(null, undefined));
assertFalse(strictEqReceiverOrNull(objA, 42));
assertFalse(strictEqReceiverOrNull(null, "str"));
assertTrue(strictEqReceiverOrNull(undefined, holeyDoubles[1]));
assertFalse(strictEqReceiverOrNull(undefined, holeyDoubles[0]));
assertOptimized(strictEqReceiverOrNull);
assertTrue(strictEqReceiverOrNull(42, 42));
assertUnoptimized(strictEqReceiverOrNull);

function strictEqReceiverUntaggedInt32(a, x) {
  return compareHelper(true, a, x | 0);
}
%PrepareFunctionForOptimization(strictEqReceiverUntaggedInt32);
assertFalse(strictEqReceiverUntaggedInt32(objA, 7));
%OptimizeFunctionOnNextCall(strictEqReceiverUntaggedInt32);
assertFalse(strictEqReceiverUntaggedInt32(objA, 7));
assertOptimized(strictEqReceiverUntaggedInt32);

const sym1 = Symbol("a");
const sym2 = Symbol("a");
function strictEqSymbol(a, b) {
  return a === b;
}
%PrepareFunctionForOptimization(strictEqSymbol);
assertTrue(strictEqSymbol(sym1, sym1));
assertFalse(strictEqSymbol(sym1, sym2));
%OptimizeFunctionOnNextCall(strictEqSymbol);
assertTrue(strictEqSymbol(sym1, sym1));
assertFalse(strictEqSymbol(sym1, sym2));
assertFalse(strictEqSymbol(sym1, "a"));
assertFalse(strictEqSymbol(sym1, 42));
assertFalse(strictEqSymbol(sym1, Object(sym1)));
assertOptimized(strictEqSymbol);
assertTrue(strictEqSymbol("a", "a"));
assertUnoptimized(strictEqSymbol);

// Abstract equality (==) must still check both operands for Symbol and Receiver.
function abstractEqSymbol(a, b) {
  return a == b;
}
%PrepareFunctionForOptimization(abstractEqSymbol);
assertTrue(abstractEqSymbol(sym1, sym1));
assertFalse(abstractEqSymbol(sym1, sym2));
%OptimizeFunctionOnNextCall(abstractEqSymbol);
assertTrue(abstractEqSymbol(sym1, sym1));
assertOptimized(abstractEqSymbol);
assertTrue(abstractEqSymbol(sym1, Object(sym1)));
assertUnoptimized(abstractEqSymbol);

const objWithValueOf = { valueOf() { return 42; } };
function abstractEqReceiver(a, b) {
  return a == b;
}
%PrepareFunctionForOptimization(abstractEqReceiver);
assertTrue(abstractEqReceiver(objWithValueOf, objWithValueOf));
assertFalse(abstractEqReceiver(objWithValueOf, objA));
%OptimizeFunctionOnNextCall(abstractEqReceiver);
assertTrue(abstractEqReceiver(objWithValueOf, objWithValueOf));
assertOptimized(abstractEqReceiver);
assertTrue(abstractEqReceiver(objWithValueOf, 42));
assertUnoptimized(abstractEqReceiver);

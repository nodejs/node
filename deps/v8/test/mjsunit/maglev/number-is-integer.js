// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function checkTagged(x) {
  return Number.isInteger(x);
}

function checkFloat64(a, b) {
  return Number.isInteger(a + b);
}

function checkInt32(x) {
  return Number.isInteger(x | 0);
}

function checkUint32(x) {
  return Number.isInteger(x >>> 0);
}

function checkIntPtr(ta) {
  return Number.isInteger(ta.length);
}

function checkHoleyDouble(arr, i) {
  const v = arr[i];
  return [Number.isInteger(v), v];
}

function checkKnownSmi(arr, i) {
  return Number.isInteger(arr[i]);
}

function checkNonNumber(obj) {
  return Number.isInteger({a: obj});
}

function checkFloat64Alt(x) {
  return [x + 0.5, Number.isInteger(x)];
}

function checkInt32Alt(ta, idx) {
  // OOB-handling TypedArray keyed load calls GetInt32(idx, true), emitting
  // CheckedNumberToInt32(idx) and populating KNA's int32 alternative on a
  // Number-typed (non-Smi) tagged parameter without deopting on 0x7fffffff.
  return [ta[idx], Number.isInteger(idx)];
}

const numBox = { v: 1 };
numBox.v = 1.5;
function checkKnownNumber(box, useSmi) {
  const n = useSmi ? 7 : box.v;
  return Number.isInteger(n);
}

function checkNoArgs() {
  return Number.isInteger();
}

function getPredicate(useIsInteger) {
  let pad = 0;
  for (let i = 0; i < 4; ++i) pad = (pad + i) | 0;
  return (useIsInteger && pad === 6) ? Number.isInteger : (x => x === 0);
}

function callPoly(fn, x) {
  return fn(x);
}

function checkPostInlining(useIsInteger, a, b, s) {
  const fn = getPredicate(useIsInteger);
  return [
    callPoly(fn, a | 0),
    callPoly(fn, a + b),
    callPoly(fn, 3.5),
    callPoly(fn, 42.0),
    callPoly(fn, s),
  ];
}

const holey = [1.0, 2.5, , -0.0, NaN, Infinity, -Infinity];
const smiArr = [1, 2, 3];
const ta = new Uint8Array(16);
const allFns = [
  checkTagged,
  checkFloat64,
  checkInt32,
  checkUint32,
  checkIntPtr,
  checkHoleyDouble,
  checkKnownSmi,
  checkNonNumber,
  checkFloat64Alt,
  checkInt32Alt,
  checkKnownNumber,
  checkNoArgs,
  checkPostInlining,
];

%PrepareFunctionForOptimization(getPredicate);
%PrepareFunctionForOptimization(callPoly);
for (const fn of allFns) {
  %PrepareFunctionForOptimization(fn);
}

function runTests() {
  assertTrue(checkTagged(0));
  assertTrue(checkTagged(42));
  assertTrue(checkTagged(-100));
  assertTrue(checkTagged(0x3fffffff));
  assertTrue(checkTagged(-0x40000000));
  assertTrue(checkTagged(0x7fffffff));
  assertTrue(checkTagged(-0x80000000));
  assertTrue(checkTagged(-0.0));
  assertTrue(checkTagged(1e20));
  assertTrue(checkTagged(Number.MAX_SAFE_INTEGER));
  assertTrue(checkTagged(Number.MAX_SAFE_INTEGER + 1));
  assertTrue(checkTagged(Number.MIN_SAFE_INTEGER));
  assertTrue(checkTagged(Number.MAX_VALUE));
  assertTrue(checkTagged(2 ** 52));
  assertTrue(checkTagged(-(2 ** 52)));
  assertFalse(checkTagged(2 ** 52 - 0.5));
  assertFalse(checkTagged(-(2 ** 52 - 0.5)));
  assertFalse(checkTagged(Number.MIN_VALUE));
  assertFalse(checkTagged(-Number.MIN_VALUE));
  assertFalse(checkTagged(3.14));
  assertFalse(checkTagged(-0.5));
  assertFalse(checkTagged(NaN));
  assertFalse(checkTagged(Infinity));
  assertFalse(checkTagged(-Infinity));
  assertFalse(checkTagged(undefined));
  assertFalse(checkTagged(null));
  assertFalse(checkTagged("42"));
  assertFalse(checkTagged(true));
  assertFalse(checkTagged(false));
  assertFalse(checkTagged({}));
  assertFalse(checkTagged(new Number(42)));

  assertTrue(checkFloat64(1.25, 2.75));
  assertFalse(checkFloat64(1.25, 2.5));
  assertFalse(checkFloat64(2 ** 52, -0.5));
  assertTrue(checkFloat64(2 ** 52, 0.0));
  assertTrue(checkFloat64(Number.MAX_VALUE, 0.0));
  assertFalse(checkFloat64(Number.MIN_VALUE, 0.0));
  assertTrue(checkFloat64(-0.0, -0.0));
  assertFalse(checkFloat64(Infinity, 1.0));
  assertFalse(checkFloat64(-Infinity, 1.0));
  assertFalse(checkFloat64(NaN, 1.0));

  assertTrue(checkInt32(12345));
  assertTrue(checkUint32(0xffffffff));
  assertTrue(checkIntPtr(ta));
  assertTrue(checkKnownSmi(smiArr, 1));
  assertFalse(checkNonNumber(1));
  assertEquals([4.5, true], checkFloat64Alt(4.0));
  assertEquals([4.75, false], checkFloat64Alt(4.25));
  assertEquals([0, true], checkInt32Alt(ta, 0));
  assertEquals([undefined, true], checkInt32Alt(ta, 0x7fffffff));
  assertTrue(checkKnownNumber({ v: 4.0 }, false));
  assertFalse(checkKnownNumber({ v: 1.5 }, false));
  assertTrue(checkKnownNumber({ v: 1.5 }, true));

  assertEquals([true, 1.0], checkHoleyDouble(holey, 0));
  assertEquals([false, 2.5], checkHoleyDouble(holey, 1));
  assertEquals([false, undefined], checkHoleyDouble(holey, 2));
  assertEquals([true, -0.0], checkHoleyDouble(holey, 3));
  assertEquals([false, NaN], checkHoleyDouble(holey, 4));
  assertEquals([false, Infinity], checkHoleyDouble(holey, 5));
  assertEquals([false, -Infinity], checkHoleyDouble(holey, 6));

  assertFalse(checkNoArgs());

  assertEquals([false, false, false, false, false],
               checkPostInlining(false, 7, 3.0, "str"));
  assertEquals([true, true, false, true, false],
               checkPostInlining(true, 7, 3.0, "str"));
  assertEquals([true, false, false, true, false],
               checkPostInlining(true, 7, 0.25, null));
}

for (let i = 0; i < 5; ++i) runTests();

for (const fn of allFns) {
  %OptimizeMaglevOnNextCall(fn);
}

runTests();

for (const fn of allFns) {
  assertMaglevved(fn);
}

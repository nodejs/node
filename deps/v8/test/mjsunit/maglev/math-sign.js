// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

// The reduction only ever runs on numbers and oddballs, so every test below
// warms up with all the input shapes it later exercises and stays optimized
// throughout. Inputs the reduction cannot handle are collected at the end.

(function testInt32() {
  function signInt32(i) {
    i = i | 0;
    return Math.sign(i);
  }
  %PrepareFunctionForOptimization(signInt32);
  // Warm up with every shape ToInt32 sees below, so it does not deoptimize.
  signInt32(0);
  signInt32(2);
  signInt32(2147483648);
  signInt32(undefined);
  %OptimizeMaglevOnNextCall(signInt32);
  assertEquals(1, signInt32(1));
  assertEquals(0, signInt32(0));
  assertEquals(-1, signInt32(-1));
  assertEquals(1, signInt32(2147483647));
  assertEquals(-1, signInt32(2147483648));
  assertEquals(-1, signInt32(-2147483648));
  // The int32 path can never produce -0.
  assertTrue(Object.is(0, signInt32(-0)));
  assertEquals(0, signInt32(NaN));
  assertEquals(0, signInt32(undefined));
  assertOptimized(signInt32);
})();

(function testFloat64() {
  function signFloat64(i) {
    return Math.sign(+i);
  }
  %PrepareFunctionForOptimization(signFloat64);
  signFloat64(0.1);
  signFloat64(-0.1);
  signFloat64(1);
  signFloat64(undefined);
  %OptimizeMaglevOnNextCall(signFloat64);
  assertEquals(1, signFloat64(1));
  assertEquals(1, signFloat64(0.001));
  assertEquals(-1, signFloat64(-0.002));
  assertEquals(1, signFloat64(1e100));
  assertEquals(-1, signFloat64(-2e100));
  assertEquals(-1, signFloat64(-1));
  assertEquals(1, signFloat64(2147483648));
  assertEquals(-1, signFloat64(-2147483649));
  assertEquals(1, signFloat64(Infinity));
  assertEquals(-1, signFloat64(-Infinity));
  assertEquals(NaN, signFloat64(NaN));
  assertEquals(NaN, signFloat64(undefined));
  // Both zeros must be returned with their sign preserved.
  assertTrue(Object.is(0, signFloat64(0)));
  assertTrue(Object.is(-0, signFloat64(-0)));
  assertEquals(Infinity, 1 / signFloat64(0));
  assertEquals(-Infinity, 1 / signFloat64(-0));
  assertOptimized(signFloat64);
})();

(function testTagged() {
  function signTagged(x) {
    return Math.sign(x);
  }
  %PrepareFunctionForOptimization(signTagged);
  signTagged(1.5);
  signTagged(-1.5);
  signTagged(3);
  %OptimizeMaglevOnNextCall(signTagged);
  assertEquals(1, signTagged(1.5));
  assertEquals(-1, signTagged(-1.5));
  assertEquals(1, signTagged(3));
  assertEquals(-1, signTagged(-3));
  assertEquals(NaN, signTagged(NaN));
  assertTrue(Object.is(0, signTagged(0)));
  assertTrue(Object.is(-0, signTagged(-0)));
  assertOptimized(signTagged);
})();

(function testSmiInput() {
  // A plain parameter carries no static type, so Smi arguments still take the
  // float64 path. The int32 path is reached through testInt32's ToInt32.
  function signSmiInput(x) {
    return Math.sign(x);
  }
  %PrepareFunctionForOptimization(signSmiInput);
  signSmiInput(1);
  signSmiInput(-1);
  %OptimizeMaglevOnNextCall(signSmiInput);
  assertEquals(1, signSmiInput(7));
  assertEquals(0, signSmiInput(0));
  assertEquals(-1, signSmiInput(-7));
  assertOptimized(signSmiInput);
})();

(function testOddballs() {
  // Oddballs go through the reduction's float64 path, they do not deoptimize.
  function signOddball(x) {
    return Math.sign(x);
  }
  %PrepareFunctionForOptimization(signOddball);
  signOddball(undefined);
  signOddball(null);
  signOddball(true);
  signOddball(false);
  %OptimizeMaglevOnNextCall(signOddball);
  assertEquals(NaN, signOddball(undefined));
  assertEquals(0, signOddball(null));
  assertEquals(1, signOddball(true));
  assertEquals(0, signOddball(false));
  assertOptimized(signOddball);
})();

(function testHoleyDouble() {
  // Reading a hole yields undefined, whose sign is NaN.
  const a = [1.5, , -2.5, , 0.5];
  function signHoley(i) {
    return Math.sign(a[i]);
  }
  %PrepareFunctionForOptimization(signHoley);
  signHoley(0);
  signHoley(1);
  signHoley(2);
  %OptimizeMaglevOnNextCall(signHoley);
  assertEquals(1, signHoley(0));
  assertEquals(NaN, signHoley(1));
  assertEquals(-1, signHoley(2));
  assertEquals(NaN, signHoley(3));
  assertEquals(1, signHoley(4));
  assertEquals('number', typeof signHoley(1));
  assertOptimized(signHoley);
})();

(function testZeroAndNaNPassThrough() {
  // -0, +0 and NaN take the arm that returns the input unchanged. The result
  // must still be a number, and -0 must keep its sign.
  function signPassThrough(x) {
    return Math.sign(x);
  }
  %PrepareFunctionForOptimization(signPassThrough);
  signPassThrough(-0);
  signPassThrough(0);
  signPassThrough(NaN);
  %OptimizeMaglevOnNextCall(signPassThrough);
  assertEquals('number', typeof signPassThrough(-0));
  assertEquals('number', typeof signPassThrough(NaN));
  assertTrue(Object.is(0, signPassThrough(0)));
  assertTrue(Object.is(-0, signPassThrough(-0)));
  assertEquals(NaN, signPassThrough(NaN));
  assertOptimized(signPassThrough);
})();

(function testConstantFold() {
  function positive() { return Math.sign(27.5); }
  function negative() { return Math.sign(-27.5); }
  function nan() { return Math.sign(NaN); }
  function minusZero() { return Math.sign(-0); }
  function infinity() { return Math.sign(-Infinity); }
  const fns = [positive, negative, nan, minusZero, infinity];
  for (const f of fns) {
    %PrepareFunctionForOptimization(f);
    f();
    %OptimizeMaglevOnNextCall(f);
  }
  assertEquals(1, positive());
  assertEquals(-1, negative());
  assertEquals(NaN, nan());
  assertTrue(Object.is(-0, minusZero()));
  assertEquals(-1, infinity());
  for (const f of fns) assertOptimized(f);
})();

(function testNoArguments() {
  function signNoArgs() { return Math.sign(); }
  %PrepareFunctionForOptimization(signNoArgs);
  signNoArgs();
  %OptimizeMaglevOnNextCall(signNoArgs);
  assertEquals(NaN, signNoArgs());
  assertOptimized(signNoArgs);
})();

(function testNullReceiver() {
  function signNullRecv(x) { return Math.sign.call(null, x); }
  %PrepareFunctionForOptimization(signNullRecv);
  signNullRecv(2);
  %OptimizeMaglevOnNextCall(signNullRecv);
  assertEquals(1, signNullRecv(2));
  assertEquals(-1, signNullRecv(-2));
  assertOptimized(signNullRecv);
})();

// Inputs below are not numbers, so the reduction bails or deoptimizes and the
// generic builtin runs. These check the fallback stays correct, not the
// reduction.

(function testNonNumberInputIsStillCorrect() {
  function signNonNumber(x) {
    return Math.sign(x);
  }
  %PrepareFunctionForOptimization(signNonNumber);
  signNonNumber('-5');
  signNonNumber('5');
  %OptimizeMaglevOnNextCall(signNonNumber);
  assertEquals(-1, signNonNumber('-5'));
  assertEquals(1, signNonNumber('5'));
  assertEquals(NaN, signNonNumber('foo'));
  assertEquals(0, signNonNumber([]));
  assertEquals(1, signNonNumber([3]));
  assertTrue(Object.is(-0, signNonNumber('-0')));
})();

(function testValueOfIsCalledOnce() {
  let calls = 0;
  const obj = {
    valueOf() {
      calls++;
      return -4;
    }
  };
  function signValueOf(x) {
    return Math.sign(x);
  }
  %PrepareFunctionForOptimization(signValueOf);
  assertEquals(-1, signValueOf(obj));
  %OptimizeMaglevOnNextCall(signValueOf);
  calls = 0;
  assertEquals(-1, signValueOf(obj));
  assertEquals(1, calls);
})();

(function testDeoptOnTypeChange() {
  function signTypeChange(x) {
    return Math.sign(x);
  }
  %PrepareFunctionForOptimization(signTypeChange);
  signTypeChange(1.5);
  signTypeChange(-1.5);
  %OptimizeMaglevOnNextCall(signTypeChange);
  assertEquals(1, signTypeChange(1.5));
  assertOptimized(signTypeChange);
  // Feeding a string after optimizing must deopt and still compute correctly.
  assertEquals(-1, signTypeChange('-9'));
  assertEquals(NaN, signTypeChange({}));
})();

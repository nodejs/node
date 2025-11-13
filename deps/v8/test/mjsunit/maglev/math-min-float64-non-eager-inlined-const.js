// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev
// Flags: --max-maglev-inlined-bytecode-size-small=0
// Flags: --maglev-non-eager-inlining

// --max-maglev-inlined-bytecode-size-small=0 prevents these from getting eagerly inlined.
function getDouble() {
  return 5.5;
}
%PrepareFunctionForOptimization(getDouble);

function getNaN() {
  return NaN;
}
%PrepareFunctionForOptimization(getNaN);

function getMinusZero() {
  return -0.0;
}
%PrepareFunctionForOptimization(getMinusZero);

function getInfinity() {
  return Infinity;
}
%PrepareFunctionForOptimization(getInfinity);

function getTrue() {
  return true;
}
%PrepareFunctionForOptimization(getTrue);

function getFalse() {
  return false;
}
%PrepareFunctionForOptimization(getFalse);

function getUndefined() {
  return undefined;
}
%PrepareFunctionForOptimization(getUndefined);

function getNull() {
  return null;
}
%PrepareFunctionForOptimization(getNull);

function test(f) {
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  f();
  assertTrue(isMaglevved(f));
}

function testDoubles() {
  assertEquals(5.5, Math.min(getDouble(), 6.2));
  assertTrue(Object.is(-0, Math.min(getMinusZero(), 0.0)));
  assertTrue(isNaN(Math.min(5.2, getNaN())));
  assertSame(Infinity, Math.min(getInfinity(), getInfinity()));
}

test(testDoubles);

function testOddballs() {
  assertTrue(isNaN(Math.min(5.2, getUndefined())));
  assertEquals(1, Math.min(getTrue(), 6));
  assertEquals(0, Math.min(getFalse(), 5));
  assertEquals(0, Math.min(5, getNull()));
}

test(testOddballs);

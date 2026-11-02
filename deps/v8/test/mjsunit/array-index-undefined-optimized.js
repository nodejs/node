// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function checkOptimizedPair(omitted, explicit, expected, nextExpected) {
  %PrepareFunctionForOptimization(omitted);
  %PrepareFunctionForOptimization(explicit);
  assertEquals(expected, omitted([42]));
  assertEquals(expected, explicit([42]));
  %OptimizeFunctionOnNextCall(omitted);
  %OptimizeFunctionOnNextCall(explicit);
  assertEquals(expected, omitted([42]));
  assertEquals(expected, explicit([42]));
  assertOptimized(omitted);
  assertOptimized(explicit);
  assertEquals(nextExpected, omitted([99]));
  assertEquals(nextExpected, explicit([99]));
  assertOptimized(omitted);
  assertOptimized(explicit);
}

function atOmitted(array) {
  return array.at();
}

function atUndefined(array) {
  return array.at(undefined);
}

checkOptimizedPair(atOmitted, atUndefined, 42, 99);
assertEquals(undefined, atOmitted([]));
assertEquals(undefined, atUndefined([]));
assertOptimized(atOmitted);
assertOptimized(atUndefined);

function indexOfOmitted(array) {
  return array.indexOf(42);
}

function indexOfUndefined(array) {
  return array.indexOf(42, undefined);
}

checkOptimizedPair(indexOfOmitted, indexOfUndefined, 0, -1);

function includesOmitted(array) {
  return array.includes(42);
}

function includesUndefined(array) {
  return array.includes(42, undefined);
}

checkOptimizedPair(includesOmitted, includesUndefined, true, false);

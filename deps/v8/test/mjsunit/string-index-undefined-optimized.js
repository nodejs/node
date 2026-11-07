// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function checkOptimizedPair(omitted, explicit, expected, nextExpected) {
  %PrepareFunctionForOptimization(omitted);
  %PrepareFunctionForOptimization(explicit);
  assertEquals(expected, omitted('abc'));
  assertEquals(expected, explicit('abc'));
  %OptimizeFunctionOnNextCall(omitted);
  %OptimizeFunctionOnNextCall(explicit);
  assertEquals(expected, omitted('abc'));
  assertEquals(omitted('abc'), explicit('abc'));
  assertOptimized(omitted);
  assertOptimized(explicit);
  assertEquals(nextExpected, omitted('xyz'));
  assertEquals(omitted('xyz'), explicit('xyz'));
  assertOptimized(omitted);
  assertOptimized(explicit);
}

function charAtOmitted(string) {
  return string.charAt();
}

function charAtUndefined(string) {
  return string.charAt(undefined);
}
checkOptimizedPair(charAtOmitted, charAtUndefined, 'a', 'x');

function charCodeAtOmitted(string) {
  return string.charCodeAt();
}

function charCodeAtUndefined(string) {
  return string.charCodeAt(undefined);
}
checkOptimizedPair(charCodeAtOmitted, charCodeAtUndefined, 97, 120);

function codePointAtOmitted(string) {
  return string.codePointAt();
}

function codePointAtUndefined(string) {
  return string.codePointAt(undefined);
}
checkOptimizedPair(codePointAtOmitted, codePointAtUndefined, 97, 120);

function startsWithOmitted(string) {
  return string.startsWith('a');
}

function startsWithUndefined(string) {
  return string.startsWith('a', undefined);
}

checkOptimizedPair(startsWithOmitted, startsWithUndefined, true, false);

function indexOfOmitted(string) {
  return string.indexOf('b');
}

function indexOfUndefined(string) {
  return string.indexOf('b', undefined);
}

checkOptimizedPair(indexOfOmitted, indexOfUndefined, 1, -1);

function includesOmitted(string) {
  return string.includes('b');
}

function includesUndefined(string) {
  return string.includes('b', undefined);
}

checkOptimizedPair(includesOmitted, includesUndefined, true, false);

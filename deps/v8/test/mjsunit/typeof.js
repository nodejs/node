// Copyright 2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

'use strict';

// The type of a regular expression should be 'object', including in
// the context of string equality comparisons.
{
  const r = new RegExp;

  assertEquals('object', typeof r);
  assertTrue(typeof r == 'object');
  assertFalse(typeof r == 'function');

  function equals(x, y) { return x == y; }
  assertTrue(equals('object', typeof r));
}

assertFalse(typeof null == "undefined");

assertEquals('undefined', typeof undefined);
assertEquals('object', typeof null);
assertEquals('boolean', typeof true);
assertEquals('boolean', typeof false);
assertEquals('number', typeof 42.42);
assertEquals('number', typeof 42);
assertEquals('bigint', typeof 42n);
assertEquals('string', typeof '42');
assertEquals('symbol', typeof Symbol(42));
assertEquals('object', typeof {});
assertEquals('object', typeof []);
assertEquals('object', typeof new Proxy({}, {}));
assertEquals('object', typeof new Proxy([], {}));
assertEquals('function', typeof (_ => 42));
assertEquals('function', typeof function() {});
assertEquals('function', typeof function*() {});
assertEquals('function', typeof async function() {});
assertEquals('function', typeof async function*() {});
assertEquals('function', typeof new Proxy(_ => 42, {}));
assertEquals('function', typeof class {});
assertEquals('function', typeof Object);

// Optimizing TypeOf

function test2_typeof(v, t) {
  assertEquals(typeof v, t);
}
function test(v1, t1, v2, t2) {
  var test2 = copyFunctionWithFreshFeedback(test2_typeof);
  %PrepareFunctionForOptimization(test2);
  test2(v1, t1);
  test2(v1, t1);
  test2(v1, t1);
  %OptimizeFunctionOnNextCall(test2);
  test2(v1, t1);
  test2(v2, t2);
}
test(function(){}, 'function', {}, 'object');
test(1, 'number', {}, 'object');
test(1.2, 'number', {}, 'object');
test("asdF", 'string', {}, 'object');
test(undefined, 'undefined', {}, 'object');
test(undefined, 'undefined', null, 'object');
test(null, 'object', undefined, 'undefined');
test(true, 'boolean', null, 'object');

// Optimizing TestTypeOf

function test2_test_typeof(v) {
  if (v) v.x
  if (typeof v == "number") return "number";
  if (typeof v == "object") return "object";
  if (typeof v == "function") return "function";
  if (typeof v == "undefined") return "undefined";
  if (typeof v == "boolean") return "boolean";
  if (typeof v == "string") return "string";
}
function test_branch(v1, t1, v2, t2) {
  var test2 = copyFunctionWithFreshFeedback(test2_test_typeof);
  %PrepareFunctionForOptimization(test2);
  assertEquals(test2(v1), t1);
  assertEquals(test2(v1), t1);
  assertEquals(test2(v1), t1);
  %OptimizeFunctionOnNextCall(test2);
  assertEquals(test2(v1), t1);
  assertEquals(test2(v2), t2);
}
test_branch(function(){}, 'function', {}, 'object');
test_branch(1, 'number', {}, 'object');
test_branch(1.2, 'number', {}, 'object');
test_branch("asdF", 'string', {}, 'object');
test_branch(undefined, 'undefined', {}, 'object');
test_branch(undefined, 'undefined', null, 'object');
test_branch(null, 'object', undefined, 'undefined');
test_branch(true, 'boolean', null, 'object');

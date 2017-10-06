// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

(function() {
  function eq(a, b) { return a == b; }

  var o = { [Symbol.toPrimitive]: () => "o" };

  assertTrue(eq(o, o));
  assertTrue(eq(o, o));
  %OptimizeFunctionOnNextCall(eq);
  assertTrue(eq(o, o));
  assertTrue(eq("o", o));
  assertTrue(eq(o, "o"));
  %OptimizeFunctionOnNextCall(eq);
  assertTrue(eq(o, o));
  assertTrue(eq("o", o));
  assertTrue(eq(o, "o"));
  assertOptimized(eq);
})();

(function() {
  function ne(a, b) { return a != b; }

  var o = { [Symbol.toPrimitive]: () => "o" };

  assertFalse(ne(o, o));
  assertFalse(ne(o, o));
  %OptimizeFunctionOnNextCall(ne);
  assertFalse(ne(o, o));
  assertFalse(ne("o", o));
  assertFalse(ne(o, "o"));
  %OptimizeFunctionOnNextCall(ne);
  assertFalse(ne(o, o));
  assertFalse(ne("o", o));
  assertFalse(ne(o, "o"));
  assertOptimized(ne);
})();

(function() {
  function eq(a, b) { return a == b; }

  var a = {};
  var b = {b};
  var u = %GetUndetectable();

  assertTrue(eq(a, a));
  assertTrue(eq(b, b));
  assertFalse(eq(a, b));
  assertFalse(eq(b, a));
  assertTrue(eq(a, a));
  assertTrue(eq(b, b));
  assertFalse(eq(a, b));
  assertFalse(eq(b, a));
  %OptimizeFunctionOnNextCall(eq);
  assertTrue(eq(a, a));
  assertTrue(eq(b, b));
  assertFalse(eq(a, b));
  assertFalse(eq(b, a));
  assertTrue(eq(null, u));
  assertTrue(eq(undefined, u));
  assertTrue(eq(u, null));
  assertTrue(eq(u, undefined));
  %OptimizeFunctionOnNextCall(eq);
  assertTrue(eq(a, a));
  assertTrue(eq(b, b));
  assertFalse(eq(a, b));
  assertFalse(eq(b, a));
  assertTrue(eq(null, u));
  assertTrue(eq(undefined, u));
  assertTrue(eq(u, null));
  assertTrue(eq(u, undefined));
  assertOptimized(eq);
})();

(function() {
  function ne(a, b) { return a != b; }

  var a = {};
  var b = {b};
  var u = %GetUndetectable();

  assertFalse(ne(a, a));
  assertFalse(ne(b, b));
  assertTrue(ne(a, b));
  assertTrue(ne(b, a));
  assertFalse(ne(a, a));
  assertFalse(ne(b, b));
  assertTrue(ne(a, b));
  assertTrue(ne(b, a));
  %OptimizeFunctionOnNextCall(ne);
  assertFalse(ne(a, a));
  assertFalse(ne(b, b));
  assertTrue(ne(a, b));
  assertTrue(ne(b, a));
  assertFalse(ne(null, u));
  assertFalse(ne(undefined, u));
  assertFalse(ne(u, null));
  assertFalse(ne(u, undefined));
  %OptimizeFunctionOnNextCall(ne);
  assertFalse(ne(a, a));
  assertFalse(ne(b, b));
  assertTrue(ne(a, b));
  assertTrue(ne(b, a));
  assertFalse(ne(null, u));
  assertFalse(ne(undefined, u));
  assertFalse(ne(u, null));
  assertFalse(ne(u, undefined));
  assertOptimized(ne);
})();

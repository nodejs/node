// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = [0, 1];
a["true"] = "true";
a["false"] = "false";
a["null"] = "null";
a["undefined"] = "undefined";

// Ensure we don't accidentially truncate true when used to index arrays.
(function() {
  function f(x) { return a[x]; }

  %PrepareFunctionForOptimization(f);
  assertEquals(0, f(0));
  assertEquals(0, f(0));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("true", f(true));
})();

// Ensure we don't accidentially truncate false when used to index arrays.
(function() {
  function f( x) { return a[x]; }

  %PrepareFunctionForOptimization(f);
  assertEquals(0, f(0));
  assertEquals(0, f(0));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("false", f(false));
})();

// Ensure we don't accidentially truncate null when used to index arrays.
(function() {
  function f( x) { return a[x]; }

  %PrepareFunctionForOptimization(f);
  assertEquals(0, f(0));
  assertEquals(0, f(0));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("null", f(null));
})();

// Ensure we don't accidentially truncate undefined when used to index arrays.
(function() {
  function f( x) { return a[x]; }

  %PrepareFunctionForOptimization(f);
  assertEquals(0, f(0));
  assertEquals(0, f(0));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("undefined", f(undefined));
})();

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test corner cases with null/undefined receivers.
(function() {
  function foo(x, y) { return Object.prototype.isPrototypeOf.call(x, y); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(null, {}));
  assertThrows(() => foo(undefined, {}));
  assertThrows(() => foo(null, []));
  assertThrows(() => foo(undefined, []));
  assertFalse(foo(null, 0));
  assertFalse(foo(undefined, 0));
  assertFalse(foo(null, ""));
  assertFalse(foo(undefined, ""));
  assertFalse(foo(null, null));
  assertFalse(foo(undefined, null));
  assertFalse(foo(null, undefined));
  assertFalse(foo(undefined, undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(null, {}));
  assertThrows(() => foo(undefined, {}));
  assertThrows(() => foo(null, []));
  assertThrows(() => foo(undefined, []));
  assertFalse(foo(null, 0));
  assertFalse(foo(undefined, 0));
  assertFalse(foo(null, ""));
  assertFalse(foo(undefined, ""));
  assertFalse(foo(null, null));
  assertFalse(foo(undefined, null));
  assertFalse(foo(null, undefined));
  assertFalse(foo(undefined, undefined));
})();

// Test general constructor prototype case.
(function() {
  function A() {}
  A.prototype = {};
  var a = new A;

  function foo(x) { return A.prototype.isPrototypeOf(x); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(0));
  assertFalse(foo(""));
  assertFalse(foo(null));
  assertFalse(foo(undefined));
  assertFalse(foo({}));
  assertFalse(foo([]));
  assertTrue(foo(a));
  assertTrue(foo(new A));
  assertTrue(foo({__proto__: a}));
  assertTrue(foo({__proto__: A.prototype}));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(0));
  assertFalse(foo(""));
  assertFalse(foo(null));
  assertFalse(foo(undefined));
  assertFalse(foo({}));
  assertFalse(foo([]));
  assertTrue(foo(a));
  assertTrue(foo(new A));
  assertTrue(foo({__proto__: a}));
  assertTrue(foo({__proto__: A.prototype}));
})();

// Test known primitive values.
(function() {
  function A() {}
  A.prototype = {};
  var a = new A;

  function foo() { return A.prototype.isPrototypeOf(0); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();
(function() {
  function A() {}
  A.prototype = {};
  var a = new A;

  function foo() { return A.prototype.isPrototypeOf(null); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();
(function() {
  function A() {}
  A.prototype = {};
  var a = new A;

  function foo() { return A.prototype.isPrototypeOf(undefined); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Test constant-folded prototype chain checks.
(function() {
  function A() {}
  A.prototype = {};
  var a = new A;

  function foo() { return A.prototype.isPrototypeOf(a); }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();
(function() {
  function A() {}
  var a = new A;
  A.prototype = {};

  function foo() { return A.prototype.isPrototypeOf(a); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

// Test Array prototype chain checks.
(function() {
  var a = [];

  function foo() { return Array.prototype.isPrototypeOf(a); }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();
(function() {
  var a = [];

  function foo() { return Object.prototype.isPrototypeOf(a); }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();
(function() {
  function A() {}
  A.prototype = {};
  var a = {__proto__: new A, gaga: 42};

  function foo() { a.gaga; return A.prototype.isPrototypeOf(a); }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

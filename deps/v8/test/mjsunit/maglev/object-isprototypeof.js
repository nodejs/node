// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test corner cases with null/undefined receivers.
(function() {
  function foo(x, y) { return Object.prototype.isPrototypeOf.call(x, y); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(null, {}));
  assertThrows(() => foo(undefined, {}));
  assertFalse(foo(null, 0));
  assertFalse(foo(undefined, ""));
  assertFalse(foo(null, null));
  assertFalse(foo(undefined, undefined));
  %OptimizeMaglevOnNextCall(foo);
  assertThrows(() => foo(null, {}));
  assertThrows(() => foo(undefined, {}));
  assertFalse(foo(null, 0));
  assertFalse(foo(undefined, ""));
  assertFalse(foo(null, null));
  assertFalse(foo(undefined, undefined));
  assertOptimized(foo);
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
  %OptimizeMaglevOnNextCall(foo);
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
  assertOptimized(foo);
})();

// Test dynamic (non-constant) receivers.
(function() {
  function A() {}
  var a = new A;
  var b = new A;
  Object.setPrototypeOf(b, a);

  function foo(x, y) { return x.isPrototypeOf(y); }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(a, b));
  assertFalse(foo(b, a));
  assertFalse(foo(a, 0));
  assertFalse(foo(a, null));
  assertFalse(foo(a, undefined));
  assertTrue(foo(A.prototype, a));
  %OptimizeMaglevOnNextCall(foo);
  assertTrue(foo(a, b));
  assertFalse(foo(b, a));
  assertFalse(foo(a, 0));
  assertFalse(foo(a, null));
  assertFalse(foo(a, undefined));
  assertTrue(foo(A.prototype, a));
  assertOptimized(foo);
})();

// Test no-argument call.
(function() {
  function A() {}
  var a = new A;

  function foo() { return a.isPrototypeOf(); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeMaglevOnNextCall(foo);
  assertFalse(foo());
  assertOptimized(foo);
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
  %OptimizeMaglevOnNextCall(foo);
  assertTrue(foo());
  assertOptimized(foo);
})();
(function() {
  function A() {}
  var a = new A;
  A.prototype = {};

  function foo() { return A.prototype.isPrototypeOf(a); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeMaglevOnNextCall(foo);
  assertFalse(foo());
  assertOptimized(foo);
})();

// Test prototype chain mutation after optimization.
(function() {
  function A() {}
  A.prototype = {};
  var a = new A;

  function foo() { return A.prototype.isPrototypeOf(a); }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeMaglevOnNextCall(foo);
  assertTrue(foo());
  Object.setPrototypeOf(a, null);
  assertUnoptimized(foo);
  assertFalse(foo());
})();

// Test proxy in the prototype chain of the argument.
(function() {
  var traps = 0;
  var target = {};
  var proxy = new Proxy(target, {
    getPrototypeOf(t) { traps++; return Object.getPrototypeOf(t); }
  });
  var o = Object.create(proxy);
  var needle = {};

  function foo() { return needle.isPrototypeOf(o); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo());
  assertFalse(foo());
  %OptimizeMaglevOnNextCall(foo);
  assertFalse(foo());
  assertOptimized(foo);
  assertTrue(traps > 0);
})();

// Test proxy as the receiver.
(function() {
  var proxy = new Proxy({}, {});
  var o = Object.create(proxy);

  function foo(x) { return proxy.isPrototypeOf(x); }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(o));
  assertFalse(foo({}));
  %OptimizeMaglevOnNextCall(foo);
  assertTrue(foo(o));
  assertFalse(foo({}));
  assertOptimized(foo);
})();

// Test a throwing getPrototypeOf trap.
(function() {
  var proxy = new Proxy({}, {
    getPrototypeOf(t) { throw new Error("trap"); }
  });
  var o = Object.create(proxy);
  var needle = {};

  function foo() { return needle.isPrototypeOf(o); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo, Error);
  assertThrows(foo, Error);
  %OptimizeMaglevOnNextCall(foo);
  assertThrows(foo, Error);
  assertOptimized(foo);
})();

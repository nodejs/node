// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Each case runs its assertions once to gather feedback and once after
// optimizing, so that the optimized code never sees an unseen input kind.
function testOptimized(check, ...funs) {
  for (const fun of funs) %PrepareFunctionForOptimization(fun);
  check();
  for (const fun of funs) %OptimizeMaglevOnNextCall(fun);
  check();
  for (const fun of funs) assertOptimized(fun);
}

// Object.is(x, x) folds to true, even for NaN and -0.
(function() {
  function foo(o) { return Object.is(o, o); }
  testOptimized(() => {
    assertTrue(foo(-0));
    assertTrue(foo(0));
    assertTrue(foo(NaN));
    assertTrue(foo(Infinity));
    assertTrue(foo(''));
    assertTrue(foo([]));
    assertTrue(foo({}));
    assertTrue(foo(undefined));
    assertTrue(foo(null));
    assertTrue(foo(Symbol.iterator));
    assertTrue(foo(1n));
  }, foo);
})();

// Missing arguments are undefined.
(function() {
  function none() { return Object.is(); }
  function one(o) { return Object.is(o); }
  testOptimized(() => {
    assertTrue(none());
    assertTrue(one(undefined));
    assertFalse(one(null));
    assertFalse(one(0));
  }, none, one);
})();

// Int32 inputs never are NaN or -0, so this is plain integer equality.
(function() {
  function foo(a, b) { return Object.is(a | 0, b | 0); }
  testOptimized(() => {
    assertTrue(foo(1, 1));
    assertTrue(foo(0, -0));
    assertTrue(foo(NaN, 0));
    assertTrue(foo(-1, -1));
    assertFalse(foo(1, 2));
  }, foo);
})();

// A constant that is neither NaN nor -0 degenerates to numeric equality.
(function() {
  function foo(o) { return Object.is(+o, 5.5); }
  testOptimized(() => {
    assertTrue(foo(5.5));
    assertFalse(foo(5));
    assertFalse(foo(NaN));
    assertFalse(foo(-0));
  }, foo);
})();

// A zero-valued side must not degenerate to numeric equality: +0 and -0 are
// numerically equal but not same-value.
(function() {
  function smiZero(o) { return Object.is(+o, 0); }
  function int32Zero(a, b) { return Object.is(+a, b | 0); }
  // Both operands constant: a zero constant must not fold via numeric equality.
  // Math.max(-0, 0) is +0, Math.min(-0, 0) is -0.
  function negZeroVsZero() { return Object.is(-0, 0); }
  function zeroVsNegZero() { return Object.is(0, -0); }
  function negZeroVsMax() { return Object.is(-0, Math.max(-0.0, 0.0)); }
  function negZeroVsMin() { return Object.is(-0, Math.min(-0.0, 0.0)); }
  testOptimized(() => {
    assertFalse(smiZero(-0));
    assertTrue(smiZero(0));
    assertFalse(smiZero(NaN));
    assertFalse(int32Zero(-0, 0));
    assertTrue(int32Zero(0, 0));
    assertFalse(int32Zero(NaN, 0));
    assertFalse(negZeroVsZero());
    assertFalse(zeroVsNegZero());
    assertFalse(negZeroVsMax());
    assertTrue(negZeroVsMin());
  }, smiZero, int32Zero, negZeroVsZero, zeroVsNegZero, negZeroVsMax, negZeroVsMin);
})();

// The full Float64 same-value comparison.
(function() {
  function foo(a, b) { return Object.is(+a, +b); }
  testOptimized(() => {
    assertTrue(foo(0, 0));
    assertTrue(foo(-0, -0));
    assertTrue(foo(0.1, 0.1));
    assertTrue(foo(NaN, NaN));
    assertTrue(foo(Infinity, Infinity));
    assertTrue(foo(-Infinity, -Infinity));
    assertFalse(foo(0, -0));
    assertFalse(foo(-0, 0));
    assertFalse(foo(-0, 1));
    assertFalse(foo(1, 2));
    assertFalse(foo(-Infinity, Infinity));
    assertFalse(foo(Infinity, NaN));
    assertFalse(foo(NaN, Infinity));
    // Denormals close to zero must not be confused with -0.
    assertFalse(foo(-5e-324, -0));
    assertFalse(foo(-0, -5e-324));
    assertTrue(foo(-5e-324, -5e-324));
  }, foo);
})();

// -0 and NaN constants against a dynamic number.
(function() {
  function minusZero(o) { return Object.is(+o, -0); }
  function nan(o) { return Object.is(NaN, +o); }
  testOptimized(() => {
    assertTrue(minusZero(-0));
    assertFalse(minusZero(0));
    assertFalse(minusZero(NaN));
    assertFalse(minusZero(-5e-324));
    assertTrue(nan(NaN));
    assertFalse(nan(0));
    assertFalse(nan(-0));
    assertFalse(nan(Infinity));
  }, minusZero, nan);
})();

// Constant folding of two number constants.
(function() {
  function nanNan() { return Object.is(NaN, NaN); }
  function zeroNegZero() { return Object.is(0, -0); }
  function negZeroNegZero() { return Object.is(-0, -0); }
  function oneOne() { return Object.is(1, 1.0); }
  function oneTwo() { return Object.is(1, 2); }
  testOptimized(() => {
    assertTrue(nanNan());
    assertFalse(zeroNegZero());
    assertTrue(negZeroNegZero());
    assertTrue(oneOne());
    assertFalse(oneTwo());
  }, nanNan, zeroNegZero, negZeroNegZero, oneOne, oneTwo);
})();

// JSReceivers, symbols and oddballs compare by reference.
(function() {
  const o = {};
  const s = Symbol();
  function receiver(x) { return Object.is(x, o); }
  function symbol(x) { return Object.is(s, x); }
  function oddball(x) { return Object.is(x, undefined); }
  testOptimized(() => {
    assertTrue(receiver(o));
    assertFalse(receiver({}));
    assertFalse(receiver(0));
    assertFalse(receiver('x'));
    assertTrue(symbol(s));
    assertFalse(symbol(Symbol()));
    assertFalse(symbol('x'));
    assertTrue(oddball(undefined));
    assertFalse(oddball(null));
    assertFalse(oddball(false));
    assertFalse(oddball(0));
  }, receiver, symbol, oddball);
})();

// Strings compare by value, not by reference.
(function() {
  function foo(o) { return Object.is(`${o}`, 'foobar'); }
  testOptimized(() => {
    assertFalse(foo('bar'));
    assertTrue(foo('foobar'));
    assertTrue(foo('foo' + 'bar'));
  }, foo);
})();

// Statically disjoint types fold to false.
(function() {
  function foo(o) { return Object.is(`${o}`, +o); }
  testOptimized(() => {
    assertFalse(foo(1));
    assertFalse(foo(0));
  }, foo);
})();

// The generic path, with no static type information at all.
(function() {
  const o = {};
  function foo(a, b) { return Object.is(a, b); }
  testOptimized(() => {
    assertTrue(foo(NaN, NaN));
    assertTrue(foo(-0, -0));
    assertTrue(foo('a', 'a'));
    assertTrue(foo(1n, 1n));
    assertTrue(foo(undefined, undefined));
    assertTrue(foo(null, null));
    assertTrue(foo(o, o));
    assertFalse(foo(0, -0));
    assertFalse(foo(1, '1'));
    assertFalse(foo(null, undefined));
    assertFalse(foo({}, {}));
    assertFalse(foo(1n, 2n));
  }, foo);
})();

// Object.is is not sensitive to the receiver.
(function() {
  function foo(a, b) { return Object.is.call(null, a, b); }
  testOptimized(() => {
    assertTrue(foo(NaN, NaN));
    assertFalse(foo(0, -0));
  }, foo);
})();

// Object.is does not read its arguments, so proxy traps must not fire.
(function() {
  let calls = 0;
  const p = new Proxy({}, {
    get() { calls++; throw new Error('unreachable'); },
    has() { calls++; throw new Error('unreachable'); },
    getPrototypeOf() { calls++; throw new Error('unreachable'); },
  });
  function foo(o) { return Object.is(o, o); }
  function bar(o) { return Object.is(o, {}); }
  testOptimized(() => {
    assertTrue(foo(p));
    assertFalse(bar(p));
  }, foo, bar);
  assertEquals(0, calls);
})();

// Spread calls bail out of the call-site reduction and are reduced later, on
// the CallKnownBuiltin node.
(function() {
  function foo(args) { return Object.is(...args); }
  testOptimized(() => {
    assertTrue(foo([NaN, NaN]));
    assertTrue(foo([-0, -0]));
    assertTrue(foo(['a', 'a']));
    assertFalse(foo([0, -0]));
    assertFalse(foo([1, 2]));
    assertTrue(foo([]));
  }, foo);
})();

// Replacing Object.is invalidates the reduction.
(function() {
  function foo(a, b) { return Object.is(a, b); }
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(NaN, NaN));
  %OptimizeMaglevOnNextCall(foo);
  assertTrue(foo(NaN, NaN));
  assertOptimized(foo);
  Object.is = function() { return 'replaced'; };
  assertUnoptimized(foo);
  assertEquals('replaced', foo(NaN, NaN));
})();

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// A DefineNamedOwnProperty into a const field may only keep the field const
// when the field still holds the uninitialized value, i.e. when the define is
// the initializing store of an object literal. In these tests the define is
// the initializing store, so Maglev code must not deoptimize and the field
// must stay const.

(function TestObjectLiteralStaysOptimized() {
  function make(x) {
    return {a: x};
  }

  %PrepareFunctionForOptimization(make);
  // The literal needs two executions before its feedback has a boilerplate.
  make({});
  assertTrue(%HasOwnConstDataProperty(make({}), "a"));

  %OptimizeMaglevOnNextCall(make);
  const marker = {};
  const o = make(marker);
  assertSame(marker, o.a);
  assertMaglevved(make);
  assertTrue(%HasOwnConstDataProperty(o, "a"));
})();

// Same, but the const field has Smi representation. The representation is
// determined by the first store; there is no runtime function to assert it,
// so the test relies on 1 giving a Smi field. The property name differs from
// the one above because object literals of the same shape share their map
// transitions, and the field of the transition above already has HeapObject
// representation.
(function TestObjectLiteralSmiStaysOptimized() {
  function make(x) {
    return {b: x};
  }

  %PrepareFunctionForOptimization(make);
  make(1);
  assertTrue(%HasOwnConstDataProperty(make(2), "b"));

  %OptimizeMaglevOnNextCall(make);
  const o = make(3);
  assertEquals(3, o.b);
  assertMaglevved(make);
  assertTrue(%HasOwnConstDataProperty(o, "b"));
})();

// Same, but the const field has Double representation.
(function TestObjectLiteralDoubleStaysOptimized() {
  function make(x) {
    return {c: x};
  }

  %PrepareFunctionForOptimization(make);
  make(1.5);
  assertTrue(%HasOwnConstDataProperty(make(2.5), "c"));

  %OptimizeMaglevOnNextCall(make);
  const o = make(3.5);
  assertEquals(3.5, o.c);
  assertMaglevved(make);
  assertTrue(%HasOwnConstDataProperty(o, "c"));
})();

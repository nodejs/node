// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// A DefineNamedOwnProperty into a const field may only keep the field const
// when the field still holds the uninitialized value, i.e. when the define is
// the initializing store of an object literal. An object literal with a spread
// clones the source object and then defines the remaining properties into the
// clone, so the define may hit an already initialized const field copied from
// the source. It must then generalize the field to mutable.
//
// Note that Maglev does not know the map of the cloned object, so the define
// is compiled from its own feedback, which the runtime only produces after it
// has already generalized the field. These tests therefore document the
// expected behavior; they do not exercise the uninitialized check in the
// optimized code.

(function TestSpreadDefineIntoInitializedConstField() {
  const first = {};
  const second = {};
  function make(src) {
    return {...src, a: second};
  }

  const src = {a: first};
  assertTrue(%HasOwnConstDataProperty(src, "a"));

  %PrepareFunctionForOptimization(make);
  make(src);
  make(src);
  %OptimizeMaglevOnNextCall(make);
  const o = make(src);
  assertSame(second, o.a);
  assertMaglevved(make);
  assertFalse(%HasOwnConstDataProperty(o, "a"));
  assertSame(first, src.a);
})();

// Same, but the const field has Smi representation. The representation is
// determined by the first store; there is no runtime function to assert it,
// so the test relies on 1 giving a Smi field.
(function TestSpreadDefineIntoInitializedConstSmiField() {
  function make(src) {
    return {...src, b: 2};
  }

  const src = {b: 1};
  assertTrue(%HasOwnConstDataProperty(src, "b"));

  %PrepareFunctionForOptimization(make);
  make(src);
  make(src);
  %OptimizeMaglevOnNextCall(make);
  const o = make(src);
  assertEquals(2, o.b);
  assertMaglevved(make);
  assertFalse(%HasOwnConstDataProperty(o, "b"));
  assertEquals(1, src.b);
})();

// Same, but the const field has Double representation.
(function TestSpreadDefineIntoInitializedConstDoubleField() {
  function make(src) {
    return {...src, c: 2.5};
  }

  const src = {c: 1.5};
  assertTrue(%HasOwnConstDataProperty(src, "c"));

  %PrepareFunctionForOptimization(make);
  make(src);
  make(src);
  %OptimizeMaglevOnNextCall(make);
  const o = make(src);
  assertEquals(2.5, o.c);
  assertMaglevved(make);
  assertFalse(%HasOwnConstDataProperty(o, "c"));
  assertEquals(1.5, src.c);
})();

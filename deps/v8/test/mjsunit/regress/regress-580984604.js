// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

(function TestMegamorphicDefineNamedOwnConstField() {
  const prototypes = Array.from({length: 50}, (_, i) => ({['p' + i]: i}));
  function make(prototype, value) {
    return {__proto__: prototype, value};
  }
  class Base {
    constructor(object) {
      return object;
    }
  }
  let nextValue = 1;
  class Define extends Base {
    value = nextValue;
  }

  for (let i = 0; i < 50; ++i) {
    make(prototypes[i], 1);
    new Define({['q' + i]: i});
  }

  const victim = make(prototypes[0], 1);
  function read() {
    return victim.value;
  }
  %PrepareFunctionForOptimization(read);
  assertEquals(1, read());
  %OptimizeFunctionOnNextCall(read);
  assertEquals(1, read());

  nextValue = 2;
  make(prototypes[0], 1);
  new Define(victim);

  assertFalse(%HasOwnConstDataProperty(victim, 'value'));
  assertEquals(2, victim.value);
  assertEquals(2, read());
})();

(function TestConstantTypedArrayRetentionOnTransfer() {
  const prototypes = Array.from({length: 50}, (_, i) => ({['p' + i]: i}));
  const empty = new Uint32Array(0);
  const replacement = new Uint32Array([77]);

  function make(prototype, value) {
    return {__proto__: prototype, value};
  }
  class Base {
    constructor(object) {
      return object;
    }
  }
  let nextValue = empty;
  class Define extends Base {
    value = nextValue;
  }

  for (let i = 0; i < 50; ++i) {
    make(prototypes[i], empty);
    new Define({['q' + i]: i});
  }

  const victim = make(prototypes[0], empty);
  function overwrite(value) {
    nextValue = value;
    make(prototypes[0], empty);
    new Define(victim);
  }

  const buffer = new ArrayBuffer(176);
  (() => {
    const view = new Uint32Array(buffer);
    view[0] = 133;
    overwrite(view);
  })();

  function readElem(index) {
    return victim.value[index];
  }
  %PrepareFunctionForOptimization(readElem);
  assertEquals(133, readElem(0));
  %OptimizeFunctionOnNextCall(readElem);
  assertEquals(133, readElem(0));

  overwrite(replacement);
  gc();
  buffer.transfer(0);

  assertEquals(77, readElem(0));
})();

// In-place Double -> Tagged generalization on a boilerplate map where the
// boilerplate still holds an UninitializedHeapNumber.
(function TestBoilerplateDoubleToTaggedInPlaceGeneralization() {
  function make_lit() {
    return {value: NaN};
  }
  %PrepareFunctionForOptimization(make_lit);
  assertTrue(Number.isNaN(make_lit().value));
  assertTrue(Number.isNaN(make_lit().value));

  // Generalize {value} in-place from Double to Tagged while keeping kConst.
  let other = {value: 'Y'};
  assertEquals('Y', other.value);

  %OptimizeFunctionOnNextCall(make_lit);
  let res = make_lit();
  assertTrue(Number.isNaN(res.value));
})();

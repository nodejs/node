// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// A DefineNamedOwnProperty into a const field may only keep the field const
// when the field still holds the uninitialized value, i.e. when the define is
// the initializing store of an object literal. A define into an already
// initialized const field must generalize the field to mutable.

// Class field initializer, run on an already initialized object via return
// override. The define site is megamorphic and has never seen an H; Maglev
// knows the receiver's map from the preceding load. The define must generalize
// the field.

(function TestDefineIntoInitializedConstField() {
  function H(v) { this.myProp = v; }
  const first = {};
  const second = {};
  const holder = new H(first);
  const other = new H(second);

  class Base { constructor(o) { return o; } }
  class D extends Base { myProp = second; }

  // Make the define site megamorphic without ever seeing an H.
  for (let i = 0; i < 30; i++) {
    const o = {};
    o["p" + i] = i;
    new D(o);
  }

  function define(o, constructD) {
    const t = o.myProp;  // Known maps for o: {H, literal}.
    if (constructD) new D(o);
    return t;
  }
  %PrepareFunctionForOptimization(define);
  // One call with an H to get its map into the load feedback, without
  // constructing D on it (that would generalize H.myProp in the runtime).
  define(other, false);
  // The other calls construct D, so that its call frequency stays high enough
  // for Maglev to inline it.
  for (let i = 0; i < 30; i++) define({myProp: second}, true);
  %OptimizeMaglevOnNextCall(define);
  define({myProp: second}, true);
  assertMaglevved(define);

  // Assert the good state before we begin the shenanigans.
  assertTrue(%HasOwnConstDataProperty(holder, "myProp"));

  // This call should invalidate the constness of 'myProp' in H.
  assertSame(first, define(holder, true));
  assertSame(second, holder.myProp);
  assertFalse(%HasOwnConstDataProperty(holder, "myProp"));
})();

// Same, but the const field has Smi representation. The representation is
// determined by the first store in H; there is no runtime function to assert
// it, so the test relies on 1 giving a Smi field.
(function TestDefineIntoInitializedConstSmiField() {
  function H(v) { this.myProp = v; }
  const first = 1;
  const second = 2;
  const holder = new H(first);
  const other = new H(second);

  class Base { constructor(o) { return o; } }
  class D extends Base { myProp = second; }

  // Make the define site megamorphic without ever seeing an H.
  for (let i = 0; i < 30; i++) {
    const o = {};
    o["p" + i] = i;
    new D(o);
  }

  function define(o, constructD) {
    const t = o.myProp;  // Known maps for o: {H, literal}.
    if (constructD) new D(o);
    return t;
  }
  %PrepareFunctionForOptimization(define);
  // One call with an H to get its map into the load feedback, without
  // constructing D on it (that would generalize H.myProp in the runtime).
  define(other, false);
  // The other calls construct D, so that its call frequency stays high enough
  // for Maglev to inline it.
  for (let i = 0; i < 30; i++) define({myProp: second}, true);
  %OptimizeMaglevOnNextCall(define);
  define({myProp: second}, true);
  assertMaglevved(define);

  // Assert the good state before we begin the shenanigans.
  assertTrue(%HasOwnConstDataProperty(holder, "myProp"));

  // This call should invalidate the constness of 'myProp' in H.
  assertSame(first, define(holder, true));
  assertSame(second, holder.myProp);
  assertFalse(%HasOwnConstDataProperty(holder, "myProp"));
})();

// Same, but the const field has Double representation.
(function TestDefineIntoInitializedConstDoubleField() {
  function H(v) { this.myProp = v; }
  const first = 1.5;
  const second = 2.5;
  const holder = new H(first);
  const other = new H(second);

  class Base { constructor(o) { return o; } }
  class D extends Base { myProp = second; }

  // Make the define site megamorphic without ever seeing an H.
  for (let i = 0; i < 30; i++) {
    const o = {};
    o["p" + i] = i;
    new D(o);
  }

  function define(o, constructD) {
    const t = o.myProp;  // Known maps for o: {H, literal}.
    if (constructD) new D(o);
    return t;
  }
  %PrepareFunctionForOptimization(define);
  // One call with an H to get its map into the load feedback, without
  // constructing D on it (that would generalize H.myProp in the runtime).
  define(other, false);
  // The other calls construct D, so that its call frequency stays high enough
  // for Maglev to inline it.
  for (let i = 0; i < 30; i++) define({myProp: second}, true);
  %OptimizeMaglevOnNextCall(define);
  define({myProp: second}, true);
  assertMaglevved(define);

  // Assert the good state before we begin the shenanigans.
  assertTrue(%HasOwnConstDataProperty(holder, "myProp"));

  // This call should invalidate the constness of 'myProp' in H.
  assertEquals(first, define(holder, true));
  assertEquals(second, holder.myProp);
  assertFalse(%HasOwnConstDataProperty(holder, "myProp"));
})();

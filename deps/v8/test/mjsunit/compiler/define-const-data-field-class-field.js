// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// A DefineNamedOwnProperty into a const field may only keep the field const
// when the field still holds the uninitialized value, i.e. when the define is
// the initializing store of an object literal. A define into an already
// initialized const field must generalize the field to mutable.

// Class field initializer, run on an already initialized object via return
// override.
(function TestDefineIntoInitializedConstField() {
  function H1(a, b) { this.otherProp = a; this.myProp = b;}
  function H2(a, b) { this.otherProp = a; this.myProp = b;}

  const first = {};
  const second = {};
  const holder = new H1(1, first);

  class Base { constructor(o) { return o; } }

  // This function is just used for feeding map information into the optimized function.
  function feedMap(o) {
    return o.otherProp;
  }
  %PrepareFunctionForOptimization(feedMap);

  // Warm up with both H1 and H2.
  const dummyH1 = new H1(1, second);
  const dummyH2 = new H2(1, second);
  feedMap(dummyH1);
  feedMap(dummyH2);

  class D extends Base { myProp = (feedMap(this), second); }

  function define(o) {
    new D(o);
  }
  %PrepareFunctionForOptimization(D);
  %PrepareFunctionForOptimization(define);

  // Warm up define only with H2.
  for (let i = 0; i < 20; i++) {
     define(new H2(1, second));
  }

  %OptimizeFunctionOnNextCall(define);
  define(new H2(1, second));
  assertOptimized(define);

  // Assert the good state before we begin the shenanigans.
  assertTrue(%HasOwnConstDataProperty(holder, "myProp"));

  // This call should invalidate the constness of 'myProp' in H1.
  define(holder);
  assertSame(second, holder.myProp);
  assertFalse(%HasOwnConstDataProperty(holder, "myProp"));
})();

// Same, but the const field has Smi representation.
(function TestDefineIntoInitializedConstFieldSmi() {
  function H1(a, b) { this.otherProp = a; this.myProp = b;}
  function H2(a, b) { this.otherProp = a; this.myProp = b;}

  const first = 10;
  const second = 20;
  const holder = new H1(1, first);

  class Base { constructor(o) { return o; } }

  // This function is just used for feeding map information into the optimized function.
  function feedMap(o) {
    return o.otherProp;
  }
  %PrepareFunctionForOptimization(feedMap);

  // Warm up with both H1 and H2.
  const dummyH1 = new H1(1, second);
  const dummyH2 = new H2(1, second);
  feedMap(dummyH1);
  feedMap(dummyH2);

  class D extends Base { myProp = (feedMap(this), second); }

  function define(o) {
    new D(o);
  }
  %PrepareFunctionForOptimization(D);
  %PrepareFunctionForOptimization(define);

  // Warm up define only with H2.
  for (let i = 0; i < 20; i++) {
     define(new H2(1, second));
  }

  %OptimizeFunctionOnNextCall(define);
  define(new H2(1, second));
  assertOptimized(define);

  // Assert the good state before we begin the shenanigans.
  assertTrue(%HasOwnConstDataProperty(holder, "myProp"));

  // This call should invalidate the constness of 'myProp' in H1.
  define(holder);
  assertSame(second, holder.myProp);
  assertFalse(%HasOwnConstDataProperty(holder, "myProp"));
})();

// Same, but the const field has Double representation.
(function TestDefineIntoInitializedConstFieldDouble() {
  function H1(a, b) { this.otherProp = a; this.myProp = b;}
  function H2(a, b) { this.otherProp = a; this.myProp = b;}

  const first = 1.5;
  const second = 2.5;
  const holder = new H1(1, first);

  class Base { constructor(o) { return o; } }

  // This function is just used for feeding map information into the optimized function.
  function feedMap(o) {
    return o.otherProp;
  }
  %PrepareFunctionForOptimization(feedMap);

  // Warm up with both H1 and H2.
  const dummyH1 = new H1(1, second);
  const dummyH2 = new H2(1, second);
  feedMap(dummyH1);
  feedMap(dummyH2);

  class D extends Base { myProp = (feedMap(this), second); }

  function define(o) {
    new D(o);
  }
  %PrepareFunctionForOptimization(D);
  %PrepareFunctionForOptimization(define);

  // Warm up define only with H2.
  for (let i = 0; i < 20; i++) {
     define(new H2(1, second));
  }

  %OptimizeFunctionOnNextCall(define);
  define(new H2(1, second));
  assertOptimized(define);

  // Assert the good state before we begin the shenanigans.
  assertTrue(%HasOwnConstDataProperty(holder, "myProp"));

  // This call should invalidate the constness of 'myProp' in H1.
  define(holder);
  assertSame(second, holder.myProp);
  assertFalse(%HasOwnConstDataProperty(holder, "myProp"));
})();

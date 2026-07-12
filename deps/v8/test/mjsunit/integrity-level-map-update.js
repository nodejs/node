// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function SealAndReconfigure() {
  function C() { this.x = 1; this.y = 1; Object.seal(this); }

  let c1 = new C();

  c1.x = 0.1;

  let c2 = new C();
  let c3 = new C();
  let c4 = new C();

  // The objects c2, c3 and c4 should follow the same transition
  // path that we reconfigured c1 to.
  assertTrue(%HaveSameMap(c1, c2));
  assertTrue(%HaveSameMap(c1, c3));
  assertTrue(%HaveSameMap(c1, c4));

  c2.x = 0.1;
  c3.x = 0.1;
  c4.x = 0.1;

  assertTrue(%HaveSameMap(c1, c2));
  assertTrue(%HaveSameMap(c1, c3));
  assertTrue(%HaveSameMap(c1, c4));
})();

(function SealAndReconfigureWithIC() {
  function C() { this.x = 1; this.y = 1; Object.seal(this); }

  let c1 = new C();

  function g(o) {
    o.x = 0.1;
  }

  g(c1);

  let c2 = new C();
  let c3 = new C();
  let c4 = new C();

  // The objects c2, c3 and c4 should follow the same transition
  // path that we reconfigured c1 to.
  assertTrue(%HaveSameMap(c1, c2));
  assertTrue(%HaveSameMap(c1, c3));
  assertTrue(%HaveSameMap(c1, c4));

  g(c2);
  g(c3);
  g(c4);

  assertTrue(%HaveSameMap(c1, c2));
  assertTrue(%HaveSameMap(c1, c3));
  assertTrue(%HaveSameMap(c1, c4));
})();

(function SealReconfigureAndMigrateWithIC() {
  function C() { this.x = 1; this.y = 1; Object.seal(this); }

  let c1 = new C();
  let c2 = new C();
  let c3 = new C();
  let c4 = new C();

  function g(o) {
    o.x = 0.1;
  }

  g(c1);

  // Now c2, c3 and c4 have deprecated maps.
  assertFalse(%HaveSameMap(c1, c2));
  assertFalse(%HaveSameMap(c1, c3));
  assertFalse(%HaveSameMap(c1, c4));

  g(c2);
  g(c3);
  g(c4);

  assertTrue(%HaveSameMap(c1, c2));
  assertTrue(%HaveSameMap(c1, c3));
  assertTrue(%HaveSameMap(c1, c4));
})();

(function SealReconfigureAndMigrateWithOptCode() {
  function C() { this.x = 1; this.y = 1; Object.seal(this); }

  let c1 = new C();
  let c2 = new C();
  let c3 = new C();
  let c4 = new C();

  function g(o) {
    o.x = 0.1;
  }

  %PrepareFunctionForOptimization(g);
  g(c1);
  g(c2);
  g(c3);
  %OptimizeFunctionOnNextCall(g);
  g(c4);

  assertTrue(%HaveSameMap(c1, c2));
  assertTrue(%HaveSameMap(c1, c3));
  assertTrue(%HaveSameMap(c1, c4));
})();

(function PreventExtensionsAndReconfigure() {
  function C() { this.x = 1; this.y = 1; Object.preventExtensions(this); }

  let c1 = new C();

  function g(o) {
    o.x = 0.1;
  }

  g(c1);

  let c2 = new C();
  let c3 = new C();
  let c4 = new C();

  c2.x = 0.1;
  c3.x = 0.1;
  c4.x = 0.1;

  assertTrue(%HaveSameMap(c1, c2));
  assertTrue(%HaveSameMap(c1, c3));
  assertTrue(%HaveSameMap(c1, c4));
})();

(function PreventExtensionsSealAndReconfigure() {
  function C() {
    this.x = 1;
    this.y = 1;
    Object.preventExtensions(this);
    Object.seal(this);
  }

  let c1 = new C();

  function g(o) {
    o.x = 0.1;
  }

  g(c1);

  let c2 = new C();
  let c3 = new C();
  let c4 = new C();

  c2.x = 0.1;
  c3.x = 0.1;
  c4.x = 0.1;

  // Ideally, all the objects would have the same map, but at the moment
  // we shortcut the unnecessary integrity level transitions.
  assertTrue(%HaveSameMap(c2, c3));
  assertTrue(%HaveSameMap(c2, c4));
})();

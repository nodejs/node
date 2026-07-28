// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
// Flags: --no-stress-opt --no-stress-incremental-marking

// ResolvePromise is strength-reduced to a direct fulfillment when the
// resolution provably has no "then" property (CanElideResolvePromiseThenLookup).

(async () => {
  // Map-inference path: maps known and fresh from a preceding property access.
  {
    async function foo(o) {
      o.x;
      return o;
    }
    %PrepareFunctionForOptimization(foo);
    const obj = {x: 1, y: 2};
    assertEquals(obj, await foo(obj));
    assertEquals(obj, await foo(obj));
    %OptimizeMaglevOnNextCall(foo);
    assertEquals(obj, await foo(obj));
    assertTrue(isMaglevved(foo));
  }

  // Heap-constant path: a context constant, which map inference doesn't reach.
  // The reduction takes a stable-map dependency, so destabilizing it must deopt.
  {
    function make() {
      const a = {p: 1, q: 1};
      async function foo() {
        return a;
      }
      return {foo, a};
    }
    const {foo, a} = make();
    %PrepareFunctionForOptimization(foo);
    assertEquals(a, await foo());
    assertEquals(a, await foo());
    %OptimizeMaglevOnNextCall(foo);
    assertEquals(a, await foo());
    assertTrue(isMaglevved(foo));

    // Destabilize the resolution's map via a sibling sharing the initial map.
    const {a: sibling} = make();
    sibling.r = 1;

    if (!%IsDictPropertyConstTrackingEnabled()) {
      assertUnoptimized(foo);
    }
  }

  // A thenable must not be reduced: chaining runs, so we observe 42, not it.
  {
    async function foo(t) {
      t.marker;
      return t;
    }
    %PrepareFunctionForOptimization(foo);
    const thenable = {marker: 1, then(resolve) { resolve(42); }};
    assertEquals(42, await foo(thenable));
    assertEquals(42, await foo(thenable));
    %OptimizeMaglevOnNextCall(foo);
    assertEquals(42, await foo(thenable));
    assertTrue(isMaglevved(foo));
  }

  // Self-resolution: a promise has "then", so the reduction never fires and the
  // ResolvePromise cycle check still rejects with TypeError.
  {
    let outer;
    async function f() {
      await 0;
      return outer;
    }
    %PrepareFunctionForOptimization(f);
    function drive() {
      outer = f();
      return outer;
    }
    await assertThrowsAsync(drive(), TypeError);
    %OptimizeMaglevOnNextCall(f);
    await assertThrowsAsync(drive(), TypeError);
    assertTrue(isMaglevved(f));
  }

  // Self-resolution with a mutated prototype chain: the resolution has no
  // "then", but the cycle check is identity-based and must still reject.
  // The proto object is hoisted so the mutated promises share a map.
  {
    const proto = {constructor: Promise, marker: 1};
    let outer;
    async function f() {
      await 0;
      outer.marker;  // Establish fresh map knowledge for the resolution.
      return outer;
    }
    %PrepareFunctionForOptimization(f);
    function drive() {
      outer = f();
      Object.setPrototypeOf(outer, proto);
      return outer;
    }
    function settled(p) {
      return new Promise((resolve) => {
        // p's own "then" is gone; go through Promise.prototype directly.
        Promise.prototype.then.call(
            p, (v) => resolve({status: "fulfilled", value: v}),
            (e) => resolve({status: "rejected", error: e}));
      });
    }
    for (let i = 0; i < 3; i++) {
      const r = await settled(drive());
      assertEquals("rejected", r.status);
      assertInstanceof(r.error, TypeError);
    }
    %OptimizeMaglevOnNextCall(f);
    const r = await settled(drive());
    assertEquals("rejected", r.status);
    assertInstanceof(r.error, TypeError);
    assertTrue(isMaglevved(f));
  }
})();

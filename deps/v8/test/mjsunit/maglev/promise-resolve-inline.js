// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
// Flags: --no-stress-opt --no-stress-incremental-marking

// Promise.resolve with a provably non-thenable value is reduced to an
// inline allocation of an already-fulfilled promise.

(async () => {
  // Primitive values.
  {
    function foo(v) {
      return Promise.resolve(v);
    }
    %PrepareFunctionForOptimization(foo);
    assertEquals(42, await foo(42));
    assertEquals("s", await foo("s"));
    %OptimizeMaglevOnNextCall(foo);
    assertEquals(42, await foo(42));
    assertEquals(undefined, await Promise.resolve());
    assertTrue(isMaglevved(foo));
    const p = foo(42);
    assertInstanceof(p, Promise);
    assertEquals(42, await p);
  }

  // Map-inference path: maps known and fresh from a preceding access.
  {
    function foo(o) {
      o.x;
      return Promise.resolve(o);
    }
    %PrepareFunctionForOptimization(foo);
    const obj = {x: 1, y: 2};
    assertEquals(obj, await foo(obj));
    assertEquals(obj, await foo(obj));
    %OptimizeMaglevOnNextCall(foo);
    assertEquals(obj, await foo(obj));
    assertTrue(isMaglevved(foo));
  }

  // A thenable must not be reduced: chaining runs, so we observe 42.
  {
    function foo(t) {
      t.marker;
      return Promise.resolve(t);
    }
    %PrepareFunctionForOptimization(foo);
    const thenable = {marker: 1, then(resolve) { resolve(42); }};
    assertEquals(42, await foo(thenable));
    assertEquals(42, await foo(thenable));
    %OptimizeMaglevOnNextCall(foo);
    assertEquals(42, await foo(thenable));
    assertTrue(isMaglevved(foo));
  }

  // A promise value keeps its identity (promise maps have "then", so the
  // reduction never fires and the builtin returns the input unchanged).
  {
    function foo(p) {
      return Promise.resolve(p);
    }
    %PrepareFunctionForOptimization(foo);
    const p = Promise.resolve(1);
    assertSame(p, foo(p));
    assertSame(p, foo(p));
    %OptimizeMaglevOnNextCall(foo);
    assertSame(p, foo(p));
    assertTrue(isMaglevved(foo));
  }

  // A promise whose prototype chain was mutated to lack "then" but keep
  // constructor === Promise must still be returned by identity
  // (PromiseResolve step 1 checks the [[PromiseState]] slot, not "then").
  {
    function foo(x) {
      x.marker;
      return Promise.resolve(x);
    }
    %PrepareFunctionForOptimization(foo);
    const weird = Promise.resolve(1);
    Object.setPrototypeOf(weird, {constructor: Promise, marker: 1});
    assertSame(weird, foo(weird));
    assertSame(weird, foo(weird));
    %OptimizeMaglevOnNextCall(foo);
    assertSame(weird, foo(weird));
    assertTrue(isMaglevved(foo));
  }

  // Subclass receivers are not reduced: the species-less fast path only
  // applies to %Promise% itself.
  {
    class MyPromise extends Promise {}
    function foo(v) {
      return MyPromise.resolve(v);
    }
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(1), MyPromise);
    assertInstanceof(foo(2), MyPromise);
    %OptimizeMaglevOnNextCall(foo);
    assertInstanceof(foo(3), MyPromise);
    assertEquals(3, await foo(3));
    assertTrue(isMaglevved(foo));
  }
})();

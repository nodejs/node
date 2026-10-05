// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev
// Flags: --no-stress-opt --no-stress-incremental-marking

// Promise.prototype.catch on a JSPromise with the initial Promise.prototype and
// callable-or-undefined reaction reduces to an inlined result-promise
// allocation plus a direct PerformPromiseThen call; rejection paths,
// non-callable reactions, and subclassed receivers must keep their semantics.

(async () => {
  // 1. Basic reduction on fulfilled and rejected promises.
  {
    async function catchTest(p) {
      return p.catch(e => -e);
    }
    %PrepareFunctionForOptimization(catchTest);
    assertEquals(42, await catchTest(Promise.resolve(42)));
    assertEquals(-5, await catchTest(Promise.reject(5)));
    %OptimizeMaglevOnNextCall(catchTest);
    assertEquals(42, await catchTest(Promise.resolve(42)));
    assertEquals(-5, await catchTest(Promise.reject(5)));
    assertTrue(isMaglevved(catchTest));
  }

  // 2. Non-callable reaction behaves like identity.
  {
    async function nonCallableCatch(p) {
      return p.catch(123);
    }
    %PrepareFunctionForOptimization(nonCallableCatch);
    assertEquals(10, await nonCallableCatch(Promise.resolve(10)));
    assertEquals(10, await nonCallableCatch(Promise.resolve(10)));
    %OptimizeMaglevOnNextCall(nonCallableCatch);
    assertEquals(10, await nonCallableCatch(Promise.resolve(10)));
    assertTrue(isMaglevved(nonCallableCatch));
  }

  // 3. Subclassed promise receiver.
  {
    class MyPromise extends Promise {}
    function subCatch(p) {
      return p.catch(e => -e);
    }
    %PrepareFunctionForOptimization(subCatch);
    const p1 = subCatch(MyPromise.reject(2));
    assertInstanceof(p1, MyPromise);
    assertEquals(-2, await p1);

    %OptimizeMaglevOnNextCall(subCatch);
    const p2 = subCatch(MyPromise.reject(3));
    assertInstanceof(p2, MyPromise);
    assertEquals(-3, await p2);
    assertTrue(isMaglevved(subCatch));
  }

  // 4. Chained catch handler.
  {
    async function chainedCatch(p) {
      return p.catch(e => e + 1).catch(e => e * 2);
    }
    %PrepareFunctionForOptimization(chainedCatch);
    assertEquals(10, await chainedCatch(Promise.resolve(10)));
    assertEquals(6, await chainedCatch(Promise.reject(5)));
    %OptimizeMaglevOnNextCall(chainedCatch);
    assertEquals(10, await chainedCatch(Promise.resolve(10)));
    assertEquals(6, await chainedCatch(Promise.reject(5)));
    assertTrue(isMaglevved(chainedCatch));
  }

  // 5. Catch with zero arguments (on_rejected defaults to undefined).
  {
    async function zeroArgsCatch(p) {
      return p.catch();
    }
    %PrepareFunctionForOptimization(zeroArgsCatch);
    assertEquals(42, await zeroArgsCatch(Promise.resolve(42)));
    let caught = false;
    try {
      await zeroArgsCatch(Promise.reject(5));
    } catch (e) {
      caught = true;
      assertEquals(5, e);
    }
    assertTrue(caught);

    %OptimizeMaglevOnNextCall(zeroArgsCatch);
    assertEquals(42, await zeroArgsCatch(Promise.resolve(42)));
    caught = false;
    try {
      await zeroArgsCatch(Promise.reject(5));
    } catch (e) {
      caught = true;
      assertEquals(5, e);
    }
    assertTrue(caught);
    assertTrue(isMaglevved(zeroArgsCatch));
  }

  // 6. Catch with extra arguments (extra arguments are ignored).
  {
    async function extraArgsCatch(p) {
      return p.catch(e => -e, () => 'extra_ignored', 123);
    }
    %PrepareFunctionForOptimization(extraArgsCatch);
    assertEquals(42, await extraArgsCatch(Promise.resolve(42)));
    assertEquals(-5, await extraArgsCatch(Promise.reject(5)));
    %OptimizeMaglevOnNextCall(extraArgsCatch);
    assertEquals(42, await extraArgsCatch(Promise.resolve(42)));
    assertEquals(-5, await extraArgsCatch(Promise.reject(5)));
    assertTrue(isMaglevved(extraArgsCatch));
  }

  // 7. Spread call p.catch(...args) (skips reduction and falls back to generic call).
  {
    async function spreadCatch(p, args) {
      return p.catch(...args);
    }
    %PrepareFunctionForOptimization(spreadCatch);
    assertEquals(42, await spreadCatch(Promise.resolve(42), [e => -e]));
    assertEquals(-5, await spreadCatch(Promise.reject(5), [e => -e]));
    assertEquals(42, await spreadCatch(Promise.resolve(42), []));
    %OptimizeMaglevOnNextCall(spreadCatch);
    assertEquals(42, await spreadCatch(Promise.resolve(42), [e => -e]));
    assertEquals(-5, await spreadCatch(Promise.reject(5), [e => -e]));
    assertEquals(42, await spreadCatch(Promise.resolve(42), []));
    assertTrue(isMaglevved(spreadCatch));
  }

  // 8. Reduction registers a dependency on PromiseThenProtector.
  // When Promise.prototype.then is modified, the protector is invalidated,
  // the optimized code must deoptimize, and re-optimization must not re-reduce.
  {
    function testThenProtector(p) {
      return p.catch(x => x);
    }
    const p = Promise.resolve(1);
    %PrepareFunctionForOptimization(testThenProtector);
    testThenProtector(p);
    testThenProtector(p);
    %OptimizeMaglevOnNextCall(testThenProtector);
    testThenProtector(p);
    assertTrue(isMaglevved(testThenProtector));

    // Invalidate PromiseThenProtector.
    Promise.prototype.then = function() {
      return 'patched';
    };
    assertUnoptimized(testThenProtector);
    assertEquals('patched', testThenProtector(p));

    // Re-optimization must refuse reduction and continue to invoke patched .then.
    %PrepareFunctionForOptimization(testThenProtector);
    testThenProtector(p);
    testThenProtector(p);
    %OptimizeMaglevOnNextCall(testThenProtector);
    assertEquals('patched', testThenProtector(p));
    assertTrue(isMaglevved(testThenProtector));
  }
})();

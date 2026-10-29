// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax

// Verifies that pre-allocated await closures survive GC and work correctly
// across multiple await expressions within an async function.

function assertEqualsAsync(expected, run) {
  var actual;
  var hadValue = false;
  var hadError = false;
  var promise = run();

  promise.then(function(value) { hadValue = true; actual = value; },
               function(error) { hadError = true; actual = error; });

  assertFalse(hadValue || hadError);

  %PerformMicrotaskCheckpoint();

  if (hadError) throw actual;

  assertTrue(hadValue, "Expected '" + run.toString() + "' to produce a value");
  assertEquals(expected, actual);
}

// Closures must survive GC between await expressions.
(function TestMultipleAwaits() {
  async function multiAwait(a, b, c) {
    let x = await Promise.resolve(a);
    gc();
    let y = await Promise.resolve(b);
    gc();
    let z = await Promise.resolve(c);
    return x + y + z;
  }

  assertEqualsAsync(6, () => multiAwait(1, 2, 3));
  assertEqualsAsync(15, () => multiAwait(4, 5, 6));
})();

// Fields must be initialized before allocations that could trigger GC.
(function TestGCDuringCreation() {
  for (let i = 0; i < 100; i++) {
    gc();
    async function localAsync() {
      return await Promise.resolve(i);
    }
    assertEqualsAsync(i, () => localAsync());
  }
})();

// Each async function instance needs its own closures.
(function TestNestedAsync() {
  async function inner(x) {
    gc();
    return await Promise.resolve(x * 2);
  }

  async function outer(x) {
    let a = await inner(x);
    gc();
    let b = await inner(a);
    gc();
    let c = await inner(b);
    return c;
  }

  assertEqualsAsync(8, () => outer(1));
  assertEqualsAsync(16, () => outer(2));
})();

// Closures must handle arbitrary numbers of await expressions.
(function TestManyAwaits() {
  async function manyAwaits() {
    let result = 0;
    for (let i = 0; i < 10; i++) {
      result += await Promise.resolve(i);
      if (i % 3 === 0) gc();
    }
    return result;  // 0+1+2+3+4+5+6+7+8+9 = 45
  }

  assertEqualsAsync(45, () => manyAwaits());
})();

// Reject closures must also be reused correctly.
(function TestRejectClosureReuse() {
  async function withRejections() {
    let result = 0;
    for (let i = 0; i < 5; i++) {
      try {
        await Promise.reject(new Error("error" + i));
      } catch (e) {
        result += i;
        gc();
      }
    }
    return result;  // 0+1+2+3+4 = 10
  }

  assertEqualsAsync(10, () => withRejections());
})();

// Concurrent async functions must not share closures.
(function TestConcurrentAsync() {
  async function asyncA(x) {
    gc();
    let a = await Promise.resolve(x);
    gc();
    let b = await Promise.resolve(a + 1);
    return b;
  }

  async function asyncB(x) {
    gc();
    let a = await Promise.resolve(x * 2);
    gc();
    let b = await Promise.resolve(a * 2);
    return b;
  }

  let promiseA = asyncA(5);
  let promiseB = asyncB(3);

  let resultA, resultB;
  promiseA.then(v => resultA = v);
  promiseB.then(v => resultB = v);

  %PerformMicrotaskCheckpoint();

  assertEquals(6, resultA);   // 5 + 1
  assertEquals(12, resultB);  // 3 * 2 * 2
})();

// Pre-allocated closures must not break functions without awaits.
(function TestNoAwait() {
  async function noAwait(x) {
    gc();
    return x + 1;
  }

  assertEqualsAsync(43, () => noAwait(42));
})();

// Baseline case: single await should work correctly.
(function TestSingleAwait() {
  async function singleAwait(x) {
    gc();
    return await Promise.resolve(x * 2);
  }

  assertEqualsAsync(84, () => singleAwait(42));
})();

// Stress test: many concurrent async function instances.
(function TestStressGC() {
  let results = [];
  let promises = [];

  for (let i = 0; i < 50; i++) {
    async function stress() {
      let a = await Promise.resolve(i);
      gc();
      let b = await Promise.resolve(a + 1);
      return b;
    }
    promises.push(stress().then(v => results.push(v)));
  }

  %PerformMicrotaskCheckpoint();

  assertEquals(50, results.length);
  for (let i = 0; i < 50; i++) {
    assertTrue(results.includes(i + 1), "Missing result " + (i + 1));
  }
})();

// Async arrow functions use the same optimization path.
(function TestAsyncArrow() {
  const arrowAsync = async (x) => {
    gc();
    let a = await Promise.resolve(x);
    gc();
    let b = await Promise.resolve(a + 10);
    return b;
  };

  assertEqualsAsync(15, () => arrowAsync(5));
})();

// Async methods must each have their own closures.
(function TestAsyncMethod() {
  const obj = {
    async method(x) {
      gc();
      let a = await Promise.resolve(x);
      gc();
      let b = await Promise.resolve(a * 3);
      return b;
    }
  };

  assertEqualsAsync(21, () => obj.method(7));
})();

// Class methods work the same as object methods.
(function TestAsyncClassMethod() {
  class MyClass {
    constructor(multiplier) {
      this.multiplier = multiplier;
    }

    async compute(x) {
      gc();
      let a = await Promise.resolve(x);
      gc();
      let b = await Promise.resolve(a * this.multiplier);
      return b;
    }
  }

  const instance = new MyClass(5);
  assertEqualsAsync(50, () => instance.compute(10));
})();

print("All async closure reuse tests passed!");

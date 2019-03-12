// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --async-stack-traces

// Basic test with an explicit throw.
(function() {
  async function one(x) {
    await two(x);
  }

  async function two(x) {
    await x;
    throw new Error();
  }

  async function test(f) {
    try {
      await f(1);
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async () => {
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
  })());
})();

// Basic test with an implicit throw (via ToNumber on Symbol).
(function() {
  async function one(x) {
    return await two(x);
  }

  async function two(x) {
    await x;
    return +x;  // This will raise a TypeError.
  }

  async function test(f) {
    try {
      await f(Symbol());
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, TypeError);
      assertMatches(/TypeError.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async() => {
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
  })());
})();

// Basic test with throw in inlined function.
(function() {
  function throwError() {
    throw new Error();
  }

  async function one(x) {
    return await two(x);
  }

  async function two(x) {
    await x;
    return throwError();
  }

  async function test(f) {
    try {
      await f(1);
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async() => {
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
  })());
})();

// Basic test with async function inlined into sync function.
(function() {
  function callOne(x) {
    return one(x);
  }

  function callTwo(x) {
    return two(x);
  }

  async function one(x) {
    return await callTwo(x);
  }

  async function two(x) {
    await x;
    throw new Error();
  }

  async function test(f) {
    try {
      await f(1);
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async() => {
    await test(callOne);
    await test(callOne);
    %OptimizeFunctionOnNextCall(callTwo);
    await test(callOne);
    %OptimizeFunctionOnNextCall(callOne);
    await test(callOne);
  })());
})();

// Basic test with async functions and promises chained via
// Promise.prototype.then(), which should still work following
// the generic chain upwards.
(function() {
  async function one(x) {
    return await two(x).then(x => x);
  }

  async function two(x) {
    await x.then(x => x);
    throw new Error();
  }

  async function test(f) {
    try {
      await f(Promise.resolve(1));
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async() => {
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
  })());
})();

// Basic test for async generators called from async
// functions with an explicit throw.
(function() {
  async function one(x) {
    for await (const y of two(x)) {}
  }

  async function* two(x) {
    await x;
    throw new Error();
  }

  async function test(f) {
    try {
      await f(1);
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async () => {
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
  })());
})();

// Basic test for async functions called from async
// generators with an explicit throw.
(function() {
  async function* one(x) {
    await two(x);
  }

  async function two(x) {
    await x;
    throw new Error();
  }

  async function test(f) {
    try {
      for await (const x of f(1)) {}
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async () => {
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
  })());
})();

// Basic test for async functions called from async
// generators with an explicit throw (with yield).
(function() {
  async function* one(x) {
    yield two(x);
  }

  async function two(x) {
    await x;
    throw new Error();
  }

  async function test(f) {
    try {
      for await (const x of f(1)) {}
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async () => {
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
  })());
})();

// Basic test to check that we also follow initial
// promise chains created via Promise#then().
(function() {
  async function one(p) {
    return await p.then(two);
  }

  function two() {
    throw new Error();
  }

  async function test(f) {
    try {
      await f(Promise.resolve());
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async () => {
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
  })());
})();

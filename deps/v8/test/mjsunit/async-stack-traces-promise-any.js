// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --async-stack-traces

// Basic test with Promise.any().
(function() {
  async function fine() { }

  async function thrower() {
    await fine();
    throw new Error();
  }

  async function driver() {
    await Promise.any([thrower(), thrower()]);
  }

  async function test(f) {
    try {
      await f();
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, AggregateError);
      assertEquals(2, e.errors.length);
      assertMatches(/Error.+at thrower.+at async Promise.any \(index 0\).+at async driver.+at async test/ms, e.errors[0].stack);
      assertMatches(/Error.+at thrower.+at async Promise.any \(index 1\).+at async driver.+at async test/ms, e.errors[1].stack);
    }
  }

  assertPromiseResult((async () => {
    %PrepareFunctionForOptimization(thrower);
    %PrepareFunctionForOptimization(driver);
    await test(driver);
    await test(driver);
    %OptimizeFunctionOnNextCall(thrower);
    await test(driver);
    %OptimizeFunctionOnNextCall(driver);
    await test(driver);
  })());
})();

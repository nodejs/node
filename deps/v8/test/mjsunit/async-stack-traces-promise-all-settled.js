// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --async-stack-traces

// Basic test with Promise.allSettled().
(function() {
  async function fine() { }

  async function thrower() {
    await fine();
    throw new Error();
  }

  async function driver() {
    return await Promise.allSettled([fine(), fine(), thrower(), thrower()]);
  }

  async function test(f) {
    const results = await f();
    results.forEach((result, i) => {
      if (result.status === 'rejected') {
        const error = result.reason;
        assertInstanceof(error, Error);
        const stackRegexp = new RegExp("Error.+at thrower.+at " +
          `async Promise.allSettled \\(index ${ i }\\)` +
          ".+ at async driver.+at async test",
          "ms")
        assertMatches(stackRegexp, error.stack);
      }
    });
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

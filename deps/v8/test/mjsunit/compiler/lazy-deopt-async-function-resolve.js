// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test that the lazy deoptimization point for JSAsyncFunctionResolve
// works correctly, aka that we return the promise and not the result
// of the JSResolvePromise operation.
(function() {
  async function foo(x) {
    return x;
  }

  assertPromiseResult((async () => {
    await foo(1);
    await foo(2);
    %OptimizeFunctionOnNextCall(foo);
    const p = new Proxy({}, {
      get(...args) {
        %DeoptimizeFunction(foo);
        return Reflect.get(...args);
      }
    });
    assertEquals(p, await foo(p));
  })());
})();

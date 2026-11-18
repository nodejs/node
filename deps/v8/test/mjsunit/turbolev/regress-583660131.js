// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-turbolev-non-eager-inlining

async function test() {
  async function foo(n, obj) {
    let outerVar = 40;
    outerVar += 2;
    // The resumable `await` loop clears KnownNodeAspects across the suspend
    // merge after hoisting `inner`'s JSFunction allocation at function entry.
    for (let i = 0; i < n; i++) {
      await i;
    }
    function inner() {
      let ref = inner;
      // Loop inside `inner` triggers loop-header state cloning while `inner`'s
      // context references the hoisted `inner` InlinedAllocation via `ref`.
      for (let k = 0; k >> 100; k++) {}
      function leaf() {
        if (!leaf || !ref) return 0;
        return obj.x + outerVar;
      }
      return leaf();
    }
    return inner();
  }

  %PrepareFunctionForOptimization(foo);
  // Warm up past --invocation-count-for-feedback-allocation (default 8) so the
  // nested closures `inner` and `leaf` allocate feedback vectors and get
  // eagerly inlined into `foo`.
  for (let i = 0; i < 15; i++) {
    assertEquals(43, await foo(0, {x: 1}));
  }
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(43, await foo(0, {x: 1}));
  assertEquals(52, await foo(0, {y: 0, x: 10}));
}

assertPromiseResult(test());

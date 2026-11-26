// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const matcher = {
  [Symbol.hasInstance]: Reflect.construct.bind(null, Uint8Array),
};

function matches(input) {
  return arguments instanceof matcher;
}

(function TestInstanceOfBoundReflectConstructTypedArray() {
  let calls = 0;
  function test() {
    const input = {
      get length() {
        calls++;
        return 1;
      },
    };
    return matches(input);
  }

  %PrepareFunctionForOptimization(matches);
  %PrepareFunctionForOptimization(test);
  assertTrue(test());
  assertEquals(1, calls);

  %OptimizeFunctionOnNextCall(test);
  calls = 0;
  assertTrue(test());
  assertEquals(1, calls);
})();

(function TestInstanceOfBoundReflectConstructTypedArrayThrows() {
  let calls = 0;
  function test(should_throw) {
    const input = {
      get length() {
        calls++;
        if (should_throw) throw new Error('boom');
        return 1;
      },
    };
    try {
      return matches(input);
    } catch (e) {
      return false;
    }
  }

  %PrepareFunctionForOptimization(matches);
  %PrepareFunctionForOptimization(test);
  assertTrue(test(false));
  assertEquals(1, calls);
  assertFalse(test(true));
  assertEquals(2, calls);

  %OptimizeFunctionOnNextCall(test);
  calls = 0;
  assertTrue(test(false));
  assertEquals(1, calls);
  assertFalse(test(true));
  assertEquals(2, calls);
})();

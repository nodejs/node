// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-sum-precise --allow-natives-syntax

function assertSum(expected, iterable) {
  let res = Math.sumPrecise(iterable);
  assertEquals(expected, res);
}

// Overriding TypedArray.prototype[Symbol.iterator]
(function TestOverriddenTypedArrayIterator() {
  let ta = new Uint8Array([1, 2, 3]);
  let originalIter = Uint8Array.prototype[Symbol.iterator];
  try {
    Uint8Array.prototype[Symbol.iterator] = function() {
      let i = 0;
      return {
        next() {
          if (i++ < 1) return { value: 100, done: false };
          return { done: true };
        }
      };
    };
    // Should verify that the fast path (if any) respects the override
    assertSum(100, ta);
  } finally {
    Uint8Array.prototype[Symbol.iterator] = originalIter;
  }
})();

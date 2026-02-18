// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-sum-precise --allow-natives-syntax

function assertSum(expected, iterable) {
  let res = Math.sumPrecise(iterable);
  assertEquals(expected, res);
}

// Modified TypedArray Iterator
(function TestModifiedTypedArrayIteratorFromIterableForeach() {
  let ta = new Uint8Array([1, 2]);
  const originalIter = Uint8Array.prototype[Symbol.iterator];
  Uint8Array.prototype[Symbol.iterator] = function() {
    let i = 0;
    return {
      next() {
        if (i < 1) {
            i++;
            return { value: 99, done: false };
        }
        return { done: true };
      }
    };
  };

  try {
    // Should fall back to slow path and use modified iterator
    assertSum(99, ta);
  } finally {
    Uint8Array.prototype[Symbol.iterator] = originalIter;
  }
})();

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-sum-precise --allow-natives-syntax

function assertSum(expected, iterable) {
  let res = Math.sumPrecise(iterable);
  assertEquals(expected, res);
}

// Modified Array Iterator -> Slow path
(function TestModifiedArrayIteratorFromIterableForeach() {
  let arr = [1, 2];

  // Save original
  const originalIter = Array.prototype[Symbol.iterator];

  // Modify
  Array.prototype[Symbol.iterator] = function() {
    let i = 0;
    return {
      next() {
        if (i < 1) {
            i++;
            return { value: 99, done: false }; // Inject 99
        }
        return { done: true };
      }
    };
  };

  try {
    assertSum(99, arr);
  } finally {
    // Restore
    Array.prototype[Symbol.iterator] = originalIter;
  }
})();

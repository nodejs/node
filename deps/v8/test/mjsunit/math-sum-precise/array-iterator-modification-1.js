// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-sum-precise --allow-natives-syntax

function assertSum(expected, iterable) {
  let res = Math.sumPrecise(iterable);
  assertEquals(expected, res);
}

// Overriding Array Iterator (Global pollution check)
(function TestOverriddenArrayIterator() {
  let x = [1, 2, 3];
  let originalIter = Array.prototype[Symbol.iterator];
  try {
    Array.prototype[Symbol.iterator] = function() {
      let done = false;
      return {
        next() {
          if (!done) {
            done = true;
            return { value: 42, done: false };
          } else {
            return { value: undefined, done: true };
          }
        }
      };
    };
    assertSum(42, x);
  } finally {
    Array.prototype[Symbol.iterator] = originalIter;
  }
})();

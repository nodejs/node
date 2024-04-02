// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function modifyNext() {
  'use strict';

  var a = [];
  var ai = a[Symbol.iterator]();

  var original_next = ai.__proto__['next'];

  function maxWithZero(...args) {
    return Math.max(0, ...args);
  }

  function testMax(x, y) {
    return maxWithZero(x, y);
  }

  %PrepareFunctionForOptimization(testMax);
  testMax(1, 2);
  testMax(1, 2);
  %OptimizeFunctionOnNextCall(testMax);
  var r = testMax(1, 2);

  assertEquals(2, r);

  var called = 0;
  Object.defineProperty(ai.__proto__, 'next', {
    get: function() {
      called++;
      return original_next;
    }
  });

  var r2 = testMax(1, 2);

  // .next() is only loaded once during the iteration prologue (see
  // https://github.com/tc39/ecma262/pull/988/ and v8:6861)
  assertEquals(1, called);
  assertEquals(2, r2);
})();

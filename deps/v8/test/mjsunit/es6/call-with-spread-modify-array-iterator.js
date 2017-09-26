// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function modifyArrayIterator() {
  'use strict';

  function maxWithZero(...args) {
    return Math.max(0, ...args);
  }

  function testMax(x, y) {
    return maxWithZero(x, y);
  }

  testMax(1, 2);
  testMax(1, 2);
  %OptimizeFunctionOnNextCall(testMax);
  var r = testMax(1, 2);

  assertEquals(2, r);

  Object.defineProperty(Array.prototype, Symbol.iterator, {
    value: function*
        () {
          yield 3;
          yield 4;
        },
    configurable: true
  });

  var r2 = testMax(1, 2);

  assertEquals(4, r2);
})();

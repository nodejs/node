// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function modifyArrayIterator() {
  'use strict';

  class Point {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
  }

  class RestPoint extends Point {
    constructor(...args) {
      super(...args);
    }
  }

  function testRestPoint(x, y) {
    return new RestPoint(x, y);
  }
  %PrepareFunctionForOptimization(testRestPoint);
  testRestPoint(1, 2);
  testRestPoint(1, 2);
  %OptimizeFunctionOnNextCall(testRestPoint);
  var r = testRestPoint(1, 2);

  assertInstanceof(r, RestPoint);
  assertInstanceof(r, Point);
  assertEquals(1, r.x);
  assertEquals(2, r.y);

  Object.defineProperty(Array.prototype, Symbol.iterator, {
    value: function*
        () {
          yield 3;
          yield 4;
        },
    configurable: true
  });

  var r2 = testRestPoint(1, 2);

  assertInstanceof(r2, RestPoint);
  assertInstanceof(r2, Point);
  assertEquals(3, r2.x);
  assertEquals(4, r2.y);
})();

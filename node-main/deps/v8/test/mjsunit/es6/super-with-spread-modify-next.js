// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function modifyNext() {
  'use strict';

  class Point {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
  }

  class ArgumentsPoint extends Point {
    constructor() {
      super(...arguments);
    }
  }

  var a = [];
  var ai = a[Symbol.iterator]();

  var original_next = ai.__proto__['next'];

  function testArgumentsPoint(x, y) {
    return new ArgumentsPoint(x, y);
  }
  %PrepareFunctionForOptimization(testArgumentsPoint);
  testArgumentsPoint(1, 2);
  testArgumentsPoint(1, 2);
  %OptimizeFunctionOnNextCall(testArgumentsPoint);
  var r = testArgumentsPoint(1, 2);

  assertInstanceof(r, ArgumentsPoint);
  assertInstanceof(r, Point);
  assertEquals(r.x, 1);
  assertEquals(r.y, 2);

  var called = 0;
  Object.defineProperty(ai.__proto__, 'next', {
    get: function() {
      called++;
      return original_next;
    }
  });

  var r2 = testArgumentsPoint(1, 2);

  // .next() is only loaded once during the iteration prologue (see
  // https://github.com/tc39/ecma262/pull/988/ and v8:6861)
  assertEquals(1, called);
  assertInstanceof(r2, ArgumentsPoint);
  assertInstanceof(r2, Point);
  assertEquals(r2.x, 1);
  assertEquals(r2.y, 2);
})();

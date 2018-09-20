// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  'use strict';

  class Point {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
  }

  function testBaselineAndOpt(func) {
    func(1, 2);
    func(1, 2);
    %OptimizeFunctionOnNextCall(func);
    return func(1, 2);
  }

  class RestPoint extends Point {
    constructor(...args) {
      super(...args);
    }
  }
  var r = testBaselineAndOpt(function(x, y) {
    return new RestPoint(x, y);
  });
  assertInstanceof(r, RestPoint);
  assertInstanceof(r, Point);
  assertEquals(r.x, 1);
  assertEquals(r.y, 2);

  class RestExtraPoint extends Point {
    constructor(...args) {
      super(-1, 0, ...args);
    }
  }
  r = testBaselineAndOpt(function(x, y) {
    return new RestExtraPoint(x, y);
  });
  assertInstanceof(r, RestExtraPoint);
  assertInstanceof(r, Point);
  assertEquals(r.x, -1);
  assertEquals(r.y, 0);

  class ArgumentsPoint extends Point {
    constructor() {
      super(...arguments);
    }
  }
  r = testBaselineAndOpt(function(x, y) {
    return new ArgumentsPoint(x, y);
  });
  assertInstanceof(r, ArgumentsPoint);
  assertInstanceof(r, Point);
  assertEquals(r.x, 1);
  assertEquals(r.y, 2);

  class ArgumentsExtraPoint extends Point {
    constructor() {
      super(1, 2, ...arguments);
    }
  }
  r = testBaselineAndOpt(function(x, y) {
    return new ArgumentsExtraPoint(x, y);
  });
  assertInstanceof(r, ArgumentsExtraPoint);
  assertInstanceof(r, Point);
  assertEquals(r.x, 1);
  assertEquals(r.y, 2);

  class LiteralPoint extends Point {
    constructor() {
      super(...[3, 4]);
    }
  }
  r = testBaselineAndOpt(function(x, y) {
    return new LiteralPoint(x, y);
  });
  assertInstanceof(r, LiteralPoint);
  assertInstanceof(r, Point);
  assertEquals(r.x, 3);
  assertEquals(r.y, 4);
})();

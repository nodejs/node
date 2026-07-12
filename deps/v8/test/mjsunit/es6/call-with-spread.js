// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  'use strict';

  function testBaselineAndOpt(func) {
    %PrepareFunctionForOptimization(func);
    func(-1, -2);
    func(-1, -2);
    %OptimizeFunctionOnNextCall(func);
    return func(-1, -2);
  }

  // Rest parameters

  function RestMax(...args) {
    return Math.max(...args);
  }

  var r = testBaselineAndOpt(function(x, y) {
    return RestMax(x, y);
  });
  assertEquals(r, -1);

  function RestMaxWithZero(...args) {
    return Math.max(0, ...args);
  }

  var r = testBaselineAndOpt(function(x, y) {
    return RestMaxWithZero(x, y);
  });
  assertEquals(r, 0);

  function RestMaxOneArg(x, ...args) {
    return Math.max(-10, ...args);
  }

  r = testBaselineAndOpt(function(x, y) {
    return RestMaxOneArg(x, y);
  });
  assertEquals(r, -2);

  // Strict Arguments Object

  function ArgumentsMax() {
    return Math.max(...arguments);
  }

  var r = testBaselineAndOpt(function(x, y) {
    return ArgumentsMax(x, y);
  });
  assertEquals(r, -1);

  function ArgumentsMaxWithZero() {
    return Math.max(0, ...arguments);
  }

  var r = testBaselineAndOpt(function(x, y) {
    return ArgumentsMaxWithZero(x, y);
  });
  assertEquals(r, 0);

  function ArgumentsMaxOneArg(x) {
    return Math.max(-10, ...arguments);
  }

  var r = testBaselineAndOpt(function(x, y) {
    return ArgumentsMaxOneArg(x, y);
  });
  assertEquals(r, -1);

})();

(function() {
  function testBaselineAndOpt(func) {
    %PrepareFunctionForOptimization(func);
    func(-1, -2);
    func(-1, -2);
    %OptimizeFunctionOnNextCall(func);
    return func(-1, -2);
  }

  // Sloppy Arguments Object

  function ArgumentsMax() {
    return Math.max(...arguments);
  }

  var r = testBaselineAndOpt(function(x, y) {
    return ArgumentsMax(x, y);
  });
  assertEquals(r, -1);

  function ArgumentsMaxWithZero() {
    return Math.max(0, ...arguments);
  }

  var r = testBaselineAndOpt(function(x, y) {
    return ArgumentsMaxWithZero(x, y);
  });
  assertEquals(r, 0);

  function ArgumentsMaxOneArg(x) {
    return Math.max(-10, ...arguments);
  }

  var r = testBaselineAndOpt(function(x, y) {
    return ArgumentsMaxOneArg(x, y);
  });
  assertEquals(r, -1);
})();

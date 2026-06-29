// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function dummy() {}

(function InlinedFunctionTestContext() {
  var f = function() {};

  function g() {
    var s = 'hey';
    dummy();  // Force a deopt point.
    if (f()) return s;
  };
  %PrepareFunctionForOptimization(g);
  g();
  g();
  g();
  %OptimizeFunctionOnNextCall(g);
  f = function() {
    return true;
  };
  assertEquals("hey", g());
})();

(function InlinedConstructorReturnTestContext() {
  function c() {
    return 1;
  }

  var f = function() {
    return !new c();
  };

  function g() {
    var s = 'hey';
    dummy();  // Force a deopt point.
    if (f()) return s;
  };
  %PrepareFunctionForOptimization(g);
  g();
  g();
  g();
  %OptimizeFunctionOnNextCall(g);
  f = function() {
    return true;
  };
  assertEquals("hey", g());
})();

(function InlinedConstructorNoReturnTestContext() {
  function c() {}

  var f = function() {
    return !new c();
  };

  function g() {
    var s = 'hey';
    dummy();  // Force a deopt point.
    if (f()) return s;
  };
  %PrepareFunctionForOptimization(g);
  g();
  g();
  g();
  %OptimizeFunctionOnNextCall(g);
  f = function() {
    return true;
  };
  assertEquals("hey", g());
})();

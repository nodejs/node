// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function foo() {
    var a = new Array(2);
    a[1] = 1.5;
    return a;
  }

  assertEquals(undefined, foo()[0]);
  assertEquals(undefined, foo()[0]);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo()[0]);
})();

(function() {
  function foo() {
    var a = Array(2);
    a[1] = 1.5;
    return a;
  }

  assertEquals(undefined, foo()[0]);
  assertEquals(undefined, foo()[0]);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo()[0]);
})();

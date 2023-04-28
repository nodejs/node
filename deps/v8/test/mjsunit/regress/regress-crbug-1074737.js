// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

try {
  throw 42
} catch (e) {
  function foo() { return e };
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo();
  var e = "expected";
}
assertEquals("expected", foo());

try {
  throw 42
} catch (f) {
  function foo2() { return f };
  %PrepareFunctionForOptimization(foo2);
  %OptimizeFunctionOnNextCall(foo2);
  foo2();
  with ({}) {
    var f = "expected";
  }
}
assertEquals("expected", foo2());

(function () {
  function foo3() { return g };
  %PrepareFunctionForOptimization(foo3);
  %OptimizeFunctionOnNextCall(foo3);
  foo3();
  with ({}) {
    var g = "expected";
  }
  assertEquals("expected", foo3());
})()

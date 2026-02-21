// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  var gaga = "gaga";
  function foo(a) {
    let y = Math.min(Infinity ? gaga : Infinity, -0) / 0;
    if (a) y = -0;
    return y ? 1 : 0;
  }
  %PrepareFunctionForOptimization(foo);
  foo(false);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(false));
})();

(function() {
  var gaga = "gaga";
  function foo(a) {
    let y = Math.min(Infinity ? gaga : Infinity, -0) % 0;
    if (a) y = 1.3;
    return y ? 1 : 0;
  }
  %PrepareFunctionForOptimization(foo);
  foo(false);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(false));
})();

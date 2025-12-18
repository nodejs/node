// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function bar(a) {
    let x = undefined;
    try {
      x = a & 1;
    } catch (e) {}
    return 2147483647 * x + 1 << 16;
  }

  %PrepareFunctionForOptimization(bar);
  bar(-1);
  bar([]); // [] & 1 = 0

  %OptimizeFunctionOnNextCall(bar);
  var obj = {};
  obj.__proto__ = null;
  assertEquals(0, bar(obj)); // obj & 1 will throw
})();

(function() {
  function foo(i) {
    var a = [0.5, 1, , 3];
    return (a[i] + 1) | 0;
  }

  %PrepareFunctionForOptimization(foo);
  foo(1);
  foo(1);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(2));
})();

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Check the results of `left >>> right`. The result is always unsigned (and
// therefore positive).
function test_shr(left) {
  var errors = 0;

  for (var i = 1; i < 1024; i++) {
    var temp = left >>> i;
    if (temp < 0) {
      errors++;
    }
  }

  return errors;
}

assertEquals(0, test_shr(1));
%OptimizeFunctionOnNextCall(test_shr);
for (var i = 5; i >= -5; i--) {
  assertEquals(0, test_shr(i));
}


(function () {
  function foo(x, b, array) {
    var y;
    x = x >>> 0;
    b ? (y = x | 0) : (y = x);
    return array[y];
  }

  foo(111, true, new Array(42));
  foo(111, true, new Array(42));
  %OptimizeFunctionOnNextCall(foo);
  foo(-111, true, new Array(42));
})();

(function () {
  function foo(x, b, array) {
    var y;
    x = x >>> 0;
    b ? (y = x | 0) : (y = x);
    return array[y];
  }

  foo(111, true, new Array(42));
  foo(111, true, new Array(42));
  %OptimizeFunctionOnNextCall(foo);
  foo(111, true, new Array(42));
})();

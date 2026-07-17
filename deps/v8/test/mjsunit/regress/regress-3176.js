// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(a) {
  var sum = 0;
  for (var i = 0; i < 10; i++) {
    sum += a[i];

    if (i > 6) {
      sum -= a[i - 4];
      sum -= a[i - 5];
    }
  }
  return sum;
}
%PrepareFunctionForOptimization(foo);

var a = new Int32Array(10);

%PrepareFunctionForOptimization(foo);
foo(a);
foo(a);
%OptimizeFunctionOnNextCall(foo);
foo(a);
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo(a);
assertOptimized(foo);

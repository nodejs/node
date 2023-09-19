// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function foo() {
  var a = {x:1};
  var b = {x:1.5, y: 1};
  var x = 0;
  for (var i = 0; i < 1; i = {}) {
    // The second iteration of this loop is dead code, leading to a
    // contradiction between dynamic and static information.
    x += a.x + 0.5;
    x += a.x % 0.5;
    x += Math.abs(a.x);
    x += a.x < 6;
    x += a.x === 7;
    x += a.x <= 8;
    a = b;
  }
  return x;
}
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

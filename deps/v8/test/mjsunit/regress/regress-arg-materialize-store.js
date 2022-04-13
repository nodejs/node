// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  return f.arguments;
}

function g(deopt) {
  var o = {x: 2};
  f();
  o.x = 1;
  deopt + 0;
  return o.x;
};
%PrepareFunctionForOptimization(g);
g(0);
g(0);
%OptimizeFunctionOnNextCall(g);
assertEquals(1, g({}));

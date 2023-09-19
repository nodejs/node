// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax


function f() {
  %DeoptimizeFunction(g);
}
%NeverOptimizeFunction(f);

function g(o) {
  f();
  return h(o);
}

function h(o) {
  return o.x;
}

%PrepareFunctionForOptimization(g);
g({x : 1});
g({x : 2});
%OptimizeFunctionOnNextCall(g);
g({x : 3});
%PrepareFunctionForOptimization(h);
%OptimizeFunctionOnNextCall(h);
g({x : 4});
g({y : 1, x : 3});

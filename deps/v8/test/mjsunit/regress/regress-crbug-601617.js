// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-tailcalls

function h() {
  var res = g.arguments[0].x;
  return res;
}

function g(o) {
  var res = h();
  return res;
}

function f1() {
  var o = { x : 1 };
  var res = g(o);
  return res;
}

function f0() {
  "use strict";
  return f1(5);
}

%NeverOptimizeFunction(h);
f0();
f0();
%OptimizeFunctionOnNextCall(f0);
assertEquals(1, f0());

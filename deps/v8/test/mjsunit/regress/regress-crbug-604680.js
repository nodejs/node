// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function h() {
  var res = g.arguments;
  return res;
}

function g(o) {
  var res = h();
  return res;
}

function f1() {
  var o = { x : 42 };
  var res = g(o);
  return 1;
}

function f0(a, b)  {
  "use strict";
  return f1(5);
}

function boom(b) {
  if (b) throw new Error("boom!");
}

%NeverOptimizeFunction(h);
f0();
f0();
%OptimizeFunctionOnNextCall(f0);

boom(false);
boom(false);
%OptimizeFunctionOnNextCall(boom);

try {
  f0(1, 2, 3);
  boom(true, 1, 2, 3);
} catch (e) {
}

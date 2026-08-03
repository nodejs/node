// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  const o = {};
  // The order of the following operations is significant
  o.a = 0;
  o[1024] = 1;  // An offset of >=1024 is required
  delete o.a;
  o.b = 2;
  return o.b;
}
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();


function g(o) {
  o.b = 2;
}
function h() {
  const o = {};
  o.a = 0;
  o[1024] = 1;
  delete o.a;
  g(o);
  assertEquals(o.b, 2);
}
%NeverOptimizeFunction(g);
%PrepareFunctionForOptimization(h);
h();
h();
%OptimizeFunctionOnNextCall(h);
h();

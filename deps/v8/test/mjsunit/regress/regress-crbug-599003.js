// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --verify-heap

function A() {}

function g1() {
  var obj = new A();
  obj.v0 = 0;
  obj.v1 = 0;
  obj.v2 = 0;
  obj.v3 = 0;
  obj.v4 = 0;
  obj.v5 = 0;
  obj.v6 = 0;
  obj.v7 = 0;
  obj.v8 = 0;
  obj.v9 = 0;
  return obj;
}

function g2() {
  return new A();
};
%PrepareFunctionForOptimization(g2);
var o = g1();
%OptimizeFunctionOnNextCall(g2);
g2();
o = null;
gc();

for (var i = 0; i < 20; i++) {
  var o = new A();
}
g2();

gc();  // Boom!

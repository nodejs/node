// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class A extends Array {
  a = 1;
}

let flag = false;
function bar() {
  if (flag) {
    o.a = 2;
  }
  return 0;
}
function g() {
  return o.a + bar() + o.a;
}

let o = new A();
let o2 = new A();
o2.b = 2;
%PrepareFunctionForOptimization(g);
g();
g();
g();
g();
%OptimizeFunctionOnNextCall(g);
assertEquals(2, g());
flag = true;
assertEquals(3, g());

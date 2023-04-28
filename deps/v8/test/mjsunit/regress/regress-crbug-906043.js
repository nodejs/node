// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function fun(arg) {
  let x = arguments.length;
  a1 = new Array(0x10);
  a1[0] = 1.1;
  a2 = new Array(0x10);
  a2[0] = 1.1;
  a1[(x >> 16) * 21] = 1.39064994160909e-309;  // 0xffff00000000
  a1[(x >> 16) * 41] = 8.91238232205e-313;  // 0x2a00000000
}

var a1, a2;
var a3 = [1.1, 2.2];
a3.length = 0x11000;
a3.fill(3.3);

var a4 = [1.1];

%PrepareFunctionForOptimization(fun);
for (let i = 0; i < 3; i++) fun(...a4);
%OptimizeFunctionOnNextCall(fun);
fun(...a4);

res = fun(...a3);

assertEquals(16, a2.length);
for (let i = 8; i < 32; i++) {
  assertEquals(undefined, a2[i]);
}

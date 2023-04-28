// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function f1() {
  const x = [,];
  x[1] = 42;
  assertEquals([, 42], x);
}

%PrepareFunctionForOptimization(f1);
f1();
f1();
%OptimizeFunctionOnNextCall(f1);
f1();
f1();


function f2() {
  const x = [0];
  for (const y of [1, 2, 3, 4]) {
    x[x.length] = y;
  }
  assertEquals([0, 1, 2, 3, 4], x);
}

%PrepareFunctionForOptimization(f2);
f2();
f2();
%OptimizeFunctionOnNextCall(f2);
f2();
f2();


function f3() {
  const x = [0];
  for (const y of [1.1, {}]) {
    x[x.length] = y;
  }
  assertEquals([0, 1.1, {}], x);
}

%PrepareFunctionForOptimization(f3);
f3();
f3();
%OptimizeFunctionOnNextCall(f3);
f3();
f3();


function f4(x) {
  x[x.length] = x.length;
}

%PrepareFunctionForOptimization(f4);
let x1 = [];
f4(x1);
assertEquals([0], x1);
f4(x1);
assertEquals([0, 1], x1);
%OptimizeFunctionOnNextCall(f4);
f4(x1);
assertEquals([0, 1, 2], x1);
f4(x1);
assertEquals([0, 1, 2, 3], x1);

%PrepareFunctionForOptimization(f4);
let x2 = {length: 42};
f4(x2);
assertEquals(42, x2[42]);
f4(x2);
assertEquals(42, x2[42]);
%OptimizeFunctionOnNextCall(f4);
f4(x2);
assertEquals(42, x2[42]);
f4(x2);
assertEquals(42, x2[42]);

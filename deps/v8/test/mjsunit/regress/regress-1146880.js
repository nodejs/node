// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --assert-types

function f(a,b) {
  let t = a >= b;
  while (t != 0) {
    a = a | (b - a);
    let unused = a >= b;
    t = a < b;
  }
}
function test() {
  f(Infinity,1);
  f(undefined, undefined);
}

// Trigger TurboFan compilation
%PrepareFunctionForOptimization(test);
%PrepareFunctionForOptimization(f);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();

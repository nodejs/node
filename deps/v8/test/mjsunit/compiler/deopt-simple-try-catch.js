
// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax

// Small program to test deoptimization with exception handling.

function g() {
  %DeoptimizeFunction(f);
  throw 42;
}
%NeverOptimizeFunction(g);

function f() {
  var a = 1;
  try {
    g();
  } catch (e) {
    return e + a;
  }
}

%PrepareFunctionForOptimization(f);
assertEquals(f(), 43);
assertEquals(f(), 43);
%OptimizeFunctionOnNextCall(f);
assertEquals(f(), 43);

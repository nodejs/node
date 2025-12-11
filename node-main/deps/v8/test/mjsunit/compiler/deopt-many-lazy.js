// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax


// It optimizes and lazily deoptimizes 2 functions several times.

function f() {
  g();
}

function g() {
  %DeoptimizeFunction(f);
}

function a() {
  b();
}

function b() {
  %DeoptimizeFunction(a);
}

%PrepareFunctionForOptimization(f);
f(); f();
%OptimizeFunctionOnNextCall(f);
f();
%PrepareFunctionForOptimization(a);
a(); a();
%OptimizeFunctionOnNextCall(a);
a();
for(var i = 0; i < 5; i++) {
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f();
  %PrepareFunctionForOptimization(a);
  %OptimizeFunctionOnNextCall(a);
  a();
}

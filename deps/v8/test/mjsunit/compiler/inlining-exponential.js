// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// {f8} has 6 calls to {f7}, which has 6 to {f6}, and so on up to {f1}, which
// means that {f8} calls 6**7=279936 calls. If Turbofan tries to inline all of
// them, compilation will become extremely slow (or even run into OOM or Node
// Ids limits).

function f1(x) {
  return x % 45 - x / 17.5 + x * 19.55 * x * 3 + x * 2 + x * 5 + 10.25;
}
function f2(x) {
  return f1(f1(f1(f1(x)))) * f1(x) + f1(x);
}
function f3(x) {
  return f2(f2(f2(f2(x)))) / f2(x) + f2(x);
}
function f4(x) {
  return f3(f3(f3(f3(x)))) / f3(x) + f3(x);
}
function f5(x) {
  return f4(f4(f4(f4(x)))) / f4(x) + f4(x);
}
function f6(x) {
  return f5(f5(f5(f5(x)))) / f5(x) + f5(x);
}
function f7(x) {
  return f6(f6(f6(f6(x)))) / f6(x) + f6(x);
}
function f8(x) {
  return f7(f7(f7(f7(x)))) / f7(x) + f7(x);
}


%PrepareFunctionForOptimization(f1);
%PrepareFunctionForOptimization(f2);
%PrepareFunctionForOptimization(f3);
%PrepareFunctionForOptimization(f4);
%PrepareFunctionForOptimization(f5);
%PrepareFunctionForOptimization(f6);
%PrepareFunctionForOptimization(f7);
%PrepareFunctionForOptimization(f8);
f8(4.5);
f8(4.5);

%OptimizeFunctionOnNextCall(f8);
f8(4.5);

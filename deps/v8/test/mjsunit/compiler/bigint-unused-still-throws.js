// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function WarmupAndOptimize(f)  {
  %PrepareFunctionForOptimization(f);
  f(1n, 1n);
  %OptimizeFunctionOnNextCall(f);
  f(1n, 1n);
  assertOptimized(f);
}
%NeverOptimizeFunction(WarmupAndOptimize);

function TestBinary(f) {
  WarmupAndOptimize(f);
  assertThrows(() => { f(1, 1n); }, TypeError);
  // Recompile in case the above deopts.
  WarmupAndOptimize(f);
  assertThrows(() => { f(1n, 1); }, TypeError);
}
%NeverOptimizeFunction(TestBinary);

function Add(a, b) {
  let [c] = [1n];
  let temp = 0n;
  temp = a + c;
  temp = c + b;
  temp = 42n;
  result = temp;
}
TestBinary(Add);

function Subtract(a, b) {
  let [c] = [1n];
  let temp = 0n;
  temp = a - c;
  temp = c - b;
  temp = 42n;
  result = temp;
}
TestBinary(Subtract);

function Multiply(a, b) {
  let [c] = [1n];
  let temp = 0n;
  temp = a * c;
  temp = c * b;
  temp = 42n;
  result = temp;
}
TestBinary(Multiply);

function Divide(a, b) {
  let [c] = [1n];
  let temp = 0n;
  temp = a / c;
  temp = c / b;
  temp = 42n;
  result = temp;
}
TestBinary(Divide);

function BitwiseAnd(a, b) {
  let [c] = [1n];
  let temp = 0n;
  temp = a & c;
  temp = c & b;
  temp = 42n;
  result = temp;
}
TestBinary(BitwiseAnd);

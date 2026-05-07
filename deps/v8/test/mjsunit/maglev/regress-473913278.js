// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let not_a_function;

function bar(x) {
  let a = x + 2;
  let b = x + 3;
  let c = x + 4;
  let d = x + 5;
  not_a_function(); // will throw
  return a + b + c + d; // keeping locals alive
}

function foo(x) {
  let a = x + 2;
  try { bar(); } catch (e) {}
  return a;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();

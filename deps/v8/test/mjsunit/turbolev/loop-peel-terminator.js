// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

function simple_return_in_the_peel(n) {
  let acc = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (n == 1) return acc;
  }
  return acc;
}
%PrepareFunctionForOptimization(simple_return_in_the_peel);
simple_return_in_the_peel(20);
simple_return_in_the_peel(1);
%OptimizeFunctionOnNextCall(simple_return_in_the_peel);
simple_return_in_the_peel(20);
assertOptimized(simple_return_in_the_peel);

function simple_return(n) {
  let acc = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (i == 3) return acc;
  }
  return acc;
}
%PrepareFunctionForOptimization(simple_return);
simple_return(20);
simple_return(0);
%OptimizeFunctionOnNextCall(simple_return);
simple_return(20);
assertOptimized(simple_return);


function simple_deopt_in_the_peel(n) {
  let acc = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (n == 1) deopt();
  }
  return acc;
}
%PrepareFunctionForOptimization(simple_deopt_in_the_peel);
simple_deopt_in_the_peel(20);
%OptimizeFunctionOnNextCall(simple_deopt_in_the_peel);
simple_deopt_in_the_peel(20);
assertOptimized(simple_deopt_in_the_peel);
try { simple_deopt_in_the_peel(1); } catch(e) {}
assertUnoptimized(simple_deopt_in_the_peel);

function simple_deopt(n) {
  let acc = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (i == 3) deopt();
  }
  return acc;
}
%PrepareFunctionForOptimization(simple_deopt);
simple_deopt(2);
%OptimizeFunctionOnNextCall(simple_deopt);
simple_deopt(2);
assertOptimized(simple_deopt);
try { simple_deopt(4); } catch(e) {}
assertUnoptimized(simple_deopt);

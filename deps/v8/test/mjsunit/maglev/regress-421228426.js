// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size-small=0

function runNearStackLimit(f) {
  function t() {
    try {
      return t();
    } catch (e) {
      return f();
    }
  }
  return t();
}

function nop() {}
function try_nop() {
  try {
    nop();
  } catch (e) {}
}

// This forces the feedback in the catch block inside try_nop to
// indicate that the block was used. This avoids Maglev to just
// add a soft deopt.
runNearStackLimit(try_nop);
runNearStackLimit(try_nop);

function foo() {
  for (let i = 0; i < 5; i++) {
    try_nop();
  }
}

%PrepareFunctionForOptimization(nop);
%PrepareFunctionForOptimization(try_nop);
%PrepareFunctionForOptimization(foo);
foo();
%OptimizeMaglevOnNextCall(foo);
foo();

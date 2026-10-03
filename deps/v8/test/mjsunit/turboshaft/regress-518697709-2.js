// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This doesn't currently trigger any bug, but it's the JS version of a Wasm
// regression test for a WasmLoadElimination bug around loop phi processing. If
// we ever do the same optimization in JavaScript, this test could be useful.

function foo() {
  let phi1 = 0;
  let phi2 = 0;
  let phi3 = 0;
  for (let i = 0; i < 42; i++) {
    phi1 = phi2;
    phi2 = 0;
    phi3 = phi1;
  }
  return phi3;
}

%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();

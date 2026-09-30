// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(a, b, c) {
  // Use a, b, c from arguments to prevent constant folding.

  // JavaScript semantics require that we don't do FMA (fused multiply-add)
  // here. In particular, we need to first round (a * b) and then round the
  // result of the +, instead of rounding only once at the end.
  return (a * b) + c;
}

let a = 1 + Math.pow(2, -28);
let b = 1 - Math.pow(2, -28);
let c = -1;

%PrepareFunctionForOptimization(foo);
let res1 = foo(a, b, c);
%OptimizeFunctionOnNextCall(foo);
let res2 = foo(a, b, c);

assertEquals(res1, res2);

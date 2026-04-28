// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --fuzzing --expose-gc --allow-natives-syntax --disable-abortjs
// Flags: --disable-in-process-stack-traces

let f64 = new Float64Array(1);
f64[0] = 1.0;
let hn = f64[0];

let script_var_1 = hn;
let script_var_2 = 1;
script_var_2 = 2;

function foo(x) {
  script_var_1 = x;
  script_var_2 = x;
}

%PrepareFunctionForOptimization(foo);
%OptimizeMaglevOnNextCall(foo);

foo(hn);

assertEquals(1, script_var_2);

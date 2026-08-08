// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax    --test-small-max-function-context-stub-size
function f() {
  let x1 = 1;
  let x2 = 1;
  let x3 = 1;
  let x4 = 1;
  let x5 = 1;
  let x6 = 1;
  let x7 = 1;
  let x8 = 1;
  let x9 = 1;
  let x10 = 1;
  let x11 = 1;
  let x12 = 1;
  ()=>(x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12);
}

%PrepareFunctionForOptimization(f);
f();
f();
f();
f();
%OptimizeMaglevOnNextCall(f);
f();

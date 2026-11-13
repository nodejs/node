// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --cache=after-execute

function f(a = 1) {
  var x = 1;
  { let b1 = 1;
  { let b2 = 1;
  { let b3 = 1;
  { let b4 = 1;
  { let b5 = 1;
  { let b6 = 1;
  { let b7 = 1;
  { let b8 = 1;
  { let b9 = 1;
  { let b10 = 1;
  { let b11 = 1;
  { let b12 = 1;
  { let b13 = 1;
  { let b14 = 1;
  { let b15 = 1;
  { let b16 = 1;
  { let b17 = 1;
  { let b18 = 1;
  { let b19 = 1;
  { let b20 = 1;
  { let b21 = 1;
  { let b22 = 1;
  { let b23 = 1;
  { let b24 = 1;
  { let b25 = 1;
  { let b26 = 1;
  { let b27 = 1;
    function inner() {
      return a + x + b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9 + b10 +
             b11 + b12 + b13 + b14 + b15 + b16 + b17 + b18 + b19 + b20 +
             b21 + b22 + b23 + b24 + b25 + b26 + b27;
    }
    eval('');
    %PrepareFunctionForOptimization(inner);
    inner(); inner();
    %OptimizeFunctionOnNextCall(inner);
    inner();
  } } } } } } } } } } } } } } } } } } } } } } } } } } }
}
f();

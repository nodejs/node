// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --jit-fuzzing

function foo(x) {
  let v = 0;
  [ 2, 3 ].forEach(y => v += y + x);
}

%PrepareFunctionForOptimization(foo);
foo(3);
foo(3);

%OptimizeFunctionOnNextCall(foo);
foo();

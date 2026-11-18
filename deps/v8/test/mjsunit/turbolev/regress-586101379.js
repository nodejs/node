// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0
// Flags: --turbolev-non-eager-inlining --turbolev-non-eager-loop-peeling

function make_array(x, y) {
  return new Array(x, y);
}

function foo(arr, y, z) {
  // Makes the truncation pass run.
  z | 1;
  // Peeling this loop reruns the optimizer after `make_array` is inlined.
  for (let i = 0; i < 3; i++) {
    %AssertPeeled();
  }
  let x = arr.length + 2;
  // Tags `x` as a Smi.
  make_array(x, y);
  // The new length is CSE'd with `x`, and its Smi tagging with the one above,
  // so the AssumeType that records it as a Smi is its last use.
  return arr.push(1, 2);
}

%PrepareFunctionForOptimization(make_array);
%PrepareFunctionForOptimization(foo);
assertEquals(2, foo([], 0, 0));
%OptimizeFunctionOnNextCall(foo);
let arr = [];
assertEquals(2, foo(arr, 0, 0));
assertEquals([1, 2], arr);

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --allow-natives-syntax --maglev-object-tracking
function make_double_empty() {
  return [];
}
for (let i = 0; i < 1000; i++) {
  let a = make_double_empty();
  a[0] = 1.1;
}

function foo() {
  let arr1 = [];
  let x = arr1[0];

  let arr2 = make_double_empty();
  let y = arr2[0];
  return [x, y];
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

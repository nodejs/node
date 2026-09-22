// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(arr, index, val) {
  let x = arr.at();
  arr[index] = val;
}
function make_object_array() {
  return [null];
}
for (let i = 0; i < 20; i++) {
  let non_cow = ["c"];
  non_cow[0] = "a"; // Make it non-COW
  foo(non_cow, 0);
}
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
let cow = make_object_array();
foo(cow, 0, {});
%PrepareFunctionForOptimization(make_object_array);
make_object_array();

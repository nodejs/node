// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function push_arr(arr, x, y) {
  arr.push(x, y);
}

let double_arr = [1.5, 2.5];

%PrepareFunctionForOptimization(push_arr);
push_arr(double_arr, 42.5, 43.5);
assertEquals([1.5, 2.5, 42.5, 43.5], double_arr);

%OptimizeFunctionOnNextCall(push_arr);
push_arr(double_arr, 55.5, 56.5);
assertEquals([1.5, 2.5, 42.5, 43.5, 55.5, 56.5], double_arr);
assertOptimized(push_arr);

// Storing a non-double will deopt (we keep the first input as a double though,
// to make sure that we don't deopt at the wrong place after having stored some
// items already: we should validate all inputs before starting to store so that
// we deopt easily without builtin continuation).
push_arr(double_arr, 17.5, {});
assertEquals([1.5, 2.5, 42.5, 43.5, 55.5, 56.5, 17.5, {}], double_arr);
assertUnoptimized(push_arr);

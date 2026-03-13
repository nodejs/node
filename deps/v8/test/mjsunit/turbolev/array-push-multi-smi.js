// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function push_arr(arr, x, y) {
  arr.push(x, y);
}

let smi_arr = [1, 2];

%PrepareFunctionForOptimization(push_arr);
push_arr(smi_arr, 42, 43);
assertEquals([1, 2, 42, 43], smi_arr);

%OptimizeFunctionOnNextCall(push_arr);
push_arr(smi_arr, 55, 56);
assertEquals([1, 2, 42, 43, 55, 56], smi_arr);
assertOptimized(push_arr);

// Storing a non-smi will deopt (we keep the first input as a Smi though, to
// make sure that we don't deopt at the wrong place after having stored some
// items already: we should validate all inputs before starting to store so that
// we deopt easily without builtin continuation).
push_arr(smi_arr, 17, {});
assertEquals([1, 2, 42, 43, 55, 56, 17, {}], smi_arr);
assertUnoptimized(push_arr);

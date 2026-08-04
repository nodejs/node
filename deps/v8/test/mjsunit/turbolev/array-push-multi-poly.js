// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function push_arr(arr, x, y) {
  arr.push(x, y);
}

%PrepareFunctionForOptimization(push_arr);
let smi_arr = [1, /*hole*/, 2];
let obj_arr = [ {}, /*hole*/, "ab" ];
push_arr(smi_arr, 42, 43);
assertEquals([1, /*hole*/, 2, 42, 43], smi_arr);
push_arr(obj_arr, [], "c");
assertEquals([{}, /*hole*/, "ab", [], "c"], obj_arr);
// Feedback collection of some transitions is a bit buggy, so re-running
// feedback collection once more.
smi_arr = [1, /*hole*/, 2];
obj_arr = [ {}, /*hole*/, "ab" ];
push_arr(smi_arr, 42, 43);
assertEquals([1, /*hole*/, 2, 42, 43], smi_arr);
push_arr(obj_arr, [], "c");
assertEquals([{}, /*hole*/, "ab", [], "c"], obj_arr);

%OptimizeFunctionOnNextCall(push_arr);
smi_arr = [1, /*hole*/, 2];
obj_arr = [ {}, /*hole*/, "ab" ];
push_arr(smi_arr, 42, 43);
assertEquals([1, /*hole*/, 2, 42, 43], smi_arr);
push_arr(obj_arr, [], "c");
assertEquals([{}, /*hole*/, "ab", [], "c"], obj_arr);

// Storing a non-smi into a Smi array will deopt.
smi_arr = [1, /*hole*/, 2];
push_arr(smi_arr, 17, "x");
assertEquals([1, /*hole*/, 2, 17, "x"], smi_arr);
assertUnoptimized(push_arr);

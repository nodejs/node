// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function create(v) {
  return { k: v };
}

function change_k(o) {
  o.k = 1.7;
}

function test(o) {
  let x = o.k;
  change_k(o);
  let y = o.k;
  return [x, y];
}

// 1. Initialize boilerplate and infer as const double
for (let i = 0; i < 1000; i++) {
  create(1.5);
}

// 2. Warm up test function
%PrepareFunctionForOptimization(test);
%PrepareFunctionForOptimization(change_k);
%PrepareFunctionForOptimization(create);
let o_stable = create(1.5);
for (let i = 0; i < 1000; i++) {
  test(o_stable);
}

// 3. Tier up to TurboFan
%OptimizeFunctionOnNextCall(test);
test(o_stable); // Trigger optimization

// 4. Create object with Hole NaN
const hole_nan = %GetHoleNaN();
const o_hole = create(hole_nan);

// 5. Run test on hole object
const result = test(o_hole);

// Verify failure (constant folding Hole NaN)
// If result is [NaN, NaN], it failed (reproduced bug).
// If result is [NaN, 1.7], it passed.
if (Number.isNaN(result[0]) && Number.isNaN(result[1])) {
  print("FAILURE: Constant folded Hole NaN");
  quit(1);
} else if (Number.isNaN(result[0]) && result[1] === 1.7) {
  print("SUCCESS: Did not constant fold Hole NaN");
  quit(0);
} else {
  print("Unexpected result: " + result);
  quit(2);
}

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev --no-turbofan

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

// 2. Create object with Hole NaN
const hole_nan = %GetHoleNaN();
const o_hole = create(hole_nan);

// 3. Run test on hole object
const result = test(o_hole);

// Verify failure
if (Number.isNaN(result[0]) && Number.isNaN(result[1])) {
  print("FAILURE: Constant folded Hole NaN in Ignition");
  quit(1);
} else if (Number.isNaN(result[0]) && result[1] === 1.7) {
  print("SUCCESS: Did not constant fold Hole NaN");
  quit(0);
} else {
  print("Unexpected result: " + result);
  quit(2);
}

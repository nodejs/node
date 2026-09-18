// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan

function create(v) {
  return { key: v };
}

let o = { key: 1.5 }; // Global object

let bla_action = function() {
  // Do nothing during warmup
};

function bla() {
  bla_action();
}

%NeverOptimizeFunction(bla);

function test() {
  let v = o.key;
  bla();
  let w = o.key;
  return [v, w];
}

%PrepareFunctionForOptimization(create);
%PrepareFunctionForOptimization(test);

// 1. Warm up create
for (let i = 0; i < 100; i++) {
  create(1.5);
}
%OptimizeMaglevOnNextCall(create);
create(1.5);

// 2. Warm up test (bla does nothing)
for (let i = 0; i < 100; i++) {
  test();
}

%OptimizeFunctionOnNextCall(test);
test(); // Trigger optimization

// 3. Create object with Hole NaN and assign to global o
const hole_nan = %GetHoleNaN();
o = create(hole_nan);

// 4. Change bla's behavior to set o.key = 1.7
bla_action = function() {
  o.key = 1.7;
};

// 5. Run optimized test
const result = test();

// Verify failure
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

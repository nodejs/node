// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

let cnt = 0;
function helper() {
  cnt++;
  // No feedback, maglev generates a deopt:
  const tmp = {m: cnt};
}

function trigger() {
  const obj = {x: 5};
  let phi = 0;
  for (let i = 0; i < 100; i++) {
    // Updates NodeInfo for `obj`:
    let val = obj.x;
    // At the time of writing, maglev does not see that the first iteration
    // always takes this branch:
    if (i % 20 === 0) {
      helper();
      // The side-effect:
      obj.y = i;
    }
    phi += val;
  }
  return phi;
}

// There's intentionally no PrepareFunctionForOptimization(helper), as the
// repro requires that Maglev finds no feedback when compiling helper and
// generates an unconditional deopt.
%PrepareFunctionForOptimization(trigger);
trigger();
trigger();
%OptimizeMaglevOnNextCall(trigger);
trigger();

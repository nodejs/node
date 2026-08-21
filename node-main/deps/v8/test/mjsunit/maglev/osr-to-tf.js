// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-stress-opt
// Flags: --no-baseline-batch-compilation --use-osr --turbofan
// Flags: --concurrent-osr --concurrent-recompilation
// Flags: --osr-from-maglev --concurrent-osr --concurrent-recompilation
// Flags: --always-osr-from-maglev

let keep_going = 10000000;  // A counter to avoid test hangs on failure.

function f() {
  let reached_tf = false;
  let prev_status = 0;
  while (!reached_tf && --keep_going) {
    // This loop should trigger OSR.
    reached_tf = %CurrentFrameIsTurbofan();
    let status = %GetOptimizationStatus(f);
    if (status !== prev_status) {
      let p = []
      for (let k in V8OptimizationStatus) {
        if (V8OptimizationStatus[k] & status) {
          p.push(k);
        }
      }
      print(p.join(","));
      prev_status = status;
    }
  }
}

function g() {
  assertTrue(%IsTurbofanEnabled());
  f();
  assertTrue(keep_going > 0);
}
%NeverOptimizeFunction(g);

g();

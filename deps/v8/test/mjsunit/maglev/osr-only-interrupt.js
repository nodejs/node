// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-stress-opt
// Flags: --no-baseline-batch-compilation --use-osr --turbofan
// Flags: --concurrent-osr --concurrent-recompilation
// Flags: --osr-from-maglev=4

let keep_going = 10000000;

function f() {
  let reached_tf = false;
  while (!reached_tf && --keep_going > 0) {
    reached_tf = %CurrentFrameIsTurbofan();
  }
  return reached_tf;
}

function test() {
  assertTrue(%IsTurbofanEnabled());
  assertTrue(f());
  assertTrue(keep_going > 0);
}
%NeverOptimizeFunction(test);

test();

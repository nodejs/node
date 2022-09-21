// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-stress-opt
// Flags: --no-baseline-batch-compilation --use-osr --turbofan

let keep_going = 100000;  // A counter to avoid test hangs on failure.

function f() {
  let reached_tf = false;
  while (!reached_tf && --keep_going) {
    // This loop should trigger OSR.
    reached_tf = %CurrentFrameIsTurbofan();
  }
}

function g() {
  assertTrue(%IsTurbofanEnabled());
  f();
  assertTrue(keep_going > 0);
}
%NeverOptimizeFunction(g);

g();

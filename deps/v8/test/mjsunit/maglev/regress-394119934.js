// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-stress-opt
// Flags: --no-baseline-batch-compilation --use-osr --turbofan
// Flags: --max-bytecode-size-for-early-opt=0
// Flags: --osr-from-maglev=4 --maglev-loop-peeling

let keep_going = 1000000;
let i;

function f() {
  let sum = 0;
  while (--i > 0 && !%CurrentFrameIsTurbofan() && --keep_going) {
    sum += i;
  }
  return sum;
}

function g() {
  if (!%IsMaglevEnabled() || !%IsTurbofanEnabled()) return;

  while (!%ActiveTierIsMaglev(f) && --keep_going) {
    i = 5;
    f();
  }

  i = 10000;
  f();
  assertTrue(keep_going > 0);
}
%NeverOptimizeFunction(g);

g();

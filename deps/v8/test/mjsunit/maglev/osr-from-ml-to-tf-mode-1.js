// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-stress-opt
// Flags: --no-baseline-batch-compilation --use-osr --turbofan
// Flags: --max-bytecode-size-for-early-opt=0
// Flags: --no-concurrent-osr

let keep_going = 100000;

function isMaglevOrTurbofan(func) {
  let opt_status = %GetOptimizationStatus(func);
  return (opt_status & (V8OptimizationStatus.kTopmostFrameIsMaglev |
                        V8OptimizationStatus.kTopmostFrameIsTurboFanned)) !== 0;
}

// Test 1: Function f has no OSR code installed on its loop.
// With --osr-from-maglev=2 (default), Maglev should not emit OSR checks,
// so f() should not OSR to TurboFan.
function f() {
  let sum = 0;
  for (let i = 0; i < 100; i++) {
    sum += i;
  }
  return sum;
}

function testNoOsr() {
  assertTrue(%IsMaglevEnabled());
  assertTrue(%IsTurbofanEnabled());

  while (!%ActiveTierIsMaglev(f) && --keep_going) {
    f();
  }
  assertTrue(keep_going > 0);

  assertEquals(4950, f());
  assertFalse(%CurrentFrameIsTurbofan());
}
%NeverOptimizeFunction(testNoOsr);
testNoOsr();

// Test 2: Function g triggers %OptimizeOsr() so OSR code is installed.
// With --osr-from-maglev=2 (default) and --no-concurrent-osr, %OptimizeOsr() compiles
// synchronously, installing OSR code.
function g() {
  for (let i = 0; i < 20; i++) {
    if (i == 5) {
      %OptimizeOsr();
    }
    if (i > 10) {
      if (isMaglevOrTurbofan(g)) {
        return true;
      }
    }
  }
  return false;
}

g();
%PrepareFunctionForOptimization(g);
assertTrue(g());

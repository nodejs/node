// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-osr --no-turbofan --use-osr
// Flags: --no-concurrent-osr

// Maglev OSR code that deopts after its loop should request Maglev tier-up of
// the function.

var global_value = 42;

function f() {
  let sum = 0;
  for (let i = 0; i < 10; i++) {
    if (i == 5) %OptimizeOsr();
    sum += i;
  }
  return sum + global_value;
}
%PrepareFunctionForOptimization(f);

assertEquals(87, f());
assertTrue(
    (%GetOptimizationStatus(f) &
     (V8OptimizationStatus.kMarkedForMaglevOptimization |
      V8OptimizationStatus.kMarkedForConcurrentMaglevOptimization)) !== 0);

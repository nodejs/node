// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function testThrowsRepeated(fn) {
  %PrepareFunctionForOptimization(fn);
  for (let i = 0; i < 5; i++) assertThrows(fn, TypeError);
  %OptimizeFunctionOnNextCall(fn);
  assertThrows(fn, TypeError);
  // Assert that the function is still optimized, i.e. no deopt happened when
  // calling it.
  assertOptimized(fn);
}

let symbol = Symbol("test");
{ // Test with smi.
  let other = 1;
  let addL = () => symbol + other;
  let addR = () => other + symbol;
  let bitAndL = () => symbol & other;
  let bitAndR = () => other & symbol;
  testThrowsRepeated(addL);
  testThrowsRepeated(addR);
  testThrowsRepeated(bitAndL);
  testThrowsRepeated(bitAndR);
}
{ // Test with non-smi.
  let other = Math.pow(2, 32);
  let addL = () => symbol + other;
  let addR = () => other + symbol;
  let bitAndL = () => symbol & other;
  let bitAndR = () => other & symbol;
  testThrowsRepeated(addL);
  testThrowsRepeated(addR);
  testThrowsRepeated(bitAndL);
  testThrowsRepeated(bitAndR);
}

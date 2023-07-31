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
let preIncrement  = () => ++symbol;
let preDecrement  = () => --symbol;
let postIncrement = () => symbol++;
let postDecrement = () => symbol--;
let unaryPlus = () => -symbol;
let unaryMinus = () => -symbol;
let bitwiseNot = () => ~symbol;
testThrowsRepeated(preIncrement);
testThrowsRepeated(preDecrement);
testThrowsRepeated(postIncrement);
testThrowsRepeated(postDecrement);
testThrowsRepeated(unaryPlus);
testThrowsRepeated(unaryMinus);
testThrowsRepeated(bitwiseNot);

// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function dematerialized_double_array(n) {
  let arr = [ 1.1, 2.2, 3.3 ];
  if (n < 0) {
    // We won't execute this code when collecting feedback, which means that
    // Maglev will do an unconditional deopt here (and {arr} will thus have no
    // use besides the frame state for this deopt).
    n + 5;
    return arr;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(dematerialized_double_array);
assertEquals(7, dematerialized_double_array(5));
assertEquals(7, dematerialized_double_array(5));

%OptimizeFunctionOnNextCall(dematerialized_double_array);
assertEquals(7, dematerialized_double_array(5));
assertOptimized(dematerialized_double_array);
{
  let result = dematerialized_double_array(-1);
  assertEquals([1.1, 2.2, 3.3], result);
  assertTrue(%HasDoubleElements(result));
  assertUnoptimized(dematerialized_double_array);
}

// Test with holes.
function test_double_array_with_holes(n) {
  let arr = [ 1.1, /* hole */, 3.3 ]
  if (n < 0) {
    n + 5;
    return arr;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(test_double_array_with_holes);
assertEquals(7, test_double_array_with_holes(5));
assertEquals(7, test_double_array_with_holes(5));

%OptimizeFunctionOnNextCall(test_double_array_with_holes);
assertEquals(7, test_double_array_with_holes(5));
assertOptimized(test_double_array_with_holes);
let result_holes = test_double_array_with_holes(-1);
assertUnoptimized(test_double_array_with_holes);
assertTrue(%HasDoubleElements(result_holes));
assertEquals([1.1, , 3.3], result_holes);

// Test with special double values.
function test_double_array_special_values(n) {
  let arr = [ -0, NaN, Infinity, -Infinity, 0 ];
  if (n < 0) {
    n + 5;
    return arr;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(test_double_array_special_values);
assertEquals(7, test_double_array_special_values(5));
assertEquals(7, test_double_array_special_values(5));

%OptimizeFunctionOnNextCall(test_double_array_special_values);
assertEquals(7, test_double_array_special_values(5));
assertOptimized(test_double_array_special_values);
let result_special = test_double_array_special_values(-1);
assertUnoptimized(test_double_array_special_values);
assertTrue(%HasDoubleElements(result_special));
assertEquals([-0, NaN, Infinity, -Infinity, 0], result_special);
assertEquals(1 / -0, -Infinity);
assertEquals(1 / result_special[0], -Infinity); // Check for -0
assertTrue(isNaN(result_special[1])); // Check for NaN
assertEquals(Infinity, result_special[2]); // Check for Infinity
assertEquals(-Infinity, result_special[3]); // Check for -Infinity
assertEquals(0, result_special[4]);

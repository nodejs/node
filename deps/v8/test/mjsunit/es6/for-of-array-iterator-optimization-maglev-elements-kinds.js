// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization

function process(results, x) {
  results.push(x);
}
%NeverOptimizeFunction(process);

// Use a counter in the eval string so each call gets a distinct
// SharedFunctionInfo and a fresh feedback vector, preventing feedback
// cross-contamination between testKind invocations.
let make_test_function_counter = 0;
function make_test_function() {
  const id = make_test_function_counter++;
  return eval(`(function foo${id}(iterable) {
    let results = [];
    for (let x of iterable) {
      process(results, x);
    }
    return results;
  })`);
}

let packed_smi = [1, 2, 3];
let holey_smi = [1, , 3];
let packed_double = [1.1, 2.2, 3.3];
let holey_double = [1.1, , 3.3];
let packed_elements = ['a', 'b', 'c'];
let holey_elements = ['a', , 'c'];

function testKind(arr, expected_results, unexpected_arr, expected_unexpected_results) {
  let foo = make_test_function();
  %PrepareFunctionForOptimization(foo);
  foo(arr);
  foo(arr);

  %OptimizeMaglevOnNextCall(foo);
  assertEquals(expected_results, foo(arr));
  assertTrue(isMaglevved(foo));

  if (unexpected_arr) {
    assertEquals(expected_unexpected_results, foo(unexpected_arr));
    // Should deopt!
    assertFalse(isMaglevved(foo));
  }
}

// 1. PACKED_SMI_ELEMENTS
testKind(packed_smi, [1, 2, 3], packed_double, [1.1, 2.2, 3.3]);

// 2. HOLEY_SMI_ELEMENTS
testKind(holey_smi, [1, undefined, 3], holey_double, [1.1, undefined, 3.3]);

// 3. PACKED_DOUBLE_ELEMENTS
testKind(packed_double, [1.1, 2.2, 3.3], packed_smi, [1, 2, 3]);

// 4. HOLEY_DOUBLE_ELEMENTS
testKind(holey_double, [1.1, undefined, 3.3], holey_smi, [1, undefined, 3]);

// 5. PACKED_ELEMENTS
testKind(packed_elements, ['a', 'b', 'c'], packed_smi, [1, 2, 3]);

// 6. HOLEY_ELEMENTS
testKind(holey_elements, ['a', undefined, 'c'], holey_smi, [1, undefined, 3]);

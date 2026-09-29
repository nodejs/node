// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev

function test(arr) {
  let iter = arr[Symbol.iterator]();
  let r1 = iter.next();
  let r2 = iter.next();
  return [r1.value, r2.value];
}

let array_smi = [1, 2];
let array_double = [1.1, 2.2];

%PrepareFunctionForOptimization(test);
test(array_smi);
test(array_double);
test(array_smi);
test(array_double);
test(array_smi);
test(array_double);

%OptimizeMaglevOnNextCall(test);
let res_smi = test(array_smi);
assertTrue(isMaglevved(test));
assertEquals([1, 2], res_smi);

let res_double = test(array_double);
assertTrue(isMaglevved(test));
assertEquals([1.1, 2.2], res_double);

// Now pass a different map (holey double) to trigger deopt if it was inlined
// with a check for only the seen maps.
let array_holey_double = [1.1, , 3.3];
test(array_holey_double);

// We want to see if it deopted.
// If it deopted, isMaglevved should be false.
assertFalse(isMaglevved(test));

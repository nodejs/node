// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Constant-folding a constant Double property whose HeapNumber holds the hole
// NaN bit pattern must canonicalize the value to a quiet NaN rather than
// treating it as a hole sentinel.
const double_obj = {x: %GetHoleNaN()};
function loadConstantDoubleField() {
  return double_obj.x;
}

%PrepareFunctionForOptimization(loadConstantDoubleField);
assertTrue(Number.isNaN(loadConstantDoubleField()));
assertTrue(Number.isNaN(loadConstantDoubleField()));
%OptimizeFunctionOnNextCall(loadConstantDoubleField);
assertTrue(Number.isNaN(loadConstantDoubleField()));

// Overwriting the constant Double property must still trigger deoptimization
// via the compilation dependency recorded on the raw unboxed bits.
double_obj.x = 42;
assertEquals(42, loadConstantDoubleField());

// Also test a constant HeapObject property holding a HeapNumber with the hole
// NaN bit pattern (generalized from HeapObject + HeapNumber).
function makeHeapObjectField(v) {
  return {x: v};
}
makeHeapObjectField({});
const heap_obj = makeHeapObjectField(%GetHoleNaN());

function loadConstantHeapObjectField() {
  return heap_obj.x;
}

%PrepareFunctionForOptimization(loadConstantHeapObjectField);
assertTrue(Number.isNaN(loadConstantHeapObjectField()));
assertTrue(Number.isNaN(loadConstantHeapObjectField()));
%OptimizeFunctionOnNextCall(loadConstantHeapObjectField);
assertTrue(Number.isNaN(loadConstantHeapObjectField()));

heap_obj.x = 42;
assertEquals(42, loadConstantHeapObjectField());

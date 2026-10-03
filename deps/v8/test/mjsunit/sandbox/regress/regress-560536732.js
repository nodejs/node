// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

const kOffsetOfPropertyX = 12;
const kHeapNumberValueOffset = 4;

const holder = { x: 1.1 };
const heap_number =
    Sandbox.dereferenceTaggedPointerField(holder, kOffsetOfPropertyX);

// Corrupt the kConst double field's HeapNumber with the hole NaN bit pattern.
const kHoleNaN =
    (BigInt(%GetHoleNaNUpper()) << 32n) | BigInt(%GetHoleNaNLower() >>> 0);
Sandbox.corruptObjectField(heap_number, kHeapNumberValueOffset, kHoleNaN, 64);

let test_id = 0;
function test(optimize) {
  const opt = (0, eval)(`(function opt_${test_id++}(deopt) {
    const arr = new Array({}, holder.x);
    const obj = { val: holder.x };
    const holey_doubles = [1.1, , 2.2];
    if (deopt) {
      deopt.z;
      return [arr, obj, holey_doubles];
    }
    return 0;
  })`);

  %PrepareFunctionForOptimization(opt);
  for (let i = 0; i < 10; i++) opt(false);
  optimize(opt);
  const [arr, obj, holey_doubles] = opt({ z: 1 });
  assertTrue(Number.isNaN(arr[1]));
  assertTrue(Number.isNaN(obj.val));
  assertFalse(1 in holey_doubles);
  assertEquals(undefined, holey_doubles[1]);
}

test(fn => %OptimizeMaglevOnNextCall(fn));
test(fn => %OptimizeFunctionOnNextCall(fn));

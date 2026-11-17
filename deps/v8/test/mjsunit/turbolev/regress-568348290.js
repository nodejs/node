// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

// 1. Unwrapping ReturnedValue(HoleyFloat64) for a kNumber truncation must
// preserve the number check via CheckedHoleyFloat64ToFloat64.
(function TestReturnedValueHoleyFloat64() {
  function get_elem(arr, i) {
    return arr[i];
  }

  function opt(arr, i) {
    const x = get_elem(arr, i);
    const y = x | 0;
    return String(x) + y;
  }

  const holey = [1.5, , 2.5];
  %PrepareFunctionForOptimization(get_elem);
  %PrepareFunctionForOptimization(opt);
  assertEquals('1.51', opt(holey, 0));
  assertEquals(undefined, get_elem(holey, 1));
  assertEquals('1.51', opt(holey, 0));
  %OptimizeFunctionOnNextCall(opt);
  assertEquals('1.51', opt(holey, 0));
  assertOptimized(opt);

  assertEquals('undefined0', opt(holey, 1));
  assertUnoptimized(opt);
})();

// 2. Unwrapping HoleyFloat64ToTagged(HoleyFloat64) for a kNumber truncation
// must also preserve the number check.
(function TestHoleyFloat64ToTaggedConversion() {
  function get_elem_tagged(arr, i, flag) {
    let v = arr[i];
    if (flag) {
      v = arr[0];
    }
    return v;
  }

  function opt(arr, i, flag) {
    const x = get_elem_tagged(arr, i, flag);
    const y = x | 0;
    return String(x) + y;
  }

  const holey = [1.5, , 2.5];
  %PrepareFunctionForOptimization(get_elem_tagged);
  %PrepareFunctionForOptimization(opt);
  assertEquals('1.51', opt(holey, 0, false));
  assertEquals(undefined, get_elem_tagged(holey, 1, false));
  assertEquals(1.5, get_elem_tagged(holey, 1, true));
  assertEquals('1.51', opt(holey, 0, false));
  %OptimizeFunctionOnNextCall(opt);
  assertEquals('1.51', opt(holey, 0, false));
  assertOptimized(opt);

  assertEquals('undefined0', opt(holey, 1, false));
  assertUnoptimized(opt);
})();

// 3. A prior weaker truncation (NumberOrOddball via Math.imul) on the same
// HoleyFloat64 node must not cause a subsequent kNumber truncation to skip
// CheckedHoleyFloat64ToFloat64.
(function TestWeakerTruncationBeforeNumberTruncation() {
  function get_elem(arr, i) {
    return arr[i];
  }

  function imul(x) {
    return Math.imul(x, 3);
  }

  function opt(arr, i) {
    const x = get_elem(arr, i);
    const a = imul(x);
    const b = x | 0;
    return String(x) + a + b;
  }

  const holey = [1.5, , 2.5];
  %PrepareFunctionForOptimization(get_elem);
  %PrepareFunctionForOptimization(imul);
  %PrepareFunctionForOptimization(opt);
  assertEquals('1.531', opt(holey, 0));
  assertEquals(undefined, get_elem(holey, 1));
  assertEquals('1.531', opt(holey, 0));
  %OptimizeFunctionOnNextCall(opt);
  assertEquals('1.531', opt(holey, 0));
  assertOptimized(opt);

  assertEquals('undefined00', opt(holey, 1));
  assertUnoptimized(opt);
})();

// 4. Merging kSmi and kJSReceiverOrNullOrUndefined and intersecting with
// kNumber on `x | y` narrows `x` to kSmi, eliding CheckSmi on a subsequent
// Smi-field store. Dropping the kNumber check corrupts `smi_obj.a` with
// #undefined in a Smi field.
(function TestSmiFieldCorruptionViaTypeIntersection() {
  function get_elem(arr, i) {
    return arr[i];
  }

  const smi_obj = {a: 100};
  smi_obj.a = 1;

  function opt(arr, i, mode, bailout, y, obj, n) {
    const x = get_elem(arr, i);
    let cmp = false;
    if (mode === 1) {
      cmp = (x + 1) === 2;
    } else {
      cmp = (x === n);
      if (bailout) return 0;
    }
    const b = x | y;
    obj.a = x;
    return b + (cmp ? 1 : 0);
  }

  const smi_arr = [1, 2];
  const holey = [1, , 2.5];

  %PrepareFunctionForOptimization(opt);
  opt(smi_arr, 0, 1, false, 0.125, smi_obj, null);
  opt(smi_arr, 0, 1, false, 0.125, smi_obj, null);

  %PrepareFunctionForOptimization(get_elem);
  get_elem(holey, 0);
  get_elem(holey, 1);

  opt(holey, 1, 0, true, 0.125, smi_obj, null);
  opt(holey, 1, 0, true, 0.125, smi_obj, null);

  %OptimizeFunctionOnNextCall(opt);
  assertEquals(2, opt(holey, 0, 1, false, 0.125, smi_obj, null));
  assertOptimized(opt);

  assertEquals(0, opt(holey, 1, 0, false, 0.125, smi_obj, null));
  assertUnoptimized(opt);
  assertEquals(undefined, smi_obj.a);
  %HeapObjectVerify(smi_obj);
})();

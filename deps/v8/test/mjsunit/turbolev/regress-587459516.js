// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Case 1: CheckedHoleyFloat64ToSmiSizedInt32 on a loop phi folded to
// HoleyFloat64Constant(undefined_nan).
let slot1 = 0x40000001;
slot1 = 0x40000002;

function makeReader1() {
  return function readSlot1() {
    return slot1;
  };
}

function inner1(do_shift, pick_left, run_loop) {
  let val = undefined;
  let k = 0;
  let reader = makeReader1();
  for (; k < 2; k++) {
    const arr = [val];
    if (k >= 0 && !run_loop) break;
    val = k;
    reader = makeReader1();
  }
  if (do_shift) {
    const left = val << k;
    const right = val >> k;
    slot1 = pick_left ? left : right;
    return reader();
  }
  return 0;
}

%PrepareFunctionForOptimization(makeReader1);
%PrepareFunctionForOptimization(makeReader1());
%PrepareFunctionForOptimization(inner1);
inner1(true, true, true);
inner1(true, false, true);

function outer1(do_shift, pick_left) {
  return inner1(do_shift, pick_left, false);
}

%PrepareFunctionForOptimization(outer1);
assertEquals(0, outer1(false, false));
%OptimizeFunctionOnNextCall(outer1);
assertEquals(0, outer1(false, false));
assertEquals(0, outer1(true, true));

// Case 2: UnsafeHoleyFloat64ToInt32 in unreachable code after a dominating
// CheckedHoleyFloat64ToFloat64 whose loop-phi input collapses to undefined_nan
// only after RangeAnalysis folds the backedge; GetTaggedValue on the resulting
// empty-typed Phi must emit Abort(kUnreachable) rather than Deopt.
let constTrue = true;
let slot2 = 0x40000001;
slot2 = 0x40000002;

function makeReader2() {
  return function readSlot2() {
    return slot2;
  };
}

function getUndef() {
  let a = 0;
  for (let i = 0; i < 1; i++) a += i;
  return undefined;
}

function inner2(do_shift, pick_left, run_loop, warm_const) {
  if (!do_shift) return 0;
  let u = getUndef();
  if (warm_const) constTrue = u;
  let val = warm_const ? u : 0;
  let out = 0;
  let k = 0;
  let sum = 0.5;
  let reader = makeReader2();
  do {
    out = val;
    sum = val + 1.5;
    if (k >= 0 && !run_loop) break;
    val = 0;
    reader = makeReader2();
    k++;
  } while (k < 2);
  const left = out << k;
  const right = out >> k;
  slot2 = pick_left ? left : right;
  return reader() + (sum > 0 ? 0 : 0);
}

%PrepareFunctionForOptimization(makeReader2);
%PrepareFunctionForOptimization(makeReader2());
%PrepareFunctionForOptimization(getUndef);
%PrepareFunctionForOptimization(inner2);
getUndef();
inner2(true, true, true, false);
inner2(true, false, true, false);

function outer2(do_shift, pick_left) {
  return inner2(do_shift, pick_left, false, true);
}

%PrepareFunctionForOptimization(outer2);
assertEquals(0, outer2(false, false));
%OptimizeFunctionOnNextCall(outer2);
assertEquals(0, outer2(false, false));
assertEquals(0, outer2(true, true));

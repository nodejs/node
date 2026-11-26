// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-typed-optimizations

function dummy(x, deoptFn) {
  if (deoptFn) %DeoptimizeFunction(deoptFn);
  return x;
}
%NeverOptimizeFunction(dummy);

// Original reproducer: Uint8Array load cloned across branches, merged into a
// Phi, and refined by TypedOptimizationsReducer to Word32Constant(0) while
// captured in a FrameState.
{
  const u8 = new Uint8Array(7389);
  u8[0] = 0;

  function f(cond, doDeopt) {
    let s = 0;
    for (let i = 0; i < 4; i++) {
      s += i;
    }
    let a = false;
    if (cond === 1) {
      a = true;
    }
    let v = u8[0];
    if (a) {
      if (v >= 1) return s;
    } else {
      if (v >= 1) return s;
    }
    dummy(v, doDeopt ? f : null);
    return v + s;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(6, f(1, false));
  assertEquals(6, f(2, false));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(6, f(1, false));
  assertEquals(6, f(2, false));
  // Trigger lazy deopt at dummy(v) to verify materialized constant value.
  assertEquals(6, f(1, true));
}

// Verify constant-folded and register/stack-slot lazy deopt materialization
// across all sub-word signed/unsigned TypedArray element representations.
function testSubWordDeopt(TypedArrayCtor, minVal, values) {
  const arr = new TypedArrayCtor(128);

  // Constant-folded path: v <= minVal refines the merged Phi to minVal.
  function fConst(cond, doDeopt) {
    let s = 0;
    for (let i = 0; i < 4; i++) {
      s += i;
    }
    let a = false;
    if (cond === 1) {
      a = true;
    }
    let v = arr[0];
    if (a) {
      if (v > minVal) return s;
    } else {
      if (v > minVal) return s;
    }
    dummy(v, doDeopt ? fConst : null);
    return v + s;
  }

  arr[0] = minVal;
  %PrepareFunctionForOptimization(fConst);
  assertEquals(minVal + 6, fConst(1, false));
  assertEquals(minVal + 6, fConst(2, false));
  %OptimizeFunctionOnNextCall(fConst);
  assertEquals(minVal + 6, fConst(1, false));
  assertEquals(minVal + 6, fConst(1, true));

  // Non-constant register/stack-slot path across boundary values.
  function fReg(doDeopt) {
    let v = arr[0];
    dummy(v, doDeopt ? fReg : null);
    return v + 6;
  }

  for (const val of values) {
    arr[0] = val;
    %PrepareFunctionForOptimization(fReg);
    assertEquals(val + 6, fReg(false));
    %OptimizeFunctionOnNextCall(fReg);
    assertEquals(val + 6, fReg(false));
    assertEquals(val + 6, fReg(true));
  }
}

testSubWordDeopt(Uint8Array, 0, [0, 255]);
testSubWordDeopt(Int8Array, -128, [-128, -1, 0, 127]);
testSubWordDeopt(Uint16Array, 0, [0, 65535]);
testSubWordDeopt(Int16Array, -32768, [-32768, -1, 0, 32767]);

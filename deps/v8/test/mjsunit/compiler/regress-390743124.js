// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f0(v1, v2) {
  try {
    Array.prototype.__defineSetter__("0", () => {});
    v1.f = v2;
    v2.f = v2;
  } catch (v4) {}
  const v3 = new Int32Array();
  try {
    v3[148] = v3;
  } catch (v5) {}
}
function f1() {
  return f0(/xvxyz{1,32}?(ab|cde)\1eFa\S/suimyg);
}
class c0 {}
const v0 = new c0();
try {
  %PrepareFunctionForOptimization(f0);
  %PrepareFunctionForOptimization(f1);
} catch (v6) {}
f0(v0);
try {
  for (let v8 = 0; v8 < 5; v8++) {
    f1();
    %OptimizeFunctionOnNextCall(f1);
  }
} catch (v9) {}

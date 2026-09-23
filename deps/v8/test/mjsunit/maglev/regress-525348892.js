// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

if (%Is64Bit()) {
  const ab = new ArrayBuffer(3000000000);
  const dv = new DataView(ab);
  const ta = new Uint8Array([1, 2, 3]);
  const f64 = new Float64Array([-2147483648.0, 1.0, 2.0, 10.0]);

  function f(ta, dv, f64, i) {
    let idx = f64[i];
    let y = ta[idx];
    dv.setInt8(idx, 42);
  }

  %PrepareFunctionForOptimization(f);
  f(ta, dv, f64, 1);
  f(ta, dv, f64, 2);
  f(ta, dv, f64, 3);
  %OptimizeMaglevOnNextCall(f);

  try {
    f(ta, dv, f64, 0);
  } catch (e) {}
}

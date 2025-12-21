// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

async function* f0(a1, a2) {
  const v4 = { value: "f32" };
  for (let v5 = 0; v5 < 5; v5++) {
    for (let i9 = 12, i10 = 10;
      i10;
      (() => {
        i10--;
      })()) {
    }
    const v25 = { maxByteLength: 256 };
    for (let v26 = 0; v26 < 5; v26++) {
      function f27() {
        return f27;
      }
      return f27;
    }
  }
  return f0;
}
%PrepareFunctionForOptimization(f0);
f0(f0, f0).next(f0);
const v30 = %OptimizeFunctionOnNextCall(f0);

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f0() {
  function f1(a2, a3) {
    for (let v4 = 0; v4 < 5; v4++) {
      v4++;
      a3 = Math.min(a3, v4);
    }
    return f0;
  }
  const v11 = f1(0, 1);
  f1(31960, f1);
  return v11;
}
%PrepareFunctionForOptimization(f0);
f0();
f0();
%OptimizeMaglevOnNextCall(f0);
f0();
%OptimizeFunctionOnNextCall(f0);
f0();

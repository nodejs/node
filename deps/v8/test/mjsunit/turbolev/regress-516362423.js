// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1() {
  const v6 = 111 < 2;
  const v8 = [v6 - v6];
  function f10(a11, a12) {
    for (let v13 = 0; v13 < 5; v13++) {
      a12 = Math.min(a12, v13);
    }
  }
  f10(0, 1);
  f10(-31960, f10);
}
%PrepareFunctionForOptimization(f1);
f1();
f1();
%OptimizeMaglevOnNextCall(f1);
f1();
f1();
%OptimizeFunctionOnNextCall(f1);
f1();

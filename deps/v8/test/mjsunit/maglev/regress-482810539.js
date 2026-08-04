// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-assert-types --single-threaded

function f0() {
  let v1 = 1;
  let v2 = 0;
  for (let i3 = 1; i3 - i3, true; i3 += v1) {
    const v7 = i3++;
    v7 >> v7;
    if (v2 == 10) {
      %OptimizeOsr();
    }
    if (v2 >= 1000) {
       return;
    }
    v1 = i3;
    v2++;
  }
  return v2;
}
%PrepareFunctionForOptimization(f0);
f0();

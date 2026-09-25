// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";

function f(a, i) {
  return a[i] + a[i + 8];
}
%PrepareFunctionForOptimization(f);

let t = new Int32Array(32);
for (let i = 0; i < t.length; ++i) {
  t[i] = i * 31;
}

let base1 = f(t, 5);
let base2 = f(t, 7);

assertEquals(t[5] + t[13], base1);
assertEquals(t[7] + t[15], base2);

%OptimizeFunctionOnNextCall(f);
let optimized = f(t, 1);
assertEquals(t[1] + t[9], optimized);

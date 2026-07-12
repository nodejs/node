// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f0() {
  let v2 = 1.0;
  v2--;
  const v4 = [-8.6, -1.1];
  function f5(a6) {
    return a6.at();
  }
  %PrepareFunctionForOptimization(f5);
  try {
    // Inline a version of f5 which always throws, since the parameter is a
    // double.
    v8 = f5(v2);
  } catch (e) {}
  // Inline a version of f5 which succeeds, since the parameter is an array.
  f5(v4);
}
%PrepareFunctionForOptimization(f0);
f0();
const v11 = f0();
%OptimizeMaglevOnNextCall(f0);
f0();

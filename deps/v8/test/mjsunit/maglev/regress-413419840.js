// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --no-lazy-feedback-allocation --allow-natives-syntax

%PrepareFunctionForOptimization(%GetFunctionForCurrentFrame());

for (let v1 = 0; v1 < 5; v1++) {
  function f2() {
    return f2;
  }
  const v3 = f2.constructor;
  const v4 = new v3();
  v4.b = v3;
  f2.constructor = 2327;
  const v5 = %OptimizeOsr();
}

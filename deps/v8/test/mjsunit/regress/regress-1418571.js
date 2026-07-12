// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-turbo-loop-peeling

let v0 = "description";
for (const v1 in v0) {
  function f2(a3) {
    v0[v1];
    do {
      --v0;
    } while (a3 < 6)
  }
  %PrepareFunctionForOptimization(f2);
  %OptimizeFunctionOnNextCall(f2);
  f2();
}

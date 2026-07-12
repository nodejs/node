// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-compact

let arr = [20];
// This forces arr.concat to create a new dictionary map which can be collected
// on a GC.
arr[Symbol.isConcatSpreadable] = true;

for (let i = 0; i < 4; ++i) {
  function tmp() {
    // Creates a new map that is collected on a GC.
    let c = arr.concat();
    // Access something from c, so c's map is embedded in code object.
    c.x;
  };
  %PrepareFunctionForOptimization(tmp);
  tmp();
  // Optimize on the second iteration, so the optimized code isn't function
  // context specialized and installed on feedback vector.
  if (i == 1) {
    %OptimizeFunctionOnNextCall(tmp);
    tmp();
  }
  // Simulate full Newspace, so on next closure creation we cause a GC.
  if (i == 2) %SimulateNewspaceFull();
}

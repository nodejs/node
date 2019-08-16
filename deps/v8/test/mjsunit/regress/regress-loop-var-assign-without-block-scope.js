// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
function f() {
  // Loop with a body that's not wrapped in a block.
  for (i = 0; i < 2; i++)
    var x = i, // var x that's assigned on each iteration
        y = y||(()=>x), // single arrow function that returns x
        z0 = (%PrepareFunctionForOptimization(y)), // prepare function for optimization
        z = (%OptimizeFunctionOnNextCall(y), y()); // optimize y on first iteration
  return y()
};
assertEquals(1, f())

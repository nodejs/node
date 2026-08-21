// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis --single-threaded

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

for (let loop_counter = 0; loop_counter < 5; loop_counter++) {
  // Allocations must be inside the loop to prevent JIT loop-invariant elimination
  const buffer = new ArrayBuffer(loop_counter);
  const view = new DataView(buffer);
  const setInt16 = view.setInt16;

  loop_counter++;

  // We need a dynamic constructor lookup to prevent JIT from constant-folding
  // the constructor to 'Object'. Using an object with a method forces a unique
  // Map and prevents JIT from optimizing this to a direct 'Object(loop_counter)' call.
  const obj_with_method = { method: function() {} };
  const obj_constructor = obj_with_method.constructor;
  obj_constructor(loop_counter);

  try {
    // Trigger type contradiction: setInt16 expects DataView, gets Smi (loop_counter)
    // inside try-catch to merge kNone type into loop header.
    setInt16.call(loop_counter);
  } catch (e) {}

  %OptimizeOsr();
}

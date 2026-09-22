// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --concurrent-recompilation
// Flags: --no-turbofan

// A concurrent Maglev job that fails on the background thread must still reach
// the main thread, which is the only place that resets tiering_in_progress.
// Dropping it there leaves the flag set and locks the function out of all
// further tiering, including Turbofan.

// Keeps `count` Int32 and Float64 values live at once, so Maglev needs more
// stack slots than it is willing to emit a frame for, while the bytecode stays
// small enough to pass the size check in PrepareJobImpl.
function buildWideFrame(count) {
  let preload = '';
  let integerFold = '';
  let doubleFold = '';
  for (let i = 0; i < count; ++i) {
    preload += `0.5 + (input ^ ${i});`;
    integerFold += `integer ^= (input ^ ${i});`;
    doubleFold += `double += (input ^ ${i});`;
  }
  return Function(`
    return function wide(input) {
      input |= 0;
      let result = 0;
      for (let iteration = 0; iteration < 20; ++iteration) {
        ${preload}
        let integer = 0;
        ${integerFold}
        let double = 0.5;
        ${doubleFold}
        result = (integer + double) | 0;
      }
      return result;
    };
  `)();
}

const wide = buildWideFrame(400);
%PrepareFunctionForOptimization(wide);
const expected = wide(1);

// Establish that Maglev really does fail on this function, otherwise the check
// below would pass vacuously.
%OptimizeMaglevOnNextCall(wide);
assertEquals(expected, wide(1));
assertUnoptimized(wide);

// Now let the same failure happen on a background thread.
%OptimizeMaglevOnNextCall(wide, 'concurrent');
assertEquals(expected, wide(1));
%WaitForBackgroundOptimization();
%FinalizeOptimization();

// The job is gone, so no tiering is in progress anymore. A leaked
// tiering_in_progress keeps kOptimizingConcurrently set here and makes every
// later tiering request for `wide` a silent no-op.
assertFalse(
    (%GetOptimizationStatus(wide) &
     V8OptimizationStatus.kOptimizingConcurrently) !== 0,
    'tiering_in_progress leaked by dropped background job');

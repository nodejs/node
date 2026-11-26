// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* testGenerator(arg) {
  let total = 0;
  let outer = 0;
  let middle = 0;
  let inner = 0;
  let heap_number;
  while (outer < 1) {
    outer++;
    heap_number = arg + 0.5;
    // At the outer loop header, heap_number is dead and set to OptimizedOut.
    // The outer loop's SwitchOnGeneratorState jumps to the middle loop header
    // on resume, merging OptimizedOut with HeapNumber into a NumberOrHole Phi.
    while (middle < 1) {
      middle++;
      total += (heap_number | 0);
      // Duplicating (heap_number | 0) in both the middle loop and the inner
      // loop must emit a control-pinned CheckedTruncateTaggedToWord32 rather
      // than a pure TruncateNumberOrOddballOrHoleToWord32 that GVN and
      // ScheduleLate would hoist above the middle loop's SwitchOnGeneratorState.
      while (inner < 1) {
        inner++;
        total += (heap_number | 0);
        yield total;
        if (inner > 0) return total;
        heap_number = arg + 0.5;
      }
    }
  }
  return total;
}

%PrepareFunctionForOptimization(testGenerator);
let warmup = testGenerator(1);
assertEquals({value: 2, done: false}, warmup.next());
assertEquals({value: 2, done: true}, warmup.next());

%OptimizeFunctionOnNextCall(testGenerator);
let iter = testGenerator(1);
assertEquals({value: 2, done: false}, iter.next());
assertEquals({value: 2, done: true}, iter.next());

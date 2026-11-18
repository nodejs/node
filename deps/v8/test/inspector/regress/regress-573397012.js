// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Allocation tracking walks the JS stack on every allocation. During
// deoptimization, closures that were escape-analyzed away are materialized
// after the output frames are already on the stack, so their function slots
// still hold the arguments marker. The stack walk must skip such frames.

let {contextGroup, Protocol} =
    InspectorTest.start('Skip unmaterialized closures in allocation tracker.');

(async function test() {
  await Protocol.HeapProfiler.startTrackingHeapObjects(
      {trackAllocations: true});

  const script = `
    function deoptWithInlinedClosure(deopt) {
      let callback = function(v) {
        return deopt ? v.x : v + 1;
      };
      let sum = 0;
      for (let i = 0; i < 20; i++) {
        sum += callback(i);
      }
      return sum;
    }

    %PrepareFunctionForOptimization(deoptWithInlinedClosure);
    deoptWithInlinedClosure(false);
    deoptWithInlinedClosure(false);
    %OptimizeFunctionOnNextCall(deoptWithInlinedClosure);
    deoptWithInlinedClosure(false);
    deoptWithInlinedClosure(true);
  `;

  contextGroup.addScript(script, 0, 0, 'test.js');

  InspectorTest.log('Test passed');
  InspectorTest.completeTest();
})();

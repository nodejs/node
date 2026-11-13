// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests that deoptimizing the topmost optimized frame on debugger break ' +
    'prevents stale TypedArray length/backing-store accesses.');

contextGroup.addScript(`
let ab = new ArrayBuffer(0x40000);   // 256 KiB
let ta = new Uint32Array(ab);

let armDetach = false;

function hot(ta) {
  %ScheduleBreak();
  let sum = 0;
  const n = ta.length;
  for (let i = 0; i < n; i++) {
    sum += ta[i];
    if (i > 10) break; // Early exit keeps it fast when deoptimized
  }
  return sum;
}

function run() {
  %PrepareFunctionForOptimization(hot);
  hot(ta);
  %OptimizeFunctionOnNextCall(hot);
  hot(ta); // Trigger optimization

  %OptimizeFunctionOnNextCall(hot);
  armDetach = true;
  hot(ta); // Should deopt and not crash
}
`, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    InspectorTest.log('Paused in ' + callFrames[0].functionName);
    await Protocol.Runtime.evaluate({
      expression: 'if (armDetach) ab.transfer(0);',
    });
    await Protocol.Debugger.resume();
  });
  await Protocol.Runtime.evaluate({expression: 'run()'});
  InspectorTest.completeTest();
})();

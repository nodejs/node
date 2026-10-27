// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/572276137.');

contextGroup.addScript(`
let should_trigger = false;
let current_trigger = null;

function target() {
  if (should_trigger) {
    should_trigger = false;
    current_trigger();
  }
  return 42;
}

const triggers = [
  () => { throw new Error('boom'); },
  () => { Promise.reject(new Error('reject')).catch(() => {}); },
  () => { new Promise((_, reject) => reject(new Error('reject2'))).catch(() => {}); },
  () => { console.assert(false); },
  () => { %ScheduleBreak(); %ScheduleBreak(); (() => {})(); },
];

function runTest() {
  const results = [];
  for (const trigger of triggers) {
    current_trigger = trigger;

    // Test restartFrame from an interpreted frame.
    should_trigger = true;
    results.push(target());

    // Test restartFrame from an optimized frame (exercising lazy deopt + unwind).
    %PrepareFunctionForOptimization(target);
    should_trigger = false;
    results.push(target());
    %OptimizeFunctionOnNextCall(target);
    should_trigger = true;
    results.push(target());
  }
  return results.join(',');
}
`);

let paused_count = 0;
let should_restart = true;

Protocol.Debugger.onPaused(msg => {
  paused_count++;
  if (should_restart) {
    should_restart = false;
    const target_frame =
        msg.params.callFrames.find(f => f.functionName === 'target');
    Protocol.Debugger.restartFrame({
      callFrameId: target_frame.callFrameId,
      mode: 'StepInto',
    });
  } else {
    should_restart = true;
    Protocol.Debugger.resume();
  }
});

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  const {result} = await Protocol.Runtime.evaluate({expression: 'runTest()'});
  InspectorTest.log('results: ' + result.result.value);
  InspectorTest.log('paused_count: ' + paused_count);
  InspectorTest.completeTest();
})();

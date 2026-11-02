// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks restartFrame during an embedder breakProgram pause');

session.setupScriptMap();

contextGroup.addScript(
    `
function target() {
  inspector.breakProgram('reason', '');
  return 42;
}
function caller() {
  return target();
}
function victim(x) {
  return x + 1;
}
`,
    0, 0, 'test.js');

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  // Blackboxing keeps PrepareRestartFrame's StepInto from flooding the frames
  // with one-shot breaks, so execution resumes from the embedder pause without
  // re-entering Runtime_DebugBreakOnBytecode (which would terminate).
  await Protocol.Debugger.setBlackboxPatterns({patterns: ['test\\.js']});

  let pauses = 0;
  Protocol.Debugger.onPaused(async ({params}) => {
    ++pauses;
    if (pauses === 1) {
      // Request a restart of `caller` while paused inside the embedder's
      // breakProgram call.
      await Protocol.Debugger.restartFrame(
          {callFrameId: params.callFrames[1].callFrameId, mode: 'StepInto'});
    } else {
      await Protocol.Debugger.resume();
    }
  });

  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'caller()'});
  InspectorTest.log(
      `caller() returned ${result.value} after ${pauses} pauses.`);

  // Ensure a subsequent lazy deopt does not crash with a stale scheduled
  // frame restart.
  await Protocol.Runtime.evaluate({
    expression: `%PrepareFunctionForOptimization(victim); victim(1); victim(2);
                 %OptimizeFunctionOnNextCall(victim); victim(3);
                 %DeoptimizeFunction(victim); victim(4);`
  });

  InspectorTest.log('Done (no crash).');
  InspectorTest.completeTest();
})();

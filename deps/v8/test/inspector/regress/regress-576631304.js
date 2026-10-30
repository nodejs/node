// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that DebugEvaluate::Local write-back does not overwrite TDZ stack ' +
    'locals with undefined.');

contextGroup.addScript(`
function readTdzAfterEvaluate() {
  let a = 10;
  debugger;
  try {
    return x;
  } catch (e) {
    return a + ': ' + e.toString();
  }
  let x = 42;
}
`);

InspectorTest.runAsyncTestSuite([
  async function testTdzPreservedAcrossEvaluateOnCallFrame() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const paused = Protocol.Debugger.oncePaused();
    const evaluated =
        Protocol.Runtime.evaluate({expression: 'readTdzAfterEvaluate()'});
    const {params: {callFrames: [callFrame]}} = await paused;

    // 1. Evaluate an expression that modifies initialized local `a` while `x`
    // is still in its TDZ.
    const eval1 = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrame.callFrameId,
      expression: 'a = 25',
    });
    InspectorTest.log(`a = 25 -> ${eval1.result.result.description}`);

    // 2. Reading `x` via evaluateOnCallFrame should still throw ReferenceError.
    const eval2 = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrame.callFrameId,
      expression: 'x',
    });
    InspectorTest.log(
        `x -> ${eval2.result.exceptionDetails.exception.description.split('\n')[0]}`);

    // 3. Resuming the paused function should still observe `x` in TDZ and `a`
    // updated to 25.
    await Protocol.Debugger.resume();
    const {result: {result}} = await evaluated;
    InspectorTest.log(`Resumed result: ${result.value}`);
  },
]);

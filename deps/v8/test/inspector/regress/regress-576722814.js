// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for b/576722814.');

const source = `
function assignTdzFromDebugger() {
  debugger;
  try {
    return x;
  } catch (e) {
    return 'threw: ' + e.message;
  }
  let x = 42;
}
`;

contextGroup.addScript(source);

(async () => {
  await Protocol.Debugger.enable();

  const callPromise = Protocol.Runtime.evaluate({
    expression: 'assignTdzFromDebugger()',
  });

  const {params: {callFrames: [{callFrameId}]}} =
      await Protocol.Debugger.oncePaused();

  InspectorTest.log('Evaluating x = 99 on call frame:');
  const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'x = 99',
  });
  InspectorTest.logMessage(evalResult);

  await Protocol.Debugger.resume();
  const {result: {result: {value}}} = await callPromise;
  InspectorTest.log(`Returned: ${value}`);

  InspectorTest.completeTest();
})();

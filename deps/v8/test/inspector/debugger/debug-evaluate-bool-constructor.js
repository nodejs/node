// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noanalyze-environment-liveness

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests evaluating an object whose constructor property is a boolean.');

contextGroup.addScript(
    `
function callDebugger() {
  // Set constructor field to a non-function value.
  var a = {constructor: true};
  debugger;
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: callFrames[0].callFrameId, expression: 'a'});
    InspectorTest.log(`a.type: ${result.type}`);
    InspectorTest.log(`a.className: ${result.className}`);
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'callDebugger()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

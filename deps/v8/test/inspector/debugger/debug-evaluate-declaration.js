// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests variable and function declarations inside ' +
    'Debugger.evaluateOnCallFrame.');

contextGroup.addScript(
    `
"use strict";
var z = 1;
function runTest() {
  debugger;
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const callFrameId = callFrames[0].callFrameId;
    await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'var x = 2'});
    await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: '\'use strict\'; let y = 3'});
    await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'var z = 4'});
    await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'function bar() { return 5; }'});
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'runTest()'});

  const xRes = await Protocol.Runtime.evaluate({expression: 'x'});
  InspectorTest.log(`x: ${xRes.result.result.value}`);

  const yRes = await Protocol.Runtime.evaluate({expression: 'y'});
  InspectorTest.log(`y exception: ${yRes.result.result.className}`);

  const zRes = await Protocol.Runtime.evaluate({expression: 'z'});
  InspectorTest.log(`z: ${zRes.result.result.value}`);

  const barRes = await Protocol.Runtime.evaluate({expression: 'bar()'});
  InspectorTest.log(`bar(): ${barRes.result.result.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

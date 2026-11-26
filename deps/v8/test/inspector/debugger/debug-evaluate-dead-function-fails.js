// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that evaluating a live inner function succeeds while an unused ' +
    'dead inner function fails.');

contextGroup.addScript(
    `
function runTest() {
  "use strict";
  function live() { return 1; }
  function dead() { return 2; }
  // Reference live so it is not dead.
  live;
  debugger;
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const liveRes = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: callFrames[0].callFrameId, expression: 'live()'});
    InspectorTest.log(`live(): ${liveRes.result.result.value}`);

    const deadRes = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: callFrames[0].callFrameId, expression: 'dead()'});
    InspectorTest.log(`dead() exception: ${deadRes.result.result.className}`);
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'runTest()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

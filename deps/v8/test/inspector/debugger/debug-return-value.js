// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that callFrames[0].returnValue is available when stepping ' +
    'to function return.');

const source = `
function f(x) {
  debugger;
  if (x) {
    return 1;
  } else {
    return 2;
  }
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let currentResolve;
  let stepCount = 0;

  Protocol.Debugger.onPaused(async (msg) => {
    stepCount++;
    const topFrame = msg.params.callFrames[0];
    const loc =
        `${topFrame.location.lineNumber}:${topFrame.location.columnNumber}`;
    const retVal = topFrame.returnValue;
    InspectorTest.log(
        `Step ${stepCount} at ${loc}, returnValue: ${JSON.stringify(retVal)}`);
    if (stepCount < 4) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
      currentResolve();
    }
  });

  async function testCall(expr) {
    InspectorTest.log(`Testing: ${expr}`);
    stepCount = 0;
    const finished = new Promise((resolve) => (currentResolve = resolve));
    Protocol.Runtime.evaluate({expression: expr});
    await finished;
  }

  await testCall('f()');
  await testCall('f(true)');
  await testCall('f(false)');

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

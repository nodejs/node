// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests evaluating function name variable inside named function ' +
    'expressions.');

contextGroup.addScript(
    `
function runTests() {
  (function f() {
    f;
    debugger;  // works
  })();

  (function f() {
    () => f;
    debugger;  // works
  })();

  (function f() {
    debugger;  // throws
  })();
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let pauseCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    pauseCount++;
    const {result: {result, exceptionDetails}} =
        await Protocol.Debugger.evaluateOnCallFrame(
            {callFrameId: callFrames[0].callFrameId, expression: 'f'});
    if (exceptionDetails) {
      InspectorTest.log(
          `Pause ${pauseCount}: threw ${exceptionDetails.exception.className}`);
    } else {
      InspectorTest.log(`Pause ${pauseCount}: startsWith("function f()") = ${
          result.description.startsWith('function f()')}`);
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'runTests()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

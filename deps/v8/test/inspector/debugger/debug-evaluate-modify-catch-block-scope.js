// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests modifying catch block scope variables via evaluateOnCallFrame.');

contextGroup.addScript(`
function runTest() {
  "use strict";
  try {
    throw 1;
  } catch (e) {
    let a = 1;
    function bar() {
      a *= 2;
      e *= 2;
    }
    // Make sure bar is 'used' so that it is visible to the debugger.
    bar;
    debugger;
    if (a !== 5) throw new Error('Expected a === 5, got ' + a);
    if (e !== 7) throw new Error('Expected e === 7, got ' + e);
    return [a, e];
  }
}
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const callFrameId = callFrames[0].callFrameId;
    for (const expression of ['a = 2', 'e = 3', 'bar()', 'a++', 'e++']) {
      const {result} = await Protocol.Debugger.evaluateOnCallFrame({
        callFrameId,
        expression,
      });
      if (result.exceptionDetails) {
        InspectorTest.log('Unexpected exception for ' + expression);
      }
    }
    await Protocol.Debugger.resume();
  });

  const {result} = await Protocol.Runtime.evaluate({
    expression: 'runTest()',
    returnByValue: true,
  });
  InspectorTest.log('hasException: ' + Boolean(result.exceptionDetails));
  InspectorTest.log('result: ' + JSON.stringify(result.result.value));
  InspectorTest.completeTest();
})();

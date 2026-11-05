// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-468661.');

contextGroup.addScript(`
var sum = 0;
function testRun() {
  'use strict';

  debugger; // Break 0.

  for (let i=0; // Break 1.
       i < 1;   // Break 2.  // Break 6.
       i++) {   // Break 5.
    let key = i; // Break 3.
    sum += key;   // Break 4.
  }
} // Break 7.
`);

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  let breakCount = 0;
  Protocol.Debugger.onPaused(async message => {
    const frame = message.params.callFrames[0];
    await session.logSourceLocation(frame.location);
    const {result} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: frame.callFrameId, expression: 'i'});
    if (result.exceptionDetails) {
      InspectorTest.log('i: ' + result.result.className);
    } else {
      InspectorTest.log('i: ' + result.result.value);
    }
    breakCount++;
    if (breakCount < 9) {
      Protocol.Debugger.stepInto();
    } else {
      Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'testRun()'});
  InspectorTest.completeTest();
})();

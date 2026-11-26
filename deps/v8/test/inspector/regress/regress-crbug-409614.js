// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-409614.');

contextGroup.addScript(`
function f() {
  return 0;  // Break
}            // Break

function testRun() {
  debugger;  // Break
  f();       // Break
}            // Break
`);

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: message.params.callFrames[0].callFrameId,
      expression: 'f()'
    });
    Protocol.Debugger.stepInto();
  });
  await Protocol.Runtime.evaluate({expression: 'testRun()'});
  InspectorTest.completeTest();
})();

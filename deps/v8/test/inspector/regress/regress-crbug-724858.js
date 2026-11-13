// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/724858.');

contextGroup.addScript(`
function f(x) {
  debugger;          // B0 StepOver
  with ({}) {        // B1 StepOver
    return x         // B2 StepOver
    ;                // B3 Continue
  }
}
`);

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();
  let stepCount = 0;
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (stepCount++ < 3) {
      Protocol.Debugger.stepOver();
    } else {
      Protocol.Debugger.resume();
    }
  });
  await Protocol.Runtime.evaluate({expression: 'f(42)'});
  InspectorTest.log('stepCount: ' + stepCount);
  InspectorTest.completeTest();
})();

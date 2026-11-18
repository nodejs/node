// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping over return in async function.');

contextGroup.addScript(
    `
async function f() {
  var a = 1;
  debugger;        // B0 StepOver
  console.log(1);  // B1 StepOver
  return a +       // B2 StepOver
      0;           // B3 Continue
}
`,
    0, 0, 'test.js');

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();
  let step_count = 0;
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (step_count++ < 3) {
      Protocol.Debugger.stepOver();
    } else {
      Protocol.Debugger.resume();
    }
  });
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.completeTest();
})();

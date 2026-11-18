// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-5559.');

session.setupScriptMap();

contextGroup.addScript(
    `
function thrower() {
  try {
    debugger;             // Break 0.
    throw 'error';
  } catch (err) {
  }
}

function run() {
  thrower();
}                         // Break 1.
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  Protocol.Debugger.onPaused(async message => {
    breakCount++;
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (breakCount < 2) {
      Protocol.Debugger.stepOut();
    } else {
      Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'run()'});
  await Protocol.Debugger.disable();
  InspectorTest.log(`breakCount: ${breakCount}`);
  InspectorTest.completeTest();
})();

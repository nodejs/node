// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-119800.');

session.setupScriptMap();

contextGroup.addScript(
    `function f() {
  console.log(1);
  console.log(2);
  console.log(3);
}
function run() {
  f();
}`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 0,
    columnNumber: 0,
    url: 'test.js',
  });

  let stepCount = 0;
  Protocol.Debugger.onPaused(async message => {
    stepCount++;
    await Protocol.Debugger.setBreakpointsActive({active: false});
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (stepCount < 5) {
      Protocol.Debugger.stepInto();
    } else {
      Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'run()'});
  await Protocol.Debugger.setBreakpointsActive({active: true});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

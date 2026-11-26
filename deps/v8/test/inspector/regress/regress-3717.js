// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-3717.');

session.setupScriptMap();

contextGroup.addScript(
    `function f() {
  function g(p) {
    return 1;
  }
  g(1);
}`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 2,
    columnNumber: 0,
    url: 'test.js',
  });

  let breakCount = 0;
  Protocol.Debugger.onPaused(async message => {
    breakCount++;
    await session.logSourceLocation(message.params.callFrames[0].location);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  await Protocol.Debugger.removeBreakpoint({breakpointId});
  await Protocol.Debugger.disable();
  InspectorTest.log(`breakCount: ${breakCount}`);
  InspectorTest.completeTest();
})();

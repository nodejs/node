// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that debugger statement inside evaluateOnCallFrame does not trigger a nested pause.');

contextGroup.addScript(`
function a() {
} // BREAK
`, 0, 0, 'test.js');

session.setupScriptMap();

(async function() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  Protocol.Runtime.onConsoleAPICalled(
      message => InspectorTest.log(message.params.args[0].value));
  let pauseCount = 0;
  Protocol.Debugger.onPaused(async message => {
    pauseCount++;
    InspectorTest.log(`pause #${pauseCount}:`);
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (pauseCount === 1) {
      await Protocol.Debugger.evaluateOnCallFrame({
        callFrameId: message.params.callFrames[0].callFrameId,
        expression: "console.log('A');\ndebugger;\nconsole.log('B');",
      });
      await Protocol.Debugger.resume();
    }
  });
  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 1,
    columnNumber: 0,
    url: 'test.js',
  });
  await Protocol.Runtime.evaluate({expression: 'a()'});
  InspectorTest.completeTest();
})();

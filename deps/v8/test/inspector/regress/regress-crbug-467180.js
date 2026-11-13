// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-467180.');

contextGroup.addScript(
    `
function f() {
  for (var i = 10; i < 14; i++) {  // 1
    i *= 1;                        // 2
  }
}                                  // 3
`,
    0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 3, columnNumber: 0, url: 'test.js', condition: 'i == 12'});

  Protocol.Debugger.onPaused(async message => {
    const frame = message.params.callFrames[0];
    await session.logSourceLocation(frame.location);
    if (frame.location.lineNumber === 3) {
      const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: frame.callFrameId, expression: 'i'});
      InspectorTest.log('i = ' + result.value);
    }
    Protocol.Debugger.stepOver();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.completeTest();
})();

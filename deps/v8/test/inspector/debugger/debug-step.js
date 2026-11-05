// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests performing 1000 stepInto steps.');

const source = `
function f() {
  var i;                        // Line 1.
  for (i = 0; i < 1000; i++) {  // Line 2.
    x = 1;                      // Line 3.
  }
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 2,
    url: 'test.js',
  });

  let stepCount = 1000;
  let evaluatedValue = -1;

  Protocol.Debugger.onPaused(async message => {
    if (stepCount > 0) {
      stepCount--;
      Protocol.Debugger.stepInto();
    } else {
      let callFrameId = message.params.callFrames[0].callFrameId;
      let {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
        callFrameId,
        expression: 'i',
      });
      evaluatedValue = result.value;
      InspectorTest.log('Evaluated i: ' + evaluatedValue);
      await Protocol.Debugger.removeBreakpoint({breakpointId});
      Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'f();'});
  InspectorTest.completeTest();
})();

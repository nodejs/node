// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests declaring var in evaluateOnCallFrame when outer scope has context-allocated parameter.');

contextGroup.addScript(
    `
function foo(arg) {
  return function() {
    // Context allocate arg.
    arg;
    debugger;
  }
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'var a = 1'});
    InspectorTest.logMessage(result.exceptionDetails.exception.description);
    Protocol.Debugger.resume();
  });
  await Protocol.Runtime.evaluate({expression: 'foo()()'});
  InspectorTest.completeTest();
})();

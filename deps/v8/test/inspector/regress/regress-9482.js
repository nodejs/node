// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests evaluating outer function variables when inner function calls eval.');

contextGroup.addScript(
    `
function run() {
  (function foo () {
     var n = "n";
     var m = "m";
     (function bar() {
       eval("m"); // force context-allocation.
       debugger;
     })();
  })();
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result: {result: resN}} =
        await Protocol.Debugger.evaluateOnCallFrame(
            {callFrameId, expression: 'n'});
    InspectorTest.logMessage(resN);
    const {result: {result: resM}} =
        await Protocol.Debugger.evaluateOnCallFrame(
            {callFrameId, expression: 'm'});
    InspectorTest.logMessage(resM);
    Protocol.Debugger.resume();
  });
  await Protocol.Runtime.evaluate({expression: 'run()'});
  InspectorTest.completeTest();
})();

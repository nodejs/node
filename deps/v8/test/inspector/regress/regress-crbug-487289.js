// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-487289.');

contextGroup.addScript(`
function f() { debugger; }
var expected = {};
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'this === expected'});
    InspectorTest.log('this === expected: ' + result.value);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f.call(expected)'});
  InspectorTest.completeTest();
})();

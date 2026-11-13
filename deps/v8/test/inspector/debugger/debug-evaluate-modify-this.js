// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that modifying this via evaluateOnCallFrame throws.');

contextGroup.addScript(`
var f = () => { debugger; }
var g = function() { debugger; }
var h = (function() { return () => { debugger; }; }).call({});
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'this = 2',
    });
    InspectorTest.log(
        'throws: ' + Boolean(result.exceptionDetails) + ' ' +
        result.result.className);
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f(); g(); g.call({}); h();'});
  InspectorTest.completeTest();
})();

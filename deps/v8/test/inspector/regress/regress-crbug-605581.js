// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-605581.');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'bar.baz'});
    InspectorTest.logMessage(result);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({
    expression: `(function() {
  debugger;    // bar is still in TDZ at this point.
  let bar = 1;
  (x => bar);  // force bar to be context-allocated.
})();`
  });
  InspectorTest.completeTest();
})();

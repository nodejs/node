// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-old-space-size=16

let {session, contextGroup, Protocol} = InspectorTest.start('Check pause on OOM');

contextGroup.addScript(`
var arr = [];
var stop = false;
function generateGarbage() {
  while(!stop) {
    arr.push(new Array(1000));
  }
}
//# sourceURL=test.js`, 10, 26);

Protocol.Debugger.onPaused((message) => {
  InspectorTest.log(`reason: ${message.params.reason}`);
  Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: message.params.callFrames[0].callFrameId,
    expression: 'arr = []; stop = true;'
  }).then(() => Protocol.Debugger.resume());
});
Protocol.Debugger.enable();
Protocol.Runtime.evaluate({ expression: 'generateGarbage()' })
  .then(InspectorTest.completeTest);

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-old-space-size=16

let { session, contextGroup, Protocol } = InspectorTest.start('Check pause on OOM');

var script = `
var arr = [];
var stop = false;
function generateGarbage() {
  while(!stop) {`

  // Force the JumpLoop to be Wide.
for (i = 0; i < 37; ++i) {
  script += `arr.push(new Array(1000));`
  script += `if (stop) { break; }`
}

script += `
    }
  }
  //# sourceURL=test.js"`

contextGroup.addScript(script, 10, 26);

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

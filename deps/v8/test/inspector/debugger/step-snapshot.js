// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Embed a user function in the snapshot and step through it.

// Flags: --embed 'function c(f, ...args) { return f(...args); }'

InspectorTest.setupScriptMap();

InspectorTest.addScript(`
function test() {
  function f(x) {
    return x * 2;
  }
  debugger;
  c(f, 2);
}
//# sourceURL=test.js`);

Protocol.Debugger.onPaused(message => {
  InspectorTest.log("paused");
  var frames = message.params.callFrames;
  InspectorTest.logSourceLocation(frames[0].location);
  Protocol.Debugger.stepInto();
})

Protocol.Debugger.enable()
  .then(() => Protocol.Runtime.evaluate({ expression: 'test()' }))
  .then(InspectorTest.completeTest);

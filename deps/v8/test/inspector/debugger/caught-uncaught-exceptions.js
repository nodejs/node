// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Check that inspector correctly passes caught/uncaught information.");

contextGroup.addScript(
`function throwCaught() { try { throw new Error(); } catch (_) {} }
 function throwUncaught() { throw new Error(); }
 function schedule(f) { setTimeout(f, 0); }
`);

Protocol.Debugger.enable();

Protocol.Debugger.setPauseOnExceptions({ "state": "all" });
Protocol.Debugger.onPaused(message => {
  InspectorTest.log("paused in " + message.params.callFrames[0].functionName);
  InspectorTest.log("uncaught: " + message.params.data.uncaught);
  Protocol.Debugger.resume();
});

Protocol.Runtime.evaluate({ "expression": "schedule(throwCaught);" })
  .then(() => Protocol.Runtime.evaluate(
      { "expression": "schedule(throwUncaught);" }))
  .then(() => InspectorTest.completeTest());

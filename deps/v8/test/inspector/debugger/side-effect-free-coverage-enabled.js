// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests side-effect-free evaluation with coverage enabled');

contextGroup.addScript(`
function testFunction()
{
  var o = 0;
  function f() { return 1; }
  function g() { o = 2; return o; }
  f,g;
  debugger;
}
//# sourceURL=foo.js`);

// Side effect free call should not result in EvalError when coverage
// is enabled:
Protocol.Profiler.enable()
Protocol.Profiler.startPreciseCoverage({callCount: true, detailed: true})

Protocol.Debugger.enable();

Protocol.Debugger.oncePaused().then(debuggerPaused);

Protocol.Runtime.evaluate({ "expression": "setTimeout(testFunction, 0)" });

var topFrameId;

function debuggerPaused(messageObject)
{
  InspectorTest.log("Paused on 'debugger;'");

  topFrameId = messageObject.params.callFrames[0].callFrameId;
  Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: "f()", throwOnSideEffect: true}).then(evaluatedFirst);
}

function evaluatedFirst(response)
{
  InspectorTest.log("f() returns " + response.result.result.value);
  InspectorTest.completeTest();
}

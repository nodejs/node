// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --turbo
let {session, contextGroup, Protocol} = InspectorTest.start('Tests side-effect-free evaluation');

contextGroup.addScript(`
function testFunction()
{
  var o = 0;
  function f() { return 1; }
  function g() { o = 2; return o; }
  debugger;
}
//# sourceURL=foo.js`);

Protocol.Debugger.enable();

Protocol.Debugger.oncePaused().then(debuggerPaused);

Protocol.Runtime.evaluate({ "expression": "setTimeout(testFunction, 0)" });

var topFrameId;

function debuggerPaused(messageObject)
{
  InspectorTest.log("Paused on 'debugger;'");

  topFrameId = messageObject.params.callFrames[0].callFrameId;
  Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: "f()"}).then(evaluatedFirst);
}

function evaluatedFirst(response)
{
  InspectorTest.log("f() returns " + response.result.result.value);
  Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: "g()"}).then(evaluatedSecond);
}

function evaluatedSecond(response)
{
  InspectorTest.log("g() returns " + response.result.result.value);
  Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: "f()", throwOnSideEffect: true}).then(evaluatedThird);
}

function evaluatedThird(response)
{
  InspectorTest.log("f() returns " + response.result.result.value);
  Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: "g()", throwOnSideEffect: true}).then(evaluatedFourth);
  InspectorTest.completeTest();
}

function evaluatedFourth(response)
{
  InspectorTest.log("g() throws " + response.result.result.className);
  InspectorTest.completeTest();
}

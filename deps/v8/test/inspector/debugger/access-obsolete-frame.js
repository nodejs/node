// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that accessing no longer valid call frames returns an error');

contextGroup.addScript(`
function testFunction()
{
  debugger;
}
//# sourceURL=foo.js`);

Protocol.Debugger.enable();

Protocol.Debugger.oncePaused().then(handleDebuggerPausedOne);

Protocol.Runtime.evaluate({ "expression": "setTimeout(testFunction, 0)" });

var obsoleteTopFrameId;

function handleDebuggerPausedOne(messageObject)
{
  InspectorTest.log("Paused on 'debugger;'");

  var topFrame = messageObject.params.callFrames[0];
  obsoleteTopFrameId = topFrame.callFrameId;

  Protocol.Debugger.resume().then(callbackResume);
}

function callbackResume(response)
{
  InspectorTest.log("resume");
  InspectorTest.log("restartFrame");
  Protocol.Debugger.restartFrame({ callFrameId: obsoleteTopFrameId }).then(callbackRestartFrame);
}

function callbackRestartFrame(response)
{
  logErrorResponse(response);
  InspectorTest.log("evaluateOnFrame");
  Protocol.Debugger.evaluateOnCallFrame({ callFrameId: obsoleteTopFrameId, expression: "0"}).then(callbackEvaluate);
}

function callbackEvaluate(response)
{
  logErrorResponse(response);
  InspectorTest.log("setVariableValue");
  Protocol.Debugger.setVariableValue({ callFrameId: obsoleteTopFrameId, scopeNumber: 0, variableName: "a", newValue: { value: 0 } }).then(callbackSetVariableValue);
}

function callbackSetVariableValue(response)
{
  logErrorResponse(response);
  InspectorTest.completeTest();
}

function logErrorResponse(response)
{
  if (response.error) {
    if (response.error.message.indexOf("Can only perform operation while paused.") !== -1) {
      InspectorTest.log("PASS, error message as expected");
      return;
    }
  }
  InspectorTest.log("FAIL, unexpected error message");
  InspectorTest.log(JSON.stringify(response));
}

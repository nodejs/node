// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests updating call frame scopes');

contextGroup.addScript(
`function TestFunction()
{
    var a = 2;
    debugger;
    debugger;
}`);

var newVariableValue = 55;

Protocol.Debugger.enable();

Protocol.Debugger.oncePaused().then(handleDebuggerPaused);

Protocol.Runtime.evaluate({ "expression": "setTimeout(TestFunction, 0)" });

function handleDebuggerPaused(messageObject)
{
  InspectorTest.log("Paused on 'debugger;'");

  var topFrame = messageObject.params.callFrames[0];
  var topFrameId = topFrame.callFrameId;
  Protocol.Debugger.evaluateOnCallFrame({ "callFrameId": topFrameId, "expression": "a = " + newVariableValue }).then(callbackChangeValue);
}

function callbackChangeValue(response)
{
  InspectorTest.log("Variable value changed");
  Protocol.Debugger.oncePaused().then(callbackGetBacktrace);
  Protocol.Debugger.resume();
}

function callbackGetBacktrace(response)
{
  InspectorTest.log("Stacktrace re-read again");
  var localScope = response.params.callFrames[0].scopeChain[0];
  Protocol.Runtime.getProperties({ "objectId": localScope.object.objectId }).then(callbackGetProperties);
}

function callbackGetProperties(response)
{
  InspectorTest.log("Scope variables downloaded anew");
  var varNamedA;
  var propertyList = response.result.result;
  for (var i = 0; i < propertyList.length; i++) {
    if (propertyList[i].name === "a") {
      varNamedA = propertyList[i];
      break;
    }
  }
  if (varNamedA) {
    var actualValue = varNamedA.value.value;
    InspectorTest.log("New variable is " + actualValue + ", expected is " + newVariableValue + ", old was: 2");
    InspectorTest.log(actualValue === newVariableValue ? "SUCCESS" : "FAIL");
  } else {
    InspectorTest.log("Failed to find variable in scope");
  }
  InspectorTest.completeTest();
}

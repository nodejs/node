// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that function location in call frames is correct');

contextGroup.addScript(
`function testFunction()
{
    var a = 2;
    debugger;
}`);

Protocol.Debugger.enable();
Protocol.Debugger.oncePaused().then(handleDebuggerPaused);
Protocol.Runtime.evaluate({ "expression": "setTimeout(testFunction, 0)" });

function handleDebuggerPaused(messageObject)
{
  InspectorTest.log("Paused on 'debugger;'");
  var topFrame = messageObject.params.callFrames[0];
  topFrame.location.scriptId = "42";
  topFrame.functionLocation.scriptId = "42";
  InspectorTest.log("Top frame location: " + JSON.stringify(topFrame.location));
  InspectorTest.log("Top frame functionLocation: " + JSON.stringify(topFrame.functionLocation));
  InspectorTest.completeTest();
}

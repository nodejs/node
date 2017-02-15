// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Check that stepInto at then end of the script go to next user script instead InjectedScriptSource.js.");

InspectorTest.addScript(
`function foo()
{
    return 239;
}`);

Protocol.Debugger.enable();
Protocol.Debugger.onPaused(debuggerPaused);
Protocol.Runtime.evaluate({ "expression": "(function boo() { setTimeout(foo, 0); debugger; })()" });

var actions = [ "stepInto", "stepInto", "stepInto" ];
function debuggerPaused(result)
{
  InspectorTest.log("Stack trace:");
  for (var callFrame of result.params.callFrames)
    InspectorTest.log(callFrame.functionName + ":" + callFrame.location.lineNumber + ":" + callFrame.location.columnNumber);
  InspectorTest.log("");

  var action = actions.shift();
  if (!action) {
    Protocol.Debugger.resume().then(InspectorTest.completeTest);
    return;
  }
  InspectorTest.log("Perform " + action);
  Protocol.Debugger[action]();
}

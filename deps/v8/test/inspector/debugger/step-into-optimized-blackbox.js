// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping with blackboxing and inlining');

contextGroup.addScript(
`function bar() {
  return 1 + foo();
}
//# sourceURL=bar.js`);

contextGroup.addScript(
`function foo() {
  return "foo";
}
//# sourceURL=foo.js`);

Protocol.Debugger.enable();
Protocol.Debugger.setBlackboxPatterns({ patterns: [ "bar.js" ] });

Protocol.Debugger.onPaused(PerformSteps);
Protocol.Runtime.evaluate({
  "expression": "bar(); bar(); %OptimizeFunctionOnNextCall(bar); bar()"
});
Protocol.Runtime.evaluate({ "expression": "debugger; bar();" });

var commands = [ "stepInto", "stepInto" ];

function PerformSteps(message) {
  InspectorTest.log("Paused in");
  var callFrames = message.params.callFrames;
  for (var callFrame of callFrames) {
    InspectorTest.log(
        (callFrame.functionName || "(...)") + ":" + (callFrame.location.lineNumber + 1));
  }
  var command = commands.shift();
  if (!command) {
    InspectorTest.completeTest();
    return;
  }
  Protocol.Debugger[command]();
}

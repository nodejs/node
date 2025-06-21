// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests blackboxing by patterns');

contextGroup.addScript(
`function bar()
{
    return 42;
}`);

contextGroup.addScript(
`function foo()
{
    var a = bar();
    return a + 1;
}
//# sourceURL=foo.js`);

contextGroup.addScript(
`function qwe()
{
    var a = foo();
    return a + 1;
}
//# sourceURL=qwe.js`);

contextGroup.addScript(
`function baz()
{
    var a = qwe();
    return a + 1;
}
//# sourceURL=baz.js`);

Protocol.Debugger.enable();
Protocol.Debugger.setBlackboxPatterns({ patterns: [ "foo([" ] }).then(dumpError);

function dumpError(message)
{
  InspectorTest.log(message.error.message);
  Protocol.Debugger.onPaused(dumpStackAndRunNextCommand);
  Protocol.Debugger.setBlackboxPatterns({ patterns: [ "baz\.js", "foo\.js" ] });
  Protocol.Runtime.evaluate({ "expression": "debugger;baz()" });
}

var commands = [ "stepInto", "stepInto", "stepInto", "stepOut", "stepInto", "stepInto" ];
function dumpStackAndRunNextCommand(message)
{
  InspectorTest.log("Paused in");
  var callFrames = message.params.callFrames;
  for (var callFrame of callFrames)
    InspectorTest.log((callFrame.functionName || "(...)") + ":" + (callFrame.location.lineNumber + 1));
  var command = commands.shift();
  if (!command) {
    InspectorTest.completeTest();
    return;
  }
  Protocol.Debugger[command]();
}

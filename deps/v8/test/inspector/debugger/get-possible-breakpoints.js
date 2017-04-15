// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Test for Debugger.getPossibleBreakpoints");

Protocol.Runtime.enable();
Protocol.Debugger.enable();

InspectorTest.runTestSuite([

  function getPossibleBreakpointsInRange(next) {
    var source = "function foo(){ return Promise.resolve(); }\nfunction boo(){ return Promise.resolve().then(() => 42); }\n\n";
    var scriptId;
    compileScript(source)
      .then(id => scriptId = id)
      .then(() => InspectorTest.log("Test start.scriptId != end.scriptId."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }, end: { lineNumber: 0, columnNumber: 0, scriptId: scriptId + "0" }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test not existing scriptId."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: "-1" }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test end < start."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 1, columnNumber: 0, scriptId: scriptId }, end: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test empty range in first line."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 16, scriptId: scriptId }, end: { lineNumber: 0, columnNumber: 16, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test one character range in first line."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 16, scriptId: scriptId }, end: { lineNumber: 0, columnNumber: 17, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test empty range in not first line."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 1, columnNumber: 16, scriptId: scriptId }, end: { lineNumber: 1, columnNumber: 16, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test one character range in not first line."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 1, columnNumber: 16, scriptId: scriptId }, end: { lineNumber: 1, columnNumber: 17, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test end is undefined"))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test end.lineNumber > scripts.lineCount()"))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }, end: { lineNumber: 5, columnNumber: 0, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test one string"))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }, end: { lineNumber: 1, columnNumber: 0, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test end.columnNumber > end.line.length(), should be the same as previous."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }, end: { lineNumber: 0, columnNumber: 256, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(next);
  },

  function getPossibleBreakpointsInArrow(next) {
    var source = "function foo() { return Promise.resolve().then(() => 239).then(() => 42).then(() => () => 42) }";
    var scriptId;
    compileScript(source)
      .then(id => scriptId = id)
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(next);
  },

  function arrowFunctionFirstLine(next) {
    Protocol.Debugger.onPaused(message => {
      InspectorTest.log("paused in " + message.params.callFrames[0].functionName);
      InspectorTest.logMessage(message.params.callFrames[0].location);
      Protocol.Debugger.resume();
    });

    var source = `function foo1() { Promise.resolve().then(() => 42) }
function foo2() { Promise.resolve().then(() => 42) }`;
    waitForPossibleBreakpoints(source, { lineNumber: 0, columnNumber: 0 }, { lineNumber: 1, columnNumber: 0 })
      .then(InspectorTest.logMessage)
      .then(setAllBreakpoints)
      .then(() => Protocol.Runtime.evaluate({ expression: "foo1(); foo2()"}))
      .then(next);
  },

  function arrowFunctionOnPause(next) {
    function dumpAndResume(message) {
      InspectorTest.log("paused in " + message.params.callFrames[0].functionName);
      InspectorTest.logMessage(message.params.callFrames[0].location);
      Protocol.Debugger.resume();
    }

    var source = `debugger; function foo3() { Promise.resolve().then(() => 42) }
function foo4() { Promise.resolve().then(() => 42) };\nfoo3();\nfoo4();`;
    waitForPossibleBreakpointsOnPause(source, { lineNumber: 0, columnNumber: 0 }, undefined, next)
      .then(InspectorTest.logMessage)
      .then(setAllBreakpoints)
      .then(() => Protocol.Debugger.onPaused(dumpAndResume))
      .then(() => Protocol.Debugger.resume());
  },

  function getPossibleBreakpointsInRangeWithOffset(next) {
    var source = "function foo(){ return Promise.resolve(); }\nfunction boo(){ return Promise.resolve().then(() => 42); }\n\n";
    var scriptId;
    compileScript(source, { name: "with-offset.js", line_offset: 1, column_offset: 1 })
      .then(id => scriptId = id)
      .then(() => InspectorTest.log("Test empty range in first line."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 1, columnNumber: 17, scriptId: scriptId }, end: { lineNumber: 1, columnNumber: 17, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test one character range in first line."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 1, columnNumber: 17, scriptId: scriptId }, end: { lineNumber: 1, columnNumber: 18, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test empty range in not first line."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 2, columnNumber: 16, scriptId: scriptId }, end: { lineNumber: 2, columnNumber: 16, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test one character range in not first line."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 2, columnNumber: 16, scriptId: scriptId }, end: { lineNumber: 2, columnNumber: 17, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test end is undefined"))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test end.lineNumber > scripts.lineCount()"))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId }, end: { lineNumber: 5, columnNumber: 0, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test one string"))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 1, columnNumber: 1, scriptId: scriptId }, end: { lineNumber: 2, columnNumber: 0, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(() => InspectorTest.log("Test end.columnNumber > end.line.length(), should be the same as previous."))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 1, columnNumber: 1, scriptId: scriptId }, end: { lineNumber: 1, columnNumber: 256, scriptId: scriptId }}))
      .then(InspectorTest.logMessage)
      .then(next);
  },

  function withOffset(next) {
    Protocol.Debugger.onPaused(message => {
      InspectorTest.log("paused in " + message.params.callFrames[0].functionName);
      InspectorTest.logMessage(message.params.callFrames[0].location);
      Protocol.Debugger.resume();
    });

    var source = `function foo5() { Promise.resolve().then(() => 42) }
function foo6() { Promise.resolve().then(() => 42) }`;
    waitForPossibleBreakpoints(source, { lineNumber: 0, columnNumber: 0 }, undefined, { name: "with-offset.js", line_offset: 3, column_offset: 18 })
      .then(InspectorTest.logMessage)
      .then(setAllBreakpoints)
      .then(() => Protocol.Runtime.evaluate({ expression: "foo5(); foo6()"}))
      .then(next);
  },

  function arrowFunctionReturn(next) {
    waitForPossibleBreakpoints("() => 239\n", { lineNumber: 0, columnNumber: 0 })
      .then(InspectorTest.logMessage)
      .then(() => waitForPossibleBreakpoints("function foo() { function boo() { return 239 }  }\n", { lineNumber: 0, columnNumber: 0 }))
      .then(InspectorTest.logMessage)
      .then(() => waitForPossibleBreakpoints("() => { 239 }\n", { lineNumber: 0, columnNumber: 0 }))
      .then(InspectorTest.logMessage)
      // TODO(kozyatinskiy): lineNumber for return position should be 21 instead of 22.
      .then(() => waitForPossibleBreakpoints("function foo() { 239 }\n", { lineNumber: 0, columnNumber: 0 }))
      .then(InspectorTest.logMessage)
      // TODO(kozyatinskiy): lineNumber for return position should be only 9, not 8.
      .then(() => waitForPossibleBreakpoints("() => 239", { lineNumber: 0, columnNumber: 0 }))
      .then(InspectorTest.logMessage)
      // TODO(kozyatinskiy): lineNumber for return position should be only 19, not 20.
      .then(() => waitForPossibleBreakpoints("() => { return 239 }", { lineNumber: 0, columnNumber: 0 }))
      .then(InspectorTest.logMessage)
      .then(next)
  }
]);

function compileScript(source, origin) {
  var promise = Protocol.Debugger.onceScriptParsed().then(message => message.params.scriptId);
  if (!origin) origin = { name: "", line_offset: 0, column_offset: 0 };
  compileAndRunWithOrigin(source, origin.name, origin.line_offset, origin.column_offset);
  return promise;
}

function waitForPossibleBreakpoints(source, start, end, origin) {
  return compileScript(source, origin)
    .then(scriptId => { (start || {}).scriptId = scriptId; (end || {}).scriptId = scriptId })
    .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: start, end: end }));
}

function waitForPossibleBreakpointsOnPause(source, start, end, next) {
  var promise = Protocol.Debugger.oncePaused()
    .then(msg => { (start || {}).scriptId = msg.params.callFrames[0].location.scriptId; (end || {}).scriptId = msg.params.callFrames[0].location.scriptId })
    .then(() => Protocol.Debugger.getPossibleBreakpoints({ start: start, end: end }));
  Protocol.Runtime.evaluate({ expression: source }).then(next);
  return promise;
}

function setAllBreakpoints(message) {
  var promises = [];
  for (var location of message.result.locations)
    promises.push(Protocol.Debugger.setBreakpoint({ location: location }).then(checkBreakpointAndDump));
  return Promise.all(promises);
}

function checkBreakpointAndDump(message) {
  if (message.error) {
    InspectorTest.log("FAIL: error in setBreakpoint");
    InspectorTest.logMessage(message);
    return;
  }
  var id_data = message.result.breakpointId.split(":");
  if (parseInt(id_data[1]) !== message.result.actualLocation.lineNumber || parseInt(id_data[2]) !== message.result.actualLocation.columnNumber) {
    InspectorTest.log("FAIL: possible breakpoint was resolved in another location");
    InspectorTest.logMessage(message);
  }
  InspectorTest.logMessage(message);
}

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Embed a user function in the snapshot and listen for scriptParsed events.

// Flags: --embed 'function f() { return 42; }; f();'
// Flags: --no-turbo-rewrite-far-jumps

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that getPossibleBreakpoints works on a snapshotted function');
session.setupScriptMap();

Protocol.Debugger.onScriptParsed(requestSourceAndDump);

Protocol.Debugger.enable()
  .then(InspectorTest.waitForPendingTasks)
  .then(InspectorTest.completeTest);

function requestSourceAndDump(scriptParsedMessage) {
  const scriptId = scriptParsedMessage.params.scriptId;
  Protocol.Debugger.getScriptSource({ scriptId: scriptId })
    .then((sourceMessage) => dumpScriptParsed(
        scriptParsedMessage, sourceMessage))
    .then(() => Protocol.Debugger.getPossibleBreakpoints({
        start: { lineNumber: 0, columnNumber: 0, scriptId: scriptId },
        end:   { lineNumber: 0, columnNumber: 1, scriptId: scriptId },
        restrictToFunction: false
        }))
    .then(InspectorTest.logMessage);
}

function dumpScriptParsed(scriptParsedMessage, sourceMessage) {
  var sourceResult = sourceMessage.result;
  sourceResult.scriptSource = sourceResult.scriptSource.replace(/\n/g, "<nl>");
  InspectorTest.log("scriptParsed");
  InspectorTest.logObject(sourceResult);
  InspectorTest.logMessage(scriptParsedMessage.params);
}

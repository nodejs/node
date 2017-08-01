// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that stepping works after calling getPossibleBreakpoints');

contextGroup.addScript(`
function boo() {}
boo();
function foo() {}
//# sourceURL=foo.js`);

Protocol.Debugger.onPaused((message) => {
  InspectorTest.logMessage(message.params.callFrames[0].functionName || "(top)");
  Protocol.Debugger.stepInto();
});
var scriptId;
Protocol.Debugger.onScriptParsed(message => {
  if (message.params.url === 'foo.js')
    scriptId = message.params.scriptId;
});
Protocol.Debugger.enable()
  .then(() => Protocol.Debugger.getPossibleBreakpoints({start: {scriptId, lineNumber:0,columnNumber:0}}))
  .then(() => InspectorTest.log('-- call boo:'))
  .then(() => Protocol.Runtime.evaluate({ expression: 'debugger; boo();'}))
  .then(() => InspectorTest.log('-- call foo:'))
  .then(() => Protocol.Runtime.evaluate({ expression: 'debugger; foo();'}))
  .then(InspectorTest.completeTest);

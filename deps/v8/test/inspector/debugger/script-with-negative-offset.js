// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Locations in script with negative offset.');

(async function test() {
  contextGroup.addScript(`function foo() { debugger; }
function boo(){ debugger; }
`, -1, -1);
  session.setupScriptMap();
  Protocol.Debugger.enable();
  let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
  let {result:{locations}} = await Protocol.Debugger.getPossibleBreakpoints({
    start: {scriptId, lineNumber: 0, columnNumber: 0}
  });
  InspectorTest.logMessage(locations);

  Protocol.Runtime.evaluate({expression: 'foo()'});
  var {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  session.logCallFrames(callFrames);
  await Protocol.Debugger.resume();

  Protocol.Runtime.evaluate({expression: 'boo()'});
  var {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  session.logCallFrames(callFrames);
  await Protocol.Debugger.resume();

  InspectorTest.completeTest();
})();

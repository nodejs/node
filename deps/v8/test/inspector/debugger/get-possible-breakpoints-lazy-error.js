// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('getPossibleBreakpoints should not crash during lazy compilation (crbug.com/715334)');

contextGroup.addScript(`
function test() { continue; }
//# sourceURL=test.js`);

(async function test() {
  Protocol.Debugger.enable();
  let script = await Protocol.Debugger.onceScriptParsed();
  InspectorTest.logMessage(script);
  let scriptId = script.params.scriptId;
  Protocol.Debugger.onScriptFailedToParse(msg => {
    InspectorTest.logMessage(msg);
    if (msg.params.scriptId !== script.params.scriptId) {
      InspectorTest.log('Failed script to parse event has different scriptId');
    } else {
      InspectorTest.log('One script is reported twice');
    }
  });
  let response = await Protocol.Debugger.getPossibleBreakpoints({
    start: {scriptId, lineNumber: 0, columnNumber: 0}});
  InspectorTest.logMessage(response);
  InspectorTest.completeTest();
})();

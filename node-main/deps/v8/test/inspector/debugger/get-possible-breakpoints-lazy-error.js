// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('getPossibleBreakpoints should not crash during lazy compilation (crbug.com/715334)');
  (async function test() {
    Protocol.Debugger.enable();
    Protocol.Debugger.onScriptFailedToParse(async msg => {
      InspectorTest.logMessage(msg);
      const response = await Protocol.Debugger.getPossibleBreakpoints({
        start: {scriptId: msg.params.scriptId, lineNumber: 0, columnNumber: 0}});
      InspectorTest.logMessage(response);
      InspectorTest.completeTest();
    });
    contextGroup.addScript(`
  function test() { continue; }
  //# sourceURL=test.js`);
})();

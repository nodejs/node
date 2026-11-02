// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/572401025.');

contextGroup.addScript(
    `1 + 1;
var call_outer = (function outer() {
  "use strict";
  let f_ref;
  label: {
    f_ref = f;
    break label;
    let x = 1;
    function f() {
      return x;
    }
  }
  return f_ref;
});`,
    0, 0, 'target.js');

contextGroup.addScript(`
for (let i = 0; i < 4; i++) gc();
`);

(async () => {
  let scriptId;
  Protocol.Debugger.onScriptParsed(msg => {
    if (msg.params.url === 'target.js') {
      scriptId = msg.params.scriptId;
    }
  });
  await Protocol.Debugger.enable();
  InspectorTest.logMessage(await Protocol.Debugger.getPossibleBreakpoints({
    start: {scriptId, lineNumber: 0, columnNumber: 0},
    end: {scriptId, lineNumber: 0, columnNumber: 5},
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: 'call_outer()() + 1',
  }));
  InspectorTest.completeTest();
})();

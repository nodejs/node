// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

// d8's `send` dispatches protocol messages synchronously, so a breakpoint
// condition can disable the debugger while it is being evaluated.

let scriptId;
globalThis.receive = function(m) {
  let msg = JSON.parse(m);
  if (msg.method === 'Debugger.scriptParsed' && msg.params.url === 'test.js') {
    scriptId = msg.params.scriptId;
  }
};
send(JSON.stringify({id: 1, method: 'Debugger.enable'}));
eval(
    '//# sourceURL=test.js\n' +
    'function outer() {\n' +
    '  function inner1() { return 1; }\n' +
    '  function inner2() { return 2; }\n' +
    '  return inner1() + inner2();\n' +
    '}');
send(JSON.stringify({
  id: 2,
  method: 'Debugger.setBreakpoint',
  params: {
    location: {scriptId: scriptId, lineNumber: 2, columnNumber: 22},
    condition: 'globalThis.onCond()'
  }
}));

let conditionEvaluated = false;
globalThis.onCond = function() {
  conditionEvaluated = true;
  send(JSON.stringify({id: 3, method: 'Debugger.disable'}));
  return false;
};

assertEquals(3, outer());
assertTrue(conditionEvaluated);

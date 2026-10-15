// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

// d8's `send` dispatches protocol messages synchronously, so a breakpoint
// condition can disable the debugger while it is being evaluated. Set two
// break points at the same location, to check that the second condition is
// not evaluated anymore once the debugger was disabled by the first one.

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
    condition: 'globalThis.cond1()'
  }
}));
send(JSON.stringify({
  id: 3,
  method: 'Debugger.setBreakpointByUrl',
  params: {
    urlRegex: 'test\\.js',
    lineNumber: 2,
    columnNumber: 22,
    condition: 'globalThis.cond2()'
  }
}));

let cond1Evaluated = false;
let cond2Evaluated = false;
globalThis.cond1 = function() {
  cond1Evaluated = true;
  send(JSON.stringify({id: 4, method: 'Debugger.disable'}));
  return false;
};
globalThis.cond2 = function() {
  cond2Evaluated = true;
  return false;
};

assertEquals(3, outer());
assertTrue(cond1Evaluated !== cond2Evaluated);

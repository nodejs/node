// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Wasm breakpoint condition can synchronously disable the debugger while it
// is being evaluated. A second break point (from setBreakpointOnFunctionCall)
// at the same position must not be evaluated anymore afterwards.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/570538338.');

const source = `
let mod_bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x05,                    // section length 5
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $main (result i32)

  0x07,                    // section kind: Export
  0x08,                    // section length 8
  0x01,                    // exports count 1: export # 0
  0x04,                    // field name length:  4
  0x6d, 0x61, 0x69, 0x6e,  // field name: main
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x07,                    // section length 7
  0x01,                    // functions count 1
                           // function #0 $main
  0x05,                    // body size 5
  0x00,                    // 0 entries in locals list
  0x01,                    // nop
  0x41, 0x2a,              // i32.const 42
  0x0b,                    // end
]);

let i1;
let cond1Evaluated = false;
let cond2Evaluated = false;
let mainResult;

globalThis.cond1 = function() {
  cond1Evaluated = true;
  inspector.disconnectSession(${session.id});
  return false;
};
globalThis.cond2 = function() {
  cond2Evaluated = true;
  return false;
};

globalThis.i1 = i1 =
    new WebAssembly.Instance(new WebAssembly.Module(mod_bytes));
`;

(async function test() {
  await Protocol.Debugger.enable();
  contextGroup.addScript(source);
  const [, {params: {scriptId}}] = await Protocol.Debugger.onceScriptParsed(2);

  const {result: {locations}} = await Protocol.Debugger.getPossibleBreakpoints({
    start: {scriptId, lineNumber: 0, columnNumber: 0},
  });
  await Protocol.Debugger.setBreakpoint({
    location: locations[0],
    condition: 'globalThis.cond1()',
  });
  const {result: {result: {objectId}}} = await Protocol.Runtime.evaluate({
    expression: 'globalThis.i1.exports.main',
  });
  await Protocol.Debugger.setBreakpointOnFunctionCall({
    objectId,
    condition: 'globalThis.cond2()',
  });

  contextGroup.addScript('mainResult = i1.exports.main();');
  InspectorTest._sessions.delete(session);

  const session2 = contextGroup.connect();
  const {result: {result}} = await session2.Protocol.Runtime.evaluate({
    expression: '({mainResult, cond1Evaluated, cond2Evaluated})',
    returnByValue: true,
  });
  InspectorTest.logObject(result.value);
  InspectorTest.completeTest();
})();

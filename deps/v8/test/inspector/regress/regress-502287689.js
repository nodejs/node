// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/502287689.');

contextGroup.addScript(`
const bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x06,                    // section length 6
  0x01, 0x60,              // types count 1:  kind: func
  0x01, 0x7f,              // param count 1:  i32
  0x01, 0x7f,              // return count 1:  i32

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $f (param i32) (result i32)

  0x07,                    // section kind: Export
  0x05,                    // section length 5
  0x01,                    // exports count 1: export # 0
  0x01,                    // field name length:  1
  0x66,                    // field name: f
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x12,                    // section length 18
  0x01,                    // functions count 1
                           // function #0 $f
  0x10,                    // body size 16
  0x00,                    // 0 entries in locals list
  0x41, 0x0a,              // i32.const 10
  0x41, 0x02,              // i32.const 2  <-- Y = 34 (relative to module start)
  0x6a,                    // i32.add
  0x41, 0x03,              // i32.const 3  <-- X = 37
  0x6a,                    // i32.add
  0x20, 0x00,              // local.get $var0
  0x6d,                    // i32.div_s
  0x20, 0x00,              // local.get $var0
  0x6d,                    // i32.div_s
  0x0b,                    // end
]);
let instance;
`);

let wasmScriptId = null;
let lastBreakpointId = null;

Protocol.Debugger.onPaused(async () => {
  InspectorTest.log('Paused!');

  // 5) Remove BP at Y (offset 34)
  // This will call UpdateReturnAddresses with a new code that (buggy)
  // might not have a source position for Y.
  if (lastBreakpointId) {
    InspectorTest.log('Removing breakpoint at Y: ' + lastBreakpointId);
    await Protocol.Debugger.removeBreakpoint({breakpointId: lastBreakpointId});
  }

  // 6) Set BP at X (offset 37)
  // This would be the next step in the two-isolate PoC, but we might have
  // already crashed.
  InspectorTest.log('Setting breakpoint at X...');
  await Protocol.Debugger.setBreakpoint({
    location: {scriptId: wasmScriptId, lineNumber: 0, columnNumber: 37},
  });

  // 8) Resume
  InspectorTest.log('Resuming...');
  await Protocol.Debugger.resume();
});

(async () => {
  // Enable debugger
  await Protocol.Debugger.enable();

  const scriptParsedPromise = Protocol.Debugger.onceScriptParsed(2);
  await Protocol.Runtime.evaluate({
    expression: `
      const module = new WebAssembly.Module(bytes);
      instance = new WebAssembly.Instance(module);
    `,
  });
  const [, {params: {scriptId}}] = await scriptParsedPromise;
  wasmScriptId = scriptId;

  InspectorTest.log('Setting breakpoint at Y (offset 34)...');
  const bpResult = await Protocol.Debugger.setBreakpoint({
    location: {scriptId: wasmScriptId, lineNumber: 0, columnNumber: 34},
  });
  lastBreakpointId = bpResult.result.breakpointId;

  InspectorTest.log('Calling f(7)...');
  await Protocol.Runtime.evaluate({
    expression: 'instance.exports.f(7)',
  });
  InspectorTest.log('Finished.');

  InspectorTest.completeTest();
})();

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-code-coverage

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/521715545.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
const builder = new WasmModuleBuilder();
const func = builder.addFunction('main', kSig_v_v)
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 1,
      kExprI32Add,
      kExprDrop
    ])
    .exportFunc();
const buffer = builder.toBuffer();

// Offset into the code section for the 'i32.add' instruction.
// The header is 8 bytes, sections are typically 1 byte ID + length.
// WasmModuleBuilder offsets are relative to the start of the buffer.
const offset = func.body_offset + 2;
`);

(async () => {
  await Protocol.Debugger.enable();

  const scriptParsedPromise = Protocol.Debugger.onceScriptParsed(2);
  await Protocol.Runtime.evaluate({
    expression: `
      const module = new WebAssembly.Module(buffer);
      const instance = new WebAssembly.Instance(module);
    `,
  });
  const [, {params: {scriptId}}] = await scriptParsedPromise;

  const {result: {result: {value: offset}}} = await Protocol.Runtime.evaluate({
    expression: 'offset',
    returnByValue: true,
  });

  InspectorTest.logMessage(await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId,
      lineNumber: 0,
      columnNumber: offset,
    },
  }));

  // If we reached here without crashing, the fix works.
  InspectorTest.completeTest();
})();

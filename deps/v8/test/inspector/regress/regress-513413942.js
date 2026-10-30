// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-wasm-tier-up
// Flags: --no-wasm-trap-handler --stress-wasm-memory-moving

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/513413942.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
const builder = new WasmModuleBuilder();
builder.addMemory(1, 1000);
builder.exportMemoryAs('memory', 0);

const body = [
  ...wasmI32Const(0),
  kExprI32LoadMem, 0, 0,
  kExprDrop,
  kExprNop,
  ...wasmI32Const(0),
  ...wasmI32Const(123),
  kExprI32StoreMem, 0, 0,
];

const target = builder.addFunction('target', kSig_v_v)
  .addBody(body)
  .exportFunc();

const module_bytes = builder.toBuffer();
const nop_offset = target.body_offset + 6;
`);

Protocol.Debugger.onPaused(() => Protocol.Debugger.resume());

(async () => {
  await Protocol.Debugger.enable();

  const scriptParsedPromise = Protocol.Debugger.onceScriptParsed(2);
  await Protocol.Runtime.evaluate({
    expression: `
      const module = new WebAssembly.Module(module_bytes);
      const instance = new WebAssembly.Instance(module);
      globalThis.instance = instance;
      globalThis.grown = false;
    `,
  });
  const [, {params: {scriptId: wasmScriptId}}] = await scriptParsedPromise;

  const {result: {result: {value: nop_offset}}} =
      await Protocol.Runtime.evaluate({
        expression: 'nop_offset',
        returnByValue: true,
      });

  const condition = `(function() {
  if (!globalThis.grown) {
    globalThis.instance.exports.memory.grow(1);
    globalThis.grown = true;
    return true;
  }
  return false;
})()`;

  InspectorTest.logMessage(await Protocol.Debugger.setBreakpoint({
    location: {scriptId: wasmScriptId, lineNumber: 0, columnNumber: nop_offset},
    condition: condition,
  }));

  await Protocol.Runtime.evaluate({
    expression: 'instance.exports.target()',
  });

  InspectorTest.completeTest();
})();

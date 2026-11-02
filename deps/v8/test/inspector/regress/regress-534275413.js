// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-trap-handler --stress-wasm-memory-moving
// Liftoff does not emit loop stack checks (relies on dynamic tiering instead),
// hence this test would fail under Liftoff.
// Flags: --no-liftoff

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/534275413.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
function setup() {
  console.log('setup');
  inspector.runNestedMessageLoop();
}

const builder = new WasmModuleBuilder();
builder.addMemory(1, 10);  // Memory 0
builder.addMemory(1, 10);  // Memory 1
builder.addMemory(1, 10);  // Memory 2
builder.exportMemoryAs('mem0', 0);
builder.exportMemoryAs('mem1', 1);
builder.exportMemoryAs('mem2', 2);

const $setup = builder.addImport('env', 'setup', kSig_v_v);

builder.addFunction('main', kSig_i_v)
  .addLocals(kWasmI32, 1) // Local 0 as loop counter
  .addBody([
    kExprI32Const, 5,
    kExprLocalSet, 0,
    kExprCallFunction, $setup,
    // Load from Memory 2 outside the loop to cache the base pointer.
    kExprI32Const, 0,
    kExprI32LoadMem, 0x40, 2, 0,
    kExprDrop,
    kExprBlock, kWasmI32,
      kExprLoop, kWasmVoid,
        // Decrement loop counter
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kExprI32Sub,
        kExprLocalTee, 0,
        // Loop stack check happens here. It will process the pending interrupt
        // from setup() on the 1st iteration, triggering Debugger.paused.
        // Load from Memory 2 inside the loop.
        kExprI32Const, 0,
        kExprI32LoadMem, 0x40, 2, 0,
        // Exit loop if counter is 0.
        kExprLocalGet, 0,
        kExprI32Eqz,
        kExprBrIf, 1,
        kExprDrop,
        kExprBr, 0,
      kExprEnd,
      kExprUnreachable,
    kExprEnd
  ]).exportFunc();

const instance = builder.instantiate({ env: {setup} });

// Write 42 to Memory 2 before growth.
new DataView(instance.exports.mem2.buffer).setInt32(0, 42, true);
`);

Protocol.Debugger.onPaused(async () => {
  // Growing memory 2 should fail because it cannot grow in place (because of
  // --stress-wasm-memory-moving).
  let {result: {result: res2}} = await Protocol.Runtime.evaluate({
    expression: 'instance.exports.mem2.grow(1)',
  });
  InspectorTest.log(res2.description.split('\n')[0]);
  // Growing memory 1 should also fail even when it must grow in place (via
  // toResizableBuffer()).
  let {result: {result: res1}} = await Protocol.Runtime.evaluate({
    expression: `
      instance.exports.mem1.toResizableBuffer();
      instance.exports.mem1.grow(1);
    `,
  });
  InspectorTest.log(res1.description.split('\n')[0]);
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Runtime.enable();

  const consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  const evalPromise = Protocol.Runtime.evaluate({
    expression: `
      const result = instance.exports.main();
      [result, new DataView(instance.exports.mem2.buffer).getInt32(0, true)];
    `,
    returnByValue: true,
  });

  await consolePromise;
  await Protocol.Debugger.enable();
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  const {result: {result}} = await evalPromise;
  InspectorTest.logObject(result.value);
  InspectorTest.completeTest();
})();

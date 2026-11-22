// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-trap-handler --stress-wasm-memory-moving

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/519715438.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
const builder = new WasmModuleBuilder();
builder.addMemory(1, 100);
builder.exportMemoryAs('mem');
const setupIdx = builder.addImport('env', 'setup', kSig_v_v);

builder.addFunction('vuln', kSig_v_i)
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprCallFunction, setupIdx,
      kExprLoop, kWasmVoid,
        kExprI32Const, 4,
        ...wasmI32Const(0x41414141),
        kExprI32StoreMem, 2, 0,
        kExprLocalGet, 1,
        kExprI32Const, 1,
        kExprI32Add,
        kExprLocalTee, 1,
        kExprLocalGet, 0,
        kExprI32LtS,
        kExprBrIf, 0,
      kExprEnd,
    ])
    .exportFunc();

function setup() {
  console.log('setup');
  inspector.runNestedMessageLoop();
}

const instance = builder.instantiate({env: {setup}});
const mem = instance.exports.mem;
`);

let pauseHandled = false;
Protocol.Debugger.onPaused(async () => {
  // Called while the optimized Wasm frame is on the stack.
  if (!pauseHandled) {
    pauseHandled = true;
    await Protocol.Runtime.evaluate({expression: 'let r = mem.grow(10);'});
  }
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Runtime.enable();

  const consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  // Iteration count must be large enough that Turboshaft's loop unrolling
  // still leaves a stack-check that fires before all stores complete.
  const evalPromise =
      Protocol.Runtime.evaluate({expression: 'instance.exports.vuln(100)'});

  await consolePromise;
  // Without Wasm lazy-deopt, this does not replace the optimized frame on
  // the stack.
  await Protocol.Debugger.enable();
  // Request an API_INTERRUPT.
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  await evalPromise;
  InspectorTest.completeTest();
})();

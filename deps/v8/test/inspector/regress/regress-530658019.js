// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-wasm-trap-handler
// Flags: --stress-wasm-memory-moving --no-wasm-inlining

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/530658019.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
const builder = new WasmModuleBuilder();
builder.addMemory(1, 100);
builder.exportMemoryAs('mem');
const $setup = builder.addImport('env', 'setup', kSig_v_v);
const $nop   = builder.addImport('env', 'nop',   kSig_v_v);

// The call to {nop} makes {inner} a non-leaf, so it keeps its stack check.
const inner = builder.addFunction('inner', kSig_v_v).exportFunc()
    .addBody([
      kExprI32Const, 4,
      ...wasmI32Const(0x41414141),
      kExprI32StoreMem, 2, 0,
      kExprCallFunction, $nop,
    ]);

const outer = builder.addFunction('outer', kSig_v_v).exportFunc()
    .addBody([
      kExprCallFunction, $setup,
      kExprCallFunction, inner.index,
    ]);

let instance;
function setup() {
  console.log('enable');
  inspector.runNestedMessageLoop();
  // In {inspector-test}, inspector protocol messages are processed
  // synchronously. That's different in Chrome, and the following two
  // natives calls fake the effect that async processing would have, which is
  // that {inner} can still be tiered up when it is called.
  %WasmLeaveDebugging();
  %WasmTierUpFunction(instance.exports.inner);
  console.log('pause');
  inspector.runNestedMessageLoop();
}

instance = builder.instantiate({env: {setup, nop: () => {}}});
const mem = instance.exports.mem;

%WasmTierUpFunction(instance.exports.inner);
%WasmTierUpFunction(instance.exports.outer);
`);

let pauseHandled = false;
Protocol.Debugger.onPaused(async () => {
  // Called from runMessageLoopOnPause while `inner`'s Turboshaft frame
  // is on the stack, paused inside Runtime_WasmStackGuard at the
  // kFunctionEntry stack check.
  if (!pauseHandled) {
    pauseHandled = true;
    await Protocol.Runtime.evaluate({expression: 'mem.grow(10);'});
  }
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Runtime.enable();

  let consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  const evalPromise = Protocol.Runtime.evaluate({
    expression: `
      instance.exports.outer();
      const view = new Int32Array(mem.buffer);
      view[1];
    `,
    returnByValue: true,
  });

  await consolePromise;
  consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.runIfWaitingForDebugger();

  await consolePromise;
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  const {result: {result}} = await evalPromise;
  InspectorTest.log(`view[1]: 0x${result.value.toString(16)}`);
  InspectorTest.completeTest();
})();

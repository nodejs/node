// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-trap-handler --stress-wasm-memory-moving --no-liftoff

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/529880019.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
function setup() {
  console.log('setup');
  inspector.runNestedMessageLoop();
}

const builder = new WasmModuleBuilder();
builder.addMemory(1, 10, true, true);
builder.exportMemoryAs('mem');
const $setup = builder.addImport('env', 'setup', kSig_v_v);
builder.addFunction('main', kSig_v_v)
  .addBody([
    kExprCallFunction, $setup,
    kExprLoop, kWasmVoid,
      // Without the fix, the base pointer for this store is cached before
      // the stack check. If growth happens during the check, it uses a stale
      // pointer, causing a crash.
      kExprI32Const, 0,
      kExprI32Const, 42,
      kExprI32StoreMem, 0, 0,
      // Exit loop if memory size is updated.
      kExprMemorySize, 0,
      kExprI32Const, 2,
      kExprI32Eq,
      kExprBrIf, 1,
    kExprEnd
  ]).exportFunc();

const instance = builder.instantiate({ env: {setup} });
`);

Protocol.Debugger.onPaused(async () => {
  // Grow memory and relocate it (via --stress-wasm-memory-moving).
  await Protocol.Runtime.evaluate({expression: 'instance.exports.mem.grow(1);'});
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Runtime.enable();

  const consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  const evalPromise =
      Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});

  await consolePromise;
  await Protocol.Debugger.enable();
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  await evalPromise;
  InspectorTest.completeTest();
})();

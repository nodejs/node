// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-trap-handler --stress-wasm-memory-moving

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/527985808.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
function setup() {
  console.log('setup');
  inspector.runNestedMessageLoop();
}

const builder = new WasmModuleBuilder();
builder.addMemory(1);
builder.exportMemoryAs('mem');
const $setup = builder.addImport('env', 'setup', kSig_v_v);
builder.addFunction('vuln', kSig_v_i).addLocals(kWasmI32, 1).addBody([
  kExprCallFunction, $setup,
  kExprLoop, kWasmVoid,
    kExprI32Const, 4,
    kExprI32Const, 0,
    kExprI32StoreMem, 0, 0,
  kExprEnd]).exportFunc();

const instance = builder.instantiate({ env: {setup} });
const memory = instance.exports.mem;
`);

Protocol.Debugger.onPaused(async () => {
  await Protocol.Runtime.evaluate({expression: 'memory.grow(10);'});
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Runtime.enable();

  const consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  const evalPromise =
      Protocol.Runtime.evaluate({expression: 'instance.exports.vuln()'});

  await consolePromise;
  await Protocol.Debugger.enable();
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  await evalPromise;
  InspectorTest.completeTest();
})();

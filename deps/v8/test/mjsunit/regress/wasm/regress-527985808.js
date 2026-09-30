// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --no-wasm-trap-handler --stress-wasm-memory-moving

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let debugCommandId = 0;
function sendDebugCommand(cmd) {
  send(JSON.stringify({
    id: ++debugCommandId,
    method: cmd,
  }));
}

globalThis.handleInspectorMessage = function () {
  memory.grow(10);
  sendDebugCommand('Debugger.resume');
};

function setup() {
  sendDebugCommand('Debugger.enable');
  sendDebugCommand('Debugger.pause');
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
instance.exports.vuln();

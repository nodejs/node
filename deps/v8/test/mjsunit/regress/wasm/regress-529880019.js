// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --no-wasm-trap-handler --stress-wasm-memory-moving --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let debugCommandId = 0;
function sendDebugCommand(cmd) {
  send(JSON.stringify({
    id: ++debugCommandId,
    method: cmd,
  }));
}

globalThis.handleInspectorMessage = function() {
  // Grow memory and relocate it (via --stress-wasm-memory-moving).
  instance.exports.mem.grow(1);
  sendDebugCommand('Debugger.resume');
};

function setup() {
  sendDebugCommand('Debugger.enable');
  sendDebugCommand('Debugger.pause');
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
instance.exports.main();

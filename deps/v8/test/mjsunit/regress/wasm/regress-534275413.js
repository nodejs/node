// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --no-wasm-trap-handler --stress-wasm-memory-moving
// Liftoff does not emit loop stack checks (relies on dynamic tiering instead),
// hence this test would fail under Liftoff.
// Flags: --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let debugCommandId = 0;
function sendDebugCommand(cmd) {
  send(JSON.stringify({
    id: ++debugCommandId,
    method: cmd,
  }));
}

globalThis.handleInspectorMessage = function() {
  // Growing memory 2 should fail because it cannot grow in place (because of
  // --stress-wasm-memory-moving).
  assertThrows(() => instance.exports.mem2.grow(1), Error, /Unable to grow/);
  sendDebugCommand('Debugger.resume');
};

function setup() {
  sendDebugCommand('Debugger.enable');
  sendDebugCommand('Debugger.pause');
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
        // from setup() on the 1st iteration, triggering handleInspectorMessage.
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

let result = instance.exports.main();
assertEquals(42, result);
assertEquals(42, new DataView(instance.exports.mem2.buffer).getInt32(0, true));

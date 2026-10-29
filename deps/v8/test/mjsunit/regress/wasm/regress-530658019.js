// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-inspector --no-wasm-trap-handler
// Flags: --stress-wasm-memory-moving --no-wasm-inlining

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let cdpId = 0;
function cdp(method, params) {
  send(JSON.stringify({id: ++cdpId, method, params: params || {}}));
}

let pauseHandled = false;
globalThis.handleInspectorMessage = function() {
  // d8 calls this from runMessageLoopOnPause while `inner`'s Turboshaft frame
  // is on the stack, paused inside Runtime_WasmStackGuard at the
  // kFunctionEntry stack check.
  if (!pauseHandled) {
    pauseHandled = true;
    mem.grow(10);
  }
  cdp('Debugger.resume');
};

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
  cdp('Debugger.enable');
  // In {d8 --enable-inspector}, inspector protocol messages are processed
  // synchronously. That's different in Chrome, and the following two
  // natives calls fake the effect that async processing would have, which is
  // that {inner} can still be tiered up when it is called.
  %WasmLeaveDebugging();
  %WasmTierUpFunction(instance.exports.inner);
  cdp('Debugger.pause');
}

instance = builder.instantiate({env: {setup, nop: () => {}}});
const mem = instance.exports.mem;

%WasmTierUpFunction(instance.exports.inner);
%WasmTierUpFunction(instance.exports.outer);

instance.exports.outer();
const view = new Int32Array(mem.buffer);
assertEquals(0x41414141, view[1]);

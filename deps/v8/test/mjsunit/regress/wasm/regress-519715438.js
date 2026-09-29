// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --no-wasm-trap-handler --stress-wasm-memory-moving

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let cdpId = 0;
function cdp(method, params) {
  send(JSON.stringify({id: ++cdpId, method, params: params || {}}));
}

let pauseHandled = false;
globalThis.handleInspectorMessage = function() {
  // Called in a loop by d8's runMessageLoopOnPause while the optimized Wasm
  // frame is on the stack.
  if (!pauseHandled) {
    pauseHandled = true;
    let r = mem.grow(10);
  }
  cdp('Debugger.resume');
};

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
  // Without Wasm lazy-deopt, this does not replace the optimized frame on
  // the stack.
  cdp('Debugger.enable');
  // Request an API_INTERRUPT.
  cdp('Debugger.pause');
}

const instance = builder.instantiate({env: {setup}});
const mem = instance.exports.mem;

// Iteration count must be large enough that Turboshaft's loop unrolling
// still leaves a stack-check that fires before all stores complete.
instance.exports.vuln(100);

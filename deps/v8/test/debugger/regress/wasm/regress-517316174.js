// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const num_params = 100;
// Use a large signature to maximize the call instruction size.
const sig_idx = builder.addType(
    makeSig(new Array(num_params).fill(kWasmI64), []));
const imp_idx = builder.addImport('m', 'js', sig_idx);
builder.addTable(kWasmFuncRef, 1);
builder.addActiveElementSegment(0, [kExprI32Const, 0], [imp_idx]);

const func_main = builder.addFunction('main', kSig_v_v)
    .addBody([
      kExprNop, // Offset 1
      ...new Array(num_params).fill([kExprI64Const, 0]).flat(),
      kExprI32Const, 0, // table index
      kExprCallIndirect, sig_idx, 0,
      kExprI64Const, 0,
      kExprDrop,
      kExprI64Const, 0,
      kExprDrop,
    ])
    .exportFunc();

const instance = builder.instantiate({
  m: {
    js: (...args) => {
      if (globalThis.recompiled) return;
      globalThis.recompiled = true;

      // Trigger UpdateReturnAddresses(isolate, ..., kAfterBreakpoint)
      // for the topmost Wasm frame ('main') which is currently at the call
      // site.
      debug.Debug.clearBreakPoint(bp_call);

      // Set a new breakpoint to force recompilation.
      debug.Debug.setBreakPoint(instance.exports.main, 0, 0);
    }
  }
});

// Calculate the offset of the call_indirect instruction.
// 1 (nop) + 100 * 2 (i64.const 0) + 2 (i32.const 0) = 203.
const call_offset = 1 + num_params * 2 + 2;

// Breakpoint 1: At nop (offset 1). This ensures 'UpdateReturnAddresses'
// has some existing state to work with and triggers the buggy path.
debug.Debug.setBreakPoint(instance.exports.main, 0, 1);

// Breakpoint 2: Specifically at the call site.
const bp_call =
    debug.Debug.setBreakPoint(instance.exports.main, 0, call_offset);

instance.exports.main();

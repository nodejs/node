// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-debug-mask-for-testing=1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
builder.addMemory(16, 32);
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: i_iii
// body:
kExprI32Const, 0x65,  // i32.const
kExprI32Const, 0x61,  // i32.const
kExprI64Const, 0x42,  // i64.const
kAtomicPrefix, kExprI32AtomicWait, 0x00, 0x0b,  // memory.atomic.wait32
]);
builder.addExport('main', 0);
assertThrows(function() { builder.instantiate(); }, WebAssembly.CompileError);

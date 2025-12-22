// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-drumbrake-super-instructions

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.addMemory(1, 1)
builder.addActiveDataSegment(0, [kExprI32Const, 0], [0x10, 0x32, 0x80, 0x7f]);

let copySig = builder.addType(kSig_v_v);
builder.addFunction("copy", copySig).addBody([
  ...wasmI32Const(0x4),
  ...wasmI32Const(0x0),
  kExprF32LoadMem, 0x00, 0x00,
  kExprF32StoreMem, 0x00, 0x00,
]);

let mainSig = builder.addType(kSig_i_v);
builder.addFunction("main", mainSig).addBody([
  kExprCallFunction, 0x00,
  kExprI32Const, 0x04,
  kExprI32LoadMem, 0x00, 0x00,
]).exportFunc();

let wasmBytes = builder.toBuffer();
let wasmModule = new WebAssembly.Module(wasmBytes);
let wasmExports = new WebAssembly.Instance(wasmModule).exports;

assertEquals(wasmExports.main(), 0x7f803210)

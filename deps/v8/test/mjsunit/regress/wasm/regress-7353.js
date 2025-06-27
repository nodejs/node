// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);
builder.addFunction('grow', kSig_i_i).addBody([
  kExprLocalGet, 0,
  kExprMemoryGrow, 0,
]).exportFunc();
builder.addFunction('main', kSig_i_i).addBody([
  ...wasmI32Const(0x41),
  kExprLocalSet, 0,
  // Enter loop, such that values are spilled to the stack.
  kExprLoop, kWasmVoid,
  kExprEnd,
  // Reload value. This must be loaded as 32 bit value.
  kExprLocalGet, 0,
  kExprI32LoadMem, 0, 0,
]).exportFunc();
const instance = builder.instantiate();
// Execute grow, such that the stack contains garbage data afterwards.
instance.exports.grow(1);
instance.exports.main();

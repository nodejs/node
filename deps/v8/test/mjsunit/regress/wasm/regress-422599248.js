// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-wasm-memory-moving

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.addFunction('main', kSig_v_v).addBody([
  kExprI32Const, 0,
  kExprMemoryGrow, kMemoryZero,
  kExprDrop,
  kExprI32Const, 0,
  kExprI32Const, 42,
  kExprI32StoreMem, 0, 0,
]).exportFunc();
var instance = builder.instantiate();
instance.exports.main();

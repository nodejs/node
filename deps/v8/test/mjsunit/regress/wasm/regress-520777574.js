// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-growable-stacks --wasm-tiering-budget=1

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const sig_index = builder.addType(makeSig([kWasmI32], [kWasmI32]));

// Target A
const target_A = builder.addFunction("target_A", sig_index)
  .addBody([
    kExprLocalGet, 0,
  ]);

// Target B
const target_B = builder.addFunction("target_B", sig_index)
  .addBody([
    kExprLocalGet, 0,
    kExprI32Const, 2,
    kExprI32Add
  ]);

builder.appendToTable([target_A.index, target_B.index]);

const test = builder.addFunction("test", makeSig([kWasmI32, kWasmI32], [kWasmI32]));
test.addBody([
  // if depth > 0, do recursive call
  kExprLocalGet, 0,
  kExprIf, 0x40,
    kExprLocalGet, 0,
    kExprI32Const, 1,
    kExprI32Sub,
    kExprLocalGet, 1, // x
    kExprCallFunction, test.index,
    kExprDrop, // drop the recursive result
  kExprEnd,
  // Always execute call_indirect
  kExprLocalGet, 1, // x
  kExprLocalGet, 0,
  kExprIf, kWasmI32,
    kExprI32Const, 1, // target_B
  kExprElse,
    kExprI32Const, 0, // target_A
  kExprEnd,
  kExprCallIndirect, sig_index, 0,
]).exportFunc();

const instance = builder.instantiate();
const wrapper = WebAssembly.promising(instance.exports.test);

for (let i = 0; i < 50; i++) {
  wrapper(0, 42);
}

try {
  wrapper(1000, 42);
} catch (e) {
}

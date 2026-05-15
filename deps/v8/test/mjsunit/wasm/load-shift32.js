// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/mjsunit.js');
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test loads with an index using a 32 bit shift.

const builder = new WasmModuleBuilder();
let $mem0 = builder.addMemory(1, undefined);
builder.exportMemoryAs('memory', $mem0);

// Use a 32-bit index that is shifted and then used as a memory offset.
// This triggers the UXTW LSL optimization on ARM64.
builder.addFunction("load", makeSig([kWasmI64], [kWasmI32])).exportFunc()
.addBody([
  kExprLocalGet, 0,
  kExprI32ConvertI64,
  kExprI32Const, 2,
  kExprI32Shl,
  kExprI32LoadMem, 2, 0,
]);

// This should NOT trigger the UXTW LSL optimization because the shift amount (3)
// does not match the access size log2 (2 for i32).
builder.addFunction("load_shift3", makeSig([kWasmI64], [kWasmI32])).exportFunc()
.addBody([
  kExprLocalGet, 0,
  kExprI32ConvertI64,
  kExprI32Const, 3,
  kExprI32Shl,
  kExprI32LoadMem, 2, 0,
]);

const instance = builder.instantiate({});
const {memory, load, load_shift3} = instance.exports;
const view = new Int32Array(memory.buffer);

// Initialize memory with ascending values.
for (let i = 0; i < view.length; i++) {
  view[i] = i;
}

function test(fct, index, shift) {
  // Truncate index to 32 bits to match Wasm behavior.
  const truncatedIndex = BigInt.asUintN(32, index);
  // In Wasm, the offset is truncatedIndex << shift (32-bit shift).
  const byteOffset = Number(BigInt.asUintN(32, truncatedIndex << BigInt(shift)));
  if (byteOffset + 4 > memory.buffer.byteLength) {
    // Expect out of bounds.
    assertThrows(() => fct(index), WebAssembly.RuntimeError);
    return;
  }

  const viewIndex = byteOffset >> 2;
  const expected = view[viewIndex];
  assertEquals(expected, fct(index), `At index ${index} with shift ${shift}`);
}

test(load, 0n, 2);
test(load, 1n, 2);
test(load, 102n, 2);
test(load, 0x1234n, 2);
test(load, 0x80000000n, 2);
test(load, 0x100000000n, 2);
test(load, 0x200000001n, 2);
test(load, 0x7fffffffn, 2);
test(load, 0x1234567800000005n, 2);

test(load_shift3, 0n, 3);
test(load_shift3, 1n, 3);
test(load_shift3, 102n, 3);
test(load_shift3, 0x1234n, 3);
test(load_shift3, 0x80000000n, 3);
test(load_shift3, 0x100000000n, 3);
test(load_shift3, 0x200000001n, 3);
test(load_shift3, 0x7fffffffn, 3);
test(load_shift3, 0x1234567800000005n, 3);

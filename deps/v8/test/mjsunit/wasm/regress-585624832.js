// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const array = builder.addArray(kWasmI64);
const arrayRef = wasmRefNullType(array);
builder.addMemory(1, 1, false);
const get = i => [kExprLocalGet, i];
const set = i => [kExprLocalSet, i];
const numbers = [0, 1, 2, 3, 4];
const old = 8, shifted = 9, firstNumber = 10, remainder = 16, len = 17;

// The cmpxchgs and division constrain rax and rdx. The i64 source of the
// shift must keep its upper 32 bits through the move cycle into its
// same-as-input Word32 result.
const make = builder.addFunction('make',
    makeSig([kWasmI64, kWasmI64, kWasmI64, ...numbers.map(() => kWasmF64)],
            [arrayRef])).addLocals(kWasmI64, 9).addLocals(kWasmI32, 1);
const cmpxchg = [
  kExprI32Const, 0, ...get(firstNumber + 2), ...get(firstNumber),
  kAtomicPrefix, kExprI64AtomicCompareExchange, 3, 0
];
make.addBody([
  ...get(2), ...get(0), kExprI64RemU, ...set(remainder),
  ...get(remainder), kExprI64Const, 32, kExprI64ShrU, ...set(shifted),
  ...numbers.flatMap(
      i => [...get(3 + i), kExprI64ReinterpretF64, ...set(firstNumber + i)]),
  ...cmpxchg, ...set(old),
  ...get(shifted), kExprI32ConvertI64, ...set(len),
  ...cmpxchg, kExprDrop,
  ...get(old), ...get(firstNumber), kExprI64DivU,
  ...numbers.slice(1).flatMap(i => [...get(firstNumber + i), kExprI64Add]),
  ...get(remainder), kExprI64Xor, ...get(len), kGCPrefix, kExprArrayNew, array
]).exportFunc();

builder.addFunction('array_len', makeSig([arrayRef], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayLen])
    .exportFunc();

builder.addFunction('array_get', makeSig([arrayRef, kWasmI32], [kWasmI64]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kGCPrefix, kExprArrayGet, array])
    .exportFunc();

const bits = new BigInt64Array(1);
const number = new Float64Array(bits.buffer);
const inputs = numbers.map(i => {
  bits[0] = BigInt(i + 1);
  return number[0];
});
const wasm = builder.instantiate().exports;

const x = (128n << 32n) | 0x12345678n;
const arr = wasm.make(-1n, 0n, x, ...inputs);
assertEquals(128, wasm.array_len(arr));
assertEquals(x ^ 14n, wasm.array_get(arr, 0));
assertEquals(x ^ 14n, wasm.array_get(arr, 127));

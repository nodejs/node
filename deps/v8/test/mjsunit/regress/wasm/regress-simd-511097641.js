// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-lazy-compilation --future-wasm-simd-opt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $mem0 = builder.addMemory(1, 195);
builder.exportMemoryAs('memory', $mem0);

// func $func1: [] -> []
builder.addFunction('func1', kSig_v_v).addLocals(kWasmI32, 1)  // $var0
  .addBody([
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kSimdPrefix, kExprV128AnyTrue,
    kExprI32Eqz,
    kExprLocalTee, 0,  // $var0
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    ...wasmI64Const(-98n),
    kSimdPrefix, kExprI64x2ReplaceLane, 0,
    kSimdPrefix, kExprI8x16Shuffle, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 31, 1, 6, 18, 1, 3,
    kSimdPrefix, kExprI8x16Shuffle, 0, 0, 0, 0, 0, 0, 25, 31, 0, 0, 0, 0, 0, 0, 0, 0,
    kSimdPrefix, kExprS128StoreMem, 2, ...wasmUnsignedLeb(22784),
    kExprLocalGet, 0,  // $var0
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    ...wasmI64Const(76n),
    kSimdPrefix, kExprI64x2ReplaceLane, 0,
    kSimdPrefix, kExprI8x16Shuffle, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 31, 1, 6, 18, 1, 3,
    kSimdPrefix, kExprI8x16Shuffle, 0, 0, 0, 0, 0, 0, 25, 31, 0, 0, 0, 0, 0, 0, 0, 0,
    kSimdPrefix, kExprS128StoreMem, 2, ...wasmUnsignedLeb(22800),
  ])
  .exportFunc();

const instance = builder.instantiate();
const store_address = 1;
const store_offset = 22784;
const store_size = 32;
const memory = new Uint8Array(instance.exports.memory.buffer);
memory.fill(0xff, store_address + store_offset,
            store_address + store_offset + store_size);

instance.exports.func1();

assertEquals(new Uint8Array(store_size),
             memory.slice(store_address + store_offset,
                          store_address + store_offset + store_size));

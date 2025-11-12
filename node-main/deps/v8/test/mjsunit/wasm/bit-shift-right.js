// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let fcts = [
  builder.addFunction('i64_shr_u_dynamic',
                      makeSig([kWasmI64, kWasmI64], [kWasmI64]))
  .exportFunc().addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprI64ShrU,
  ]),
  builder.addFunction('i64_shr_u_by_negative_48',
                      makeSig([kWasmI64], [kWasmI64]))
  .exportFunc().addBody([
    kExprLocalGet, 0,
    ...wasmI64Const(-48),
    kExprI64ShrU,
  ]),
  builder.addFunction('i64_shr_s_dynamic',
                      makeSig([kWasmI64, kWasmI64], [kWasmI64]))
  .exportFunc().addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprI64ShrS,
  ]),
  builder.addFunction('i64_shr_s_by_negative_48',
                      makeSig([kWasmI64], [kWasmI64]))
  .exportFunc().addBody([
    kExprLocalGet, 0,
    ...wasmI64Const(-48),
    kExprI64ShrS,
  ]),
  builder.addFunction('i32_shr_u_dynamic',
                      makeSig([kWasmI32, kWasmI32], [kWasmI32]))
  .exportFunc().addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprI32ShrU,
  ]),
  builder.addFunction('i32_shr_u_by_negative_22',
                      makeSig([kWasmI32], [kWasmI32]))
  .exportFunc().addBody([
    kExprLocalGet, 0,
    ...wasmI32Const(-22),
    kExprI32ShrU,
  ]),
  builder.addFunction('i32_shr_s_dynamic',
                      makeSig([kWasmI32, kWasmI32], [kWasmI32]))
  .exportFunc().addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprI32ShrS,
  ]),
  builder.addFunction('i32_shr_s_by_negative_22',
                      makeSig([kWasmI32], [kWasmI32]))
  .exportFunc().addBody([
    kExprLocalGet, 0,
    ...wasmI32Const(-22),
    kExprI32ShrS,
  ]),
];

let instance = builder.instantiate();
let wasm = instance.exports;

let testFct = () => {
  // i64.shr_u
  assertEquals(0n, wasm.i64_shr_u_dynamic(0n, 0n));
  assertEquals(1n, wasm.i64_shr_u_dynamic(3n, 1n));
  assertEquals(-1n, wasm.i64_shr_u_dynamic(-1n, 64n));
  assertEquals(-1n, wasm.i64_shr_u_dynamic(-1n, -64n));
  assertEquals(1n, wasm.i64_shr_u_dynamic(-1n, 63n));
  assertEquals(1n, wasm.i64_shr_u_dynamic(-1n, -1n));
  assertEquals(BigInt(2 ** 32 - 1), wasm.i64_shr_u_dynamic(-1n, -32n));
  assertEquals(8n, wasm.i64_shr_u_dynamic(16n, 65n));
  assertEquals(8n, wasm.i64_shr_u_dynamic(16n, -127n));

  assertEquals(0n, wasm.i64_shr_u_by_negative_48(123n));
  assertEquals(BigInt(2 ** 48 - 1), wasm.i64_shr_u_by_negative_48(-1n));
  assertEquals(4n, wasm.i64_shr_u_by_negative_48(BigInt(2 ** 18)))

  // i64.shr_s
  assertEquals(0n, wasm.i64_shr_s_dynamic(0n, 0n));
  assertEquals(1n, wasm.i64_shr_s_dynamic(3n, 1n));
  assertEquals(-1n, wasm.i64_shr_s_dynamic(-1n, 64n));
  assertEquals(-1n, wasm.i64_shr_s_dynamic(-1n, -64n));
  assertEquals(-1n, wasm.i64_shr_s_dynamic(-8n, 63n));
  assertEquals(-4n, wasm.i64_shr_s_dynamic(-8n, -63n));
  assertEquals(-1n, wasm.i64_shr_s_dynamic(-16n, -2n));

  assertEquals(0n, wasm.i64_shr_s_by_negative_48(123n));
  assertEquals(-BigInt(2 ** 16),
      wasm.i64_shr_s_by_negative_48(-BigInt(2 ** 32)));

  // i32.shr_u
  assertEquals(0, wasm.i32_shr_u_dynamic(0, 0));
  assertEquals(1, wasm.i32_shr_u_dynamic(3, 1));
  assertEquals(-1, wasm.i32_shr_u_dynamic(-1, 32));
  assertEquals(-1, wasm.i32_shr_u_dynamic(-1, -32));
  assertEquals((1 << 17) - 1, wasm.i32_shr_u_dynamic(-1, 15));

  assertEquals(123, wasm.i32_shr_u_by_negative_22((123 << 10) + 456));
  assertEquals((1 << 22) - 123, wasm.i32_shr_u_by_negative_22(-123 << 10));

  // i32.shr_s
  assertEquals(0, wasm.i32_shr_s_dynamic(0, 0));
  assertEquals(1, wasm.i32_shr_s_dynamic(3, 1));
  assertEquals(-1, wasm.i32_shr_s_dynamic(-1, 32));
  assertEquals(-1, wasm.i32_shr_s_dynamic(-1, -32));
  assertEquals(-123, wasm.i32_shr_s_dynamic(-123 << 15, 15));

  assertEquals(123, wasm.i32_shr_s_by_negative_22((123 << 10) + 456));
  assertEquals(-123, wasm.i32_shr_s_by_negative_22((-123 << 10) + 456));
};

for (let i = 0; i < 20; i++) testFct();
for (let fct of fcts) {
  %WasmTierUpFunction(wasm[fct.name]);
}
testFct();

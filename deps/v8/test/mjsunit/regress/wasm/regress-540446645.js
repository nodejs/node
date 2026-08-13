// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addFunction("f", makeSig([kWasmI64], [kWasmI32])).exportFunc()
.addBody([
  kExprLocalGet, 0,
  ...wasmI64Const(1099511627776n), // 1 << 40
  kExprI64And,
  kExprI32ConvertI64,
  kExprIf, kWasmI32,
    kExprI32Const, 1,
  kExprElse,
    kExprI32Const, 0,
  kExprEnd,
]);

builder.addFunction("g", makeSig([kWasmI64], [kWasmI32])).exportFunc()
.addBody([
  kExprLocalGet, 0,
  ...wasmI64Const(0x300000000n), // bits 32 and 33 set
  kExprI64And,
  kExprI32ConvertI64,
  kExprIf, kWasmI32,
    kExprI32Const, 1,
  kExprElse,
    kExprI32Const, 0,
  kExprEnd,
]);

const {f, g} = builder.instantiate().exports;
const highBit = 1n << 40n;
const lowBit = 1n << 8n;

assertEquals(0, f(highBit));
assertEquals(0, f(lowBit));
%WasmTierUpFunction(f);
assertEquals(0, f(0n));
assertEquals(0, f(highBit));
assertEquals(0, f(lowBit));

const val = 0x300000000n;
assertEquals(0, g(val));
%WasmTierUpFunction(g);
assertEquals(0, g(0n));
assertEquals(0, g(val));

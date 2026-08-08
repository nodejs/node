// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let array_type = builder.addArray(kWasmI64);

// For 32 bit platforms:
// Choose a big constant offset, so that when adding the constant wasm array
// header offset and the extra offset for the 2nd i32 load we end up with a
// value that exactly equals INT32_MIN (which is the singular value that
// returns false for LoadOp::OffsetIsValid()).
const BIG_CONST = 0xFFFFFFE;
const WASM_ARRAY_HEADER = 12; // map + hash + array size (subject to change)
const SECOND_LOAD_OFFSET = 4
const INT32_MIN = 1 << 31;
assertEquals(
  (BIG_CONST * 8 + WASM_ARRAY_HEADER + SECOND_LOAD_OFFSET) | 0,
  INT32_MIN);
let testValue = 0x1234_5678_90ab_cdefn;

builder.addFunction("test", makeSig([kWasmI32], [kWasmI64]))
  .addLocals(wasmRefNullType(array_type), 1)
  .addBody([
    // Create an array with a single value
    ...wasmI32Const(1),
    kGCPrefix, kExprArrayNewDefault, array_type,
    kExprLocalTee, 1,
    // Write a value at offset 0.
    kExprI32Const, 0,
    ...wasmI64Const(testValue),
    kGCPrefix, kExprArraySet, array_type,
    // Read the value at offset 0 by having the index calculation
    // (BIG_CONST - BIG_CONST). The i32.const in the code will be folded into
    // the offset of the load.
    kExprLocalGet, 1,
    kExprLocalGet, 0,
    ...wasmI32Const(BIG_CONST),
    kExprI32Add,
    kGCPrefix, kExprArrayGet, array_type,
  ])
  .exportFunc();

let instance = builder.instantiate();
let result = instance.exports.test(-BIG_CONST);
assertEquals(testValue, result);

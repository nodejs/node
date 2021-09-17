// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test case is minified from a clusterfuzz generated test. It exercises a
// bug in I64x2ShrS where the codegen was overwriting a input Register
// containing the shift value.
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

// Generate function 1 (out of 1).
builder.addFunction("main", kSig_i_v)
  .addBodyWithEnd([
    // signature: i_iii
    // body:
    kExprI32Const, 0x00,  // i32.const
    kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
    kExprI32Const, 0xee, 0xc6, 0x01,  // i32.const, 25454 (0x636e)
    kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
    kSimdPrefix, kExprI8x16ExtractLaneS, 0x00,  // i8x16.extract_lane_s
    kSimdPrefix, kExprI64x2ShrS, 0x01,  // i64x2.shr_s
    kSimdPrefix, kExprI8x16ExtractLaneS, 0x00,  // i8x16.extract_lane_s
    kExprI32Const, 0xee, 0xc6, 0x01,  // i32.const, 0x636e
    kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
    kSimdPrefix, kExprI8x16ExtractLaneS, 0x00,  // i8x16.extract_lane_s
    kExprI32Const, 0x00,  // i32.const
    kExprSelect,  // select
    kExprEnd,  // end @48
]).exportFunc();

const instance = builder.instantiate();
print(instance.exports.main());

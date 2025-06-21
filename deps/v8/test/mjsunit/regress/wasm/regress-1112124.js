// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
    // signature: i_iii
    // body:
    kSimdPrefix, kExprS128Const, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // s128.const
    kSimdPrefix, kExprI32x4ExtractLane, 0x00,  // i32x4.extract_lane
    kExprEnd,  // end @22
  ]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertEquals(-1, instance.exports.main(1, 2, 3));
// The bug happens on the second call to the same exported function, it returns 0.
assertEquals(-1, instance.exports.main(1, 2, 3));

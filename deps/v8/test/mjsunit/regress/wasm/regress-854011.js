// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_d_d)
    .addBody([
      // Call with param 0 (converted to i64), to fill the stack with non-zero
      // values.
      kExprGetLocal, 0, kExprI64SConvertF64,  // arg 0
      kExprGetLocal, 0, kExprI64SConvertF64,  // arg 1
      kExprGetLocal, 0, kExprI64SConvertF64,  // arg 2
      kExprGetLocal, 0, kExprI64SConvertF64,  // arg 3
      kExprGetLocal, 0, kExprI64SConvertF64,  // arg 4
      kExprGetLocal, 0, kExprI64SConvertF64,  // arg 5
      kExprGetLocal, 0, kExprI64SConvertF64,  // arg 6
      kExprGetLocal, 0, kExprI64SConvertF64,  // arg 7
      kExprCallFunction, 1,                   // call #1
      // Now call with 0 constants.
      // The bug was that they were written out as i32 values, thus the upper 32
      // bit were the previous values on that stack memory.
      kExprI64Const, 0,      // i64.const 0  [0]
      kExprI64Const, 0,      // i64.const 0  [1]
      kExprI64Const, 0,      // i64.const 0  [2]
      kExprI64Const, 0,      // i64.const 0  [3]
      kExprI64Const, 0,      // i64.const 0  [4]
      kExprI64Const, 0,      // i64.const 0  [5]
      kExprI64Const, 0,      // i64.const 0  [6]
      kExprI64Const, 0,      // i64.const 0  [7]
      kExprCallFunction, 1,  // call #1
      // Return the sum of the two returned values.
      kExprF64Add
    ])
    .exportFunc();
builder.addFunction(undefined, makeSig(new Array(8).fill(kWasmI64), [kWasmF64]))
    .addBody([
      kExprGetLocal, 7,     // get_local 7 (last parameter)
      kExprF64SConvertI64,  // f64.convert_s/i64
    ]);
const instance = builder.instantiate();
const big_num_1 = 2 ** 48;
const big_num_2 = 2 ** 56 / 3;
assertEquals(big_num_1, instance.exports.main(big_num_1));
assertEquals(big_num_2, instance.exports.main(big_num_2));

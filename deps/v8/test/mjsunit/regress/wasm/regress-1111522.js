// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Regression test to exercise Liftoff's i64x2.shr_s codegen, which back up rcx
// to a scratch register, and immediately overwrote the backup, then later
// restoring the incorrect value. See https://crbug.com/v8/10752 and
// https://crbug.com/1111522 for more.
const builder = new WasmModuleBuilder();
// i64x2.shr_s shifts a v128 (with all bits set), by 1, then drops the result.
// The function returns param 2, which should be unmodified.
builder.addFunction(undefined, kSig_i_iii).addBodyWithEnd([
  kSimdPrefix, kExprS128Const, ...new Array(16).fill(0xff),
  kExprI32Const, 0x01,
  kSimdPrefix, kExprI64x2ShrS, 0x01,
  kExprDrop,
  kExprLocalGet, 0x02,
  kExprEnd,
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertEquals(3, instance.exports.main(1, 2, 3));

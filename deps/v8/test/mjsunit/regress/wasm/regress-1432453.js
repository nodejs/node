// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([kWasmI32], [kWasmI64]));
builder.addMemory(1, 1, true);
// Generate function 1 (out of 1).
builder.addFunction('main', 0 /* sig */)
  .addLocals(kWasmI64, 1)
  .addBodyWithEnd([
// signature: l_i
// body:
kExprI32Const, 0x04,  // i32.const
kExprI32Const, 0x00,  // i32.const
kExprLocalGet, 0x00,  // local.get
kExprI64SConvertI32,  // i64.extend_i32_s
kExprLocalTee, 0x01,  // local.tee
kAtomicPrefix, kExprI32AtomicWait, 0x02, 0x00,  // memory.atomic.wait32
kExprDrop,  // drop
kExprLocalGet, 0x01,  // local.get
kExprEnd,  // end @29
]).exportFunc();
const instance = builder.instantiate();
assertEquals(2000n, instance.exports.main(2000));

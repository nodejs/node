// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
builder.addFunction('main', sig)
    .addBody([kExprI32Const, 0x01, kExprI32SExtendI8])
    .exportFunc();
const instance = builder.instantiate();
assertEquals(1, instance.exports.main());

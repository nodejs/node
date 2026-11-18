// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-random-rescheduling --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(kSig_v_v);

builder.addFunction('test_abstract', makeSig([kWasmNullRef], [kWasmI32]))
  .addLocals(wasmRefType(kWasmAnyRef), 1)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, kAnyRefCode,
    kExprLocalTee, 1,
    kGCPrefix, kExprRefTest, kEqRefCode,
    kExprLocalGet, 1,
    kGCPrefix, kExprRefTest, kStructRefCode,
    kExprLocalGet, 1,
    kGCPrefix, kExprRefTest, kArrayRefCode,
    kExprLocalGet, 1,
    kGCPrefix, kExprRefTest, kI31RefCode,
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
  ])
  .exportFunc();

builder.addFunction('test_indexed', makeSig([kWasmNullFuncRef], [kWasmI32]))
  .addLocals(wasmRefType(kWasmFuncRef), 1)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, kFuncRefCode,
    kExprLocalTee, 1,
    kGCPrefix, kExprRefTest, $sig0,
    kExprLocalGet, 1,
    kGCPrefix, kExprRefTest, $sig0,
    kExprI32Add,
  ])
  .exportFunc();

const instance = builder.instantiate();
assertTraps(kTrapIllegalCast, () => instance.exports.test_abstract(null));
assertTraps(kTrapIllegalCast, () => instance.exports.test_indexed(null));

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc
// Flags: --no-liftoff --no-wasm-speculative-inlining --no-wasm-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();

var sig_index = builder.addType({params: [kWasmDataRef], results: [kWasmI32]});

var sub1 = builder.addStruct([makeField(kWasmI32, true)]);
var sub2 = builder.addStruct([makeField(kWasmI32, false)]);

builder.addFunction('producer', makeSig([], [kWasmDataRef]))
  .addBody([
    kExprI32Const, 10,
    kGCPrefix, kExprStructNew, sub1])
  .exportFunc();

builder.addFunction('main', sig_index)
  .addBody([
    // Cast to sub1 and write field 0.
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, sub1,
    kExprI32Const, 42,
    kGCPrefix, kExprStructSet, sub1, 0,
    // Cast to sub2 and read field 0.
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, sub2,
    kGCPrefix, kExprStructGet, sub2, 0])
  .exportFunc();

var instance = builder.instantiate();

try {
  instance.exports.main(instance.exports.producer());
  console.log(`Failure: expected trap`);
} catch (e) {
  console.log(`Success: trapped: ${e}`);
}

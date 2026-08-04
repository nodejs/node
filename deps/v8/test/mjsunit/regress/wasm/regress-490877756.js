// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --harmony-struct

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let sharedStructType = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  shared: true
});

let structType = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  shared: false
});

builder.addFunction("createSharedStruct",
                    makeSig([], [wasmRefType(sharedStructType)]))
  .addBody([kGCPrefix, kExprStructNewDefault, sharedStructType])
  .exportFunc();

builder.addFunction("createStruct",
                    makeSig([], [wasmRefType(structType)]))
  .addBody([kGCPrefix, kExprStructNewDefault, structType])
  .exportFunc();

let instance = builder.instantiate();

let sharedArray = new SharedArray(10);
sharedArray[0] = instance.exports.createSharedStruct();
assertThrows(() => sharedArray[0] = instance.exports.createStruct(), TypeError);

// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $array0 = builder.addArray(kWasmAnyRef, true, kNoSuperType, true);
let $sig2 = builder.addType(makeSig([], [kWasmAnyRef]));
let main = builder.addFunction("main", $sig2);
let $mem0 = builder.addMemory64(9, 20, false);

main.addLocals(wasmRefType($array0), 1)
  .addBody([
    // array.new_default triggers young allocation.
    kExprI32Const, 1,
    kGCPrefix, kExprArrayNewDefault, $array0,
    kExprLocalSet, 0,
    // memory.grow can potentially trigger a GC.
    kExprI64Const, 4,
    kExprMemoryGrow, $mem0,
    kExprDrop,
    // array.set needs to perform a write.
    kExprLocalGet, 0,
    kExprI32Const, 0,
    kExprLocalGet, 0,
    kGCPrefix, kExprArraySet, $array0,
    kExprLocalGet, 0,
  ]).exportFunc();

const instance = builder.instantiate();
instance.exports.main();

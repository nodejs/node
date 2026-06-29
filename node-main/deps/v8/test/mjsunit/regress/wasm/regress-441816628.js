// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var module = new WasmModuleBuilder();
module.addMemory(1, 1);
module.addFunction("main", kSig_v_v)
  .addBody([
        ...wasmI32Const(100000),
        kExprI32Const, 0,
        kAtomicPrefix, 0x99, 0x00, 0, 0,

        ...wasmI32Const(100000),
        kExprI32Const, 0,
        kAtomicPrefix, 0x99, 0x80, 0x80, 0x80, 0x00, 0, 0,
    ])
  .exportAs("main");
var instance = module.instantiate();
assertTraps(kTrapMemOutOfBounds, instance.exports.main);

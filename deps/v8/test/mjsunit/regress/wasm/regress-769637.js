// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addMemory();
builder.addFunction("main", kSig_v_v)
    .addBody([kExprI32Const, 4,
              kExprI32Const, 8,
              kExprI32StoreMem, 0, 16])
    .exportAs("main");
let instance = builder.instantiate();
assertTraps(kTrapMemOutOfBounds, instance.exports.main);

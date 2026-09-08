// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function CreateWasmObjects() {
  let builder = new WasmModuleBuilder();

  let struct_type = builder.addStruct([makeField(kWasmI32, true)]);
  let array_type = builder.addArray(kWasmI32);

  builder.addFunction('MakeStruct', makeSig([], [kWasmExternRef]))
      .exportFunc()
      .addBody([
        kExprI32Const, 42,
        kGCPrefix, kExprStructNew, struct_type,
        kGCPrefix, kExprExternConvertAny
      ]);

  builder.addFunction('MakeArray', makeSig([], [kWasmExternRef]))
      .exportFunc()
      .addBody([
        kExprI32Const, 2,
        kGCPrefix, kExprArrayNewDefault, array_type,
        kGCPrefix, kExprExternConvertAny
      ]);

  let instance = builder.instantiate();
  return {
    struct: instance.exports.MakeStruct(),
    array: instance.exports.MakeArray(),
  };
}

let objects = CreateWasmObjects();
%TakeHeapSnapshot("/dev/null");

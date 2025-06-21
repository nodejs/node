// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --no-wasm-opt

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
(function TestAllocation() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, false)]);
  builder.addFunction("main", makeSig([kWasmI32], [kWasmExternRef])).addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, struct,
    kGCPrefix, kExprExternConvertAny,
  ]).exportFunc();
  let instance = builder.instantiate();

  assertFalse(null == instance.exports.main(0));
})();

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestSetGet() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct({fields: [makeField(kWasmWaitQueue, true)],
                                  shared: true});

  let struct_type = wasmRefNullType(struct);

  builder.addFunction("make", makeSig([kWasmI32], [struct_type]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  builder.addFunction("get", makeSig([struct_type], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  builder.addFunction("set", makeSig([struct_type, kWasmI32], []))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kGCPrefix, kExprStructSet, struct, 0])
    .exportFunc();

  builder.addFunction("atomic_get", makeSig([struct_type], [kWasmI32]))
    .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct, 0])
    .exportFunc();

  builder.addFunction("atomic_set", makeSig([struct_type, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let value0 = 42;
  let struct_obj = wasm.make(value0);
  assertEquals(value0, wasm.get(struct_obj));
  let value1 = -100;
  wasm.set(struct_obj, value1);
  assertEquals(value1, wasm.get(struct_obj));
  let value2 = 10;
  wasm.atomic_set(struct_obj, value2);
  assertEquals(value2, wasm.atomic_get(struct_obj));
})();

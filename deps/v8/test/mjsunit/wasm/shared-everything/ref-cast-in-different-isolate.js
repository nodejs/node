// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function PostSharedStructTypeCheck() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct(
      {fields: [makeField(kWasmI32, true)], shared: true});
  let producer_sig = makeSig([kWasmI32], [wasmRefType(struct)]);
  builder.addFunction("producer", producer_sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  let instance = builder.instantiate();

  let worker = new Worker(function() {
    onmessage = function({data:msg}) {
      d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

      let builder = new WasmModuleBuilder();

      let struct = builder.addStruct(
          {fields: [makeField(kWasmI32, true)], shared: true});
      let check_sig =
          makeSig([wasmRefNullType(kWasmAnyRef).shared()], [kWasmI32]);
      builder.addFunction("check", check_sig)
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprRefTest,
          ...wasmEncodeHeapType(wasmRefNullType(struct))
        ])
        .exportFunc();

      let instance = builder.instantiate();
      let is_match = instance.exports.check(msg.struct);
      postMessage({is_match: is_match});
      return;
    }
  }, {type: 'function'});

  let value = 42;
  let struct_obj = instance.exports.producer(value);
  worker.postMessage({struct: struct_obj});

  let response = worker.getMessage();
  assertEquals(1, response.is_match);
})();
